# macOS Dev-C++ 适配实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 让小熊猫 C++ 在 macOS 上用 lldb-dap 调试、默认使用 Homebrew GCC、兼容教材里的 Windows 写法，并把默认界面调成 Dev-C++ 5.11 的观感。

**Architecture:** 新增 `DAPTransport`（纯分帧/编解码，可单测）和完整的 `DAPDebuggerClient`（实现 `DebuggerClient` 全部接口，信号与 GDB-MI 客户端一致，UI 层零改动）。`Debugger::startClient` 按调试器路径后缀选择客户端。编译器发现走上游的 Lua compiler_hint 机制；Windows 兼容层是随包安装的头文件和脚本；界面差异只改 macOS 默认值。

**Tech Stack:** Qt 6.11 (Core/Widgets/Test), CMake, lldb-dap (Xcode CLT), Homebrew GCC 16, Lua 5.4 add-on。

规范：spec 见 `docs/superpowers/specs/2026-09-06-macos-devcpp-design.md`。构建：`cmake -S . -B build/dev -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_BUILD_TYPE=Debug && cmake --build build/dev --parallel`。

---

## 文件结构

| 文件 | 责任 |
|---|---|
| `RedPandaIDE/src/debugger/daptransport.{h,cpp}` (新) | Content-Length 分帧：`feed(bytes)` 产出完整 JSON 对象；`encode(obj)` 生成报文 |
| `RedPandaIDE/src/debugger/dapdebugger.{h,cpp}` (重写) | `DAPDebuggerClient`：进程、请求队列、seq→回调、事件处理、全部接口方法 |
| `RedPandaIDE/src/debugger/debugger.cpp` | 客户端选型；DAP 下控制台命令转发；`useDebugServer` 恒 false |
| `RedPandaIDE/src/systemconsts.h` | `LLDB_DAP_PROGRAM` |
| `RedPandaIDE/src/settings/compilersetsettings.cpp` | 找不到 gdb/lldb-mi 时查找 lldb-dap |
| `RedPandaIDE/CMakeLists.txt` | 加入 daptransport；macOS bundle 安装 compiler_hint.lua 与 compat |
| `tests/unit/CMakeLists.txt`, `tests/unit/test_daptransport.cpp` (新) | QtTest 单元测试 |
| `packages/macos/compiler_hint.lua` (新) | macOS 编译器发现脚本（手写 Lua） |
| `platform/macos/compat/include/conio.h`, `platform/macos/compat/bin/{pause,cls}` (新) | Windows 习惯兼容 |
| `RedPandaIDE/src/settings/environmentsettings.cpp` | macOS 默认主题/图标/缩放/语言 |
| `RedPandaIDE/src/mainwindow.cpp` | macOS 左侧面板标签横排 |

---

### Task 1: DAPTransport 分帧与编解码（含单元测试）

**Files:** 新建 `RedPandaIDE/src/debugger/daptransport.h/.cpp`、`tests/unit/CMakeLists.txt`、`tests/unit/test_daptransport.cpp`；修改根 `CMakeLists.txt`（`option(BUILD_UNIT_TESTS)` + `add_subdirectory(tests/unit)`）。

- [x] 写测试：单帧解析、两帧粘包、半帧等待、`encode` 输出含正确 `Content-Length`。
- [x] 运行失败（目标不存在）。
- [x] 实现：

```cpp
class DAPTransport {
public:
    void feed(const QByteArray& data);            // 追加字节
    bool next(QJsonObject& out);                   // 取出一条完整消息
    static QByteArray encode(const QJsonObject&);  // "Content-Length: N\r\n\r\n" + compact json
private:
    QByteArray mBuffer;
};
```
- [x] 运行通过；`git commit -m "feat(dap): add DAPTransport framing with unit tests"`。

### Task 2: DAPDebuggerClient 骨架：进程、队列、请求关联、启动流程

**Files:** 重写 `dapdebugger.h/.cpp`。

- [x] 类成员：`QProcess mProcess`（线程内创建）、`DAPTransport mTransport`、`QMutex mQueueMutex`、`QQueue<QByteArray> mOutgoing`、`QHash<qint64, std::function<void(const QJsonObject& resp)>> mPending`、`qint64 mSeq`、`bool mInitialized, mConfigured, mRunRequested, mStop`、`qint64 mThreadId`、`int mCurrentFrameId`、`QList<QJsonObject> mFrames`、`QString mCurrentFile; int mCurrentLine; qulonglong mCurrentAddress; QString mCurrentFunc`。
- [x] `sendRequest(command, args, cb)`：加锁、`seq++`、`mPending[seq]=cb`、入队。
- [x] `run()`：启动 `lldb-dap`（PATH 处理同 GDB-MI），循环：取队列写 stdin；`waitForReadyRead(5)`；`mTransport.feed(readAll())`；逐条 `dispatch`；处理完一批 `emit parseFinished()`；`mStop` 时 `disconnect(terminateDebuggee)` 后 kill。
- [x] `dispatch(obj)`：`response` → 取回调；`event` → `onEvent`；`request`（反向）→ `onReverseRequest`。
- [x] `initialize(inferior, hasSymbols)`：发 `initialize{adapterID, clientID:"redpanda", linesStartAt1, columnsStartAt1, supportsRunInTerminalRequest:true, pathFormat:"path"}`；回调后发 `launch{program, cwd, args(executor params), console:"integratedTerminal", stopOnEntry:false}`（launch 回调：失败则 console 输出 + `mProcessExited=true`）。
- [x] `onEvent("initialized")`：`mInitialized=true`；`sendAllBreakpoints()`（Task 4）；若 `mRunRequested` 则 `sendConfigurationDone()`。
- [x] `runInferior(hasBreakpoints)`：`mHasBreakpoints=hasBreakpoints; mRunRequested=true;` 若已 `mInitialized` 则 `sendConfigurationDone()`。`sendConfigurationDone`：若 `!mHasBreakpoints` 先 `setFunctionBreakpoints{[{name:"main"}]}`；再 `configurationDone`；`mInferiorRunning=true; emit inferiorContinued()`。
- [x] `onReverseRequest("runInTerminal")`：`args` → `wrapCommandForTerminalEmulator(terminalPath, pattern, args, &pSettings->dirs())` → `QProcess::startDetached(cmd, arguments, cwd)`；temp file owner 存入 `mTempFiles`；回复 `success:true, body:{}`。
- [x] `onEvent("exited")`：console 行 `Process exited with code N`；`onEvent("terminated")`：`mProcessExited=true`。`onEvent("output")`：`mConsoleOutput << body.output` 按行。
- [x] `stopDebug()`：`mStop=true`。`commandRunning()`：`mInferiorRunning || !mPending.isEmpty()`。
- [x] 编译通过；commit `feat(dap): DAP client process, request queue and launch flow`。

### Task 3: 执行控制、停止事件、调用栈、局部变量

- [x] `stepOver/stepInto/stepOut/resume/interrupt/stepOverInstruction/stepIntoInstruction`：`next/stepIn/stepOut/continue/pause`，带 `threadId`，指令级加 `granularity:"instruction"`；发送前 `mInferiorRunning=true; emit inferiorContinued(); emit cmdStarted()`。
- [x] `runTo(file,line)`：把 `{file,line}` 记入 `mTempBreakpoint`，重发该文件 `setBreakpoints`（含临时行），`continue`；停止时若命中临时行则清除并重发。
- [x] `onEvent("stopped")`：`mInferiorRunning=false; mThreadId=body.threadId; mUpdateCPUInfo=true;` reason 为 `exception`/`signal` 时 `mSignalReceived=true, mSignalName=body.description`; reason 含 `watchpoint`/`data breakpoint` 时 `emit watchpointHitted(desc,"","")`；然后 `requestStackTrace()`。
- [x] `requestStackTrace()`：`stackTrace{threadId, startFrame:0, levels:64}` → `mFrames`；每帧 `Trace{funcname=name, filename=source.path, line=line-1, level=i, address=instructionPointerReference}`；`backtraceModel()->setTraces`；`mCurrentFrameId=frames[0].id`, `mCurrentFile/Line/Address/Func`；`emit inferiorStopped(mCurrentFile, mCurrentLine-1)`（line 已是 1 起，传 0 起）；`emit cmdFinished()`。
- [x] `selectFrame(trace)`：`mCurrentFrameId=mFrames[trace->level].id`，更新 current*，`emit inferiorStopped`（触发 refreshAll）。`refreshFrame()`：`requestStackTrace()`。
- [x] `refreshStackVariables()`：`scopes{frameId}` → 找 `name=="Locals"` → `variables{ref}` → `localsUpdated("name = value")`。
- [x] commit `feat(dap): execution control, stop handling, stack and locals`。

### Task 4: 断点、条件、临时断点、监视点

- [x] `QMap<QString /*file*/, QList<PBreakpoint>> mBreakpoints`；`addBreakpoint(bp)`：加入并 `sendBreakpointsForFile(file)`（若 `mInitialized`）；`removeBreakpoint(bp)`：移除并重发；`setBreakpointCondition(bp)`：重发。
- [x] `sendBreakpointsForFile(file)`：`setBreakpoints{source:{path}, breakpoints:[{line:bp->line+1, condition}]}`；回调：逐个 `emit breakpointInfoGetted(file, line-1, id)`。
- [x] `sendAllBreakpoints()`：遍历 `mBreakpoints` 的 key。
- [x] `addWatchpoint(expr)`：`dataBreakpointInfo{name:expr, frameId}` → `dataId` 非空则加入 `mDataBreakpoints` 并 `setDataBreakpoints{breakpoints:[{dataId, accessType:"write"}]}`；失败则 console 输出原因。
- [x] commit `feat(dap): breakpoints, conditions and data breakpoints`。

### Task 5: 监视表达式

- [x] `struct WatchInfo{QString expression; int variablesReference; QString type;}`；`QHash<QString /*name*/, WatchInfo> mWatches`；`int mWatchSeq`；顶层名 `"dap_%1"`，子项名 `parentName + ".c" + n`。
- [x] `addWatch(expr)`：`evaluate{expression, frameId, context:"watch"}` → 成功：`name=newName()`, `mWatches[name]={expr, ref, type}`；`numChild = ref>0 ? (indexedVariables ?: namedVariables ?: 1) : 0`；`emit varCreated(expr, name, numChild, result, type, false)`；失败：`emit varCreated(expr, name, 0, "<"+message+">", "", false)`（名称仍登记，后续可重试）。
- [x] `refreshWatch()`/`refreshWatch(var)`：对每个（或指定）顶层 watch 重新 `evaluate`，回调 `emit varValueUpdated(name, result, "true"/"false", typeChanged, type, numChild, false)`；子项 ref 失效：删除 `name` 前缀匹配的子项条目；全部完成后 `emit varsValueUpdated()`。
- [x] `fetchWatchVarChildren(name)`：`variables{variablesReference}` → `emit prepareVarChildren(name, count, false)`，每个子项登记并 `emit addVarChild(name, childName, child.name, childNumChild, value, type, false)`。
- [x] `removeWatch(var)`：删除条目。`writeWatchVar(name, value)`：`setExpression{expression, value, frameId}`，然后 `refreshWatch()`。
- [x] commit `feat(dap): watch expressions with lazy children`。

### Task 6: 表达式求值、内存、寄存器、反汇编、调试控制台

- [x] `evalExpression(expr)`：`evaluate{context:"hover"}` → `emit evalUpdated(result 或 message)`。
- [x] `readMemory(start, rows, cols)`：`start` 若为十六进制/十进制数直接用，否则先 `evaluate{context:"hover"}` 取 `memoryReference` 或 result；`readMemory{memoryReference:"0x..", count:rows*cols}` → base64 解码 → 每行 `"0x%addr b1 b2 ..."`（每行 cols 个）→ `emit memoryUpdated`。
- [x] `writeMemory(addr, data)`：`evaluate{expression:"memory write 0x.. 0x..", context:"repl"}`。
- [x] `refreshRegisters()`：`scopes` 的 `Registers` → `variables` → 若子项含 `variablesReference`（寄存器组）再展开一层；`emit registerNamesUpdated(names)`、`emit registerValuesUpdated({i:value})`。
- [x] `disassembleCurrentFrame(blend)`：`disassemble{memoryReference: 0x pc, instructionOffset:-24, instructionCount:64, resolveSymbols:true}` → 行 `"=> "/"   " + address + " " + instruction`（blend 且带 `location/line` 时在变化处插入 `"file:line"` 行）→ `emit disassemblyUpdate(mCurrentFile, mCurrentFunc, lines)`。`setDisassemblyLanguage(intel)`：repl `settings set target.x86-disassembly-flavor intel|att`。
- [x] `runConsoleCommand(cmd)`（新公共方法）：`evaluate{context:"repl"}` → 结果按行进 `mConsoleOutput`。`Debugger::runClientCommand` 增加 DAP 分支调用它。
- [x] `skipDirectoriesInSymbolSearch/addSymbolSearchDirectories/skipStandardLibraryFunctions`：空实现。
- [x] commit `feat(dap): evaluate, memory, registers, disassembly and console`。

### Task 7: 接线与自动检测

- [x] `systemconsts.h`：`#define LLDB_DAP_PROGRAM "lldb-dap"`（Windows 为 `"lldb-dap.exe"`）。
- [x] `debugger.cpp` `startClient`：`if (debugger.endsWith(LLDB_DAP_PROGRAM)) setDebuggerType(DAP)`；`useDebugServer` 对 DAP 为 false；`mClient = (type==DAP) ? new DAPDebuggerClient(this) : new GDBMIDebuggerClient(...)`；`refreshWatchVars` 对 DAP 走 `refreshWatch()`。
- [x] `compilersetsettings.cpp` `setExecutables`：Clang 与 GCC 分支在 debugger 为空时 `findProgramInBinDirs(LLDB_DAP_PROGRAM)`；macOS 下 bin dirs 额外含 `/Library/Developer/CommandLineTools/usr/bin` 与 `xcode-select -p` 的 `usr/bin`。
- [x] `CMakeLists.txt` 源文件列表加 `daptransport`。编译、启动 IDE、用 Clang 集调试 `tests/hello.cpp` 走通。commit `feat(dap): select DAP client by debugger path; detect lldb-dap`。

### Task 8: macOS 编译器发现脚本与 Windows 兼容层

- [x] `packages/macos/compiler_hint.lua`：扫描 `/opt/homebrew/bin` 与 `/usr/local/bin` 的 `^g\+\+-[0-9]+$`；每个版本生成 `GCC N, 发布/调试` 两套（`compilerType="GCC"`, `debugger=<lldb-dap>`, `binDirs={dir}`, `cIncludeDirs/cppIncludeDirs` 含 `appResourceDir()/compat/include`，`ccCmdOptDebugInfo` 等）；再加 Xcode Clang 两套；`preferCompiler` 指向最高版本 GCC 调试；`noSearch` 空。lldb-dap 路径：`xcode-select -p` + `/usr/bin/lldb-dap`，不存在则 `/Library/Developer/CommandLineTools/usr/bin/lldb-dap`。
- [x] `platform/macos/compat/include/conio.h`：`getch/getche/kbhit/clrscr`（termios + `<sys/select.h>`），`#if defined(__APPLE__) || defined(__unix__)`。`compat/bin/pause`（`printf '请按任意键继续. . . '; read -rsn1; echo`）、`compat/bin/cls`（`printf '\033[2J\033[H'`）。
- [x] `RedPandaIDE/CMakeLists.txt` bundle 分支：`install(FILES packages/macos/compiler_hint.lua DESTINATION RedPandaIDE.app/Contents/MacOS)`；`install(DIRECTORY platform/macos/compat DESTINATION RedPandaIDE.app/Contents/Resources USE_SOURCE_PERMISSIONS)`。
- [x] compat/bin 加入运行 PATH：脚本里 `binDirs` 追加 `appResourceDir().."/compat/bin"`。
- [x] 重新打包，删除 `~/Library/Application Support/RedPandaIDE/redpandacpp.ini` 后启动，确认默认集为 GCC 16 调试且 `bits.cpp`、`system("pause")`、`conio.h` 可用。commit `feat(macos): compiler hint for Homebrew GCC and Windows-compat shims`。

### Task 9: 界面默认值

- [x] `environmentsettings.cpp`：`#ifdef Q_OS_MACOS` 默认 `theme="default"`, `icon_set="bluesky"`, `icon_zoom_factor=1.5`, 语言：系统 locale 的地区为 China 且语言非中文时 `"zh_CN"`。
- [x] `mainwindow.cpp` `setTabsInDockLocation`：macOS 下左右区也用 `North`。
- [x] `uisettings.cpp`：macOS 默认 `show_problem_set=false`。
- [x] 重新打包，截图对比。commit `feat(macos): Dev-C++ 5.11 style defaults`。

### Task 10: 端到端验证与打包

- [x] GCC 集：`tests/bits.cpp` 设断点、F5、单步、监视 `v` 展开、修改值、终端输入 `hello.cpp`。
- [x] Clang 集同样跑一遍。
- [x] `./packages/macos/build.sh --qt-dir /opt/homebrew` + brotli/xattr 修补；`dist/` 产物。
- [x] 更新 `docs/superpowers/plans` 勾选；commit。

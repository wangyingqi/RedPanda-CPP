# macOS Dev-C++ 体验：小熊猫 C++ 适配设计

日期：2026-09-06　分支：`macos-devcpp`　基线：Red Panda C++ 3.5.0-beta.2 (cbe084d)

## 目标

让学生在 macOS 上得到与 Windows 经典 Dev-C++ 5.11 一致的学习体验：打开即写、一键编译运行、能下断点调试、教材代码（`bits/stdc++.h`、`system("pause")`、`conio.h`）直接可用、界面观感接近 5.11。

不在范围内：签名与公证、`windows.h` 兼容、编辑器上方的类/函数下拉框、原版图标。

## 一、DAP 调试后端（lldb-dap）

**选型。** 调试器路径以 `lldb-dap` 结尾时，`Debugger::start` 创建 `DAPDebuggerClient` 而非 `GDBMIDebuggerClient`；`useDebugServer` 对 DAP 恒为 false。`CompilerSet::setExecutables` 在找不到 gdb 与 lldb-mi 时查找 `lldb-dap`（新增常量 `LLDB_DAP_PROGRAM`），搜索目录含 `/Library/Developer/CommandLineTools/usr/bin`。

**结构。** `DAPDebuggerClient : DebuggerClient`（QThread）。

- 传输层：`QProcess` 启动 `lldb-dap`，stdio 上按 `Content-Length` 分帧；复用 `dapprotocol.{h,cpp}` 的编解码。读循环在线程 `run()` 内，写请求由 UI 线程调用的接口方法投递到线程安全队列。
- 请求/响应关联：`seq -> 回调` 表；响应到达后在调试线程执行回调并发出与 GDB-MI 客户端相同的信号。
- 状态：`mThreadId`、`mCurrentFrameId`、`mFrames`、断点表 `文件 -> {行, 条件, Breakpoint 编号}`、监视表 `varName -> {expression, variablesReference}`。

**生命周期。** `initialize`（`supportsRunInTerminalRequest=true`）→ `launch`（`program`、`cwd`、`args`、`console="integratedTerminal"`）→ 收到 `initialized` 事件后下发全部断点与监视点 → `configurationDone`。`runInTerminal` 反向请求由客户端处理：用 `wrapCommandForTerminalEmulator` 在 Terminal.app 中运行 lldb-dap 提供的 `args`，回复成功。收到 `exited`/`terminated` 事件后结束线程；`stopDebug` 发 `disconnect(terminateDebuggee=true)`。

**接口映射。**

| DebuggerClient 方法 | DAP |
|---|---|
| initialize/runInferior | initialize / launch / configurationDone |
| stepOver/stepInto/stepOut/resume/interrupt | next / stepIn / stepOut / continue / pause |
| stepOverInstruction/stepIntoInstruction | next / stepIn，`granularity="instruction"` |
| runTo(file,line) | 临时加断点 + continue，停下后移除 |
| add/removeBreakpoint/setBreakpointCondition | 按文件重发 `setBreakpoints`（DAP 以文件为单位整体替换） |
| addWatchpoint | `dataBreakpointInfo` + `setDataBreakpoints` |
| refreshFrame/selectFrame | `stackTrace` → `BacktraceModel::setTraces`；选择帧只改 `mCurrentFrameId` 并刷新 locals/watch |
| refreshStackVariables | `scopes` + `variables(Locals)` → `localsUpdated("name = value")` |
| addWatch/refreshWatch/fetchWatchVarChildren/removeWatch | `evaluate(context="watch")` → `varCreated`；子项 `variables(ref)` → `prepareVarChildren`/`addVarChild`；每次停止后全部重求值 → `varValueUpdated` + `varsValueUpdated` |
| writeWatchVar | `setVariable` 或 `setExpression` |
| evalExpression | `evaluate(context="hover")` → `evalUpdated` |
| readMemory/writeMemory | `readMemory`（base64 解码，按 rows×cols 组织）；写用 `evaluate(context="repl")` 执行 `memory write` |
| refreshRegisters | `scopes` 的 Registers 作用域 → `registerNamesUpdated`/`registerValuesUpdated` |
| disassembleCurrentFrame | `disassemble(memoryReference=当前 pc)` → `disassemblyUpdate` |
| 调试控制台输入 | `evaluate(context="repl")`，输出回显到控制台 |
| skip*/addSymbolSearchDirectories | 空实现（lldb 无对应概念） |

**停止处理。** `stopped` 事件 → `stackTrace` → 更新 backtrace，`inferiorStopped(file, line)`；随后刷新 locals 与 watch。`output` 事件写入调试控制台。

**已知限制。** 表达式中调用 STL 成员函数（如 `v.size()`）会因内联而失败，与 GDB 行为相同；变量名和下标访问正常。

## 二、编译器：Homebrew GCC 默认

新增 `addon/compiler_hint/macos.tl`：扫描 `/opt/homebrew/bin` 与 `/usr/local/bin` 下的 `gcc-N`/`g++-N`，为每个版本生成 Release/Debug 两套配置，调试器指向 lldb-dap；同时保留 Xcode Clang 配置。默认集为最高版本的 GCC Debug。macOS 打包时把该脚本作为 `compiler_hint.lua` 安装到 libexec。

## 三、Windows 习惯兼容

应用内新增 `Resources/compat/`：
- `include/conio.h`：`getch`、`getche`、`kbhit`、`clrscr`，termios 实现，仅在 `__APPLE__`/`__unix__` 下生效。
- `bin/pause`、`bin/cls`：shell 脚本。

编译器提示脚本把 `compat/include` 加入各配置的 C/C++ 包含目录；`compat/bin` 加入运行程序时的 PATH（通过编译器集的 bin 目录）。

## 四、界面默认值向 Dev-C++ 5.11 看齐

仅改 macOS 首次运行的默认值与 `.ui`：
- 主题 `default`（浅色）、图标集 `bluesky`、`icon_zoom_factor=1.5`、界面语言 `zh_CN`。
- `tabExplorer`、`tabMessages` 标签位置改为 `North`；`Problem Set` 默认隐藏（已有设置项）。

## 五、测试

- 单元测试：DAP 分帧解析、请求/响应关联、断点表到 `setBreakpoints` 的转换、watch 名称映射。
- 端到端：`tests/*.cpp` 用 GCC 16 编译；用 lldb-dap 走断点/单步/变量/子项展开；在真实 IDE 中验证终端输入、监视、修改变量。
- 回归：Clang 配置下同样能调试；Windows 构建不受影响（DAP 代码在各平台都编译，选型只看路径）。

# 容闳CPP (RHCPP)

**容闳公学信息竞赛 C++ 学习专版** — 面向信息学竞赛入门教学的 macOS 版 C/C++ 学习环境。

> 中文简称 **容闳CPP**，英文简称 **RHCPP**，当前版本 **1.0**。

容闳CPP 基于开源项目 [小熊猫 C++（Red Panda C++）](https://github.com/royqh1979/RedPanda-CPP) 修改而来，原作者 瞿华（Roy Qu, royqh1979@gmail.com），遵循 **GNU 通用公共许可证第 3 版（GPL v3）**。本发行版在其基础上为 macOS 做了适配与教学定制，同样以 GPL v3 发布，源代码在原项目基础上修改。

做这个版本的初衷：让老师在 Windows 上用 Dev-C++ 教学、学生在 Mac 上也能用**几乎一样的界面和操作**跟着学，不必再为了一个 IDE 去装 Windows。

## macOS 版做了什么

**调试器**
- 使用 Xcode 自带的 `lldb-dap`（无需 gdb）实现完整调试：断点、条件断点、单步、调用栈、局部变量、监视表达式（含展开）、求值、内存/寄存器/反汇编、调试控制台。
- 被调试程序在系统终端中运行（支持键盘输入）。

**编译器与 Windows 习惯兼容**
- 默认使用 Homebrew GCC（`bits/stdc++.h` 原生可用），同时识别 Apple Clang。
- 自带兼容层：`conio.h`（getch/kbhit 等）、`pause`、`cls`，让 `system("pause")` 等教材常见写法在 Mac 上照常工作。

**界面（对齐经典 Dev-C++ 5.11）**
- 浅色主题、彩色图标、简体中文、经典配色（注释深蓝斜体、运算符红色、关键字加粗）。
- 菜单栏：文件 / 编辑 / 查找 / 视图 / 项目 / 运行 / 调试 / 工具 / 窗口 / 帮助。
- 编辑器上方“类 / 函数”下拉导航条。
- 左侧面板 项目 / 类 / 调试，底部面板 编译器 / 编译日志 / 调试 / 查找结果。
- 工具栏按钮加大，下方显示命令名与快捷键（跟随界面语言）。
- 常用快捷键与 Dev-C++ 一致：F9 编译、F11 编译运行、F5 调试。

**内置学习内容**
- 自带 16 道由简入深的入门练习题（含样例数据），首次运行自动加载。
- 支持 Competitive Companion 浏览器插件，可从洛谷、Codeforces、力扣、AtCoder 等在线判题网站一键导入题目。

## 在 macOS 上构建

需要 Apple Silicon 或 Intel Mac、Xcode 命令行工具、[Homebrew](https://brew.sh)。

```bash
# 依赖
brew install qtbase qtsvg qttools cmake gcc

# 构建并打包为 .app（产物在 dist/）
git clone https://github.com/wangyingqi/RedPanda-CPP.git
cd RedPanda-CPP
./packages/macos/build.sh --qt-dir /opt/homebrew
```

调试需要 Xcode 命令行工具自带的 `lldb-dap`（`xcode-select --install` 后即有）。

> 说明：本仓库的可执行文件与 `.app` 内部名称仍沿用上游的 `RedPandaIDE`，但界面显示的产品名为“容闳CPP”。

## 许可证与致谢

本项目以 **GPL v3** 发布。衷心感谢原作者 **瞿华（Roy Qu）** 及 Red Panda C++ 的贡献者们——没有他们的开源工作，就没有这个 Mac 教学版。原项目：<https://github.com/royqh1979/RedPanda-CPP>。

**作者（本 macOS 发行版）**：学生家长 · X: [@wangyingqi](https://x.com/wangyingqi) · Email: wangyingqi@gmail.com

---

以下为原项目 Red Panda C++ 的说明（保留原文）：

# RedPanda C++

Red Panda C++ (Old name: Red Panda Dev-C++ 7) is an fast ,lightweight, open source, and cross platform C/C++/GNU Assembly IDE.

Simplified Chinese Website: [http://royqh.net/redpandacpp](http://royqh.net/redpandacpp)

English Website: [https://sourceforge.net/projects/redpanda-cpp](https://sourceforge.net/projects/redpanda-cpp)

[Donate to this project](https://ko-fi.com/royqh1979)

**New Features (Compared with Red Panda Dev-C++ 6):**

* Cross Platform (Windows/Linux/MacOS)
* Problem Set (run and test program against predefined input / expected output data)
* Competitive Companion support ( It's an chrome/firefox extension that can fetch problems from OJ websites)
* Edit/compile/run/debug Assembly language programs ( GNU Assember / NASM ).
* Find symbol occurrences
* Memory View for debugging
* TODO View
* Support SDCC Compiler

**UI Improvements:**

* Full high-dpi support, including fonts and icons
* Better dark theme support
* Better editor color scheme support
* Redesigned Find/Replace in Files UI
* Redesigned bookmark UI

**Editing Improvements:**

* Enhanced auto indent
* Enhanced code completion
* Better code folding support

**Debugging Improvements:**

* Use gdb/mi interface
* Enhanced watch
* gdbserver mode

**Code Intellisense Improvements:**

* Better support identifiers for complex expressions
* Support UTF-8 identifiers
* Support C++ 14 using type alias
* Support C-Style enum variable definitions
* Support MACRO with arguments
* Support C++ lambdas

And many other improvements and bug fixes. See NEWS.md for full information.

## Acknowledgement

[Lua](https://www.lua.org/) 5.4.6 ([source mirror](https://github.com/lua/lua/tree/v5.4.6)) is used as add-on runtime.

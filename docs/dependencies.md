# 依赖登记

> **注（2026-09-23 清理）**：下文提到的 `.artifacts/format_tool`（clang-format 临时安装）已随临时目录删除。它不参与工程构建或运行，删除不影响源码的可构建性与可复现性。

## 运行/构建依赖

| 项目 | 固定版本/位置 | 来源与许可 |
| --- | --- | --- |
| nlohmann/json | v3.11.3；third_party/nlohmann/json.hpp | https://github.com/nlohmann/json/tree/v3.11.3 ；MIT，随附 LICENSE.MIT |
| 编译器 | MinGW GCC 15.1.0，本机 D:/mingw64 | C++17；Windows SDK/CRT 由工具链提供 |
| 构建系统 | CMake 3.20+，MinGW Makefiles | 本地配置，无下载步骤 |

json.hpp 官方来源：https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp

SHA-256：`9BEA4C8066EF4A1C206B2BE5A36302F8926F7FDC6087AF5D20B417D0CF103EA6`

CMake 通过 INTERFACE 目标 `nlohmann_json::nlohmann_json` 引入。第三方头文件与许可证已入工作区，干净目录配置/编译不需联网。升级必须重新登记版本/哈希/许可并运行解析、恢复与往返测试，不能浮动使用 master。

## 开发辅助工具

格式化使用 clang-format 19.1.7，临时安装于 .artifacts/format_tool；它不参与工程构建或运行，交付源码无需携带该目录。clangd 检查使用 Zed 自带 22.1.6（--tweaks=none 关闭宏重构动作自测，保留解析/clang-tidy 检查）。

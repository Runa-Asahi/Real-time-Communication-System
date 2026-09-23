# 使用帮助（HELP）

模拟即时通讯系统 · C++17 面向对象课程设计（552502 班）
本文件面向「拿到源码后要构建、运行、演示、排错」的人。项目背景与模块清单见 [README.md](README.md)，各阶段的验收记录见 [docs/](docs)。

---

## 1. 五分钟快速上手

```powershell
# 1) 配置并构建（首次约 1~3 分钟）
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -- -j4

# 2) 跑全部自动测试（9 项）
ctest --test-dir build --output-on-failure

# 3) 看完整功能演示（自建临时数据目录，不污染仓库）
.\build\fanlx_app.exe --demo

# 4) 进入交互式数字菜单（在当前目录的 data 下保存数据）
.\build\fanlx_app.exe
```

构建输出目录 `build/` 已被 `.gitignore` 忽略，可随时删除重建。

---

## 2. 环境要求

| 项目 | 要求 |
| --- | --- |
| 操作系统 | Windows（网络层使用 Winsock2） |
| CMake | 3.20 或更高 |
| 编译器 | MinGW-w64 / GCC（本机验证版本 GCC 15.1.0，路径 `D:/mingw64`），**不需要 Visual Studio** |
| C++ 标准 | C++17（不要使用 C++20 语法） |
| 网络依赖 | 无。JSON 头文件 `third_party/nlohmann/json.hpp`（v3.11.3，MIT）随源码提供，配置与构建**无需联网** |

`CMakeLists.txt` 在 MinGW 下自动加 `-static`，把 GCC 运行库静态链接进 exe，避免 `PATH` 中其它工具链的旧版 `libstdc++` 抢先加载。

---

## 3. 构建

### 3.1 常规构建

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -- -j4
```

`CMakeLists.txt` 已开启 `CMAKE_EXPORT_COMPILE_COMMANDS`，配置后会生成 `build/compile_commands.json` 供 clangd 使用。

### 3.2 干净重建（推荐在改动头文件后执行）

```powershell
Remove-Item -Recurse -Force build
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -- -j4
```

### 3.3 编译严格度

新目标以 `-Wall -Wextra -Wpedantic -Werror` 编译：**任何新增警告都会让编译直接失败**。这不是配置错误，而是有意的质量门禁；请修改源码消除警告，不要去掉这些选项。

---

## 4. 运行

### 4.1 `--demo`：一键功能演示

```powershell
.\build\fanlx_app.exe --demo
```

在系统临时目录下新建唯一的 `fanlx-demo-*` 子目录，演示真实写入、无损切换、关闭重开、全字段比较与登录态清空，并打印证据目录。这是 CTest 中 `smoke` 项的实体。

### 4.2 交互式数字菜单

```powershell
.\build\fanlx_app.exe                          # 使用当前目录的 data
.\build\fanlx_app.exe --data-dir data-classroom # 使用指定目录
```

空库不会被自动用空状态覆盖，启动时会提示初始化或确认恢复。所有操作在退出时统一执行保存检查。

### 4.3 命令行参数

| 参数 | 说明 |
| --- | --- |
| `--demo` | 演示模式（临时目录，用完即走） |
| `--data-dir <路径>` | 指定数据目录，默认 `data` |
| `--init-demo` | 在**空目录**显式初始化演示数据；已有主文件或备份时拒绝覆盖 |
| `--recover` | 主文件损坏而备份有效时，显式确认恢复（先保留损坏证据） |
| `--check` | 启动时只做一次检查，不进菜单 |
| `--menu` | 强制进入数字菜单（默认行为） |
| `--im-server` | 以中心转发 IM 服务端身份启动（需先 `--init-demo`） |
| `--im-host <IPv4>` | IM 服务端地址 |
| `--im-port <端口>` | IM 服务端端口 |
| `--im-tokens <JSON 路径>` | 令牌文件：账号 ID → 令牌 的 UTF-8 JSON |

---

## 5. 菜单速查

输入 `0` 逐级返回；输入结束时（EOF）会执行保存检查后退出。

### 主菜单

```
1 登录或切换账号   2 账号与服务   3 好友   4 群   5 缓存统计   6 QQ聊天   0 保存退出
```

### 账号与服务（主菜单 2）

```
1 本人资料与服务  2 开通服务  3 逐项确认登录  4 绑定  5 解绑
6 修改资料  7 当前服务注销  8 全平台注销  9 注册  0 返回
```

### 好友（主菜单 3）

```
1 列表和备注  2 添加  3 删除  4 备注  5 同平台共同好友
6 跨平台推荐  7 跨平台共同好友  0 返回
```

跨平台推荐 / 跨平台共同好友要求先选「本人另一个已确认登录的平台账号」，两个会话须属于同一人。

### 群（主菜单 4）

```
1 我的群   2 成员   3 QQ申请          4 批准      5 拒绝申请   6 微信邀请
7 确认邀请 8 拒绝邀请 9 退群          10 踢人     11 任命管理员 12 撤销管理员
13 转让群主 14 切换模式 15 创建讨论组 16 子群加人 17 解散      18 创建群
19 改名    20 子群与待处理记录        0 返回
```

群的治理模式（QQ / 微信）可运行时切换，切换只交换策略对象、不移动任何成员数据，因此是无损的。

### 缓存统计（主菜单 5）

打印 ARC 查询缓存的命中 / 未命中 / 有效条目 / 逻辑载荷字节 / 内存池保留字节。

### QQ 聊天（主菜单 6）

```
1 单聊发送  2 历史记录  3 群聊发送  4 好友在线查询  5 状态通知  0 返回
```

需要先用 `--im-host` / `--im-port` / `--im-tokens` 启动，并以 QQ 身份登录。返回菜单后长连接保持、继续收消息；注销或退出才断开。

---

## 6. 中心转发 IM 联调（三终端）

详见 [docs/network-validation.md](docs/network-validation.md)。要点：服务端与各客户端**必须使用不同数据目录**。

```powershell
# 1) 先初始化三个不同目录
.\build\fanlx_app.exe --data-dir data-im-server --init-demo
.\build\fanlx_app.exe --data-dir data-im-a --init-demo
.\build\fanlx_app.exe --data-dir data-im-b --init-demo
```

2) 准备 UTF-8 JSON 令牌文件（示例值请替换为随机令牌）：

```json
{"2026001":"replace-with-random-token-A","2026002":"replace-with-random-token-B"}
```

服务端 `server-tokens.json` 放全部准入账号；客户端 `a-token.json` 只放 A，`b-token.json` 只放 B。

3) 三个终端分别运行：

```powershell
.\build\fanlx_app.exe --data-dir data-im-server --im-server --im-host 127.0.0.1 --im-port 9500 --im-tokens server-tokens.json
.\build\fanlx_app.exe --data-dir data-im-a --im-host 127.0.0.1 --im-port 9500 --im-tokens a-token.json
.\build\fanlx_app.exe --data-dir data-im-b --im-host 127.0.0.1 --im-port 9500 --im-tokens b-token.json
```

客户端主菜单 1 登录 QQ，账号 `2026001` / `2026002`；群号 `1001`，成员含 `2026001` / `2026002` / `2026003`。服务端 `Ctrl+C` 正常停止，重启沿用同目录，客户端会自动恢复。

---

## 7. 测试

```powershell
ctest --test-dir build --output-on-failure          # 全部 9 项
ctest --test-dir build -R network_behavior          # 只跑某一项
```

| CTest 名称 | 可执行文件 | 覆盖内容 |
| --- | --- | --- |
| `smoke` | `fanlx_app --demo` | 端到端演示：写入、重开、登录态清空 |
| `cache_behavior` | `cache_tests` | 2Q / ARC 缓存与好友域断言 |
| `group_behavior` | `group_tests` | 群权限矩阵与无损切换 |
| `persistence_behavior` | `persistence_tests` | 原子写、事务、故障注入、子进程重启 |
| `query_cache_behavior` | `query_cache_tests` | 查询缓存、版本失效与内存池 |
| `console_behavior` | `console_tests` | 菜单输入、返回、EOF 与保存 |
| `network_behavior` | `network_tests` | 拆包、断连、有界队列、双进程收发 |
| `im_behavior` | `network_tests --im` | 中心转发 IM 全链路（超时 90s） |
| `integration_behavior` | `integration_tests` | 事务权限、缓存开关对照与重启 |

测试用 `assert` 断言：**退出码 0 即全部通过**。也可单独运行 exe 查看细节。

### 基准

```powershell
.\build\cache_benchmark.exe
```

固定种子缓存 / 回源对照基准，输出可复现的对比数据。

### 发布验证

```powershell
.\scripts\verify-release.ps1                    # Release（默认）
.\scripts\verify-release.ps1 -Configuration Debug
```

在独立临时目录完成全新配置、构建与全量 CTest，并保存日志和源码 SHA-256 清单；任一步失败即抛错。证据目录会打印在最后一行。

---

## 8. 数据与文件

- 一个数据目录含 `state.json`（主文件）、`state.json.prev`（上一有效备份）、`.lock`（独占锁）。
- **同一数据目录只允许一个进程持有 `.lock` 独占句柄**；不要手工删除锁文件去抢占。异常终止后由 Windows 释放句柄，可直接重启。
- 无参数运行使用当前目录的 `data`；测试只清理自己创建的唯一临时目录，失败时保留并输出路径。
- 没有全局数据重置命令——要重置就换一个新的 `--data-dir` 路径。
- 文件协议（schemaVersion=1、字段与恢复边界）见 [docs/data-format.md](docs/data-format.md)。

---

## 9. 常见问题（FAQ）

**Q：运行 exe 报 `0xC0000139`（找不到入口点）？**
A：`PATH` 中其它工具链（常见是 Anaconda 的 `Library\mingw-w64\bin`）的旧版 `libstdc++` 抢先加载。`CMakeLists.txt` 已为 MinGW 静态链接运行库，请先做一次干净重建；仍不行时显式把 MinGW 提前：

```powershell
$env:PATH = "D:\mingw64\bin;$env:PATH"
```

**Q：`std::cout` 输出的中文是乱码？**
A：控制台代码页是 cp936，而源码按 UTF-8 编译。这是已知现象：**程序输出一律用 ASCII，中文只出现在源码注释里**。若你新增了中文输出，请改回 ASCII。

**Q：编译因为 `-Werror` 失败？**
A：这是有意的质量门禁。修正警告（常见是未使用参数、有符号/无符号比较、窄化转换），不要删除 `-Werror`。

**Q：改了 `#include` 之后测试行为异常？**
A：聚合头文件（`*DomainFanLX.h`）会把类型"顺带"传给下游，形成隐式依赖。**改动 include 后必须删除 `build` 目录、全新重建**才算验证通过。

**Q：启动时提示需要初始化或恢复？**
A：空目录或损坏目录不会被静默覆盖。空目录用 `--init-demo`；主文件损坏而备份有效时用 `--recover`，它会先保留损坏证据。

**Q：菜单里操作抛异常了，数据会坏吗？**
A：不会。业务规则在接口层校验，违规操作立即抛出异常并且不改变状态；退出时统一保存检查。异常会打印为 `操作失败或取消：<原因>`。

**Q：文档里的 `.artifacts/` 目录不见了？**
A：2026-09 的清理中已删除该临时工作目录（渲染图、旧构建目录、一次性脚本）。需要发布级证据请重新运行 `scripts/verify-release.ps1`，它会在 `.artifacts/` 下新建带唯一后缀的验证目录。

---

## 10. 目录结构

```
CMakeLists.txt       构建定义（含 -Wall -Wextra -Wpedantic -Werror 门禁）
README.md            项目说明与模块清单
HELP.md              本文件
include/             四层架构头文件（account / friend / group / infra / network / persistence / ui）
                     ├ AccountDomainFanLX.h / FriendDomainFanLX.h / GroupDomainFanLX.h  域聚合入口
src/                 main.cpp + persistence / network / ui 实现
tests/               CTest 测试源与 fixtures
benchmarks/          固定种子缓存基准
docs/                设计、协议与分阶段验收记录
deliverables/        课程工程报告（docx）
scripts/             verify-release.ps1 发布验证
third_party/         nlohmann/json.hpp（固定版本，含许可）
data/                默认数据目录（运行时生成 state.json / .lock）
build/               构建输出（被 .gitignore 忽略，可随时删除重建）
```

代码入口：

- `include/persistence/PersistenceApplicationFanLX.h` — 应用写门面与查询缓存入口
- `include/ui/ConsoleAppFanLX.h` — 数字菜单控制台门面
- `include/persistence/RepositoryFanLX.h` — 可替换仓储接口
- `src/persistence/TransactionCoordinatorFanLX.cpp` — 候选提交与局部版本
- `src/persistence/PreparedCommitFanLX.cpp` — 预分配无异常发布

`#include` 路径一律以 `include/` 为根，例如 `#include "group/GroupFanLX.h"`。三个 `*DomainFanLX.h` 是该域全部类型的聚合入口，新读者可从它们入手。

---

## 11. 文档索引

| 文档 | 内容 |
| --- | --- |
| [README.md](README.md) | 项目概览、模块清单、当前阶段状态 |
| [docs/progress.md](docs/progress.md) | 阶段实施日志与后续路线 |
| [docs/architecture.md](docs/architecture.md) | 分层架构与职责划分 |
| [docs/cache-validation.md](docs/cache-validation.md) | 缓存接入、菜单测试与固定种子基准 |
| [docs/data-format.md](docs/data-format.md) | schemaVersion=1、文件协议与恢复边界 |
| [docs/dependencies.md](docs/dependencies.md) | 固定版本、许可与 SHA-256 |
| [docs/persistence-validation.md](docs/persistence-validation.md) | 持久化真实测试记录与未测故障边界 |
| [docs/network-validation.md](docs/network-validation.md) | 中心转发 IM 协议、时序与验收 |
| [docs/integration-validation.md](docs/integration-validation.md) | 任务书条款到实现、测试的对应 |

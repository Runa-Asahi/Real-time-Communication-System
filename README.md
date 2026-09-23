# 模拟即时通讯系统

C++17 面向对象课程设计 · 2026-2027 学年第一学期 · 552502 班

S06 已接入文件持久化、候选事务、稳定群外壳、恢复确认与启动/保存关闭；S07 已接入版本隔离的 ARC 查询缓存、LFU 衰减、内存池和固定种子基准；S09 数字菜单已接入。S08 已增量升级为 QQ 中心转发 IM（菜单 6）：登录长连接、单聊/群聊、离线 ACK、好友在线状态和重连去重，S10 已补齐应用事务权限矩阵和缓存开关集成对照。

## 构建与测试

Windows、CMake 3.20+、MinGW（本机 GCC 15.1.0，D:/mingw64），不依赖 Visual Studio。固定 JSON 头文件及 MIT 许可证随源码提供；CMake 配置/构建无需联网。

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -- -j4
ctest --test-dir build --output-on-failure
.\build\fanlx_app.exe --demo
```

九项 CTest：smoke、cache_behavior、group_behavior、persistence_behavior、query_cache_behavior、console_behavior、network_behavior、im_behavior、integration_behavior；另有 cache_benchmark。Debug/Release 均启用关键断言，新目标以 -Wall -Wextra -Wpedantic -Werror 编译。

## 真实数据目录

```powershell
# 显式创建新演示库；已有主文件/备份时拒绝初始化覆盖
.\build\fanlx_app.exe --data-dir data-classroom --init-demo
# 第二次启动：读取已有快照，检查保存并关闭
.\build\fanlx_app.exe --data-dir data-classroom
# 主文件损坏且备份有效时：显式确认恢复（先保留损坏证据）
.\build\fanlx_app.exe --data-dir data-classroom --recover
```

无参数使用当前目录的 data 并进入数字菜单。空库/损坏库不会被自动用空状态覆盖，启动时按提示初始化或确认恢复。

--demo 使用系统临时目录下的唯一 fanlx-demo-* 子目录，输出证据目录；展示真实写入、无损切换、关闭重开、全字段比较与登录态清空。测试只清理自己创建的唯一临时目录，失败时保留并输出路径。没有全局数据重置命令。

同一数据目录只允许一个进程持有 .lock 独占句柄；不要删除锁文件抢占。异常进程终止后 Windows 释放句柄，可直接重启。

## 目录结构

源码按四层架构分目录。此次 IM 按“只改现有代码”要求，将客户端/服务端会话增量放入现有 TcpPeer/TcpListener 文件。

```
include/
├── AccountDomainFanLX.h       # 账户域入口（聚合全部账户类型）
├── FriendDomainFanLX.h        # 好友域入口（聚合全部好友类型）
├── GroupDomainFanLX.h         # 群域入口（聚合全部群类型）
├── account/                   # 账户域：资料 / 平台账号 / 开通 / 会话 / 绑定（10 文件）
├── friend/                    # 好友域：目录 / 身份解析 / 门面 / 快照（5 文件）
├── group/                     # 群域：成员 / 角色 / 权限 / 工作流 / 子群 / 策略族（17 文件）
├── infra/                     # 基础设施：2Q / ARC / LFU / 内存池 / 查询缓存（9 文件）
├── persistence/               # 持久化：仓储 / 编解码 / 校验 / 原子写 / 事务 / 恢复（15 文件）
├── network/                   # Winsock、长度帧、TCP、有界队列、IM客户端/服务端
└── ui/                        # 数字菜单与 QQ 聊天应用门面
src/
├── main.cpp                   # 演示与启动入口
├── persistence/               # 持久化层实现（9 个 .cpp）
├── network/                   # 传输与消息编解码实现
└── ui/                        # 数字菜单与聊天门面实现
benchmarks/
└── cache_benchmark.cpp        # 固定种子缓存/回源对照基准
tests/
├── cache_tests.cpp            # 缓存与好友域断言测试
├── group_tests.cpp            # 群域权限矩阵与无损切换
├── persistence_tests.cpp      # T11/T12 与故障注入
├── query_cache_tests.cpp      # T13 查询缓存、版本失效与内存池
├── console_tests.cpp          # S09 菜单输入、返回、EOF 与保存
├── network_tests.cpp          # T14 拆包/断连/有界队列与双进程中文收发
└── integration_tests.cpp      # S10 事务权限、缓存开关对照与重启
```

约定：`#include` 路径一律以 `include/` 为根（如 `#include "group/GroupFanLX.h"`）；三个 `*DomainFanLX.h` 是该域全部类型的聚合入口，新读者可从它们入手。

## 文档与代码入口

- HELP.md：构建、命令行参数、菜单速查、IM 三终端联调、测试与基准、数据与锁、排错 FAQ。新读者从这里开始。
- docs/progress.md：阶段实施日志与后续路线。
- docs/cache-validation.md：S07 缓存接入、菜单测试与固定种子基准。
- docs/data-format.md：schemaVersion=1、文件协议与恢复边界。
- docs/dependencies.md：固定版本、许可、SHA-256。
- docs/persistence-validation.md：真实测试记录与未测故障边界。
- include/persistence/PersistenceApplicationFanLX.h：应用写门面与查询缓存入口。
- include/ui/ConsoleAppFanLX.h：数字菜单控制台门面。
- include/persistence/RepositoryFanLX.h：可替换仓储接口。
- src/persistence/TransactionCoordinatorFanLX.cpp：候选提交与局部版本。
- src/persistence/PreparedCommitFanLX.cpp：预分配无异常发布。
- tests/persistence_tests.cpp：T11/T12、子进程重启/崩溃与故障注入。
- tests/query_cache_tests.cpp、tests/console_tests.cpp：T13 查询缓存与菜单断言。
- tests/fixtures/persistence/valid-state-v1.json：程序实际生成的合法快照。

课程原始任务书保持原样。deliverables 中工程报告已同步 S08/S10 实施日志与当前模块清单；约 5 页正式报告和发布冻结仍属 S11。
`scripts/verify-release.ps1` 执行独立 Release 构建和全量 CTest，自动保存日志及源码 SHA-256 清单；可传入 `-Configuration Debug`。`scripts/im-tokens.ps1` 生成并分发 IM 登录令牌：新增账号时用它统一改写服务端与客户端令牌文件（服务端需重启后生效）。S10 已补齐条款追踪和跨域集成回归；S11 最终报告与发布冻结尚待完成。

中心转发 IM 的启动命令、假设、协议、数据结构、关键时序及验收标准见 [network-validation.md](docs/network-validation.md)。任务书条款到实现、测试的对应见 [integration-validation.md](docs/integration-validation.md)。服务器与各客户端须使用不同数据目录。通过 --im-server 启动中心服务；客户端使用 --im-host / --im-port / --im-tokens 配置后登录。服务器领域数据决定好友/群权限；消息协议不负责同步客户端领域菜单的改动。旧 P2P 菜单已移除，底层 v1 仅用于兼容回归。

# 模拟即时通讯系统

C++17 面向对象课程设计 · 2026-2027 学年第一学期 · 552502 班

S06 已接入文件持久化、候选事务、稳定群外壳、恢复确认与启动/保存关闭；S07 已接入版本隔离的 ARC 查询缓存、LFU 衰减、内存池和固定种子基准；S09 数字菜单已接入。S08 已增量升级为 QQ 中心转发 IM（菜单 6）：登录长连接、单聊/群聊、离线 ACK、好友在线状态和重连去重，S10 已补齐应用事务权限矩阵和缓存开关集成对照。2026-09-30 起 IM 与控制台登录统一为「用户名 + 用户自选密码」，账号库即领域快照里的账号，令牌文件与 `--im-tokens` / `--tokens` 已全部移除。

2026-10 又完成一轮功能改造（用户提出的 5 条）：**微X（WeiX）成为 QQ / 微信 / 微博 的上层账号**——登录身份只有微X号，注册只创建微X账号、**不捆绑任何服务账号**，登录后在「开通管理」里逐个开通下级服务；QQ / 微信 / 微博 的**好友与群分别存储**（state.json 里 `friends` / `groups` 是按服务分区的对象，schemaVersion=2）；聊天**跟随当前服务**（QQ 连接只收发 QQ 的好友与群，微信连接只收发微信的）；聊天界面的会话列表用 **[好友]（蓝）/ [群]（紫）** 区分两类会话；登录失败（微X号不存在 / 密码错 / 服务端拒绝）一律**留在登录界面**重试，不再带着未认证身份进主界面；昵称与所在地支持**单项修改**（留空表示该项不改）。这 5 条都有自动化回归（见下文测试清单）。

第二轮（同为用户要求）：**微博补齐"除群以外"的全部功能**——QQ / 微信 / 微博 三种下级服务都能单聊（资料、开通、登录联动、好友增删改查与共同好友、历史查询、在线状态都可用），**只有 QQ 与微信有群**。因为 QQ 与微博按课程约定共用同一个号码，服务端内部一律按 **`服务|号码`** 分通道（消息库收件人、投递游标、会话序号都带通道前缀），出站帧仍只带纯号码，因此客户端协议与聊天库格式不变；客户端聊天目录也按服务分一层（`<账号目录>/<服务名>/chat.db`），同号码的两条通道各有各的库与目录锁。`scripts\run-im-server.bat` 现在会自动识别并重建两代旧数据目录（schemaVersion=1 或缺少 `password`），把它们改名保留为 `<数据目录>.legacy-<时间戳>` 后重新初始化，双击即可启动服务端。

## 构建与测试

Windows、CMake 3.20+、MinGW（本机 GCC 15.1.0，D:/mingw64），不依赖 Visual Studio。固定 JSON 头文件及 MIT 许可证随源码提供；CMake 配置/构建无需联网。

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -- -j4
ctest --test-dir build --output-on-failure
.\build\fanlx_app.exe --demo
```

基础版（含 SQLite 聊天记录）注册 11 项 CTest；启用 GUI 扩展后为 14 项（另有数据库十万条性能项，标签 `perf`，默认不跑）。Debug/Release 均启用关键断言，新目标以 -Wall -Wextra -Wpedantic -Werror 编译。

```powershell
# 可视化扩展（需要 Qt 6.11.1；基础版不需要 Qt）
cmake -S . -B build -G "MinGW Makefiles" -DFANLX_BUILD_GUI=ON -DCMAKE_PREFIX_PATH="E:/Qt/6.11.1/mingw_64"
cmake --build build -- -j4
ctest --test-dir build --output-on-failure
```

启用 GUI 后新增的三项测试：

- `gui_behavior`：聊天链路（控制器 / 模型 / 两个 SQLite 客户端真实收发），并断言会话列表里群与好友的区别标志、登录被拒时控制器始终不在线、按服务分流（QQ 连接看不到微信好友、QQ 好友关系不能发微信单聊）。
- `gui_domain_behavior`：GUI 领域层，逐条覆盖课设要求 1—6 与本次 5 条改造（单项资料修改、注册微X不捆绑服务、微X之下开通三种服务、登录失败分类、好友与群按服务分区落盘），并断言四个页签与各分区控件真的显示在界面上。
- `gui_three_client_behavior`：三客户端真实收发 + 六个页面离屏截图为 PNG（交付证据）。

GUI 启动：`build\fanlx_gui.exe --host 127.0.0.1 --port 9500 --account 2026001 --password 123`。登录用「微X号 + 注册时自选的密码」，账号库就是领域快照 `state.json` 里的 `users[]`（含明文 `password`）。演示数据里 **微X号 = 原来的号码**（2026001 这类），QQ 号与微博号沿用微X号、微信号是 `wx_<微X号>`，因此演示命令与旧脚本仍然可用；**演示密码统一为 `123`**。没有微X号时在登录窗口点「注册微X账号…」新建（注册只建微X，不建任何服务账号）。三个客户端可共用一个数据根（默认 `data-im-gui`，用 `--data-root` 覆盖）：聊天目录按 `<根>/im-client/<host>_<端口>/<服务账号>/<服务名>` 派生（领域库在同层的 `domain` 子目录），账号之间天然隔离、同一账号的不同服务也各有各的聊天库。服务端使用 `--im-sqlite` 启用 SQLite 消息库。历史查询支持会话、发送者、关键词和 UTC 时间范围。

### GUI 的四个页签

主窗口把课程要求的全部功能放在同一窗口的四个页签里，登录后直接可见，不需要逐层回车：

| 页签 | 内容 | 对应课设要求 |
| --- | --- | --- |
| 聊天 | 会话列表（`[好友]` 蓝 / `[群]` 紫）+ 消息列表 + 发送/更早/最新/重连、历史查询、授权目录；聊天跟随"当前服务"（QQ / 微信 / 微博 都能单聊） | 一、6（5）TCP 收发（中心转发，取代旧 P2P 菜单） |
| 账号与服务 | 微X号与用户基本信息（昵称/出生时间/申请时间/T龄/所在地/好友与群列表）、开通管理（在微X之下开通 QQ/微信/微博 并填写号码）、登录管理、绑定管理、资料与密码维护（昵称 / 所在地 可单项修改） | 一、1、一、4、一、5、一、6（2） |
| 好友 | 当前服务的好友列表、添加/删除/修改备注标签、同服务共同好友、跨服务推荐与共同好友 | 一、2（1）（2） |
| 群 | 当前命名空间的群列表（1001—1006）、群成员、群特色功能展示与动态切换、申请/批准/推荐/确认/退群/挨 T/管理员/转让/子群/解散/改名/建群（**只有 QQ 与微信有群**） | 一、3（1）（2）（3）、一、6（4） |

**注册入口在登录界面**（「注册微X账号…」按钮），不在登录后的"账号与服务"页面：注册产出的是登录身份本身，注册时只写一个微X账号 + 密码 + 资料，不创建任何 QQ / 微信 / 微博 服务账号。只有以微X身份登录后，「开通管理」才能为本人开通下级服务（领域层 `AccountSessionFanLX::activateService` 会拒绝非微X会话的请求）。

**三种服务的聊天数据也分开**：领域里的好友目录按服务分区（`FriendDirectorySetFanLX`），落盘时 `friends` / `groups` 是按服务分区的对象；IM 连接固定属于一个服务（QQ / 微信 / 微博），服务端按 `(服务, 号码)` 认证并只返回该服务的好友与群目录，因此 QQ 的好友关系不会给微信或微博单聊授权。**微博可以正常单聊**（发消息、历史、在线状态、好友增删改查都可用），只是没有群：群列表为空、群操作会被明确拒绝。服务端内部按 `服务|号码` 分通道，所以即使 QQ 与微博共用同一个号码，两条通道的消息、未读与历史也互不串。微X 是上层登录身份、没有聊天通道，切到它时聊天页会明确提示该怎么切。

领域页签由 `DomainControllerFanLX` 驱动，它持有一个 `PersistenceApplicationFanLX`，库目录是
`<根>/im-client/<host>_<端口>/<账号>/domain`（见 `imClientPathFanLX::domainDirectoryFanLX`）。
放在**账号目录的下一层**而不是同一层，是因为聊天库与领域库各自要独占自己那层的 `.lock`——
同目录会互相拒绝启动。按账号派生则保证一个数据根可以同时跑多个 GUI 客户端（各自一个"本人"视角的
完整领域副本）。首次启动时该目录为空，会自动写入与 `--init-demo` 相同的演示数据；
若库损坏或 schema 不受支持，只报告原因并退化为"只有聊天页签"，绝不静默覆盖。

写操作经候选事务立即原子落盘（每次成功操作都会 +1 次提交，界面状态栏可见），关窗时再做一次收尾提交并释放目录锁，因此"断电保存"不依赖析构时机。

其它开关：`--skip-domain` 只开聊天页签；`--no-confirm-others` 只登录微X身份，不联动本人已开通的下级服务。

新注册的微X号按课设要求**不捆绑任何下级服务**，因此它一开始没有聊天通道：这时窗口仍会打开（身份已认证），
聊天页显示"未连接"并提示去「账号与服务 → 开通管理」开通 QQ / 微信 / 微博；开通后聊天页自动连上，
不需要退出重进，也不需要重启服务端（开通时会顺带把该服务登记到服务端账号库）。

无人值守演示用的两个等价入口：`--register`（注册微X账号：本机 + 服务端各登记一次）与
`--activate <QQ|微信|微博>`（登录后开通该服务），例如
`build\fanlx_gui.exe --register --account 2026099 --password pw-2026099 --data-root data-im-gui --host 127.0.0.1 --port 9500`。

## 数据都存在哪（工程师速查）

完整版见 **[docs/storage-layout.md](docs/storage-layout.md)**（每个文件是什么、谁是权威副本、跨进程怎么同步）。一页速查：

| 位置 | 内容 | 权威性 |
| --- | --- | --- |
| 服务端 `<数据根>\state.json` | 账号库：微X号与服务号、密码、开通关系、好友关系（镜像）、群 | **认证与授权的权威副本** |
| 服务端 `<数据根>\im-server\server-chat.db` | 会话 / 消息 / 收件人快照 / 投递游标（`--im-sqlite`） | 消息的权威副本 |
| 服务端 `<数据根>\im-server\server-identity.json` | 服务器实例标识（客户端校验聊天库归属） | — |
| 客户端 `<数据根>\im-client\<host>_<port>\<微X号>\domain\state.json` | 本机领域库：资料、好友、群、开通、绑定 | 用户操作的那一份 |
| 客户端 `<数据根>\im-client\<host>_<端口>\<微X号>\<QQ\|Wechat\|Weibo>\chat.db` | 本机聊天库（每服务一份，含各自的 `.lock` 与 `client-identity.json`） | 本地历史副本 |

**好友关系同理会"对不上"**：本机加的好友只在本机副本里，服务端按它自己的账号库判定"是不是好友"。
客户端因此在两处与服务端对齐：添加好友前用 `LOOKUP` 向服务端确认对端账号（对端只存在于服务端时导入本机副本），
添加成功后再随 `REGISTER` 把关系镜像到服务端。控制台需要在启动时给 `--im-host / --im-port` 才能做这两步。

**两份账号库靠 IM 的 `REGISTER` 对齐**：客户端在登录前（以及登录被拒 / 开通服务后）把
「微X号 + 密码 + 资料 + 本机已开通的服务 + 本机已加的好友」登记到服务端，服务端用候选事务写进自己的
持久化快照；认证前再"读穿"一次该快照。因此 **注册完不需要重启服务端**，服务端重启后账号也还在。
细节与逐步验证见 storage-layout 第 4 节；自动化回归是 `gui_behavior` 的 t69。

## 真实数据目录

```powershell
# 显式创建新演示库；已有主文件/备份时拒绝初始化覆盖
.\build\fanlx_app.exe --data-dir data-classroom --init-demo
# 第二次启动：读取已有快照，检查保存并关闭
.\build\fanlx_app.exe --data-dir data-classroom
# 主文件损坏且备份有效时：显式确认恢复（先保留损坏证据）
.\build\fanlx_app.exe --data-dir data-classroom --recover
```

无参数使用当前目录的 data 并进入数字菜单。空库/损坏库不会被自动用空状态覆盖，启动时按提示初始化或确认恢复。两类历史快照加载会失败并给出可照做的原因，迁移方式都是删掉该 `data-*/` 目录后重新执行一次 `--init-demo`（程序不做就地改写）：凭据改造之前的文件报 `用户缺少登录密码（旧数据请重新 --init-demo 初始化）`；分区改造之前、schemaVersion=1 的文件报 `未知 schemaVersion：本版本读写 schemaVersion=2（好友与群按服务分区；旧库请备份后重新初始化）`。

**不想手工重建就直接用启动脚本**：`scripts\run-im-server.bat`（以及它调用的 `run-im-server.ps1`）会在启动服务端之前检查 `<数据目录>/state.json`，命中"空目录 / schemaVersion=1 / 缺 password"任一情形就把旧目录**改名保留**为 `<数据目录>.legacy-<yyyyMMdd-HHmmss>`，然后自动 `--init-demo` 重建并继续启动；`-DryRun`（或 `FANLX_DRY_RUN=1`）只报告计划、不改动任何目录。

--demo 使用系统临时目录下的唯一 fanlx-demo-* 子目录，输出证据目录；展示真实写入、无损切换、关闭重开、全字段比较与登录态清空。测试只清理自己创建的唯一临时目录，失败时保留并输出路径。没有全局数据重置命令。

同一数据目录只允许一个进程持有 .lock 独占句柄；不要删除锁文件抢占。异常进程终止后 Windows 释放句柄，可直接重启。

## 目录结构

源码按四层架构分目录。此次 IM 按“只改现有代码”要求，将客户端/服务端会话增量放入现有 TcpPeer/TcpListener 文件。

```
include/
├── AccountDomainFanLX.h       # 账户域入口（聚合全部账户类型）
├── FriendDomainFanLX.h        # 好友域入口（聚合全部好友类型）
├── GroupDomainFanLX.h         # 群域入口（聚合全部群类型）
├── account/                   # 账户域：资料 / 服务账号（含 WeiX 上层）/ 开通 / 会话 / 绑定（10 文件）
├── friend/                    # 好友域：按服务分区的目录集合 / 单服务目录 / 身份解析 / 门面 / 快照（6 文件）
├── group/                     # 群域：成员 / 角色 / 权限 / 工作流 / 子群 / 策略族（17 文件）
├── infra/                     # 基础设施：2Q / ARC / LFU / 内存池 / 查询缓存（9 文件）
├── persistence/               # 持久化：仓储 / 编解码（schemaVersion=2 分区格式）/ 校验 / 原子写 / 事务 / 恢复（16 文件）
├── network/                   # Winsock、长度帧、TCP、有界队列、IM 客户端/服务端（按服务认证）
├── db/                        # 扩展：SQLite 聊天记录（封装 / 仓储 / 迁移 / DB 工作线程）
├── gui/                       # 扩展：Qt 图形客户端（控制器 / 模型 / 登录窗 / 注册窗 / 历史窗 / 主窗口 / 领域门面 / 三个领域页面）
└── ui/                        # 数字菜单与聊天应用门面（按当前服务建连）
src/
├── main.cpp                   # 演示与启动入口
├── persistence/               # 持久化层实现
├── network/                   # 传输与消息编解码实现
├── db/                        # 扩展：SQLite 聊天记录实现
├── gui/                       # 扩展：Qt 客户端实现与 GUI 入口
└── ui/                        # 数字菜单与聊天门面实现
benchmarks/
└── cache_benchmark.cpp        # 固定种子缓存/回源对照基准（含写放大扫描，输出 CSV）
tools/
└── gui_screenshot.cpp         # 三客户端真实收发 + 六个页面离屏截图为 PNG（交付证据）
tests/
├── cache_tests.cpp            # 缓存与好友域断言测试
├── group_tests.cpp            # 群域权限矩阵与无损切换
├── persistence_tests.cpp      # T11/T12 与故障注入
├── query_cache_tests.cpp      # T13 查询缓存、版本失效与内存池
├── console_tests.cpp          # S09 菜单输入、返回、EOF 与保存
├── network_tests.cpp          # T14 拆包/断连/有界队列与双进程中文收发
├── chat_history_tests.cpp     # 扩展：SQLite 聊天库 14 个用例（--perf 为十万条测量）
├── gui_tests.cpp              # 扩展：图形层离屏用例（聊天链路）
├── gui_domain_tests.cpp       # 扩展：GUI 领域层（账号/好友/群页面与四页签装配）
└── integration_tests.cpp      # S10 事务权限、缓存开关对照与重启
```

SQLite 以系统 `winsqlite3.dll` 的 C API 接入，仓库只保存官方 `third_party/nlohmann/sqlite/sqlite3.h`，构建时用 `gendef`/`dlltool` 现场生成导入库，不保存任何第三方二进制。

约定：`#include` 路径一律以 `include/` 为根（如 `#include "group/GroupFanLX.h"`）；三个 `*DomainFanLX.h` 是该域全部类型的聚合入口，新读者可从它们入手。

## 文档与代码入口

- HELP.md：构建、命令行参数、菜单速查、IM 三终端联调、测试与基准、数据与锁、排错 FAQ。新读者从这里开始。
- docs/project-structure.md：**目录结构、分层依赖、扩展新增模块、构建与运行命令、测试清单**。
- docs/extension-plan.md：可视化/多线程/数据库扩展的实施计划（E01–E06）。
- docs/handoff-prompt.md：**交接提示词**——当前状态、全部已知问题与复现方法、硬性约束、剩余工作优先级、已修缺陷清单。
- docs/progress.md：阶段实施日志与后续路线；末尾有扩展的当前收尾状态与未解决项。
- docs/cache-validation.md：S07 缓存接入、菜单测试与固定种子基准。
- docs/data-format.md：schemaVersion=2（好友与群按服务分区）、文件协议与恢复边界。
- docs/dependencies.md：固定版本、许可、SHA-256。
- docs/persistence-validation.md：真实测试记录与未测故障边界。
- include/persistence/PersistenceApplicationFanLX.h：应用写门面与查询缓存入口。
- include/ui/ConsoleAppFanLX.h：数字菜单控制台门面。
- include/persistence/RepositoryFanLX.h：可替换仓储接口。
- include/account/ServiceTypeFanLX.h：服务枚举（WeiX 上层 + QQ/微信/微博 下级）与显示名换算。
- include/friend/FriendDirectorySetFanLX.h：**按服务分区的好友目录集合**（三种服务不共享好友的落点）。
- src/persistence/TransactionCoordinatorFanLX.cpp：候选提交与局部版本。
- src/persistence/PreparedCommitFanLX.cpp：预分配无异常发布。
- tests/persistence_tests.cpp：T11/T12、子进程重启/崩溃与故障注入（含 v1 快照被拒、微X 开通规则）。
- tests/query_cache_tests.cpp、tests/console_tests.cpp：T13 查询缓存与菜单断言（含登录口注册）。
- tests/gui_tests.cpp、tests/gui_domain_tests.cpp：GUI 聊天链路与领域层（含本次 5 条改造的回归）。
- tests/fixtures/persistence/valid-state-v2.json：程序实际生成的合法快照（schemaVersion=2）。
- tests/fixtures/persistence/valid-state-v1.json：旧格式样例，仅用于断言"旧 schema 必须被拒绝"。

课程原始任务书保持原样。deliverables 中工程报告已同步 S08/S10 实施日志与当前模块清单；约 5 页正式报告和发布冻结仍属 S11。
`scripts/verify-release.ps1` 执行独立 Release 构建和全量 CTest，自动保存日志及源码 SHA-256 清单；可传入 `-Configuration Debug`。`scripts/im-tokens.ps1` 曾用于生成并分发 IM 登录令牌文件；2026-09-30 起登录改为「用户名 + 密码」，账号库直接取自领域快照，该脚本随之删除，不要再引用（历史机制）。S10 已补齐条款追踪和跨域集成回归；S11 最终报告与发布冻结尚待完成。

中心转发 IM 的启动命令、假设、协议、数据结构、关键时序及验收标准见 [network-validation.md](docs/network-validation.md)。任务书条款到实现、测试的对应见 [integration-validation.md](docs/integration-validation.md)。服务器与各客户端须使用不同数据目录。通过 `--im-server` 启动中心服务；客户端使用 `--im-host` / `--im-port` 配置地址后用**微X号 + 密码**登录——控制台在菜单 1 输入，GUI 另以 `--account 微X号 --password 登录密码` 传入。登录身份只有微X：QQ / 微信 / 微博 是登录之后在「开通管理」里逐个开通的下级服务；聊天的连接身份是"当前服务"的服务账号（QQ 号 / 微信号），服务端按 `(服务, 号码)` 认证并只返回该服务的好友与群目录。服务器领域数据决定好友/群权限；消息协议不负责同步客户端领域菜单的改动。旧 P2P 菜单已移除，底层 v1 仅用于兼容回归。

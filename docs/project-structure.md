# 项目结构（Ciallo 即时通讯系统）

本文说明仓库的目录结构、各模块职责、构建方式与交付物位置。
实时状态与验收证据见 [progress.md](progress.md)；计划见 [extension-plan.md](extension-plan.md)。

---

## 1 目录总览

```
Ciallo/
├─ CMakeLists.txt            单一构建入口（基础版默认构建，GUI 由开关启用）
├─ README.md  HELP.md        项目说明 / 使用帮助
├─ data-README.md            运行期 data-* 目录哪些能删
├─ 2026-面向对象程序课程设计.doc   原始任务书（未修改）
│
├─ include/                  头文件（按四层架构分目录，include 路径以此为根）
│   ├─ account/   (11)       账号域：用户资料、服务账号、开通、统一登录、会话、错误分类
│   ├─ friend/    ( 6)       好友域：邻接目录、按服务分区、关系细节、身份解析、门面、快照
│   ├─ group/     (17)       群域：群策略族、群、成员、子群、申请/邀请、服务与快照
│   ├─ infra/     ( 9)       基础设施：2Q/LRU-K、ARC、LFU、内存池、查询缓存与统计
│   ├─ persistence/(16)      持久化：仓储接口、JSON 编解码、校验、原子写、目录锁、事务
│   ├─ network/              网络：帧编解码、TcpPeer/监听、IM 客户端/服务端、DirectLinkFanLX、P2pProtocolFanLX
│   ├─ db/        ( 8)       【扩展】聊天记录数据库：SQLite 封装、仓储、服务端消息库、迁移、DB 线程
│   └─ gui/       (10)       【扩展】图形客户端：控制器、会话/消息模型、登录窗、历史窗、主窗口、领域门面、三个领域页面、注册对话框
│
├─ src/                      实现（与 include 同层对应）
│   ├─ persistence/(10)      含 NamedFilePublisherFanLX（具名文件原子发布）
│   ├─ network/              MessageMailbox / TcpPeer / TcpListener / ImClient / ImServer / DirectLinkFanLX
│   ├─ db/        ( 7)       【扩展】SQLite 仓储、服务端消息库、迁移、数据库工作线程
│   ├─ gui/       (11)       【扩展】控制器、模型、登录窗、历史窗、主窗口、领域门面、三个领域页面、注册对话框、GUI 入口
│   ├─ ui/        ( 2)       控制台数字菜单（课程必做交付）
│   └─ main.cpp              控制台程序入口（--demo / --init-demo / --menu / --im-* / --im-migrate）
│
├─ tests/                    含 p2p_tests.cpp。启用 GUI 时 CTest 比原先多一项 p2p_behavior
│   ├─ network_tests.cpp     T14 协议与 TCP；`--im` 为 IM 长连接/离线/群/重启/ACK 用例
│   ├─ p2p_tests.cpp         在线好友单聊直连与回退
│   ├─ chat_history_tests.cpp【扩展】数据库 18 个用例（T20–T56）；`--perf` 为十万条测量
│   ├─ gui_tests.cpp         【扩展】图形层 8 个用例（离屏，t60–t64、t66–t68；含微博通道 t68）
│   ├─ gui_domain_tests.cpp  【扩展】领域页签用例（离屏，t70–t82）
│   └─ cache/console/group/integration/persistence/query_cache_tests.cpp   原有域与集成回归
│
├─ resources/                Qt 资源：样式表、应用图标、会话列表图标（icons/）
├─ tools/                    【扩展】gui_screenshot：三客户端真实收发 + 窗口离屏截图为 PNG
├─ benchmarks/               cache_benchmark（固定种子）；pool_benchmark（对象池与 new/delete 对照）
├─ scripts/                  启动、部署、发布包（含 release/Ciallo.bat）
├─ third_party/
│   ├─ nlohmann/json.hpp     JSON 库（v3.11.3，MIT，随源码离线提供）
│   └─ sqlite/sqlite3.h      【扩展】官方 SQLite 头文件（3.51.1，公共领域）
├─ docs/                     架构、数据格式、依赖、各阶段验收记录、扩展计划与进度
├─ deliverables/             工程报告（.docx）与界面截图证据（gui-screenshots/*.png）
└─ data*/ , build/           运行期数据目录与构建输出（均被 .gitignore 忽略，不要打进作业包）

> 运行期数据目录布局：数据根统一为 `data`（默认）/ `data-im-server`（服务端）/ `data-im-a`、`data-im-b`
> （控制台实例）/ `data-im-gui`（GUI，单根多账号）。客户端存储目录由「服务器身份 + 登录账号 + 服务」派生：
> 领域库在 `<根>/im-client/<host>_<端口>/<账号>/domain/`，**聊天库再按服务分一层**为
> `<根>/im-client/<host>_<端口>/<账号>/<QQ|Wechat|Weibo>/`（各一份 `chat.db` 与一把目录锁，
> 因此 QQ 与微博共用同一个号码时也不会互相覆盖）；规则见 `include/network/ClientDataPathFanLX.h`，
> 设计理由见 [architecture.md](architecture.md) 的「数据目录与身份边界」。
>
> 服务端数据目录可用 `scripts\run-im-server.bat` 直接启动：脚本会先读 `<数据目录>/state.json`
> 判断格式——空目录直接 `--init-demo`；`schemaVersion=1` 或缺少 `users[].password` 的旧库先
> **改名保留**为 `<数据目录>.legacy-<时间戳>`（不删证据）再重建，然后照常启动；`-DryRun`（或
> `FANLX_DRY_RUN=1`）只打印计划、不改动任何目录。`.bat` 本身只含 ASCII 并转发给
> `run-im-server.ps1`，后者必须保持 **UTF-8 with BOM**（否则 PowerShell 5.1 按 GBK 读会乱码）。
```

> 构建输出与截图证据均可重新生成：删除 `build/` 后重跑 3 节的构建命令即可；
> 截图用 `gui_screenshot.exe --out deliverables\gui-screenshots` 重出（见 HELP.md 第 7 节）。

---

## 2 分层与依赖方向

```
        ui/ (控制台菜单)              gui/ (Qt 可视化客户端)   ← 【扩展】
              \                        /
               \                      /
                network/ (Winsock + IM 客户端/服务端)
                        |
                   db/ (SQLite 聊天记录)   ← 【扩展】
                        |
   persistence/ (JSON 快照事务、目录锁、原子写)
                        |
      group/  friend/  account/   (领域规则)
                        |
                   infra/ (缓存、内存池)
```

- 领域层（`account/`、`friend/`、`group/`）**不依赖** JSON、Win32、SQLite 或 Qt。
- `db/` 只依赖 `persistence/`（复用原子发布）与官方 `sqlite3.h`；不依赖 Qt。
- `gui/` 只依赖 `network/`、`persistence/` 与 Qt；界面不直接触碰 SQL 或 socket。
- 构建目标与依赖：`nlohmann_json` → `fanlx_persistence` → `fanlx_chat` → `fanlx_network`
  → `fanlx_console`（控制台）/ `fanlx_gui_core`（Qt，可选）。

### GUI 类图（组合 / 继承 / 多态）

```
             ┌──────────────────────┐
             │  QMainWindow         │  Qt 基类
             └──────────┬───────────┘
                        │ 继承
             ┌──────────┴───────────┐        ┌──────────────────────┐
             │  MainWindowFanLX     │◆──────▶│ QTabWidget (Qt)      │
             │  四页签宿主 + 状态栏  │        └──────────────────────┘
             └───┬──────────┬───┬───┘
                 │组合      │   │  组合（页面只持有门面引用，不持有领域对象）
   ┌─────────────┘          │   └───────────────┐
   ▼                        ▼                   ▼
┌────────────────────┐ ┌──────────────────┐ ┌────────────────────┐
│ ChatControllerFanLX│ │AccountPageFanLX  │ │FriendPageFanLX     │
│ 聊天链路（QObject）│ │账号/开通/登录/绑定│ │好友增删改查/共同好友│
└─────────┬──────────┘ └────────┬─────────┘ └─────────┬──────────┘
          │ 组合                │ 引用                │ 引用
          ▼                     ▼                     ▼
┌────────────────────┐ ┌──────────────────────────────────────────┐
│ ImClientFanLX      │ │ DomainControllerFanLX   (QObject 门面)    │
│ TCP + SQLite 线程  │ │ 唯一持有领域库；异常 → operationFinished  │
└────────────────────┘ └───────────────┬──────────────────────────┘
                                       │ 组合
                       ┌───────────────┴───────────────┐
                       ▼                               ▼
        ┌────────────────────────────┐   ┌────────────────────────────┐
        │ PersistenceApplicationFanLX│   │ GroupPageFanLX             │
        │ 候选事务 + 查询缓存 + 目录锁│   │ 群管理 + 特色动态切换       │
        └───────────────┬────────────┘   └────────────────────────────┘
                        │ 组合（在线聚合体）
        ┌───────────────┴───────────────────────────────┐
        ▼                       ▼                       ▼
┌──────────────────┐  ┌───────────────────────┐  ┌──────────────────────────┐
│ AccountManager   │  │ FriendDirectorySet    │  │ GroupServiceFanLX        │
│ 用户/账号/开通/  │  │ 按服务分区的好友目录  │  │ 群注册表 + 申请/邀请/子群│
│ 绑定/登录态      │  │ (QQ/微信/微博 各一份) │  └───────────┬──────────────┘
│ (WeiX 是上层身份)│  └──────────┬────────────┘              │ 组合（策略）
└──────────────────┘             │ 组合                    ▼
                      ┌──────────┴───────────┐  ┌──────────────────────────────┐
                      │ FriendDirectoryFanLX │  │  GroupPolicyFanLX  «abstract»│
                      │ 单服务：邻接表 + 备注 │  │  canCreateDiscussion()       │
                      └──────────────────────┘  │  canAssignAdmin() / mode()   │
                                                └───────┬──────────────┬───────┘
                                                        │继承          │继承
                                             ┌──────────┴───┐   ┌──────┴────────────┐
                                             │QQGroupPolicy │   │WechatGroupPolicy  │
                                             │FanLX         │   │FanLX              │
                                             │允许子群+管理员│   │仅群主是特权账号   │
                                             └──────────────┘   └───────────────────┘
```

GUI 侧的分工：`ChatControllerFanLX` 管"消息"，`DomainControllerFanLX` 管"账号 / 好友 / 群"，
两个控制器互不引用；三个页面只通过门面的公开槽与 `dataChangedFanLX` / `operationFinishedFanLX`
信号交互，因此页面里没有一条 SQL、socket 或领域容器操作。
`switchGroupMode` 只替换 `GroupPolicyFanLX` 的实现对象，成员表与子群记录原地保留——
这就是课设要求"群成员数据不受伤害的动态变换"在类图上的落点。

---

## 3 扩展新增内容（E01–E04，约 3700 行）

| 位置 | 文件 | 职责 |
| --- | --- | --- |
| `include/db` `src/db` | `SqliteHandleFanLX` | 薄封装：连接/语句/事务 RAII、按序绑定、ASCII 折叠函数、有界忙碌重试 |
| | `ChatRepositoryFanLX` | 仓储接口与 DTO（消息、待发送、历史分页、会话摘要）、会话键与错误分类 |
| | `ChatSchemaFanLX` / `ChatSchemaStoreFanLX` | 客户端/服务端建表语句、版本与身份校验、计数器初值 |
| | `SqliteChatRepositoryFanLX` | 客户端聊天库：发送状态机、幂等、keyset 分页、会话摘要、迁移导入 |
| | `SqliteMessageStoreFanLX` | 服务端消息库：编号/序号分配、收件人快照、投递游标、幂等重放 |
| | `ChatMigrationFanLX` | 旧 IM JSON → SQLite 离线迁移与完整性校验；服务端日志迁移时账号 / 群号 / 会话键 / 投递游标一律补 `QQ\|` 前缀（旧服务端只有 QQ 一条通道） |
| | `DatabaseWorkerFanLX` | 数据库专用工作线程：有界队列、事件回调、取消标记、异常不逃逸 |
| `include/network` `src/network` | `MessageMailboxFanLX` | 每连接有界待发送缓冲（非阻塞推进、半帧状态、慢端判定） |
| | `ClientDataPathFanLX` | 客户端存储目录的唯一派生规则：`clientDirectoryFanLX`（服务器身份 + 账号）、`domainDirectoryFanLX`（领域库）、`chatDirectoryFanLX`（**聊天库按服务分层**：`<账号目录>/<QQ\|Wechat\|Weibo>`），并做字节级目录名转义 |
| | `ImClientFanLX` | 客户端：网络工作线程、发送状态机、数据库事件关联、授权目录、异步命令；连接固定属于一个服务（`service` 参数取 QQ / Wechat / Weibo），并提供"登录被拒"判定 |
| | `ImServerFanLX` | 服务端：按 `(服务, 号码, 密码)` 认证（接受 QQ / Wechat / Weibo）、幂等接收、投递状态机、非阻塞发送、按服务分区的目录应答；内部一律用**通道标识** `<服务名>\|<号码>` 存放与投递（含群号与会话键），出站帧剥回纯号码，微博的群列表恒为空、群操作报 `<服务名>没有群功能` |
| `include/gui` `src/gui` | `ChatControllerFanLX` | 唯一持有客户端的控制器：异步事件 → Qt 信号；聊天跟随"当前服务"（`switchIdentity`）、登录结果等待（`waitForLogin`）、会话类型标志（群/好友） |
| | `ConversationModelFanLX` | 会话列表模型（输出 `[好友] …` / `[群] …` 并按类型着色）+ 消息列表模型（含状态文案） |
| | `LoginDialogFanLX` `HistoryDialogFanLX` `MainWindowFanLX` | 登录窗（微X号 + 注册入口）、历史查询窗、主窗口（四页签宿主，随当前服务重建聊天连接） |
| | `DomainControllerFanLX` | 领域页签的唯一门面：持有 `PersistenceApplicationFanLX`，把账号/好友/群写的异常转成界面可读文案；`openWithOutcome` 区分"身份被拒"与"库不可用" |
| | `AccountPageFanLX` | 账号与服务页签：微X 与用户基本信息、开通管理（在微X之下开通 QQ/微信/微博 并填号码）、登录管理、绑定管理、资料与密码（昵称/所在地可单项修改） |
| | `FriendPageFanLX` | 好友页签：增删改查 + 同服务共同好友 + 跨服务推荐与共同好友 |
| | `GroupPageFanLX` | 群页签：群列表/成员/特色展示与动态切换/申请/推荐/挨T/管理员/转让/子群/解散/建群/改名 |
| | `RegisterDialogFanLX` | 注册对话框（登录窗口打开）：只创建微X号 + 密码 + 资料，**不捆绑任何服务账号** |
| | `main_gui.cpp` | `fanlx_gui` 入口与命令行参数：先领域登录、再按当前服务建连并等待认证结果 |
| `include/ui` `src/ui` | `ConsoleAppFanLX` | 控制台数字菜单（课程必做交付）：主菜单 1「登录微X或注册」、账号与服务菜单（开通 / 切换当前服务 / 绑定 / 单项改资料 / 改密）、聊天菜单按当前服务给选项——QQ / 微信 / 微博 都能单聊，微博不显示「3 群聊发送」并标注"（本服务没有群）"；聊天连接按 `(服务, 号码)` 建连、目录用 `chatDirectoryFanLX` 派生 |
| `tools` | `gui_screenshot.cpp` | 三客户端真实收发 + 六个页面离屏截图为 PNG |
| `include/persistence` `src/persistence` | `NamedFilePublisherFanLX` | 具名文件原子发布（身份/标记文件专用） |

---

## 4 构建与运行

```powershell
# 基础版（课程必做交付，不需要 Qt）
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -- -j4
ctest --test-dir build -E performance          # 默认测试；perf 单独跑

# 可视化客户端（需要 Qt 6；路径用 CMAKE_PREFIX_PATH 注入，不写死盘符）
cmake -S . -B build -G "MinGW Makefiles" -DFANLX_BUILD_GUI=ON -DCMAKE_PREFIX_PATH="E:/Qt/6.11.1/mingw_64"
cmake --build build -- -j4

# 把 Qt 运行依赖部署到目标目录（干净目录可直接双击运行）
powershell -File scripts/deploy-gui.ps1 -TargetDir build -QtBin "E:/Qt/6.11.1/mingw_64/bin" -MinGwBin "D:/mingw64/bin"
```

### 运行

```powershell
.\build\fanlx_app.exe --demo                    # 控制台功能演示（临时目录，不动仓库数据）
.\build\fanlx_app.exe                           # 数字菜单（课程必做）

# 三端联调：服务端 + 三个 GUI 客户端（共用一个数据根，账号子目录自动派生）
# 登录凭据为「微X号 + 用户自选密码」，演示数据密码统一为 123，无需任何令牌文件
.\build\fanlx_app.exe --data-dir data-im-server --init-demo
.\build\fanlx_app.exe --data-dir data-im-server --im-server --im-sqlite --im-host 127.0.0.1 --im-port 9500
# 或直接用启动脚本（会检查快照格式，旧库自动改名保留并重建）：scripts\run-im-server.bat
.\build\fanlx_gui.exe --host 127.0.0.1 --port 9500 --account 2026001 --password 123
.\build\fanlx_gui.exe --host 127.0.0.1 --port 9500 --account 2026002 --password 123
.\build\fanlx_gui.exe --host 127.0.0.1 --port 9500 --account 2026003 --password 123
# 加 --auto-login 可跳过登录窗口；--account 填的是微X号，聊天连接另按"当前服务"取该服务的号码
```

> 数据根默认 `data-im-gui`，可用 `--data-root` 覆盖；三个客户端共用一个根即可，无需各自指定目录。
> 真实存储目录由 `<根>/im-client/<host>_<端口>/<账号>` 派生，账号之间天然隔离——
> 隔离不再依赖人工约定目录名。**聊天库在该账号目录下再按服务分一层**（`QQ` / `Wechat` / `Weibo`），
> 领域库在同层的 `domain/`；因此 QQ 与微博共用同一个号码时各有各的 `chat.db`，不会互相覆盖。
> 同一账号被两个客户端同时打开仍会被目录独占锁拒绝（刻意的隔离）。
> 群 1001 的成员为 2026001/2026002/2026003，可直接演示三人群聊；
> 演示数据里 QQ 与微博各有一条 U1↔U2 好友关系，因此切到微博即可演示单聊（微博没有群）。

---

## 5 测试清单

| 名称 | 可执行文件 | 覆盖 |
| --- | --- | --- |
| network_behavior | network_tests | T14 协议边界、拆包粘包、资源上限 |
| im_behavior | network_tests --im | IM 长连接、离线投递、群增量、重启补发、双 ACK 丢失、幂等/伪造/权限、磁盘失败 |
| p2p_behavior | p2p_tests | 在线好友单聊直连、回退、群与跨服务仍走中转。见 [hybrid-tcp.md](hybrid-tcp.md) |
| integration_behavior | integration_tests | 跨域权限、事务、缓存对照、故障回归 |
| smoke | fanlx_app --demo | 控制台确定性演示 D01–D08 |
| cache_behavior | cache_tests | 2Q/ARC/LFU/内存池/好友域/统一登录 |
| group_behavior | group_tests | T06–T09 权限与工作流、T10 无损切换 |
| persistence_behavior | persistence_tests | T11/T12 往返、故障注入、子进程中断；演示种子 **2 条好友关系**（QQ 分区 U1↔U2、微博分区 U1↔U2，微信为空）、跨服务加好友被拒、QQ 与微博必须同号、微信可用与 QQ 相同的号码 |
| query_cache_behavior | query_cache_tests | T13 缓存、失效、统计 |
| console_behavior | console_tests | 菜单输入、返回、EOF、越权；登录口注册、微X 登录联动、单项资料修改、开通 QQ 不捆绑微博 |
| chat_history_behavior | chat_history_tests | T20–T56 数据库 18 个用例（t33 十万条仅 `--perf` 时跑）；t53 断言服务端旧 JSON 迁移后的账号 / 会话键 / 投递游标都带 `QQ\|` 前缀 |
| chat_history_performance | chat_history_tests --perf | 十万条：写入、keyset 分页 p50/p95、关键词扫描（标签 `perf`，默认不跑） |
| gui_behavior | gui_tests | 图形层 8 个用例（离屏，t60–t64、t66–t68）：模型与控制器、两个 SQLite 客户端真实收发、登录被拒（含账号不存在）、按服务分流（t67）、微博通道与同号码双通道隔离（t68） |
| gui_domain_behavior | gui_domain_tests | 领域页签（离屏，t70–t82）：资料/开通/好友/群、登录失败分类、按服务分区落盘、中文服务名规范化 |
| gui_three_client_behavior | gui_screenshot | 三客户端单聊往返、群聊、截图、停止与重启历史恢复 |

运行：`ctest --test-dir build -E performance`（默认）/ `ctest --test-dir build -L perf`（十万条测量）。
上表末三行只在 `-DFANLX_BUILD_GUI=ON` 时注册。`p2p_behavior` 在命令行构建里就有。2026-10-14 没有重跑整表；当次通过的项目和耗时写在 [hybrid-tcp.md](hybrid-tcp.md) 与 [progress.md](progress.md)。`gui_domain_behavior` 与 `gui_three_client_behavior` 这次没跑。

# S10 集成收敛与要求追踪

2026-09-23。以原始任务书章节/条目为主键核对现行工程文档，避免沿用已移除旧计划的 R01—R33 编号而误配条款。下面 R 表示必做需求，O 表示选做，E 表示本工程优化；编号是本验收文档的局部编号，不替换历史 R12/R21 引用。

## 课程要求到实现和测试

| 编号与任务书位置 | 实现 | 证据/边界 |
| --- | --- | --- |
| R1 一、1 用户资料与列表 | UserProfileFanLX（含明文登录密码）、AccountManagerFanLX（`updateNickname` / `updateLocation` 支持**单项修改**，空串 = 该项不改）、ConsoleAppFanLX | cache_behavior、console_behavior；T龄由申请日期推导，好友/群列表按账号查询；gui_domain_behavior t79 覆盖单项修改（只改昵称 / 只改所在地 / 两项都空被拒）、console_behavior 覆盖控制台单项修改 |
| R2 一、1 共享号码与独立绑定 | ServiceAccountFanLX、IdentityResolverFanLX、**微X 上层**（`registerWeiXAccount` / `activateService` / `syncSharedFlagsFanLX`） | cache_behavior 统一登录、query_cache_behavior 解绑失效；**号码按服务各自唯一**（索引键是「服务名 + 号码」）：QQ 与微博必须同号（两边都标 `sharedId`），只有一边时号码等于微X号才算共享；微信可以用与 QQ 相同的号码（本轮取消了"QQ 号与微信号不能相同"的限制，服务端按"服务 + 号码"分通道，不会串消息）；persistence_behavior 覆盖开通规则（无微X不能开通、重复开通被拒、同服务内号码撞车被拒、微信与 QQ 同号可用） |
| R3 一、2（1）好友增删改查 | FriendServiceFanLX 与应用事务门面 | cache_behavior、persistence_behavior、integration_behavior；双向删除与私有备注；**好友只能在同一服务内建立**（`requireSameServiceFanLX`） |
| R4 一、2（2）共同好友和推荐 | queryCommonFriends/queryCrossService | query_cache_behavior、integration_behavior；仅共享号码或显式绑定映射，不自动添加；跨服务推荐/共同好友读**各自服务**的分区，因此分区后仍然可行 |
| R5 一、3（1）1001—1006 群号 | demoSeedFanLX 分别创建 QQ/微信六群 | smoke 输出 12 群；persistence_behavior 验证同号不同命名空间 |
| R6 一、3（2）入群/退群/踢人/成员 | GroupServiceFanLX、PersistenceApplicationFanLX | group_behavior T06—T09，integration_behavior 验证在线事务与拒绝不变 |
| R7 一、3（3）QQ/微信差异 | GroupPolicyFanLX 策略族 | group_behavior 与 integration_behavior；申请/邀请、休眠 Admin、子群门控 |
| R8 一、4 自主开通 | AccountSessionFanLX（`subscribeTo` / **`activateService`**）、AccountManagerFanLX::activateService | cache_behavior、persistence_behavior、console_behavior；登录不替用户开通；**开通下级服务需要微X会话**（非微X抛「开通服务需要以微X身份登录」）；gui_domain_behavior t77 覆盖注册微X后开通三种服务、重复开通被拒（**同一服务内**号码被占用才拒绝——跨服务同号自本轮起允许；t77 里"微信用已被 QQ 占用的号码"这条用例现在因"该服务已经开通"而拒绝，断言仍然通过但标签已过时） |
| R9 一、5 确认统一登录 | login(微X号, 密码, confirmOthers) / confirmLoginInSession | cache_behavior、query_cache_behavior；**登微X 即授权**：本人全部已开通的下级服务一并登录，下级之间仍是同号/显式绑定；批量/逐项、注销边界；密码错误与账号不存在同样拒绝（gui_domain_behavior t80） |
| R10 一、6（1）启动加载 | JsonFileRepositoryFanLX、SnapshotCodecFanLX | persistence_behavior T11/T12：重启、恢复、锁、非法文件；真实断电/磁盘耗尽未测 |
| R11 一、6（2—4）展示与动态切换 | main --demo、ConsoleAppFanLX | smoke D01—D08；group_behavior T10、integration_behavior 地址/快照/回切 |
| O1 一、6（5）QQ TCP | fanlx_network、QQChatSessionFanLX、菜单 6 | network_behavior T14，两进程 100 条双向中文；仅回环实测 |
| R12 二、2—4 面向对象分层 | 值对象、容器、策略继承、仓储抽象及组合 | docs/architecture.md 类图；领域无 JSON/Win32；群策略可替换 |
| R13 二、5（1）数字菜单 | ConsoleAppFanLX | console_behavior：范围、完整数字、0 返回、EOF 和保存 |
| R14 二、5（2）持久化 | 候选事务、刷新、备份、原子发布 | persistence_behavior：真实进程终止/只读拒绝与故障注入；不将析构写盘当作断电保证 |
| R15 二、5（3—4）扩展/动态模式 | 稳定群外壳 + 策略组合 | group_behavior、persistence_behavior、integration_behavior |
| R16 二、5（5—7）注释/类图/独立文件 | FanLX 类型与独立头文件、中文注释、Mermaid UML | 本轮亦把 ThrowingGroupPolicyFanLX 测试替身拆为独立文件；最终报告排版留给 S11 |
| R17 四 工具与控制台 | CMake/C++17/MinGW、ws2_32 | 无 Visual Studio/MFC/配置期网络下载；verify-release.ps1 干净构建 |
| R18 五 报告格式 | 工程报告持续更新，最终报告冻结为 S11 | 约 5 页正式报告、答辩与发布包尚未验收，不计为已完成 |
| R19 本轮改造·三种服务好友与群分别存储 | FriendDirectorySetFanLX（按服务分区）、FriendServiceFanLX::requireSameServiceFanLX、SnapshotCodecFanLX（schemaVersion = 2 按服务分区落盘） | gui_domain_behavior t73/t81（QQ 好友列表里不会出现微信号、跨服务建关系被拒、`state.json` 里 `friends`/`groups` 是按服务分区的对象且没有扁平 `friendships`）、gui_behavior t67（QQ 连接只看到 QQ 好友与 QQ 群、微信连接好友为空）、persistence_behavior（v2 夹具可读、v1 被拒绝、号码只在"各自服务内"唯一：QQ 与微博必须同号、微信可与 QQ 同号）、demoSeedFanLX 现在给 QQ 与微博各建一条 U1↔U2 好友关系（微信分区为空） |
| R20 本轮改造·登录失败留在登录界面 | DomainControllerFanLX::openWithOutcome（OpenOutcomeFanLX::IdentityRejected / Unavailable）、ChatControllerFanLX::waitForLogin、ImClientFanLX::authenticationRejected | gui_domain_behavior t80（密码错 / 用下级服务号码登录 → IdentityRejected；库不可用 → Unavailable）、gui_behavior t66（真实服务端 + 错误密码 → `waitForLogin` 返回 false 且原因为「用户名或密码错误」、控制器始终不在线；微X（上层身份）在 start 阶段被拒；**账号不存在**也被服务端拒绝；正确凭据仍能登录）；GUI 手工实测见下 |
| R21 本轮改造·聊天界面群/好友标志 | ConversationItemFanLX::kindText / kindColor、ConversationListModelFanLX（DisplayRole/TitleRole 统一输出 `[群] 1001` / `[好友] 2026002`、KindTextRoleFanLX、ForegroundRole）、MainWindowFanLX 标题栏与状态栏 | gui_behavior t65（好友 `#0b6bcb`、群 `#7b1fa2`，打开群会话判定为群）、gui_three_client_behavior 断言"聊天会话列表里群与好友都有区别标志""群紫 / 好友蓝" |
| R22 本轮改造·微博除群以外全部可用 | ServiceTypeFanLX.h 的 `chatCapableFanLX`（QQ / 微信 / 微博 三种下级服务都能单聊）与 `groupCapableFanLX`（只有 QQ 与微信有群）、ImServerFanLX 的通道标识与"微博没有群"落点、chatDirectoryFanLX（客户端聊天目录按服务分层） | gui_behavior t68（微博能上线、`DIRECTORY_LIST` 有好友且 `groups` 为空、微博单聊真实收发、同号码的 QQ 与微博两条通道消息互不串）、t66（微博不再是"被拒绝"的通道）、persistence_behavior（演示种子 2 条好友关系：QQ + 微博）、手工双终端实测（见 [network-validation.md](network-validation.md) 的「微博单聊的复现步骤」） |
| O2 八 课后扩展 | GUI、多聊、历史与数据库 | 不属于本轮实现，也非本次 TCP 选做验收范围 |
| E1 缓存优化 | 单路由 2Q/LRU-K、复合 ARC、平均上限 LFU | cache_behavior、query_cache_behavior，S07 固定种子 CSV |
| E2 内存池 | MemoryPoolFanLX + QueryCacheFanLX | query_cache_behavior：过对齐、失败回收、复用/析构 |
| E3 事务和缓存一致性 | 局部版本键、候选发布、鉴权先于缓存 | integration_behavior 开关缓存相同业务轨迹，查询与重启全字段快照相等 |

**2026-09-30 凭据改造（对本表的修订）**：登录凭据由「服务端令牌表 + 客户端令牌文件」改为
「用户名（QQ 号）+ 用户自选密码」，账号库就是领域快照 `users[].password`（课程范围内明文保存）。
因此上表 R1 的实现列补上密码字段、R9 的 login 增加密码参数；`--im-tokens` / `--token` / `--tokens`
与 `scripts/im-tokens.ps1` 已删除，按本文复现联调时以 [network-validation.md](network-validation.md)
的「启动与操作」为准。三处对外错误文本已固定，测试与文档都按原文引用：
凭据不符为 `用户名或密码错误`（账号不存在与密码错误共用同一句，避免靠提示枚举出系统里有哪些 QQ 号）、
账号已存在但未开通 QQ 为 `该账号未开通 QQ 服务`、
旧快照缺密码为 `用户缺少登录密码（旧数据请重新 --init-demo 初始化）`（迁移＝删掉该数据目录后重新 `--init-demo`）。

**2026-10 微X 上层与按服务分区改造（对本表的修订）**：本轮把"服务"从展示概念提升为存储与连接的分层，
上表四处被改写（R1 支持单项资料修改、R2 微X 上层与 QQ/微博共号、R8 开通下级服务需微X会话、
R9 登微X 联动登录），并新增 R19—R21 三行。要点：微X 是唯一登录身份，注册只建微X、**不捆绑**任何服务账号
（`registerWeiXAccount`）；QQ / 微信 / 微博 由 `AccountSessionFanLX::activateService` 在**微X会话**下逐个开通；
好友按服务分区（`FriendDirectorySetFanLX`），跨服务建关系被 `requireSameServiceFanLX` 拒绝；
快照升到 schemaVersion = 2，旧库明确拒绝；IM 连接按 `(服务, 号码)` 认证。
本轮新增的三处对外错误文本：
`开通服务需要以微X身份登录（微X 是 QQ/微信/微博 的上层账号）`、
`聊天通道只支持 QQ 与微信（微X 是上层登录身份，微博没有聊天）`、
`微X 是上层登录身份，没有聊天通道：请开通并切到 QQ / 微信`。
上一条固定的 `该账号未开通 QQ 服务` 也随之变为按服务取名：`该账号未开通 <服务中文名> 服务`
（例如 `该账号未开通 微信 服务`）。
按本文复现联调时，登录填的是**微X号**（演示数据里微X号 = `2026001` 这类号码，密码统一为 `123`），
QQ / 微信 是登录之后在「开通管理」里开通出来的下级服务。

**2026-10 第二轮·微博成为聊天通道（对上一条的修订）**：微博从"没有聊天通道"变成
"**除群以外全部可用**"（资料 / 开通 / 登录联动 / 好友 / 单聊 / 历史 / 在线状态），
因此上一条里的第二处错误文本不再存在，改为按三个取值报错：
`聊天通道只支持 QQ / 微信 / 微博（微X 是上层登录身份）`（服务端 LOGIN 与 `MessageCodecFanLX` 同一口径）；
第三处在控制台与 GUI 一侧都补上微博：`微X 是上层登录身份，没有聊天通道：请先切到已开通的 QQ / 微信 / 微博`
（控制台）与 `微X 是上层登录身份，没有聊天通道：请在「账号与服务」里开通并切换到 QQ / 微信 / 微博`（GUI；
该分支现在只有微X会到达，因为 QQ / 微信 / 微博 三种服务都有聊天通道）。
`ChatControllerFanLX::switchIdentity` 同时接受界面中文名与存储名（`微信` → `Wechat`、`微博` → `Weibo`），
避免"界面上切了服务、控制器却拿中文名去建连"的错位（由 gui_domain_behavior t82 断言）。
本轮另外两项：`activateService` 取消"QQ 号与微信号不能相同"的限制（号码只要求**在各服务内唯一**，
QQ 与微博仍必须共号）；服务端内部改用**通道标识** `<服务名>|<号码>` 存放与投递、出站帧剥回纯号码，
客户端聊天目录由 `<账号目录>` 改为 `<账号目录>/<服务名>`（`chatDirectoryFanLX`），
因此两条同号码的通道各有自己的聊天库与目录锁。条款表相应地补 R22 一行，R2 / R19 两处补上号码唯一性口径。

## 新增应用事务权限矩阵

每项拒绝同时检查在线 JSON（含 generation/versions）、state.json 文件字节和群/子群地址不变，并再次查询预热缓存。执行两遍，分别关闭/开启缓存。

| 操作 | 允许路径 | 拒绝路径 |
| --- | --- | --- |
| 任免管理员、转让、模式切换、解散 | Owner；转让后由新 Owner 解散 | Admin/Member/非成员 |
| 踢人 | QQ Admin 踢普通成员，父群/子群同步 | Member 踢任意角色；Admin 踢 Owner/自己/同级；Owner 踢自己；微信休眠 Admin |
| 退群 | 转让后的原 Owner、普通成员 | 未转让 Owner |
| 申请审批 | QQ Admin 通过申请 | 普通成员审批、微信模式申请 |
| 子群 | QQ Admin 扩员 | Member 新建、微信 Owner 新建/扩员 |
| 查询 | 群成员/当前账号 | 非成员、退群后、注销后 |
| 平台/身份 | 账号所属命名空间内操作 | 微信会话操作 QQ 命名空间、跨平台目标、绑定别人账号 |
| 提交 | 成功后查询反映新成员 | BeforePublish 注入异常后文件/内存/版本不变 |

另覆盖 QQ→微信→QQ 后角色/成员保留、成功好友增删和中文备注、解绑/重绑推荐、解散后同键重建不命中旧缓存、重启无会话/缓存且全字段相等。原 T06—T13 与故障测试全部保留。

## GUI 版本对同一批条款的覆盖（新增）

上表描述的是领域层与控制台；可视化客户端此前只做了聊天页签，因此 R1—R9 与 R11 在界面上"看不见"。
本轮把同一批领域能力接到 Qt 界面上：主窗口为 `QTabWidget`，`聊天 / 账号与服务 / 好友 / 群` 四个页签
同时存在，`DomainControllerFanLX` 是这三个领域页签唯一的门面（对应控制台的账号/好友/群三组菜单）。

| 编号与任务书位置 | GUI 实现 | GUI 证据 |
| --- | --- | --- |
| R1 一、1 用户资料与列表 | AccountPageFanLX「用户基本信息」：微X号/昵称/出生时间/号码申请时间/T龄/所在地/号码归属/好友与群列表；资料维护拆成「保存昵称」「保存所在地」**两个独立按钮**（单项修改） | gui_domain_behavior t71；页面文案断言含"T龄""号码ID"；t79 覆盖单项修改；t78 断言两个独立按钮存在 |
| R2 一、1 共享号码与独立绑定 | AccountPageFanLX「服务开通与登录状态」表新增**「微X（上层）」行**与"（未开通）"，标明"QQ 与微博共享号 / 独立号"与已绑定；「绑定管理」可绑定/解绑 | t71、t72、t77；页面文案断言含"微X""QQ 与微博共享号""wx_2026001" |
| R3 一、2（1）好友增删改查 | FriendPageFanLX：列表、添加、删除、保存备注/标签、填入选中好友 | t73；页面文案断言含四个按钮 |
| R4 一、2（2）共同好友和推荐 | FriendPageFanLX 查询区：同平台共同好友、跨平台推荐好友、跨平台共同好友 | t73（推荐结果 `wx_2026002` 用于在微信侧添加）；断言推荐不会自动建立关系 |
| R5 一、3（1）1001—1006 群号 | GroupPageFanLX 群列表按当前平台列出本人参与的群（QQ / 微信各自命名空间） | t70 断言六个群号全部就位；t81 断言 QQ 与微信各有自己的群列表 |
| R6 一、3（2）入群/退群/踢人/成员 | GroupPageFanLX 操作区：申请/批准/拒绝/推荐/确认/拒绝/退群/挨 T/任命/撤销/转让/解散/建群/改名 | t74（含批准演示数据里的 U4 申请、踢人、转让后原群主退群） |
| R7 一、3（3）QQ/微信差异 | 同一页面上按策略给出可点/被拒的操作，拒绝原因是领域原话 | t74：微信群申请被拒、微信群不支持管理员与子群 |
| R8 一、4 自主开通 | AccountPageFanLX「开通管理」：服务下拉 + **号码输入框**（默认建议值 QQ/微博 = 微X号，微信 = `wx_微X号`）+「开通所选服务」 | t72、t77（注册微X后 QQ/微信/微博 逐个开通、重复开通被拒）、t78（断言「开通所选服务」按钮） |
| R9 一、5 确认统一登录 | 登录窗口用户名改为**「微X号」**并带「注册微X账号…」按钮；「账号与服务」页「逐项确认登录」/「一键联动登录本人其它服务」 | t72、t77（注册微X → 登录 → 联动）；`--no-confirm-others` 可关闭联动 |
| R10 一、6（1）启动加载 | `DomainControllerFanLX::openWithOutcome` 读 `<账号目录>/domain/state.json`；空库写入演示数据，损坏库只报告不覆盖；返回 `OpenOutcomeFanLX{Ok, IdentityRejected, Unavailable}` 供调用方区分"身份被拒"与"库不可用" | t70、t76（第二次打开为"已从文件加载"）、t80（两类失败的分类） |
| R11 一、6（4）动态切换 | GroupPageFanLX「切换为 QQ / 微信群管理特色」+ 特色面板 | t75：切换后成员逐行一致、子群记录保留、能力随策略变化；`gui_three_client_behavior` 也断言成员数不变 |
| R13 二、5（1）简便菜单 | 四个页签 + 顶部菜单（工具 / 领域数据 / 帮助），不再需要逐层输入数字 | t78：断言四个页签、三个页面实例与各分区按钮文案 |
| R16 二、5（5—7）注释/类图/独立文件 | 每个 GUI 类各自一对 `.h/.cpp`，全部以 `FanLX` 结尾，中文注释说明取舍 | 见 docs/project-structure.md 的 GUI 类图 |
| R19 本轮改造·三种服务好友与群分别存储 | 服务下拉只列三种下级服务（微X 是上层身份，不可"开通"）；`setCurrentService` 同时接受中文名与存储名；账号页提示"微X 是上层登录身份，注册时只创建它" | t73（QQ 身份加不了微信号、微信侧好友独立计数）、t81（快照分区与每条关系同服务） |
| R20 本轮改造·登录失败留在登录界面 | `main_gui.cpp` 先做领域登录（`IdentityRejected` 直接返回失败），再按"当前服务"建聊天连接并 `waitForLogin(6000ms)`；连上但认证失败同样回退，不打开主窗口；`LoginDialogFanLX::showFailure` 保持窗口打开并保留输入 | t80（领域侧分类）、t66（聊天侧 `waitForLogin` 返回 false 且控制器不在线）、t78（账号页不再有注册按钮，注册入口在登录界面） |
| R21 本轮改造·聊天界面群/好友标志 | 聊天页顶部"当前聊天服务 + 标志说明"标签；会话列表 `[群] 1001`（紫）/ `[好友] 2026002`（蓝）；标题栏与状态栏也带类型标志 | t65（kindText/kindColor、`currentConversationIsGroup`）、`gui_three_client_behavior`（"群与好友都有区别标志""群紫 / 好友蓝""打开群会话判定为群"） |
| R22 本轮改造·微博除群以外全部可用 | 服务下拉里的"微博"与 QQ / 微信 并列，可作为"当前服务"；聊天页对微博照常建连接、显示好友会话并能单聊；群页签对微博给出"群功能只支持 QQ 与微信（微X 是上层身份、微博没有群）"，不假装可用 | t68（微博通道：上线、好友目录、空群列表、真实收发、与同号码 QQ 通道互不串）、t82（界面中文名"微博"被规范化为存储名 `Weibo` 后才派生目录与建连，`dataDirectory()` 含 `Wechat` / 微X 被拒）、t67（三个服务的聊天库目录互不相同） |

**GUI 与中心转发 IM 的边界**：聊天仍走 `ImClientFanLX`（TCP 长度帧 + 中心转发），领域页签走本机
`PersistenceApplicationFanLX`，两者各自独占自己那层的 `.lock`，因此可以同时打开。IM 协议不负责同步
客户端领域菜单的改动——这一点在凭据改造之前就已经成立，GUI 只是把"客户端领域菜单"从控制台搬到了界面。

**2026-10 的界面顺序与身份切换**：登录顺序变为「先领域登录（微X号）→ 再按"当前服务"建聊天连接 →
`waitForLogin(6000ms)` 等一次服务器认证结果」，任一步失败都留在登录界面（见 R20）。
`MainWindowFanLX` 连接 `DomainControllerFanLX::identityChangedFanLX` → `onIdentityChangedFanLX()`：
当前服务是 **QQ / 微信 / 微博** 时用该服务的号码重建聊天连接（三种下级服务都能单聊），
是微X 时断开并提示「微X 是上层登录身份，没有聊天通道：请在「账号与服务」里开通并切换到 QQ / 微信」，
此时发送会被拦下并保留输入；微博只是**没有群**（群页签给出"群功能只支持 QQ 与微信"），不影响聊天页。
聊天库与领域库在磁盘上也是分开的，而且聊天库现在按服务再分一层：同一个账号目录下
`<服务名>/chat.db`（`chatDirectoryFanLX` 派生的聊天库）与 `domain/state.json`（领域快照）互不覆盖；
QQ 与微博共用同一个号码时分别是 `QQ/` 与 `Weibo/`，各有自己的库与目录锁。

**单机演示的取舍**：好友域要求对端"已开通且已确认登录"才能建立关系（现实里就是"对方在线才能加好友"）。
单机演示没有第二个进程替对方登录，因此 `DomainControllerFanLX::addFriend` 会按演示口令规则
（`demoPasswordFanLX`）先把对端临时置为在线；口令推不出来的账号保持离线，领域照原样报
`对端服务尚未确认登录`。该登录态是**内存态**，不写回文件。

## 验证和剩余边界

使用 scripts/verify-release.ps1 在唯一目录中配置、构建并执行 CTest，保存配置/编译/测试日志及 SHA-256 清单。新网络/集成目标启用 -Wall -Wextra -Wpedantic -Werror；测试在 Release 仍执行断言。各次实测路径、配置和结果写入 progress.md 完成日志。

启用 `FANLX_BUILD_GUI=ON` 后 CTest 共 14 项（性能项默认不跑）；`gui_domain_behavior` 与
`gui_three_client_behavior` 都在 `QT_QPA_PLATFORM=offscreen` 下运行，不需要真实窗口。

### 2026-10 实测（含第二轮：微博成为聊天通道）

**自动测试**：`ctest --test-dir build -E performance` → **13/13 passed**（注册的 14 项里
`chat_history_performance` 是性能项，默认不跑）。相关回归用例：

- `gui_domain_behavior`：t79 单项资料修改；t77 注册微X + 开通三种服务（含重复开通被拒；
  其中"微信用已被 QQ 占用的号码"这条用例现在因"该服务已经开通"而拒绝，标签仍是上一轮的"号码撞车"口径）；
  t80 登录失败分类（密码错 / 用下级服务号码登录 → `IdentityRejected`，库不可用 → `Unavailable`）；
  t81 分别存储（`state.json` 里 `friends`/`groups` 为按服务分区的对象、无扁平 `friendships`、
  `schemaVersion = 2`、每条关系同服务）；t73 增加"QQ 好友列表里不会出现微信号"等断言；
  t78 断言账号页不再有注册按钮、有「保存昵称/保存所在地」。
- `gui_behavior`：t65 会话列表群/好友标志与颜色（好友 `#0b6bcb`、群 `#7b1fa2`）；
  t66 登录被拒（真实服务端 + 错误密码 → `waitForLogin` 返回 false 且原因为「用户名或密码错误」，
  控制器始终不在线；微X（上层身份）在配置阶段被拒；**账号不存在**同样被服务端拒绝；正确凭据仍能登录）；
  t67 按服务分流（QQ 连接只看到 QQ 好友与 QQ 群、微信连接好友为空、QQ 好友关系不能用于微信单聊、
  两个客户端聊天库目录不同）；
  **t68（本轮新增）微博通道**（微博能上线、目录里有好友且群列表为空、微博单聊真实收发且服务器已接收、
  以及"QQ 与微博共用同一个号码时两条通道的消息互不串"：同号码的 QQ 客户端看不到微博消息，
  两者 `dataDirectory()` 不同且微博目录含 `Weibo`）。
- `persistence_behavior`：schemaVersion = 2 夹具、v1 被拒绝、微X 注册与开通的权限规则；
  演示种子现在有 **2 条好友关系**（QQ 分区 U1↔U2、微博分区 U1↔U2）、跨服务加好友被拒、
  QQ 与微博必须同号、微信可以用与 QQ 相同的号码（本轮取消的限制）。
- `chat_history_behavior`：t53 服务端旧 JSON 迁移后的账号 / 会话键 / 投递游标都带 `QQ|` 前缀
  （断言已按新键更新）。
- `console_behavior`：登录口注册、微X 登录联动、单项资料修改、开通 QQ 不捆绑微博。
- `gui_three_client_behavior`（`tools/gui_screenshot.cpp`）：新增"会话列表里群与好友都有区别标志"
  "群紫/好友蓝""打开群会话判定为群"等断言，并重新生成 6 张截图。

**手工验收（本轮）**：一台真实服务端 + 两个控制台客户端（数据根 `data-im-a` / `data-im-b`，
端口 19530）：客户端 A 用微X号 `2026001` 登录 → 「切换当前服务」到微博 → 聊天菜单显示"（本服务没有群）"
→ 给 `2026002` 发 `hello-weibo`，状态行 `在线 待确认=0`，历史里出现
`单聊 2026002 #1 2026001: hello-weibo`；客户端 B 用 `2026002` 登录并切到微博后，历史里收到同一条消息。
数据目录重建也做了实测：`scripts\run-im-server.bat` 遇到 schemaVersion=1 的 `data-im-server` 时
自动把它改名为 `data-im-server.legacy-20260929-233359`（保留证据）并重建，随后正常监听
（`Test-NetConnection` 为 True）；`data-im-a` / `data-im-b` 同样重建为 schemaVersion=2。

**手工验收（上一轮记录，登录顺序与失败回退本轮未改）**：`fanlx_gui --auto-login --password wrong-pw`
直接以非零码退出并打印「自动登录失败：微X登录失败：用户名或密码错误」，**没有打开主窗口**；
换成正确密码则打开主窗口。当时观察到的是账号目录下的 `2026001/chat.db`（聊天库）与
`2026001/domain/state.json`（领域库）分开存在；本轮聊天库改为按服务分层，同一位置现在是
`2026001/QQ/chat.db`，切到微博时另建 `2026001/Weibo/chat.db`。
未实测：真实两机局域网、物理断电、128 连接压测。

S10 范围是当前 Windows/MinGW 实现的集成正确性与课程条款追踪，不等同跨平台、生产级网络安全或硬件断电保证。局域网真实两机、极限资源压力、最终正式报告与交付冻结仍须单独验收。

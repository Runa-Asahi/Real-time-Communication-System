# S06 快照与恢复协议

本文件对应 schemaVersion=2，代码源于 SnapshotCodecFanLX / SchemaValidatorFanLX / JsonFileRepositoryFanLX。完整合法样例见 tests/fixtures/persistence/valid-state-v2.json（由真实程序提交生成），其中 users[] 含登录密码字段，取自演示数据的密码规则（2026-10-10 起统一为 `123`；见 DemoSeedFanLX 的 `demoPasswordFanLX`）。

**2026-10 格式升级（schemaVersion 1 → 2）**：好友与群不再是一个扁平数组，改为**按服务分区**的对象
（"friends": {QQ/Wechat/Weibo}，"groups"/"archivedGroups": {QQ/Wechat}），因为 QQ / 微信 / 微博
三种服务的好友与群互不共享、分别存储。schemaVersion=1 的旧文件会被明确拒绝（错误信息见下），
迁移方式仍是"备份旧目录后重新 `--init-demo`"，程序不做就地改写。旧版样例保留在
tests/fixtures/persistence/valid-state-v1.json，用于断言"旧 schema 必须被拒绝"。

## 文件布局

- state.json：唯一当前提交快照。
- state.json.prev：上一有效快照；主发布前失败时可能与主文件同代。
- .tmp.<pid>.<tick>.<sequence>、.backup.tmp.*：写入/备份中间文件，永远不因代数较大就当作成功提交。
- .lock：Windows 独占句柄载体；残留文件不是活跃锁，不能靠删除它抢占。
- state.damaged.<nonce>：恢复确认前保留的损坏主文件证据。
- recovery.<nonce>.json：显式恢复的来源代数、原代数（可信可读时，否则 null）、时间、原因记录。

目录路径用 std::filesystem::path 和 Win32 宽字符 API，程序/子进程入口用 wmain。所有 JSON 以 UTF-8 存储。

## 字段与稳定身份

顶层必有 schemaVersion、generation、parentGeneration、committedAtUtc、versions、users、accounts、subscriptions、bindings、friends、groups、archivedGroups。users/accounts/subscriptions/bindings 与每个服务分区内的集合都显式为数组；friends 与 groups/archivedGroups 本身是**按服务命名的对象**（空分区也必须出现）。未知版本拒绝进入可写状态。

- generation/parentGeneration 为 uint64；文件第一代为 1、父代为 0；后续父代为本代减一。内存初始 DTO 允许 0/0，但不能把 0 代文件当成已提交状态。
- users：userId、nickname、birthDate、applyDate、location、password。日期 YYYY-MM-DD，验证闰年/月份/先后关系。password 为登录密码，**明文**保存（课程范围内的取舍），非空且不超过 4096 字节；缺字段按空串读入后由校验器拒绝，错误信息为「用户缺少登录密码（旧数据请重新 --init-demo 初始化）」。一个用户一个密码，该用户各服务共用。
- accounts：userId、service（WeiX/QQ/Wechat/Weibo）、accountId、sharedId。服务内账号 ID 唯一，一个用户每服务一号。**WeiX 是上层登录身份**（注册时创建、注册即开通，不参与"共享号码"语义，sharedId 恒为 false）；QQ / 微信 / 微博 是登录之后在内部开通的下级服务。同人的 QQ/微博若同时存在，号码必须相同且 sharedId=true；微信 sharedId=false。
- subscriptions：{service,accountId}；只保留开通，登录态不写盘。微X 注册即开通，下级服务由 `activateService` 建号并开通。
- bindings：left/right 结构化服务账号引用。保存底层有向关系；同一个 left 不重复，端点必须同用户且不同服务；读侧统一按双向绑定语义使用。
- friends：**按服务分区的对象**，键为服务名（QQ/Wechat/Weibo），每个键下是该服务自己的好友数组：left/right 结构化账号引用与 leftRemark/leftTag/rightRemark/rightTag，每个无序同服务账号对一条。解码时校验"关系两端与所在分区同名"，不一致直接拒绝加载；恢复时写进对应服务的好友目录（FriendDirectorySetFanLX::directory）。
- groups / archivedGroups：同样是**按群命名空间分区的对象**，键为 QQ/Wechat。每个群记录含 key{namespaceMode,id}、currentMode、name、nextJoinOrder、members、applications、invitations、discussions，key 永久标识创建命名空间，currentMode 可以不同；解码时校验群键命名空间与分区一致。
- members：{userId,role,joinOrder,joinedAtUtc}；role=Owner/Admin/Member。每活动群恰好一个 Owner。joinOrder 唯一且小于 nextJoinOrder，退出造成的空洞保留。joinedAtUtc 为 UTC 秒级时间或 null（未知历史）；新事务内加入使用可注入时钟。
- applications：applicant/status/createdAtUtc/expiresAtUtc。invitations 另外含 inviter/invitee。状态 Pending/Approved/Rejected/Cancelled/Expired，暂无完整尝试日志；每个申请人/受邀人保留最近记录，再次申请会替换上一已结束记录。模式不匹配时 Pending 自然暂停，重新确认时校验邀请人资格；过期只在候选写事务关闭。
- discussions：id、members、owner、archived。父群由嵌套位置确定。空子群标记归档，保留对象；QQ 模式下有权限者可重新扩员激活，保持 S05 已验收的回切兼容行为。微信模式禁止新建/扩员，但退出清理继续执行。
- archivedGroups：解散时的完整历史片段，子群归档；可包含同稳定键的不同时期记录。同号群重建不继承其活动工作流。

所有业务 ID 是字符串，禁止 |、控制字符，长度 1—128 UTF-8 字节。其它文字字段最长 4096 字节。文件最多 64 MiB、解析嵌套深度最多 32；拒绝重复 JSON 属性、非法 UTF-8、负/浮点整数、未知角色/模式、重复或缺失引用。

集合限制：users 10000、accounts/subscriptions/bindings 各 30000、好友关系总量 100000（按分区累加）、groups/archivedGroups 各 1000（按分区累加）；每群 members/applications/invitations/discussions 以及每子群 members 最多 10000，群域累计成员/工作流/子群记录不超过 1000000。文件总字节上限先于 DOM 分配检查，深度与重复属性在解析回调检查。

versions 是局部键到最后修改 generation 的映射：users/accounts/subscriptions/bindings；friends/<结构化账号 JSON>、remarks/<账号 JSON>；group/<群键 JSON>/members、roles、currentMode、applications、invitations、discussions、name、deleted。无关群写入不提升另一个群的版本。备注/角色变化允许保守失效；S07 已将这些局部版本用于业务查询缓存键；写失败不发布版本，恢复/重启清空缓存。分区格式下版本键不变——账号引用本身已含服务名，因此不同服务的好友版本天然互不影响。

## 写事务

PersistenceApplicationFanLX 是在线写门面。外部只取得 const 状态，后台注册/首次导入有专门入口。账号配给、注册微X账号（`registerWeiXUser`）、开通下级服务（`activateService`）、开通、绑定/解绑、资料、改密、好友、群工作流、角色/切换、子群及解散均使用 TransactionCoordinatorFanLX。后台批量入口 `registerUser(user, accounts)` 保留给测试与演示导入：它按调用方给定的账号列表建号，首项为初始开通服务；**界面的注册入口不走它**，只用 `registerWeiXUser`（只建微X、不捆绑任何下级服务）。`activateService` 的事务内前置是"调用方持有本人的微X会话"，因此"只有登录微X后才能开通三种服务"由应用与领域两层各自把关。

登录与写事务刻意分开：`login(actor, password, confirmOthers)` 用「微X号 + 密码」校验凭据、失败抛 CredentialRejectedFanLX，成功的产物是**内存登录态**（盘上只有 subscriptions 表示"已开通"，登录态从不写盘），因此它不经过事务协调器、也不提升代数。confirmOthers=true 且登录的是微X账号时，本人已开通的下级服务一并置为登录态（微X 即授权来源）。`changePassword(actor, oldPassword, newPassword)` 改的是 users 里的持久字段，必须走事务提交，提交后 users 局部版本提升、相关缓存按版本失效；它不要求处于登录态，凭据是旧密码本身。资料维护是单项的：`updateNickname` / `updateLocation` 以空串表示"这一项不改"，因此改昵称不会顺手覆盖所在地。

协调器复制纯数据候选，保留同进程会话；执行候选规则、时间戳、局部版本和全量校验，准备所有对象与返回结果。无变化事务不增加代数。

PreparedCommitFanLX 对存续群/子群保存旧外壳并定位交换目标；只交换内部成员/策略等状态，管理器本身及群对象地址不变。它使用预定位指针和经 static_assert 检查的容器 swap；发布阶段不查哈希、不分配、不运行用户业务回调。应用为单线程事件循环模型，事务内有写锁；const 视图不得从其他线程并发读取正在发布的状态。未来 TCP 只投递事件。

文件写入同目录唯一临时文件，短写循环，FlushFileBuffers 并关闭。旧主文件复制成备份临时文件并刷新，再发布为 .prev；此时原主文件仍存在。最后 MoveFileExW 同卷发布新主文件为提交点。首次创建不覆盖意外文件，已有目标采用覆盖与 WRITE_THROUGH。遇到短暂共享/访问占用最多重试 190 ms；真正只读属性立即拒绝。

发布成功后执行内存 noexcept 发布，再应答。提交点前失败保留在线状态/版本；提交点后应答前崩溃可能已经提交。异常时核验实际主文件内容，若等于候选则完成内存发布；无法确定时阻断写入并报告 CommitIndeterminate，不能声称已回滚。

## 加载与恢复

Empty/UncommittedOnly 需要显式 initialize；Loaded 可读写；RecoveredReadOnly 只加载合法备份，确认前禁止写；Failed/UnsupportedSchema 不进入可写路径。主文件优先，不扫描最大 tmp 代数。主备代数矛盾阻断。未知版本主文件不自动回退，但调用方可显式确认选择兼容备份。两类历史快照都归入 Failed/UnsupportedSchema 并给出可照做的提示，且不会用空状态覆盖：凭据改造之前的文件因 users[] 缺 password 报「用户缺少登录密码（旧数据请重新 --init-demo 初始化）」；本轮分区改造之前的文件（schemaVersion=1）报「未知 schemaVersion：本版本读写 schemaVersion=2（好友与群按服务分区；旧库请备份后重新初始化）」。两者都不做就地补字段。

recover 先核验备份，保存并验证损坏主文件证据，记录来源，再发布备份到主文件；不覆盖原有效 .prev。成功后重建内存并清会话，提交前失败保持只读并可重试。若恢复发布后重新校验/内存重建失败，应用关闭仓储，必须重启重新加载，禁止旧内存继续写入。恢复后从所选备份代数继续，损坏分支证据单独保存。

saveAndClose 检查未提交变化，失败抛出明确错误、不报告成功。析构只负责资源关闭；不把析构写文件等同断电保障。支持范围是本机实测 Windows/NTFS 进程崩溃恢复；没有实测硬件断电或真实磁盘写满，不能承诺任意硬件零丢失。


# S10 集成收敛与要求追踪

2026-09-23。以原始任务书章节/条目为主键核对现行工程文档，避免沿用已移除旧计划的 R01—R33 编号而误配条款。下面 R 表示必做需求，O 表示选做，E 表示本工程优化；编号是本验收文档的局部编号，不替换历史 R12/R21 引用。

## 课程要求到实现和测试

| 编号与任务书位置 | 实现 | 证据/边界 |
| --- | --- | --- |
| R1 一、1 用户资料与列表 | UserProfileFanLX、AccountManagerFanLX、ConsoleAppFanLX | cache_behavior、console_behavior；T龄由申请日期推导，好友/群列表按账号查询 |
| R2 一、1 共享号码与独立绑定 | ServiceAccountFanLX、IdentityResolverFanLX | cache_behavior 统一登录、query_cache_behavior 解绑失效 |
| R3 一、2（1）好友增删改查 | FriendServiceFanLX 与应用事务门面 | cache_behavior、persistence_behavior、integration_behavior；双向删除与私有备注 |
| R4 一、2（2）共同好友和推荐 | queryCommonFriends/queryCrossService | query_cache_behavior、integration_behavior；仅共享号码或显式绑定映射，不自动添加 |
| R5 一、3（1）1001—1006 群号 | demoSeedFanLX 分别创建 QQ/微信六群 | smoke 输出 12 群；persistence_behavior 验证同号不同命名空间 |
| R6 一、3（2）入群/退群/踢人/成员 | GroupServiceFanLX、PersistenceApplicationFanLX | group_behavior T06—T09，integration_behavior 验证在线事务与拒绝不变 |
| R7 一、3（3）QQ/微信差异 | GroupPolicyFanLX 策略族 | group_behavior 与 integration_behavior；申请/邀请、休眠 Admin、子群门控 |
| R8 一、4 自主开通 | AccountSessionFanLX、subscribe | cache_behavior、persistence_behavior、console_behavior；登录不替用户开通 |
| R9 一、5 确认统一登录 | login/confirmLogin | cache_behavior、query_cache_behavior；同号/绑定、批量/逐项、注销边界 |
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
| O2 八 课后扩展 | GUI、多聊、历史与数据库 | 不属于本轮实现，也非本次 TCP 选做验收范围 |
| E1 缓存优化 | 单路由 2Q/LRU-K、复合 ARC、平均上限 LFU | cache_behavior、query_cache_behavior，S07 固定种子 CSV |
| E2 内存池 | MemoryPoolFanLX + QueryCacheFanLX | query_cache_behavior：过对齐、失败回收、复用/析构 |
| E3 事务和缓存一致性 | 局部版本键、候选发布、鉴权先于缓存 | integration_behavior 开关缓存相同业务轨迹，查询与重启全字段快照相等 |

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

## 验证和剩余边界

使用 scripts/verify-release.ps1 在唯一目录中配置、构建并执行 CTest，保存配置/编译/测试日志及 SHA-256 清单。新网络/集成目标启用 -Wall -Wextra -Wpedantic -Werror；测试在 Release 仍执行断言。各次实测路径、配置和结果写入 progress.md 完成日志。

S10 范围是当前 Windows/MinGW 实现的集成正确性与课程条款追踪，不等同跨平台、生产级网络安全或硬件断电保证。局域网真实两机、极限资源压力、最终正式报告与交付冻结仍须单独验收。

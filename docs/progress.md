# 开发进度 — 樊陆旭模拟即时通讯系统

## 项目概况

| 项目       | 内容                                              |
| ---------- | ------------------------------------------------- |
| 项目名称   | 樊陆旭模拟即时通讯系统                            |
| 学期       | 2026-2027 学年第一学期                            |
| 班级       | 552502 班                                         |
| 工程文档   | `deliverables/樊陆旭_面向对象课设工程文档.docx`（按课程设计文档要求撰写） |
| 工具链     | CMake 3.20+ / MinGW（`D:/mingw64`）/ C++17        |
| 当前构建   | 2026-10-14 见文末：直连与对象池相关测试已通过；整表 CTest 与 gui_domain 这次没跑 |

---

## 文件与模块现状

| 目录 / 文件                      | 对应阶段  | 状态                     | 包含的主要类型                                                                            |
| ------------------------------- | --------- | ------------------------ | ----------------------------------------------------------------------------------------- |
| `include/account/`（10 文件）    | S02       | ✅ 完整实现               | `ServiceTypeFanLX`、`UserProfileFanLX`、`ServiceAccountFanLX`、`AccountManagerFanLX`、`AccountSessionFanLX`、绑定/快照与三类业务异常 |
| `include/friend/`（5 文件）      | S03       | ✅ 完整实现               | `FriendshipFanLX`、`FriendDirectoryFanLX`、`IdentityResolverFanLX`、`FriendServiceFanLX`、`FriendSnapshotFanLX` |
| `include/group/`（17 文件）      | S01/S04/S05 | ✅ 完整实现（含无损切换） | `GroupPolicyFanLX` 策略族、`GroupKeyFanLX`、`GroupFanLX`、`GroupMembershipFanLX`、`DiscussionGroupFanLX`、`GroupServiceFanLX` |
| `include/infra/`（9 文件）       | S01/S07   | ✅ 完整实现               | 2Q/LRU-K、ARC/B1/B2、LFU 衰减、MemoryPool、QueryCache、查询统计 |
| `include/persistence/`（15 文件）| S06       | ✅ 完整实现               | 仓储接口、JSON 编解码、快照校验、原子写、目录锁、事务协调、恢复与错误分类 |
| `include/*DomainFanLX.h`（3 个） | —         | ✅ 聚合入口               | 三个域全部类型的兼容 include 入口；每个类仍各自独立成文件 |
| `src/main.cpp`                  | S06/S09   | ✅ 启动、演示与菜单       | `--demo`、初始化/恢复/保存关闭、默认数字菜单和确定性 D01–D08 演示 |
| `src/persistence/`（9 文件）     | S06       | ✅ 完整实现               | 上述持久化头文件对应的实现 |
| `tests/cache_tests.cpp`         | S07       | ✅ 断言测试全通过         | ①LRU ②ARC ③好友域 ④LruList 组件 ⑤ARC 查询接口 ⑥统一登录与会话 |
| `tests/group_tests.cpp`         | S04/S05   | ✅ 断言测试全通过         | T06–T09 权限/工作流/子群；T10a–f 无损切换 |
| `tests/persistence_tests.cpp`   | S06       | ✅ 断言测试全通过         | T11/T12、子进程重启与故障注入 |
| `tests/query_cache_tests.cpp`   | S07       | ✅ 断言测试全通过         | T13 缓存、LFU、ARC、内存池、权限和版本失效 |
| `tests/console_tests.cpp`       | S09       | ✅ 断言测试全通过         | 菜单输入、返回、EOF、保存和越权场景 |
| `include/network/`、`src/network/` | S08    | ✅ 已实现                 | Winsock/Socket RAII、长度帧、TCP、有界队列与接收线程 |
| `include/ui/QQChatSessionFanLX.h` | S08    | ✅ 已接数字菜单           | 主线程聊天鉴权、账号与连续序号检查 |
| `tests/network_tests.cpp`、`tests/integration_tests.cpp` | S08/S10 | ✅ 断言测试全通过 | T14 双进程中文、协议/资源边界；跨域权限/事务/缓存对照 |

---

目录约定：源码按四层架构分目录（`account/`、`friend/`、`group/`、`infra/`、`persistence/`），`#include` 路径一律以 `include/` 为根（如 `#include "group/GroupFanLX.h"`）；`include/*DomainFanLX.h` 只作聚合入口。持久化验证入口为 `tests/persistence_tests.cpp`。

## 阶段完成情况

| 阶段 | 名称                       | 状态                                                                 |
| ---- | -------------------------- | -------------------------------------------------------------------- |
| S00  | 需求基线与开发环境         | ✅ 完成                                                               |
| S01  | 类职责、抽象与骨架         | ✅ 完成（策略接口、内存池、可编译骨架）                               |
| S02  | 资料、账号、开通与统一登录 | ✅ 完成                                                               |
| S03  | 好友 CRUD、交集与跨服务推荐 | ✅ 完成                                                              |
| S04  | 群基本操作与差异化策略     | ✅ 完成                                                                              |
| S05  | 动态切换并证明数据无损     | ✅ 完成                                                                |
| S06  | 持久化事务、启动与恢复     | ✅ 代码已完成；T11/T12 与 S05 存储延迟项通过（见 S06 实施记录） |
| S07  | 索引、LRU 缓存与性能基准   | ✅ 查询缓存、内存池、T13 与固定种子基准已完成                         |
| S08  | QQ 双端 TCP 通信           | ✅ 协议、传输、菜单与 T14 双进程测试通过                           |
| S09  | 数字菜单与确定性功能展示   | ✅ 数字菜单、返回/EOF/严格数字解析、D01–D08 已接入                    |
| S10  | 集成测试与缺陷收敛         | ✅ 权限矩阵、缓存对照、故障回归与条款追踪完成                                                             |
| S11  | 正式报告、答辩与发布冻结   | ❌ 未开始                                                             |

> **当前主线**：S06、S07、S09 已完成可验收实现；S08 TCP 与 S10 集成收敛已实现；下一步为 S11 正式报告及发布冻结。S05 重启保持模式及写失败不变已在 T11/T12 补验。

---

## 详细记录

### 2026-09-08

#### S00/S01 — 环境与骨架

- 建立 CMake 工程（MinGW Makefiles，C++17，`/W4 /utf-8`）
- 实现 `MemoryPoolFanLX`：批量分配 + 空闲链表复用；`alignas(T)` 保证对齐；析构时沿块链表统一回收
- 建立群策略多态骨架：`GroupPolicyFanLX` 抽象基类（纯虚 `canCreateDiscussion / name`）、`QQGroupPolicyFanLX`（允许群讨论）、`WechatGroupPolicyFanLX`（不允许）
- ARC 缓存初版上线，控制台入口（`--demo` 占位）

#### S02 — 账户域

- **设计决策**：统一存储键 `"userId|服务名"`；违反业务规则直接抛异常，不静默返回错误状态
- `UserProfileFanLX`（userId / nickname / birthDate / applyDate / location）
- `ServiceAccountFanLX`（userId + service + accountId + `sharedId` 共享标志）
- `AccountManagerFanLX`：用户注册（去重）、服务账号添加、服务订阅、`bindWechatToQQ`、统一登录（`confirmOthers=true` 一并登录其他已开通平台）
- 种子数据验证：QQ/微博共享账号 ID、微信独立账号、重复/非法绑定拒绝、未开通服务不被自动登录

#### S03 — 好友域

- **设计决策**：细节表键用 `"owner\npeer"`（换行符隔离方向），备注/标签单向私有；账号键格式 `"userId|服务名|accountId"` 三段
- `FriendDirectoryFanLX`：无向图邻接 + 细节表，add/remove 幂等
- `IdentityResolverFanLX`：`userId+service → accountKey` 双向转换
- `FriendServiceFanLX`（门面）：前置校验"已开通且已登录"，委托目录层执行
- 功能：`addFriend`（幂等）、`removeFriend`（双向一致）、`updateRemark`（单向）、`findFriends`（字典序）
- 跨服务：`commonFriends`（键集合交集）、`crossServiceCommonFriends`（userId 级去重交集）、`recommendFriends`（source 平台好友 → target 平台过滤）
- 测试覆盖：重复添加幂等、备注私有性、共同好友交集、推荐去重/已存在过滤、双向删除一致性

**验证（2026-09-08）**：`cmake -S . -B build`、`cmake --build build`、`ctest 2/2`、`--demo` 全通过。

---

### 2026-09-13

#### S07 — 缓存系统重构（深度融合）

**背景**：2Q 与 ARC 原本各自内置链表 + 哈希路由，相同逻辑重复实现。重构目标：提取公共底座，策略保持独立。

**LruListFanLX — 可复用 LRU 段底座**
- 封装「哈希路由（`unordered_map`）+ 双向链表（`std::list`）LRU 次序 + 字节账目」三者同步
- 接口：`pushFront / moveToFront / take / popBack / erase / evictToFit / locate / peek / contains`
- `take()` 严格遵循"先摘路由、再搬内容、再销毁结点"顺序

**LruCacheFanLX（2Q / LRU-K）**
- Buffer 段（约 1/4）+ Main 段（约 3/4），各用一条 `LruListFanLX`
- 晋升阈值 `k` 可配置；Buffer 中访问达 k 次即整条搬入 Main 队首

**ArcCacheFanLX（ARC + LFU）**
- L1/L2 数据段 + B1/B2 幽灵段（value 存频率快照，bytes=0）
- 冗余消除：无 `EntryFanLX::level` 字段，无 `ghostRouteFanLX`，字节数向数据段即时求账
- 新增查询接口：`contains / l2Frequency / averageL2Frequency / ghostFrequency`

**LfuFrequencyFanLX — 独立频次控制器**
- 增量维护 `totalAccessFanLX`：`averageAccessCount()` O(1)
- 频次衰减：平均计数超阈值时整体 `(n+1)/2`；`set(key,0)` 自动抬成 1

**行为等价性验证**：重构前后 A/B 差分对拍，4 组随机种子 × (2Q 20 万步 + ARC 20 万步随机 + ARC 2.4 万步确定性扫描)，全部结果一致。覆盖率：`maxP=6`、`p≠0` 步数 100303、晋升 30866 次、幽灵峰值 6。

**测试新增**：④ `LruListFanLX` 组件行为、⑤ ARC 查询接口/幽灵路径断言。

#### IDE 配置修复

- 重新运行 CMake 加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`，在 `build/` 生成 `compile_commands.json`
- 新增 `.clangd`（编译数据库路径）、`.zed/settings.json`（`--query-driver` + `--compile-commands-dir`）
- 修复后 Zed 零报错，MinGW 标准库头文件路径正确解析

#### S04 — 群域：全面实现（`--demo` 待补）

新增 `include/GroupDomainFanLX.h`、`tests/group_tests.cpp`，更新 `CMakeLists.txt`。

**数据结构层（100% 完整，首批实现）**

| 类型 | 说明 |
| ---- | ---- |
| `GroupRoleFanLX`（enum class）| Owner / Admin / Member 三档角色 |
| `GroupKeyFanLX` + `GroupKeyHashFanLX` | 稳定群标识（mode + id），`==` 与哈希均实现，避免用容器下标做外键 |
| `GroupFanLX`（稳定外壳）| `unique_ptr<GroupPolicyFanLX>` 持有策略（运行时多态），状态与策略分离 |
| `GroupFanLX::removeMember` 权限矩阵 | 四条规则：不能踢群主；普通成员无踢人权；管理员不能踢同级管理员；不能踢自己 |
| `GroupApplicationFanLX` / `GroupInvitationFanLX` | 数据容器（applicant/inviter/invitee/pending） |
| `DiscussionGroupFanLX` | 子群；`addMember` 前置检查 `parent.hasMember(id)` |

**Service 层业务流程（100% 完整，本轮新增）**

| 方法 | 语义要点 |
| ---- | -------- |
| `GroupFanLX::leave` | Owner 退群前抛异常，防止无主群产生 |
| `GroupFanLX::transferOwnership` | 先存在性检查再修改，自转让净结果正确，target 迭代器在遍历后仍有效 |
| `applyToJoin` | QQ 专属；`emplace` 对已存在 key 不覆盖，幂等；已是成员时静默跳过 |
| `approveApplication / rejectApplication` | requireManager 权限检查；批准后消费 pending 并入群；拒绝后移除 pending |
| `inviteMember` | Wechat 专属；邀请人须为群成员；`emplace` 幂等（同一 invitee 最多一条 pending） |
| `confirmInvitation` | 确认前重新验证邀请人仍在群，离群则清理 pending 并返回 false |
| `rejectInvitation` | 移除 pending，不存在返回 false |
| `leave`（Service） | 委托 GroupFanLX::leave；同步清理子群席位；撤销该 userId 发出的所有 pending 邀请（erase 返回新迭代器，安全） |
| `dissolve` | Owner 验权；按「子群→申请→邀请→群对象」顺序清理，无悬垂引用 |
| `transferOwnership`（Service） | 委托 GroupFanLX::transferOwnership，Actor 须为 Owner |
| `createDiscussion` | Owner/Admin 权限 + `canCreateDiscussion()` 双重检查；重复创建抛异常 |
| `getDiscussion` | 不存在时抛 `out_of_range` |
| `pendingApplicationCount / pendingInvitationCount` | 只读计数，供测试与界面用 |

**测试（T06–T09，本轮新增 `tests/group_tests.cpp`）**

| 测试组 | 覆盖场景 |
| ------ | -------- |
| T06 权限矩阵 | Member/Admin/Owner 踢人权边界（8 组允许/禁止断言）；setRole 升降；leave Owner 异常 |
| T07 QQ 申请审批 | 幂等申请；非 Manager 拒绝；Owner/Admin 均可审批；已是成员后再申请 count=0 |
| T08 微信邀请确认 | 模式互斥；非成员邀请异常；幂等邀请；离群邀请人 pending 撤销；preserved 邀请不受影响 |
| T09 子群+生命周期 | 父群成员约束；退群子群同步；策略禁止时 createDiscussion 异常；dissolve 全清；同号群重建后工作流空白 |

**⑤ `--demo` 群演示（本轮补全）**

在 `src/main.cpp` 中引入 `GroupDomainFanLX.h`，在 `--demo` 分支末尾追加完整群演示段，严格遵守 §19 约束（每步输出实际返回值与关键不变量，不绕过业务规则）：

- **QQ/1001**：U001 建群（Owner）→ U002 提升为 Admin → U003/U004/U005 直接入群 → U006 申请（重复申请幂等验证）→ Owner 审批 → Admin 踢 Member（U003）→ Admin 踢同级 Admin 被拒（权限矩阵）→ 创建子群 D1001（Owner 建立，父群成员约束，非父群成员 U999 被拒）→ U004 自愿退群并子群同步清除
- **Wechat/1001**：U001 建群 → 邀请 U002（重复邀请幂等验证）→ U002 确认入群（Member 权限，无管理特权）→ 群主转让 U001→U002 → 原群主退群

**最终验证（2026-09-13，S04 全部完成）**：clangd 零诊断，`--demo` 正常输出全部 ①–⑤ 段演示内容并 exit 0。

---

## S04 已完整关闭

数据结构层、Service 层、T06–T09 测试、`--demo` 群演示全部完成，CTest 3/3 通过，clangd 零诊断。S04 正式关闭，进入 S05。

---

## S05 设计与约束（2026-09-13 定案，对齐工程计划 §7）

### 核心矛盾与解决

§6.1“群的唯一键是 (ServiceType, GroupId)” 与 §7.1“GroupKey、群对象身份...保持稳定”两者看似冲突：如果 `GroupKeyFanLX.mode` 既是容器哈希键的一部分，切换时就不能改它，否则群对象会“换键”丢失身份。

**解决方案**：`GroupKeyFanLX.mode` 仅表示群的“创建时所属平台命名空间”（支撑 1001–1006 的分服务编号，创建后永久冻结，不参与切换）；群当前的**生效治理模式**是一个独立、可变的属性，通过 `policyFanLX->mode()` 暴露（新增 `GroupFanLX::currentMode()`），只能由 `switchPolicy` 修改。于是：
- `GroupServiceFanLX::requireMode` 从比对 `key.mode` 改为比对 `group.currentMode()`，不影响未切换过的群（创建时两者相等）。
- QQ 申请流 / Wechat 邀请流因此自然“按当前模式冻结”，无需额外 pending 标记字段（§7.2“未处理申请标记为暂停”自动满足）。

### 无损性的实现路径

`switchPolicy` 只做一件事：交换 `policyFanLX` 指针。**不触摸** `membersFanLX`、`discussionsFanLX`、`applicationsFanLX`、`invitationsFanLX`。因此：
- 成员、角色、加入顺序、子群、待审批申请全部原子保留 → 自动满足“无损”（§7.1）。
- “管理员配置保留为休眠数据”（§7.2）自动成立：Admin 角色从未被删除，只是其**生效权限**跟着新策略重新计算。
- “恢复仅限仍为成员者，已被踢/退群/被撤销者不恢复”（§7.3）自动成立：前两者已从 `membersFanLX` 删除，后者已被 `setRole` 改为 Member，均无需额外快照/恢复簿记。

此设计遵循 §7.5“选择策略模式与组合…继承复制会带来数据丢失与引用失效风险”的原始动机，且用最少代码量严格实现“无半切换”（单线程业务执行，策略所有权替换不抛异常；不表示跨线程原子事务）。

### 权限矩阵变为策略感知

§6.2 表格中“设置管理员：微信模式不提供有效管理员权限”意味着权限检查不能只看存储角色，必须同时询问策略。§9 建议接口已列出 `canAssignAdmin`，本轮补上：

- `GroupPolicyFanLX` 新增两个纯虚方法：
  - `virtual GroupModeFanLX mode() const = 0;`（QQ→QQ，Wechat→Wechat，支撑 `currentMode()`）
  - `virtual bool canAssignAdmin() const = 0;`（QQ→true，Wechat→false）——**同时表达两个含义**：能否新封 Admin，以及已持有 Admin 角色者当前是否生效（本项目规则下二者始终一致，不拆成两个接口避免过度设计）。
- `GroupFanLX::removeMember` 的权限矩阵新增一条：actor 为 Admin 时，只有 `policyFanLX->canAssignAdmin()==true` 才视为生效管理员，否则视为无权（与 Member 同等）。QQ 模式下行为与现有 T06 完全一致（不回归）。
- 新增 Service 层 `assignAdmin(key, actor, target)` / `revokeAdmin(key, actor, target)`：Owner 专属；`assignAdmin` 受 `canAssignAdmin()` 门控（Wechat 下拦截）；`revokeAdmin`（降级为 Member）始终允许——因为只减权不加权，不违反“微信不提供有效管理员权限”。

### 子群只读保留（§7.2）

`createDiscussion` 已在 S04 通过 `canCreateDiscussion()` 实现策略门控，切换到 Wechat 后自动抛异常，无需修改。但“扩员”（向已存在子群加新成员）当前无任何门控，需新增 Service 层包装：

```
bool joinDiscussion(parentKey, discId, actor, target)
    —— requireManager(actor) + canCreateDiscussion() 双重门控，再委托 DiscussionGroupFanLX::addMember
```

复用 `canCreateDiscussion()` 作为“子群管理整体开关”，与新建同门，无需另建新虚方法。退群触发的子群同步清理（`Service::leave`）不受此门控影响，无论何种模式都必须继续工作（§7.2“子集约束仍正常维护”）。

### 加入时间（§7.1）与无损对比接口

§7.1 要求“按照账号 ID 和加入时间规范化排序后比较”。本阶段尚无持久化（S06），无真实时间戳可用，采用**单调递增的加入序号**（`joinOrder: size_t`）作为确定性替代指标，可用于顺序比较且可复现，但不等同真实时间，待 S06 接入真实时间戳时再统一替换（非本轮范围，提前记录以免遗漏）。

数据形态变化：`membersFanLX` 从 `unordered_map<string, GroupRoleFanLX>` 改为 `unordered_map<string, GroupMembershipFanLX>`，其中 `GroupMembershipFanLX{ role, joinOrder }`；`GroupFanLX` 新增 `nextJoinOrderFanLX` 计数器，仅在 `addMember` 实际插入成功时自增。

新增对比接口：
```
struct GroupMemberSnapshotFanLX { string id; GroupRoleFanLX role; size_t joinOrder; operator== 手写（C++17 项目，不能用 C++20 = default） };
vector<GroupMemberSnapshotFanLX> GroupFanLX::snapshot() const;  // 按 id 字典序排序
```
`std::vector<T>::operator==` 自动逐元比较，无需额外实现对比函数。

受影响但**公开接口不变**的方法（需同步改内部存储）：`addMember`、`setRole`、`roleOf`、`removeMember`、`members()`、`transferOwnership`、`leave`——与 T06–T09 现有断言全部兼容，**不回归**。

### `switchPolicy` 接口设计（仿照现有 `transferOwnership` 分层模式）

```
// GroupFanLX（领域层：只检查策略非空，不检查 actor）
void switchPolicy(unique_ptr<GroupPolicyFanLX> newPolicy);

// GroupServiceFanLX（服务层：加 Owner 验权，与 transferOwnership 同模式）
void switchPolicy(key, actor, unique_ptr<GroupPolicyFanLX> newPolicy);
void switchToQQ(key, actor);      // 便捷封装，内部构造 QQGroupPolicyFanLX
void switchToWechat(key, actor);  // 便捷封装，内部构造 WechatGroupPolicyFanLX
```

“先验证目标策略、计算兼容状态、构造新策略；成功后在一个事务内替换”（§7.1）对应到：新策略在**调用方**构造完成后才传入 `switchPolicy`；若构造过程抛异常，`switchPolicy` 从未被调用，`policyFanLX` 保持原样——天然满足“不得出现半切换”（单线程业务执行，策略所有权替换不抛异常；不表示跨线程原子事务）。

### T10 测试设计（对应 §20 T10 + §7.4）

在 `tests/group_tests.cpp` 新增 T10 测试块（T06–T09 保持原样不变）：

| 子项 | 断言内容 |
| ---- | -------- |
| T10a 权限矩阵随切换变化 | QQ 模式下 Admin 能踢 Member、能建子群；切到 Wechat 后同一 Admin 两者均失败；Owner 始终可操作 |
| T10b 双向切换无损 | `snapshot()` 前后对比：QQ→Wechat→QQ 与 Wechat→QQ→Wechat 连续执行两轮，中间无其他业务修改时 snapshot 完全相等 |
| T10c 休眠与恢复语义 | QQ 模式 assignAdmin → 切 Wechat（roleOf 仍返 Admin，但 removeMember 失去权限）→ 切回 QQ（权限自动恢复）；另一人在 Wechat 下被 Owner revokeAdmin 降级 → 切回 QQ 验证其不再是 Admin（“被撤销者不恢复”） |
| T10d 子群只读 | QQ 模式建子群并加入一人 → 切 Wechat → `joinDiscussion`/`createDiscussion` 均抛异常（冻结）；父群退群同步清理仍正常工作（不受冻结影响） |
| T10e 失败不产生半切换 | 非 Owner 调用 switchPolicy 抛异常且策略/成员不变；传 nullptr 抛异常且不变；测试专用的“构造即抛”假策略子类（仅在测试文件内定义，不进入生产头文件）验证构造失败时 `switchPolicy` 从未被调用 |

### 明确边界：本轮不处理什么

- **重启保持新模式、写盘失败回滚**（§7.4 后两条）依赖持久化（S06），本轮仅能证明内存内无损，**不在 S05 声称已完成重启验证**，待 S06 接入后补足。
- 不引入 Weibo 群模式或群数三方切换（§7.2/7.3 仅讨论 QQ↔微信，未要求扩展）。
- 不在本轮强制要求 `--demo` 接入切换演示（可作为可选扩展，优先保证代码与 T10 测试正确）。

---

## S05 实施与验收（2026-09-13 完成）

按上述设计逐项落地：`GroupPolicyFanLX.h` 新增 `mode()` / `canAssignAdmin()`；`GroupDomainFanLX.h` 完成成员存储改造（`GroupMembershipFanLX` + `joinOrder`）、策略感知的 `removeMember`、`currentMode()`/`switchPolicy()`/`snapshot()`、`requireMode` 改为按群当前模式判断（6 个调用点同步更新）、Service 层新增 `switchPolicy/switchToQQ/switchToWechat/assignAdmin/revokeAdmin/joinDiscussion`。`tests/group_tests.cpp` 新增 T10a–f（含 T10f 六工作流入口模式门控测试，超出原设计的 T10a–e，属于测试严谨度的自发扩展）。

**验收结果**：CTest 3/3（smoke / cache_behavior / group_behavior）全通过，clangd 诊断检查通过（测试文件关闭 assert 宏相关重构动作自测），T06–T09 无回归。已验证 §7 的内存内切换条款；T10b 比较双向往返快照，T10e 同时比较策略对象地址、名称与成员内容。§7.4 存储相关条款尚待 S06。`requireManager` 顺带改为策略感知（非原计划要求，但推演确认在 QQ 专属流程中不产生行为差异，属合理一致性增强）。

重启保持新模式与写盘失败回滚按设计边界正确推迟给 S06，未虚报完成。S05 正式关闭。

---

## S06 设计与约束（2026-09-13 修订，对齐工程计划 §7、§8–10、§12、§17 与 P07）

### 1. 当前状态与范围

以下保留 S06 设计基线；2026-09-14 已完成 P07.1—P07.7，实际落地差异和证据见“S06 实施与验收”。沿用用户指定的 MinGW / CMake 3.20+ / C++17，替代计划旧版 MSVC 环境要求。

| 依据 | 必须完成 | 验收证据 |
| --- | --- | --- |
| R12、R21；§10.1 | 全字段快照、启动加载、显式保存退出 | T11 往返与独立进程重启 |
| §7.1–7.4 | 策略、休眠角色、加入信息、子群和暂停工作流保留 | S05 延迟验收：重启与写失败 |
| §8–10.2 | 仓储接口、候选事务、稳定群外壳、Windows 文件发布 | 地址不变、失败不变、代数验证 |
| §10.3、§17 | 单写进程、损坏保护、确认恢复 | T12 故障与中断测试 |
| §12 | 成功提交才发布局部版本，加载清缓存 | 版本不变测试；S07 接入查询缓存 |

S06 必须接入全部已存在的写用例，提供应用启动、恢复确认、saveAndClose 入口。完整数字菜单归 S09；仅实现可独立测试的 JSON 文件读写不能宣布 S06 完成。

### 2. 旧草案纠正与实施前置缺口

- 整体 swap 在线 GroupService 会替换群对象，违反稳定身份要求；候选可以复制，发布必须保留存续群外壳。
- 不先将主文件移动为备份。移动后主文件确实不存在，旧草案对此中断窗口的说明错误。必须先复制并发布备份，最后单次替换主文件。
- 不以锁文件存在代表进程活跃。CREATE_NEW 锁文件在崩溃后可能残留，改用 OS 独占句柄。
- joinOrder 只是加入顺序，不是真实时间。S06 保留它并新增 joinedAtUtc；历史未知时间使用 null 和来源说明，不伪造。新业务使用可注入 UTC 时钟。
- 当前可写底层引用可绕开事务；S06 必须建立应用事务门面，并限制恢复接口。账户/好友身份规则需按计划校验，不能把缺用户/账号的群单测夹具直接当作有效持久化快照。
- 踢人和退群都要同步清理子群及失效邀请。子群负责人转交、空子群归档、解散历史按 §6.3 补齐；S04 内存删除语义不等于最终历史保留契约。

### 3. 依赖与文件职责

固定 nlohmann/json v3.11.3，官方单头文件离线入库到 third_party/nlohmann/json.hpp，附 MIT LICENSE；docs/dependencies.md 记录来源、版本、实际文件 SHA-256 和升级规则。CMake 建 INTERFACE 目标及 nlohmann_json::nlohmann_json 别名，让基础设施目标显式链接；不靠联网 FetchContent 才能构建。

领域头文件不得包含 JSON/Win32。新增自编类独立头文件，非模板实现配 .cpp；不要把多个仓储类堆进一个文件。

| 拟新增类型/文件 | 职责 |
| --- | --- |
| PlatformStateFanLX.h | 在线状态聚合、代数；不包含 JSON |
| PlatformSnapshotFanLX.h | 纯 STL 快照 DTO，无句柄、裸指针或会话 |
| GroupStateFanLX.h | 稳定群外壳内可交换的成员、角色、加入信息与下一序号 |
| SnapshotCodecFanLX.h/.cpp | JSON 与 DTO 转换、确定性排序 |
| SchemaValidatorFanLX.h/.cpp | 结构/范围/唯一性/引用校验，先于领域恢复 |
| RepositoryFanLX.h | load/commit/recover 抽象接口，明确错误与恢复状态 |
| JsonFileRepositoryFanLX.h/.cpp | 文件仓储，组合校验器和适配器 |
| AtomicFileWriterFanLX.h/.cpp | Windows 写入、刷新、备份、主文件发布 |
| DataDirectoryLockFanLX.h/.cpp | 独占目录句柄 RAII |
| PreparedCommitFanLX.h | 预分配好的内存发布计划 |
| TransactionCoordinatorFanLX.h/.cpp | 候选修改、写锁、文件提交和内存发布 |
| PersistenceApplicationFanLX.h/.cpp | 启动、恢复确认、写用例与 saveAndClose |
| docs/data-format.md、docs/dependencies.md | 格式/样例/协议、依赖许可 |
| tests/persistence_tests.cpp、tests/fixtures/persistence/ | T11/T12 与故障夹具 |

以上是拟新增文件，不表示已经存在。现有合并域头文件的独立类拆分另作兼容性重构，保留原 include 入口与已测试公开签名。

### 4. schemaVersion=1 数据契约

使用单个 UTF-8 JSON 完整快照。业务 ID 全部为字符串；枚举使用受限字符串。schemaVersion、generation、parentGeneration 为检查范围的无符号整数，首次提交 generation=1，初始空内存为 0；溢出拒绝提交，不能回绕。

| 字段/记录 | 保存内容 |
| --- | --- |
| 顶层 | schemaVersion、generation、parentGeneration、committedAtUtc、versions、各域数组 |
| users | userId、nickname、birthDate、applyDate、location；T 龄按日期推导 |
| accounts | userId、service、accountId、sharedId |
| subscriptions | 结构化账号引用 (userId, service)，不保存登录态 |
| bindings | 同一已验证用户的 QQ/微信端点；解除关系不重现 |
| friendships | 每个无序账号对一条关系，包含双方各自 remark/tag |
| groups | key{namespaceMode,id}、currentMode、群资料、成员、nextJoinOrder、局部版本、工作流与子群 |
| members | 用户/服务账号引用、role、joinOrder、joinedAtUtc |
| applications/invitations | 参与者、明确状态、创建时间、可选失效时间；必要历史另存，不冒充 pending |
| discussions | discId、父群键、成员、负责人、归档状态 |
| versions | 目录/绑定/资料、双方好友邻接/本人备注、群成员/角色/策略/工作流版本 |

namespaceMode 永久对应创建命名空间，currentMode 来自策略；不要求二者相等。Admin 原样保存，是否生效由策略计算。Pending 且当前模式不匹配即暂停，不保存容易失同步的额外 paused 字段。通过/拒绝/撤销/失效记录使用明确状态与历史容器。

nextJoinOrder 必须显式保存，不能只用最大存活序号加一推导：最后加入者离群会导致已用序号丢失。它须大于所有现存 joinOrder，重启保留空洞。角色修改/转让/模式切换均不重置加入信息。

文件使用结构化引用，不解析内部拼接键作为格式。邻接索引、推荐结果、会话、缓存、Socket/线程/锁不持久化；恢复统一建索引、清会话和缓存。

### 5. 校验与受控恢复

顺序：读取受控目录 → 解析版本 → 类型/长度/范围校验 → 唯一键/引用校验 → 构造临时完整状态与索引/策略 → 全部成功才发布。

在 data-format.md 冻结可配置限额：基线文件 64 MiB、嵌套深度 32、ID 128 UTF-8 字节、普通文字字段 4096 字节；至少容纳计划的 10000 用户、100000 好友关系和 1000 群。对子集合和累计记录数分别限额；扩大基准时同步评估内存。文件大小/深度应在读取和解析期间限制，不能先无限分配。

拒绝重复 JSON 属性、非法 UTF-8、错误字段类型、负数/浮点代数、未知版本/枚举、重复用户/账号联合键/服务号码/群键/子群键、超界数值和非法日期。校验账号归属、QQ/微博共享身份、微信绑定、订阅、好友两端及双方私有资料完整性。活动群恰好一个 Owner；成员引用创建命名空间内有效服务账号，切换不要求偷偷开通另一服务。子群活动成员为父群子集，负责人有效；归档历史按历史语义校验。

提供只读 STL 导出和受控 restore 工厂/友元接口；恢复精确序号、时间、当前模式与下一序号。禁止菜单调用恢复入口，禁止按普通审批/任命方法重放休眠角色和冻结子群。好友关系先校验再建立双向邻接，不能导入互相矛盾的两套索引。

已有 pending 可能在后续失效；重启不自动批准，审批/确认时重查已入群、过期、邀请人资格。关闭失效记录通过写事务完成，不在只读 load 中悄悄改盘。

### 6. 候选状态、稳定外壳与内存发布

采用候选 DTO/独立可变状态，复用领域规则。无需为整个在线 GroupService 强行增加复制构造；无状态内建策略在准备阶段由 mode 工厂构建，未知类型拒绝。未来有状态策略另定义克隆/序列化协议，不在本阶段强加 clone 纯虚方法。

在线注册表拟持有 shared_ptr<GroupFanLX> 稳定外壳。新注册表对存续群复用同一外壳，对新群提前创建对象，对解散群移除引用。候选必须是独立 GroupState，禁止通过共享指针提前修改在线成员。需要稳定引用的子群同样分离外壳与成员状态。

PreparedCommit 提前准备：存续群内部数据和策略、新注册表、账号/好友状态、申请/邀请/子群、代数/版本和返回值。发布持写锁，交换存续外壳内部数据，再发布注册表与代数。存续对象地址不变；解散后引用失效属于生命周期结束，业务应按稳定键查询。

发布不得分配、序列化、构造策略、调用可能抛出的哈希查找或回调。所有定位提前完成；用具体成员 swap、默认分配器/哈希约束与 static_assert(noexcept(...)) 验证。不能笼统声称所有 STL swap 天然 noexcept。读操作与写锁互斥或在单线程事件循环执行；指针赋值本身不是跨线程事务保证。

事务顺序：

1. 验证会话、权限和当前 generation，复制候选，不改在线对象。
2. 候选执行命令与全量不变量验证；幂等无变化不消耗 generation。
3. 预分配发布计划、策略与返回结果，形成新代数/局部版本及快照。
4. 仓储提交。提交点前失败丢弃候选，原数据/策略/版本不变。
5. 磁盘确定提交后执行 noexcept 内存发布，再返回预制成功结果。
6. 缓存维护/统计/日志不得抛异常使已成功业务被误报回滚；缓存正确性由版本键保证。

账号/好友服务持有的管理器外壳也不替换，只交换内部数据。同进程提交保留当前会话，新进程 load 清会话。禁止先改在线对象再保存、失败后补救回滚。

### 7. Windows 文件协议与提交点

受支持基线是 Windows 本地同卷数据目录。宽字符路径；主文件 state.json、上一有效版本 state.json.prev、唯一临时文件 .tmp.<generation>.<nonce>。提交元数据嵌入快照，避免单独成功标志与主文件不一致。

1. 持独占目录锁，验证旧主代数；候选 generation=旧代+1，parentGeneration=旧代。
2. 完整写入同目录临时文件，处理短写并检查每次返回值；FlushFileBuffers 后关闭。
3. 已有主文件时，将已验证主文件复制到备份临时文件，刷新/关闭，再用 MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH) 发布为 .prev。主文件始终保留。首次创建不伪造备份。
4. 最后同卷 MoveFileExW 发布候选为主文件。首次创建禁止覆盖意外存在目标；已有主文件使用覆盖标志。此主文件发布为逻辑提交点，不先删除/移动旧主文件。
5. 提交成功后立即发布已准备的内存状态。日志/临时文件清理失败不报告业务回滚。

提交点前中断读取旧主文件；备份可能与旧主代相等，这是有效冗余。提交点后、应答前崩溃，重启可能已经是新代；未收到应答不代表未提交，必须记录此边界。

若 API 返回失败且磁盘结果不确定，返回 CommitIndeterminate、阻断后续写入，持锁重新校验主代数/候选内容；确定已提交才同步内存，不能确定则进入恢复流程。不能一概承诺所有异常都保持磁盘旧字节。

仅承诺实测支持文件系统的进程崩溃恢复。FlushFileBuffers 与写穿透不等于任意硬件断电零丢失；测试记录操作系统、文件系统和故障边界。

### 8. 单写进程、启动状态与恢复确认

规范化绝对数据目录后，CreateFileW(OPEN_ALWAYS, shareMode=0) 持有 .lock 句柄直到仓储关闭；共享冲突、访问拒绝和路径错误分别报告。进程被终止时 OS 释放句柄，残留锁文件不阻止重启。正常关闭可保留锁文件，不靠删锁抢占。单进程事务另用写锁。

| 文件状态 | 加载结果与写权限 |
| --- | --- |
| 全新空目录（忽略锁文件） | Empty；可显式初始化，不自动落盘 |
| 主文件合法且版本受支持 | Loaded；完整恢复后允许写入 |
| 主文件缺失/损坏，备份有效 | RecoveredReadOnly；说明来源代数与可能丢失提交，确认前禁止写入 |
| 主备均无效 | LoadFailed；保留原文件，不进入可写菜单 |
| 主文件未知 schemaVersion | UnsupportedSchema；不静默降级，显式选择兼容备份才进入恢复确认 |
| 仅遗留 tmp | UncommittedOnly；不当作已提交状态，需显式初始化 |

主文件优先，绝不按最大 tmp 代数猜测提交。主备代数矛盾必须诊断；读取失败不等同空目录。

确认恢复时，先保存损坏主文件的唯一诊断副本并核验；失败维持只读。将验证过的备份发布为恢复主文件，记录来源/旧代（可读时）/时间/原因，之后才开放写入。保留有效备份，不能复用正常“主转备份”流程把损坏主文件覆盖到 .prev。恢复失败保留证据与只读状态。

### 9. 全写用例接入与退出

| 写用例 | 同事务变更与版本 |
| --- | --- |
| 账号/开通/资料/绑定 | 用户、账号、订阅、绑定一致；目录/资料/绑定版本 |
| 好友增删、备注 | 双向关系及私有资料一致；双方邻接/本人备注版本 |
| 群创建/加退踢 | 成员、子群席位、失效邀请一致；相关成员版本 |
| 管理员任免/群主转让 | 仅角色改变，加入信息不变；角色版本 |
| 模式切换 | 策略/角色版本改变，成员不变 |
| 申请/邀请及审批确认 | 工作流与成员原子更新；工作流/必要成员版本 |
| 子群/归档/解散 | 父子引用、负责人、历史及待处理记录一起更新 |

查询不读文件，权限直接检查权威状态。登录/注销不持久化。恢复清缓存并重建索引；S07 再使用版本键接入查询。

S06 提供启动与 saveAndClose 测试入口；无新修改不制造重复提交。保存失败报告错误/重试，不显示成功；析构仅释放资源、不抛异常、不充当断电保存保障。demo 与测试使用隔离目录，不清空默认 data。

### 10. T11/T12 与 S05 延迟验收

新建 persistence_tests、注册 persistence_behavior，保留现有三个测试。所有测试使用隔离目录和合法夹具；关键断言在 Release 也执行。故障替身通过仓储/文件适配器注入，真实中断通过辅助子进程。

| 用例 | 判定 |
| --- | --- |
| T11a | 全字段往返：账号/开通/绑定/双方备注、同号群、策略、加入信息、工作流与子群逐项比较 |
| T11b | 独立进程重启；开通保留、会话清空；nextJoinOrder 含最后加入者离群空洞不变 |
| T11c | 双向切换保存重启；休眠管理员恢复、撤销/退群者不复活；工作流暂停恢复 |
| T11d | 多次事务存续群地址稳定；转让/切换不改加入信息；同号群隔离 |
| T11e | 全写用例接入事务、退出保存结果明确，失败不显示成功 |
| T12a | 空目录/仅 tmp/主缺失但备份有效分别处理，tmp 从不自动提交 |
| T12b | 主损坏/双损坏/未知版本；坏文件字节保留；恢复未确认不能 commit |
| T12c | 错误类型/长度/深度/UTF-8/重复键/缺引用/双 Owner/子群越界/非法代数全部拒绝 |
| T12d | 候选/策略构造/序列化/分配失败，在线快照、版本、原文件不变 |
| T12e | 短写、flush、备份、主发布逐点失败；按提交点验证旧或新完整状态 |
| T12f | 子进程在 tmp 写后、备份后、主发布前、主发布后内存发布前、应答前终止；重启校验完整代数 |
| T12g | 模式切换写失败保留旧策略对象/成员/版本；成功重启保持新模式，补齐 §7.4 |
| T12h | 第二进程同目录被拒；首进程正常/异常结束后重新获得锁；不同目录互不阻塞 |
| T12i | 恢复失败保留有效备份/证据，确认成功后可再提交；代数矛盾与不确定结果阻断写入 |
| T12j | 中文路径、访问拒绝及空间不足/flush 故障注入，区分真实 OS 测试与模拟故障 |

证据按 §20 记录用例/需求、前置快照/代数、输入、预期/实际、构建信息、故障点和文件路径。不可写路径不等于磁盘写满，硬件故障未测不得宣称通过。

### 11. 实施拆解、交付与辅助提示词

| 顺序 | 产物 | 放行标准 |
| --- | --- | --- |
| P07.1 | JSON 依赖/许可证、DTO、data-format.md、完整夹具 | 离线构建，字段/约束可审查 |
| P07.2 | Codec/Validator、导出和受控恢复 | 内存 round-trip，非法输入不发布 |
| P07.3 | GroupState、稳定注册表、PreparedCommit、局部版本 | noexcept 发布检查、存续地址不变 |
| P07.4 | 仓储接口、目录锁、Windows 文件适配器 | 首次/覆盖/备份/锁真实测试 |
| P07.5 | 启动与恢复确认、代数/故障注入 | T12 中断恢复矩阵 |
| P07.6 | 全写用例协调器、启动/saveAndClose | 失败不发布、S05 延迟验收 |
| P07.7 | persistence_tests、README、故障证据及进度 | 原有回归与新测试通过，Debug/Release 干净目录离线构建 |

可复用执行提示词：

> 依据 docs/progress.md 的 S06 第 1–11 小节与工程计划 §10/§12/P07，完成 P07.N（替换为当前步骤）。先核对前置验收和实际代码，按 FanLX 命名、C++17/MinGW、新增类独立文件和中文不变量注释实现。领域不得依赖 JSON/Win32；不得替换存续群外壳、绕过事务或恢复门控。完成该步骤产物、必要回归和文档记录，未实现后续步骤明确保持待实施；不得伪造测试与断电保障。

关闭 S06 必须同时满足：全写用例接入、字段重启正确、稳定外壳、失败不变、单写/显式恢复、T11/T12/S05 延迟项通过、离线依赖与文档齐全。仅完成序列化不算阶段完成。

---

## S02 修订 — 统一登录语义纠正（2026-09-14）

### 问题

原 `login(user, service, confirmOthers)` 以 `userId` 为登录入口，并在 `confirmOthers=true` 时用
`key.rfind(user + "|", 0) == 0` 前缀扫描，把该用户**所有已订阅平台**一并置为登录态。
于是"登录 QQ 会顺带登录微信"，唯一依据是"属于同一个人"——这与现实不符：
微信号与 QQ 号是完全独立的账号，`userId` 只应是后台内部聚合维度。

同时暴露出两处既有缺陷：

- `ServiceAccountFanLX::sharedId` 被声明为"用于统一登录识别"，却从未被登录逻辑读取；
- `bindingsFanLX` 是**只写不读**的死字段（`bindWechatToQQ` 写入后无人消费）。

### 修订方案

**① 身份分层，登录以平台账号为主体**

| 重载 | 语义 |
| ---- | ---- |
| `login(service, accountId, confirmOthers)` | **主入口**。调用方持有的是 QQ 号 / 微信号，与真实系统一致：平台校验凭据 → 返回账号 ID → 由账号反查归属用户。账号不存在时抛"平台账号不存在" |
| `login(user, service, confirmOthers)` | 便捷重载。已知归属用户时用于定位该用户在该平台的账号，`userId` 仅起"找哪个账号"的作用 |

两者共用私有核心 `loginCoreFanLX`，行为完全一致。

**② 联动只依据"同号"或"显式绑定"，不再依据"同一个人"**

`confirmOthers == true` 时，遍历范围仍是该用户已开通的平台，但入列条件收紧为：

| 依据 | 实现 | 含义 |
| ---- | ---- | ---- |
| 同号 | `sameSharedAccountFanLX`：双方 `sharedId==true` 且 `accountId` 相同 | 同一账号 ID 在多个平台复用（QQ 与微博） |
| 显式绑定 | `boundFanLX`：`bindingsFanLX` 双向命中 | 账号 ID 不同但用户主动绑定（微信绑定 QQ） |

只有"同一个人"、既不同号也未绑定时 **不联动**。`sharedId` 与 `bindingsFanLX` 由此都成为真实读者。

**③ 明确并文档化既有不变量**

`addAccount` 保证"一个用户在一个平台只持有一个账号"，故存储键 `"userId|服务名"` 是该平台账号的
**无损别名**，可继续安全用作 `subscriptionsFanLX` / `loggedInFanLX` 的键；好友域
`IdentityResolverFanLX` 的 3 段键在此不变量下第三段不产生新维度，保留以维持键格式对上层稳定（已在头文件注释写明）。

### 影响与验证

| 文件 | 改动 |
| ---- | ---- |
| `include/AccountDomainFanLX.h` | 新增 `sameSharedAccountFanLX` / `boundFanLX` / `loginCoreFanLX`；`login` 拆为主入口 + 便捷重载；补 `<algorithm>` |
| `include/FriendDomainFanLX.h` | 仅注释：说明 3 段键与账户域"1 人 1 平台 1 账号"不变量的关系（接口与行为不变） |
| `src/main.cpp` | 演示改用 `login(QQ, "20260001", true)`；U001 增加微博订阅以展示同号联动；新增"U002 不联动"对照输出 |
| `tests/cache_tests.cpp` | 原第 ③ 组循环改为分别登录 QQ 与微信，并断言"仅同人不足以登录微信"；新增第 ⑥ 组统一登录语义断言 |

第 ⑥ 组断言覆盖：同号联动、不同号且未绑定不联动、绑定后联动、未开通拒绝登录、账号不存在拒绝登录、
`confirmOthers=false` 时即使同号也不联动。

**输出编码注意**：演示程序新增的标签改用 ASCII（`same-id` / `bound`）。控制台代码页为 cp936 而源码按
UTF-8 编译，直接输出中文会显示为乱码；既有演示输出一直全为 ASCII 故此前未暴露。中文仅保留在源码注释中。

**验收（2026-09-14）**：`cmake --build build` 零警告；`cache_tests` / `group_tests` 退出码 0；
CTest 3/3（smoke / cache_behavior / group_behavior）通过；`--demo` 输出
`Unified login(QQ 20260001): QQ=true Weibo(same-id)=true Wechat(bound)=true` 与
`No-propagation check U002: QQ=true Wechat=false`。

---

## S02 修订 v2 — 前端身份会话化（2026-09-14）

### 问题

上一轮把 `login` 改成了以平台账号为主体，但**其余接口仍以 `userId` 为参数**：
`bindWechatToQQ(user)` 与 `FriendServiceFanLX` 的 `(user, service)` 双端签名都是前端 API，
等于把内部主键直接暴露给调用方，与"user 完全作为后端内部标识"的设想不符。

### 依据（工程计划原文）

| 出处 | 要求 |
| ---- | ---- |
| §4.1 | `平台用户以不可变内部 UserId 标识真人，服务账号以 (ServiceType, AccountId) 标识，服务开通记录以 (UserId, ServiceType) 标识` —— 三层身份 |
| §4.2 | 绑定必须属于同一平台用户、目标账号存在且归属可验证；绑定要提供**建立、查询、解除**接口；`不能输入任意 QQ 号就绑定他人账号` |
| P05 | 跨服务匹配只使用**明示的共享 ID 或已验证绑定映射** |
| P03 | S02 须实现**平台会话**、**自主开通**、**当前服务注销与平台注销** |
| 验收 | `接口错误能区分未开通、未确认、已过期`；`重启不恢复旧会话` |

结论：会话不是过度设计，而是计划中的既有交付项；本轮把它补上，并把 `userId` 收回后端。

### 修订内容

**① 新增 `AccountSessionFanLX`（登录会话）**

- 由 `login(service, accountId, confirmOthers)` 签发；对外只暴露 `service()` / `accountId()`。
- `userIdFanLX` 是**私有成员，不提供任何访问器**，前端无法取得。
- 需要用户维度的操作在会话内部完成：`subscribeTo`（自主开通）、`logoutAll`（平台注销）。
- 生命周期：不持久化（计划「重启不恢复旧会话」）；`logout` / `logoutAll` 后 `isLoggedIn()` 为 false。

**② 删除 `login(user, service, confirm)` 便捷重载与 `bindWechatToQQ(user)`**

登录只认平台账号。绑定改为以平台账号为参数，归属校验在内部完成：

| 能力 | 会话接口 | 后端接口 |
| ---- | -------- | -------- |
| 建立绑定 | `session.bindTo(targetService, targetAccountId)` | `bindAccounts(...)` |
| 解除绑定 | `session.unbindFrom(...)` | `unbindAccounts(...)` |
| 查询绑定 | `session.boundAccounts()` | `isBound(...)` / `bindingsOf(...)` |

**③ 好友域改口径：发起方 = 会话，对方 = 平台账号**

`addFriend(self, peerService, peerAccountId)` 等 7 个接口全部换签名 —— 现实中加好友用的是对方的
QQ 号 / 微信号。`IdentityResolverFanLX` 区分前端口径（`accountKey(service, accountId)`）与内部口径
（`accountKeyOfUser`），`userId` 只在内部参与跳平台聚合。

**④ 补齐错误分类**

`ServiceNotSubscribedFanLX`（未开通）/ `ServiceNotConfirmedFanLX`（未确认）/ `SessionExpiredFanLX`（已过期），
均继承 `std::runtime_error`，旧的通用捕获继续有效。

**⑤ 账号 ID 唯一性**

既然 `accountId` 是登录身份，就必须能唯一定位账号。`addAccount` 新增"同一平台账号 ID 不得被两人共用"
校验，并引入索引 `accountIdIndexFanLX`（同时把按账号 ID 的定位从 O(n) 扫描降为 O(1)）。

### 影响与验证

| 文件 | 改动 |
| ---- | ---- |
| `include/AccountDomainFanLX.h` | 会话类、错误分类、账号 ID 索引、绑定建立/查询/解除、注销；删除 userId 版 login 与 bindWechatToQQ |
| `include/FriendDomainFanLX.h` | 门面 7 个接口改为「会话 + 平台账号」；解析器区分前端/内部口径 |
| `src/main.cpp` | 绑定由微信会话发起；用 QQ 号登录；加好友改用对方 QQ 号 |
| `tests/cache_tests.cpp` | ③ 改用会话与平台账号；⑥ 新增会话生命周期、绑定三接口、平台注销、错误分类、账号 ID 唯一性断言 |

**验收（2026-09-14）**：编译零警告、clangd 零诊断；CTest 3/3 通过；`--demo` 输出
`Binding(Wechat -> QQ 20260001): bound=true` 与
`Unified login(QQ 20260001): QQ=true Weibo(same-id)=true Wechat(bound)=true`。

### 2026-09-14 历史待办（第 1、2 项已于 S07/S09 完成）

1. **跨服务匹配的依据与计划不一致**：计划要求 `经共享ID或有效绑定映射到目标服务`，
   而当前 `recommendFriends` / `crossServiceCommonFriends` 是用内部 `userId` 把对端的
   源平台账号映射到目标平台账号。按 P05 口径，未绑定微信的用户不应被推荐——
   若改按"共享 ID / 绑定映射"，现有演示与测试的推荐条数会变化。**属行为变更，待确认后再动。**
2. **逐项确认登录未实现**：计划的"一次简单确认登录全部或逐项确认"目前只实现了批量
   （`confirmOthers=true`），逐项确认需新增按服务确认的接口。
3. **`sharedId` 的写入方尚未定**：目前由建号时的 `addAccount` 传入，缺少"标记 / 取消同号"的操作接口。

---

## S06 实施与验收（2026-09-14）

P07.1—P07.7 已实现。当前代码采用 C++17、MinGW GCC 15.1.0，实际验证环境为 Windows/NTFS。未改动原始任务书；工程报告中的旧文件数量及“持久化未实现”历史描述需在 S11 正式报告冻结时更新。

### 实际代码与设计对照

- 固定 nlohmann/json v3.11.3 已离线入库，MIT LICENSE 与 SHA-256 登记于 docs/dependencies.md；CMake 的 nlohmann_json::nlohmann_json 目标不联网下载。
- PlatformSnapshotFanLX / SnapshotCodecFanLX / SchemaValidatorFanLX 实现全字段 DTO、结构化编码、UTF-8/重复属性/深度/容量/类型/日期/唯一性/引用校验，恢复前先验证。完整合法样例由真实程序提交产生，位于 tests/fixtures/persistence/valid-state-v1.json。
- GroupStateFanLX 保存成员/nextJoinOrder/joinedAtUtc；GroupService 以 shared_ptr 注册稳定群与子群，PreparedCommit 预先定位原外壳并交换内部状态。存续群/子群地址不变；角色、模式、时间与序号跨重启保留。
- RepositoryFanLX 为多态接口，JsonFileRepositoryFanLX 组合独占目录锁和 Windows 文件适配器；临时写入→刷新→复制备份→主文件发布→noexcept 内存发布，首次创建与已有文件分别处理。
- DataDirectoryLockFanLX 持有 OS 独占句柄，进程崩溃后自动释放；锁文件残留不妨碍重启。宽字符路径与 wmain 修复中文路径子进程参数问题。
- Empty、Loaded、RecoveredReadOnly、UnsupportedSchema、UncommittedOnly、Failed 状态明确；恢复需显式确认，保留损坏主文件副本及恢复记录，不以坏主覆盖有效备份。提交结果不确定时阻断写入。
- PersistenceApplicationFanLX 提供注册/开通/绑定/资料/好友/群/角色/模式/工作流/子群/解散等事务入口，外部仅获得 const 在线状态。注册首个账号为用户选定的初始服务，其余需显式开通；会话只在内存保留。
- main 已接真实启动、初始化、恢复确认和 saveAndClose；--demo 使用唯一临时目录完成存档、切换、关闭重开和全字段比较，不改默认 data。**历史基线记录**：本段形成于 S06，当时数字菜单仍属 S09；该阶段已在 2026-09-22 完成。
- 账户、会话、好友、群、策略类型拆为独立头文件，原 AccountDomain/FriendDomain/GroupDomain 保留兼容 include；新增领域代码不依赖 JSON 或 Win32。原 tests/cache_tests.cpp 和 tests/group_tests.cpp 保持其已有场景。

### 落地细节与兼容性决定

- 未新增 clone 纯虚方法；候选策略按受支持 mode 工厂重建。内部恢复只创建候选对象，不提供在线可写 restore 入口。
- 归档群保存完整历史片段；工作流按参与者保存最近一次状态，再次申请替换已结束的那次记录，不声称具备无限历史日志。子群清空时保留归档对象，有权限者在 QQ 下重新扩员可激活，兼容 T10d 的回切恢复测试；微信仍禁止管理性扩员。
- **历史基线记录**：S06 阶段尚无业务查询缓存接入在线状态；后续 S07 已接入 ARC/内存池查询缓存并通过 T13。恢复时仍重新创建应用领域状态，缓存和会话不写入快照。
- 临时文件名采用 pid/tick/sequence 唯一后缀，不把临时文件名中的数字当提交代数；权威代数只读取主/备快照内容。
- 高频文件替换实测曾出现 Win32=5；文件适配器增加最多 190 ms 的有界占用重试，只读目标立即失败，未清除用户文件权限。
- 恢复发布后若重新校验/内存重建失败，关闭应用仓储，要求重启重新加载；不允许旧内存继续覆盖存档。恢复记录的 oldGeneration 仅在原文件可可信解析时填写，否则为 null。

### 验证记录

- **历史基线记录**：S06 当时默认 build、Debug、Release 均为 CTest 4/4；S07/S09 完成后测试扩展为 6 项，最新结果见文末 S10 收敛日志。
- 持久化库、新入口与测试采用 -Wall -Wextra -Wpedantic -Werror。核心源文件 clangd 检查 0 errors（--tweaks=none 关闭 assert 宏重构自测）。
- T11：账号/绑定/中文备注/同号群/子群/角色/时间/序号/工作流全字段 round-trip；新进程重启清登录态；对象地址不变；全部写用例与无变化不增代。
- T12：七个提交前位置注入异常；六个位置真实终止辅助进程；损坏/双损坏/未知版本/孤立 tmp/缺主/代数矛盾/恢复失败/外部改写/提交结果不确定；独占锁释放与只读文件访问拒绝；非法 JSON/引用/角色/序号及过期工作流。
- S05 延迟项：模式切换保存后重启仍为新模式；写失败时策略对象地址、成员快照、局部版本和旧主文件不变；原 T06—T10 无回归。
- 示例目录 .artifacts/S06/sample-data 已通过 --init-demo 与第二次启动实测；输出 users=12、generation=2。构建与测试证据详见 docs/persistence-validation.md。

未进行真实硬件断电或真实磁盘耗尽试验；写入/flush 失败通过注入验证，只读文件拒绝和进程崩溃为真实 OS 实测。不能将本阶段结论写成任意硬件绝对零丢失。S07 查询缓存、S09 数字菜单已经接入；TCP 选做与最终报告冻结仍属后续阶段。

---

## 2026-09-22 S07 实施中日志

本轮继续沿用已分层的 include/account、friend、group、infra、persistence 目录，不恢复旧扁平布局。

- 已修正 2Q 为两条 std::list 与唯一 unordered_map，Entry 保留 freq/level；两区预算独立、晋升使用 splice。
- 已连接复合 ARC 的比例反馈、最近 K 次历史、L2 LFU 淘汰及平均阈值/周期衰减，LFU 不再重复记账；T13 已验收。
- 内存池修复过对齐、BlockSize=1、构造失败槽位回收及禁止浅复制，增加实测计数。
- 在线好友账号/群成员查询已接 ARC 与对象池，局部版本键失效，权限必须先于命中检查；缓存异常只降级回源。
- T13、固定种子基准、数字菜单与菜单回归均已完成；结果见 docs/cache-validation.md，未实测硬件断电和磁盘写满仍不作承诺。

---

### 2026-09-22 S07/S09 完成日志

- T13 和 `console_tests` 在默认/Release 中均通过，CTest 从 4 项扩展为 6 项。
- `cache_benchmark` 产生 `.artifacts/S07/cache-benchmark.csv`，固定种子 2026、1012 用户、112 群、3 类负载、1000 查询×5 轮×缓存开关；各开关结果逐项相等。
- `src/ui/ConsoleAppFanLX.cpp` 接入严格数字解析、EOF/返回、登录、服务/好友/群操作、恢复和保存退出；`--demo` 补齐 D01–D08 实际演示。
- 复核并收紧推荐规则：跨服务映射必须是 sharedId 同号或显式 binding；仅同一 userId 不再足够。
- 报告 `deliverables/樊陆旭_面向对象课设工程文档.docx` 已更新学期、菜单、持久化、缓存和基准状态；Word 导出 17 页，已检查首尾页及新增实现页布局。
- `.artifacts/S07/ctest-default.log`、`ctest-release.log`、`cache-benchmark.csv`、`final-manifest.txt` 与 clangd 日志保存了本轮证据。

---

## 后续整体路线

### 2026-09-23 S08/S10 实施中日志

- 新增 network 独立模块：Winsock/套接字 RAII、IPv4 监听/连接、4 字节大端长度帧、严格 JSON/UTF-8 校验、128 条有界队列和接收线程。
- 新增 QQChatSessionFanLX 主线程门面，主菜单第 6 项进入 QQ 双端聊天；发送/读取前检查当前 QQ 会话和好友资格，校验接收方、发送方及连续序号。连接/监听有超时，0/EOF/异常回收线程与句柄。
- T14 已编写拆包、粘包、非法帧、半帧断连/超时、队列满、端口占用、两个进程 100 条双向中文消息、越权拒绝等测试；尚未以本段记录宣告全部通过。
- 网络线程不访问领域状态；聊天不进入 state.json，账号仍为教学模拟身份，未实现远端加密认证或离线投递。

S05—S10 已完成当前约定范围内的实现与回归 → S11 正式报告/发布。S08 通过独立有界队列向主线程投递消息，不修改持久化快照。

| 阶段 | 待完成工作 | 验收出口 |
| --- | --- | --- |
| S06（已完成） | 保持回归，后续新增业务须走统一事务 | T11/T12、完整事务与恢复 |
| S07（已完成） | 业务查询 ARC/内存池接入、版本失效、LFU/2Q/ARC T13、固定种子基准 | T13 开关结果一致；1000 查询×9 轮、缓存/回源与写延迟 CSV |
| S08（已完成） | Winsock RAII、有界队列、长度帧，网络线程仅提交事件 | T14 中文双端、100 条消息、拆包/断连；不同 data 目录 |
| S09（已完成） | 仓储启动/恢复确认/退出、数字菜单、getline 校验、0 返回、隔离 demo | T15 非法输入/EOF、D01–D08 真实结果 |
| S10（已完成） | R/O/E 矩阵、身份/好友/群/存储/缓存链复核及缺陷修复 | 故障回归、干净目录离线验证；只报告实际可用的 MinGW 检查结果 |
| S11 | 与代码一致的 UML、测试/性能/故障记录、约 5 页报告、源码与依赖许可 | 樊陆旭、552502班、2026-2027学年第一学期封面；任务书逐项验收与发布 |

S07 基准结果和菜单验收记录见 docs/cache-validation.md；缓存性能仅代表本机单线程内存领域测量，不能外推硬件、线程或数据库吞吐。S08 TCP 已实现并通过本机双进程测试；S11 最终报告冻结仍未完成。S06 未实施真实硬件断电/磁盘写满试验。

---

## 构建与测试命令

```bash
# 配置（修改 CMakeLists.txt 后需重跑）
cmake -B build -G "MinGW Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 编译
cmake --build build -- -j4

# 全量测试
ctest --test-dir build --output-on-failure

# 功能演示
build\fanlx_app.exe --demo

# 直接运行各测试
build\cache_tests.exe
build\group_tests.exe
build\persistence_tests.exe
```

## 2026-09-23 S10 首轮集成收敛记录（历史基线）

- 修正 S06 历史状态说明，保留当时 4 项测试的真实记录；当前为 6 项测试。
- 补齐 README 的 UI、基准及测试目录；新增 scripts/verify-release.ps1，可从任意目录执行，使用唯一构建目录、逐阶段检查退出码并保存日志与 SHA-256 清单，不清理用户数据或旧证据。
- 独立目录 .artifacts/build-s10-0923 完成 MinGW GCC 15.1.0 / C++17 Release 全量构建，CTest 6/6 通过（2.05 秒），包含实际隔离演示、故障恢复、查询缓存和控制台回归。
- 本轮未修改业务算法；S10 仅完成可重复构建与已有集成回归，不能据此宣称 R/O/E 全矩阵追踪已完成。
- 后续：补齐任务书条款到测试的追踪矩阵；按选做方案实现 S08 TCP 并增加 T14；最后更新 UML 和最终报告，执行 S11 发布冻结。原始任务书保持不变。


## 2026-09-23 S08 与 S10 完成日志

- S08 新增 7 个独立网络类与 QQChatSessionFanLX 应用门面，菜单 6 提供监听、连接、发送、读取和断开。严格长度/UTF-8/协议字段校验，短写/拆包处理、有界队列、半帧总超时、关闭唤醒接收并 join 均已实现。
- T14 在真实双进程、独立 A/B 数据目录、动态回环端口下逐条比对 100 条中文请求与 100 条应答；两端聊天前后领域快照相等。另测非法帧/重复属性/超限、分片合并、断连、队列满、监听超时、端口占用、连接拒绝、账号/序号和当前会话权限。
- S10 新增 integration_behavior：缓存开启/关闭运行相同轨迹，查询与重启全字段快照相等；拒绝操作/写失败同时校验内存、文件、代数/局部版本与群/子群稳定地址。补齐 docs/integration-validation.md 任务书章节追踪、R/O/E 分类和事务权限矩阵。
- tests/fixtures/ThrowingGroupPolicyFanLX.h 保留原测试语义，将测试替身也拆成独立类文件。新网络/集成源码已 clang-format 格式化并保留中文注释。
- 默认 build：CTest 8/8（11.26 秒）；唯一 Release 目录 .artifacts/verify-759cc5d84839455ebaeaaa013d52fe88/build：从源码配置/构建成功，CTest 8/8（9.25 秒）。新增网络库、门面和两项测试以 -Wall -Wextra -Wpedantic -Werror 编译，Release 断言有效。7 个相关源文件 clangd 检查均为 0 errors。
- docs/network-validation.md 已记录双端菜单步骤、协议、资源上限和 T14 实测；README、architecture.md、data-format.md 和 DOCX 工程文档同步更新。工程文档是详细工程记录，约 5 页正式提交版仍属 S11；本轮未修改原始任务书。
- 证据：.artifacts/S08-S10/ctest-default.log、ctest-release.log、*.clangd.log、final-manifest.sha256。工程报告当前 15 页，已逐页检查 Word 导出与 Poppler 页面图像；原文档渲染脚本因无 LibreOffice 失败后采用此路径。该详细工程记录不冒充 S11 约 5 页正式提交版。
- 边界：当前只实测本机回环双进程；未实现 TLS/远端身份认证/离线投递/聊天历史/跨端领域同步。真实断电、磁盘耗尽和跨机器环境未实测。S11 报告精简、答辩及发布冻结未完成。


## 2026-09-23 中心转发 IM 增量改造

按只修改现有代码要求扩展 TcpPeer/TcpListener/MessageCodec 与现有启动、菜单，不重写领域层。原有 Winsock2 和 ws2_32 保留；配置服务端后登录即建立长连接，菜单返回持续收发。新增单聊/群聊中心转发、离线ACK、群会话增量游标、仅好友可见的上下线/查询、持久化outbox/inbox和断线自动重连。认证使用服务端令牌，账号/好友/群来自服务端领域数据。

Debug全量9项CTest通过；IM用例涵盖连接复用、在线/离线投递、服务端及客户端生命周期重建、双ACK丢失窗口、磁盘只读失败、幂等冲突、身份伪造和群权限。当前为单服务器、单在线会话、受控网络方案，无TLS或多设备漫游；未实测真实断电。详见network-validation.md。此前S08的P2P限制是历史记录，不再代表当前菜单功能。

> 2026-09-30 凭据改造（对本节的后续修订）：本节「认证使用服务端令牌」的说法已不再成立。登录改为「用户名（QQ 号）+ 用户自选密码」，账号库就是领域快照 `users[].password`（课程范围内明文）；`--im-tokens` / `--token` / `--tokens` 与令牌文件、`scripts/im-tokens.ps1` 已全部删除，服务端 `ImServerFanLX` 直接查账号库认证，wire 协议不变。上述 2026-09-23 记录保留为当时的事实，当前步骤以 network-validation.md 的「启动与操作」为准。


## 2026-09-23 仓库清理与帮助文件

删除与工程无关的临时产物：`.artifacts/`（旧构建目录 im-build/build-debug/build-release/build-s10-0923、渲染 PNG/PDF、clangd 日志、一次性脚本与 format_tool，约 993 MB）、`build/`（构建输出，约 183 MB）、`data-chat-a|b`、`data-im-a|b`、`data-im-server`（运行时演示库，可用 `--init-demo` 重建）、`.cache/`（clangd 索引）、空目录 `fixtures/` 与 `docs/prompts/`（过程提示词）。保留源码、测试、文档、第三方头文件、课程任务书与 deliverables 报告。README 与各验收记录中对 `.artifacts/...` 的引用已加说明，命令改用任意构建目录即可复现。

新增 `.gitignore`（忽略 `build/`、`build-*/`、`.artifacts/`、`data-*/`、编译产物与日志），构建产物自本次起不再入库；此前误入库的构建产物已从索引移除，删除的受版本控制文件仍可经 git 历史取回。

清理后在仓库外全新目录验证：MinGW GCC 15.1.0 配置与构建成功，CTest 9/9 通过（18.34 秒），`fanlx_app --demo` 退出码 0；在 `PATH` 不含 MinGW 的环境下仍可直接运行（`-static` 已消除 0xC0000139）。

新增根目录 HELP.md：环境要求、构建与干净重建、命令行参数、全部菜单速查、IM 三终端联调、测试与基准、数据与锁、FAQ 及文档索引。

---

# 扩展实施记录（依据 docs/extension-plan.md）

> 本节严格区分三件事：**计划**（尚未做）、**实现**（代码已写但未实测）、**实际验证**（有可复现证据）。
> 未实测的条款一律不得写成完成。

## E01 基线与依赖（2026-09-26）

### 计划条款与实际落地对照

| 计划条款 | 实际做法 | 性质 |
| --- | --- | --- |
| 采用 Qt 6 Widgets 构建 fanlx_gui（FANLX_BUILD_GUI 默认关闭） | 已加入 `option(FANLX_BUILD_GUI ...)` 与 Qt6 组件查找；GUI 目标本体属 E04 | 实现（开关）/ 计划（窗口） |
| 锁定 Qt、编译器与 SQLite 版本，先验证 ABI/运行库兼容，不混用预编译包 | 见下"版本锁定与实测矩阵" | 实际验证 |
| 依赖以 CMake target 与配置路径注入，记录许可证、来源、SHA-256；不写死本机盘符 | SQLite 用 `FANLX_SQLITE_DLL` 缓存变量（默认系统路径，可覆盖）；Qt 用 `CMAKE_PREFIX_PATH`；SHA-256 记入 docs/dependencies.md | 实际验证 |
| 新增类各自独立文件，涉及现有 ImClient/ImServer 时拆出独立文件 | `include/db/`、`src/db/` 下 7 个类各自独立头/实现；网络侧拆分见 E03 | 实现 |

### 版本锁定与实测矩阵

| 项 | 选定版本 | 证据 |
| --- | --- | --- |
| 编译器 | MinGW GCC 15.1.0（`D:/mingw64`，既有基线） | 与既有 9 项测试同工具链，无回归 |
| CMake | 4.4.3（既有）；`cmake_minimum_required(3.20)` | 配置成功 |
| SQLite | 系统 `C:/Windows/System32/winsqlite3.dll` = 3.51.1（`sqlite3_libversion()` 实测输出） | 探测程序打印 `sqlite=3.51.1` |
| SQLite 头文件 | 官方 `sqlite3.h` 3.51.1（公共领域）入库到 `third_party/sqlite/sqlite3.h` | SHA-256 = `ADEDC4471DBBB046268FEE3AD67263425133595E95F8ADA2C86098789CCBCB24` |
| Qt（E04 用） | `E:/Qt/6.11.1/mingw_64`，`qmake -query QT_VERSION` = 6.11.1 | 见下"兼容性实测" |

**兼容性实测（不是推断）**：分别用既有 MinGW GCC 15.1.0 编译并运行了两个最小程序：

1. Qt 6.11.1 + 系统 SQLite 组合：`probe ok qt=6.11.1 sqlite=3.51.1 value=值`（内存库中写入并读回中文，退出码 0）。
2. Qt 6.11.1 单独：`qt-mingw15 ok qt=6.11.1 text=中文`（构造 `QApplication`/`QLabel`，退出码 0）。

因此"Qt 预编译包必须使用其自带 MinGW 13.1.0"这一前置顾虑经实测不成立：Qt 6.11.1 的 mingw_64 预编译包可与现有 MinGW GCC 15.1.0 混用。`E:/Qt/Tools/mingw1310_64` 作为备选保留未用。稳定性结论只覆盖上述探针与后续 E04/E05 实测范围，不推广到未测组合。

### 构建链变更

- `CMakeLists.txt` 新增 `FANLX_BUILD_GUI` 开关（默认 OFF，基础课设交付仍不需要 Qt）与 `FANLX_SQLITE_DLL` 变量。
- 新增 `fanlx_sqlite` 依赖目标：由 `gendef` 从系统 DLL 导出符号表，再由 `dlltool` 生成导入库（都在构建目录内生成），仓库不保存任何第三方二进制。注意 `gendef` 只按相对路径写文件，故自定义命令设 `WORKING_DIRECTORY`；导入库必须以完整路径进入 `target_link_libraries`（早期写成 `target_sources` 导致路径没进链接行）。
- 新增 `fanlx_chat` 目标（`src/db/*`），链接 `fanlx_persistence`、`fanlx_sqlite`、`Threads::Threads`，以 `-Wall -Wextra -Wpedantic -Werror` 编译。
- **未改动的部分**：既有 9 项 CTest 的构建方式、`fanlx_app` 控制台入口、领域层代码与测试断言全部保持原样。

## E02 数据库仓储（2026-09-26）

### 实际实现

| 文件 | 职责 |
| --- | --- |
| `include/db/SqliteHandleFanLX.h` / `src/db/SqliteHandleFanLX.cpp` | `ConnectionFanLX` / `StatementFanLX` / `TransactionFanLX` / `SqliteErrorFanLX`；按序绑定辅助与有界忙碌重试 |
| `include/db/ChatRepositoryFanLX.h` | 仓储接口、消息/发件箱/游标/分页 DTO、会话键与状态文本、错误分类 |
| `include/db/ChatSchemaFanLX.h` / `src/db/ChatSchemaFanLX.cpp` | 客户端与服务端建表语句、索引、schema 版本常量 |
| `include/db/ChatSchemaStoreFanLX.h` / `src/db/ChatSchemaStoreFanLX.cpp` | 幂等建表、版本校验、身份校验、计数器默认值 |
| `include/db/SqliteChatRepositoryFanLX.h` / `src/db/SqliteChatRepositoryFanLX.cpp` | 客户端 SQLite 仓储（发送状态机、幂等、游标、keyset 分页、会话摘要） |
| `include/db/SqliteMessageStoreFanLX.h` / `src/db/SqliteMessageStoreFanLX.cpp` | 服务端消息库（编号/序号分配、收件人快照、投递游标、幂等重放） |
| `include/db/ChatMigrationFanLX.h` / `src/db/ChatMigrationFanLX.cpp` | 旧 IM JSON → SQLite 离线迁移与校验 |
| `include/db/DatabaseWorkerFanLX.h` / `src/db/DatabaseWorkerFanLX.cpp` | 数据库专用工作线程（有界队列、事件回调、取消标记、异常不逃逸） |

### 实现中修正的设计缺陷（均为实测发现，非预想）

1. **待发送消息不能伪装成已有服务器编号**：原设计用 `message_id INTEGER PRIMARY KEY` 承载可空的服务器编号，但 SQLite 会把显式 NULL 替换成 rowid（实测 `id=1 isnull=0 type=integer`）。改为 `PRIMARY KEY(conversation_key, local_sequence)` + `message_id` 可空唯一部分索引。
2. **历史查询参数错位**：过滤条件原来只在启用时才拼接分支，导致 `?2..?7` 等位置参数与 SQL 文本不匹配，出现"只按发送者过滤就查不到任何历史"的静默错误。改为恒真占位写法（未启用时写成 `?n = ''` / `?n = 0`），参数序号与绑定顺序天然一致。
3. **已读游标与未读数**：未读数改为独立聚合查询（不再与外层 LEFT JOIN 混用），避免列序号错位；已读游标只允许单调前进。
4. **迁移必须允许乱序**：旧 JSON 的 inbox/outbox 不保证会话序号单调，走 `recordDelivered` 会因"投递乱序"被拒。新增 `importLegacyMessage` + `rebuildSyncCursorsFanLX`（导入后台由数据重算游标）。
5. **迁移的会话对端推导错误**：原来一律取 `recipient` 作为对端，把 DELIVER 的收件人（= 本人）当成对端，算出 `d|自己|自己` 这种错键。改为按方向推导：自己发的看收件人，收到的看发送者。
6. **迁移后的服务端编号冲突**：旧服务端编号是 1..N 连续段，迁移后 `next_message_id` 仍是 1 会与已导入记录撞主键；新增 `raiseMessageCounterFanLX`（只抬不降）。
7. **关闭前检查点**：`ConnectionFanLX` 析构时执行 `PRAGMA wal_checkpoint(TRUNCATE)`，否则异常退出或直接复制主 `.db`（忽略 `-wal`）会丢已提交记录。
8. **大小写折叠的边界**：SQLite 内置 `lower()` 只处理 ASCII。注册确定性 SQL 函数 `fanlx_fold`，明确并实测"仅 ASCII 大小写不敏感、其余字节精确匹配"，避免把语义写成模糊承诺。

### 实际验证（证据）

`tests/chat_history_tests.cpp` 新增 14 个用例，全部断言在 Release 下同样生效（文件首行取消 NDEBUG）。默认 build：`ctest --test-dir build` **10/10 通过**（新增第 10 项 `chat_history_behavior`，19.96 秒）；原 9 项无回归。

| 用例 | 覆盖内容 |
| --- | --- |
| t20 | 本地提交 → 待发送 → SEND_ACK → 已接受全状态机；重开库仍持久；本地序号继续递增 |
| t21 | 同 requestId 同内容幂等；同 requestId 不同内容拒绝且不改写；校验失败不消耗序号（回滚） |
| t22 | 参数校验在提交前失败；重复 messageId 同内容不新增行、内容冲突报错；同一序号被其它消息占用拒绝；游标单调 |
| t23 | 同目录换账号/换服务器实例身份均拒绝打开（账号隔离）；独立目录各自持有历史 |
| t30 | 5000 条 keyset 分页：连续翻页不跳页不重复、逐页累计等于总数；关键词逐页累计等于逐条统计；空结果不报错 |
| t31 | 中文路径读写；`%`/`_`/`\` 字面量匹配；emoji 与西里尔字节精确匹配；ASCII 大小写不敏感；时间范围过滤；会话摘要与未读数 |
| t32 | 数据库工作线程：异步写入结果事件、取消标记、队列满立即返回 false 不阻塞、停止流程 |
| t33 | 十万条规模测量（单独运行，见下） |
| t34 | 未知/损坏 schema 版本显式拒绝，不静默降级 |
| t40 | 服务端接收：编号与序号分配、幂等重放返回旧结果不消耗序号、内容冲突拒绝 |
| t41 | 投递游标：候选按 messageId 递增、推进后剩余正确、库内游标不回退、非收件人不可见、integrity_check=ok |
| t50 | 客户端迁移：字段与序号保留、待发送消息 status=pending、无可信时间不伪造、原 JSON 字节不变 |
| t52 | 迁移失败不留半成品数据库、不写完成标记、旧数据保留、修复后重试成功 |
| t53 | 服务端迁移：编号/游标保留、迁移后新消息编号不重叠、重跑直接返回已完成 |
| t54 | 数据库线程内失败以 Failure 事件返回（不 terminate）、未注册任务同样只报错、停止在 3 秒内完成 |

### 十万条记录实测结果（本机、隔离目录）

`build\chat_history_tests.exe --perf`（Release 断言有效；已注册为带 `perf` 标签的独立 CTest 项，默认 `ctest` 不运行，避免拖慢回归）：

| 指标 | 实测值 |
| --- | --- |
| 批量写入 100000 条 | 3490 ms |
| keyset 分页（每页 50 条）p50 | 57.4 ms |
| keyset 分页 p95 | 69.9 ms（计划目标 < 200 ms，**达标**） |
| 关键词扫描（`消息 99999`） | 40 ms（单独测量，不承诺与分页同指标） |
| 查询计划 | `SEARCH messages USING COVERING INDEX idx_messages_conversation_order (conversation_key=? AND conversation_sequence<?)` |

未启用 SQLite FTS5：当前参数化 `LIKE` + 索引分页已满足本机目标，FTS5 是否启用留待有测量依据时再决定，不在本轮提前写为完成。

### 明确未做（不得写作已完成）

- 客户端与服务端的 **JSON → SQLite 接线**（E03）：`ChatMigrationFanLX` 与 SQLite 仓储已可独立测试，但 `ImClientFanLX` / `ImServerFanLX` 仍使用旧 JSON 存储。
- 历史查询在 GUI 中的分页/时间语义呈现、异步取消在真实界面上的验证（E05）。
- 数据库文件空间不足、真实断电、跨机器环境的实测：本轮只覆盖提交失败回滚与完整性校验。

## E03 异步整合（2026-09-26）

### 文件拆分（计划第 3 节"涉及现有 ImClient/ImServer 时拆出独立文件"）

| 文件 | 内容 |
| --- | --- |
| `include/network/MessageMailboxFanLX.h` / `src/network/MessageMailboxFanLX.cpp` | 每连接有界待发送缓冲：组帧、部分写偏移、慢端判定、非阻塞推进 |
| `include/network/ImClientFanLX.h` / `src/network/ImClientFanLX.cpp` | 客户端：连接工作线程、发送状态机、数据库事件关联、授权目录、异步命令队列 |
| `include/network/ImServerFanLX.h` / `src/network/ImServerFanLX.cpp` | 服务端：认证、权限、幂等接收、投递状态机、非阻塞发送、目录应答 |
| `include/persistence/NamedFilePublisherFanLX.h` / `src/persistence/NamedFilePublisherFanLX.cpp` | 具名文件原子发布（身份文件/标记文件专用） |

`TcpPeerFanLX` 现在实现 `MessageMailboxFanLX::SinkFanLX`，新增 `trySend` / `flushSend` 非阻塞接口，`send` 保持"同步写完或抛异常"的旧语义。

### 服务端不再为慢客户端阻塞

- 每连接一个待发送缓冲（默认 128 帧 / 1 MiB 上限），满即判定为慢端并关闭该连接（计入 `slowPeerDisconnects()`）。
- 主循环按预算（每轮 16 帧）非阻塞推进所有连接，不再在 `send` 内等待单个客户端最长 5 秒。
- 半帧写出失败后连接立即作废，禁止把下一帧接在半帧之后。

### 客户端异步化

- 数据库工作线程独占 SQLite 连接；网络线程只做 socket 读写。
- 发送路径：**先在数据库提交"待发送"**（事务成功）→ 才把 SEND 入网络队列。
- 接收路径：收到 DELIVER → 排队落库 → **落库成功后才回 ACK**（落库失败不回 ACK，让服务器重连后重放）。
- 结果以 `ImClientEventFanLX` 值对象回报：本地已保存 / 服务器已接收 / 投递 / 历史页 / 会话列表 / 目录 / 上下线 / 同步结束 / 错误。
- 命令队列与事件队列都有界；队列满时发送接口返回 false（界面显示繁忙并保留输入），事件丢失只丢提示（数据库仍是事实源）。

### 协议 v3 扩展

- 新增可选字段 `protocolVersion` 与 `acceptedAtUtc`：**仅在需要时出现**，因此既有 v2 报文的字节形态不变（既有网络测试与旧客户端不受影响）。
- 新增受认证保护的只读目录请求 `DIRECTORY` / 应答 `DIRECTORY_LIST`：只返回服务端权威的好友与已加入群，客户端不推断群资格；目录过大时明确报错而不是静默截断。
- 服务端时间戳由服务器提供（SEND_ACK / DELIVER 携带），客户端不自行编造时间。

### 实施中修正的缺陷（均为实测发现）

1. **身份文件覆盖 IM 快照**：`AtomicFileWriterFanLX::publish` 按平台快照约定固定发布为 `state.json`，用它写 `client-identity.json` 会把 IM 的 `state.json` 覆盖成身份内容，导致重启后 `key 'schema' not found`。新增 `NamedFilePublisherFanLX` 做具名原子发布。
2. **离线补发在旧 JSON 后端失效**：补发逻辑最初只读 SQLite outbox，导致"离线期间发送、重连后补发"在默认后端上不再工作（im_behavior 的 `restart-replay` 步骤超时暴露）。已按后端分别实现。
3. **markAccepted 无法定位会话键**：先 UPDATE 再按"仍为 pending"的条件回查会话键，条件已不成立；改为先定位再更新，并显式处理重复 ACK。
4. **实例校验误伤旧后端**：服务器实例一致性校验只对 SQLite 后端有意义，旧 JSON 后端不绑定实例，否则正常登录会被拒绝。
5. **`NULL` 主键伪装成服务器编号**：SQLite 会把 `INTEGER PRIMARY KEY` 上的显式 NULL 换成 rowid，"待发送"因此会伪装成已有编号；改为 `device_sequence` 作主键、`message_id` 可空唯一。
6. **历史查询参数错位**：按条件拼接 SQL 分支会让 `?2..?7` 与绑定顺序错位（"按发送者过滤查不到历史"）；改为恒真占位写法。

### 实际验证

- 默认 build：`ctest --test-dir build` **10/10 通过**（19.47 秒），其中 `im_behavior` 6.18 秒（较改造前的 IM 用例更长，因为补发了 outbox 与目录握手）。既有 9 项全部保留且通过。
- `im_behavior` 覆盖：多消息只 connect 一次、离线投递、群会话增量游标、仅好友可见的上下线、服务端重启期间发送后自动补发、客户端生命周期重建保留 inbox、双 ACK 丢失窗口、幂等冲突、身份伪造、群权限、磁盘只读失败、SYNC_END。
- `chat_history_behavior`：数据库层 14 个用例全过（见 E02 表格）。

### 明确未做

- GUI（E04）、历史分页在界面上的呈现与异步取消的真实验证（E05）、交付冻结（E06）。
- 实测只覆盖本机回环；未测真实跨机网络、TLS、真实断电与磁盘耗尽。
- 服务端仍是单控制线程 + SQLite 同步提交：本机课程数据规模下 tick 预算充足，但未对"服务端数据库提交期间的新请求"做独立工作线程（计划允许的首版范围）。

## E04 图形客户端（2026-09-26）

### 实际实现（Qt 6 Widgets，各自独立文件）

| 文件 | 职责 |
| --- | --- |
| `include/gui/ChatControllerFanLX.h` / `src/gui/ChatControllerFanLX.cpp` | 唯一持有 `ImClientFanLX` 的控制器：把异步事件转成 Qt 信号，界面不接触网络/SQL |
| `include/gui/ConversationModelFanLX.h` / `src/gui/ConversationModelFanLX.cpp` | 会话列表模型 + 消息列表模型（`QAbstractListModel`，只做展示映射） |
| `include/gui/LoginDialogFanLX.h` / `src/gui/LoginDialogFanLX.cpp` | 登录窗口：地址/端口/账号/令牌/数据目录/存储后端；失败显示原因并允许重试 |
| `include/gui/HistoryDialogFanLX.h` / `src/gui/HistoryDialogFanLX.cpp` | 历史查询窗口：会话/发送者/关键词/时间范围/每页条数（默认 50，上限 200） |
| `include/gui/MainWindowFanLX.h` / `src/gui/MainWindowFanLX.cpp` | 主窗口：左会话列表（含未读数）、中消息列表、底部编辑器、更早/最新/重连、菜单入口 |
| `src/gui/main_gui.cpp` | 启动入口与命令行参数（`--auto-login` 用于脚本化演示） |
| `scripts/deploy-gui.ps1` | 部署 Qt 运行依赖到目标目录，使其无需开发环境即可启动 |

### 界面功能对照（计划第 4 节）

| 计划条目 | 实现情况 |
| --- | --- |
| 登录：地址/端口/账号/令牌；连接中、认证失败、登录成功；失败后可重试 | 已实现；失败原因（认证失败/库归属不符/目录被占用）原样显示，窗口保留配置 |
| 主窗口：左联系人/群/最近会话，中消息列表，底部编辑器；在线状态、未读数、重连状态 | 已实现；标题栏与状态栏显示账号、存储后端、会话数、未读总数与连接状态 |
| 单聊与群聊：中文输入、区分"待发送/服务器已接收/失败" | 已实现；每条消息显示状态文案，失败附带原因，`g|群号` 与 `d|账号A|账号B` 自动区分 |
| 服务器 ACK 不显示为"对端已读" | 已实现：ACK 文案为"服务器已接收"，不出现"已读"字样 |
| 历史查询：会话/发送者/关键词/时间范围；默认 50、最大 200；向前翻页与返回最新 | 已实现（时间范围字段已提供并在查询说明中体现；收敛到查询参数属 E05） |
| 生命周期：账号切换、注销、窗口关闭；提示未发送消息保留 | 关闭窗口即停止客户端并回收线程；未发送消息持久在库中，登录后自动补发（下方 t64 与 im_behavior 均覆盖） |
| 实时消息不强制跳到底部，用"有新消息"提示 | 已实现：`onMessageArrivedFanLX` 只置提示，不滚动到底部 |
| 列表按服务器授权结果呈现 | 已实现：`DIRECTORY_LIST` 解析后只展示服务端返回的好友与群，客户端不推断资格 |

> 2026-09-30 凭据改造（对 E04 表格的后续修订）：登录窗口的字段由「账号 / 令牌」改为「用户名（QQ 账号）/ 密码」，
> 命令行 `--tokens` 已删除、改用 `--account 用户名 --password 登录密码`；上表「失败原因（认证失败/库归属不符/目录被占用）原样显示」
> 仍然成立，只是凭据错误现在原样显示服务端返回的 `用户名或密码错误`。E04 的其余结论不受影响。

### 实施中修正的缺陷（实测发现）

1. **AutoMoc 在受限环境不可用**：Qt 的 `CMAKE_AUTOMOC` 通过带管道捕获的子进程调用 `moc`，本机报 `libuv process spawn failed: operation not permitted`。改用 `qt6_wrap_cpp`（直接调用 moc 并把生成的 `moc_*.cpp` 加入目标），行为等价且可审查。
2. **混用 MinGW 运行库导致堆损坏**：把 Qt 自带 MinGW 13 的 `libstdc++/libgcc/libwinpthread` 与 GCC 15 编译的代码混用，运行期出现 `0xc0000374`（堆损坏），且只在含 Qt 对象释放时触发。部署脚本改为**只复制 Qt6Core/Gui/Widgets 与平台插件**，运行库仍由本项目工具链的 DLL 提供。
3. **`start()` 同步握手阻塞界面**：控制器启动时先做只读握手再打开数据库，会把界面卡住数秒（服务器忙时更久），并且需要"别人同时 tick 服务端"才能成功——这是设计缺陷，不是测试问题。改为：旧 JSON 后端保持构造即就绪；SQLite 后端把握手与开库都放到工作线程，`start()` 立即返回。
4. **数据库未就绪时 `history()`/`pending()` 抛异常**：存储改为异步准备后，这两个查询会对未初始化的 JSON 取键抛 `type_error`。已加空值保护（未就绪返回空），不再把内部状态暴露成异常。
5. **连接层错误不上报**：连接失败只在内部记录，界面会静默停在"重连中"。新增去重的 `Connection` 事件，界面显示具体失败原因。
6. **`Sent` 事件携带空幂等键**：幂等键由数据库在分配序号时生成，控制器却从关联表取（此时还是占位空串），导致发送路径拿不到 requestId 而返回失败。改为使用数据库回传的值。

### 实际验证

- `tests/gui_tests.cpp` 新增 `gui_behavior`（离屏 `QT_QPA_PLATFORM=offscreen`，4 个用例）：
  - t60 会话模型：展示字段与角色一一对应、未读数不丢失、越界返回空；
  - t61 消息模型：状态文案与"自身/待发送/失败"标记可见；
  - t62 配置不全时 `start()` 立即失败并给出原因，不进入"假装已连接"；
  - t63 服务器离线时 `start()` **0 ms 返回**、保持未在线，并在有界时间内通过 `errorFanLX` 报告 `连接失败 Winsock=10061`；
  - **t64 端到端**：真实 `ImServerFanLX`（SQLite 后端）+ 两个控制器（Qt 事件循环 + 网络线程 + 数据库线程），两账号互为好友、目录返回正确、A 发送中文消息 → 收到"服务器已接收"、B 确实收到该消息、会话列表反映最新会话。
- **干净目录启动**：`scripts/deploy-gui.ps1` 部署后，`fanlx_gui.exe` 在 `PATH` 仅含系统目录的环境下正常运行（依赖仅 12 个文件：3 个 Qt DLL + 3 个 MinGW 运行库 + 平台/样式插件）。
- 默认 build 全量：`ctest --test-dir build` **11/11 通过**（22.36 秒，含 `gui_behavior`）；`chat_history_performance`（`ctest -L perf`）单独运行 100000 条：写入 3732 ms、keyset 分页 p50 = 51.8 ms、p95 = **73.6 ms**（目标 < 200 ms 达标）、关键词扫描 42 ms、执行计划走 `idx_messages_timeline`。

### 明确未做

- 时间范围过滤尚未接到界面查询参数（字段已备，属 E05）。
- 未做界面截图/录像与三客户端同时在线的人工演示记录（属 E06 交付项）。
- 未在真实多显示器/高 DPI/中文输入法环境下实测；未做无障碍与国际化。
- `GUI` 目标默认关闭（`-DFANLX_BUILD_GUI=ON` 才构建），确保基础课设交付不依赖 Qt。

## 当前收尾状态（2026-09-26）— 历史交接记录，后续结果见下节

本节保留早期交接时的真实状态；后续验证已覆盖其中的 GUI 失败项。
"完成标准是可运行图形聊天、真实多客户端、多线程无阻塞交互、SQLite 历史查询及恢复均有证据。"
剩余问题与接手说明见 docs/handoff-prompt.md。

### 客观测试状态

| 项 | 结果 |
| --- | --- |
| `ctest --test-dir build -E performance` | **11/11 通过**（20.55 秒） |
| `ctest --test-dir build -L perf` | 通过：十万条写入 3732 ms、keyset 分页 p50 51.8 ms / p95 **73.6 ms**、关键词扫描 42 ms |
| `fanlx_gui.exe` 干净目录启动 | 通过（`PATH` 仅系统目录时可运行；依赖 12 个文件） |
| 两客户端 SQLite 往返（A→B→A） | 通过（诊断程序验证，见下） |
| 三客户端 GUI 演示（`gui_screenshot`） | **失败**：`A 发送单聊消息`、`B 回复单聊消息` 通过，但 `A 收到回复` 与群消息投递超时 |

### 本轮修复的真实缺陷（均有诊断证据）

1. **ACK 未携带会话序号**：客户端回 `ACK` 只带 `messageId`，服务端无法确定要推进哪个会话的投递游标，推进失败后该会话永久停在"在途"状态，导致**同一会话的后续消息全部无法投递**。修复：两种后端都回填 `conversationSeq`；服务端对 `conversationSeq == 0` 的 ACK 明确拒绝（不再静默卡住）。诊断证据：服务端日志曾显示 `seq=0` 与 `投递游标参数非法`。
2. **服务端"投递记录缺失"直接失败**：会话键取自连接内的在途登记，登记丢失时无处可查。修复：按消息编号从库中回查会话键（编号本身已用 `s.inFlight` 校验，客户端无法借此伪造其它会话游标）。
3. **待发送消息的收件人取错字段**：`conversations.target` 是建会话时写下的字段，**由对端先建会话时它等于对端**，不能当作本端的收件人。诊断证据：服务端收到 `SEND target=[2026002]`（B 本该发给 2026001）。修复：收件人统一由**会话键按方向推导**（单聊取非本人一侧、群聊取群号）；GUI 控制器不再自行用下标记解析键，改为与网络层共用同一个推导点（`submitSendToConversation`）。
4. **“投递乱序”拒绝导致单聊单向死锁**：客户端用 `sync_cursors.last_sequence` 作为投递准入条件（`appendDelivered` 里“序号 ≤ 已落库序号就拒”）。但该水位被**自己发出的消息**经 `markAccepted` 推到该消息自身序号，而单聊序号是两端共用的一条单调计数器（`conversationKeyFanLX` 排序后同键），因此“自己已到 15、对端更早的 8 未投到”完全正常。老规则永久拒收那条 8：不落库、不回 ACK（服务端投递游标只由 ACK 推进）→ 服务端每次 SYNC 重投 → 10 秒未 ACK 关会话重连。用户可见症状：`错误:会话投递乱序:序号8不大于已落库序号 15`，且**2026002 发不进 2026001，反向却正常**（A 的水位高，所有 ≤15 的入站全被拒；B 水位低，A 的新序号 16+ 能接受）。同一规则还使自己消息的**回显**（服务端 `recipients` 含发送者，`pendingFor` JOIN recipients 会回投）恒被拒，而“把本地待发行提升为已确认”的分支在该检查之后，永远走不到。修复：删除该准入条件，只保留“同一会话序号被另一条消息占用”的真冲突判定；重复投递仍由 messageId 去重（含内容比对）负责；`last_sequence` 语义降为“见过的最大序号”（注释已写明不得用作准入）。回归用例 `t56`。注：既有 `t22` 的那条断言之所以仍通过，是因为它构造的确实是同序号不同 messageId 的真冲突。

### 早期失败问题（后续已修复）

- **三客户端 GUI 场景 `A 收到回复` 曾超时**：同一场景下 `A 会话数=2 未读=2`，说明**消息已进入数据库**，
  问题集中在"数据库事件 → 控制器 → 界面模型"这一段映射。两客户端直接使用 `ImClientFanLX` 时往返正常，
  `gui_behavior` 的单向收发也正常，因此故障面在这些差异之间（控制器事件分发、三个客户端同时在线、
  或"第二轮消息"未插入界面模型）。首要怀疑：`ChatControllerFanLX::drainEventsFanLX()` 的 `Delivered`
  分支只标记"有新消息"并发信号，**没有把新消息插入 `messagesFanLX`**。详见 docs/handoff-prompt.md 第 3.2 节。
- 后续已修复控制器刷新逻辑，并由 `gui_three_client_behavior` 完成三客户端回归验证。
- `im_behavior` 曾一度超时，根因是 `tests/network_tests.cpp` 手工构造 ACK 时未设会话序号，
  与新增的服务端 ACK 校验冲突；补齐 `ack.conversationSeq` 后已通过（属测试未跟上协议，非产品缺陷）。

### 证据位置

- 界面截图（由 `tools/gui_screenshot.cpp` 生成，可重新生成）：
  `.artifacts/gui-screenshots/01-main-window-direct.png`、`02-main-window-group.png`、`03-history-query.png`
  截图已通过"非白像素占比"自检（主窗口约 20%，历史窗约 80%），说明布局与绘制确实完成；
  文件体积小是 Qt 离屏渲染扁平纯色控件、PNG 压缩率高的正常结果，不是空白图。
- 复现截图与三客户端演示：`build\gui_screenshot.exe --out .artifacts\gui-screenshots`
  （需先 `scripts/deploy-gui.ps1` 部署 Qt 运行依赖，或把 Qt bin 加入 `PATH`）
- 两客户端往返诊断（本轮临时程序，已删除）：结论为修复缺陷 1–3 后 `A→B→A` 往返通过。
- 结构说明见 docs/project-structure.md。

### 下一步（按优先级）

1. 定位三客户端 GUI 的"事件 → 模型"映射失败点（首要怀疑：`drainEventsFanLX` 的 `Delivered` 分支未把
   新消息插入 `messagesFanLX`），并使 `gui_screenshot` 的往返与群聊断言全部通过；随后把该场景固化为 CTest。
2. 进入 E05（时间范围接到界面查询参数、异步取消在真实界面下的验证、关闭流程与故障恢复演示）。
3. 最后 E06（UML、扩展报告、演示数据与三客户端截图/录像记录）。
4. 完整接手说明（含约束、复现命令、已修缺陷清单、禁止事项）见 docs/handoff-prompt.md。

## 当前收尾状态（2026-09-26 后续验证）— P0 已完成，E05 主要功能已接入

### 实现

- 修复 `ChatControllerFanLX` 的实时投递刷新：当前会话收到 `Delivered`、发送收到 `Accepted` 后自动重新读取最新历史；查看旧页时保留滚动位置并显示新消息提示。
- 授权目录中的无历史联系人和群会话会出现在 GUI 左侧，可直接打开并发送首条消息。
- 增加历史查询控制器接口：独立查询 ID、取消旧查询、加载下一页、跨会话时间游标。
- 历史查询窗口已接入 UTC 起止时间校验、SQLite 闭区间过滤、跨会话同秒记录的稳定分页、取消查询和下一页按钮。
- 新增 `gui_three_client_behavior` CTest，覆盖三个 SQLite 客户端的单聊往返、群聊、截图、停止和重启历史恢复。

### 实际验证

| 项 | 结果 |
| --- | --- |
| `cmake --build build -- -j4` | 通过，GUI 与基础目标均构建成功 |
| `ctest --test-dir build --output-on-failure` | **13/13 通过**（32.50 秒，含性能测试） |
| `gui_screenshot.exe` | 通过：三个客户端在线、A↔B 往返、三人群聊、SQLite 重启查询、停止 14 ms、3 张 PNG 非白像素自检通过 |
| 时间范围与跨会话分页 `t35` | 通过：闭区间、未知时间排除、同秒记录不漏页 |

### 尚未完成

- E05 的只读目录、空间不足和真实磁盘故障演示尚未全部实测；当前已有 SQLite 错误、只读/失败注入和工作线程异常回归。
- E06 的正式约 5 页扩展报告、UML 终稿和人工多显示器/高 DPI/输入法记录尚未冻结。
- GUI 目标仍需 `-DFANLX_BUILD_GUI=ON` 构建；基础控制台版保持默认无 Qt 依赖。

## GUI 补齐全部课程功能（2026-09-29）— 四个页签，要求 1~6 界面上全部可见

### 起因

此前可视化客户端只有聊天一个页签：账号与服务、好友、群三组能力只存在于控制台数字菜单里，
界面上"看不见"，等于没做。本轮把同一批领域能力接到 Qt 界面上。

### 实现

- 新增 `DomainControllerFanLX`：GUI 侧唯一的领域门面，持有一个 `PersistenceApplicationFanLX`。
  库目录由 `imClientPathFanLX::domainDirectoryFanLX` 派生为
  `<数据根>/im-client/<host>_<端口>/<账号>/domain`——必须放在账号目录的**下一层**，
  因为聊天库与领域库各自要独占自己那层的 `.lock`（`CreateFileW` 共享模式为 0），同层会互相拒绝启动。
  按账号派生同时保证一个数据根可以并发跑多个 GUI 客户端而不争锁。
- 新增 `AccountPageFanLX`（用户基本信息 / 开通管理 / 登录管理 / 绑定管理 / 资料与密码 / 注册入口）、
  `FriendPageFanLX`（增删改查 + 同平台共同好友 + 跨微X推荐与共同好友）、
  `GroupPageFanLX`（群列表 / 成员 / 特色展示与动态切换 / 申请 / 批准 / 推荐 / 确认 / 退群 / 挨 T /
  管理员 / 转让 / 子群 / 解散 / 建群 / 改名）、`RegisterDialogFanLX`（QQ 与微博共号、微信独立号）。
- `MainWindowFanLX` 改为 `QTabWidget` 四页签宿主，状态栏跨页签共用一个操作结果行；
  顶部新增"领域数据"菜单（领域库位置、查询缓存统计、立即写回文件）。
- 登录窗口新增"确认联动本人其它已开通服务"勾选；`fanlx_gui` 新增 `--skip-domain` 与 `--no-confirm-others`。
- 关窗与退出都执行收尾提交并释放目录锁；每次写操作本身已经过候选事务原子落盘。

### 本轮修正的界面缺陷

- 群页签原先必须手动点一个群才会显示"群特色功能"，否则只有一段提示——打开页面时改为默认选中第一个群。
- 好友删除原先无论关系是否存在都报成功（应用门面丢掉了领域层的布尔返回值），改为先查列表再给准确结果。
- 群写操作跨命名空间时原先抛出领域原文"操作者账号不属于群命名空间"，改为提前拦下并提示
  "请先切换到对应平台"；14 处重复的前置校验收敛成 `groupContextFanLX` 一个入口。
- 注册在按账号派生目录的模型下原本"注册完就找不回来"，改为注册成功即在同一库内切到新身份。
- 单机演示没有第二个进程替对方登录，而好友域要求对端"已确认登录"；`addFriend` 现在按演示口令
  规则把对端临时置为在线（内存态，不写回文件），推不出口令时仍由领域报"对端服务尚未确认登录"。

### 实际验证

| 项 | 结果 |
| --- | --- |
| `cmake --build build -- -j4`（`-DFANLX_BUILD_GUI=ON`） | 通过，含 `-Wall -Wextra -Wpedantic -Werror` |
| `ctest --test-dir build` | **14/14 通过**（连跑三次均通过，约 39 秒含性能项） |
| 基础版回归（`FANLX_BUILD_GUI=OFF` 独立目录配置+构建+CTest） | 通过，默认无 Qt 依赖 |
| 新增 `gui_domain_behavior` | 通过：课设要求 1~6 逐条断言 + 四个页签与各分区控件文案断言 |
| `gui_screenshot.exe --out deliverables\gui-screenshots` | 通过：六张 PNG（新增账号与服务 / 好友 / 群三张），并在截图前真实执行改资料与群特色来回切换 |
| 真机联调 | `fanlx_app --im-server --im-sqlite` + `fanlx_gui --auto-login`：领域库按预期生成在账号目录的 `domain/` 子目录，两把锁互不干扰，进程正常退出 |

### 尚未完成 / 已知边界

- IM 协议仍不负责同步客户端领域菜单的改动：GUI 的好友/群修改只写本机的领域库，
  服务端 `state.json` 不跟着变（与改造前的一致边界，已写入 docs/integration-validation.md）。
- 好友/群的两个 GUI 客户端之间不共享领域数据（按账号隔离），跨客户端的一致性演示需要走服务端领域。
- `chat_history_performance` 在磁盘被占满（例如刚跑完一次完整 MinGW 构建）时偶发
  `0xc0000409` 崩溃一次，单独重跑稳定通过；该用例不涉及本轮改动，属于既有的负载敏感项。

---

## 微X 上层与三种服务分别存储（2026-10）— 用户提出的 5 条修改

### 起因

用户在试用后提出 5 条修改要求，逐条对应如下（编号沿用本节的 ①②③④⑤）：

① 用户昵称、所在地应允许**单项修改**，而不是强制一起改；
② **登录失败不应该继续跳转界面**，而应留在登录界面；
③ **聊天界面的群与好友应该有区别标志**；
④ QQ / 微博 / 微信 三种服务**不应共享群聊和好友**，应分别存储；
⑤ 账号注册不应放在登录后的"账号与服务"界面，**入口放到登录界面**，注册时**不捆绑服务账号**，
   并把**微X作为 QQ/微信/微博 的上层**：只有登录微X后才能在内部开通三种服务。

### 实现（方案 A：微X 号作为唯一登录账号；聊天跟随当前服务；存储与领域都按服务分区）

- **领域层**
  - `ServiceTypeFanLX` 新增 `WeiX`（存储名 `"WeiX"`、界面名 `微X`），并补 `serviceDisplayNameFanLX` /
    `serviceFromNameFanLX` / `isSubordinateServiceFanLX` / `chatCapableFanLX` / `groupCapableFanLX`。
  - `AccountManagerFanLX` 新增 `registerWeiXAccount`（注册只建微X + 立即开通，**不建任何下级服务账号**）、
    `activateService`（建号 + 开通；校验必须是下级服务、必须有微X账号、号码未被占用、两个聊天服务号码不得撞车）、
    `updateNickname` / `updateLocation`（空串 = 该项不改 → 对应 ①）与 `syncSharedFlagsFanLX`
    （QQ 与微博必须同号并标 sharedId，微信独立号）。
  - 联动登录：微X 会话在 `confirmOthers=true` 时联动本人全部已开通下级服务（微X 即授权来源）；
    下级服务之间仍保留"同号 / 显式绑定"旧规则。
  - `AccountSessionFanLX::activateService` 只接受微X会话——"只有登录微X后才能开通三种服务"的落点。
  - 好友按服务分区：新增 `FriendDirectorySetFanLX`，`PlatformStateFanLX::friends` 换成它；
    `FriendServiceFanLX` 新增 `requireSameServiceFanLX`，**跨服务好友关系直接拒绝**（对应 ④）。
- **持久化层**：schemaVersion 升到 **2**，`friends` 与 `groups`/`archivedGroups` 改为按服务命名的对象；
  解码校验"关系两端与分区同名""群键命名空间与分区一致"；v1 旧库明确拒绝并提示重新初始化。
  演示数据改为"先注册微X、再开通 QQ/微博/微信"，好友只建在 QQ 分区。
  事务协调器的好友版本视图与内存发布（PreparedCommit）相应改为按分区遍历 / 整表交换。
- **网络层**：IM 帧新增 `service` 字段，一条连接固定属于一个服务；服务端按 `(服务, 号码, 密码)` 认证，
  好友、群成员、在线状态、授权目录与投递全部只在本服务分区内进行（对应 ④ 的聊天侧）。
- **GUI 层**：登录身份改为微X号，登录窗新增「注册微X账号…」（对应 ⑤）；注册窗重写为独立窗口，
  只填微X号 / 昵称 / 出生时间 / 所在地 / 密码；`DomainControllerFanLX::openWithOutcome` 把失败分为
  `IdentityRejected`（留在登录界面）与 `Unavailable`（可降级为仅聊天）；`main_gui` 在建连后用
  `waitForLogin(6000ms)` 等服务器的认证结论，失败一律回退、不开主窗口（对应 ②）。
  会话项新增 `kindText`/`kindColor`，模型输出 `[好友] 2026002`（蓝）/ `[群] 1001`（紫），
  标题栏与状态栏同源（对应 ③）；聊天连接跟随"当前服务"，切服务即按新服务账号重建连接。
  账号页开通管理加号码输入框、资料维护拆成"保存昵称 / 保存所在地"两个按钮、移除注册按钮（对应 ①⑤）。
- **控制台**：主菜单 1 改为「登录微X或注册」，注册入口移出账号菜单；账号菜单加"开通下级服务 / 切换当前服务 /
  单项修改资料"；登录微X并确认联动后自动把当前服务切到 QQ（优先）或微信。

### 实际验证

| 项 | 结果 |
| --- | --- |
| `cmake --build build -- -j4`（`-DFANLX_BUILD_GUI=ON`） | 通过，含 `-Wall -Wextra -Wpedantic -Werror` |
| `ctest --test-dir build -E performance` | **13/13 通过**（约 27 秒） |
| `gui_domain_behavior` | 通过：新增 t79（单项资料修改）、t77（注册微X + 开通三种服务，含重复开通与号码撞车被拒）、t80（登录失败分类）、t81（state.json 里 friends/groups 为按服务分区的对象、无扁平 friendships、schemaVersion=2、每条关系同服务）；t71/t72/t73/t78 按新模型更新 |
| `gui_behavior` | 通过：新增 t65（群/好友标志与颜色）、t66（错误密码 → `waitForLogin` 返回 false 且原因为「用户名或密码错误」、控制器始终不在线；微博通道被拒；正确凭据仍可登录）、t67（QQ 连接只看到 QQ 好友与 QQ 群、微信连接好友为空、QQ 好友关系不能发微信单聊、两个客户端聊天库目录不同） |
| `persistence_behavior` | 通过：v2 夹具、v1 被拒（断言错误文本含 schemaVersion）、微X 注册只建一个账号、未登录微X不能开通、QQ/微博必须同号、聊天服务号码撞车被拒、跨服务加好友被拒，重开后仍在 |
| `console_behavior` | 通过：登录口注册微X、微X 登录联动、单项资料修改只改填写的那一项、开通 QQ 不捆绑微博 |
| `gui_three_client_behavior` | 通过：三客户端真实收发 + 六张 PNG；新增会话类型标志断言；截图已刷新到 `deliverables/gui-screenshots` |
| 真机联调（自动登录失败路径） | `fanlx_gui --auto-login --password wrong-pw` 以非零码退出并打印 `自动登录失败：微X登录失败：用户名或密码错误`，**未打开主窗口**；正确密码则打开窗口，客户端目录下 `2026001/chat.db` 与 `2026001/domain/state.json` 分开 |

### 尚未完成 / 已知边界

- 领域管理仍不经 IM 协议同步：GUI 里注册的微X号、开通的服务、加的好友只写本机领域库，
  服务端 `state.json` 不跟着变（既有边界，未变）。
- 微信与 QQ 的聊天通道要求号码互不相同（服务端按号码定位收件人），因此开通时会拒绝撞车；
  微博不提供聊天通道（无群、也不收发消息），微X 是上层登录身份、没有自己的好友与群。
- schemaVersion=1 的旧数据目录必须重新 `--init-demo`；程序不做就地迁移（与凭据改造同一策略）。

---

## 微博补齐"除群以外"的能力 + 服务端按通道分键 + 启动脚本自动重建旧库（2026-10 第二轮）

### 起因

用户提出两条：

① 重建旧数据目录等，使 `scripts\run-im-server.bat` 可正常运行（升级到 schemaVersion=2 后，
   `data-im-server` 仍是 v1，服务端会以"未知 schemaVersion"退出）；
② **微博也应加入"除群功能以外"的全部功能**，使其可以正常使用。

### 实现

- **能力拆分**：`chatCapableFanLX` 现在放行 QQ / Wechat / **Weibo**（三种下级服务都能单聊），
  `groupCapableFanLX` 仍只放行 QQ / Wechat。微博因此可用：资料、开通、登录联动、好友增删改查与
  备注、同服务共同好友、跨服务推荐/共同好友、单聊消息与历史、在线状态；不可用：群
  （群列表为空、群操作被拒、控制台聊天菜单不显示"群聊发送"并明确说明）。
- **服务端按"服务 + 号码"分通道**（关键设计）：QQ 与微博按课程约定共用同一个号码，两条通道的号码
  字符串完全相同，因此服务端内部一律使用 `服务名|号码`（群号同样带前缀）：消息库的
  `sender/target/recipients`、`delivery_cursors.account`、`conversations.conversation_key` 都带前缀，
  旧 JSON 后端的 `seqs`/`acks` 亦然。**出站帧一律剥回纯号码**，因此客户端协议、客户端聊天库格式与
  客户端会话键完全不变。新增的辅助函数：`channelIdFanLX` / `plainIdFanLX` / `channelKeyFanLX` /
  `parseChannelKeyFanLX`（后者同时认 SQLite 的 `g|QQ|1001` 与 JSON 的 `QQ|g|1001` 两种形状）。
- **客户端聊天目录按服务分层**：`imClientPathFanLX::chatDirectoryFanLX` 派生
  `<账号目录>/<服务名>`（领域库仍在 `<账号目录>/domain`），因此同一个人的 QQ 与微博各有自己的
  `chat.db` 与目录锁。`ChatControllerFanLX` 与 `ConsoleAppFanLX` 都改用它。
- **号码唯一性放宽**：`activateService` 取消了"QQ 号与微信号必须不同"的限制（号码只要求各自服务内
  唯一；QQ 与微博仍必须共号）。有了通道分键，同号码不会再串消息。
- **演示数据**：新增微博分区的一对好友（U1↔U2），微信分区保持为空，开箱即可演示微博单聊，
  同时保留"QQ 好友不会出现在微信里"的演示点。
- **迁移**：`--im-migrate` 迁移旧 JSON 服务端日志时，账号/群号/会话键/游标统一补 `QQ|` 前缀。
- **启动脚本**：`run-im-server.ps1` 的前置检查 2 现在读快照判断格式，命中"空目录 / schemaVersion=1 /
  缺 password"就把旧目录改名保留（`<数据目录>.legacy-<yyyyMMdd-HHmmss>`，不删除证据）并自动
  `--init-demo` 重建后继续启动；`-DryRun` 只报告计划、无副作用。脚本内的说明文案同步为
  "微X号 + 密码""三种服务都能单聊、只有 QQ 与微信有群""聊天目录按 <服务账号>/<服务名> 派生"。
  注意 `run-im-server.ps1` 必须保持 **UTF-8 with BOM**（PowerShell 5.1 否则按 GBK 读会解析失败）。
- **数据目录重建（实测）**：`run-im-server.bat` 自动把 schemaVersion=1 的 `data-im-server` 改名为
  `data-im-server.legacy-20260929-233359` 并重建；`data-im-a` / `data-im-b` 也重建为 schemaVersion=2；
  旧客户端目录（`data-im-gui`、`build/data-im-gui`）已移走，下次登录自动按新布局重建。

### 实际验证

| 项 | 结果 |
| --- | --- |
| `cmake --build build -- -j4`（`-DFANLX_BUILD_GUI=ON`） | 通过，含 `-Wall -Wextra -Wpedantic -Werror` |
| `ctest --test-dir build -E performance` | **13/13 通过**（约 32 秒） |
| `gui_behavior` t68（新增） | 通过：微博通道上线、目录里有好友且群列表为空、微博单聊收发成功（服务器已接收）、**同号码的 QQ 客户端看不到微博消息**、两通道 `dataDirectory()` 不同且微博目录含 `Weibo` |
| `gui_behavior` t66/t67 | t66 改为"微X（上层身份）在配置阶段被拒"并新增"账号不存在也被拒"；t67 保持 QQ/微信按服务分流与"QQ 好友关系不能发微信单聊" |
| `persistence_behavior` | 通过：演示种子现在 2 条好友关系（QQ + 微博）、QQ/微博必须同号、微信可用与 QQ 相同的号码、跨服务加好友被拒 |
| `chat_history_behavior` | 通过：t53 服务端迁移断言改用带 `QQ|` 前缀的新键 |
| `gui_domain_behavior` / `console_behavior` / `gui_three_client_behavior` | 通过（含六张截图与"会话列表群/好友标志"断言） |
| 手工双终端实测（真实服务端 + 两个控制台客户端，端口 19530） | A：微X `2026001` 登录 → 切到微博 → 菜单显示"（本服务没有群）" → 给 `2026002` 发 `hello-weibo`，状态 `在线 待确认=0`，历史出现 `单聊 2026002 #1 2026001: hello-weibo`；B：微X `2026002` 登录并切到微博后收到同一条消息 |
| 启动脚本实跑 | `scripts\run-im-server.bat` 自动重建旧库后正常监听（`Test-NetConnection` 为 True）；`FANLX_DRY_RUN=1` 下只打印计划、不改目录 |

### 尚未完成 / 已知边界

- 微博没有群（用户要求的正是"除群以外"），因此 1001—1006 仍然只有 QQ 与微信两套命名空间。
- 服务端通道前缀是**服务端内部**约定：同一 host:port 上的旧消息库（升级前写入的、不带前缀的
  `recipients`/`delivery_cursors`）不会与新格式混用，重启服务端后按新格式继续；`--im-migrate` 会补前缀。
- 客户端聊天目录由"账号目录"改为"账号目录 + 服务名"，旧布局的 `chat.db` 不会被自动搬迁（历史重新累积）；
  旧的客户端数据根建议删除或改名（本轮已把仓库里的 `data-im-gui` 移走）。


## 注册后登录失败定位 + 服务端无状态化（2026-10 第三轮）

### 现象与根因

用户实测：**新注册账号、密码没输错，聊天登录失败**。根因不是密码，也不是"服务端需要重启"，
而是**两份账号库互不相通**：

| 写入方 | 文件 | 内容 |
| --- | --- | --- |
| 客户端注册/开通/加好友 | `<客户端数据根>\im-client\<host>_<port>\<微X号>\domain\state.json` | 本机领域数据 |
| 服务端认证 | `<服务端数据根>\state.json` | 服务端账号库 |

聊天认证查的是后者，而注册只写了前者；服务端每次启动只读自己的快照，所以**怎么重启都不会认识这个新账号**。

### 改动

- **协议新增 `REGISTER` / `REGISTER_OK`**：账号层的自举消息（未认证也能发）。
  `sender` = 微X号，`text` = JSON 资料 + 本机已开通的 `services` + 本机已加的 `friends`。
- **`ServerAccountRegistrarFanLX`（新增，`include|src/network/`）**：服务端的登记落地实现。
  幂等；账号不存在则 `registerWeiXUser`（只建微X号、不捆绑服务）；`services` 走 `activateService`
  （领域层要求微X会话，因此临时建立会话后立即注销）；`friends` 走新的
  `PersistenceApplicationFanLX::mirrorFriendship`（镜像**已经成立**的关系，不重复校验"对端确认登录"——
  客户端加好友时已校验，服务端也没有对端密码可重放）。
- **服务端读穿持久化存储**：`ImServerFanLX::OptionsFanLX` 新增 `refreshAccounts`（每次认证前调用）与
  `registerAccount`；`main.cpp --im-server` 接上；
  `JsonFileRepositoryFanLX::changedOnDisk` + `PersistenceApplicationFanLX::reloadIfChangedFromDisk`
  实现"只有文件真的变了才重新解析"，重载时保留内存登录态。
- **客户端自动补登记**：GUI 登录被拒时用刚输入的凭据 `REGISTER` 一次再重试登录（`main_gui`），
  注册窗口在注册成功时立即登记并把结果写进提示（`RegisterDialogFanLX`）；
  控制台在登录与每次开聊天连接前登记（`ConsoleAppFanLX::loginFlowFanLX` / `openImClientFanLX`）。
- **服务端错误信息更可诊断**：服务账号不存在但该号码是"密码正确的微X号"时，回
  `服务端还没有为微X号 X 开通 QQ 服务（…无需重启）`，而不是笼统的"用户名或密码错误"。

### 验证

| 项 | 结果 |
| --- | --- |
| `gui_behavior` t69（新增，5 个阶段） | 通过：① 未登记 → 拒绝；② 只登记微X → 拒绝并提示未开通该服务；③ 带服务+好友登记 → **同一服务端进程**立即可登录，且发给本机新加好友的消息被接受并投递到对端；④ 密码不一致的重复登记被拒；⑤ 停掉服务端重开（同一数据目录）→ 账号仍可登录（持久化） |
| 手工双终端（真实脚本启动的服务端 + 两个控制台客户端，端口 19630） | 注册 2026077 → 登录 → 服务端登记成功 → 开通 QQ → 第二次登录自动切到 QQ 并 `在线 待确认=0`，**服务端全程未重启** |
| `ctest --test-dir build -E performance` | 13/13 通过 |
| 文档 | 新增 [storage-layout.md](storage-layout.md)（存储位置总览）；README 增加"数据都存在哪"速查；HELP 增加两条 FAQ（注册后登录失败 / 是否需要重启服务端） |

### 尚未完成 / 已知边界

- 服务端镜像的只是**账号、开通关系与好友**；群成员与资料修改仍以各自本机领域库为准，
  服务端判定群消息可见性用的是它自己的群数据（`--init-demo` 写入的 12 个群）。
- 登记是"客户端推"的模式：服务端不会反向修改客户端数据；客户端离线期间的他人关系变化不会自动回灌。

## 注册后仍"进不去"的死结：新账号没有聊天通道（2026-10 第四轮）

用户复测反馈"还是不行"。复查后确认**不是编译问题**（`build\fanlx_gui.exe` / `fanlx_app.exe` 均含
`REGISTER` / `mirrorFriendship` / `reloadIfChangedFromDisk`，构建时间与源码一致，仓库里没有旧副本），
真正的死结是登录门与开通流程互相锁死：

1. 按课设要求，**注册不捆绑任何下级服务** → 新微X号没有任何聊天通道；
2. 而 GUI 的登录门要求"聊天通道认证成功"才开主窗口；
3. 「开通管理」在主窗口里 → 于是用户永远开不了聊天通道，界面只报
   `服务端还没有为微X号 X 开通 QQ 服务`，看起来就像"注册了也登不上"。

### 改动

- **区分"认证失败"与"还没有通道"**：`DomainControllerFanLX` 新增
  `subscribedChatServices()` / `hasChatChannel()` / `chatChannelHint()`；
  身份被拒（微X号不存在 / 密码错）仍留在登录界面；身份没问题但没有通道时**放行进主界面**，
  聊天页置为未连接并在顶部提示"到「账号与服务 → 开通管理」开通 QQ / 微信 / 微博"。
  判定用"已开通"而不是"已登录"（聊天凭据由服务端独立认证），因此刚开通、还没点"逐项确认登录"也能连上。
- **开通/加好友后立刻登记到服务端**：登记逻辑收敛到
  `DomainControllerFanLX::pushAccountToServer()`（幂等），在登录被拒重试、开通成功、加好友后自动调用。
- **窗口内切服务的重试**：`MainWindowFanLX::onIdentityChangedFanLX` 在连接被拒时先登记再重试一次，
  因此"在窗口里开通 QQ → 聊天页自动连上"，不需要退出重进，更不需要重启服务端。
- **新增两个等价于界面按钮的 CLI**（供无人值守演示与回归）：
  `--register [--nickname/--birth-date/--location]`、`--activate <QQ|微信|微博> [--service-number]`。
- **测试**：`gui_domain_behavior` t83 覆盖"新账号没有通道 → 提示正确 → 开通后立刻有通道"；
  t69 覆盖服务端登记/持久化；t82 覆盖中文服务名切换。

### 实测（每一步都是真实进程，服务端全程未重启）

```
run-im-server.bat 19695                 # 脚本启动的服务端
fanlx_gui --register --account 2026097 --password pw-2026097 --data-root <临时根>
   → 本机注册成功：微X号 2026097（未捆绑任何下级服务）
   → 服务端登记成功：微X账号已登记到服务端（持久化存储），可直接登录
fanlx_gui --auto-login --account 2026097 …        # 没有任何下级服务
   → 主窗口照常打开（修复前：报"自动登录失败"并退出），顶部提示去开通管理
fanlx_gui --activate QQ --account 2026097 …
   → 已开通 QQ（号码 2026097）；服务端 state.json 里出现 QQ/2026097（userId=u-2026097）
控制台客户端（同一个领域库）登录 2026097
   → 登录成功 → 当前服务自动切到 QQ 2026097 → 聊天状态"在线 待确认=0"
```

`ctest --test-dir build -E performance` 13/13 通过；截图证据已重新生成。
## 加好友"对端平台账号不存在" + 联动登录按钮的定位（2026-10 第五轮）

### ① 加好友报"对端平台账号不存在"

根因与注册那次同源：**本机领域库只是"谁存在"的一份局部视图**。别人在各自客户端里注册/开通出来的
账号不会同步到本机，而 `FriendServiceFanLX::requireReadyFanLX` 查的正是本机账号表，于是号码没错也报
"对端平台账号不存在"（控制台原文如此；GUI 显示同一句领域错误）。

修复（GUI 与控制台共用一条实现，`network/PeerAccountResolutionFanLX.h`）：

1. 本机已认识且已确认 → 直接走原有流程（不打扰服务端）；
2. 本机不认识 → 用**当前服务的号码 + 密码**向服务端发 `LOOKUP`，服务端只做一次凭据校验
   （**不建立第二个会话**——查询方此刻正连着自己的聊天通道，若先 LOGIN 会被单设备策略拒成
   "账号已有在线会话"，这一点是实测踩出来的）；
3. 服务端确认存在 → `AccountManagerFanLX::importRemoteAccount` 把对端的最小记录导入本机副本
   （用户占位密码满足快照校验、补开通、标记"已确认登录"以通过领域的演示前置条件，见该函数注释）；
4. 服务端也没有 → 明确回报"对端平台账号不存在：服务端也没有 X 这个 QQ 号码（注册只建微X号…）"；
5. 服务端连不上 → 附一句线索后仍按本机账号表继续（离线演示不受影响）；
6. 添加成功后随 `pushAccountToServer` 把好友关系镜像到服务端（它按自己的库判定"是不是好友"）。

新增 `gui_domain_behavior` t84：服务端上造一个"别人注册的账号 2026081（已开通 QQ）"，
本机以 2026099 登录并开通 QQ 后添加它——断言加好友成功且好友列表出现该号码。

实测（真实进程、服务端未重启）：
```
控制台 2026001 → 好友 → 添加 2026071（该号码只存在于服务端）
   → 服务端确认该号码存在，已导入本机账号副本。
   → 操作完成 / QQ 好友 2 人 / 2026071 备注= 标签=
```

### ② 「一键联动登录本人已开通服务」按钮是否有必要

结论：**保留，但移位、改名并按"有没有事可做"置灰**。理由：

- 它与「逐项确认登录」是同一件事的两个粒度（全部 / 单个），功能上确实重复，但**不是冗余**：
  「全部注销」「单独注销某服务」「刚开通一个新服务」之后都需要它——那时本人有已开通但未登录的服务；
- 它原来放在「① 当前身份」里，语义不对（那组是切换身份），点击后又什么都不像"切换"；
  现已移到「④ 登录管理」，改名为「一键联动登录（本人已开通的全部服务）」，并加了说明性 tooltip；
- 新增 `DomainControllerFanLX::unlinkedServices()/hasUnlinkedServices()`，页面刷新时据此
  `setEnabled(...)`：正常情况下登录微X 已经联动确认过（课设要求 5），按钮是**灰的**；
  只有确实有"已开通但未登录"的服务时才可点——避免摆一个点了没反应的按钮。

### 其它

- `tools/gui_screenshot.cpp`：修掉一处已知假红——重启客户端后**一次性**发起历史查询，若恰好撞上
  本地聊天库尚未就绪会被静默丢弃；现在改为在等待期间重复发起（判定条件不变，仍必须真的查到那条消息）。

## 好友申请 / 临时会话 / 跨服务添加好友（2026-10-05）

### 需求

① 添加好友改为"一方发出申请、对方同意后才成为好友"；同意之前允许一定条数的临时对话；
② 可以依据本人开通的**另一个服务**里的好友来添加好友。

决定（与用户确认）：跨服务添加时**自动换算**目标号码（源服务好友 → 同一个人 → 他在当前服务的号码）；
申请记录**只在服务端**保存与中转（本机不另存一份）。

### 实现

- 服务端（此前已有申请簿 `FriendRequestBookFanLX`、额度表 `TemporaryChatQuotaFanLX` 与 FRIEND 协议）：
  - FRIEND 新增可选字段 `via`：`op=request` 时 sender 解释为"本人在 via 服务里的好友"，服务端先校验
    **这段好友关系确实存在**，再换算成对方在当前服务的号码（`resolveViaFanLX`），对方未开通则明确报错；
  - FRIEND_RESULT 增加 `peer`（换算后的号码）与 `friendsComplete`（好友清单被裁剪时为 false，
    客户端据此只补不删）。
- 共享实现 `peerAccountFanLX::friendOperationFanLX`（`network/PeerAccountResolutionFanLX`）：
  执行一次服务端好友操作，并按服务端好友清单**对账本机领域副本**（缺的导入账号后补登，多的删除）。
  GUI 与控制台共用。
- GUI：`DomainControllerFanLX::addFriend` 改为发申请；新增 `sendFriendRequest / accept / reject /
  cancelFriendRequest`、`friendRequestView`、`crossServiceFriendCandidates`、`addFriendFromService`、
  `onFriendNoticeFanLX`；删除好友先经服务端解除。好友页新增「好友申请与临时会话」子页（同意 / 拒绝 / 撤回、
  临时会话剩余条数），「添加好友」页增加附言与「依据本人另一个服务的好友添加」。
  主窗口把聊天通道收到的 FRIEND_NOTICE 接到对账与申请页刷新（此前该信号无人接收）。
- 控制台好友菜单：2 发送好友申请、8 申请与临时会话、9/10/11 同意/拒绝/撤回、12 依据另一个服务的好友申请。
- 服务端不可达时（未配置或连不上）添加好友退回**单机演示路径**：本机直接建立关系，提示文本写明
  "服务端不可达，单机演示模式"。这是为了保留离线回归（t73 等），不是正式流程。

### 顺带修复（均为此前未完成改动导致的构建失败，不属于新功能）

- `ImServerFanLX.cpp` 两处 `utcNowFanLX()` 与 `chatDbFanLX::utcNowFanLX` 二义（改为 `::utcNowFanLX()`）；
- `ChatControllerFanLX::reconnect()` 已声明、被主窗口调用但没有定义（补为转调 `ImClientFanLX::reconnect`）。

### 实际验证

| 项 | 结果 |
| --- | --- |
| `cmake --build build`（GUI 开，`-Werror`） | 通过 |
| `im_behavior` 新增 `friendRequestTestsFanLX`（真实 TCP，额度设为 2） | 通过：非好友每方向 2 条、第 3 条拒绝、重放不重复扣额度、反方向独立；申请后非好友、对方可见附言、发起方不能替对方同意；同意后不限额；微信里依据 QQ 好友换算为 wx_2026007 并需对方同意；冒充"非好友"作依据被拒；拒绝 / 撤回不建关系；删除后回到临时会话；服务端持久化账号库中关系正确 |
| `gui_domain_behavior` t84（改写） | 通过：申请 → 未成好友 → 出现在『我发出的申请』→ 对方同意 → 对账后出现在好友列表；微信里依据 QQ 好友添加并经对方同意 |
| `gui_behavior` t67（改写） | 通过：QQ 好友关系不带到微信——微信单聊作为临时会话被接收并扣额度，双方仍不是微信好友（旧断言"非好友单聊必须被拒"与临时会话需求冲突） |
| `ctest -E performance` | 12/13 通过；唯一失败为 `gui_domain_behavior` 的 t80 三条（见下） |

### 尚未解决

- **t80 与此前的登录别名改动冲突**：`openWithOutcome` 已在更早的未提交改动中允许"用 QQ/微信/微博号码作为
  登录别名"，而 t80 仍断言"用下级服务号码登录必须被拒"。两者必须二选一，本轮未改动该逻辑，待确认。
- 测试环境注意：本机沙箱里默认 `%TEMP%` 不可写，跑 CTest 需把 `TEMP`/`TMP` 指到可写目录。

---

### 2026-10-05 `cache_benchmark` 重写

原基准有四处会误导读者，本轮按"要么测准、要么明说测不了"的原则重写 `benchmarks/cache_benchmark.cpp`：

- **`mixed` 名不副实（已修）**：原负载的请求池只由 `queryGroupMembers` 决定，共同好友查询固定一组参数、必然全命中，因此 43.7% 的命中率与"局部版本隔离"无关，只是单一工作集的容量效应。现改为 80% 群成员（跨 100 群）+ 20% 共同好友（对端在 4 个里随机），命中率 35.2%，才是两类查询真实争抢缓存预算的结果。
- **轮数不足且存在漂移（已修）**：实测本机会在固定轮次同时抬高开/关两种模式的延迟约 49%（cache=0 由 320→490 μs，cache=1 由 24→47 μs，同步发生，与缓存和内存池无关，判定为 CPU 降频/热效应）。现改为 1 轮预热 + 9 轮测量，并新增 `p95_spread_us` 汇总行把轮间极差直接写进 CSV；文档改用中位数而非均值。
- **写死下标（已修）**：`latencies[500]`/`latencies[950]`、`queries` 列写死 1000，改请求数即越界或静默偏移。现从样本数推导。
- **异常路径不清理（已修）**：`remove_all` 在 try 尾部，任何 throw 都会在 `%TEMP%` 留下整个数据目录。现用 RAII guard，并去掉 `<windows.h>` 依赖（改用 `std::random_device` 生成目录名），基准可在非 Windows 编译。
- 其他：每请求的 `std::string` 构造与比较改为 `std::string_view`；规模魔数集中到 `BenchConfigFanLX`；总结行的 users/groups/friendships 从实际落盘快照实取，不再手抄常数；新增 `pool_blocks` 与 `pool_creates_delta` 列，并把"增量列"和"瞬时值列"在表头注释里分清（原 `pool_creates` 写的是累积量，容易被当成每轮量）。
- 写放大扫描保留并改为同库 5 次重复：2.89–3.72 s/次。**未做规模梯度**——`importInitial` 只允许空库导入，一次进程内只能有一个规模的库，做梯度需要在子进程里重建；这一点已在代码注释和 `docs/cache-validation.md` 里写明，没有把单一规模的数据冒充成曲线。
- 验证：`cache_benchmark` 在 `-Wall -Wextra -Wpedantic -Werror` 下编译通过；实跑 54 s，退出码 0，各开关结果逐项相等；`ctest -E "performance|gui"` 10/10 通过。

---

### 2026-10-10 `build/` 内客户端数据迁出与残留清理

**背景**：GUI 的默认数据根按"当前工作目录"解析（`src/gui/main_gui.cpp`），在 `build\` 下启动就会落到
`build\data-im-gui\`。该路径仅靠 `.gitignore` 的 `/build/` 才被忽略，而 `/build/` 的语义是
"可随时删除重建"——全量重建会连同聊天记录一起抹掉，且不受 git 保护。HELP.md 第 7 节早已把
`data-im-gui/` 记为项目根目录，本次改动让实际布局与文档一致。

**只动数据布局，未改任何源码逻辑。**

| 项 | 处理 | 依据 |
| --- | --- | --- |
| `build/data-im-gui/` | **移到** 项目根 `data-im-gui/`（34 文件 / 639.71 KB，搬迁前后逐文件校验一致） | 与 `data-im-a`/`data-im-b`/`data-im-server` 平级，由 `/data-*/` 统一保护 |
| `build/data/` | 删除（仅 1 个 0 字节 `.lock`） | 首次启动留下的孤儿；与 `build/data-im-gui` 是不同路径 |
| `build/data-im-gui.old-server-instance/` | 删除 | 2026-09-29 人工改名的退役实例存档，其自带说明已写明"确认不需要后可整个删掉" |
| `build/data-im-gui.legacy-20260929-233428/` | 删除 | 服务商分层（QQ/Wechat）改造前的旧扁平布局备份 |
| `build/tmp/` | 清空 323 个测试临时目录（3.45 MB），保留 `tmp/` 自身 | 全部匹配 `fanlx-gui-tests-*` / `fanlx-network-*` / `fanlx-demo-*` |

**可回滚**：前三个目录已打包至 `.artifacts/backup-data-20261010/`（该目录被 `.gitignore` 忽略），
逐个校验过条目数与源目录一致后才删除。

**未做（有意为之）**：
- 未改 `main_gui.cpp` 的数据根解析逻辑——改为 exe 目录或 `QStandardPaths` 会改变现有行为，需另行评估；
  当前以文档警示 + 数据迁出的方式消除风险。
- `build/tmp` 的清理只是治标：测试仍不回收临时目录（`std::filesystem::temp_directory_path()`
  下的 323 个残留、含 108 个 `.lock` 与 2 个 `-wal`），根因是测试缺 RAII 清理，
  且残留的 `-wal` 提示存在未正常关闭的 SQLite 连接。已在下方「尚未解决」记录。

**验证**：`build/` 关键产物全部在位（`CMakeCache.txt` / `Makefile` / `compile_commands.json` /
`fanlx_gui.exe` / `libfanlx_gui_core.a` / `.qt/` / `platforms/qwindows.dll` / `generated/sqlite/`），
`CMakeFiles/` 22 个目标目录完整，构建树未被破坏；`git check-ignore` 确认
`data-im-gui/` 下所有文件均被忽略，无数据文件进入版本控制。

**尚未解决**：测试临时目录不回收 + SQLite `-wal` 残留（疑连接泄漏），待单独处理。

### 2026-10-10（续）演示数据目录清理与重建

承接上一节，清理可重建的演示数据与已失效备份，**未改动任何源码**。

| 项 | 处理 | 依据 |
| --- | --- | --- |
| `.artifacts/backup-data-20261010/` | 删除（上一节的回滚包，确认新布局稳定后不再需要） | 该目录定位即"用完即弃"，2026-09-23 已有同类清理先例（约 993 MB） |
| `data-im-a/` `data-im-b/` | 删除并重建 | 实测两者 `state.json` / `.prev` **逐字节相同**，纯演示数据 |
| `data-im-server/` | 删除并重建 | 同上；原目录含 737 KB WAL（大于主库 57 KB），整目录删除避免只删主库造成不一致 |
| `build/tmp/` | 再次清空 | 上节清理后测试运行又产生的新残留 |

**重建命令**（与 HELP.md 4.4 节一致，三条均返回 `Loaded users=12 generation=2`、退出码 0）：

```
build\fanlx_app.exe --data-dir data-im-server --init-demo
build\fanlx_app.exe --data-dir data-im-a --init-demo
build\fanlx_app.exe --data-dir data-im-b --init-demo
```

**保留**：`data/`（仅 `.gitkeep`，且被 git 跟踪，`src/main.cpp:92` 以此为控制台默认目录）、
`data-im-gui/`（**真实聊天记录**，34 文件未变动）。

**关于「把密码统一改成 123」的结论：不做，且不建议做。**
演示密码并非存在数据里，而是**代码里的生成规则**：`demoPasswordFanLX(n) = "pw-" + n`
（`src/persistence/DemoSeedFanLX.cpp:8`），全项目 **78 处引用**（12 个测试文件、
`tools/gui_screenshot.cpp`、`benchmarks/cache_benchmark.cpp`、`src/main.cpp`）。
只改数据目录会让测试与截图工具全部因密码不符而失败；要改必须连代码一起改，
属于既有约定的变更而非清理，本轮不动。用户已确认保持现状。

**验证**：`ctest -E "performance|gui"` 10/10 通过；服务端以重建后的
`data-im-server` 启动成功、`127.0.0.1:9500` 可连接并生成新 `server-identity.json`。

### 2026-10-10（续二）本机演示账号密码改为 123

用户要求把文档里列出的演示账号（`2026001` 等）密码改为 `123` 以便手工登录。
**只改数据文件，未改任何源码**——`demoPasswordFanLX()` 仍是 `pw-<微X号>`，
因此重新 `--init-demo` 会回到 `pw-...`（已写入 HELP.md 6.1 节提示）。

| 数据目录 | 改动 | 说明 |
| --- | --- | --- |
| `data-im-gui/`（4 份 `state.json` + 4 份 `.prev`） | 每份 12 个 `U*` 演示账号 | 客户端侧账号库 |
| `data-im-server/state.json` | 12 个 | 服务端侧账号库 |
| `data-im-a/` `data-im-b/` | 各 12 个 | 控制台客户端 |

**踩到的坑（已修正，记录以免重犯）**：
`state.json` 里用户对象的键序是 `applyDate,birthDate,location,nickname,password,userId`
——**`password` 在 `userId` 之前**。第一版脚本"边扫边记住上一个 userId"，
导致 `password` 被按前一个对象的身份改写：`u-1` 被误改、`u-111` 的
`<remote-account>` 占位值被覆盖成 `123`。**已从备份完整还原**，
改为按对象边界（`{`…`}`）成对解析后重做。修正版脚本内置两道防线：
改后必须仍是合法 JSON、且**非演示账号（`u-*`）必须与原文一致**，否则拒绝落盘。

**必须两边都改**：服务端按自己的 `state.json` 校验、客户端按
`.../<账号>/domain/state.json` 校验。只改客户端会得到「用户名或密码错误」。

**验证**（真实登录，非仅校验文件）：
- `fanlx_gui --auto-login --account 2026001 --password 123` → **无错误输出、窗口保持运行**
- 对照组 `--password pw-2026001` → 打印 `自动登录失败：微X登录失败：用户名或密码错误`
- `fanlx_app --check` 逐个加载四个目录 → 全部 `Loaded users=12`、退出码 0；
  其中 `data-im-gui` 的领域快照 `Loaded users=14 generation=13`，证明改后文件被程序正常接受
- `ctest -E "performance|gui"` 10/10 通过

**未受影响**：用户自己注册的账号（`u-1`、`u-111`、`u-123`）密码保持原样；
测试代码引用的 `pw-...` 常量与新建数据的行为均未改变。

**新增文档**：`data-README.md`（哪个目录能删、如何重建、目录结构与密码机制）。

**测试过程中的副作用已清理**：探针曾在 `data-im-gui` 下生成
`im-client/127.0.0.1_9600/`、`2026002/`、`2026003/` 三个目录与一个根级 `.lock`，
均已删除；`data-im-gui` 恢复为原有的 34 个文件、5 个账号目录（`1` `111` `123` `2026001` `2222`）。

### 2026-10-10（续三）新增 `FANLX_BUILD_CLI` 开关，可只构建 GUI

用户只需要 GUI 版，询问能否精简掉控制台客户端。**未删除任何源码**，
而是仿照既有的 `FANLX_BUILD_GUI` 增加一个对称开关，默认 `ON`（保持原有行为不变）。

**依赖关系实测结论**（决定了改动的安全边界）：

```
fanlx_gui_core → fanlx_network → fanlx_persistence + fanlx_chat
fanlx_console  → fanlx_persistence + fanlx_network        ← 独立分支
```

- GUI 源码中引用 `ConsoleApp` / `QQChatSession` 的**只有 2 处注释**，无代码依赖；
- 控制台源码中引用 `gui/` 的**只有 1 处注释**。

即 `fanlx_console` 只被 `fanlx_app`、`network_tests`、`console_tests` 使用，
关掉它**不触碰 GUI 任何一行**。

**改动**（`CMakeLists.txt`）：
- 新增 `option(FANLX_BUILD_CLI ... ON)`；
- 将 `fanlx_console` / `fanlx_app` / `network_tests`（含 `network_behavior`、`im_behavior`）
  / `console_tests`（含 `console_behavior`）/ `smoke` 用例包进 `if(FANLX_BUILD_CLI)`。
  `smoke` 用例单独加了守卫，因为它执行的是 `fanlx_app --demo`。

**为什么是开关而不是删除**：`fanlx_app.exe` 同时是**服务端的唯一实现**（`--im-server`）
与 `--init-demo` 的提供者。直接删除会让 GUI 无法联网、演示数据无法重建，
且 `network_tests` / `integration_tests` 会连带失效。开关可随时取回这些能力。

**验证**：
| 项 | 结果 |
| --- | --- |
| 默认配置（`FANLX_BUILD_CLI` 缺省）重新 configure + build | 通过；`build/` 仍产出 `fanlx_app.exe`、`console_tests.exe`，行为与改动前一致 |
| 默认配置 `ctest -E "performance\|gui"` | 10/10 通过 |
| `-DFANLX_BUILD_CLI=OFF` configure | 通过；目标列表不再含 `fanlx_app` / `network_tests` / `console_tests` |
| CLI=OFF 下构建 `fanlx_gui` | **通过**（`libfanlx_gui_core.a` + `fanlx_gui.exe` 链接成功），证明 GUI 不受影响 |
| CLI=OFF 下全量构建 + `ctest -E "performance\|gui"` | 6/6 通过（用例数由 10 降为 6，正是被守卫的 4 个） |

测试用的 `build-test-nocli/`（303 MB）已删除；该目录名匹配 `.gitignore` 第 3 行 `/build-*/`。

**文档**：`HELP.md` 新增 3.4 节（两个开关的对照表 + "fanlx_app 兼作服务端"的警示）。

### 2026-10-10（续四）新增 GUI 启动脚本；定位"登录失败"的真实原因

用户反馈：双击 `build\fanlx_gui.exe`，用 `2026001` / `123` 登录失败。**不是密码改错了**，
是两个独立原因叠加：

| # | 原因 | 证据 |
| --- | --- | --- |
| 1 | **数据根跟着当前工作目录**：双击 exe 时 CWD 是 `build\`，于是读 `build\data-im-gui`——那是**另一套**数据，密码仍是 `pw-2026001` | `src/gui/main_gui.cpp` 取 `QDir::current()`；实测 `build\data-im-gui` 存在且 `U1='pw-2026001'`，而项目根 `data-im-gui` 为 `'123'` |
| 2 | **服务端没在跑**：客户端登录要经服务端认证，端口 9500 不通时即使密码正确也报"用户名或密码错误" | 停止服务端后重测，正确密码同样失败 |

**新增 `scripts/run-gui.ps1` + `scripts/run-gui.bat`**：切到项目根、显式传 `--data-root`、
必要时先拉起服务端并等待其就绪、GUI 退出后回收服务端。

**编码约定（沿用仓库既有做法，且踩过一次坑）**：
- `run-gui.bat` 必须**纯 ASCII**（cmd.exe 用控制台当前代码页解析 .bat，中文会让它执行自己的行片段）；
  脚本内*所有*中文注释与提示都放在 .ps1 里。
- `run-gui.ps1` 必须 **UTF-8 with BOM**，否则 PowerShell 5.1 按 GBK 解码，中文既乱码又会让
  **解析器报语法错误**（`Array index expression is missing`）。
  ⚠️ 用 `edit` 工具改这个文件后 **BOM 会丢失**，已发生两次；每次改完都要重新补 BOM 并重新做语法检查。
  已用 `[Parser]::ParseFile` 验证通过。

**位置参数顺序**（修过一个 bug）：`.bat` 用 `%*` 原样转发，所以 `param()` 里
**端口必须是第一个位置参数**。初版把 `$Host_` 放在首位，`run-gui.bat 9500` 会把 `9500`
绑到主机名上，服务端报 `IM 端口非法`。现已改为 `$Port`, `$Account`，主机名用命名参数 `-BindHost`。
（另注：`$Host` 是 PowerShell 内置只读变量，不要复用这个名字。）

**逐个验证通过的部分**：
- `run-gui.ps1` 语法检查通过、UTF-8 BOM 正确、`.bat` 纯 ASCII；
- 服务端经脚本拉起后可监听 `127.0.0.1:9500`；
- GUI 以脚本使用的**完全相同参数**启动后持续存活、无错误输出；
- 自动登录 + `123` → 无错误输出（成功）。

**未能验证的部分（如实记录）**：**启动脚本的端到端一次性运行**。
本会话的 shell 在每次调用结束时回收全部子进程，GUI 窗口无法存活到可观测，
因此"双击 `run-gui.bat` → 窗口出现并停留"这一步**没有在本环境中被直接观测到**。
脚本各组成环节均已单独验证，但端到端仍需用户实机确认。

**文档**：`HELP.md` 新增 4.2 节（启动脚本用法、两个坑的对照表、参数表），
并把原 4.2~4.5 顺延为 4.3~4.6；同步修正 `HELP.md:361`、`HELP.md:595`、
`data-README.md` 中随编号变化的交叉引用。

### 2026-10-10（续五）修复并回滚重建 data-im-server 造成的聊天库归档

承上：排查登录失败时发现 `data-im-gui\...\2026001\QQ\` 下多出
`chat.db.legacy-20261010T042456Z`（52 KB，含真实记录）与一个**空的 4 KB `chat.db`**，
外加 `-wal` / `-shm` 残留。

**成因**（`src/network/ImClientFanLX.cpp:318`）：客户端的 `client-identity.json` 记录
`server_instance_id`；一旦与当前服务端实例号不符，客户端会**归档旧库并按新实例建空库**
（设计如此：避免串历史，又不阻断登录）。
我在续二节删除并重建 `data-im-server` 时，服务端实例号由
`srv-3303556171-…` 变为 `srv-2051415770-…`，于是触发归档。

**修复（回滚该副作用）**：
1. 先整目录备份到 `build/tmp/restore-backup-20261010/`；
2. 删除空的 `chat.db` 与 `-wal` / `-shm`（sidecar 会污染恢复）；
3. 把两份 `.legacy-20261010T042456Z` 改回主名；
4. **关键**：把 `client-identity.json` 的 `server_instance_id` 指向**当前**实例号。
   若照搬旧身份文件，登录时会再次判定不一致并**重新归档**，白做。
5. 序列化格式对齐应用自身写法：`dump(2)` + **字母序键**（nlohmann 默认 `std::map`）
   + LF + 无末尾换行，与未改动的同侪文件（`111/QQ/`）逐键比对一致。

**验证**：以 `2026001` / `123` 登录后，`chat.db` **仍是那份 52 KB 文件**（时间戳停留在
2026-09-30 14:47:44）、身份号与在线服务端 `MATCH: True`，**未被二次归档**。

**结果**：`data-im-gui` 恢复为 34 个文件；仅剩 3 组**先前就存在**的归档
（`1/QQ`、`111/QQ`、`111/Wechat`，对应 09-30 / 10-02 / 10-03 的更早实例切换），本轮未触碰。

**教训**：重建服务端数据目录**不是零副作用**——它会让每个客户端把聊天库归档一次。
再做同类操作前应先备份 `data-im-gui`，或事先把服务端实例号固定下来。

#### ⚠️ 更正：上述第 4 步是错的，已回滚

用户随后遇到**硬失败**（而非自动归档）：

```
登录失败: 聊天数据库归属不符(字段 server instance id):
库内为 'srv-3303556171-…', 本次连接为 'srv-2051415770-…'
```

**我的错误**：只改了 `client-identity.json`，却漏掉了**同一个实例号还被写在 `chat.db`
内部的 `schema_meta` 表里**（`src/db/ChatSchemaStoreFanLX.cpp:102-113` 的
`applyClientSchemaFanLX` → `applySchema` → `requireIdentity`，错误文案正是
`聊天数据库归属不符（字段 …）`）。
`ImClientFanLX.cpp:272-273` 的注释**早已写明**这一点：
"chat.db 的 schema_meta 里也存了同一个实例号……只换 client-identity.json 会被库本身再拦一次。"

于是我造出了一个**比原问题更糟**的状态：JSON 身份说"是新实例"、库内身份说"是旧实例"，
两者不一致 → 目录被判定为"误用"，直接抛错、**连自动归档这条兜底路径也走不到了**。

**正确做法（已恢复）**：**什么都不要手工改**。
`archiveStaleChatStoreFanLX` 本就会把 `chat.db` / `chat.db-wal` / `chat.db-shm` /
`client-identity.json` **四个文件一起**改名归档，再按当前实例新建——这是设计好的正常路径。
我把它从备份**逐字节还原**为应用自己产生的那份布局（4 KB 空 `chat.db` + 52 KB 归档），
登录随即恢复正常，且**没有再产生新的归档**。

**验证**：`network_tests --im` 通过（其中 `writeStaleStore` / `rebind-online` 用例
正是覆盖"实例号退役 → 自动归档 → 继续登录"这一路径）；
四个 `chat.db` 全部 `pragma integrity_check = ok`；`2026001/QQ` 的库内实例号与
在线服务端一致。

**同时发现**：`1/QQ`(21 条)、`111/QQ`(18 条)、`111/Wechat`(1 条)、`2222/Wechat`(1 条)
的库内实例号**同样是旧的**，登录时会走同一套自动归档。经用户确认**不做手工改写**，
交给程序的既有逻辑处理，旧数据会保留在 `.legacy-` 文件里。

**清理**：移除了 10 个运行时残留 `chat.db-wal` / `chat.db-shm`（均已 checkpoint 且
integrity ok）与 6 个**归档副本的** sidecar（`*.legacy-*-wal` / `-shm`，对已归档的死文件没有意义）。
`data-im-gui` 现为 36 个文件 = 原 34 + 应用新建的 2 个 `2026001` 归档文件。

**教训（更重要的那条）**：遇到"程序自己会处理"的状态时，
**先读代码确认它的兜底路径，再决定要不要手工介入**。
我这次在没读完 `ChatSchemaStoreFanLX.cpp` 的情况下就动手改了身份文件，
反而摧毁了程序自愈的能力。备份救了我第二次。

### 2026-10-10（续六）演示密码规则改为固定 `123`（改代码，根治）

用户指出 `2026002` 等账号的密码仍未改，无法用于演示。

**排查出的真正症结**：之前几轮都只在"改数据"，但演示密码**不是存出来的，是生成出来的**：

- 客户端目录 `<根>/im-client/<host>_<端口>/<账号>/domain/state.json`
  **只在首次用该账号登录时才创建**，并由 `DomainControllerFanLX.cpp:146`
  调用 `demoSeedFanLX()` 现场种入演示数据；
- 于是**每登录一个新账号，就多出一份带旧口令的库**。
  当前只有 `1/111/123/2026001/2026002/2222` 这几个目录存在，
  `2026003`…`20260012` **根本还没有目录**——改数据永远追不上。

**根治**：改 `src/persistence/DemoSeedFanLX.cpp` 的函数体（唯一权威定义）：

```cpp
std::string demoPasswordFanLX(const std::string &number) {
    (void)number;      // 规则与号码无关；保留形参以兼容既有调用点。
    return "123";
}
```

**为什么改这一行就够**：全项目 60 余处调用点（12 个测试文件、`tools/gui_screenshot.cpp`、
`benchmarks/cache_benchmark.cpp`、`src/main.cpp`、`ConsoleAppFanLX`、`DomainControllerFanLX`）
**一律经此函数取口令**，无一处硬编码演示口令字面量，因此改函数体即全局一致，
且数据与测试不会脱节。刻意**保留 `number` 形参**，将来若要恢复"一人一口令"
只需把函数体改回 `"pw-" + number`，调用点一行都不用动。

**连带修复**（改规则后暴露的**硬编码演示口令**，共 4 处；这些是"自己写死的字面量"，
不走函数，必须单独改）：
- `tests/console_tests.cpp`：3 处 `pw-2026001` → `123`
- `tests/gui_domain_tests.cpp`：2 处 `pw-2026001` → `123`

**注意保留**（这些是**测试自注册账号**的口令，与演示规则无关，不能动）：
`pw-2026077`、`pw-2026099`、`pw-2026081`、`pw-2026088`、`pw-new`、`pw-500`、
`pw-test`、`pw-other`、`pw-b*`，以及 `cache_tests.cpp` 的 `"pw-" + i`。
判断依据是号码是否属于演示种子 `2026001..20260012`。

**数据侧收尾**：`data-im-gui/.../2026002/domain/` 是旧规则下生成的**纯净副本**
（无任何用户改动，users/groups/subscriptions 与种子一致），按新规则重新种入
（旧的先备份到 `build/tmp/pwd-backup-20261010/2026002-domain-backup/`）。

**验证**：
| 项 | 结果 |
| --- | --- |
| 构建（`-Werror`） | 通过，无警告 |
| `ctest -E "performance\|gui"` | **10/10 通过**（首轮 `console_behavior` 因硬编码口令失败，修正后全绿） |
| 四个数据根全部演示账号 | `demo-not-123 = 0` |
| 用户自有账号 `u-1`/`u-111`/`u-123` | 未被触碰（仍为 `1` / `<remote-account>` / `123`） |
| `--init-demo` 到空目录 | 产出 `123`（确认新库不再回退到旧口令） |

**文档同步**：`HELP.md`（6.1 密码表 + 4 处示例/排错）、`README.md`、`data-README.md`、
`docs/project-structure.md`、`docs/network-validation.md`、`docs/integration-validation.md`、
`docs/data-format.md`、`docs/persistence-validation.md`、`docs/handoff-prompt.md`
均已更新；保留少量带"历史"标注的旧口令说明供对照。

**教训**：**先分清"数据"与"生成规则"**。前几轮我一直在改数据的产物，
而问题的根源在生成器；只要还有没登录过的账号，产物就永远改不完。
判断方法：找出该值**是"写出来的"还是"算出来的"**——算出来的只需改一处。

## 对象池加固与在线好友直连（2026-10-14）

服务器连接仍是控制面。`DirectLinkFanLX` 在已登录的好友之间再开一条 TCP，只运送在线单聊正文。群、离线、非好友、临时会话和发送方历史回显仍是 `DELIVER`。3 秒没有 ACK 时，同一 `messageId` 改由服务器送出正文。说明、课程对照和命令见 [hybrid-tcp.md](hybrid-tcp.md)。

对象池：`destroy` 用整数地址校验槽位，空块不回收。`poolCreates` 仍是成功构造。六次 `pool_benchmark` 谁更快会换边，不能写成池更快。改池前没有全量 CTest 基线。

本轮已通过：`network_behavior` 7.22s，`im_behavior` 9.34s，`p2p_behavior` 13.65s，`cache_behavior` 0.01s，`persistence_behavior` 1.74s，`query_cache_behavior` 0.14s，`integration_behavior` 0.77s，`group_behavior` 0.01s，`chat_history_behavior` 0.54s，`smoke` 0.91s，`console_behavior` 0.76s，`gui_behavior` 13.37s。`fanlx_app`、`console_tests`、`fanlx_gui`、`gui_tests` 已按新库重新链接。`gui_domain_behavior` 与 `gui_three_client_behavior` 本轮没有跑。









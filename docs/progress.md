# 开发进度 — 樊陆旭模拟即时通讯系统

## 项目概况

| 项目       | 内容                                              |
| ---------- | ------------------------------------------------- |
| 项目名称   | 樊陆旭模拟即时通讯系统                            |
| 学期       | 2026-2027 学年第一学期                            |
| 班级       | 552502 班                                         |
| 工程文档   | `deliverables/樊陆旭_面向对象课设工程文档.docx`（按课程设计文档要求撰写） |
| 工具链     | CMake 3.20+ / MinGW（`D:/mingw64`）/ C++17        |
| 当前构建   | ✅ 编译通过，CTest **8/8** 通过                    |

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
| S07（已完成） | 业务查询 ARC/内存池接入、版本失效、LFU/2Q/ARC T13、固定种子基准 | T13 开关结果一致；1000 查询×5 轮、缓存/回源与写延迟 CSV |
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


## 2026-09-23 仓库清理与帮助文件

删除与工程无关的临时产物：`.artifacts/`（旧构建目录 im-build/build-debug/build-release/build-s10-0923、渲染 PNG/PDF、clangd 日志、一次性脚本与 format_tool，约 993 MB）、`build/`（构建输出，约 183 MB）、`data-chat-a|b`、`data-im-a|b`、`data-im-server`（运行时演示库，可用 `--init-demo` 重建）、`.cache/`（clangd 索引）、空目录 `fixtures/` 与 `docs/prompts/`（过程提示词）。保留源码、测试、文档、第三方头文件、课程任务书与 deliverables 报告。README 与各验收记录中对 `.artifacts/...` 的引用已加说明，命令改用任意构建目录即可复现。

新增 `.gitignore`（忽略 `build/`、`build-*/`、`.artifacts/`、`data-*/`、编译产物与日志），构建产物自本次起不再入库；此前误入库的构建产物已从索引移除，删除的受版本控制文件仍可经 git 历史取回。

清理后在仓库外全新目录验证：MinGW GCC 15.1.0 配置与构建成功，CTest 9/9 通过（18.34 秒），`fanlx_app --demo` 退出码 0；在 `PATH` 不含 MinGW 的环境下仍可直接运行（`-static` 已消除 0xC0000139）。

新增根目录 HELP.md：环境要求、构建与干净重建、命令行参数、全部菜单速查、IM 三终端联调、测试与基准、数据与锁、FAQ 及文档索引。

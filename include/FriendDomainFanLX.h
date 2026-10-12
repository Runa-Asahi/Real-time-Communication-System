// ============================================================================
// FriendDomainFanLX.h —— 好友域：好友关系 / 账号身份解析 / 好友服务
// ----------------------------------------------------------------------------
// 本文件把"好友"领域拆成三个协作类：
//   1. FriendDirectoryFanLX  好友目录：无向图式的底层存储，维护
//      "账号键(owner) <-> 账号键(peer)" 的双向关系与备注/标签等细节；
//   2. IdentityResolverFanLX 身份解析器：把"平台 + 账号 ID"解析成全局唯一的
//      账号键（"用户ID|平台|账号ID"），或从账号键反解出用户 ID；
//   3. FriendServiceFanLX    好友服务门面：面向"已登录用户"的高层业务接口。
// 身份口径（rev2，与工程计划 §4.1 的三层身份一致）：
//   - 发起方一律用 AccountSessionFanLX（登录会话）表示，对端一律用平台账号
//     (ServiceTypeFanLX, accountId) 表示；
//   - 公开签名中不出现内部 userId —— 现实中加好友用的是对方的 QQ 号 / 微信号；
//   - userId 只在 IdentityResolverFanLX 内部参与聚合（如跳平台推荐）。
// 组合设计（面向对象）：
//   FriendServiceFanLX 内部以"引用成员 + 组合"的方式持有 FriendDirectoryFanLX、
//   AccountManagerFanLX 与 IdentityResolverFanLX，职责单一、易于替换测试。
// ============================================================================
#pragma once
// 兼容入口：好友域的全部类型。按课程要求，每个类各自独立存放于 include/friend/ 下。
#include "friend/FriendshipFanLX.h"
#include "friend/FriendDirectoryFanLX.h"
#include "friend/FriendDirectorySetFanLX.h"
#include "friend/IdentityResolverFanLX.h"
#include "friend/FriendServiceFanLX.h"
#include "friend/FriendSnapshotFanLX.h"

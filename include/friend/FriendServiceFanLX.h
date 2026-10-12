#pragma once
#include "friend/FriendDirectorySetFanLX.h"
#include "friend/IdentityResolverFanLX.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// FriendServiceFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
//
// 2026-10 改造：好友目录由"一份共用表"改为"按服务分区的目录集合"。本类不再直接持有某一
// 份目录，而是按发起方/对端的服务去取对应的那一份——因此
//   · 同一服务内的关系只写进该服务的分区；
//   · 跨服务建立好友关系被明确拒绝（QQ 号只能加 QQ 号），不再可能"QQ 加好友、微信也出现"。
class FriendServiceFanLX {
    FriendDirectorySetFanLX &directoriesFanLX; // 按服务分区的好友目录集合
    const AccountManagerFanLX &accountsFanLX;  // 账户管理器（只读引用）
    IdentityResolverFanLX resolverFanLX;       // 身份解析器（组合成员，负责键转换）

    // 前置校验：发起方会话必须仍然有效，否则按"会话已过期"处理。
    static void requireSessionFanLX(const AccountSessionFanLX &session) {
        if (!session.isLoggedIn())
            throw SessionExpiredFanLX("会话已过期，请重新登录");
    }
    // 前置校验：对端平台账号必须"已建号、已开通、已确认登录"，否则按错误类型区分抛异常。
    void requireReadyFanLX(ServiceTypeFanLX service, const std::string &accountId) const {
        if (!accountsFanLX.hasAccount(service, accountId))
            throw std::runtime_error("对端平台账号不存在");
        if (!accountsFanLX.isAccountSubscribed(service, accountId))
            throw ServiceNotSubscribedFanLX("对端服务尚未开通");
        if (!accountsFanLX.isAccountLoggedIn(service, accountId))
            throw ServiceNotConfirmedFanLX("对端服务尚未确认登录");
    }
    // 前置校验：两个会话必须属于同一个真人（跳服务平台内聚合的前提）。
    void requireSameOwnerFanLX(const AccountSessionFanLX &left, const AccountSessionFanLX &right) const {
        const auto &a = accountsFanLX.accountByAccountId(left.service(), left.accountId());
        const auto &b = accountsFanLX.accountByAccountId(right.service(), right.accountId());
        if (a.userId != b.userId)
            throw std::runtime_error("两个会话不属于同一个用户");
    }
    // 前置校验：好友关系必须落在同一个服务分区内（三种服务不共享好友）。
    static void requireSameServiceFanLX(const AccountSessionFanLX &self, ServiceTypeFanLX peerService) {
        if (peerService != self.service())
            throw std::runtime_error("好友关系只能在同一个服务内建立（QQ 加 QQ、微信加微信、微博加微博）");
    }

  public:
    // 构造：注入按服务分区的目录集合与账户管理器（引用组合，不拷贝大容器）。
    FriendServiceFanLX(FriendDirectorySetFanLX &directories, const AccountManagerFanLX &accounts)
        : directoriesFanLX(directories), accountsFanLX(accounts), resolverFanLX(accounts) {}

    // 添加好友：发起方须处于有效会话，对端须同服务、已开通且已登录；其"账号键"登记进该服务的分区。
    bool addFriend(const AccountSessionFanLX &self, ServiceTypeFanLX peerService,
                   const std::string &peerAccountId) {
        requireSessionFanLX(self);
        requireSameServiceFanLX(self, peerService);
        requireReadyFanLX(peerService, peerAccountId);
        const auto left = resolverFanLX.accountKey(self.service(), self.accountId()),
                   right = resolverFanLX.accountKey(peerService, peerAccountId);
        return directoriesFanLX.directory(self.service()).add(left, right);
    }
    // 修改备注/标签（可带默认空标签），操作对象解析为账号键后交给所属服务的目录层。
    void updateRemark(const AccountSessionFanLX &self, ServiceTypeFanLX peerService,
                      const std::string &peerAccountId, std::string remark, std::string tag = {}) {
        requireSessionFanLX(self);
        requireSameServiceFanLX(self, peerService);
        directoriesFanLX.directory(self.service())
            .update(resolverFanLX.accountKey(self.service(), self.accountId()),
                    resolverFanLX.accountKey(peerService, peerAccountId), std::move(remark),
                    std::move(tag));
    }
    // 删除好友：先校验发起方状态，再执行该服务分区内的删除。
    bool removeFriend(const AccountSessionFanLX &self, ServiceTypeFanLX peerService,
                      const std::string &peerAccountId) {
        requireSessionFanLX(self);
        requireSameServiceFanLX(self, peerService);
        return directoriesFanLX.directory(self.service())
            .remove(resolverFanLX.accountKey(self.service(), self.accountId()),
                    resolverFanLX.accountKey(peerService, peerAccountId));
    }
    // 查询：列出"某用户在某平台"的全部好友（只读该平台的分区，返回包含备注/标签的完整细节）。
    std::vector<FriendshipFanLX> findFriends(const AccountSessionFanLX &self) const {
        requireSessionFanLX(self);
        const auto &directory = directoriesFanLX.directory(self.service());
        const auto key = resolverFanLX.accountKey(self.service(), self.accountId());
        std::vector<FriendshipFanLX> out;
        for (const auto &peer : directory.peers(key))
            out.push_back(directory.detail(key, peer));
        return out;
    }
    // 共同好友：求两个账号（同一服务内）在**该服务分区**里好友键集合的交集。
    std::vector<std::string> commonFriends(const AccountSessionFanLX &self, ServiceTypeFanLX otherService,
                                           const std::string &otherAccountId) const {
        requireSessionFanLX(self);
        requireSameServiceFanLX(self, otherService);
        const auto &directory = directoriesFanLX.directory(self.service());
        const auto a = directory.peers(resolverFanLX.accountKey(self.service(), self.accountId()));
        const auto b = directory.peers(resolverFanLX.accountKey(otherService, otherAccountId));
        std::vector<std::string> out;
        std::unordered_set<std::string> setB(b.begin(), b.end());
        for (const auto &key : a)
            if (setB.count(key))
                out.push_back(key);
        std::sort(out.begin(), out.end());
        return out; // 集合求交 + 排序
    }
    // 跨平台共同好友：同一用户在其两个不同平台（如 QQ 与微信）上，
    // 分别有哪些"人"（按用户 ID 去重）同时是好友。
    // 两个平台各读自己的分区——这正是"分别存储"之后仍然能回答跨平台问题的写法。
    std::vector<std::string> crossServiceCommonFriends(const AccountSessionFanLX &first,
                                                       const AccountSessionFanLX &second) const {
        requireSessionFanLX(first);
        requireSessionFanLX(second);
        requireSameOwnerFanLX(first, second); // 两个会话必须属于同一个真人
        const auto &firstDirectory = directoriesFanLX.directory(first.service());
        const auto &secondDirectory = directoriesFanLX.directory(second.service());
        std::unordered_set<std::string> usersA, usersB;
        for (const auto &key :
             firstDirectory.peers(resolverFanLX.accountKey(first.service(), first.accountId())))
            if (!resolverFanLX.verifiedTarget(key, first.service(), second.service()).empty())
                usersA.insert(resolverFanLX.userForAccount(key));
        for (const auto &key :
             secondDirectory.peers(resolverFanLX.accountKey(second.service(), second.accountId())))
            usersB.insert(resolverFanLX.userForAccount(key));
        std::vector<std::string> out;
        for (const auto &id : usersA)
            if (usersB.count(id))
                out.push_back(id);
        std::sort(out.begin(), out.end());
        return out; // 先在各自平台去重，再按用户求交
    }
    // 好友推荐：把用户在 source 平台的好友，推荐为该用户在 target 平台上的
    // 可加好友对象（对方须有 target 平台账号、尚未成为好友、且不重复推荐）。
    // 两个平台读各自的分区，推荐本身不写入任何关系（要添加必须切到目标平台再执行）。
    std::vector<std::string> recommendFriends(const AccountSessionFanLX &source,
                                              const AccountSessionFanLX &target) const {
        // 前置校验：两个会话都有效、属于同一人，且各自处于登录态
        requireSessionFanLX(source);
        requireSessionFanLX(target);
        requireSameOwnerFanLX(source, target);
        const auto &sourceDirectory = directoriesFanLX.directory(source.service());
        const auto &targetDirectory = directoriesFanLX.directory(target.service());
        const auto sourceKey = resolverFanLX.accountKey(source.service(), source.accountId()),
                   targetKey = resolverFanLX.accountKey(target.service(), target.accountId());
        std::vector<std::string> out;
        std::unordered_set<std::string> seen; // seen 集合用于对推荐结果去重
        const auto &self = accountsFanLX.accountByAccountId(source.service(), source.accountId())
                               .userId; // 跳平台聚合需要的归属用户（仅在本函数内使用）
        // 遍历 source 平台每个好友：跳过自己；对方须有 target 平台账号、与目标尚未互为好友、且未被重复推荐
        for (const auto &peerKey : sourceDirectory.peers(sourceKey)) {
            const auto peerUser = resolverFanLX.userForAccount(peerKey);
            if (peerUser == self)
                continue;
            const auto targetPeer = resolverFanLX.verifiedTarget(peerKey, source.service(), target.service());
            if (targetPeer.empty() || targetDirectory.contains(targetKey, targetPeer) ||
                !seen.insert(targetPeer).second)
                continue;
            out.push_back(targetPeer);
        }
        std::sort(out.begin(), out.end());
        return out; // 排序后返回，保证推荐结果顺序稳定
    }
};

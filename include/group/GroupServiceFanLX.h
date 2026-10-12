#pragma once
#include "group/DiscussionGroupFanLX.h"
#include "group/GroupApplicationFanLX.h"
#include "group/GroupFanLX.h"
#include "group/GroupInvitationFanLX.h"
#include "group/GroupKeyHashFanLX.h"
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

// GroupServiceFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
class GroupServiceFanLX {
    friend class SnapshotCodecFanLX;
    friend class PreparedCommitFanLX;
    std::unordered_map<GroupKeyFanLX, std::shared_ptr<GroupFanLX>, GroupKeyHashFanLX> groupsFanLX;
    // 历史片段拥有独立对象；同号群重建不会覆盖旧历史。
    std::vector<std::shared_ptr<GroupServiceFanLX>> archivesFanLX;
    // 按父群隔离工作流；同一申请者/被邀请人最多只有一条待处理记录。
    using ApplicationsFanLX = std::unordered_map<std::string, GroupApplicationFanLX>;
    using InvitationsFanLX = std::unordered_map<std::string, GroupInvitationFanLX>;
    using DiscussionsFanLX = std::unordered_map<std::string, std::shared_ptr<DiscussionGroupFanLX>>;
    std::unordered_map<GroupKeyFanLX, ApplicationsFanLX, GroupKeyHashFanLX> applicationsFanLX;
    std::unordered_map<GroupKeyFanLX, InvitationsFanLX, GroupKeyHashFanLX> invitationsFanLX;
    std::unordered_map<GroupKeyFanLX, DiscussionsFanLX, GroupKeyHashFanLX> discussionsFanLX;

    static void requireMode(const GroupFanLX &group, GroupModeFanLX mode) {
        if (group.currentMode() != mode)
            throw std::runtime_error("当前群模式不支持该加入方式");
    }
    static void requireManager(const GroupFanLX &group, const std::string &actor) {
        if (!group.hasMember(actor) ||
            (group.roleOf(actor) != GroupRoleFanLX::Owner &&
             !(group.roleOf(actor) == GroupRoleFanLX::Admin && group.policy().canAssignAdmin())))
            throw std::runtime_error("需要群主或管理员权限");
    }
    static void requireOwner(const GroupFanLX &group, const std::string &actor) {
        if (!group.hasMember(actor) || group.roleOf(actor) != GroupRoleFanLX::Owner)
            throw std::runtime_error("需要群主权限");
    }

  public:
    GroupFanLX &create(GroupKeyFanLX key) {
        auto policy = key.mode == GroupModeFanLX::QQ
                          ? std::unique_ptr<GroupPolicyFanLX>(new QQGroupPolicyFanLX())
                          : std::unique_ptr<GroupPolicyFanLX>(new WechatGroupPolicyFanLX());
        auto [it, ok] = groupsFanLX.emplace(key, std::make_shared<GroupFanLX>(key, std::move(policy)));
        if (!ok)
            throw std::runtime_error("群已存在");
        return *it->second;
    }
    GroupFanLX &get(const GroupKeyFanLX &key) {
        return *groupsFanLX.at(key);
    }
    const GroupFanLX &get(const GroupKeyFanLX &key) const {
        return *groupsFanLX.at(key);
    }

    void applyToJoin(const GroupKeyFanLX &key, const std::string &applicant) {
        const auto &group = get(key);
        requireMode(group, GroupModeFanLX::QQ);
        if (!group.hasMember(applicant) &&
            (!applicationsFanLX[key].count(applicant) || !applicationsFanLX[key].at(applicant).pending))
            applicationsFanLX[key].insert_or_assign(
                applicant, GroupApplicationFanLX{applicant, true, "Pending", {}, {}});
    }

    // QQ 群的"邀请他人"：成员替对方提交入群申请，仍由群主/管理员审批（保持申请-审批制，
    // 与微信的"推荐-本人确认"区分开）。已是成员或已有待处理申请时不重复创建。
    void inviteToApply(const GroupKeyFanLX &key, const std::string &inviter, const std::string &invitee) {
        const auto &group = get(key);
        requireMode(group, GroupModeFanLX::QQ);
        if (!group.hasMember(inviter))
            throw std::runtime_error("邀请人必须是群成员");
        applyToJoin(key, invitee);
    }

    bool approveApplication(const GroupKeyFanLX &key, const std::string &actor,
                            const std::string &applicant) {
        auto &group = get(key);
        requireMode(group, GroupModeFanLX::QQ);
        requireManager(group, actor);
        auto pending = applicationsFanLX.find(key);
        if (pending == applicationsFanLX.end() ||
            (!pending->second.count(applicant) || !pending->second.at(applicant).pending))
            return false;
        // 先入群再消费申请，分配失败时仍可重试审批。
        group.addMember(applicant);
        pending->second.at(applicant).pending = false;
        pending->second.at(applicant).status = "Approved";
        return true;
    }

    bool rejectApplication(const GroupKeyFanLX &key, const std::string &actor, const std::string &applicant) {
        const auto &group = get(key);
        requireMode(group, GroupModeFanLX::QQ);
        requireManager(group, actor);
        auto pending = applicationsFanLX.find(key);
        if (pending == applicationsFanLX.end() || !pending->second.count(applicant) ||
            !pending->second.at(applicant).pending)
            return false;
        pending->second.at(applicant).pending = false;
        pending->second.at(applicant).status = "Rejected";
        return true;
    }

    void inviteMember(const GroupKeyFanLX &key, const std::string &inviter, const std::string &invitee) {
        const auto &group = get(key);
        requireMode(group, GroupModeFanLX::Wechat);
        if (!group.hasMember(inviter))
            throw std::runtime_error("邀请人必须是群成员");
        if (!group.hasMember(invitee) &&
            (!invitationsFanLX[key].count(invitee) || !invitationsFanLX[key].at(invitee).pending))
            invitationsFanLX[key].insert_or_assign(
                invitee, GroupInvitationFanLX{inviter, invitee, true, "Pending", {}, {}});
    }

    bool confirmInvitation(const GroupKeyFanLX &key, const std::string &invitee) {
        auto &group = get(key);
        requireMode(group, GroupModeFanLX::Wechat);
        auto pending = invitationsFanLX.find(key);
        if (pending == invitationsFanLX.end())
            return false;
        auto invitation = pending->second.find(invitee);
        if (invitation == pending->second.end() || !invitation->second.pending)
            return false;
        // 即使邀请人通过底层接口离群，确认时仍要重新验证资格。
        if (!group.hasMember(invitation->second.inviter)) {
            invitation->second.pending = false;
            invitation->second.status = "Cancelled";
            return false;
        }
        group.addMember(invitee);
        invitation->second.pending = false;
        invitation->second.status = "Approved";
        return true;
    }

    bool rejectInvitation(const GroupKeyFanLX &key, const std::string &invitee) {
        const auto &group = get(key);
        requireMode(group, GroupModeFanLX::Wechat);
        auto pending = invitationsFanLX.find(key);
        if (pending == invitationsFanLX.end() || !pending->second.count(invitee) ||
            !pending->second.at(invitee).pending)
            return false;
        pending->second.at(invitee).pending = false;
        pending->second.at(invitee).status = "Rejected";
        return true;
    }

    bool leave(const GroupKeyFanLX &key, const std::string &userId) {
        if (!get(key).leave(userId))
            return false;
        cleanupDeparted(key, userId);
        return true;
    }

    void cleanupDeparted(const GroupKeyFanLX &key, const std::string &userId) {
        auto children = discussionsFanLX.find(key);
        if (children != discussionsFanLX.end()) {
            for (auto &child : children->second) {
                const bool wasOwner = child.second->owner() == userId;
                child.second->removeMember(userId);
                // 子群负责人离开时优先交给仍在子群的父群群主，否则沿用字典序接任者。
                if (wasOwner)
                    for (const auto &member : get(key).members())
                        if (get(key).roleOf(member) == GroupRoleFanLX::Owner)
                            child.second->preferOwner(member);
            }
        }
        auto pending = invitationsFanLX.find(key);
        if (pending != invitationsFanLX.end()) {
            for (auto it = pending->second.begin(); it != pending->second.end();) {
                if (it->second.inviter == userId && it->second.pending) {
                    it->second.pending = false;
                    it->second.status = "Cancelled";
                }
                ++it;
            }
        }
    }

    bool removeMember(const GroupKeyFanLX &key, const std::string &actor, const std::string &target) {
        if (!get(key).removeMember(actor, target))
            return false;
        cleanupDeparted(key, target);
        return true;
    }

    std::size_t archivedGroupCount() const noexcept {
        return archivesFanLX.size();
    }
    // 只读枚举：按群键字典序返回当前存续群键，供"服务器授权目录"等只读场景使用。
    // 不暴露群对象引用，避免调用方绕过服务层直接改成员。
    std::vector<GroupKeyFanLX> groupKeys() const {
        std::vector<GroupKeyFanLX> keys;
        keys.reserve(groupsFanLX.size());
        for (const auto &entry : groupsFanLX)
            keys.push_back(entry.first);
        std::sort(keys.begin(), keys.end(), [](const GroupKeyFanLX &left, const GroupKeyFanLX &right) {
            if (left.mode != right.mode) return left.mode < right.mode;
            return left.id < right.id;
        });
        return keys;
    }
    void expirePending(const std::string &now) {
        // 仅在写事务候选内关闭过期记录；只读加载绝不隐式改写历史。
        for (auto &parent : applicationsFanLX)
            for (auto &item : parent.second) {
                auto &a = item.second;
                if (a.pending && a.expiresAtUtc && *a.expiresAtUtc <= now) {
                    a.pending = false;
                    a.status = "Expired";
                }
            }
        for (auto &parent : invitationsFanLX)
            for (auto &item : parent.second) {
                auto &a = item.second;
                if (a.pending && a.expiresAtUtc && *a.expiresAtUtc <= now) {
                    a.pending = false;
                    a.status = "Expired";
                }
            }
    }

    void dissolve(const GroupKeyFanLX &key, const std::string &actor) {
        requireOwner(get(key), actor);
        // 候选事务中保留独立历史片段，再从活动注册表移除。
        auto history = std::make_shared<GroupServiceFanLX>();
        history->groupsFanLX.emplace(key, groupsFanLX.at(key));
        if (applicationsFanLX.count(key))
            history->applicationsFanLX.emplace(key, applicationsFanLX.at(key));
        if (invitationsFanLX.count(key))
            history->invitationsFanLX.emplace(key, invitationsFanLX.at(key));
        if (discussionsFanLX.count(key)) {
            history->discussionsFanLX.emplace(key, discussionsFanLX.at(key));
            for (auto &item : history->discussionsFanLX.at(key))
                item.second->archive();
        }
        archivesFanLX.push_back(std::move(history));
        discussionsFanLX.erase(key);
        applicationsFanLX.erase(key);
        invitationsFanLX.erase(key);
        groupsFanLX.erase(key);
    }

    void transferOwnership(const GroupKeyFanLX &key, const std::string &actor, const std::string &newOwner) {
        auto &group = get(key);
        requireOwner(group, actor);
        group.transferOwnership(newOwner);
    }

    DiscussionGroupFanLX &createDiscussion(const GroupKeyFanLX &parentKey, const std::string &discId,
                                           const std::string &actor) {
        const auto &group = get(parentKey);
        requireManager(group, actor);
        if (!group.canCreateDiscussion())
            throw std::runtime_error("当前策略禁止子群");
        auto result =
            discussionsFanLX[parentKey].try_emplace(discId, std::make_shared<DiscussionGroupFanLX>(discId));
        if (!result.second)
            throw std::runtime_error("子群已存在");
        return *result.first->second;
    }

    DiscussionGroupFanLX &getDiscussion(const GroupKeyFanLX &parentKey, const std::string &discId) {
        get(parentKey);
        return *discussionsFanLX.at(parentKey).at(discId);
    }
    const DiscussionGroupFanLX &getDiscussion(const GroupKeyFanLX &parentKey, const std::string &discId) const {
        get(parentKey);
        return *discussionsFanLX.at(parentKey).at(discId);
    }

    void switchPolicy(const GroupKeyFanLX &key, const std::string &actor,
                      std::unique_ptr<GroupPolicyFanLX> newPolicy) {
        auto &group = get(key);
        requireOwner(group, actor);
        group.switchPolicy(std::move(newPolicy));
    }

    void switchToQQ(const GroupKeyFanLX &key, const std::string &actor) {
        switchPolicy(key, actor, std::make_unique<QQGroupPolicyFanLX>());
    }

    void switchToWechat(const GroupKeyFanLX &key, const std::string &actor) {
        switchPolicy(key, actor, std::make_unique<WechatGroupPolicyFanLX>());
    }

    void assignAdmin(const GroupKeyFanLX &key, const std::string &actor, const std::string &target) {
        auto &group = get(key);
        requireOwner(group, actor);
        if (!group.policy().canAssignAdmin())
            throw std::runtime_error("当前模式不支持设置管理员");
        // 群主变更必须通过转让入口，不能用任命操作意外创建无主群。
        if (group.roleOf(target) == GroupRoleFanLX::Owner)
            throw std::runtime_error("不能将群主设为管理员");
        group.setRole(target, GroupRoleFanLX::Admin);
    }

    void revokeAdmin(const GroupKeyFanLX &key, const std::string &actor, const std::string &target) {
        auto &group = get(key);
        requireOwner(group, actor);
        if (group.roleOf(target) != GroupRoleFanLX::Admin)
            throw std::runtime_error("目标不是管理员");
        // 微信模式仍允许撤销休眠配置，回切 QQ 时不会恢复已撤销权限。
        group.setRole(target, GroupRoleFanLX::Member);
    }

    bool joinDiscussion(const GroupKeyFanLX &parentKey, const std::string &discId, const std::string &actor,
                        const std::string &target) {
        auto &group = get(parentKey);
        requireManager(group, actor);
        if (!group.canCreateDiscussion())
            throw std::runtime_error("当前策略禁止子群扩员");
        return getDiscussion(parentKey, discId).addMember(group, target);
    }

    // 只读计数用于界面及幂等性验收，不暴露可写的待处理容器。
    std::size_t pendingApplicationCount(const GroupKeyFanLX &key) const {
        get(key);
        auto it = applicationsFanLX.find(key);
        return it == applicationsFanLX.end()
                   ? 0
                   : static_cast<std::size_t>(std::count_if(it->second.begin(), it->second.end(),
                                                            [](const auto &x) { return x.second.pending; }));
    }
    std::size_t pendingInvitationCount(const GroupKeyFanLX &key) const {
        get(key);
        auto it = invitationsFanLX.find(key);
        return it == invitationsFanLX.end()
                   ? 0
                   : static_cast<std::size_t>(std::count_if(it->second.begin(), it->second.end(),
                                                            [](const auto &x) { return x.second.pending; }));
    }
};

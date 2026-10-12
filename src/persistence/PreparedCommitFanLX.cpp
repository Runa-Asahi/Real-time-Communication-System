#include "persistence/PreparedCommitFanLX.h"

PreparedCommitFanLX::PreparedCommitFanLX(PlatformStateFanLX &live, PlatformStateFanLX &candidate)
    : liveFanLX(live), candidateFanLX(candidate) {
    for (auto &item : candidate.groups.groupsFanLX) {
        auto old = live.groups.groupsFanLX.find(item.first);
        if (old == live.groups.groupsFanLX.end())
            continue;
        groupsFanLX.emplace_back(old->second.get(), item.second.get());
        heldGroupsFanLX.push_back(item.second);
        item.second = old->second;
    }
    for (auto &parent : candidate.groups.discussionsFanLX) {
        auto oldParent = live.groups.discussionsFanLX.find(parent.first);
        if (oldParent == live.groups.discussionsFanLX.end())
            continue;
        for (auto &item : parent.second) {
            auto old = oldParent->second.find(item.first);
            if (old == oldParent->second.end())
                continue;
            discussionsFanLX.emplace_back(old->second.get(), item.second.get());
            heldDiscussionsFanLX.push_back(item.second);
            item.second = old->second;
        }
    }
}
void PreparedCommitFanLX::publish() noexcept {
    for (const auto &pair : groupsFanLX) {
        pair.first->stateFanLX.swap(pair.second->stateFanLX);
        pair.first->nameFanLX.swap(pair.second->nameFanLX);
        pair.first->policyFanLX.swap(pair.second->policyFanLX);
    }
    for (const auto &pair : discussionsFanLX) {
        static_assert(noexcept(pair.first->membersFanLX.swap(pair.second->membersFanLX)), "子群交换须无异常");
        pair.first->membersFanLX.swap(pair.second->membersFanLX);
        pair.first->ownerFanLX.swap(pair.second->ownerFanLX);
        std::swap(pair.first->archivedFanLX, pair.second->archivedFanLX);
    }
    auto &a = liveFanLX.accounts;
    auto &b = candidateFanLX.accounts;
    // 这些是固定默认分配器的 STL 容器；每一类交换均经编译期校验。
    static_assert(noexcept(a.usersFanLX.swap(b.usersFanLX)), "用户交换须无异常");
    static_assert(noexcept(a.accountsFanLX.swap(b.accountsFanLX)), "账号交换须无异常");
    static_assert(noexcept(a.accountIdIndexFanLX.swap(b.accountIdIndexFanLX)), "索引交换须无异常");
    static_assert(noexcept(a.subscriptionsFanLX.swap(b.subscriptionsFanLX)), "订阅交换须无异常");
    static_assert(noexcept(a.bindingsFanLX.swap(b.bindingsFanLX)), "绑定交换须无异常");
    static_assert(noexcept(a.loggedInFanLX.swap(b.loggedInFanLX)), "会话交换须无异常");
    a.usersFanLX.swap(b.usersFanLX);
    a.accountsFanLX.swap(b.accountsFanLX);
    a.accountIdIndexFanLX.swap(b.accountIdIndexFanLX);
    a.subscriptionsFanLX.swap(b.subscriptionsFanLX);
    a.bindingsFanLX.swap(b.bindingsFanLX);
    a.loggedInFanLX.swap(b.loggedInFanLX);
    auto &f = liveFanLX.friends;
    auto &cf = candidateFanLX.friends;
    // 好友目录按服务分区存放，因此整体交换分区表即可（各服务的目录一次性换完）。
    static_assert(noexcept(f.directoriesFanLX.swap(cf.directoriesFanLX)), "好友分区交换须无异常");
    f.directoriesFanLX.swap(cf.directoriesFanLX);
    auto &g = liveFanLX.groups;
    auto &cg = candidateFanLX.groups;
    static_assert(noexcept(g.groupsFanLX.swap(cg.groupsFanLX)), "群注册表交换须无异常");
    static_assert(noexcept(g.discussionsFanLX.swap(cg.discussionsFanLX)), "子群注册表交换须无异常");
    static_assert(noexcept(g.applicationsFanLX.swap(cg.applicationsFanLX)), "工作流交换须无异常");
    static_assert(noexcept(g.invitationsFanLX.swap(cg.invitationsFanLX)), "邀请交换须无异常");
    static_assert(noexcept(g.archivesFanLX.swap(cg.archivesFanLX)), "归档交换须无异常");
    static_assert(noexcept(liveFanLX.versions.swap(candidateFanLX.versions)), "版本交换须无异常");
    g.groupsFanLX.swap(cg.groupsFanLX);
    g.discussionsFanLX.swap(cg.discussionsFanLX);
    g.applicationsFanLX.swap(cg.applicationsFanLX);
    g.invitationsFanLX.swap(cg.invitationsFanLX);
    g.archivesFanLX.swap(cg.archivesFanLX);
    liveFanLX.generation = candidateFanLX.generation;
    liveFanLX.parentGeneration = candidateFanLX.parentGeneration;
    liveFanLX.committedAtUtc.swap(candidateFanLX.committedAtUtc);
    liveFanLX.versions.swap(candidateFanLX.versions);
}

#include "persistence/TransactionCoordinatorFanLX.h"
#include "persistence/PersistenceErrorFanLX.h"
#include "persistence/PreparedCommitFanLX.h"
#include "persistence/SchemaValidatorFanLX.h"
#include "persistence/SnapshotCodecFanLX.h"
#include <limits>

TransactionCoordinatorFanLX::TransactionCoordinatorFanLX(RepositoryFanLX &repository,
                                                         PlatformStateFanLX &state,
                                                         std::function<std::string()> clock,
                                                         CommitHookFanLX hook)
    : repositoryFanLX(repository), stateFanLX(state), clockFanLX(std::move(clock)),
      hookFanLX(std::move(hook)) {}

bool TransactionCoordinatorFanLX::execute(const std::function<void(PlatformStateFanLX &)> &mutation,
                                          bool force) {
    std::lock_guard<std::mutex> guard(mutexFanLX);
    if (!repositoryFanLX.writable())
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::RecoveryRequired, "需要初始化或确认恢复");
    auto before = SnapshotCodecFanLX::capture(stateFanLX);
    auto candidate = SnapshotCodecFanLX::build(before);
    SnapshotCodecFanLX::copySessions(candidate, stateFanLX);
    const auto now = clockFanLX();
    candidate.groups.expirePending(now);
    mutation(candidate);
    auto after = SnapshotCodecFanLX::capture(candidate);
    const auto oldJson = SnapshotCodecFanLX::encode(before);
    if (!force && oldJson == SnapshotCodecFanLX::encode(after))
        return false;
    if (before.generation == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("提交代数耗尽");
    // 新入群时间在事务内统一赋值，旧的未知历史时间保持 null。
    for (auto &group : after.groups) {
        const GroupRecordFanLX *old = nullptr;
        for (const auto &g : before.groups)
            if (g.key == group.key) {
                old = &g;
                break;
            }
        for (auto &m : group.state.members)
            if ((!old || m.second.joinOrder >= old->state.nextJoinOrder) && !m.second.joinedAtUtc)
                m.second.joinedAtUtc = now;
        for (auto &a : group.applications)
            if (a.pending && !a.createdAtUtc)
                a.createdAtUtc = now;
        for (auto &a : group.invitations)
            if (a.pending && !a.createdAtUtc)
                a.createdAtUtc = now;
    }
    after.parentGeneration = before.generation;
    after.generation = before.generation + 1;
    after.committedAtUtc = now;
    auto newJson = SnapshotCodecFanLX::encode(after);
    auto bump = [&](const std::string &key) { after.versions[key] = after.generation; };
    for (const auto &field : {"users", "accounts", "subscriptions", "bindings"})
        if (oldJson[field] != newJson[field])
            bump(field);
    // 每个账号邻接/备注独立版本。JSON 引用作为结构化键编码，避免拼接碰撞。
    // schemaVersion 2 起好友按服务分区，因此先遍历分区再遍历分区内的关系。
    auto friendViews = [](const auto &json) {
        std::map<std::string, nlohmann::json> views;
        for (auto section = json.begin(); section != json.end(); ++section) {
            for (const auto &f : section.value()) {
                const auto l = f["left"].dump(), r = f["right"].dump();
                views["friends/" + l].push_back(f["right"]);
                views["friends/" + r].push_back(f["left"]);
                views["remarks/" + l].push_back({f["right"], f["leftRemark"], f["leftTag"]});
                views["remarks/" + r].push_back({f["left"], f["rightRemark"], f["rightTag"]});
            }
        }
        return views;
    };
    auto oldViews = friendViews(oldJson["friends"]), newViews = friendViews(newJson["friends"]);
    for (const auto &item : oldViews)
        if (newViews[item.first] != item.second)
            bump(item.first);
    for (const auto &item : newViews)
        if (oldViews[item.first] != item.second)
            bump(item.first);
    auto collectGroups = [](const auto &json, auto &out) {
        for (auto section = json.begin(); section != json.end(); ++section)
            for (const auto &g : section.value())
                out[g["key"].dump()] = g;
    };
    std::map<std::string, nlohmann::json> oldGroups, newGroups;
    collectGroups(oldJson["groups"], oldGroups);
    collectGroups(newJson["groups"], newGroups);
    for (const auto &item : newGroups) {
        const auto &old = oldGroups[item.first];
        const auto &next = item.second;
        for (const auto &field :
             {"members", "currentMode", "applications", "invitations", "discussions", "name"})
            if (old.is_null() || old.at(field) != next.at(field))
                bump("group/" + item.first + "/" + field);
        if (old.is_null() || old.at("members") != next.at("members") ||
            old.at("currentMode") != next.at("currentMode"))
            bump("group/" + item.first + "/roles");
    }
    for (const auto &item : oldGroups)
        if (!newGroups.count(item.first))
            bump("group/" + item.first + "/deleted");
    SchemaValidatorFanLX::validate(after);
    auto preparedState = SnapshotCodecFanLX::build(after);
    SnapshotCodecFanLX::copySessions(preparedState, candidate);
    PreparedCommitFanLX prepared(stateFanLX, preparedState);
    repositoryFanLX.commit(after);
    // 提交点之后任何测试/诊断异常都不能误报回滚；真正终止进程由重启恢复新代。
    try {
        if (hookFanLX)
            hookFanLX(CommitStageFanLX::BeforeMemoryPublish);
    } catch (...) {
    }
    prepared.publish();
    try {
        if (hookFanLX)
            hookFanLX(CommitStageFanLX::BeforeReply);
    } catch (...) {
    }
    return true;
}

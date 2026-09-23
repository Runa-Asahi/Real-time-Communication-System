#include "account/AccountSessionFanLX.h"
#include "friend/FriendServiceFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include "persistence/PersistenceErrorFanLX.h"
#include "friend/IdentityResolverFanLX.h"

PersistenceApplicationFanLX::PersistenceApplicationFanLX(const std::filesystem::path &directory,
                                                         std::function<std::string()> clock,
                                                         CommitHookFanLX hook)
    : repositoryFanLX(directory, hook),
      transactionsFanLX(repositoryFanLX, stateFanLX, std::move(clock), std::move(hook)) {
    loadFanLX = repositoryFanLX.load();
    if (loadFanLX.status == LoadStatusFanLX::Loaded || loadFanLX.status == LoadStatusFanLX::RecoveredReadOnly)
        stateFanLX = SnapshotCodecFanLX::build(loadFanLX.snapshot);
}
void PersistenceApplicationFanLX::requireReady() const {
    if (closedFanLX || !repositoryFanLX.writable())
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::RecoveryRequired, "当前只读/未初始化/已关闭");
}
void PersistenceApplicationFanLX::initialize() {
    repositoryFanLX.initialize();
    transactionsFanLX.execute([](auto &) {}, true);
}
void PersistenceApplicationFanLX::confirmRecovery() {
    // 恢复只在未进入可写状态时执行；旧会话清空。
    if (repositoryFanLX.writable())
        throw std::runtime_error("当前无需恢复");
    loadFanLX = repositoryFanLX.recover();
    try {
        if (loadFanLX.status != LoadStatusFanLX::Loaded)
            throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::RecoveryRequired,
                                        "恢复后未得到有效主快照");
        stateFanLX = SnapshotCodecFanLX::build(loadFanLX.snapshot);
        queryCacheFanLX.clear();
    } catch (...) {
        // 磁盘恢复后，领域重建仍可能因分配失败而中止。关闭仓储，禁止旧内存覆盖新主文件；
        // 调用方重新启动即可再次加载已恢复快照。提交前的恢复错误仍可原地重试。
        repositoryFanLX.close();
        closedFanLX = true;
        throw;
    }
}
void PersistenceApplicationFanLX::saveAndClose() {
    requireReady();
    transactionsFanLX.execute([](auto &) {});
    repositoryFanLX.close();
    closedFanLX = true;
}
void PersistenceApplicationFanLX::importInitial(const PlatformSnapshotFanLX &seed) {
    requireReady();
    if (!snapshot().users.empty() || !snapshot().groups.empty())
        throw std::runtime_error("仅允许空库导入初始数据");
    transactionsFanLX.execute([&](auto &candidate) {
        auto imported = SnapshotCodecFanLX::build(seed);
        imported.generation = candidate.generation;
        imported.parentGeneration = candidate.parentGeneration;
        imported.committedAtUtc = candidate.committedAtUtc;
        candidate = std::move(imported);
    });
}
AccountSessionFanLX PersistenceApplicationFanLX::session(PlatformStateFanLX &s,
                                                         const AccountRefFanLX &actor) const {
    if (!s.accounts.isAccountLoggedIn(actor.service, actor.accountId))
        throw SessionExpiredFanLX("服务未登录");
    return s.accounts.login(actor.service, actor.accountId, false);
}
std::string PersistenceApplicationFanLX::actorUser(PlatformStateFanLX &s, const AccountRefFanLX &actor,
                                                   const GroupKeyFanLX &key) const {
    session(s, actor);
    const auto service = key.mode == GroupModeFanLX::QQ ? ServiceTypeFanLX::QQ : ServiceTypeFanLX::Wechat;
    if (actor.service != service)
        throw std::runtime_error("操作者账号不属于群命名空间");
    return s.accounts.accountByAccountId(actor.service, actor.accountId).userId;
}
void PersistenceApplicationFanLX::registerUser(UserProfileFanLX u,
                                               std::vector<ServiceAccountFanLX> accounts) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        s.accounts.addUser(u);
        for (const auto &a : accounts) {
            if (a.userId != u.userId)
                throw std::runtime_error("注册账号归属不一致");
            s.accounts.addAccount(a);
        }
        // 注册时首个账号是调用方选择的初始服务，其余账号保留为未开通。
        if (!accounts.empty())
            s.accounts.subscribe(u.userId, accounts.front().service);
    });
}
void PersistenceApplicationFanLX::login(AccountRefFanLX actor, bool confirm) {
    requireReady();
    stateFanLX.accounts.login(actor.service, actor.accountId, confirm);
}
void PersistenceApplicationFanLX::logout(AccountRefFanLX actor, bool all) {
    requireReady();
    if (all)
        stateFanLX.accounts.logoutAll(actor.service, actor.accountId);
    else
        stateFanLX.accounts.logout(actor.service, actor.accountId);
}
void PersistenceApplicationFanLX::subscribe(AccountRefFanLX actor, ServiceTypeFanLX target) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) { session(s, actor).subscribeTo(target); });
}
void PersistenceApplicationFanLX::bind(AccountRefFanLX actor, AccountRefFanLX target, bool remove) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        auto self = session(s, actor);
        if (remove)
            self.unbindFrom(target.service, target.accountId);
        else
            self.bindTo(target.service, target.accountId);
    });
}
void PersistenceApplicationFanLX::updateProfile(AccountRefFanLX actor, std::string nickname,
                                                std::string location) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        session(s, actor);
        s.accounts.updateProfile(s.accounts.accountByAccountId(actor.service, actor.accountId).userId,
                                 nickname, location);
    });
}
void PersistenceApplicationFanLX::addFriend(AccountRefFanLX actor, AccountRefFanLX peer) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        FriendServiceFanLX f(s.friends, s.accounts);
        f.addFriend(session(s, actor), peer.service, peer.accountId);
    });
}
void PersistenceApplicationFanLX::removeFriend(AccountRefFanLX actor, AccountRefFanLX peer) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        FriendServiceFanLX f(s.friends, s.accounts);
        f.removeFriend(session(s, actor), peer.service, peer.accountId);
    });
}
void PersistenceApplicationFanLX::updateRemark(AccountRefFanLX actor, AccountRefFanLX peer,
                                               std::string remark, std::string tag) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        FriendServiceFanLX f(s.friends, s.accounts);
        f.updateRemark(session(s, actor), peer.service, peer.accountId, remark, tag);
    });
}
void PersistenceApplicationFanLX::createGroup(AccountRefFanLX actor, GroupKeyFanLX key) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        auto user = actorUser(s, actor, key);
        s.groups.create(key).addMember(user, GroupRoleFanLX::Owner);
    });
}
void PersistenceApplicationFanLX::applyToJoin(AccountRefFanLX actor, GroupKeyFanLX key) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) { s.groups.applyToJoin(key, actorUser(s, actor, key)); });
}
void PersistenceApplicationFanLX::approveApplication(AccountRefFanLX actor, GroupKeyFanLX key,
                                                     AccountRefFanLX target, bool reject) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (target.service != actor.service)
            throw std::runtime_error("目标服务不匹配");
        const auto peer = s.accounts.accountByAccountId(target.service, target.accountId).userId;
        if (reject)
            s.groups.rejectApplication(key, user, peer);
        else
            s.groups.approveApplication(key, user, peer);
    });
}
void PersistenceApplicationFanLX::inviteMember(AccountRefFanLX actor, GroupKeyFanLX key,
                                               AccountRefFanLX target) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (target.service != actor.service)
            throw std::runtime_error("目标服务不匹配");
        s.groups.inviteMember(key, user,
                              s.accounts.accountByAccountId(target.service, target.accountId).userId);
    });
}
void PersistenceApplicationFanLX::confirmInvitation(AccountRefFanLX actor, GroupKeyFanLX key, bool reject) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (reject)
            s.groups.rejectInvitation(key, user);
        else
            s.groups.confirmInvitation(key, user);
    });
}
void PersistenceApplicationFanLX::leave(AccountRefFanLX actor, GroupKeyFanLX key) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) { s.groups.leave(key, actorUser(s, actor, key)); });
}
void PersistenceApplicationFanLX::kick(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX target) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (target.service != actor.service)
            throw std::runtime_error("目标服务不匹配");
        if (!s.groups.removeMember(key, user,
                                   s.accounts.accountByAccountId(target.service, target.accountId).userId))
            throw std::runtime_error("踢人被权限规则拒绝");
    });
}
void PersistenceApplicationFanLX::dissolve(AccountRefFanLX actor, GroupKeyFanLX key) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) { s.groups.dissolve(key, actorUser(s, actor, key)); });
}
void PersistenceApplicationFanLX::transferOwnership(AccountRefFanLX actor, GroupKeyFanLX key,
                                                    AccountRefFanLX target) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (target.service != actor.service)
            throw std::runtime_error("目标服务不匹配");
        s.groups.transferOwnership(key, user,
                                   s.accounts.accountByAccountId(target.service, target.accountId).userId);
    });
}
void PersistenceApplicationFanLX::assignAdmin(AccountRefFanLX actor, GroupKeyFanLX key,
                                              AccountRefFanLX target, bool revoke) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (target.service != actor.service)
            throw std::runtime_error("目标服务不匹配");
        const auto peer = s.accounts.accountByAccountId(target.service, target.accountId).userId;
        if (revoke)
            s.groups.revokeAdmin(key, user, peer);
        else
            s.groups.assignAdmin(key, user, peer);
    });
}
void PersistenceApplicationFanLX::switchPolicy(AccountRefFanLX actor, GroupKeyFanLX key,
                                               GroupModeFanLX mode) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (mode == GroupModeFanLX::QQ)
            s.groups.switchToQQ(key, user);
        else if (mode == GroupModeFanLX::Wechat)
            s.groups.switchToWechat(key, user);
        else
            throw std::runtime_error("非法群模式");
    });
}
void PersistenceApplicationFanLX::createDiscussion(AccountRefFanLX actor, GroupKeyFanLX key, std::string id) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        s.groups.createDiscussion(key, id, user);
        s.groups.joinDiscussion(key, id, user, user);
    });
}
void PersistenceApplicationFanLX::joinDiscussion(AccountRefFanLX actor, GroupKeyFanLX key, std::string id,
                                                 AccountRefFanLX target) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (target.service != actor.service)
            throw std::runtime_error("目标服务不匹配");
        if (!s.groups.joinDiscussion(key, id, user,
                                     s.accounts.accountByAccountId(target.service, target.accountId).userId))
            throw std::runtime_error("子群加入失败");
    });
}
void PersistenceApplicationFanLX::renameGroup(AccountRefFanLX actor, GroupKeyFanLX key, std::string name) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (s.groups.get(key).roleOf(user) != GroupRoleFanLX::Owner)
            throw std::runtime_error("需要群主权限");
        s.groups.get(key).rename(name);
    });
}

namespace {
std::uint64_t queryVersionFanLX(const PlatformStateFanLX &state, const std::string &key) {
    auto found = state.versions.find(key);
    return found == state.versions.end() ? 0 : found->second;
}
std::string accountJsonFanLX(const AccountRefFanLX &account) {
    return nlohmann::json{{"service", serviceNameFanLX(account.service)}, {"accountId", account.accountId}}.dump();
}
}
std::vector<std::string> PersistenceApplicationFanLX::queryFriendAccounts(AccountRefFanLX actor) {
    requireReady();
    session(stateFanLX, actor); // 缓存命中也必须验证当前会话。
    IdentityResolverFanLX resolver(stateFanLX.accounts);
    const auto accountKey = resolver.accountKey(actor.service, actor.accountId);
    const auto ref = accountJsonFanLX(actor);
    const auto key = nlohmann::json{"friends", ref,
        queryVersionFanLX(stateFanLX, "friends/" + ref)}.dump();
    if (auto cached = queryCacheFanLX.get(key)) return *cached;
    std::vector<std::string> result;
    for (const auto &peer : stateFanLX.friends.peers(accountKey))
        result.push_back(stateFanLX.accounts.account(resolver.userForAccount(peer), actor.service).accountId);
    std::sort(result.begin(), result.end());
    queryCacheFanLX.put(key, result);
    return result;
}
std::vector<std::string> PersistenceApplicationFanLX::queryGroupMembers(AccountRefFanLX actor,
                                                                      const GroupKeyFanLX &key) {
    requireReady();
    const auto user = actorUser(stateFanLX, actor, key);
    const auto &group = stateFanLX.groups.get(key);
    if (!group.hasMember(user)) throw std::runtime_error("只有群成员可以查询群成员");
    // 权限直接查询在线状态，必须位于缓存 get 之前，防止先前成员身份命中的越权泄露。
    const auto groupRef = nlohmann::json{{"namespaceMode", key.mode == GroupModeFanLX::QQ ? "QQ" : "Wechat"},
                                       {"id", key.id}}.dump();
    const auto prefix = "group/" + groupRef;
    const auto cacheKey = nlohmann::json{"group-members", groupRef,
        queryVersionFanLX(stateFanLX, prefix + "/members"),
        queryVersionFanLX(stateFanLX, prefix + "/deleted")}.dump();
    if (auto cached = queryCacheFanLX.get(cacheKey)) return *cached;
    std::vector<std::string> result;
    for (const auto &member : group.members())
        result.push_back(stateFanLX.accounts.account(member, actor.service).accountId);
    std::sort(result.begin(), result.end());
    queryCacheFanLX.put(cacheKey, result);
    return result;
}

void PersistenceApplicationFanLX::confirmLogin(AccountRefFanLX actor, AccountRefFanLX target) {
    requireReady();
    session(stateFanLX, actor);
    const auto &from = stateFanLX.accounts.accountByAccountId(actor.service, actor.accountId);
    const auto &to = stateFanLX.accounts.accountByAccountId(target.service, target.accountId);
    if (from.userId != to.userId ||
        (!(from.sharedId && to.sharedId && from.accountId == to.accountId) &&
         !stateFanLX.accounts.isBound(actor.service, actor.accountId, target.service, target.accountId)))
        throw std::runtime_error("逐项确认需要本人的共享号码或已验证绑定");
    stateFanLX.accounts.login(target.service, target.accountId, false);
}
std::vector<std::string> PersistenceApplicationFanLX::queryCommonFriends(AccountRefFanLX actor,
                                                                         AccountRefFanLX peer) {
    requireReady();
    auto self = session(stateFanLX, actor);
    if (actor.service != peer.service) throw std::runtime_error("共同好友需在同一平台查询");
    stateFanLX.accounts.accountByAccountId(peer.service, peer.accountId);
    const auto a = accountJsonFanLX(actor), b = accountJsonFanLX(peer);
    const auto key = nlohmann::json{"common", a, b, queryVersionFanLX(stateFanLX, "friends/" + a),
                                    queryVersionFanLX(stateFanLX, "friends/" + b)}.dump();
    if (auto cached = queryCacheFanLX.get(key)) return *cached;
    FriendServiceFanLX friends(stateFanLX.friends, stateFanLX.accounts);
    IdentityResolverFanLX resolver(stateFanLX.accounts);
    std::vector<std::string> result;
    for (const auto &entry : friends.commonFriends(self, peer.service, peer.accountId))
        result.push_back(stateFanLX.accounts.account(resolver.userForAccount(entry), actor.service).accountId);
    std::sort(result.begin(), result.end());
    queryCacheFanLX.put(key, result);
    return result;
}
std::vector<std::string> PersistenceApplicationFanLX::queryCrossService(AccountRefFanLX source,
                                               AccountRefFanLX target, bool recommendations) {
    requireReady();
    auto from = session(stateFanLX, source), to = session(stateFanLX, target);
    if (source.service == target.service ||
        stateFanLX.accounts.accountByAccountId(source.service, source.accountId).userId !=
        stateFanLX.accounts.accountByAccountId(target.service, target.accountId).userId)
        throw std::runtime_error("跨服务查询需要本人两个不同平台的有效会话");
    const auto a = accountJsonFanLX(source), b = accountJsonFanLX(target);
    const auto key = nlohmann::json{"cross", recommendations, a, b,
        queryVersionFanLX(stateFanLX, "friends/" + a), queryVersionFanLX(stateFanLX, "friends/" + b),
        queryVersionFanLX(stateFanLX, "accounts"), queryVersionFanLX(stateFanLX, "subscriptions"),
        queryVersionFanLX(stateFanLX, "bindings")}.dump();
    if (auto cached = queryCacheFanLX.get(key)) return *cached;
    FriendServiceFanLX friends(stateFanLX.friends, stateFanLX.accounts);
    IdentityResolverFanLX resolver(stateFanLX.accounts);
    std::vector<std::string> result;
    if (recommendations) {
        for (const auto &entry : friends.recommendFriends(from, to))
            result.push_back(stateFanLX.accounts.account(resolver.userForAccount(entry), target.service).accountId);
    } else {
        for (const auto &user : friends.crossServiceCommonFriends(from, to))
            result.push_back(stateFanLX.accounts.account(user, target.service).accountId);
    }
    std::sort(result.begin(), result.end());
    queryCacheFanLX.put(key, result);
    return result;
}

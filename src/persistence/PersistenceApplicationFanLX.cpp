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

// 账号库读穿：只有磁盘内容真的变了才重新解析，避免每次认证都重建领域对象。
// 任何一步失败都保持原内存状态——宁可继续用旧快照，也不用半加载的状态覆盖在线对象。
bool PersistenceApplicationFanLX::reloadIfChangedFromDisk() {
    if (closedFanLX || !repositoryFanLX.writable())
        return false;
    if (!repositoryFanLX.changedOnDisk())
        return false;
    const auto result = repositoryFanLX.load();
    if (result.status != LoadStatusFanLX::Loaded)
        return false;
    auto rebuilt = SnapshotCodecFanLX::build(result.snapshot);
    // 登录态是内存概念（不写盘）：重载时必须保留，否则会把已登录的领域会话清掉。
    SnapshotCodecFanLX::copySessions(rebuilt, stateFanLX);
    stateFanLX = std::move(rebuilt);
    loadFanLX = result;
    queryCacheFanLX.clear();
    return true;
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
    // 会话恢复不校验密码：能走到这里的调用方都是在 login 通过之后凭登录态工作的，
    // 密码关口只在 login 那一处（未登录时 resumeSession 抛 SessionExpiredFanLX）。
    return s.accounts.resumeSession(actor.service, actor.accountId);
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
    u.password = AccountManagerFanLX::hashNewCredentialFanLX(u.password); // 口令只以加盐哈希落库
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
void PersistenceApplicationFanLX::registerWeiXUser(UserProfileFanLX user, std::string weixAccountId) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        // 注册只产生"微X + 密码"：一个真人一个微X号，登录身份就是它。
        s.accounts.registerWeiXAccount(std::move(user), std::move(weixAccountId));
    });
}
void PersistenceApplicationFanLX::activateService(AccountRefFanLX actor, ServiceTypeFanLX service,
                                                  std::string serviceAccountId) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        // 授权来源是"本人已认证的会话"；领域层再要求该会话必须是微X身份。
        session(s, actor).activateService(service, serviceAccountId);
    });
}
void PersistenceApplicationFanLX::login(AccountRefFanLX actor, const std::string &password, bool confirm) {
    requireReady();
    stateFanLX.accounts.login(actor.service, actor.accountId, password, confirm);
    // 透明迁移：口令已经校验通过，若库里仍是旧版明文就在一次事务里改写成哈希并落盘。
    // 迁移是尽力而为：存储只读或提交失败时登录照常成功，下次登录会再尝试。
    if (stateFanLX.accounts.credentialNeedsUpgradeFanLX(actor.service, actor.accountId)) {
        try {
            transactionsFanLX.execute(
                [&](auto &s) { s.accounts.upgradeCredentialFanLX(actor.service, actor.accountId, password); });
        } catch (const std::exception &) {
        }
    }
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
        // 单项修改：空串保留原值，因此"只改昵称"不会再顺手清掉所在地。
        const auto user = s.accounts.accountByAccountId(actor.service, actor.accountId).userId;
        s.accounts.updateNickname(user, std::move(nickname));
        s.accounts.updateLocation(user, std::move(location));
    });
}
void PersistenceApplicationFanLX::changePassword(AccountRefFanLX actor, const std::string &oldPassword,
                                                 std::string newPassword) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        // 不要求当前处于登录态：改密的凭据是"旧密码"，不是会话（会话可能已经过期）。
        s.accounts.changePassword(actor.service, actor.accountId, oldPassword, std::move(newPassword));
    });
}
void PersistenceApplicationFanLX::addFriend(AccountRefFanLX actor, AccountRefFanLX peer) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        FriendServiceFanLX f(s.friends, s.accounts);
        f.addFriend(session(s, actor), peer.service, peer.accountId);
    });
}
// 远端账号导入：见 AccountManagerFanLX::importRemoteAccount。走事务，因此要么整条记录落盘、
// 要么完全不变（与其它写操作同一条持久化路径）。
bool PersistenceApplicationFanLX::importRemotePeerAccount(AccountRefFanLX peer, const std::string &userId,
                                                          const std::string &nickname) {
    requireReady();
    bool created = false;
    transactionsFanLX.execute([&](auto &s) {
        created = s.accounts.importRemoteAccount(peer.service, peer.accountId, userId, nickname,
                                                utcNowFanLX().substr(0, 10));
    });
    return created;
}
// 服务端镜像一条**已经成立**的好友关系：不做"对端是否确认登录"的检查（发起端在客户端那边
// 已经校验过），只把关系写进目录，让服务端的好友权限判定能看到它。幂等：已存在则返回 false。
bool PersistenceApplicationFanLX::mirrorFriendship(AccountRefFanLX actor, AccountRefFanLX peer) {
    requireReady();
    if (actor.service != peer.service)
        throw std::runtime_error("好友关系只能建立在同一个服务里");
    bool created = false;
    transactionsFanLX.execute([&](auto &s) {
        IdentityResolverFanLX resolver(s.accounts);
        const auto left = resolver.accountKey(actor.service, actor.accountId);
        const auto right = resolver.accountKey(peer.service, peer.accountId);
        auto &directory = s.friends.directory(actor.service);
        if (directory.contains(left, right))
            return;
        if (!directory.add(left, right))
            return;
        created = true;
    });
    return created;
}
bool PersistenceApplicationFanLX::unmirrorFriendship(AccountRefFanLX actor, AccountRefFanLX peer) {
    requireReady();
    if (actor.service != peer.service)
        throw std::runtime_error("好友关系只能建立在同一个服务里");
    bool removed = false;
    transactionsFanLX.execute([&](auto &s) {
        IdentityResolverFanLX resolver(s.accounts);
        const auto left = resolver.accountKey(actor.service, actor.accountId);
        const auto right = resolver.accountKey(peer.service, peer.accountId);
        removed = s.friends.directory(actor.service).remove(left, right);
    });
    return removed;
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
void PersistenceApplicationFanLX::inviteToApply(AccountRefFanLX actor, GroupKeyFanLX key,
                                                AccountRefFanLX target) {
    requireReady();
    transactionsFanLX.execute([&](auto &s) {
        const auto user = actorUser(s, actor, key);
        if (target.service != actor.service)
            throw std::runtime_error("目标服务不匹配");
        s.groups.inviteToApply(key, user,
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
    // 只读"当前服务"那一份好友分区：切到微信看到的就是微信的好友，不会串到 QQ。
    for (const auto &peer : stateFanLX.friends.directory(actor.service).peers(accountKey))
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
    // 联动规则（同号 / 显式绑定）与授权来源（本人已认证的会话）都收敛在领域里实现，
    // 应用层不再自己拼一遍判定条件——避免两份规则随时间漂移。
    stateFanLX.accounts.confirmLoginInSession(actor.service, actor.accountId, target.service,
                                              target.accountId);
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

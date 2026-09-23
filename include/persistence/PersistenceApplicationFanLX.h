#pragma once
#include "persistence/JsonFileRepositoryFanLX.h"
#include "persistence/SnapshotCodecFanLX.h"
#include "persistence/TransactionCoordinatorFanLX.h"
#include "infra/QueryCacheFanLX.h"

// 单线程应用门面。在线聚合体只读，所有持久化写入都在候选事务中执行。
class PersistenceApplicationFanLX {
    JsonFileRepositoryFanLX repositoryFanLX;
    PlatformStateFanLX stateFanLX;
    TransactionCoordinatorFanLX transactionsFanLX;
    QueryCacheFanLX queryCacheFanLX;
    LoadResultFanLX loadFanLX;
    bool closedFanLX = false;
    void requireReady() const;
    AccountSessionFanLX session(PlatformStateFanLX &state, const AccountRefFanLX &actor) const;
    std::string actorUser(PlatformStateFanLX &state, const AccountRefFanLX &actor,
                          const GroupKeyFanLX &group) const;

  public:
    explicit PersistenceApplicationFanLX(const std::filesystem::path &directory,
                                         std::function<std::string()> clock = utcNowFanLX,
                                         CommitHookFanLX hook = {});
    const PlatformStateFanLX &state() const noexcept {
        return stateFanLX;
    }
    const LoadResultFanLX &loadResult() const noexcept {
        return loadFanLX;
    }
    PlatformSnapshotFanLX snapshot() const {
        return SnapshotCodecFanLX::capture(stateFanLX);
    }
    // 查询只返回值副本；缓存不保存在线领域对象或会话。
    std::vector<std::string> queryFriendAccounts(AccountRefFanLX actor);
    std::vector<std::string> queryGroupMembers(AccountRefFanLX actor, const GroupKeyFanLX &key);
    std::vector<std::string> queryCommonFriends(AccountRefFanLX actor, AccountRefFanLX peer);
    std::vector<std::string> queryCrossService(AccountRefFanLX source, AccountRefFanLX target,
                                               bool recommendations = true);
    void confirmLogin(AccountRefFanLX actor, AccountRefFanLX target);
    QueryCacheMetricsFanLX queryCacheMetrics() const noexcept { return queryCacheFanLX.metrics(); }
    void setQueryCacheEnabled(bool enabled) noexcept { queryCacheFanLX.setEnabled(enabled); }
    void initialize();
    void confirmRecovery();
    void saveAndClose();
    // 后台首次配给/注册入口；导入只允许空白库，普通 UI 不能批量恢复覆盖在线状态。
    void importInitial(const PlatformSnapshotFanLX &seed);
    void registerUser(UserProfileFanLX user, std::vector<ServiceAccountFanLX> accounts);
    void login(AccountRefFanLX actor, bool confirmOthers = false);
    void logout(AccountRefFanLX actor, bool all = false);
    void subscribe(AccountRefFanLX actor, ServiceTypeFanLX target);
    void bind(AccountRefFanLX actor, AccountRefFanLX target, bool remove = false);
    void updateProfile(AccountRefFanLX actor, std::string nickname, std::string location);
    void addFriend(AccountRefFanLX actor, AccountRefFanLX peer);
    void removeFriend(AccountRefFanLX actor, AccountRefFanLX peer);
    void updateRemark(AccountRefFanLX actor, AccountRefFanLX peer, std::string remark, std::string tag = {});
    void createGroup(AccountRefFanLX actor, GroupKeyFanLX key);
    void applyToJoin(AccountRefFanLX actor, GroupKeyFanLX key);
    void approveApplication(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX applicant,
                            bool reject = false);
    void inviteMember(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX invitee);
    void confirmInvitation(AccountRefFanLX actor, GroupKeyFanLX key, bool reject = false);
    void leave(AccountRefFanLX actor, GroupKeyFanLX key);
    void kick(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX target);
    void dissolve(AccountRefFanLX actor, GroupKeyFanLX key);
    void transferOwnership(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX target);
    void assignAdmin(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX target, bool revoke = false);
    void switchPolicy(AccountRefFanLX actor, GroupKeyFanLX key, GroupModeFanLX mode);
    void createDiscussion(AccountRefFanLX actor, GroupKeyFanLX key, std::string id);
    void joinDiscussion(AccountRefFanLX actor, GroupKeyFanLX key, std::string id, AccountRefFanLX target);
    void renameGroup(AccountRefFanLX actor, GroupKeyFanLX key, std::string name);
};

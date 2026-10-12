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
    // ---- 服务端无状态：账号库读穿 ----
    // 如果磁盘上的主快照与内存里的不同（被本进程之外的一次提交改过，或外部工具重写过），
    // 就重新加载进内存并清空查询缓存；否则什么也不做（只做一次读盘字节比较）。
    // 返回 true 表示确实重新加载了。内存登录态（会话）会被保留，不受重载影响。
    // 服务端在每次认证前调用它，就不会依赖"启动那一刻"的账号快照。
    bool reloadIfChangedFromDisk();
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
    // 注册微X账号：只建"微X（上层）+ 密码"，**不**捆绑任何下级服务账号。
    // 登录界面的"注册"入口走这一条，QQ/微信/微博 之后由 activateService 在内部开通。
    void registerWeiXUser(UserProfileFanLX user, std::string weixAccountId);
    // 开通下级服务（微X 上层之下）：必须由微X会话发起（session(...) 里校验登录态，
    // 领域层再校验"必须是微X身份"），建号 + 置为已开通一次事务完成。
    void activateService(AccountRefFanLX actor, ServiceTypeFanLX service, std::string serviceAccountId);
    // 登录以「平台账号 + 用户自选密码」为凭据；密码错误抛 CredentialRejectedFanLX。
    // 界面统一用微X号登录（微X 是上层身份）；下级服务登录用于"简单确认"与演示对端上线。
    void login(AccountRefFanLX actor, const std::string &password, bool confirmOthers = false);
    void logout(AccountRefFanLX actor, bool all = false);
    void subscribe(AccountRefFanLX actor, ServiceTypeFanLX target);
    void bind(AccountRefFanLX actor, AccountRefFanLX target, bool remove = false);
    // 资料维护：空串表示"这一项不改"，因此可以只改昵称或只改所在地。
    void updateProfile(AccountRefFanLX actor, std::string nickname, std::string location);
    // 改密：旧密码必须正确，新密码不允许为空；走事务入口，与资料修改同样持久化。
    void changePassword(AccountRefFanLX actor, const std::string &oldPassword, std::string newPassword);
    void addFriend(AccountRefFanLX actor, AccountRefFanLX peer);
    void removeFriend(AccountRefFanLX actor, AccountRefFanLX peer);
    // ---- 服务端登记/镜像好友关系（见 network/ServerAccountRegistrarFanLX）----
    // 与 addFriend 的区别：不检查"对端是否确认登录"。客户端加好友时已经在本机按领域规则
    // 校验过（同服务、对端确认登录），服务端只是把这条**已经成立**的关系镜像进自己的账号库，
    // 好在"是不是好友"的权限判定里看到它（客户端本机的关系不会自动出现在服务端）。
    // 返回 true 表示新建，false 表示关系已存在（幂等）。
    bool mirrorFriendship(AccountRefFanLX actor, AccountRefFanLX peer);
    // mirrorFriendship 的逆操作：服务端在"一方删除好友"时同步解除关系（不要求会话），
    // 本机客户端在"服务端已解除"时也用它对账。返回 true 表示确实删除，false 表示本来就不是好友。
    bool unmirrorFriendship(AccountRefFanLX actor, AccountRefFanLX peer);
    // 远端账号导入（添加好友前用）：把服务端确认存在的对端写进本机副本，返回是否新建了账号记录。
    // 见 AccountManagerFanLX::importRemoteAccount 的说明（密码用占位值、幂等、标记"已确认登录"）。
    bool importRemotePeerAccount(AccountRefFanLX peer, const std::string &userId,
                                 const std::string &nickname);
    void updateRemark(AccountRefFanLX actor, AccountRefFanLX peer, std::string remark, std::string tag = {});
    void createGroup(AccountRefFanLX actor, GroupKeyFanLX key);
    void applyToJoin(AccountRefFanLX actor, GroupKeyFanLX key);
    void approveApplication(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX applicant,
                            bool reject = false);
    void inviteMember(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX invitee);
    // QQ 群邀请：成员替对方提交入群申请，等待群主/管理员审批。
    void inviteToApply(AccountRefFanLX actor, GroupKeyFanLX key, AccountRefFanLX invitee);
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

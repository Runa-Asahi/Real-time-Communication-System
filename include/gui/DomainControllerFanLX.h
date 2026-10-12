#pragma once
#include "AccountDomainFanLX.h"
#include "FriendDomainFanLX.h"
#include "GroupDomainFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include "network/PeerAccountResolutionFanLX.h"
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

namespace fanlxGui {

// 服务的界面视图行：账号与服务页面"开通管理 / 登录管理"表格的一行。
// 只承载界面需要的字段，不暴露领域对象或内部 userId。
// 用户基本信息的结构化视图（profileText 的同一份数据），供界面做卡片排版，不必解析文本。
struct ProfileViewFanLX {
    bool valid = false;   // false = 还没登录 / 领域库未打开
    QString weixId;       // 微X 号
    QString service;      // 当前服务
    QString accountId;    // 当前服务的号码（未开通为空）
    QString nickname;
    QString birthDate;
    QString applyDate;    // 号码申请时间
    QString location;
    QString userId;       // 号码归属（后台用户 ID）
    int tAgeYears = 0;    // T 龄（按号码申请时间计算）
    int friendCount = 0;  // 当前服务的好友数
    int groupCount = 0;   // 当前服务的群数
};

struct ServiceRowFanLX {
    QString service;    // 服务名（微X / QQ / 微信 / 微博）
    QString accountId;  // 该身份的号码（未开通时为空）
    bool subscribed = false; // 是否已开通（微X 注册即开通；下级服务由本人自主开通）
    bool loggedIn = false;   // 是否处于登录态
    bool sharedId = false;   // 是否与其它服务共用同一号码（QQ 与微博共享号）
    bool bound = false;      // 是否已与本人另一个服务账号显式绑定（微信绑 QQ）
    bool current = false;    // 是否为界面当前选中的操作服务
    bool upperLayer = false; // 是否为上层身份（微X）：注册即存在，不能"开通"
    bool exists = false;     // 是否已建号（未开通的下级服务 exists=false）
};

// DomainControllerFanLX：GUI 侧的领域门面（对应控制台 ConsoleAppFanLX 的账号/好友/群三组菜单）。
//
// 为什么 GUI 另开一个领域库：聊天（ImClientFanLX）与会话消息走中心转发，只承载"消息"；
// 用户资料、开通、绑定、好友、群这些**领域**概念由 PersistenceApplicationFanLX 承载。
// 两者各自独占自己那层的 .lock，所以可以同时打开（目录见 imClientPathFanLX::domainDirectoryFanLX）。
//
// 线程约定：本类只在 Qt 主线程使用。领域库是单线程应用门面，所有写操作都在
// 候选事务里完成，提交成功即已原子落盘；界面不会看到"半提交"的群或好友关系。
//
// 持久化约定（对应课设要求 6.1 与七(2)）：
// - open() 在启动时把 state.json 读进内存（空库自动写入演示数据）；
// - 每次写操作（加好友、改群模式等）都经事务提交写回文件，因此断电不丢已确认的操作；
// - saveAndClose() 在退出时再做一次收尾提交并释放目录锁。
class DomainControllerFanLX : public QObject {
    Q_OBJECT

  public:
    explicit DomainControllerFanLX(QObject *parent = nullptr);
    ~DomainControllerFanLX() override;
    DomainControllerFanLX(const DomainControllerFanLX &) = delete;
    DomainControllerFanLX &operator=(const DomainControllerFanLX &) = delete;

    // ---- 生命周期 ----
    // 打开领域库并完成"微X 登录 + 本人其它已开通服务联动"。
    // 空目录会初始化演示数据；损坏/版本不支持的库只报告原因，绝不自动覆盖。
    // confirmOthers 对应界面上"确认联动本人其它已开通服务"的勾选（课设要求 5、6.2）。
    //
    // 失败分成两类，调用方必须区别对待（这正是"登录失败不得跳转界面"的落点）：
    //   IdentityRejected = 身份不被接受（微X号不存在 / 密码错误）→ 留在登录界面重试；
    //   Unavailable      = 领域库本身不可用（目录、schema、损坏）→ 可由调用方降级为"只有聊天"。
    enum class OpenOutcomeFanLX { Ok, IdentityRejected, Unavailable };
    OpenOutcomeFanLX openWithOutcome(const QString &dataRoot, const QString &host, quint16 port,
                                     const QString &account, const QString &password, bool confirmOthers,
                                     QString *detail = nullptr);
    bool open(const QString &dataRoot, const QString &host, quint16 port, const QString &account,
              const QString &password, bool confirmOthers, QString *detail = nullptr);
    // 收尾提交并释放目录锁；可重复调用。
    void saveAndClose();
    bool opened() const { return appFanLX != nullptr; }
    QString directory() const { return directoryFanLX; }
    // 加载结果的一句话说明（"已加载 N 位用户" / "新库，已写入演示数据"）。
    QString loadSummary() const { return loadSummaryFanLX; }
    // 领域事务提交次数，界面用它证明"每次操作都写回了文件"。
    quint64 commitCount() const { return commitCountFanLX; }

    // ---- 连接上下文（聊天页签按"当前服务"重连时要用） ----
    QString host() const { return hostFanLX; }
    quint16 port() const { return portFanLX; }
    QString dataRoot() const { return dataRootFanLX; }
    // 本次登录使用的密码：本人各服务共用，因此聊天通道用同一个口令认证。
    QString loginPassword() const { return loginPasswordFanLX; }

    // ---- 注册（登录界面里的"注册微X账号"入口） ----
    // 静态入口：注册发生在**登录之前**，此时领域库还没有打开，因此这里自行按
    // <数据根>/im-client/<服务器>_<端口>/<微X号> 打开（或创建）该账号的领域库，写入
    // "微X + 密码"后立即关闭。注册只产生上层身份，不捆绑任何下级服务账号。
    static bool registerWeiXUser(const QString &dataRoot, const QString &host, quint16 port,
                                 const QString &weixId, const QString &nickname,
                                 const QString &birthDate, const QString &location,
                                 const QString &password, QString *detail = nullptr);

    // ---- 当前身份 ----
    QString currentService() const { return currentServiceFanLX; }
    QString currentAccountId() const;
    // 本人的微X号（上层登录身份）。
    QString weixAccountId() const;
    // 当前身份是否为微X：只有微X能开通下级服务（QQ/微信/微博）。
    bool isWeiXIdentity() const { return currentServiceFanLX == QStringLiteral("微X"); }
    // 切换"以哪个服务的身份操作"。只允许切到本人已建号且已登录的服务。
    bool setCurrentService(const QString &service);
    // 本人已建号的服务名（微X / QQ / 微信 / 微博），按固定顺序返回。
    QStringList ownedServices() const;
    // 本人已开通的服务名。
    QStringList subscribedServices() const;
    // 本人任一服务的号码（按服务名取；未建号返回空串）。
    QString accountIdOf(const QString &service) const;
    // 该服务当前是否处于登录态（未建号 / 未登录 都是 false）。界面据此判断能否切换、能否操作。
    bool isServiceLoggedIn(const QString &service) const;
    bool currentServiceLoggedIn() const { return isServiceLoggedIn(currentServiceFanLX); }
    // 本人是否还有任何一个服务（含微X）处于登录态；全部注销后为 false。
    bool hasAnyLogin() const;
    // 全部注销后用微X密码重新登录（同时联动本人已开通的服务），不用退出重启。
    bool relogin(const QString &password);
    // 是否有聊天通道（QQ / 微信 / 微博）；微X 是上层身份，不承载聊天。
    static bool chatCapable(const QString &service);
    // 本人"已开通且已登录"的聊天服务（QQ 优先于微信，其次微博），供聊天页签建连使用。
    QStringList chatServices() const;
    // 本人是否**已经**有可用的聊天通道。新注册的微X号没有任何下级服务，因此这是"还没开通"，
    // 不是"登录失败"：界面对这种情况要放行进主界面（否则永远到不了开通管理，形成死循环），
    // 只把聊天页签置为未连接并提示去哪儿开通。
    // 本人"已开通"的聊天服务（QQ > 微信 > 微博；不看领域登录态，见实现处说明）。
    QStringList subscribedChatServices() const;
    // 是否已经有可用的聊天通道。判断用"已开通"而不是"已登录"：刚开通、还没点确认登录的服务
    // 同样能连上聊天（凭据是服务账号 + 密码，由服务端认证），否则用户会在开通后仍被当成"没有通道"。
    bool hasChatChannel() const { return !subscribedChatServices().isEmpty(); }
    // 还没有聊天通道时的界面提示（含"去哪儿开通"的具体路径）。
    QString chatChannelHint() const;
    // 本人**已开通但还没处于登录态**的下级服务。界面用它决定「一键联动登录」按钮是否可用：
    // 登录时默认已经联动确认过（课设要求 5），所以正常情况下这个按钮没什么可做；
    // 只有在"全部注销 / 单独注销某服务 / 开通了新服务"之后才需要它。
    QStringList unlinkedServices() const;
    bool hasUnlinkedServices() const { return !unlinkedServices().isEmpty(); }
    // ---- 把本机领域状态登记到服务端（幂等）----
    // 聊天认证查的是**服务端**的持久化账号库，而注册/开通/加好友发生在本机领域库；
    // 本方法把「微X号 + 密码 + 资料 + 已开通的服务 + 已加的好友」推给服务端（IM 的 REGISTER），
    // 服务端每次认证前读穿存储，因此登记完**不需要重启服务端**。
    // 调用时机：登录后、开通服务后、加好友后，以及聊天连接被服务端以"未登记/未开通/不是好友"拒绝时重试。
    // 返回 false 时 detail 给出可直接显示的原因（例如服务端没启动）。
    bool pushAccountToServer(QString *detail = nullptr);

    // 本人内部用户 ID（后台维度，界面上只在"用户基本信息"里作为号码归属展示）。
    QString userId() const { return userIdFanLX; }

    // ---- 只读视图（对应课设要求 1：用户基本信息） ----
    // 多行文本：昵称 / 出生时间 / 号码申请时间 / T龄 / 所在地 / 号码归属。
    QString profileText();
    ProfileViewFanLX profileView();
    QList<ServiceRowFanLX> serviceRows() const;
    QStringList bindingRows() const;
    // 好友列表：每行"平台账号 ｜ 备注 ｜ 标签"。走应用层查询（命中查询缓存）。
    QStringList friendRows();
    // 好友列表的纯账号形式，供"选一个好友"的界面控件使用。
    QStringList friendAccounts();
    // 好友申请视图（服务端权威，每次调用向服务端取一次）：
    // incoming = 别人发给我的待处理申请，outgoing = 我发出的待处理申请。
    // 每行"号码 ｜ 附言 ｜ 时间"；服务端不可达时返回空并在 lastMessage 说明。
    struct FriendRequestViewFanLX {
        bool reachable = false;
        QStringList incoming;
        QStringList outgoing;
        QStringList temporary; // 非好友临时会话："号码 ｜ 我已发 n / 对方已发 m ｜ 剩余 k"
        int limit = 0;
    };
    FriendRequestViewFanLX friendRequestView();
    // 群列表：按群命名空间过滤，只列出本人是成员的群。
    QStringList groupRows(const QString &service) const;
    // 群成员：每行"账号 | 角色 | 加入序号 | 加入时间"。
    QStringList memberRows(const QString &service, const QString &groupId) const;
    // 待处理记录（申请 / 邀请），行文本已含状态。
    QStringList pendingRows(const QString &service, const QString &groupId) const;
    // 子群（讨论组）列表。
    QStringList discussionRows(const QString &service, const QString &groupId) const;
    // 当前群的特色功能说明：加入方式 / 管理员制度 / 子群能力 / 成员数据是否完整。
    QString policyText(const QString &service, const QString &groupId) const;
    // 查询缓存统计：证明"查询结果有缓存、写操作会使旧结果失效"。
    QString cacheSummary() const;
    QString lastMessage() const { return lastMessageFanLX; }

  public slots:
    // ---- 开通管理（课设要求 4：在微X之下自主开通 QQ/微信/微博） ----
    // 建号 + 立即开通一步完成；accountId 是用户为该服务填写的号码（QQ 号 / 微信号 / 微博号）。
    // 只有微X身份能调用——"只有登录微X后才能在内部去开通三种服务"。
    bool activateService(const QString &service, const QString &accountId);
    // 旧的"对已建号的服务补开通"入口，保留给控制台同款语义与回归测试。
    bool subscribeService(const QString &service);
    // ---- 登录管理（课设要求 5、6.2） ----
    // 逐项确认登录：在本人已认证会话内确认另一个服务账号登录。
    bool confirmLoginService(const QString &service, const QString &accountId);
    // 把本人其它已开通服务一并联动登录（"简单确认后视为自动登录"）。
    bool confirmAllServices();
    // 注销：allServices=true 为平台注销（本人全部服务）。
    bool logoutCurrent(bool allServices);
    // ---- 绑定（课设要求 1：微信独立号可与 QQ 号绑定对应） ----
    bool bindService(const QString &service, const QString &accountId, bool remove);
    // ---- 用户基本信息维护 ----
    // 单项修改：空串表示"这一项不改"，因此可以只改昵称或只改所在地。
    bool updateProfile(const QString &nickname, const QString &location);
    bool changePassword(const QString &oldPassword, const QString &newPassword);

    // ---- 好友管理（课设要求 2：增删改查 + 跨微X共同好友） ----
    // addFriend 现在走"申请→同意"流程：服务端可达时发好友申请（对方同意后才成为好友），
    // 服务端不可达时退回本机直接建立关系（演示/离线回归用）。返回 true 只代表操作已提交。
    bool addFriend(const QString &peerAccount);
    bool removeFriend(const QString &peerAccount);
    bool updateFriendRemark(const QString &peerAccount, const QString &remark, const QString &tag);
    // 同平台共同好友（在**对方平台**账号上查询本人的共同好友）。
    QStringList queryCommonFriends(const QString &peerAccount);
    // 跨平台推荐：本人另一个平台账号推荐的、可在当前平台添加的好友。
    QStringList queryCrossServiceRecommend(const QString &targetService);
    // 跨平台共同好友：两个平台账号之间的共同好友。
    QStringList queryCrossServiceCommon(const QString &targetService);
    // ---- 好友申请（课设新增：一方申请、对方同意后才能成为好友）----
    // 发出好友申请：经服务端中转，对方同意后关系才成立；附言 note 可为空。
    // 服务端不可达时退回本机直接建立关系（演示/离线回归用）。
    bool sendFriendRequest(const QString &peerAccount, const QString &note);
    // 同意 / 拒绝 / 撤回 好友申请：均经服务端中转。
    bool acceptFriendRequest(const QString &peerAccount);
    bool rejectFriendRequest(const QString &peerAccount);
    bool cancelFriendRequest(const QString &peerAccount);
    // ---- 跨服务添加好友（课设新增：依据本人开通的另一个服务的好友）----
    // 列出"本人另一个服务的好友"中、在当前服务也有账号且尚未成为好友的人。
    // 按 userId 换算：源服务好友键 → 归属用户 → 当前服务账号。
    QStringList crossServiceFriendCandidates(const QString &sourceService);
    // 依据源服务里的某个好友，换算到当前服务账号后发出好友申请。
    bool addFriendFromService(const QString &sourceService, const QString &sourcePeerAccount,
                              const QString &note);
    // 远端好友动态到达时刷新视图（由界面把 ChatController 的 friendNotice 转发过来）。
    void onFriendNoticeFanLX();

    // ---- 群管理（课设要求 3） ----
    bool createGroup(const QString &service, const QString &groupId);
    bool renameGroup(const QString &service, const QString &groupId, const QString &name);
    // QQ 群：申请加入 / 批准 / 拒绝申请
    bool applyToJoinGroup(const QString &service, const QString &groupId);
    bool approveGroupApplication(const QString &service, const QString &groupId, const QString &accountId,
                                 bool reject);
    // 邀请他人入群（两种群都有，流程不同）：
    //   QQ 群   ：替对方提交入群申请，由群主/管理员审批；
    //   微信群 ：向对方发出推荐，对方本人确认 / 拒绝（confirmGroupInvitation）。
    bool inviteGroupMember(const QString &service, const QString &groupId, const QString &accountId);
    bool confirmGroupInvitation(const QString &service, const QString &groupId, const QString &accountId,
                                bool reject);
    bool leaveGroup(const QString &service, const QString &groupId);
    // 挨 T：群主/管理员移出成员
    bool kickGroupMember(const QString &service, const QString &groupId, const QString &accountId);
    bool assignGroupAdmin(const QString &service, const QString &groupId, const QString &accountId,
                          bool revoke);
    bool transferGroupOwner(const QString &service, const QString &groupId, const QString &accountId);
    bool dissolveGroup(const QString &service, const QString &groupId);
    // 特色：在成员数据不受伤害的前提下动态变换群管理特色（QQ <-> 微信）。
    bool switchGroupMode(const QString &service, const QString &groupId, bool toQqMode);
    // QQ 群专用的子群（讨论组）；微信群策略禁止，调用会得到明确拒绝原因。
    bool createDiscussionGroup(const QString &service, const QString &groupId, const QString &discussionId);
    bool joinDiscussionGroup(const QString &service, const QString &groupId, const QString &discussionId,
                             const QString &accountId);

  signals:
    // 任何写操作成功后发出：各页面据此重读视图，界面与内存完全同步。
    void dataChangedFanLX();
    // 操作结果：(成功?, 可直接显示的中文说明)。
    void operationFinishedFanLX(bool ok, const QString &message);
    // 当前平台切换（各页面需要按平台重画）。
    void identityChangedFanLX();

  private:
    // 经服务端执行一次好友操作并对账本机副本（实现见 network/PeerAccountResolutionFanLX）；
    // 服务端还不认识本人时先登记再重试一次。via 非空 = 依据本人在 via 服务的好友添加。
    peerAccountFanLX::FriendOperationResultFanLX friendOperationFanLX(const AccountRefFanLX &actor,
                                                                      const std::string &op,
                                                                      const std::string &peer,
                                                                      const std::string &note,
                                                                      const std::string &via = {});
    // 同意 / 拒绝 / 撤回 的共用实现。
    bool respondFriendRequestFanLX(const char *op, const QString &peerAccount, const QString &failurePrefix,
                                   const QString &success);
    // 服务端不可达时的单机演示路径：在本机直接建立好友关系（旧行为，供离线回归）。
    bool addFriendLocallyFanLX(const AccountRefFanLX &actor, const std::string &peer, const QString &prefix);
    // 统一包装：捕获领域异常 → 失败信号；成功则重提交计数并发数据变更信号。
    bool runFanLX(const QString &successMessage, const std::function<void()> &action);
    // 当前服务的登录态发生变化（注销 / 确认登录）时发 identityChanged，让聊天连接跟着断开或重连。
    void syncSessionStateFanLX();
    // 当前平台对应的账号引用；未登录时返回 false 并给出原因。
    bool actorFanLX(AccountRefFanLX &out, QString *reason) const;
    // 群命名空间校验：QQ 群只能用 QQ 身份操作，微信群只能用微信身份操作。
    bool groupKeyFanLX(const QString &service, const QString &groupId, GroupKeyFanLX &out,
                       QString *reason) const;
    // 群写操作的统一前置：群号合法 + 当前平台与该群命名空间一致 + 本人处于登录态。
    // 三个条件分开报错，界面才能给出"该切平台"还是"该先登录"这种可照做的提示。
    bool groupContextFanLX(const QString &service, const QString &groupId, GroupKeyFanLX &key,
                           AccountRefFanLX &actor, QString *reason) const;
    // 平台名 <-> 枚举互转（界面统一用平台名，避免枚举值在界面层泄漏）。
    static bool serviceOfFanLX(const QString &name, ServiceTypeFanLX &out);
    // 按平台名列出本人该平台的账号 ID：内部 userId 只在控制器里参与换算。
    AccountRefFanLX refOfFanLX(ServiceTypeFanLX service) const;
    // 本人某服务的服务账号（未建号返回 nullptr）。
    const ServiceAccountFanLX *ownedAccountFanLX(ServiceTypeFanLX service) const;
    // 单机演示：把"对端账号"临时置为在线（纯内存态，不写回文件）。
    // 好友域要求对端"已开通且已确认登录"才能建立关系——现实里就是"对方在线才能加好友"。
    // 演示时没有第二个进程替对方登录，因此按演示口令规则 pw-<号码> 代登录；
    // 口令推不出来的账号保持离线，领域会照原样报"对端服务尚未确认登录"。
    bool ensurePeerOnlineFanLX(ServiceTypeFanLX service, const std::string &accountId);
    // 首次登录的"锚点"账号：逐项确认与批量联动都以它为授权来源。
    bool anchorFanLX(AccountRefFanLX &out, QString *reason) const;
    // 跨平台推荐 / 跨平台共同好友的共用实现（recommendations 区分两者）。
    QStringList crossServiceQueryFanLX(const QString &targetService, bool recommendations);
    void emitResultFanLX(bool ok, const QString &message);

    std::unique_ptr<PersistenceApplicationFanLX> appFanLX;
    QString directoryFanLX;
    QString loadSummaryFanLX;
    QString lastMessageFanLX;
    QString userIdFanLX;
    QString currentServiceFanLX;
    bool lastCurrentLoggedInFanLX = true; // 上一次同步时当前服务是否处于登录态
    // 连接上下文：聊天页签在"当前服务"变化时按同一套参数重新建连。
    QString dataRootFanLX, hostFanLX;
    quint16 portFanLX = 0;
    // 锚点平台与账号：open() 里第一次成功登录的那个身份。
    QString anchorServiceFanLX;
    QString anchorAccountIdFanLX;
    // 本次登录使用的密码：用于"简单确认"联动本人其它平台（一个用户一个密码）。
    QString loginPasswordFanLX;
    quint64 commitCountFanLX = 0;
};

} // namespace fanlxGui

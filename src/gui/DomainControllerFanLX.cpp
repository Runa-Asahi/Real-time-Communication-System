#include "gui/DomainControllerFanLX.h"
#include "network/ImClientFanLX.h"
#include "network/PeerAccountResolutionFanLX.h"
#include "network/ClientDataPathFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include <QDateTime>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fanlxGui {

namespace {

// 界面统一用中文服务名（微X / QQ / 微信 / 微博），避免 ServiceTypeFanLX 的枚举值或
// 存储名（Wechat）泄漏到界面层。
QString serviceTextFanLX(ServiceTypeFanLX service) {
    return QString::fromUtf8(serviceDisplayNameFanLX(service).c_str());
}

// 群命名空间对应的服务：QQ 群 -> QQ 身份，微信群 -> 微信身份。
ServiceTypeFanLX platformOfModeFanLX(GroupModeFanLX mode) {
    return mode == GroupModeFanLX::QQ ? ServiceTypeFanLX::QQ : ServiceTypeFanLX::Wechat;
}

QString roleTextFanLX(GroupRoleFanLX role) {
    switch (role) {
    case GroupRoleFanLX::Owner: return QStringLiteral("群主");
    case GroupRoleFanLX::Admin: return QStringLiteral("管理员");
    default: return QStringLiteral("成员");
    }
}

// 领域异常文本是 UTF-8（源码与 GCC 输入字符集一致），统一走 fromUtf8，
// 与控制台把异常按 UTF-8 输出的口径保持一致。
QString fromUtf8FanLX(const std::string &text) { return QString::fromUtf8(text.c_str()); }

// T龄：号码申请日期到今天的整年数。与 ConsoleAppFanLX 用同一口径（按 "月-日" 比较决定是否减一年），
// 否则同一个账号在控制台与 GUI 会显示不同的 T龄。
int tAgeYearsFanLX(const std::string &applyDate) {
    const auto today = utcNowFanLX();
    if (applyDate.size() < 10 || today.size() < 10)
        return 0;
    const auto yearOf = [](const std::string &date) {
        return std::atoi(date.substr(0, 4).c_str());
    };
    const int years = yearOf(today) - yearOf(applyDate) - (today.substr(5) < applyDate.substr(5) ? 1 : 0);
    return std::max(0, years);
}

// 固定顺序：微X（上层）在前，随后是三种下级服务。界面表格、下拉框都按这个顺序。
const std::vector<ServiceTypeFanLX> &allServicesFanLX() {
    static const std::vector<ServiceTypeFanLX> services{ServiceTypeFanLX::WeiX, ServiceTypeFanLX::QQ,
                                                         ServiceTypeFanLX::Wechat,
                                                         ServiceTypeFanLX::Weibo};
    return services;
}

} // namespace

DomainControllerFanLX::DomainControllerFanLX(QObject *parent) : QObject(parent) {}
DomainControllerFanLX::~DomainControllerFanLX() { saveAndClose(); }

bool DomainControllerFanLX::serviceOfFanLX(const QString &name, ServiceTypeFanLX &out) {
    // 存储名（QQ / Wechat / Weibo / WeiX）与界面中文名（微X / 微信 / 微博）都认，
    // 这样命令行的 --account、界面下拉框与状态栏可以共用同一个解析入口。
    return serviceFromNameFanLX(name.toStdString(), out);
}

bool DomainControllerFanLX::chatCapable(const QString &service) {
    ServiceTypeFanLX parsed{};
    return serviceOfFanLX(service, parsed) && chatCapableFanLX(parsed);
}

void DomainControllerFanLX::emitResultFanLX(bool ok, const QString &message) {
    lastMessageFanLX = message;
    emit operationFinishedFanLX(ok, message);
}

bool DomainControllerFanLX::runFanLX(const QString &successMessage, const std::function<void()> &action) {
    if (!appFanLX) {
        emitResultFanLX(false, QStringLiteral("领域库尚未打开：请重新启动并登录"));
        return false;
    }
    try {
        action();
    } catch (const std::exception &error) {
        // 领域层用异常表达"未开通 / 未确认 / 权限不足 / 已过期"，界面必须原样呈现，
        // 不能统一改成"操作失败"——用户据此才知道下一步该做什么。
        emitResultFanLX(false, fromUtf8FanLX(error.what()));
        return false;
    }
    // 写操作已经过候选事务原子落盘，这里只累加计数供界面展示"断电保存"的证据。
    ++commitCountFanLX;
    emit dataChangedFanLX();
    emitResultFanLX(true, successMessage);
    return true;
}

bool DomainControllerFanLX::open(const QString &dataRoot, const QString &host, quint16 port,
                                 const QString &account, const QString &password, bool confirmOthers,
                                 QString *detail) {
    return openWithOutcome(dataRoot, host, port, account, password, confirmOthers, detail) ==
           OpenOutcomeFanLX::Ok;
}

// 打开领域库并 "用微X号登录"。微X 是 QQ / 微信 / 微博 的上层账号，也是本系统唯一的登录身份：
// 下级服务不是登录入口，而是登录之后在内部逐个开通出来的。
DomainControllerFanLX::OpenOutcomeFanLX DomainControllerFanLX::openWithOutcome(
    const QString &dataRoot, const QString &host, quint16 port, const QString &account,
    const QString &password, bool confirmOthers, QString *detail) {
    const auto fail = [detail](const QString &text) {
        if (detail)
            *detail = text;
        return OpenOutcomeFanLX::Unavailable;
    };
    const auto reject = [detail](const QString &text) {
        if (detail)
            *detail = text;
        return OpenOutcomeFanLX::IdentityRejected;
    };
    saveAndClose();
    dataRootFanLX = dataRoot;
    hostFanLX = host;
    portFanLX = port;
    try {
        const auto clientDirectory = imClientPathFanLX::clientDirectoryFanLX(
            std::filesystem::path(dataRoot.toStdWString()), host.toStdString(),
            static_cast<unsigned short>(port), account.toStdString());
        const auto directory = imClientPathFanLX::domainDirectoryFanLX(clientDirectory);
        directoryFanLX = QString::fromStdWString(directory.wstring());
        appFanLX = std::make_unique<PersistenceApplicationFanLX>(directory);
    } catch (const std::exception &error) {
        appFanLX.reset();
        return fail(QStringLiteral("领域库目录不可用：") + fromUtf8FanLX(error.what()));
    }

    // 启动时把文件读进内存（课设要求 6.1）。三种状态分开处理，绝不静默覆盖已有数据：
    //   Empty / UncommittedOnly → 全新目录，写入演示数据；
    //   Loaded / RecoveredReadOnly → 直接使用（恢复态只读，写操作会被领域层拒绝）；
    //   UnsupportedSchema / Failed → 只报告原因，界面进入"领域功能不可用"而不是清库。
    const auto status = appFanLX->loadResult().status;
    try {
        if (status == LoadStatusFanLX::Empty || status == LoadStatusFanLX::UncommittedOnly) {
            appFanLX->initialize();
            appFanLX->importInitial(demoSeedFanLX());
            const auto snapshot = appFanLX->snapshot();
            loadSummaryFanLX = QStringLiteral("新领域库：已按约定写入演示数据（%1 位用户、%2 个群，"
                                              "QQ/微信各有 1001-1006；登录请用微X号）")
                                   .arg(snapshot.users.size())
                                   .arg(snapshot.groups.size());
        } else if (status == LoadStatusFanLX::Loaded || status == LoadStatusFanLX::RecoveredReadOnly) {
            const auto snapshot = appFanLX->snapshot();
            loadSummaryFanLX = QStringLiteral("已从文件加载：%1 位用户、%2 个群、%3 条好友关系、%4 条开通记录%5")
                                   .arg(snapshot.users.size())
                                   .arg(snapshot.groups.size())
                                   .arg(snapshot.friendships.size())
                                   .arg(snapshot.subscriptions.size())
                                   .arg(status == LoadStatusFanLX::RecoveredReadOnly
                                            ? QStringLiteral("（备份恢复态，只读）")
                                            : QString());
        } else {
            const auto message = appFanLX->loadResult().message;
            saveAndClose();
            return fail(QStringLiteral("领域库未加载：") + fromUtf8FanLX(message) +
                        QStringLiteral("；请备份该目录后重建"));
        }
    } catch (const std::exception &error) {
        saveAndClose();
        return fail(QStringLiteral("领域库初始化失败：") + fromUtf8FanLX(error.what()));
    }

    // 微X号是唯一的登录身份：QQ / 微信 / 微博 号码不能作为登录别名，
    // 输入的号码在账号库里不是微X号时直接拒绝（留在登录界面），不去猜测所属用户。
    const auto &manager = appFanLX->state().accounts;
    const std::string loginWeix = account.toStdString();
    if (!manager.hasAccount(ServiceTypeFanLX::WeiX, loginWeix)) {
        saveAndClose();
        return reject(QStringLiteral("账号库里没有微X号 %1：请用微X号登录（QQ/微信/微博号码不能直接登录）")
                          .arg(account));
    }
    try {
        appFanLX->login({ServiceTypeFanLX::WeiX, loginWeix}, password.toStdString(),
                        confirmOthers);
    } catch (const std::exception &error) {
        // 密码错误 / 未开通：属于身份问题，调用方必须留在登录界面而不是进主界面。
        saveAndClose();
        return reject(QStringLiteral("微X登录失败：") + fromUtf8FanLX(error.what()));
    }
    const auto &weix = manager.accountByAccountId(ServiceTypeFanLX::WeiX, loginWeix);
    userIdFanLX = QString::fromStdString(weix.userId);
    anchorServiceFanLX = QStringLiteral("微X");
    anchorAccountIdFanLX = QString::fromStdString(loginWeix);
    loginPasswordFanLX = password;
    // 界面默认聚焦到一个已登录的下级服务（QQ 优先）：好友/群/聊天三个页签立刻可用；
    // 需要开通服务时用「开通管理」，它走微X上层身份（见 activateService），不必手工切回来。
    currentServiceFanLX = QStringLiteral("微X");
    for (const auto service : {ServiceTypeFanLX::QQ, ServiceTypeFanLX::Wechat, ServiceTypeFanLX::Weibo}) {
        const auto *found = manager.findAccount(userIdFanLX.toStdString(), service);
        if (found && manager.isAccountLoggedIn(service, found->accountId)) {
            currentServiceFanLX = serviceTextFanLX(service);
            break;
        }
    }
    // 课设要求 5 / 6.2：登微X 后本人其它已开通服务"简单确认"即视为自动登录（微X 是授权来源）。
    if (confirmOthers && currentServiceFanLX == QStringLiteral("微X"))
        confirmAllServices();
    lastCurrentLoggedInFanLX = currentServiceLoggedIn();
    emit identityChangedFanLX();
    return OpenOutcomeFanLX::Ok;
}

void DomainControllerFanLX::saveAndClose() {
    if (!appFanLX)
        return;
    try {
        appFanLX->saveAndClose();
    } catch (const std::exception &) {
        // 只读库或已关闭：收尾提交没有意义；析构仍会释放目录锁，不阻塞退出。
    }
    appFanLX.reset();
}

QString DomainControllerFanLX::currentAccountId() const { return accountIdOf(currentServiceFanLX); }

QString DomainControllerFanLX::weixAccountId() const { return accountIdOf(QStringLiteral("微X")); }

QString DomainControllerFanLX::accountIdOf(const QString &service) const {
    if (!appFanLX || userIdFanLX.isEmpty())
        return {};
    ServiceTypeFanLX parsed{};
    if (!serviceOfFanLX(service, parsed))
        return {};
    const auto *found = ownedAccountFanLX(parsed);
    return found ? QString::fromStdString(found->accountId) : QString();
}

bool DomainControllerFanLX::isServiceLoggedIn(const QString &service) const {
    ServiceTypeFanLX parsed{};
    if (!appFanLX || !serviceOfFanLX(service, parsed))
        return false;
    const auto accountId = accountIdOf(service);
    return !accountId.isEmpty() && appFanLX->state().accounts.isAccountLoggedIn(parsed, accountId.toStdString());
}

bool DomainControllerFanLX::hasAnyLogin() const {
    for (const auto &name : ownedServices())
        if (isServiceLoggedIn(name))
            return true;
    return false;
}

void DomainControllerFanLX::syncSessionStateFanLX() {
    const bool now = currentServiceLoggedIn();
    if (now == lastCurrentLoggedInFanLX)
        return;
    lastCurrentLoggedInFanLX = now;
    emit identityChangedFanLX();
}

// 全部注销后的重新登录：只认微X号和密码（登录身份就是微X），并联动本人已开通的下级服务。
bool DomainControllerFanLX::relogin(const QString &password) {
    if (!appFanLX || anchorAccountIdFanLX.isEmpty()) {
        emitResultFanLX(false, QStringLiteral("领域库尚未打开：请重新启动并登录"));
        return false;
    }
    try {
        appFanLX->login({ServiceTypeFanLX::WeiX, anchorAccountIdFanLX.toStdString()}, password.toStdString(),
                        true);
    } catch (const std::exception &error) {
        emitResultFanLX(false, QStringLiteral("微X登录失败：") + fromUtf8FanLX(error.what()));
        return false;
    }
    loginPasswordFanLX = password;
    // 当前服务这次没有登回来（例如它已不再开通）时，退到一个已登录的服务，避免停在无效身份上。
    if (!currentServiceLoggedIn()) {
        for (const auto &name : {QStringLiteral("QQ"), QStringLiteral("微X")})
            if (isServiceLoggedIn(name)) {
                currentServiceFanLX = name;
                break;
            }
    }
    emit dataChangedFanLX();
    syncSessionStateFanLX();
    emitResultFanLX(true, QStringLiteral("已重新登录微X，并联动本人已开通的服务"));
    return true;
}

// 本人某服务的服务账号（未建号返回 nullptr）。
const ServiceAccountFanLX *DomainControllerFanLX::ownedAccountFanLX(ServiceTypeFanLX service) const {
    if (!appFanLX || userIdFanLX.isEmpty())
        return nullptr;
    return appFanLX->state().accounts.findAccount(userIdFanLX.toStdString(), service);
}

QStringList DomainControllerFanLX::chatServices() const {
    QStringList result;
    if (!appFanLX || userIdFanLX.isEmpty())
        return result;
    const auto &manager = appFanLX->state().accounts;
    // 三种下级服务都能单聊（微博没有群），因此都可以作为聊天通道。
    for (const auto service :
         {ServiceTypeFanLX::QQ, ServiceTypeFanLX::Wechat, ServiceTypeFanLX::Weibo}) {
        const auto *found = manager.findAccount(userIdFanLX.toStdString(), service);
        if (found && manager.isAccountSubscribed(service, found->accountId) &&
            manager.isAccountLoggedIn(service, found->accountId))
            result << serviceTextFanLX(service);
    }
    return result;
}

// 本人"已开通"的聊天服务（不看是否已完成领域登录）。与 chatServices() 的区别：
// chatServices() 还要"已登录"，那是领域层的确认登录语义；而**聊天连接**的凭据是
// 服务账号 + 密码，由服务端独立认证，所以刚开通、还没点"逐项确认登录"的服务也可以直接建连。
QStringList DomainControllerFanLX::subscribedChatServices() const {
    QStringList result;
    for (const auto &name : subscribedServices()) {
        ServiceTypeFanLX service{};
        if (serviceOfFanLX(name, service) && chatCapableFanLX(service))
            result << name;
    }
    // 与 chatServices() 同一优先级：QQ > 微信 > 微博。
    QStringList ordered;
    for (const auto &preferred : {QStringLiteral("QQ"), QStringLiteral("微信"), QStringLiteral("微博")})
        if (result.contains(preferred))
            ordered << preferred;
    return ordered;
}

// 已开通但未登录的下级服务（"一键联动登录"是否有事可做的判据）。
QStringList DomainControllerFanLX::unlinkedServices() const {
    QStringList result;
    if (!appFanLX)
        return result;
    const auto &manager = appFanLX->state().accounts;
    for (const auto &row : serviceRows()) {
        ServiceTypeFanLX parsed{};
        if (!row.exists || !row.subscribed ||
            !serviceFromNameFanLX(row.service.toStdString(), parsed) ||
            !isSubordinateServiceFanLX(parsed))
            continue;
        if (!manager.isAccountLoggedIn(parsed, row.accountId.toStdString()))
            result << row.service;
    }
    return result;
}

// 还没有聊天通道时给用户的一句话：说清"这不是登录失败"，以及去哪儿开通。
// 新注册的微X号天然没有下级服务（注册不捆绑服务），所以这条提示是必经路径而不是异常。
QString DomainControllerFanLX::chatChannelHint() const {
    // 只看**下级**服务：微X 是上层登录身份，本身不承载聊天，不能算作"已开通的通道"。
    QStringList subscribed;
    for (const auto &row : serviceRows()) {
        ServiceTypeFanLX parsed{};
        if (row.exists && row.subscribed && serviceFromNameFanLX(row.service.toStdString(), parsed) &&
            isSubordinateServiceFanLX(parsed))
            subscribed << row.service;
    }
    if (subscribed.isEmpty())
        return QStringLiteral("本人还没有开通任何下级服务：请到「账号与服务 → 开通管理」开通 "
                              "QQ / 微信 / 微博（微X 是上层登录身份，本身不承载聊天），"
                              "开通后聊天页会自动连接。");
    return QStringLiteral("本人已开通 %1，但都还没登录：请到「账号与服务 → 登录管理」逐项确认登录，"
                          "聊天页会按当前服务自动连接。")
        .arg(subscribed.join(QStringLiteral(" / ")));
}

// 把本机领域状态登记到服务端：账号、已开通的服务、已加的好友。幂等，可反复调用。
// 放在领域控制器里是因为：它同时知道"本机的账号/服务/好友"与"服务端地址 + 登录密码"；
// 聊天控制器只知道一条通道（构造它需要先有服务账号），反而覆盖不了"还没有服务"的新账号。
bool DomainControllerFanLX::pushAccountToServer(QString *detail) {
    if (!opened() || hostFanLX.isEmpty() || portFanLX == 0 || weixAccountId().isEmpty()) {
        if (detail)
            *detail = QStringLiteral("尚未登录或缺少服务端地址，无法登记到服务端");
        return false;
    }
    std::vector<ImClientFanLX::ServiceEntryFanLX> services;
    std::vector<ImClientFanLX::ServiceEntryFanLX> friends;
    for (const auto &row : serviceRows()) {
        if (!row.exists || !row.subscribed)
            continue;
        ServiceTypeFanLX parsed{};
        if (!serviceFromNameFanLX(row.service.toStdString(), parsed))
            continue;
        // 只有能承载聊天的服务需要在服务端登记（微X 是上层身份，注册时就已经在服务端了）。
        if (!chatCapableFanLX(parsed))
            continue;
        const auto storage = serviceNameFanLX(parsed);
        services.emplace_back(storage, row.accountId.toStdString());
        // 好友按服务分别登记：本机加的好友不会自动出现在服务端，而服务端要按它自己的账号库
        // 判定"是不是好友"。该服务会话没登录时查询会抛错，跳过它不影响其它服务。
        try {
            const AccountRefFanLX actor{parsed, row.accountId.toStdString()};
            for (const auto &peer : appFanLX->queryFriendAccounts(actor))
                friends.emplace_back(storage, peer);
        } catch (const std::exception &) {
        }
    }
    std::string reason;
    const auto pushed = ImClientFanLX::registerAccountFanLX(
        hostFanLX.toStdString(), portFanLX, weixAccountId().toStdString(),
        loginPasswordFanLX.toStdString(), QString().toStdString(), QString().toStdString(),
        QString().toStdString(), reason, services, friends);
    if (detail)
        *detail = QString::fromUtf8(reason.c_str());
    return pushed;
}
AccountRefFanLX DomainControllerFanLX::refOfFanLX(ServiceTypeFanLX service) const {
    const auto *found = ownedAccountFanLX(service);
    return {service, found ? found->accountId : std::string{}};
}

bool DomainControllerFanLX::ensurePeerOnlineFanLX(ServiceTypeFanLX service, const std::string &accountId) {
    if (!appFanLX)
        return false;
    const auto &manager = appFanLX->state().accounts;
    if (manager.isAccountLoggedIn(service, accountId))
        return true;
    if (!manager.hasAccount(service, accountId) || !manager.isAccountSubscribed(service, accountId))
        return false;
    // 演示口令规则只有一处定义（DemoSeedFanLX），这里复用而不是再抄一遍 "pw-"——
    // 微信的账号是 wx_<号码>，先剥掉前缀才能还原出号码。
    std::string number = accountId;
    if (number.rfind("wx_", 0) == 0)
        number = number.substr(3);
    try {
        appFanLX->login({service, accountId}, demoPasswordFanLX(number), false);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

bool DomainControllerFanLX::anchorFanLX(AccountRefFanLX &out, QString *reason) const {
    if (!appFanLX || userIdFanLX.isEmpty()) {
        if (reason)
            *reason = QStringLiteral("领域库尚未打开");
        return false;
    }
    const auto &manager = appFanLX->state().accounts;
    // 优先用 open() 里登录成功的那个锚点账号（就是微X号），它被注销后，退而使用
    // 本人任一仍在登录态的服务——包括微X自己，因为微X才是开通服务的授权来源。
    ServiceTypeFanLX anchorService{};
    if (serviceOfFanLX(anchorServiceFanLX, anchorService) && !anchorAccountIdFanLX.isEmpty() &&
        manager.isAccountLoggedIn(anchorService, anchorAccountIdFanLX.toStdString())) {
        out = {anchorService, anchorAccountIdFanLX.toStdString()};
        return true;
    }
    for (const auto service : allServicesFanLX()) {
        const auto *found = manager.findAccount(userIdFanLX.toStdString(), service);
        if (found && manager.isAccountLoggedIn(service, found->accountId)) {
            out = {service, found->accountId};
            return true;
        }
    }
    if (reason)
        *reason = QStringLiteral("本人当前没有登录中的服务：请先用微X号登录，再使用联动功能");
    return false;
}

QStringList DomainControllerFanLX::ownedServices() const {
    QStringList result;
    if (!appFanLX || userIdFanLX.isEmpty())
        return result;
    for (const auto service : allServicesFanLX()) {
        const auto *found = appFanLX->state().accounts.findAccount(userIdFanLX.toStdString(), service);
        if (found && !found->accountId.empty())
            result << serviceTextFanLX(service);
    }
    return result;
}

QStringList DomainControllerFanLX::subscribedServices() const {
    QStringList result;
    if (!appFanLX)
        return result;
    for (const auto &name : ownedServices()) {
        ServiceTypeFanLX service{};
        if (serviceOfFanLX(name, service) &&
            appFanLX->state().accounts.isAccountSubscribed(service, accountIdOf(name).toStdString()))
            result << name;
    }
    return result;
}

bool DomainControllerFanLX::setCurrentService(const QString &service) {
    if (!appFanLX) {
        emitResultFanLX(false, QStringLiteral("领域库尚未打开"));
        return false;
    }
    // 入参既接受界面中文名（微X / 微信 / 微博），也接受存储名（QQ / Wechat / Weibo / WeiX）：
    // 页面控件、状态栏与测试都从这里过一遍，避免两套写法各写一半。
    ServiceTypeFanLX parsed{};
    if (!serviceOfFanLX(service, parsed)) {
        emitResultFanLX(false, QStringLiteral("未知服务：%1").arg(service));
        return false;
    }
    const auto canonical = serviceTextFanLX(parsed);
    if (!ownedServices().contains(canonical)) {
        emitResultFanLX(false, QStringLiteral("本人没有 %1 身份：请先在「开通管理」里开通").arg(canonical));
        return false;
    }
    if (!appFanLX->state().accounts.isAccountLoggedIn(parsed, accountIdOf(canonical).toStdString())) {
        emitResultFanLX(false, QStringLiteral("%1 尚未登录：请先在「登录管理」里确认登录").arg(canonical));
        return false;
    }
    currentServiceFanLX = canonical;
    emit identityChangedFanLX();
    emitResultFanLX(true, QStringLiteral("当前服务已切换到 %1（号码 %2）")
                               .arg(canonical, accountIdOf(canonical)));
    return true;
}

bool DomainControllerFanLX::actorFanLX(AccountRefFanLX &out, QString *reason) const {
    if (!appFanLX) {
        if (reason)
            *reason = QStringLiteral("领域库尚未打开");
        return false;
    }
    ServiceTypeFanLX service{};
    if (!serviceOfFanLX(currentServiceFanLX, service)) {
        if (reason)
            *reason = QStringLiteral("当前服务无效");
        return false;
    }
    const auto accountId = accountIdOf(currentServiceFanLX);
    if (accountId.isEmpty()) {
        if (reason)
            *reason = QStringLiteral("本人在 %1 没有身份").arg(currentServiceFanLX);
        return false;
    }
    if (!appFanLX->state().accounts.isAccountLoggedIn(service, accountId.toStdString())) {
        if (reason)
            *reason = QStringLiteral("%1 未登录：请先在「登录管理」里确认登录").arg(currentServiceFanLX);
        return false;
    }
    out = {service, accountId.toStdString()};
    return true;
}

bool DomainControllerFanLX::groupKeyFanLX(const QString &service, const QString &groupId, GroupKeyFanLX &out,
                                          QString *reason) const {
    if (groupId.trimmed().isEmpty()) {
        if (reason)
            *reason = QStringLiteral("请先选择或填写群号");
        return false;
    }
    // 群命名空间只有 QQ 与微信：微X 是上层身份、微博账号没有群
    // （与控制台 groupFanLX 的判定一致）。
    ServiceTypeFanLX parsed{};
    if (serviceOfFanLX(service, parsed) && groupCapableFanLX(parsed)) {
        out = {parsed == ServiceTypeFanLX::Wechat ? GroupModeFanLX::Wechat : GroupModeFanLX::QQ,
               groupId.toStdString()};
        return true;
    }
    if (reason)
        *reason = QStringLiteral("群功能只支持 QQ 与微信（微X 是上层身份、微博没有群）");
    return false;
}

bool DomainControllerFanLX::groupContextFanLX(const QString &service, const QString &groupId,
                                              GroupKeyFanLX &key, AccountRefFanLX &actor,
                                              QString *reason) const {
    if (!groupKeyFanLX(service, groupId, key, reason))
        return false;
    // 群按"命名空间"隔离：QQ 群只能由 QQ 账号管理，微信群只能由微信账号管理。
    // 这一条来自领域层（actorUser 会拒绝跨命名空间），在这里提前拦下并给出可照做的提示，
    // 比让用户看到"操作者账号不属于群命名空间"要好懂得多。
    // 比较用解析后的枚举：界面中文名（微信）与存储名（Wechat）都指向同一个服务。
    ServiceTypeFanLX requested{};
    ServiceTypeFanLX current{};
    serviceOfFanLX(service, requested);
    const auto currentName = serviceOfFanLX(currentServiceFanLX, current)
                                 ? serviceTextFanLX(current)
                                 : currentServiceFanLX;
    if (requested != current) {
        if (reason)
            *reason = QStringLiteral("%1 上的群需要用 %1 身份操作（QQ 群用 QQ 身份、微信群用微信身份）："
                                     "请先在「账号与服务」里把当前服务切到 %1，当前是 %2")
                          .arg(serviceTextFanLX(requested), currentName);
        return false;
    }
    return actorFanLX(actor, reason);
}

// ============================ 只读视图 ============================

QString DomainControllerFanLX::profileText() {
    if (!appFanLX || userIdFanLX.isEmpty())
        return QStringLiteral("（未登录：请用微X号与密码登录后查看）");
    const auto &manager = appFanLX->state().accounts;
    const auto &profile = manager.profile(userIdFanLX.toStdString());
    const auto accountId = currentAccountId();
    const auto friendCount = friendAccounts().size();
    QString text;
    text += QStringLiteral("微X号（上层登录身份）：%1\n").arg(weixAccountId());
    text += QStringLiteral("当前服务与号码：%1 ｜ %2\n")
                .arg(currentServiceFanLX, accountId.isEmpty() ? QStringLiteral("（未开通）") : accountId);
    text += QStringLiteral("昵称：%1\n").arg(fromUtf8FanLX(profile.nickname));
    text += QStringLiteral("出生时间：%1\n").arg(fromUtf8FanLX(profile.birthDate));
    text += QStringLiteral("号码申请时间：%1\n").arg(fromUtf8FanLX(profile.applyDate));
    text += QStringLiteral("T龄：%1 年（按号码申请时间计算）\n").arg(tAgeYearsFanLX(profile.applyDate));
    text += QStringLiteral("所在地：%1\n").arg(fromUtf8FanLX(profile.location));
    text += QStringLiteral("号码归属（后台用户ID）：%1\n").arg(userIdFanLX);
    text += QStringLiteral("好友列表（%1 服务独立）：%2 人\n").arg(currentServiceFanLX).arg(friendCount);
    text += QStringLiteral("群列表（%1 服务独立）：%2 个\n")
                .arg(currentServiceFanLX)
                .arg(groupRows(currentServiceFanLX).size());
    return text;
}

ProfileViewFanLX DomainControllerFanLX::profileView() {
    ProfileViewFanLX view;
    if (!appFanLX || userIdFanLX.isEmpty())
        return view;
    const auto &profile = appFanLX->state().accounts.profile(userIdFanLX.toStdString());
    view.valid = true;
    view.weixId = weixAccountId();
    view.service = currentServiceFanLX;
    view.accountId = currentAccountId();
    view.nickname = fromUtf8FanLX(profile.nickname);
    view.birthDate = fromUtf8FanLX(profile.birthDate);
    view.applyDate = fromUtf8FanLX(profile.applyDate);
    view.location = fromUtf8FanLX(profile.location);
    view.userId = userIdFanLX;
    view.tAgeYears = tAgeYearsFanLX(profile.applyDate);
    view.friendCount = friendAccounts().size();
    view.groupCount = groupRows(currentServiceFanLX).size();
    return view;
}

QList<ServiceRowFanLX> DomainControllerFanLX::serviceRows() const {
    QList<ServiceRowFanLX> rows;
    if (!appFanLX || userIdFanLX.isEmpty())
        return rows;
    const auto &manager = appFanLX->state().accounts;
    // 四种身份都列出来：微X（上层，注册即有）与 QQ / 微信 / 微博（未开通的也显示，界面上
    // 才能看出"还差哪一项没开通"）。未开通的行只有服务名，没有号码。
    for (const auto service : allServicesFanLX()) {
        ServiceRowFanLX row;
        row.service = serviceTextFanLX(service);
        row.upperLayer = service == ServiceTypeFanLX::WeiX;
        const auto *found = manager.findAccount(userIdFanLX.toStdString(), service);
        row.exists = found != nullptr;
        if (found) {
            row.accountId = QString::fromStdString(found->accountId);
            row.subscribed = manager.isAccountSubscribed(service, found->accountId);
            row.loggedIn = manager.isAccountLoggedIn(service, found->accountId);
            row.sharedId = found->sharedId;
            row.bound = !manager.bindingsOf(service, found->accountId).empty();
        }
        row.current = row.service == currentServiceFanLX;
        rows.append(row);
    }
    return rows;
}

QStringList DomainControllerFanLX::bindingRows() const {
    QStringList rows;
    if (!appFanLX || userIdFanLX.isEmpty())
        return rows;
    const auto &manager = appFanLX->state().accounts;
    for (const auto service : allServicesFanLX()) {
        const auto *found = manager.findAccount(userIdFanLX.toStdString(), service);
        if (!found)
            continue;
        for (const auto &peer : manager.bindingsOf(service, found->accountId))
            rows << QStringLiteral("%1（%2） <--> %3（%4）")
                        .arg(serviceTextFanLX(service), QString::fromStdString(found->accountId),
                             serviceTextFanLX(peer.service), QString::fromStdString(peer.accountId));
    }
    return rows;
}

QStringList DomainControllerFanLX::friendAccounts() {
    QStringList rows;
    if (!appFanLX || userIdFanLX.isEmpty())
        return rows;
    ServiceTypeFanLX service{};
    if (!serviceOfFanLX(currentServiceFanLX, service) || !isSubordinateServiceFanLX(service))
        return rows; // 微X 是上层身份，没有自己的好友列表
    // 没登录的服务查询会抛 SessionExpired；这里被大量界面刷新调用，不能让异常逃进 Qt 槽。
    if (!currentServiceLoggedIn())
        return rows;
    // 好友查询走应用层入口，因此命中查询缓存；写操作会让旧结果按版本号失效。
    try {
        for (const auto &peer : appFanLX->queryFriendAccounts(refOfFanLX(service)))
            rows << QString::fromStdString(peer);
    } catch (const std::exception &) {
        rows.clear();
    }
    return rows;
}

QStringList DomainControllerFanLX::friendRows() {
    QStringList rows;
    if (!appFanLX || userIdFanLX.isEmpty())
        return rows;
    ServiceTypeFanLX service{};
    if (!serviceOfFanLX(currentServiceFanLX, service) || !currentServiceLoggedIn())
        return rows; // 未登录的服务不查询（查询会抛 SessionExpired，界面刷新路径上不能让异常逃出）
    try {
        const auto actor = refOfFanLX(service);
        IdentityResolverFanLX resolver(appFanLX->state().accounts);
        const auto own = resolver.accountKey(actor.service, actor.accountId);
        // 备注/标签只来自"当前服务"那一份好友分区：QQ 好友的备注不会出现在微信列表里。
        const auto &directory = appFanLX->state().friends.directory(service);
        for (const auto &peer : appFanLX->queryFriendAccounts(actor)) {
            const auto detail = directory.detail(own, resolver.accountKey(actor.service, peer));
            rows << QStringLiteral("%1 ｜ 备注：%2 ｜ 标签：%3")
                        .arg(QString::fromStdString(peer),
                             detail.remark.empty() ? QStringLiteral("（无）") : fromUtf8FanLX(detail.remark),
                             detail.tag.empty() ? QStringLiteral("（无）") : fromUtf8FanLX(detail.tag));
        }
    } catch (const std::exception &) {
        rows.clear();
    }
    return rows;
}

QStringList DomainControllerFanLX::groupRows(const QString &service) const {
    QStringList rows;
    if (!appFanLX || userIdFanLX.isEmpty())
        return rows;
    ServiceTypeFanLX parsed{};
    if (!serviceOfFanLX(service, parsed) || !groupCapableFanLX(parsed))
        return rows;
    const auto mode = parsed == ServiceTypeFanLX::QQ ? GroupModeFanLX::QQ : GroupModeFanLX::Wechat;
    const auto user = userIdFanLX.toStdString();
    // 群本身就按命名空间分开存放（QQ 群 / 微信群），这里只列当前命名空间里本人参与的群。
    for (const auto &key : appFanLX->state().groups.groupKeys()) {
        if (key.mode != mode)
            continue;
        const auto &group = appFanLX->state().groups.get(key);
        if (!group.hasMember(user))
            continue; // 只列出本人是成员的群
        rows << QStringLiteral("%1 ｜ %2 ｜ 模式 %3 ｜ 成员 %4 人")
                    .arg(QString::fromStdString(key.id),
                         group.name().empty() ? QStringLiteral("（未命名）") : fromUtf8FanLX(group.name()),
                         group.currentMode() == GroupModeFanLX::QQ ? QStringLiteral("QQ") : QStringLiteral("微信"))
                    .arg(group.snapshot().size());
    }
    return rows;
}

QStringList DomainControllerFanLX::memberRows(const QString &service, const QString &groupId) const {
    QStringList rows;
    GroupKeyFanLX key{};
    if (!appFanLX || !groupKeyFanLX(service, groupId, key, nullptr))
        return rows;
    const auto &manager = appFanLX->state().accounts;
    const auto platform = platformOfModeFanLX(key.mode);
    try {
        const auto &group = appFanLX->state().groups.get(key);
        for (const auto &member : group.snapshot()) {
            // 成员表存的是内部 userId；界面必须显示平台账号（与现实一致），
            // 因此按"归属用户 + 本群命名空间对应平台"换算回账号 ID。
            const auto *account = manager.findAccount(member.id, platform);
            rows << QStringLiteral("%1 ｜ %2 ｜ 加入序号 %3")
                        .arg(account ? QString::fromStdString(account->accountId) : fromUtf8FanLX(member.id),
                             roleTextFanLX(member.role))
                        .arg(member.joinOrder);
        }
    } catch (const std::exception &) {
        // 群不存在：返回空列表，界面显示"（无数据）"，不弹异常。
    }
    return rows;
}

QStringList DomainControllerFanLX::pendingRows(const QString &service, const QString &groupId) const {
    QStringList rows;
    GroupKeyFanLX key{};
    if (!appFanLX || !groupKeyFanLX(service, groupId, key, nullptr))
        return rows;
    const auto &manager = appFanLX->state().accounts;
    const auto platform = platformOfModeFanLX(key.mode);
    const auto accountText = [&manager, platform](const std::string &user) {
        const auto *account = manager.findAccount(user, platform);
        return account ? QString::fromStdString(account->accountId) : fromUtf8FanLX(user);
    };
    const auto snapshot = appFanLX->snapshot();
    for (const auto &record : snapshot.groups) {
        if (!(record.key == key))
            continue;
        for (const auto &application : record.applications)
            rows << QStringLiteral("入群申请：%1 ｜ 状态 %2%3")
                        .arg(accountText(application.applicant), fromUtf8FanLX(application.status),
                             application.pending ? QStringLiteral("  ← 待审批") : QString());
        for (const auto &invitation : record.invitations)
            rows << QStringLiteral("入群推荐：%1 邀请 %2 ｜ 状态 %3%4")
                        .arg(accountText(invitation.inviter), accountText(invitation.invitee),
                             fromUtf8FanLX(invitation.status),
                             invitation.pending ? QStringLiteral("  ← 待确认") : QString());
    }
    return rows;
}

QStringList DomainControllerFanLX::discussionRows(const QString &service, const QString &groupId) const {
    QStringList rows;
    GroupKeyFanLX key{};
    if (!appFanLX || !groupKeyFanLX(service, groupId, key, nullptr))
        return rows;
    const auto &manager = appFanLX->state().accounts;
    const auto platform = platformOfModeFanLX(key.mode);
    const auto snapshot = appFanLX->snapshot();
    for (const auto &record : snapshot.groups) {
        if (!(record.key == key))
            continue;
        for (const auto &discussion : record.discussions) {
            QStringList members;
            for (const auto &member : discussion.members) {
                const auto *account = manager.findAccount(member, platform);
                members << (account ? QString::fromStdString(account->accountId) : fromUtf8FanLX(member));
            }
            rows << QStringLiteral("子群 %1 ｜ 成员 %2 人：%3 ｜ 已归档：%4")
                        .arg(fromUtf8FanLX(discussion.id))
                        .arg(members.size())
                        .arg(members.isEmpty() ? QStringLiteral("（无）") : members.join(QStringLiteral("、")))
                        .arg(discussion.archived ? QStringLiteral("是") : QStringLiteral("否"));
        }
    }
    return rows;
}

QString DomainControllerFanLX::policyText(const QString &service, const QString &groupId) const {
    GroupKeyFanLX key{};
    if (!appFanLX || !groupKeyFanLX(service, groupId, key, nullptr))
        return QStringLiteral("请选择左侧的一个群：这里显示该群**当前**的管理特色，"
                              "并可动态切换为另一类群的管理模式。");
    // 群号可能在当前命名空间里不存在（手填、已解散、或刚从另一个服务切过来），
    // 直接 get() 会抛出 unordered_map::at，这里先判断并给出人能看懂的提示。
    {
        bool exists = false;
        for (const auto &existing : appFanLX->state().groups.groupKeys())
            if (existing == key)
                exists = true;
        if (!exists)
            return QStringLiteral("群 %1 在%2里不存在。请在列表中选择已有的群，或先创建。")
                .arg(QString::fromStdString(key.id),
                     key.mode == GroupModeFanLX::QQ ? QStringLiteral("QQ 群") : QStringLiteral("微信群"));
    }
    try {
        const auto &group = appFanLX->state().groups.get(key);
        const auto &policy = group.policy();
        const bool qq = policy.mode() == GroupModeFanLX::QQ;
        const bool switched = policy.mode() != key.mode;
        QString text;
        text += QStringLiteral("群号 %1 ｜ 群名 %2\n").arg(
            QString::fromStdString(key.id),
            group.name().empty() ? QStringLiteral("（未命名）") : fromUtf8FanLX(group.name()));
        text += QStringLiteral("【群身份】登记在 %1 命名空间；【当前管理特色】%2（策略对象 %3）%4\n")
                    .arg(key.mode == GroupModeFanLX::QQ ? QStringLiteral("QQ") : QStringLiteral("微信"),
                         qq ? QStringLiteral("QQ 群") : QStringLiteral("微信群"),
                         qq ? QStringLiteral("QQGroupPolicyFanLX") : QStringLiteral("WechatGroupPolicyFanLX"),
                         switched ? QStringLiteral("  ← 已动态切换（群身份与成员数据不变）") : QString());
        text += QStringLiteral("【加入方式】%1\n")
                    .arg(qq ? QStringLiteral("申请-审批制：非成员可主动申请入群，群主或管理员批准后生效")
                            : QStringLiteral("推荐制：只能由群内成员推荐（邀请），被推荐人确认后入群；"
                                             "没有申请入口"));
        text += QStringLiteral("【特权账号】%1\n")
                    .arg(qq ? QStringLiteral("群主 + 管理员制度：群主可任免管理员，管理员与群主共同管理成员")
                            : QStringLiteral("仅群主为特权账号：不支持设置管理员，成员管理只由群主执行"));
        text += QStringLiteral("【子群/讨论组】%1\n")
                    .arg(policy.canCreateDiscussion()
                             ? QStringLiteral("允许：群主或管理员可开设临时讨论组（子群）并从中拉人")
                             : QStringLiteral("不允许：微信没有讨论组概念，创建请求会被策略直接拒绝"));
        text += QStringLiteral("【成员数据】%1 名成员、%2 个子群：切换模式只替换策略对象，"
                               "成员、加入序号与子群记录原地保留（数据不受伤害）")
                    .arg(group.snapshot().size())
                    .arg(discussionRows(service, groupId).size());
        return text;
    } catch (const std::exception &error) {
        return QStringLiteral("读取群策略失败：") + fromUtf8FanLX(error.what());
    }
}

QString DomainControllerFanLX::cacheSummary() const {
    if (!appFanLX)
        return QStringLiteral("领域库尚未打开");
    const auto metrics = appFanLX->queryCacheMetrics();
    return QStringLiteral("查询缓存：命中 %1 ｜ 未命中 %2 ｜ 有效条目 %3 ｜ 逻辑载荷 %4 字节 ｜ 池保留 %5 字节")
        .arg(metrics.hits)
        .arg(metrics.misses)
        .arg(metrics.entries)
        .arg(metrics.bytes)
        .arg(metrics.poolReservedBytes);
}

// ============================ 开通管理 ============================

// 开通下级服务：建号 + 立即开通一步完成。授权来源是"本人登录微X"这一次认证，
// 因此界面不必要求用户先把当前服务切回微X——领域层仍会拒绝非微X会话的调用。
bool DomainControllerFanLX::activateService(const QString &service, const QString &accountId) {
    ServiceTypeFanLX target{};
    if (!serviceOfFanLX(service, target) || !isSubordinateServiceFanLX(target)) {
        emitResultFanLX(false, QStringLiteral("只有 QQ / 微信 / 微博 可以开通（微X 是上层登录身份）"));
        return false;
    }
    if (accountId.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请先填写 %1 的号码").arg(service));
        return false;
    }
    // 授权来源是"本人这一次微X登录"：先把锚点（微X）确认在登录态，再用微X身份发起开通。
    // 领域层还会再判一次"必须是微X会话"，因此界面不必要求用户手工把当前服务切回微X。
    AccountRefFanLX weix{};
    QString reason;
    if (!anchorFanLX(weix, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (weix.service != ServiceTypeFanLX::WeiX) {
        emitResultFanLX(false, QStringLiteral("开通下级服务需要先以微X身份登录（当前只有 %1 处于登录态）")
                                   .arg(serviceTextFanLX(weix.service)));
        return false;
    }
    const auto ok = runFanLX(QStringLiteral("已开通 %1（号码 %2）：好友与群从此刻起按该服务独立存放")
                                 .arg(service, accountId),
                             [&] { appFanLX->activateService(weix, target, accountId.toStdString()); });
    if (ok) {
        // 开通是**本机**领域操作，而聊天认证查的是服务端账号库：顺手登记一次，
        // 之后"在窗口里开通 QQ，聊天页立刻连上"就不需要退出重进，更不需要重启服务端。
        // 登记失败不阻断开通（本机数据已经落盘），只在提示里说明，登录/切服务时会自动重试。
        QString why;
        if (!pushAccountToServer(&why))
            emitResultFanLX(true, QStringLiteral("已开通 %1（号码 %2）：本机已生效；"
                                                 "服务端登记失败：%3（连接聊天时会自动重试）")
                                      .arg(service, accountId, why));
    }
    return ok;
}

bool DomainControllerFanLX::subscribeService(const QString &service) {
    ServiceTypeFanLX target{};
    if (!serviceOfFanLX(service, target)) {
        emitResultFanLX(false, QStringLiteral("未知服务：%1").arg(service));
        return false;
    }
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (accountIdOf(service).isEmpty()) {
        emitResultFanLX(false, QStringLiteral("%1 还没有号码：请用「开通所选服务」填写号码并开通").arg(service));
        return false;
    }
    return runFanLX(QStringLiteral("已开通 %1 服务（号码 %2）").arg(service, accountIdOf(service)), [&] {
        appFanLX->subscribe(actor, target);
    });
}

// ============================ 登录管理 ============================

bool DomainControllerFanLX::confirmLoginService(const QString &service, const QString &accountId) {
    ServiceTypeFanLX target{};
    if (!serviceOfFanLX(service, target)) {
        emitResultFanLX(false, QStringLiteral("未知服务：%1").arg(service));
        return false;
    }
    // 与当前服务比较用枚举：界面中文名（微信）与存储名（Wechat）指向同一个服务。
    ServiceTypeFanLX current{};
    if (serviceOfFanLX(currentServiceFanLX, current) && current == target && currentServiceLoggedIn()) {
        emitResultFanLX(false, QStringLiteral("该服务就是当前服务，无需逐项确认"));
        return false;
    }
    if (accountIdOf(service).isEmpty()) {
        emitResultFanLX(false, QStringLiteral("%1 还没有号码：请先在「开通管理」里开通").arg(service));
        return false;
    }
    // 授权来源：当前服务仍在登录态就用它；当前服务刚被注销时，退而使用本人仍在登录态的锁点（微X）。
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason) && !anchorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    const bool done = runFanLX(QStringLiteral("已确认 %1（%2）登录：微X 上层身份或同号/显式绑定关系成立")
                                   .arg(service, accountId),
                               [&] { appFanLX->confirmLogin(actor, {target, accountId.toStdString()}); });
    if (done)
        syncSessionStateFanLX();
    return done;
}

bool DomainControllerFanLX::confirmAllServices() {
    if (!appFanLX || userIdFanLX.isEmpty()) {
        emitResultFanLX(false, QStringLiteral("领域库尚未打开"));
        return false;
    }
    AccountRefFanLX anchor{};
    QString reason;
    if (!anchorFanLX(anchor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    const auto &manager = appFanLX->state().accounts;
    QStringList linked;
    QStringList skipped;
    // 逐个服务"简单确认"：微X 身份是授权来源（登微X即代表本人），
    // 下级服务之间仍保留"同号 / 显式绑定"这条老规则作为兜底。
    for (const auto service : allServicesFanLX()) {
        if (service == ServiceTypeFanLX::WeiX)
            continue; // 微X 就是授权来源本身，不需要"确认自己"
        const auto *found = manager.findAccount(userIdFanLX.toStdString(), service);
        if (!found) {
            skipped << serviceTextFanLX(service) + QStringLiteral("（未开通）");
            continue;
        }
        const auto name = serviceTextFanLX(service);
        if (!manager.isAccountSubscribed(service, found->accountId)) {
            skipped << name + QStringLiteral("（未开通）");
            continue;
        }
        if (manager.isAccountLoggedIn(service, found->accountId)) {
            linked << name;
            continue;
        }
        try {
            appFanLX->confirmLogin(anchor, {service, found->accountId});
            linked << name + (anchor.service == ServiceTypeFanLX::WeiX ? QStringLiteral("（微X上层）")
                                                                       : QStringLiteral("（同号/绑定）"));
            continue;
        } catch (const std::exception &) {
            // 既不是微X上层、也不同号也未绑定：回落到同一份密码的"简单确认"。
        }
        try {
            appFanLX->login({service, found->accountId}, loginPasswordFanLX.toStdString(), false);
            linked << name + QStringLiteral("（本人密码）");
        } catch (const std::exception &error) {
            skipped << name + QStringLiteral("（") + fromUtf8FanLX(error.what()) + QStringLiteral("）");
        }
    }
    if (linked.isEmpty() && skipped.isEmpty()) {
        emitResultFanLX(false, QStringLiteral("本人没有可联动的服务"));
        return false;
    }
    emit dataChangedFanLX();
    syncSessionStateFanLX();
    emitResultFanLX(true, QStringLiteral("登录微X后已联动本人其它服务：%1%2")
                               .arg(linked.join(QStringLiteral("、")),
                                    skipped.isEmpty() ? QString()
                                                      : QStringLiteral("；未联动：") +
                                                            skipped.join(QStringLiteral("、"))));
    return !linked.isEmpty();
}

bool DomainControllerFanLX::logoutCurrent(bool allServices) {
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    const auto service = currentServiceFanLX;
    const bool done = runFanLX(
        allServices ? QStringLiteral("已平台注销：本人全部服务的登录态均已退出，可在「账号与服务」里重新登录")
                    : QStringLiteral("已注销 %1 的登录态（其它服务不受影响），可切换到其它已登录的服务")
                          .arg(service),
        [&] { appFanLX->logout(actor, allServices); });
    // 当前服务已经退出登录态：通知聊天等订阅者断开连接，并让各页显示"未登录"。
    if (done)
        syncSessionStateFanLX();
    return done;
}

// ============================ 绑定 ============================

bool DomainControllerFanLX::bindService(const QString &service, const QString &accountId, bool remove) {
    ServiceTypeFanLX target{};
    if (!serviceOfFanLX(service, target)) {
        emitResultFanLX(false, QStringLiteral("未知服务：%1").arg(service));
        return false;
    }
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (accountId.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请先填写要绑定的对方号码（或用本人该服务的号码）"));
        return false;
    }
    return runFanLX(remove ? QStringLiteral("已解除与 %1（%2）的绑定：此后不再联动登录").arg(service, accountId)
                           : QStringLiteral("已绑定 %1（%2）：此后可用它联动登录").arg(service, accountId),
                    [&] { appFanLX->bind(actor, {target, accountId.toStdString()}, remove); });
}

// ============================ 资料与注册 ============================

bool DomainControllerFanLX::updateProfile(const QString &nickname, const QString &location) {
    // 单项修改：留空的那一项保持原值。以前两项都必填并且一起覆盖，用户想改昵称
    // 也被迫把所在地重打一遍（打错就把原值冲掉了）。
    if (nickname.trimmed().isEmpty() && location.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("昵称与所在地都为空：没有要修改的内容"));
        return false;
    }
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    const auto what = nickname.trimmed().isEmpty()
                          ? QStringLiteral("所在地 %1（昵称未改动）").arg(location)
                          : (location.trimmed().isEmpty()
                                 ? QStringLiteral("昵称 %1（所在地未改动）").arg(nickname)
                                 : QStringLiteral("昵称 %1、所在地 %2").arg(nickname, location));
    return runFanLX(QStringLiteral("资料已保存：") + what, [&] {
        appFanLX->updateProfile(actor, nickname.toStdString(), location.toStdString());
    });
}

bool DomainControllerFanLX::changePassword(const QString &oldPassword, const QString &newPassword) {
    if (newPassword.isEmpty()) {
        emitResultFanLX(false, QStringLiteral("新密码不能为空"));
        return false;
    }
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    const auto ok = runFanLX(
        QStringLiteral("密码已修改：本人各平台共用这一份密码，下次登录请用新密码"),
        [&] { appFanLX->changePassword(actor, oldPassword.toStdString(), newPassword.toStdString()); });
    if (ok)
        loginPasswordFanLX = newPassword;
    return ok;
}

// 注册微X账号（登录界面里的入口）：注册发生在登录**之前**，此时还没有打开领域库，
// 因此这里自行按 <数据根>/im-client/<服务器>_<端口>/<微X号> 派生该账号的领域库目录，
// 打开（或创建）它、写入"微X + 密码"、然后立刻关闭——随后用户就能用这个微X号登录了。
// 注册只创建上层身份：**不**捆绑任何服务账号，QQ / 微信 / 微博 登录后在内部自主开通。
bool DomainControllerFanLX::registerWeiXUser(const QString &dataRoot, const QString &host, quint16 port,
                                             const QString &weixId, const QString &nickname,
                                             const QString &birthDate, const QString &location,
                                             const QString &password, QString *detail) {
    const auto report = [detail](bool ok, const QString &text) {
        if (detail)
            *detail = text;
        return ok;
    };
    if (weixId.trimmed().isEmpty() || nickname.trimmed().isEmpty() || password.isEmpty()) {
        return report(false, QStringLiteral("微X号、昵称与登录密码都不能为空"));
    }
    if (!birthDate.trimmed().isEmpty()) {
        // 出生时间会写进快照并被 schema 校验，这里先给出可照做的提示而不是等到落盘失败。
        const auto &text = birthDate.trimmed();
        const auto valid = text.size() == 10 && text.at(4) == QLatin1Char('-') && text.at(7) == QLatin1Char('-');
        if (!valid)
            return report(false, QStringLiteral("出生时间格式应为 YYYY-MM-DD"));
    }
    try {
        const auto clientDirectory = imClientPathFanLX::clientDirectoryFanLX(
            std::filesystem::path(dataRoot.toStdWString()), host.toStdString(),
            static_cast<unsigned short>(port), weixId.toStdString());
        const auto directory = imClientPathFanLX::domainDirectoryFanLX(clientDirectory);
        PersistenceApplicationFanLX app(directory);
        const auto status = app.loadResult().status;
        if (status == LoadStatusFanLX::Empty || status == LoadStatusFanLX::UncommittedOnly) {
            app.initialize();
            app.importInitial(demoSeedFanLX());
        } else if (status != LoadStatusFanLX::Loaded) {
            return report(false, QStringLiteral("该微X号的领域库不可用（%1）：请换一个微X号或先备份该目录")
                                     .arg(fromUtf8FanLX(app.loadResult().message)));
        }
        const auto &manager = app.state().accounts;
        if (manager.hasAccount(ServiceTypeFanLX::WeiX, weixId.toStdString()))
            return report(false, QStringLiteral("微X号 %1 已存在：请直接登录，或换一个微X号").arg(weixId));
        const auto birth = birthDate.trimmed().isEmpty() ? QStringLiteral("2000-01-01") : birthDate.trimmed();
        // 内部 userId 由后台生成（界面不要求输入）：微X号唯一，因此直接由它派生即可。
        app.registerWeiXUser({"u-" + weixId.toStdString(), nickname.trimmed().toStdString(),
                              birth.toStdString(), utcNowFanLX().substr(0, 10),
                              location.trimmed().toStdString(), password.toStdString()},
                             weixId.toStdString());
        app.saveAndClose();
        return report(true, QStringLiteral("注册成功：微X号 %1 已创建（未捆绑任何下级服务）。"
                                           "请在登录界面用它登录，然后到「开通管理」里开通 QQ / 微信 / 微博。")
                                 .arg(weixId));
    } catch (const std::exception &error) {
        return report(false, QStringLiteral("注册失败：") + fromUtf8FanLX(error.what()));
    }
}

// ============================ 好友管理 ============================

bool DomainControllerFanLX::addFriend(const QString &peerAccount) {
    return sendFriendRequest(peerAccount, QString());
}

// 好友操作统一经服务端中转：申请簿与好友关系的权威副本都在服务端，
// 对方离线也能收到申请，双方看到的状态天然一致。执行后按服务端清单对账本机副本。
peerAccountFanLX::FriendOperationResultFanLX
DomainControllerFanLX::friendOperationFanLX(const AccountRefFanLX &actor, const std::string &op,
                                            const std::string &peer, const std::string &note,
                                            const std::string &via) {
    const auto attempt = [&] {
        return peerAccountFanLX::friendOperationFanLX(*appFanLX, hostFanLX.toStdString(), portFanLX, actor,
                                                      loginPasswordFanLX.toStdString(), op, peer, note, via);
    };
    auto result = attempt();
    // 服务端还不认识本人（刚开通、尚未登记）：登记一次再重试，而不是把"密码错误"甩给用户。
    if (!result.ok && result.reachable &&
        (result.reason.find("用户名或密码错误") != std::string::npos ||
         result.reason.find("尚未开通") != std::string::npos) &&
        pushAccountToServer())
        result = attempt();
    if (result.localChanged) {
        ++commitCountFanLX;
        emit dataChangedFanLX();
    }
    return result;
}

bool DomainControllerFanLX::sendFriendRequest(const QString &peerAccount, const QString &note) {
    if (peerAccount.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请填写对方在当前服务里的号码"));
        return false;
    }
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (!isSubordinateServiceFanLX(actor.service)) {
        emitResultFanLX(false, QStringLiteral("微X 是上层身份、没有自己的好友列表："
                                              "请先把当前服务切到已开通的 QQ / 微信 / 微博"));
        return false;
    }
    const auto peer = peerAccount.trimmed().toStdString();
    if (friendAccounts().contains(peerAccount.trimmed())) {
        emitResultFanLX(true, QStringLiteral("%1 已经是你在 %2 的好友").arg(peerAccount, currentServiceFanLX));
        return true;
    }
    const auto result = friendOperationFanLX(actor, "request", peer, note.trimmed().toStdString());
    if (result.ok) {
        emit dataChangedFanLX();
        emitResultFanLX(true, QStringLiteral("%1 → %2：%3（对方同意前，你们之间可以临时会话，每方向限额 %4 条）")
                                  .arg(currentServiceFanLX, peerAccount, fromUtf8FanLX(result.message))
                                  .arg(result.limit));
        return true;
    }
    if (result.reachable) {
        emitResultFanLX(false, QStringLiteral("好友申请未发出：") + fromUtf8FanLX(result.reason));
        return false;
    }
    return addFriendLocallyFanLX(actor, peer,
                                 QStringLiteral("服务端不可达（%1），单机演示模式：").arg(fromUtf8FanLX(result.reason)));
}

// 单机演示：没有服务端可中转申请时，在本机领域库直接建立关系（同服务、对端确认登录等领域规则照旧）。
bool DomainControllerFanLX::addFriendLocallyFanLX(const AccountRefFanLX &actor, const std::string &peer,
                                                  const QString &prefix) {
    const auto peerOnline = ensurePeerOnlineFanLX(actor.service, peer);
    return runFanLX(prefix + QStringLiteral("已在 %1 直接添加好友 %2（只存在于 %1 的好友分区）%3")
                                 .arg(currentServiceFanLX, QString::fromStdString(peer),
                                      peerOnline ? QStringLiteral("；本机演示已把对方置为在线（内存态）")
                                                 : QString()),
                    [&] { appFanLX->addFriend(actor, {actor.service, peer}); });
}

bool DomainControllerFanLX::respondFriendRequestFanLX(const char *op, const QString &peerAccount,
                                                      const QString &failurePrefix, const QString &success) {
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    const auto result = friendOperationFanLX(actor, op, peerAccount.trimmed().toStdString(), {});
    if (!result.ok) {
        emitResultFanLX(false, failurePrefix + fromUtf8FanLX(result.reason));
        return false;
    }
    emit dataChangedFanLX();
    emitResultFanLX(true, success);
    return true;
}

bool DomainControllerFanLX::acceptFriendRequest(const QString &peerAccount) {
    return respondFriendRequestFanLX("accept", peerAccount, QStringLiteral("同意失败："),
                                     QStringLiteral("已同意 %1 的好友申请：你们已在 %2 成为好友")
                                         .arg(peerAccount, currentServiceFanLX));
}

bool DomainControllerFanLX::rejectFriendRequest(const QString &peerAccount) {
    return respondFriendRequestFanLX("reject", peerAccount, QStringLiteral("拒绝失败："),
                                     QStringLiteral("已拒绝 %1 的好友申请").arg(peerAccount));
}

bool DomainControllerFanLX::cancelFriendRequest(const QString &peerAccount) {
    return respondFriendRequestFanLX("cancel", peerAccount, QStringLiteral("撤回失败："),
                                     QStringLiteral("已撤回发给 %1 的好友申请").arg(peerAccount));
}

DomainControllerFanLX::FriendRequestViewFanLX DomainControllerFanLX::friendRequestView() {
    FriendRequestViewFanLX view;
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason) || !isSubordinateServiceFanLX(actor.service)) {
        lastMessageFanLX = reason.isEmpty() ? QStringLiteral("微X 没有好友申请：请切到 QQ / 微信 / 微博") : reason;
        return view;
    }
    const auto result = friendOperationFanLX(actor, "list", {}, {});
    if (!result.ok) {
        lastMessageFanLX = QStringLiteral("无法从服务端读取好友申请：") + fromUtf8FanLX(result.reason);
        return view;
    }
    const auto toList = [](const std::vector<std::string> &rows) {
        QStringList out;
        for (const auto &row : rows)
            out << fromUtf8FanLX(row);
        return out;
    };
    view.reachable = true;
    view.incoming = toList(result.incoming);
    view.outgoing = toList(result.outgoing);
    view.temporary = toList(result.temporary);
    view.limit = result.limit;
    return view;
}

void DomainControllerFanLX::onFriendNoticeFanLX() {
    AccountRefFanLX actor{};
    if (!actorFanLX(actor, nullptr) || !isSubordinateServiceFanLX(actor.service))
        return;
    // list 会顺带对账好友清单：对方同意 / 删除后，本机好友列表随之更新。
    friendOperationFanLX(actor, "list", {}, {});
}

// 依据本人另一个服务的好友：源服务好友 → 归属用户 → 当前服务账号。
// 本机能换算的先给出候选（只用于展示）；真正的申请由服务端再换算一次并校验关系存在。
QStringList DomainControllerFanLX::crossServiceFriendCandidates(const QString &sourceService) {
    QStringList rows;
    ServiceTypeFanLX source{};
    AccountRefFanLX actor{};
    if (!appFanLX || !serviceOfFanLX(sourceService, source) || !chatCapableFanLX(source) ||
        !actorFanLX(actor, nullptr) || source == actor.service)
        return rows;
    const auto &manager = appFanLX->state().accounts;
    const auto *mine = manager.findAccount(userIdFanLX.toStdString(), source);
    if (!mine || !manager.isAccountSubscribed(source, mine->accountId))
        return rows;
    const auto current = friendAccounts();
    IdentityResolverFanLX resolver(manager);
    const auto &directory = appFanLX->state().friends.directory(source);
    for (const auto &peerKey : directory.peers(resolver.accountKey(source, mine->accountId))) {
        const auto peerUser = resolver.userForAccount(peerKey);
        const auto *sourcePeer = manager.findAccount(peerUser, source);
        if (!sourcePeer)
            continue;
        const auto *target = manager.findAccount(peerUser, actor.service);
        // 本机副本可能不认识对方在当前服务的号码（远端导入的账号只带一个服务）：交给服务端换算。
        QString status;
        if (!target)
            status = QStringLiteral("号码由服务端换算");
        else if (current.contains(QString::fromStdString(target->accountId)))
            status = QStringLiteral("已是好友");
        else
            status = QStringLiteral("可申请");
        rows << QStringLiteral("%1 ｜ → %2 ｜ %3")
                    .arg(QString::fromStdString(sourcePeer->accountId),
                         target ? QString::fromStdString(target->accountId) : QStringLiteral("—"), status);
    }
    return rows;
}

bool DomainControllerFanLX::addFriendFromService(const QString &sourceService, const QString &sourcePeerAccount,
                                                 const QString &note) {
    ServiceTypeFanLX source{};
    if (!serviceOfFanLX(sourceService, source) || !chatCapableFanLX(source)) {
        emitResultFanLX(false, QStringLiteral("来源服务必须是 QQ / 微信 / 微博：%1").arg(sourceService));
        return false;
    }
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (!isSubordinateServiceFanLX(actor.service) || source == actor.service) {
        emitResultFanLX(false, QStringLiteral("请先把当前服务切到要添加好友的那个服务（与来源服务不同）"));
        return false;
    }
    const auto sourcePeer = sourcePeerAccount.trimmed().toStdString();
    if (sourcePeer.empty()) {
        emitResultFanLX(false, QStringLiteral("请选择或填写来源服务里的好友号码"));
        return false;
    }
    const auto result = friendOperationFanLX(actor, "request", sourcePeer, note.trimmed().toStdString(),
                                             serviceNameFanLX(source));
    if (result.ok) {
        emit dataChangedFanLX();
        emitResultFanLX(true, QStringLiteral("依据 %1 好友 %2 换算出 %3 号码 %4：%5")
                                  .arg(sourceService, sourcePeerAccount, currentServiceFanLX,
                                       QString::fromStdString(result.peer), fromUtf8FanLX(result.message)));
        return true;
    }
    if (result.reachable) {
        emitResultFanLX(false, QStringLiteral("跨服务好友申请未发出：") + fromUtf8FanLX(result.reason));
        return false;
    }
    // 服务端不可达：本机按同一规则换算（源服务好友 → 归属用户 → 当前服务账号），单机演示直接建立关系。
    const auto &manager = appFanLX->state().accounts;
    const auto *mine = manager.findAccount(userIdFanLX.toStdString(), source);
    IdentityResolverFanLX resolver(manager);
    if (!mine || !manager.hasAccount(source, sourcePeer) ||
        !appFanLX->state().friends.directory(source).contains(resolver.accountKey(source, mine->accountId),
                                                              resolver.accountKey(source, sourcePeer))) {
        emitResultFanLX(false, QStringLiteral("%1 不是本人在 %2 的好友").arg(sourcePeerAccount, sourceService));
        return false;
    }
    const auto *target =
        manager.findAccount(manager.accountByAccountId(source, sourcePeer).userId, actor.service);
    if (!target) {
        emitResultFanLX(false, QStringLiteral("对方还没有开通 %1：请让对方先开通，再依据 %2 好友添加")
                                   .arg(currentServiceFanLX, sourceService));
        return false;
    }
    return addFriendLocallyFanLX(actor, target->accountId,
                                 QStringLiteral("服务端不可达（%1），单机演示模式：依据 %2 好友 %3 换算，")
                                     .arg(fromUtf8FanLX(result.reason), sourceService, sourcePeerAccount));
}

bool DomainControllerFanLX::removeFriend(const QString &peerAccount) {
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    // 领域层的 removeFriend 返回"是否真的删掉了一条"，应用门面把它丢掉了。
    // 界面必须区分"删掉了"和"本来就不是好友"，否则用户会以为操作没生效。
    if (!friendAccounts().contains(peerAccount)) {
        emitResultFanLX(false, QStringLiteral("%1 不在 %2 的好友列表里：无需删除")
                                   .arg(peerAccount, currentServiceFanLX));
        return false;
    }
    // 先让服务端解除（权威副本，对方列表随之更新），再在本机删除；服务端不可达时只删本机。
    const auto result = friendOperationFanLX(actor, "remove", peerAccount.toStdString(), {});
    if (!result.ok && result.reachable && result.reason.find("不是你的好友") == std::string::npos) {
        emitResultFanLX(false, QStringLiteral("删除失败：") + fromUtf8FanLX(result.reason));
        return false;
    }
    if (!friendAccounts().contains(peerAccount)) {
        emit dataChangedFanLX();
        emitResultFanLX(true, QStringLiteral("已删除好友 %1（服务端已解除，对方列表同步更新）").arg(peerAccount));
        return true;
    }
    return runFanLX(QStringLiteral("已删除好友 %1（双向关系与备注一并清除）").arg(peerAccount),
                    [&] { appFanLX->removeFriend(actor, {actor.service, peerAccount.toStdString()}); });
}

bool DomainControllerFanLX::updateFriendRemark(const QString &peerAccount, const QString &remark,
                                               const QString &tag) {
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    return runFanLX(QStringLiteral("已更新 %1 的备注/标签（只影响本人视角）").arg(peerAccount), [&] {
        appFanLX->updateRemark(actor, {actor.service, peerAccount.toStdString()}, remark.toStdString(),
                               tag.toStdString());
    });
}

QStringList DomainControllerFanLX::queryCommonFriends(const QString &peerAccount) {
    if (peerAccount.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请填写对方平台账号"));
        return {};
    }
    AccountRefFanLX actor{};
    QString reason;
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return {};
    }
    try {
        const auto result =
            appFanLX->queryCommonFriends(actor, {actor.service, peerAccount.toStdString()});
        QStringList rows;
        for (const auto &item : result)
            rows << QString::fromStdString(item);
        emit dataChangedFanLX();
        emitResultFanLX(true, QStringLiteral("%1 与 %2 的同平台共同好友 %3 人")
                                 .arg(currentAccountId(), peerAccount)
                                 .arg(rows.size()));
        return rows;
    } catch (const std::exception &error) {
        emitResultFanLX(false, fromUtf8FanLX(error.what()));
        return {};
    }
}

QStringList DomainControllerFanLX::crossServiceQueryFanLX(const QString &targetService, bool recommendations) {
    ServiceTypeFanLX target{};
    if (!serviceOfFanLX(targetService, target) || !isSubordinateServiceFanLX(target)) {
        emitResultFanLX(false, QStringLiteral("跨服务查询的目标必须是 QQ / 微信 / 微博：%1").arg(targetService));
        return {};
    }
    AccountRefFanLX source{};
    QString reason;
    if (!actorFanLX(source, &reason)) {
        emitResultFanLX(false, reason);
        return {};
    }
    if (target == source.service) {
        emitResultFanLX(false, QStringLiteral("跨服务查询需要另一个服务的身份"));
        return {};
    }
    const auto &manager = appFanLX->state().accounts;
    const auto *targetAccount = manager.findAccount(userIdFanLX.toStdString(), target);
    if (!targetAccount) {
        emitResultFanLX(false, QStringLiteral("本人在 %1 还没有身份：请先在「开通管理」里开通").arg(targetService));
        return {};
    }
    if (!manager.isAccountLoggedIn(target, targetAccount->accountId)) {
        emitResultFanLX(false, QStringLiteral("%1 未登录：请先在「登录管理」里确认登录").arg(targetService));
        return {};
    }
    try {
        // 两个服务各读自己的好友分区（分别存储），因此这里的结果是"跨服务的推荐/交集"，
        // 而不是把两份好友混在一起。
        const auto result = appFanLX->queryCrossService(source, {target, targetAccount->accountId},
                                                        recommendations);
        QStringList rows;
        for (const auto &item : result)
            rows << QString::fromStdString(item);
        emit dataChangedFanLX();
        emitResultFanLX(true,
                        QStringLiteral("%1 上共 %2 人（%3）")
                            .arg(targetService)
                            .arg(rows.size())
                            .arg(recommendations
                                     ? QStringLiteral("本人另一服务的好友，可在 %1 添加").arg(targetService)
                                     : QStringLiteral("与本人另一服务都互为好友的人")));
        return rows;
    } catch (const std::exception &error) {
        emitResultFanLX(false, fromUtf8FanLX(error.what()));
        return {};
    }
}

QStringList DomainControllerFanLX::queryCrossServiceRecommend(const QString &targetService) {
    return crossServiceQueryFanLX(targetService, true);
}

QStringList DomainControllerFanLX::queryCrossServiceCommon(const QString &targetService) {
    return crossServiceQueryFanLX(targetService, false);
}

// ============================ 群管理 ============================

bool DomainControllerFanLX::createGroup(const QString &service, const QString &groupId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    // 创建者自动成为群主（领域层 createGroup 会给首个成员 Owner 角色），
    // 因此新建出来的群一定有主，不存在"无主群"。
    return runFanLX(QStringLiteral("已创建群 %1（%2 模式策略），本人为群主").arg(groupId, service),
                    [&] { appFanLX->createGroup(actor, key); });
}

bool DomainControllerFanLX::renameGroup(const QString &service, const QString &groupId, const QString &name) {
    GroupKeyFanLX key{};
    QString reason;
    if (!groupKeyFanLX(service, groupId, key, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (name.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("新群名不能为空"));
        return false;
    }
    AccountRefFanLX actor{};
    if (!actorFanLX(actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    return runFanLX(QStringLiteral("群 %1 已更名为 %2").arg(groupId, name),
                    [&] { appFanLX->renameGroup(actor, key, name.toStdString()); });
}

bool DomainControllerFanLX::applyToJoinGroup(const QString &service, const QString &groupId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    return runFanLX(QStringLiteral("已提交入群申请：等待群主或管理员批准（QQ 群特有流程）"),
                    [&] { appFanLX->applyToJoin(actor, key); });
}

bool DomainControllerFanLX::approveGroupApplication(const QString &service, const QString &groupId,
                                                    const QString &accountId, bool reject) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    const auto target = accountId.trimmed().isEmpty() ? currentAccountId() : accountId;
    return runFanLX(reject ? QStringLiteral("已拒绝 %1 的入群申请").arg(target)
                           : QStringLiteral("已批准 %1 入群").arg(target),
                    [&] {
                        appFanLX->approveApplication(actor, key, {actor.service, target.toStdString()},
                                                     reject);
                    });
}

bool DomainControllerFanLX::inviteGroupMember(const QString &service, const QString &groupId,
                                              const QString &accountId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (accountId.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请填写被推荐人（被邀请人）的平台账号"));
        return false;
    }
    const AccountRefFanLX invitee{actor.service, accountId.trimmed().toStdString()};
    // 两种群的"邀请"落到各自的流程里：
    //   QQ 群   ：替对方提交入群申请，由群主/管理员审批（申请-审批制）；
    //   微信群 ：向对方发出推荐，对方本人确认后入群（推荐-确认制）。
    // 注意看的是群当前的管理特色（策略），而不是群所在的命名空间：群可以动态切换特色。
    bool qqStyle = key.mode == GroupModeFanLX::QQ;
    try {
        qqStyle = appFanLX->state().groups.get(key).currentMode() == GroupModeFanLX::QQ;
    } catch (const std::exception &) {
    }
    if (qqStyle)
        return runFanLX(QStringLiteral("已邀请 %1：已为对方提交入群申请，等待群主或管理员审批（QQ 群）").arg(accountId),
                        [&] { appFanLX->inviteToApply(actor, key, invitee); });
    return runFanLX(QStringLiteral("已向 %1 发出入群推荐：需对方确认后入群（微信群）").arg(accountId),
                    [&] { appFanLX->inviteMember(actor, key, invitee); });
}

bool DomainControllerFanLX::confirmGroupInvitation(const QString &service, const QString &groupId,
                                                   const QString &accountId, bool reject) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    // 被推荐人本人确认：领域接口以 actor 自身为被邀请人，因此这里不接受第三方账号。
    (void)accountId;
    return runFanLX(reject ? QStringLiteral("已拒绝入群推荐：推荐记录标记为 Rejected")
                           : QStringLiteral("已确认入群推荐：本人进入该群"),
                    [&] { appFanLX->confirmInvitation(actor, key, reject); });
}

bool DomainControllerFanLX::leaveGroup(const QString &service, const QString &groupId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    return runFanLX(QStringLiteral("已退出群 %1").arg(groupId), [&] { appFanLX->leave(actor, key); });
}

bool DomainControllerFanLX::kickGroupMember(const QString &service, const QString &groupId,
                                            const QString &accountId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (accountId.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请填写要移出的成员账号"));
        return false;
    }
    return runFanLX(QStringLiteral("已将 %1 移出群 %2（挨 T）").arg(accountId, groupId), [&] {
        appFanLX->kick(actor, key, {actor.service, accountId.toStdString()});
    });
}

bool DomainControllerFanLX::assignGroupAdmin(const QString &service, const QString &groupId,
                                             const QString &accountId, bool revoke) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (accountId.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请填写目标成员账号"));
        return false;
    }
    return runFanLX(revoke ? QStringLiteral("已撤销 %1 的管理员权限").arg(accountId)
                           : QStringLiteral("已任命 %1 为管理员（QQ 群管理员制度）").arg(accountId),
                    [&] {
                        appFanLX->assignAdmin(actor, key, {actor.service, accountId.toStdString()}, revoke);
                    });
}

bool DomainControllerFanLX::transferGroupOwner(const QString &service, const QString &groupId,
                                               const QString &accountId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (accountId.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请填写新群主账号"));
        return false;
    }
    return runFanLX(QStringLiteral("已把群主转让给 %1：原群主降为普通成员").arg(accountId), [&] {
        appFanLX->transferOwnership(actor, key, {actor.service, accountId.toStdString()});
    });
}

bool DomainControllerFanLX::dissolveGroup(const QString &service, const QString &groupId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    return runFanLX(QStringLiteral("已解散群 %1：群记录作为历史片段归档，成员数据不丢失").arg(groupId),
                    [&] { appFanLX->dissolve(actor, key); });
}

bool DomainControllerFanLX::switchGroupMode(const QString &service, const QString &groupId, bool toQqMode) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    // service 是群**登记所在**的命名空间（QQ 群用 QQ 身份，微信群用微信身份）；
    // 切换只替换策略对象，群键、成员、加入序号与子群记录都原地保留。
    // 因此"再切回来"仍要用原来那个命名空间，而不是切换后的目标模式。
    return runFanLX(QStringLiteral("群 %1 的管理特色已切换为 %2：加入方式、特权账号与子群能力立即"
                                   "按新策略生效，群身份与成员数据原地保留")
                        .arg(groupId, toQqMode ? QStringLiteral("QQ 群") : QStringLiteral("微信群")),
                    [&] {
                        appFanLX->switchPolicy(actor, key, toQqMode ? GroupModeFanLX::QQ
                                                                   : GroupModeFanLX::Wechat);
                    });
}

bool DomainControllerFanLX::createDiscussionGroup(const QString &service, const QString &groupId,
                                                  const QString &discussionId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (discussionId.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("请填写讨论组（子群）编号"));
        return false;
    }
    return runFanLX(QStringLiteral("已在群 %1 下创建讨论组 %2").arg(groupId, discussionId),
                    [&] { appFanLX->createDiscussion(actor, key, discussionId.toStdString()); });
}

bool DomainControllerFanLX::joinDiscussionGroup(const QString &service, const QString &groupId,
                                                const QString &discussionId, const QString &accountId) {
    GroupKeyFanLX key{};
    AccountRefFanLX actor{};
    QString reason;
    if (!groupContextFanLX(service, groupId, key, actor, &reason)) {
        emitResultFanLX(false, reason);
        return false;
    }
    if (discussionId.trimmed().isEmpty() || accountId.trimmed().isEmpty()) {
        emitResultFanLX(false, QStringLiteral("讨论组编号与成员账号都不能为空"));
        return false;
    }
    return runFanLX(QStringLiteral("已把 %1 加入子群 %2").arg(accountId, discussionId), [&] {
        appFanLX->joinDiscussion(actor, key, discussionId.toStdString(),
                                 {actor.service, accountId.toStdString()});
    });
}

} // namespace fanlxGui

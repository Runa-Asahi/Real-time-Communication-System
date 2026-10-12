#include "gui/ChatControllerFanLX.h"
#include "account/ServiceTypeFanLX.h"
#include "db/ChatRepositoryFanLX.h"
#include "network/ClientDataPathFanLX.h"
#include <nlohmann/json.hpp>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QEventLoop>
#include <QThread>
#include <algorithm>

namespace fanlxGui {

namespace {

// 服务名规范化：界面里到处是中文显示名（微X / 微信 / 微博），而网络协议与存储目录用的是
// 存储名（WeiX / Wechat / Weibo）。两套写法都在这里收敛成存储名，避免"切到微信之后
// 控制器还在拿『微信』去建连"这类只在界面上才暴露的错位。
QString normalizeServiceFanLX(const QString &service) {
    ServiceTypeFanLX parsed{};
    if (serviceFromNameFanLX(service.toStdString(), parsed))
        return QString::fromStdString(serviceNameFanLX(parsed));
    return service;
}

// 目录应答是 JSON 文本；解析失败时明确报错，不静默丢弃。
bool parseDirectoryFanLX(const std::string &text, QList<QString> &friends, QList<QString> &groups,
                         QList<QPair<QString, int>> &temporary, int &limit) {
    try {
        const auto parsed = nlohmann::json::parse(text);
        if (parsed.value("schema", std::string()) != "fanlx-directory-1")
            return false;
        for (const auto &item : parsed.at("friends"))
            friends.push_back(QString::fromStdString(item.get<std::string>()));
        for (const auto &item : parsed.at("groups"))
            groups.push_back(QString::fromStdString(item.get<std::string>()));
        if (parsed.contains("temporary") && parsed.at("temporary").is_array()) {
            for (const auto &row : parsed.at("temporary")) {
                const auto peer = QString::fromStdString(row.at("peer").get<std::string>());
                const auto remaining = row.value("remaining", 0);
                temporary.push_back({peer, remaining});
            }
        }
        limit = parsed.value("limit", 10);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

// 服务器时间未知时显示空串（界面明确标注"原始时间未知"），不伪造成本地时间。
QString serverTimeTextFanLX(const std::string &acceptedAtUtc) {
    return acceptedAtUtc.empty() ? QString() : QString::fromStdString(acceptedAtUtc);
}

QString truncateFanLX(const QString &text, int limit = 60) {
    return text.size() <= limit ? text : text.left(limit) + QStringLiteral("…");
}

} // namespace

ChatControllerFanLX::ChatControllerFanLX(QObject *parent) : QObject(parent) {
    // 轮询周期 30 ms：既保证界面及时刷新，又不让主线程陷入忙等。
    timerFanLX = new QTimer(this);
    timerFanLX->setInterval(30);
    connect(timerFanLX, &QTimer::timeout, this, &ChatControllerFanLX::drainEventsFanLX);
}

ChatControllerFanLX::~ChatControllerFanLX() { stop(); }

void ChatControllerFanLX::configure(const QString &host, quint16 port, const QString &account,
                                   const QString &password, const QString &dataRoot, bool useChatDatabase,
                                   const QString &service) {
    hostFanLX = host;
    portFanLX = port;
    accountFanLX = account;
    passwordFanLX = password;
    dataRootFanLX = dataRoot;
    useChatDatabaseFanLX = useChatDatabase;
    // 界面传进来的可能是中文显示名（微信 / 微博），网络与目录只认存储名：先规范化。
    if (!service.isEmpty())
        serviceFanLX = normalizeServiceFanLX(service);
}

QString ChatControllerFanLX::serviceDisplayName() const {
    ServiceTypeFanLX parsed{};
    if (serviceFromNameFanLX(serviceFanLX.toStdString(), parsed))
        return QString::fromStdString(serviceDisplayNameFanLX(parsed));
    return serviceFanLX;
}

QString ChatControllerFanLX::dataDirectory() const {
    if (clientFanLX)
        return QString::fromStdWString(clientFanLX->dataDirectory().wstring());
    // 尚未启动：按同一套派生规则给出预览，便于界面在登录前就说明"数据会存到哪"。
    if (dataRootFanLX.isEmpty() || hostFanLX.isEmpty() || portFanLX == 0 || accountFanLX.isEmpty())
        return QString();
    try {
        // 聊天存储目录 = <账号目录>/<服务名>：一个服务一份聊天库与一把锁（QQ/微信/微博 分开）。
        const auto derived = imClientPathFanLX::chatDirectoryFanLX(
            std::filesystem::path(dataRootFanLX.toStdWString()), hostFanLX.toStdString(), portFanLX,
            serviceFanLX.toStdString(), accountFanLX.toStdString());
        return QString::fromStdWString(derived.wstring());
    } catch (const std::exception &) {
        return QString();
    }
}

bool ChatControllerFanLX::start(QString *detail) {
    stop();
    if (hostFanLX.isEmpty() || portFanLX == 0 || accountFanLX.isEmpty() || passwordFanLX.isEmpty() ||
        dataRootFanLX.isEmpty()) {
        if (detail) *detail = QStringLiteral("服务器地址、端口、用户名、密码与数据根目录都必填");
        return false;
    }
    // 存储目录由"服务器身份 + 服务 + 登录账号"派生，不接受调用方直接指定到账号目录。
    std::filesystem::path directory;
    try {
        directory = imClientPathFanLX::chatDirectoryFanLX(
            std::filesystem::path(dataRootFanLX.toStdWString()), hostFanLX.toStdString(), portFanLX,
            serviceFanLX.toStdString(), accountFanLX.toStdString());
    } catch (const std::exception &error) {
        lastErrorFanLX = QString::fromUtf8(error.what());
        if (detail) *detail = lastErrorFanLX;
        emit errorFanLX(lastErrorFanLX);
        return false;
    }
    ImClientFanLX::ChatDbConfigFanLX chatDb;
    chatDb.enabled = useChatDatabaseFanLX;
    try {
        clientFanLX = std::make_unique<ImClientFanLX>(hostFanLX.toStdString(), portFanLX,
                                                      accountFanLX.toStdString(), passwordFanLX.toStdString(),
                                                      directory, chatDb, serviceFanLX.toStdString());
    } catch (const std::exception &error) {
        // 构造失败（认证失败、库归属不符、目录被占用等）必须原样报告，不进入"假装在线"状态。
        lastErrorFanLX = QString::fromUtf8(error.what());
        if (detail) *detail = lastErrorFanLX;
        emit errorFanLX(lastErrorFanLX);
        return false;
    }
    sessionIdFanLX = QString::fromStdString(clientFanLX->sessionInstanceId());
    startedFanLX = true;
    stateFanLX = ConnectionStateFanLX::Connecting;
    emit stateChangedFanLX(stateFanLX, QStringLiteral("连接中"));
    timerFanLX->start();
    clientFanLX->submitDirectory();
    requestConversations();
    return true;
}

// 聊天跟随"当前服务"：领域层切换当前服务后，界面用本方法把聊天连接换成该服务的身份。
// service 既接受界面中文名（QQ / 微信 / 微博），也接受存储名（QQ / Wechat / Weibo）。
bool ChatControllerFanLX::switchIdentity(const QString &service, const QString &accountId, QString *detail) {
    stop();
    if (service.isEmpty() || accountId.isEmpty()) {
        if (detail) *detail = QStringLiteral("当前服务没有可用的聊天身份");
        return false;
    }
    const auto normalized = normalizeServiceFanLX(service);
    if (normalized != QStringLiteral("QQ") && normalized != QStringLiteral("Wechat") &&
        normalized != QStringLiteral("Weibo")) {
        if (detail)
            *detail = normalized == QStringLiteral("WeiX")
                          ? QStringLiteral("微X 是上层登录身份，没有聊天通道：请开通并切到 QQ / 微信 / 微博")
                          : QStringLiteral("%1 没有聊天通道：请切到 QQ / 微信 / 微博").arg(service);
        return false;
    }
    configure(hostFanLX, portFanLX, accountId, passwordFanLX, dataRootFanLX, useChatDatabaseFanLX,
              normalized);
    return start(detail);
}

// 认证结果等待：服务器可能"连上了但拒绝登录"（用户名或密码错误 / 未开通该服务），
// 界面必须在打开主窗口之前拿到这个结论，并留在登录界面重试。
bool ChatControllerFanLX::waitForLogin(int timeoutMs, QString *reason) {
    if (!clientFanLX) {
        if (reason) *reason = QStringLiteral("聊天客户端尚未启动");
        return false;
    }
    const auto deadline = QDateTime::currentMSecsSinceEpoch() + std::max(0, timeoutMs);
    for (;;) {
        drainEventsFanLX();
        if (clientFanLX->online())
            return true;
        if (clientFanLX->authenticationRejected()) {
            const auto text = QString::fromUtf8(clientFanLX->authenticationRejectionReason().c_str());
            if (reason) *reason = text.isEmpty() ? QStringLiteral("服务器拒绝登录") : text;
            return false;
        }
        if (QDateTime::currentMSecsSinceEpoch() >= deadline) {
            const auto text = QString::fromUtf8(clientFanLX->error().c_str());
            if (reason)
                *reason = text.isEmpty() ? QStringLiteral("连接服务器超时：请检查地址、端口与服务端是否已启动")
                                         : text;
            return false;
        }
        // 继续跑事件循环：等待期间窗口仍可拖动、可取消，不出现"未响应"。
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
}

void ChatControllerFanLX::stop() {
    if (timerFanLX)
        timerFanLX->stop();
    if (clientFanLX) {
        // 关闭顺序：先停止接收新命令并回收工作线程（客户端析构内部完成），再清空界面状态。
        clientFanLX.reset();
    }
    if (startedFanLX)
        emit stateChangedFanLX(ConnectionStateFanLX::Disconnected, QStringLiteral("已断开"));
    startedFanLX = false;
    stateFanLX = ConnectionStateFanLX::Disconnected;
    cancelSearch();
    authorizedFriendsFanLX.clear(); authorizedGroupsFanLX.clear();
    temporaryPeersFanLX.clear();
    openedTemporaryFanLX.clear();
    conversationsFanLX.clear();
    messagesFanLX.clear();
    currentKeyFanLX.clear();
    historyHasMoreFanLX = false;
    currentQueryIdFanLX = 0;
    historyBeforeFanLX = 0;
    historySenderFanLX.clear();
    historyKeywordFanLX.clear();
    lastErrorFanLX.clear();
    emit conversationsChangedFanLX();
    emit messagesChangedFanLX();
}

bool ChatControllerFanLX::online() const { return clientFanLX && clientFanLX->online(); }
void ChatControllerFanLX::reconnect() {
    if (clientFanLX)
        clientFanLX->reconnect();
}
QString ChatControllerFanLX::storageBackend() const {
    return clientFanLX ? QString::fromStdString(clientFanLX->storageBackend()) : QStringLiteral("(未连接)");
}
QString ChatControllerFanLX::lastError() const {
    if (clientFanLX && !clientFanLX->error().empty())
        return QString::fromStdString(clientFanLX->error());
    return lastErrorFanLX;
}

QString ChatControllerFanLX::openTemporaryConversation(const QString &peer) {
    if (peer.isEmpty() || peer == accountFanLX)
        return {};
    const auto key = QString::fromStdString(chatDbFanLX::conversationKeyFanLX(
        chatDbFanLX::ScopeFanLX::Direct, accountFanLX.toStdString(), peer.toStdString()));
    // 记住这个对端：applyConversationsFanLX 会为它加一行，即使尚无历史。
    if (!openedTemporaryFanLX.contains(peer))
        openedTemporaryFanLX.push_back(peer);
    openConversation(key);
    // 立刻刷新列表：让这个会话以 [临时] 标志出现。
    requestConversations();
    return key;
}

int ChatControllerFanLX::temporaryRemaining(const QString &peer) const {
    for (const auto &item : temporaryPeersFanLX)
        if (item.first == peer)
            return item.second;
    // 不在目录里（对端不存在 / 自己未登录该服务 / 服务端尚未推送目录）：保守起见视为满额。
    return 0;
}

int ChatControllerFanLX::directKindFanLX(const QString &peer) const {
    if (authorizedFriendsFanLX.contains(peer))
        return -1;
    return temporaryRemaining(peer);
}


qint64 ChatControllerFanLX::totalUnread() const {
    qint64 total = 0;
    for (const auto &item : conversationsFanLX)
        total += item.unread;
    return total;
}

bool ChatControllerFanLX::sendText(const QString &conversationKey, qint64 sequence, const QString &text) {
    lastRejectionFanLX.clear();
    if (!clientFanLX || text.trimmed().isEmpty())
        return false;
    if (conversationKey.isEmpty()) {
        lastRejectionFanLX = QStringLiteral("请先选择一个会话");
        emit errorFanLX(lastRejectionFanLX);
        return false;
    }
    // 收件人由会话键在客户端层统一推导：控制器不再自己解析键，避免把消息发给自己。
    std::string requestId;
    bool accepted = false;
    try {
        accepted = clientFanLX->submitSendToConversation(conversationKey.toStdString(), text.toStdString(),
                                                        requestId);
    } catch (const std::exception &error) {
        // 提交失败（正文超长/含控制字符/非法 UTF-8、库不可用、会话键异常）必须原样报告，
        // 界面保留输入文本。这里的原因会经 lastRejection() 传给窗口，不被通用文案覆盖。
        lastRejectionFanLX = QString::fromUtf8(error.what());
        emit errorFanLX(lastRejectionFanLX);
        return false;
    }
    if (!accepted)
        emit busyChangedFanLX(true); // 队列满：提示繁忙，输入文本由界面保留
    (void)sequence;
    return accepted;
}

void ChatControllerFanLX::openConversation(const QString &conversationKey) {
    if (currentKeyFanLX == conversationKey)
        return;
    // 切换会话必须作废旧查询，避免过期结果覆盖新会话的历史。
    if (clientFanLX && currentQueryIdFanLX > 0)
        clientFanLX->cancelQuery(currentQueryIdFanLX);
    currentKeyFanLX = conversationKey;
    messagesFanLX.clear();
    historyBeforeFanLX = 0;
    historyHasMoreFanLX = false;
    hasNewMessageFanLX = false;
    historySenderFanLX.clear();
    historyKeywordFanLX.clear();
    emit messagesChangedFanLX();
    requestLatest();
}

bool ChatControllerFanLX::requestHistory(const QString &conversationKey, const QString &sender,
                                        const QString &keyword, int limit, qint64 beforeSequence) {
    if (!clientFanLX || (conversationKey.isEmpty() && sender.isEmpty() && keyword.isEmpty()))
        return false;
    chatDbFanLX::HistoryQueryFanLX query;
    query.conversationKey = conversationKey.toStdString();
    query.sender = sender.toStdString();
    query.keyword = keyword.toStdString();
    query.limit = limit;
    query.beforeSequence = beforeSequence;
    query.ascending = false; // 先取最新一页，再转换为界面时间升序。
    qint64 queryId = 0;
    bool accepted = false;
    try {
        accepted = clientFanLX->submitHistory(query, queryId);
    } catch (const std::exception &error) {
        emit errorFanLX(QString::fromUtf8(error.what()));
        return false;
    }
    if (!accepted)
        return false;
    if (currentQueryIdFanLX > 0) clientFanLX->cancelQuery(currentQueryIdFanLX);
    currentQueryIdFanLX = queryId;
    queryPrependFanLX = beforeSequence > 0;
    historySenderFanLX = sender;
    historyKeywordFanLX = keyword;
    historyLimitFanLX = limit;
    return true;
}

bool ChatControllerFanLX::requestLatest() {
    if (currentKeyFanLX.isEmpty())
        return false;
    historyBeforeFanLX = 0;
    followingLatestFanLX = true;
    return requestHistory(currentKeyFanLX, historySenderFanLX, historyKeywordFanLX, historyLimitFanLX, 0);
}

bool ChatControllerFanLX::requestEarlier() {
    if (currentKeyFanLX.isEmpty() || !historyHasMoreFanLX || historyBeforeFanLX <= 0)
        return false;
    followingLatestFanLX = false;
    return requestHistory(currentKeyFanLX, historySenderFanLX, historyKeywordFanLX, historyLimitFanLX,
                          historyBeforeFanLX);
}

bool ChatControllerFanLX::requestConversations() {
    if (!clientFanLX)
        return false;
    return clientFanLX->submitConversations();
}

bool ChatControllerFanLX::requestDirectory() {
    if (!clientFanLX)
        return false;
    return clientFanLX->submitDirectory();
}

bool ChatControllerFanLX::searchHistory(const QString &key, const QString &sender, const QString &keyword,
                                      int limit, const QString &fromUtc, const QString &toUtc) {
    cancelSearch();
    if (!clientFanLX) return false;
    searchQueryFanLX = {};
    searchQueryFanLX.conversationKey = key.toStdString();
    searchQueryFanLX.sender = sender.toStdString();
    searchQueryFanLX.keyword = keyword.toStdString();
    searchQueryFanLX.limit = limit;
    searchQueryFanLX.fromAcceptedAtUtc = fromUtc.toStdString();
    searchQueryFanLX.toAcceptedAtUtc = toUtc.toStdString();
    const auto accepted = clientFanLX->submitHistory(searchQueryFanLX, searchQueryIdFanLX);
    if (!accepted) searchQueryIdFanLX = 0;
    return accepted;
}

bool ChatControllerFanLX::searchEarlier() {
    if (!clientFanLX || !searchPageFanLX.hasMore) return false;
    if (searchQueryFanLX.conversationKey.empty()) {
        searchQueryFanLX.beforeAcceptedAtUtc = searchPageFanLX.nextBeforeAcceptedAtUtc;
        searchQueryFanLX.beforeLocalSequence = searchPageFanLX.nextBeforeLocalSequence;
    } else searchQueryFanLX.beforeSequence = searchPageFanLX.nextBeforeSequence;
    if (searchQueryIdFanLX) clientFanLX->cancelQuery(searchQueryIdFanLX);
    const auto accepted = clientFanLX->submitHistory(searchQueryFanLX, searchQueryIdFanLX);
    if (!accepted) searchQueryIdFanLX = 0;
    return accepted;
}

void ChatControllerFanLX::cancelSearch() {
    if (clientFanLX && searchQueryIdFanLX) clientFanLX->cancelQuery(searchQueryIdFanLX);
    searchQueryIdFanLX = 0;
    searchPageFanLX = {};
}

void ChatControllerFanLX::markRead(const QString &conversationKey, qint64 sequence) {
    if (clientFanLX && !conversationKey.isEmpty() && sequence > 0)
        clientFanLX->submitMarkRead(conversationKey.toStdString(), sequence);
}

bool ChatControllerFanLX::currentConversationIsGroup() const {
    for (const auto &item : conversationsFanLX)
        if (item.key == currentKeyFanLX)
            return item.group;
    // 列表里还没有这一项（例如刚打开、会话摘要尚未回填）时按会话键前缀判断。
    return currentKeyFanLX.startsWith(QStringLiteral("g|"));
}

QString ChatControllerFanLX::currentConversationKindText() const {
    if (currentKeyFanLX.isEmpty())
        return {};
    return currentConversationIsGroup() ? QStringLiteral("群") : QStringLiteral("好友");
}

QString ChatControllerFanLX::statusOfFanLX(chatDbFanLX::MessageStateFanLX state, bool self) {
    using StateFanLX = chatDbFanLX::MessageStateFanLX;
    switch (state) {
    case StateFanLX::Pending: return QStringLiteral("待发送");
    case StateFanLX::SentAck: return QStringLiteral("服务器已接收");
    case StateFanLX::Failed: return QStringLiteral("发送失败");
    case StateFanLX::Received: return self ? QStringLiteral("已发送") : QString();
    }
    return {};
}

void ChatControllerFanLX::applyConversationsFanLX(
    const std::vector<chatDbFanLX::ConversationRowFanLX> &rows) {
    conversationsFanLX.clear();
    for (const auto &row : rows) {
        ConversationItemFanLX item;
        item.key = QString::fromStdString(row.conversationKey);
        item.group = row.scope == chatDbFanLX::ScopeFanLX::Group;
        // 对端标题从会话键推导：g|<群号> 或 d|<账号A>|<账号B>。
        const auto parts = item.key.split(QLatin1Char('|'));
        if (item.group)
            item.title = parts.size() > 1 ? parts.at(1) : item.key;
        else {
            const auto selfIndex = parts.size() > 1 && parts.at(1) == accountFanLX ? 2 : 1;
            item.title = parts.size() > selfIndex ? parts.at(selfIndex) : item.key;
        }
        item.unread = row.unreadCount;
        item.subtitle = truncateFanLX(QString::fromStdString(row.lastMessageBody));
        item.timeText = QString(); // 会话摘要不携带服务器时间：界面显示"原始时间未知"而不是本地时间
        // 单聊：判断对端是好友还是临时。
        const auto tempRemaining = item.group ? -1 : directKindFanLX(item.title);
        item.temporaryRemaining = tempRemaining;
        applyKindFanLX(item, item.group, tempRemaining);
        conversationsFanLX.push_back(item);
    }
    // 尚无历史的授权联系人和群也必须可点击，才能发起第一条消息。
    const auto add = [this](const QString &key, const QString &title, bool group, int tempRemaining = -1) {
        for (const auto &item : conversationsFanLX) if (item.key == key) return;
        ConversationItemFanLX item;
        item.key = key; item.title = title; item.group = group; item.temporaryRemaining = tempRemaining;
        applyKindFanLX(item, group, tempRemaining);
        conversationsFanLX.push_back(item);
    };
    for (const auto &peer : authorizedFriendsFanLX)
        add(QString::fromStdString(chatDbFanLX::conversationKeyFanLX(chatDbFanLX::ScopeFanLX::Direct,
            accountFanLX.toStdString(), peer.toStdString())), peer, false, -1);
    for (const auto &group : authorizedGroupsFanLX) add(QStringLiteral("g|") + group, group, true, -1);
    // 本地主动打开的临时会话（来自"发起临时会话"按钮），即使尚无消息也要出现在列表里。
    for (const auto &peer : openedTemporaryFanLX) {
        const auto key = QString::fromStdString(chatDbFanLX::conversationKeyFanLX(
            chatDbFanLX::ScopeFanLX::Direct, accountFanLX.toStdString(), peer.toStdString()));
        add(key, peer, false, temporaryRemaining(peer));
    }
    emit conversationsChangedFanLX();
}

// 会话类型的视觉标志：群与好友在列表、标题栏和状态栏上都要能一眼分开。
// 放在控制器里而不是各控件里，保证三处显示永远一致。
void ChatControllerFanLX::applyKindFanLX(ConversationItemFanLX &item, bool group, int temporaryRemaining) {
    item.group = group;
    if (group) {
        item.kindText = QStringLiteral("群");
        item.kindColor = QStringLiteral("#7b1fa2");
        item.temporaryRemaining = -1;
    } else if (temporaryRemaining >= 0) {
        item.kindText = QStringLiteral("临时");
        item.kindColor = QStringLiteral("#c75c00");
        item.temporaryRemaining = temporaryRemaining;
    } else {
        item.kindText = QStringLiteral("好友");
        item.kindColor = QStringLiteral("#0b6bcb");
        item.temporaryRemaining = -1;
    }
}

void ChatControllerFanLX::applyHistoryFanLX(const chatDbFanLX::HistoryPageFanLX &page, bool prepend) {
    QList<MessageItemFanLX> converted;
    for (auto it = page.messages.rbegin(); it != page.messages.rend(); ++it) {
        const auto &row = *it;
        MessageItemFanLX item;
        item.sender = QString::fromStdString(row.sender);
        item.body = QString::fromStdString(row.body);
        item.timeText = serverTimeTextFanLX(row.acceptedAtUtc);
        item.self = row.sender == accountFanLX.toStdString();
        item.sequence = row.conversationSequence;
        item.failed = row.state == chatDbFanLX::MessageStateFanLX::Failed;
        item.pending = row.state == chatDbFanLX::MessageStateFanLX::Pending;
        item.statusText = statusOfFanLX(row.state, item.self);
        if (row.state == chatDbFanLX::MessageStateFanLX::Failed && !row.failureReason.empty())
            item.statusText += QStringLiteral("（") + QString::fromStdString(row.failureReason) + QStringLiteral("）");
        converted.push_back(item);
    }
    if (prepend)
        messagesFanLX = converted + messagesFanLX;
    else
        messagesFanLX = converted;
    historyHasMoreFanLX = page.hasMore;
    // 下一页游标：本页最早一条的会话序号。
    if (!page.messages.empty())
        historyBeforeFanLX = page.nextBeforeSequence;
    emit messagesChangedFanLX();
}

void ChatControllerFanLX::drainEventsFanLX() {
    if (!clientFanLX)
        return;
    // 单次最多处理 32 个事件：保证主线程每轮都回到事件循环，界面保持可拖动。
    for (int budget = 0; budget < 32; ++budget) {
        auto event = clientFanLX->pollClientEvent();
        if (!event)
            break;
        using KindFanLX = ImClientEventFanLX::KindFanLX;
        switch (event->kind) {
        case KindFanLX::Sent:
            emit busyChangedFanLX(false);
            if (currentKeyFanLX == event->conversationKey || event->conversationKey.empty())
                requestLatest();
            break;
        case KindFanLX::Accepted:
        case KindFanLX::SendRejected:
            if (event->kind == KindFanLX::SendRejected && !event->detail.empty())
                emit errorFanLX(QString::fromStdString(event->detail));
            requestConversations();
            if (followingLatestFanLX) requestLatest();
            break;
        case KindFanLX::Delivered:
            if (!event->conversationKey.empty() && event->conversationKey == currentKeyFanLX.toStdString()) {
                // 只看相关会话：查看旧记录时不强制跳到底部，改为提示"有新消息"。
                hasNewMessageFanLX = true;
                emit messageArrivedFanLX(event->conversationKey.c_str(), event->sequence);
                if (followingLatestFanLX) requestLatest();
            }
            requestConversations();
            break;
        case KindFanLX::History:
            if (searchQueryIdFanLX && event->queryId == searchQueryIdFanLX) {
                searchPageFanLX = event->history;
                emit searchLoadedFanLX();
                break;
            }
            // 过期查询结果直接丢弃（切换会话/账号后旧结果不得覆盖界面）。
            if (event->queryId != currentQueryIdFanLX)
                break;
            applyHistoryFanLX(event->history, queryPrependFanLX);
            emit historyLoadedFanLX(event->queryId, event->history.hasMore);
            break;
        case KindFanLX::Conversations:
            applyConversationsFanLX(event->conversations);
            break;
        case KindFanLX::Directory: {
            QList<QString> friends, groups;
            QList<QPair<QString, int>> temporary;
            int limit = 10;
            if (parseDirectoryFanLX(event->detail, friends, groups, temporary, limit)) {
                authorizedFriendsFanLX = friends;
                authorizedGroupsFanLX = groups;
                temporaryPeersFanLX = temporary;
                temporaryLimitFanLX = limit;
                requestConversations();
                emit directoryFanLX(friends, groups);
            } else
                emit errorFanLX(QStringLiteral("授权目录格式无法识别"));
            break;
        }
        case KindFanLX::Presence:
            break;
        case KindFanLX::SyncEnd:
            break;
        case KindFanLX::Connection:
            // 连接层错误（连接失败、握手超时、被服务器断开）必须让用户看到，
            // 否则界面会静默停在"重连中"，用户不知道原因。
            if (!event->detail.empty())
                emit errorFanLX(QString::fromStdString(event->detail));
            break;
        case KindFanLX::FriendNotice:
            if (!event->detail.empty())
                emit friendNoticeFanLX(QString::fromStdString(event->detail));
            requestDirectory();  // 刷新好友与临时会话列表
            break;
        case KindFanLX::Notice:
            // 不是错误（登录是成功的），但用户必须知情，例如旧聊天库已被归档重建。
            if (!event->detail.empty())
                emit noticeFanLX(QString::fromStdString(event->detail));
            break;
        case KindFanLX::Error:
            if (!event->detail.empty())
                emit errorFanLX(QString::fromStdString(event->detail));
            break;
        }
    }
    // 连接状态单独轮询：在线/离线变化要立刻反映在标题栏。
    const auto next = online() ? ConnectionStateFanLX::Online
                               : (startedFanLX ? ConnectionStateFanLX::Reconnecting
                                               : ConnectionStateFanLX::Disconnected);
    if (next != stateFanLX) {
        stateFanLX = next;
        emit stateChangedFanLX(stateFanLX, online() ? QStringLiteral("在线")
                                                    : QStringLiteral("重连中/离线"));
    }
    if (clientFanLX->droppedEventCount() > 0) {
        // 事件丢失只影响提示；历史事实源是数据库，可重新查询补齐。
        requestConversations();
    }
}

} // namespace fanlxGui

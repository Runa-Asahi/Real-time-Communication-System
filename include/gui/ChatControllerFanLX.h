#pragma once
#include "db/ChatRepositoryFanLX.h"
#include "network/ImClientFanLX.h"
#include <QObject>
#include <QPair>
#include <QString>
#include <QTimer>
#include <cstdint>
#include <memory>
#include <vector>

namespace fanlxGui {

// 会话列表项：由数据库会话摘要转换而来，只含界面需要的字段。
struct ConversationItemFanLX {
    QString key;
    QString title;       // 对端账号或群号
    QString subtitle;    // 最后一条消息正文（截断）
    QString timeText;    // 最后一条消息时间（未知时为空，界面显示"原始时间未知"）
    qint64 unread = 0;
    bool group = false;
    // 群 / 好友的区别标志（课程要求：聊天界面上两者必须一眼可辨）。
    // kindText 直接显示在会话名前后，kindColor 用于列表与标题栏着色。
    QString kindText;    // "群" / "好友" / "临时"
    QString kindColor;   // "#7b1fa2"（群）/ "#0b6bcb"（好友）/ "#c75c00"（临时会话）
    // 临时会话（对方还不是好友）：本人方向还能发几条；好友与群为 -1。
    int temporaryRemaining = -1;
};

// 消息列表项：状态直接决定界面文案（待发送/已发送/已接收/失败）。
struct MessageItemFanLX {
    QString sender;
    QString body;
    QString timeText;
    QString statusText;
    bool self = false;
    bool failed = false;
    bool pending = false;
    qint64 sequence = 0;
};

enum class ConnectionStateFanLX { Disconnected, Connecting, Online, Reconnecting };

// 界面控制器：唯一持有 ImClientFanLX 的对象，负责把异步事件转换成 Qt 信号。
//
// 线程约定：
// - Qt 主线程只调用本类方法；网络与数据库工作都在 ImClientFanLX 内部线程；
// - 事件通过 QTimer 周期轮询（30 ms）投递到主线程，避免跨线程直接触碰控件；
// - 队列满或库不可用时立即返回 false，由界面提示繁忙并保留输入文本。
class ChatControllerFanLX : public QObject {
    Q_OBJECT

  public:
    explicit ChatControllerFanLX(QObject *parent = nullptr);
    ~ChatControllerFanLX() override;

    // 参数与 ImClientFanLX 一致；useChatDatabase=false 时退化为旧 JSON 存储。
    // password 是用户在领域账号库里注册的登录密码（明文），与领域登录同一套凭据。
    // service 是本连接的聊天通道（"QQ" / "Wechat"）：聊天跟随"当前服务"，
    // 切到 QQ 就只收发 QQ 的好友与群，切到微信则只收发微信的。
    // dataRoot 是"数据根"，不是某个账号的目录：真实存储目录由
    // imClientPathFanLX::clientDirectoryFanLX(根, 服务器, 端口, 登录账号) 派生，
    // 因此同一个根目录可以安全地承载多个账号（界面不再让用户选到具体账号目录）。
    void configure(const QString &host, quint16 port, const QString &account, const QString &password,
                   const QString &dataRoot, bool useChatDatabase, const QString &service = QStringLiteral("QQ"));
    // 启动连接与目录请求。返回 false 表示配置缺失或库不可用（detail 给出原因）。
    bool start(QString *detail = nullptr);
    // 切换聊天身份（跟随领域层的"当前服务"）：停掉旧连接，按新服务的账号重新建连。
    // service 为空或不是聊天服务（微X / 微博）时只断开，并给出原因。
    bool switchIdentity(const QString &service, const QString &accountId, QString *detail = nullptr);
    // 等待服务器对本连接的认证结果（登录失败要留在登录界面，不能先进主界面再报错）。
    // 返回 true = 已登录成功；false = 被拒绝或超时，reason 给出可直接显示的原因。
    // 等待期间会继续跑 Qt 事件循环，界面不假死。
    bool waitForLogin(int timeoutMs, QString *reason = nullptr);
    void stop();

    bool online() const;
    QString storageBackend() const;
    // 最近一次 sendText 被拒的**具体**原因（空串表示没有特别原因，例如队列繁忙）。
    // 界面用它给出可操作的提示，避免被"繁忙或存储不可用"这类通用文案覆盖掉真实原因。
    QString lastRejection() const { return lastRejectionFanLX; }
    // 登录后已派生的实际存储目录；未启动时返回按当前参数推算的预览值。
    QString dataDirectory() const;
    QString lastError() const;
    ConnectionStateFanLX state() const { return stateFanLX; }
    QString account() const { return accountFanLX; }
    // 本连接的聊天通道服务（存储名："QQ" / "Wechat" / "Weibo"）。
    QString service() const { return serviceFanLX; }
    // 同一个服务的界面显示名（"QQ" / "微信" / "微博"），供窗口标题与状态栏使用。
    QString serviceDisplayName() const;
    QString sessionId() const { return sessionIdFanLX; }

    // ---- 命令 ----
    // 发送：先落库再入网；返回 false 表示繁忙/失败（界面必须保留输入文本）。
    bool sendText(const QString &conversationKey, qint64 sequence, const QString &text);
    void openConversation(const QString &conversationKey);
    // 与非好友发起临时会话：生成单聊会话键并打开（列表里以 [临时] 标出）。
    // 真正的条数限额由服务端执行，这里只负责让会话"可点可发"。返回会话键。
    QString openTemporaryConversation(const QString &peer);
    // 临时会话对端 -> 本人方向剩余条数（来自服务端目录）。
    int temporaryRemaining(const QString &peer) const;
    // 历史查询：queryId 变化后旧结果自动丢弃。
    bool requestHistory(const QString &conversationKey, const QString &sender, const QString &keyword,
                        int limit, qint64 beforeSequence);
    bool requestEarlier();   // 加载更早一页
    bool requestLatest();    // 返回最新
    bool requestConversations();
    bool requestDirectory();
    void markRead(const QString &conversationKey, qint64 sequence);
    void reconnect();
    bool searchHistory(const QString &key, const QString &sender, const QString &keyword,
                       int limit, const QString &fromUtc = {}, const QString &toUtc = {});
    bool searchEarlier();
    void cancelSearch();
    const chatDbFanLX::HistoryPageFanLX &searchPage() const { return searchPageFanLX; }

    const QList<ConversationItemFanLX> &conversations() const { return conversationsFanLX; }
    // 当前会话是群还是好友（界面标题栏与状态栏都用它，与列表里的标志同一来源）。
    bool currentConversationIsGroup() const;
    QString currentConversationKindText() const;
    const QList<MessageItemFanLX> &messages() const { return messagesFanLX; }
    bool historyHasMore() const { return historyHasMoreFanLX; }
    bool followingLatest() const { return followingLatestFanLX; }
    qint64 currentQueryId() const { return currentQueryIdFanLX; }
    QString currentConversation() const { return currentKeyFanLX; }
    // 未读总数（由会话摘要累加）。
    qint64 totalUnread() const;

  signals:
    // 状态与错误
    void stateChangedFanLX(ConnectionStateFanLX state, const QString &detail);
    void errorFanLX(const QString &detail);
    // 非错误提示：操作仍然成功，但有需要用户知情的变化（例如旧聊天库已被归档重建）。
    void noticeFanLX(const QString &detail);
    void busyChangedFanLX(bool busy);
    // 会话与消息
    void conversationsChangedFanLX();
    void messagesChangedFanLX();
    // 实时到达：只提示，不强制跳到底部；界面据此显示"有新消息"。
    void messageArrivedFanLX(const QString &conversationKey, qint64 sequence);
    void historyLoadedFanLX(qint64 queryId, bool hasMore);
    void directoryFanLX(const QList<QString> &friends, const QList<QString> &groups);
    void searchLoadedFanLX();
    // 好友申请相关的服务端推送（收到申请 / 被同意 / 被拒绝 / 被删除）。
    void friendNoticeFanLX(const QString &detail);

  private slots:
    void drainEventsFanLX();

  private:
    void applyHistoryFanLX(const chatDbFanLX::HistoryPageFanLX &page, bool prepend);
    void applyConversationsFanLX(const std::vector<chatDbFanLX::ConversationRowFanLX> &rows);
    // 给会话项补上"群 / 好友"的显示标志（文案 + 颜色）。
    // temporaryRemaining >= 0 表示这是与非好友的临时会话。
    static void applyKindFanLX(ConversationItemFanLX &item, bool group, int temporaryRemaining = -1);
    // 单聊会话：对端是好友返回 -1，否则返回临时会话剩余条数（不在目录里视为满额）。
    int directKindFanLX(const QString &peer) const;
    static QString statusOfFanLX(chatDbFanLX::MessageStateFanLX state, bool self);

    std::unique_ptr<ImClientFanLX> clientFanLX;
    QTimer *timerFanLX = nullptr;
    QString hostFanLX, accountFanLX, passwordFanLX, dataRootFanLX, sessionIdFanLX;
    QString serviceFanLX = QStringLiteral("QQ");
    QString lastRejectionFanLX;
    quint16 portFanLX = 0;
    bool useChatDatabaseFanLX = false;
    bool startedFanLX = false;
    ConnectionStateFanLX stateFanLX = ConnectionStateFanLX::Disconnected;
    QString lastErrorFanLX;
    QList<ConversationItemFanLX> conversationsFanLX;
    QList<MessageItemFanLX> messagesFanLX;
    QString currentKeyFanLX;
    qint64 currentQueryIdFanLX = 0;
    qint64 historyBeforeFanLX = 0;
    bool historyHasMoreFanLX = false;
    int historyLimitFanLX = chatDbFanLX::defaultPageSizeFanLX;
    QString historySenderFanLX;
    QString historyKeywordFanLX;
    bool hasNewMessageFanLX = false;
    bool followingLatestFanLX = true;
    bool queryPrependFanLX = false;
    qint64 searchQueryIdFanLX = 0;
    chatDbFanLX::HistoryQueryFanLX searchQueryFanLX;
    chatDbFanLX::HistoryPageFanLX searchPageFanLX;
    QList<QString> authorizedFriendsFanLX, authorizedGroupsFanLX;
    // 临时会话（对端, 本人方向剩余条数）：服务端目录给出，好友不在其中。
    QList<QPair<QString, int>> temporaryPeersFanLX;
    // 本地主动打开、尚未产生消息的临时会话对端（让会话出现在列表里）。
    QList<QString> openedTemporaryFanLX;
    // 服务端目录里的单次限额（每方向条数），未知时为 10。
    int temporaryLimitFanLX = 10;
};

} // namespace fanlxGui

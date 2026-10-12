#pragma once
#include "gui/ChatControllerFanLX.h"
#include "gui/ConversationModelFanLX.h"
#include <QMainWindow>
#include <QString>

class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QSplitter;
class QTabWidget;
class QTextBrowser;
class QTimer;

namespace fanlxGui {

class AccountPageFanLX;
class DomainControllerFanLX;
class FriendPageFanLX;
class GroupPageFanLX;
class HistoryDialogFanLX;

// 主窗口：把课设要求的全部功能都放进一个窗口的四个页签里，避免"有些功能没显示出来"。
//
//   页签 1 聊天      左联系人/群/最近会话（[好友] 蓝 / [群] 紫）+ 未读数，中间消息列表，底部编辑器
//   页签 2 账号与服务 用户基本信息、开通管理（微X 之下开通 QQ/微信/微博）、登录管理、绑定、资料维护
//   页签 3 好友       好友增删改查、同服务共同好友、跨服务推荐与共同好友
//   页签 4 群         群列表、群成员、群特色功能与动态切换、申请/推荐/踢人/子群/解散
//
// 聊天跟随"当前服务"：领域层切换当前服务后，本窗口会把聊天连接换成该服务的身份
// （QQ 连接只看到 QQ 的好友与群，微信连接只看到微信的），微X / 微博没有聊天通道时会明确提示。
//
// 只负责交互与展示：聊天走网络与数据库线程，领域读写走候选事务，界面不做阻塞操作。
class MainWindowFanLX : public QMainWindow {
    Q_OBJECT

  public:
    // controller 负责聊天（必须非空）；domain 负责账号/好友/群（为空时只显示聊天页签，
    // 便于只关心消息链路的测试与截图工具复用本窗口）。
    MainWindowFanLX(ChatControllerFanLX *controller, DomainControllerFanLX *domain, const QString &account,
                    const QString &storageBackend, QWidget *parent = nullptr);

    // 切换可见页签：0 聊天 / 1 账号与服务 / 2 好友 / 3 群。供演示与截图工具使用。
    void selectPageFanLX(int index);
    // 顶部提示条：用于"不是错误、但用户必须知道下一步做什么"的情况，
    // 例如新注册的微X号还没有任何下级服务（聊天页未连接，需要去「开通管理」开通）。
    void showNoticeFanLX(const QString &text);

  protected:
    // 关窗即收尾提交并释放领域目录锁（对应课设要求"析构时写回文件中"）。
    void closeEvent(QCloseEvent *event) override;

  private slots:
    void onConversationsChangedFanLX();
    void onMessagesChangedFanLX();
    void onConversationSelectedFanLX(const QModelIndex &index);
    void onSendFanLX();
    void onSearchFanLX();
    void onStateChangedFanLX(ConnectionStateFanLX state, const QString &detail);
    void onErrorFanLX(const QString &detail);
    void onMessageArrivedFanLX(const QString &conversationKey, qint64 sequence);
    void onHistoryLoadedFanLX(qint64 queryId, bool hasMore);
    void onDirectoryFanLX(const QList<QString> &friends, const QList<QString> &groups);
    void onEarlierFanLX();
    void onLatestFanLX();
    void onReconnectFanLX();
    void refreshHeaderFanLX();
    // "当前服务"变化：把聊天连接换成该服务的身份（微X / 微博没有聊天通道时断开并提示）。
    void onIdentityChangedFanLX();
    // 编辑器内容变化：按 UTF-8 字节数裁剪到协议上限，并刷新"已用字节"提示。
    void onEditorChangedFanLX(const QString &text);
    // 领域操作结果：成功/失败都写到底部状态栏，失败原因来自领域层原话。
    void onDomainResultFanLX(bool ok, const QString &message);

  private:
    void buildChatPageFanLX();
    void buildDomainPagesFanLX();
    void buildMenusFanLX();
    void renderMessagesFanLX();
    void showHistoryResultsFanLX();
    void showAboutFanLX();

    ChatControllerFanLX *controllerFanLX = nullptr;
    DomainControllerFanLX *domainFanLX = nullptr;
    QString accountFanLX;
    QString storageBackendFanLX;
    ConversationListModelFanLX *conversationsFanLX = nullptr;
    MessageListModelFanLX *messagesFanLX = nullptr;
    QTabWidget *tabsFanLX = nullptr;
    QListView *conversationViewFanLX = nullptr;
    QTextBrowser *messageViewFanLX = nullptr;
    QLineEdit *editorFanLX = nullptr;
    QLabel *editorHintFanLX = nullptr;
    QPushButton *sendButtonFanLX = nullptr;
    QPushButton *earlierButtonFanLX = nullptr;
    QPushButton *latestButtonFanLX = nullptr;
    QPushButton *reconnectButtonFanLX = nullptr;
    QLabel *headerFanLX = nullptr;
    QLabel *statusFanLX = nullptr;
    QLabel *newMessageHintFanLX = nullptr;
    // 聊天页顶部的"当前聊天服务 + 好友/群标志说明"（群紫、好友蓝）。
    QLabel *serviceLabelFanLX = nullptr;
    HistoryDialogFanLX *historyDialogFanLX = nullptr;
    AccountPageFanLX *accountPageFanLX = nullptr;
    FriendPageFanLX *friendPageFanLX = nullptr;
    GroupPageFanLX *groupPageFanLX = nullptr;
    QList<QString> friendsFanLX;
    QList<QString> groupsFanLX;
    bool hasPendingNewMessageFanLX = false;
};

} // namespace fanlxGui

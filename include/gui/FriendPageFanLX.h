#pragma once
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLabel;
class QFrame;
class QLineEdit;
class QListWidget;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QStackedWidget;

namespace fanlxGui {

class DomainControllerFanLX;
class FriendAvatarFanLX;
class FriendEmptyStateFanLX;
class FriendPageHeaderFanLX;
class FriendRowDelegateFanLX;

// FriendPageFanLX：好友页面。对应课设要求 2：
//   （1）好友信息的添加、修改、删除、查询；
//   （2）微X之间各自共同好友（含"用另一个平台推荐的好友来添加"）；
//   （3）添加好友是"申请 → 对方同意"，并可依据本人另一个服务的好友发起申请。
//
// 首页只有好友列表与按钮；添加 / 好友申请 / 备注标签 / 共同好友查询各是一个子页面，
// 删除直接作用于列表里选中的好友。
class FriendPageFanLX : public QWidget {
    Q_OBJECT

  public:
    explicit FriendPageFanLX(DomainControllerFanLX *controller, QWidget *parent = nullptr);
    // 截图和页内跳转用。0 首页，1 添加，2 备注，3 共同好友，4 申请。
    void showFriendSectionFanLX(int index);

  public slots:
    void refreshFanLX();
    // 从服务端重新读取好友申请与临时会话（打开申请页、收到好友动态时调用）。
    void refreshRequestsFanLX();

  signals:
    // temporary 为真时打开临时会话，否则按好友单聊打开。主窗口负责切到聊天页。
    void chatWithPeerFanLX(const QString &peer, bool temporary);
    // 收到的待处理申请数，供左侧「好友」导航画角标。0 表示没有。
    void incomingRequestsChangedFanLX(int count);

  protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    QWidget *buildHomeFanLX();
    QWidget *buildAddBodyFanLX();
    QWidget *buildRequestsBodyFanLX();
    QWidget *buildRemarkBodyFanLX();
    QWidget *buildQueryBodyFanLX();
    // 列表里选中的好友号码；没有选中（或只是占位提示行）返回空串。
    QString selectedPeerFanLX() const;
    void openRemarkPageFanLX();
    void refreshCandidatesFanLX();
    void applyFriendFilterFanLX();
    void updateFriendDetailFanLX();
    void confirmRemoveFriendFanLX(const QString &peer);
    void openCommonQueryFanLX(const QString &peer);
    void showQueryResultFanLX(const QStringList &rows, const QString &note, const QString &title, bool success);
    void updateRequestActionsFanLX();
    void setRequestSegmentFanLX(int index);
    void syncFriendActionsFanLX();
    void rebuildFriendListFanLX();
    void updateDetailPlacementFanLX();
    void activateFriendRowFanLX();
    QString selectedFriendRowFanLX() const;
    FriendPageHeaderFanLX *installFriendHeaderFanLX(int index, const QString &title);

    DomainControllerFanLX *controllerFanLX = nullptr;
    QStackedWidget *stackFanLX = nullptr;

    QListWidget *friendListFanLX = nullptr;
    QLabel *listHintFanLX = nullptr;
    QPushButton *removeButtonFanLX = nullptr;
    QPushButton *remarkNavButtonFanLX = nullptr;
    QPushButton *requestsNavButtonFanLX = nullptr;

    QLineEdit *peerEditFanLX = nullptr;
    QLineEdit *noteEditFanLX = nullptr;
    QPushButton *addButtonFanLX = nullptr;
    QComboBox *sourceServiceComboFanLX = nullptr;
    QListWidget *candidateListFanLX = nullptr;
    QStackedWidget *candidateStackFanLX = nullptr;
    FriendEmptyStateFanLX *candidateEmptyFanLX = nullptr;
    QPushButton *addFromServiceButtonFanLX = nullptr;
    QLabel *addHintFanLX = nullptr;

    QListWidget *incomingListFanLX = nullptr;
    QListWidget *outgoingListFanLX = nullptr;
    QListWidget *temporaryListFanLX = nullptr;
    QPushButton *acceptButtonFanLX = nullptr;
    QPushButton *rejectButtonFanLX = nullptr;
    QPushButton *cancelButtonFanLX = nullptr;
    QPushButton *requestsRefreshButtonFanLX = nullptr;
    QLabel *requestsHintFanLX = nullptr;
    QStackedWidget *requestStackFanLX = nullptr;
    QPushButton *incomingSegmentFanLX = nullptr;
    QPushButton *outgoingSegmentFanLX = nullptr;
    QPushButton *temporarySegmentFanLX = nullptr;
    QStackedWidget *incomingStackFanLX = nullptr;
    QStackedWidget *outgoingStackFanLX = nullptr;
    QStackedWidget *temporaryStackFanLX = nullptr;
    FriendEmptyStateFanLX *incomingEmptyFanLX = nullptr;
    FriendEmptyStateFanLX *outgoingEmptyFanLX = nullptr;
    FriendEmptyStateFanLX *temporaryEmptyFanLX = nullptr;
    QLabel *temporaryNoteFanLX = nullptr;

    QLineEdit *remarkPeerEditFanLX = nullptr;
    QLineEdit *remarkEditFanLX = nullptr;
    QLineEdit *tagEditFanLX = nullptr;
    QPushButton *remarkButtonFanLX = nullptr;

    QLineEdit *commonPeerEditFanLX = nullptr;
    QPushButton *commonButtonFanLX = nullptr;
    QComboBox *targetServiceComboFanLX = nullptr;
    QPushButton *recommendButtonFanLX = nullptr;
    QPushButton *crossCommonButtonFanLX = nullptr;
    QListWidget *resultListFanLX = nullptr;
    QLabel *resultHintFanLX = nullptr;
    QLabel *resultTitleFanLX = nullptr;
    QStackedWidget *resultStackFanLX = nullptr;
    FriendEmptyStateFanLX *resultEmptyFanLX = nullptr;

    QLineEdit *searchEditFanLX = nullptr;
    QLabel *homeTitleFanLX = nullptr;
    QLabel *homeCountFanLX = nullptr;
    QFrame *detailPanelFanLX = nullptr;
    QWidget *narrowActionsFanLX = nullptr;
    QPushButton *chatButtonFanLX = nullptr;
    QPushButton *narrowChatButtonFanLX = nullptr;
    QPushButton *narrowRemarkButtonFanLX = nullptr;
    QPushButton *narrowCommonButtonFanLX = nullptr;
    QPushButton *narrowRemoveButtonFanLX = nullptr;
    QStringList pendingIncomingFanLX;
    QStringList pendingTemporaryFanLX;
    QString pendingServiceFanLX;
    QStackedWidget *homeListStackFanLX = nullptr;
    FriendEmptyStateFanLX *homeEmptyFanLX = nullptr;
    QLabel *requestBadgeFanLX = nullptr;
    FriendAvatarFanLX *detailAvatarFanLX = nullptr;
    QLabel *detailRemarkFanLX = nullptr;
    QLabel *detailAccountFanLX = nullptr;
    QLabel *detailTagFanLX = nullptr;
    QPushButton *detailCommonButtonFanLX = nullptr;
    FriendRowDelegateFanLX *rowDelegateFanLX = nullptr;
    // 只在刚打开申请页时为真：用来挑默认分段。同意、拒绝、撤回、刷新都不置位。
    bool pickRequestSegmentFanLX = false;
};

} // namespace fanlxGui

#include "gui/FriendPageFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/FriendChromeFanLX.h"
#include "gui/PageKitFanLX.h"
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace fanlxGui {

class FriendAvatarFanLX : public QWidget {
  public:
    explicit FriendAvatarFanLX(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(72, 72); }

    void showPeer(const QString &account, const QString &text) {
        account_ = account;
        text_ = text;
        setVisible(!account.isEmpty());
        update();
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        if (account_.isEmpty())
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(friendAvatarColorFanLX(account_));
        painter.drawEllipse(rect().adjusted(1, 1, -1, -1));
        painter.setPen(Qt::white);
        QFont font = this->font();
        font.setPixelSize(text_.size() > 1 ? 18 : 28);
        font.setWeight(QFont::DemiBold);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter, text_);
    }

  private:
    QString account_;
    QString text_;
};

namespace {

constexpr int kAddPage = 1;
constexpr int kRemarkPage = 2;
constexpr int kQueryPage = 3;
constexpr int kRequestsPage = 4;

// 好友列表行格式为 "号码 ｜ 备注 ｜ 标签 ..."，号码是第一个分隔符之前的部分。
// 申请、临时会话、跨服务候选三种列表沿用同一格式。以"（"开头的是占位提示行，不是真正的号码。
QString peerOfRowFanLX(const QString &row) {
    if (row.isEmpty() || row.startsWith(QStringLiteral("（")))
        return {};
    const auto separator = row.indexOf(QStringLiteral(" ｜ "));
    return separator < 0 ? row : row.left(separator);
}

QString selectedPeerOfFanLX(const QListWidget *list) {
    const auto *item = list->currentItem();
    return item ? peerOfRowFanLX(item->text()) : QString();
}

// 成功文案一定同时带人数和查询种类。校验失败（例如空号码）不能当成「没有共同好友」。
bool friendQuerySucceededFanLX(const QString &message) {
    return message.contains(QStringLiteral("人")) &&
           (message.contains(QStringLiteral("共同好友")) || message.contains(QStringLiteral("上共")));
}

void polishPropertyFanLX(QWidget *widget) {
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QWidget *friendFieldFanLX(const QString &caption, QWidget *input, QWidget *parent) {
    auto *wrap = new QWidget(parent);
    auto *layout = new QVBoxLayout(wrap);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *label = new QLabel(caption, wrap);
    label->setObjectName(QStringLiteral("friendFieldFanLX"));
    layout->addWidget(label);
    layout->addWidget(input);
    return wrap;
}

void setInlineResultFanLX(QLabel *label, const QString &text, bool ok) {
    if (!label)
        return;
    label->setText(text);
    label->setProperty("tone", text.isEmpty() ? QString() : (ok ? QStringLiteral("ok") : QStringLiteral("bad")));
    polishPropertyFanLX(label);
}

// 子页内容列：最宽约 1040，窄窗口仍能缩到 0，避免把外层滚动区域撑出横向滚动条。
QWidget *friendSubpageShellFanLX(QVBoxLayout **columnLayout, int maxWidth) {
    auto *page = new QWidget();
    page->setMinimumWidth(0);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 8, 16, 16);
    outer->setSpacing(0);
    auto *center = new QHBoxLayout();
    center->setContentsMargins(0, 0, 0, 0);
    auto *column = new QWidget(page);
    column->setMinimumWidth(0);
    column->setMaximumWidth(maxWidth);
    column->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto *layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    center->addStretch(0);
    center->addWidget(column, 1, Qt::AlignTop);
    center->addStretch(0);
    outer->addLayout(center);
    if (columnLayout)
        *columnLayout = layout;
    return page;
}

} // namespace

FriendPageFanLX::FriendPageFanLX(DomainControllerFanLX *controller, QWidget *parent)
    : QWidget(parent), controllerFanLX(controller) {
    setObjectName(QStringLiteral("friendPageFanLX"));
    setAttribute(Qt::WA_StyledBackground, true);
    const QColor pageBg(QStringLiteral("#f3f5f8"));
    setAutoFillBackground(true);
    auto pagePalette = palette();
    pagePalette.setColor(QPalette::Window, pageBg);
    setPalette(pagePalette);

    stackFanLX = new QStackedWidget(this);
    kit::setHomePage(stackFanLX, buildHomeFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("添加好友（发送申请）"), buildAddBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("备注与标签"), buildRemarkBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("共同好友查询"), buildQueryBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("好友申请与临时会话"), buildRequestsBodyFanLX());
    installFriendHeaderFanLX(kAddPage, QStringLiteral("添加好友"));
    if (auto *header = installFriendHeaderFanLX(kRequestsPage, QStringLiteral("好友申请"))) {
        if (requestsRefreshButtonFanLX)
            header->trailingLayout()->addWidget(requestsRefreshButtonFanLX);
    }

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->addWidget(stackFanLX);
    for (auto *scroll : findChildren<QScrollArea *>()) {
        scroll->setAutoFillBackground(true);
        scroll->viewport()->setAutoFillBackground(true);
        auto viewportPalette = scroll->viewport()->palette();
        viewportPalette.setColor(QPalette::Window, pageBg);
        scroll->viewport()->setPalette(viewportPalette);
    }
    rowDelegateFanLX = new FriendRowDelegateFanLX(this);
    prepareFriendRowListFanLX(friendListFanLX, QStringLiteral("friendListFanLX"));
    prepareFriendRowListFanLX(resultListFanLX, QStringLiteral("friendResultListFanLX"));
    prepareFriendRowListFanLX(candidateListFanLX, QStringLiteral("friendCandidateListFanLX"));
    prepareFriendRowListFanLX(incomingListFanLX, QStringLiteral("friendIncomingListFanLX"));
    prepareFriendRowListFanLX(outgoingListFanLX, QStringLiteral("friendOutgoingListFanLX"));
    prepareFriendRowListFanLX(temporaryListFanLX, QStringLiteral("friendTemporaryListFanLX"));
    for (auto *list : {friendListFanLX, resultListFanLX, candidateListFanLX, incomingListFanLX, outgoingListFanLX,
                       temporaryListFanLX})
        list->setItemDelegate(rowDelegateFanLX);

    // 当前平台切换与任何好友写操作成功后都要重读列表。
    connect(controllerFanLX, &DomainControllerFanLX::dataChangedFanLX, this, &FriendPageFanLX::refreshFanLX);
    connect(controllerFanLX, &DomainControllerFanLX::identityChangedFanLX, this,
            &FriendPageFanLX::refreshFanLX);
    connect(friendListFanLX, &QListWidget::currentItemChanged, this, [this] { syncFriendActionsFanLX(); });
    connect(candidateListFanLX, &QListWidget::currentItemChanged, this, [this] { syncFriendActionsFanLX(); });
    connect(incomingListFanLX, &QListWidget::currentItemChanged, this, [this] { updateRequestActionsFanLX(); });
    connect(outgoingListFanLX, &QListWidget::currentItemChanged, this, [this] { updateRequestActionsFanLX(); });
    connect(peerEditFanLX, &QLineEdit::textChanged, this, [this] { syncFriendActionsFanLX(); });
    refreshFanLX();
}

QWidget *FriendPageFanLX::buildHomeFanLX() {
    auto *home = new QWidget();
    auto *layout = new QVBoxLayout(home);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    homeTitleFanLX = new QLabel(home);
    homeTitleFanLX->setObjectName(QStringLiteral("friendHomeTitleFanLX"));
    searchEditFanLX = new QLineEdit(home);
    searchEditFanLX->setObjectName(QStringLiteral("friendSearchFanLX"));
    searchEditFanLX->setPlaceholderText(QStringLiteral("搜索号码、备注或标签"));
    searchEditFanLX->setClearButtonEnabled(true);
    searchEditFanLX->setFixedWidth(240);
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(12);
    titleRow->addWidget(homeTitleFanLX, 1);
    titleRow->addWidget(searchEditFanLX, 0, Qt::AlignVCenter);

    listHintFanLX = kit::hintLabel(home);

    auto *addNav = friendPrimaryButtonFanLX(QStringLiteral("＋ 添加好友"), home);
    requestsNavButtonFanLX = friendSecondaryButtonFanLX(QStringLiteral("好友申请"), home);
    requestBadgeFanLX = new QLabel(home);
    requestBadgeFanLX->setObjectName(QStringLiteral("friendBadgeFanLX"));
    requestBadgeFanLX->setAlignment(Qt::AlignCenter);
    requestBadgeFanLX->setMinimumWidth(18);
    requestBadgeFanLX->hide();
    auto *queryNav = friendSecondaryButtonFanLX(QStringLiteral("共同好友"), home);
    auto *requestWrap = new QWidget(home);
    auto *requestRow = new QHBoxLayout(requestWrap);
    requestRow->setContentsMargins(0, 0, 0, 0);
    requestRow->setSpacing(6);
    requestRow->addWidget(requestsNavButtonFanLX);
    requestRow->addWidget(requestBadgeFanLX);
    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);
    toolbar->addWidget(addNav);
    toolbar->addWidget(requestWrap);
    toolbar->addWidget(queryNav);
    toolbar->addStretch(1);

    friendListFanLX = new QListWidget(home);
    friendListFanLX->setMinimumHeight(280);
    friendListFanLX->setContextMenuPolicy(Qt::CustomContextMenu);
    homeEmptyFanLX = new FriendEmptyStateFanLX(QStringLiteral("还没有好友"),
                                              QStringLiteral("添加一位好友后，会显示在这里。"), home);
    auto *emptyAction = homeEmptyFanLX->setAction(QStringLiteral("添加好友"));
    homeListStackFanLX = new QStackedWidget(home);
    homeListStackFanLX->addWidget(friendListFanLX);
    homeListStackFanLX->addWidget(homeEmptyFanLX);

    auto *detail = new QFrame(home);
    detail->setObjectName(QStringLiteral("friendDetailFanLX"));
    detail->setFrameShape(QFrame::NoFrame);
    detail->setAttribute(Qt::WA_StyledBackground, true);
    detail->setFixedWidth(320);
    auto *detailLayout = new QVBoxLayout(detail);
    detailLayout->setContentsMargins(20, 24, 20, 20);
    detailLayout->setSpacing(8);
    detailAvatarFanLX = new FriendAvatarFanLX(detail);
    detailRemarkFanLX = new QLabel(detail);
    detailRemarkFanLX->setObjectName(QStringLiteral("friendDetailRemarkFanLX"));
    detailRemarkFanLX->setWordWrap(true);
    detailAccountFanLX = new QLabel(detail);
    detailAccountFanLX->setObjectName(QStringLiteral("friendDetailAccountFanLX"));
    detailTagFanLX = new QLabel(detail);
    detailTagFanLX->setObjectName(QStringLiteral("friendTagChipFanLX"));
    remarkNavButtonFanLX = friendSecondaryButtonFanLX(QStringLiteral("备注 / 标签"), detail);
    detailCommonButtonFanLX = friendSecondaryButtonFanLX(QStringLiteral("共同好友"), detail);
    removeButtonFanLX = friendDangerButtonFanLX(QStringLiteral("删除好友"), detail);
    detailLayout->addWidget(detailAvatarFanLX, 0, Qt::AlignHCenter);
    detailLayout->addWidget(detailRemarkFanLX);
    detailLayout->addWidget(detailAccountFanLX);
    detailLayout->addWidget(detailTagFanLX, 0, Qt::AlignLeft);
    detailLayout->addSpacing(8);
    detailLayout->addWidget(remarkNavButtonFanLX);
    detailLayout->addWidget(detailCommonButtonFanLX);
    detailLayout->addWidget(removeButtonFanLX);
    detailLayout->addStretch(1);

    auto *split = new QHBoxLayout();
    split->setSpacing(16);
    split->addWidget(homeListStackFanLX, 1);
    split->addWidget(detail);
    layout->addLayout(titleRow);
    layout->addWidget(listHintFanLX);
    layout->addLayout(toolbar);
    layout->addLayout(split, 1);

    const auto openAdd = [this] {
        refreshCandidatesFanLX();
        stackFanLX->setCurrentIndex(kAddPage);
    };
    connect(addNav, &QPushButton::clicked, this, openAdd);
    connect(emptyAction, &QPushButton::clicked, this, openAdd);
    connect(requestsNavButtonFanLX, &QPushButton::clicked, this, [this] {
        pickRequestSegmentFanLX = true;
        stackFanLX->setCurrentIndex(kRequestsPage);
        refreshRequestsFanLX();
    });
    // 工具栏只进入查询页，不替用户发查询。
    connect(queryNav, &QPushButton::clicked, this, [this] { stackFanLX->setCurrentIndex(kQueryPage); });
    connect(remarkNavButtonFanLX, &QPushButton::clicked, this, [this] { openRemarkPageFanLX(); });
    connect(detailCommonButtonFanLX, &QPushButton::clicked, this,
            [this] { openCommonQueryFanLX(selectedPeerFanLX()); });
    connect(friendListFanLX, &QListWidget::itemDoubleClicked, this, [this] { openRemarkPageFanLX(); });
    connect(searchEditFanLX, &QLineEdit::textChanged, this, [this] { applyFriendFilterFanLX(); });
    connect(removeButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto peer = selectedPeerFanLX();
        if (peer.isEmpty()) {
            listHintFanLX->setText(QStringLiteral("请先在列表里选中要删除的好友"));
            return;
        }
        confirmRemoveFriendFanLX(peer);
    });
    connect(friendListFanLX, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const auto viewportPos = friendListFanLX->viewport()->mapFrom(friendListFanLX, pos);
        auto *item = friendListFanLX->itemAt(viewportPos);
        if (!item)
            return;
        friendListFanLX->setCurrentItem(item);
        const auto peer = peerOfRowFanLX(item->text());
        if (peer.isEmpty())
            return;
        QMenu menu(friendDialogParentFanLX(this));
        const bool loggedIn = controllerFanLX->currentServiceLoggedIn();
        auto *remark = menu.addAction(QStringLiteral("备注/标签"));
        auto *common = menu.addAction(QStringLiteral("共同好友"));
        auto *remove = menu.addAction(QStringLiteral("删除"));
        remark->setEnabled(loggedIn);
        common->setEnabled(loggedIn);
        remove->setEnabled(loggedIn);
        const auto *chosen = menu.exec(friendListFanLX->viewport()->mapToGlobal(viewportPos));
        if (chosen == remark)
            openRemarkPageFanLX();
        else if (chosen == common)
            openCommonQueryFanLX(peer);
        else if (chosen == remove)
            confirmRemoveFriendFanLX(peer);
    });
    return home;
}

QWidget *FriendPageFanLX::buildAddBodyFanLX() {
    QVBoxLayout *layout = nullptr;
    auto *page = friendSubpageShellFanLX(&layout, 1040);

    auto *notice = new QLabel(
        QStringLiteral("好友需要对方同意才会成立；同意之前你们可以在聊天页临时会话（每方向有条数上限）。"), page);
    notice->setObjectName(QStringLiteral("friendNoticeFanLX"));
    notice->setWordWrap(true);
    notice->setAttribute(Qt::WA_StyledBackground, true);

    auto *byNumber = new FriendCardFanLX(QStringLiteral("按号码发送申请"), page);
    byNumber->setMaximumWidth(560);
    peerEditFanLX = new QLineEdit(byNumber);
    peerEditFanLX->setPlaceholderText(QStringLiteral("对方在当前服务的号码，如 2026002"));
    noteEditFanLX = new QLineEdit(byNumber);
    noteEditFanLX->setPlaceholderText(QStringLiteral("附言（可选），如：我是你的同学"));
    addButtonFanLX = friendPrimaryButtonFanLX(QStringLiteral("发送好友申请"), byNumber);
    addButtonFanLX->setEnabled(false);
    byNumber->bodyLayout()->addWidget(friendFieldFanLX(QStringLiteral("号码"), peerEditFanLX, byNumber));
    byNumber->bodyLayout()->addWidget(friendFieldFanLX(QStringLiteral("附言"), noteEditFanLX, byNumber));
    byNumber->bodyLayout()->addWidget(addButtonFanLX, 0, Qt::AlignLeft);

    auto *byService = new FriendCardFanLX(QStringLiteral("依据本人另一个服务的好友添加"), page);
    sourceServiceComboFanLX = new QComboBox(byService);
    candidateListFanLX = new QListWidget(byService);
    candidateEmptyFanLX =
        new FriendEmptyStateFanLX(QStringLiteral("（本人还没有开通其它服务）"), QString(), byService);
    candidateStackFanLX = new QStackedWidget(byService);
    candidateStackFanLX->setMinimumHeight(300);
    candidateStackFanLX->addWidget(candidateListFanLX);
    candidateStackFanLX->addWidget(candidateEmptyFanLX);
    addFromServiceButtonFanLX = friendPrimaryButtonFanLX(QStringLiteral("向选中的人发送申请"), byService);
    addFromServiceButtonFanLX->setEnabled(false);
    byService->bodyLayout()->addWidget(friendFieldFanLX(QStringLiteral("来源服务"), sourceServiceComboFanLX, byService));
    byService->bodyLayout()->addWidget(candidateStackFanLX);
    byService->bodyLayout()->addWidget(addFromServiceButtonFanLX, 0, Qt::AlignLeft);

    addHintFanLX = kit::hintLabel(page);
    addHintFanLX->setObjectName(QStringLiteral("friendInlineResultFanLX"));

    layout->addWidget(notice);
    layout->addWidget(byNumber, 0, Qt::AlignLeft);
    layout->addWidget(byService);
    layout->addWidget(addHintFanLX);

    connect(addButtonFanLX, &QPushButton::clicked, this, [this] {
        if (!addButtonFanLX->isEnabled())
            return;
        const bool ok = controllerFanLX->sendFriendRequest(peerEditFanLX->text().trimmed(), noteEditFanLX->text());
        if (ok) {
            peerEditFanLX->clear();
            noteEditFanLX->clear();
        }
        setInlineResultFanLX(addHintFanLX, controllerFanLX->lastMessage(), ok);
    });
    connect(peerEditFanLX, &QLineEdit::returnPressed, this, [this] {
        if (addButtonFanLX->isEnabled())
            addButtonFanLX->click();
    });
    connect(sourceServiceComboFanLX, &QComboBox::currentTextChanged, this, [this] {
        refreshCandidatesFanLX();
        syncFriendActionsFanLX();
    });
    connect(addFromServiceButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto peer = selectedPeerOfFanLX(candidateListFanLX);
        if (peer.isEmpty()) {
            setInlineResultFanLX(addHintFanLX, QStringLiteral("请先在列表里选中来源服务里的一位好友"), false);
            return;
        }
        const bool ok = controllerFanLX->addFriendFromService(sourceServiceComboFanLX->currentText(), peer,
                                                              noteEditFanLX->text());
        setInlineResultFanLX(addHintFanLX, controllerFanLX->lastMessage(), ok);
        refreshCandidatesFanLX();
        syncFriendActionsFanLX();
    });
    return page;
}

QWidget *FriendPageFanLX::buildRequestsBodyFanLX() {
    QVBoxLayout *layout = nullptr;
    auto *page = friendSubpageShellFanLX(&layout, 1040);

    incomingSegmentFanLX = friendSegmentButtonFanLX(QStringLiteral("收到的申请"), page);
    outgoingSegmentFanLX = friendSegmentButtonFanLX(QStringLiteral("我发出的"), page);
    temporarySegmentFanLX = friendSegmentButtonFanLX(QStringLiteral("临时会话"), page);
    auto *segments = new QHBoxLayout();
    segments->setSpacing(4);
    segments->addWidget(incomingSegmentFanLX);
    segments->addWidget(outgoingSegmentFanLX);
    segments->addWidget(temporarySegmentFanLX);
    segments->addStretch(1);

    requestsHintFanLX = kit::hintLabel(page);
    requestsRefreshButtonFanLX = friendSecondaryButtonFanLX(QStringLiteral("刷新"), page);

    const auto makePane = [page](QListWidget **listOut, FriendEmptyStateFanLX **emptyOut, QStackedWidget **stackOut,
                                 const QString &emptyText) {
        auto *pane = new QWidget(page);
        auto *paneLayout = new QVBoxLayout(pane);
        paneLayout->setContentsMargins(0, 0, 0, 0);
        paneLayout->setSpacing(8);
        auto *list = new QListWidget(pane);
        auto *empty = new FriendEmptyStateFanLX(emptyText, QString(), pane);
        auto *stack = new QStackedWidget(pane);
        stack->setMinimumHeight(280);
        stack->addWidget(list);
        stack->addWidget(empty);
        paneLayout->addWidget(stack, 1);
        *listOut = list;
        *emptyOut = empty;
        *stackOut = stack;
        return pane;
    };
    auto *incomingPane = makePane(&incomingListFanLX, &incomingEmptyFanLX, &incomingStackFanLX,
                                  QStringLiteral("（没有待处理的申请）"));
    auto *outgoingPane = makePane(&outgoingListFanLX, &outgoingEmptyFanLX, &outgoingStackFanLX,
                                  QStringLiteral("（没有等待中的申请）"));
    auto *temporaryPane = makePane(&temporaryListFanLX, &temporaryEmptyFanLX, &temporaryStackFanLX,
                                   QStringLiteral("（没有临时会话）"));
    acceptButtonFanLX = friendPrimaryButtonFanLX(QStringLiteral("同意"), incomingPane);
    rejectButtonFanLX = friendDangerButtonFanLX(QStringLiteral("拒绝"), incomingPane);
    cancelButtonFanLX = friendDangerButtonFanLX(QStringLiteral("撤回"), outgoingPane);
    auto *incomingButtons = new QHBoxLayout();
    incomingButtons->addStretch(1);
    incomingButtons->addWidget(acceptButtonFanLX);
    incomingButtons->addWidget(rejectButtonFanLX);
    qobject_cast<QVBoxLayout *>(incomingPane->layout())->addLayout(incomingButtons);
    qobject_cast<QVBoxLayout *>(outgoingPane->layout())->addWidget(cancelButtonFanLX, 0, Qt::AlignRight);
    temporaryNoteFanLX = new QLabel(temporaryPane);
    temporaryNoteFanLX->setObjectName(QStringLiteral("friendTemporaryNoteFanLX"));
    qobject_cast<QVBoxLayout *>(temporaryPane->layout())->insertWidget(0, temporaryNoteFanLX);

    requestStackFanLX = new QStackedWidget(page);
    requestStackFanLX->addWidget(incomingPane);
    requestStackFanLX->addWidget(outgoingPane);
    requestStackFanLX->addWidget(temporaryPane);
    layout->addLayout(segments);
    layout->addWidget(requestsHintFanLX);
    layout->addWidget(requestStackFanLX);
    setRequestSegmentFanLX(0);

    const auto act = [this](QListWidget *list, bool (DomainControllerFanLX::*op)(const QString &)) {
        const auto peer = selectedPeerOfFanLX(list);
        if (peer.isEmpty()) {
            requestsHintFanLX->setText(QStringLiteral("请先在列表里选中一条申请"));
            return;
        }
        (controllerFanLX->*op)(peer);
        const auto message = controllerFanLX->lastMessage();
        refreshRequestsFanLX();
        requestsHintFanLX->setText(message);
    };
    connect(acceptButtonFanLX, &QPushButton::clicked, this,
            [this, act] { act(incomingListFanLX, &DomainControllerFanLX::acceptFriendRequest); });
    connect(rejectButtonFanLX, &QPushButton::clicked, this,
            [this, act] { act(incomingListFanLX, &DomainControllerFanLX::rejectFriendRequest); });
    connect(cancelButtonFanLX, &QPushButton::clicked, this,
            [this, act] { act(outgoingListFanLX, &DomainControllerFanLX::cancelFriendRequest); });
    connect(requestsRefreshButtonFanLX, &QPushButton::clicked, this, &FriendPageFanLX::refreshRequestsFanLX);
    connect(incomingSegmentFanLX, &QPushButton::clicked, this, [this] { setRequestSegmentFanLX(0); });
    connect(outgoingSegmentFanLX, &QPushButton::clicked, this, [this] { setRequestSegmentFanLX(1); });
    connect(temporarySegmentFanLX, &QPushButton::clicked, this, [this] { setRequestSegmentFanLX(2); });
    return page;
}

void FriendPageFanLX::refreshRequestsFanLX() {
    if (!controllerFanLX->opened())
        return;
    const auto view = controllerFanLX->friendRequestView();
    if (rowDelegateFanLX)
        rowDelegateFanLX->setDirectionLimit(view.reachable ? view.limit : 0);
    const auto showBadge = [this](int count) {
        if (!requestBadgeFanLX)
            return;
        if (count <= 0) {
            requestBadgeFanLX->hide();
            return;
        }
        requestBadgeFanLX->setText(QString::number(count));
        requestBadgeFanLX->show();
    };
    requestsNavButtonFanLX->setText(QStringLiteral("好友申请"));
    const auto fillPane = [&](QListWidget *list, FriendEmptyStateFanLX *empty, QStackedWidget *stack,
                              const QStringList &rows, const QString &emptyText) {
        if (!list || !empty || !stack)
            return;
        list->blockSignals(true);
        list->clear();
        if (!view.reachable) {
            empty->setError(true);
            empty->setText(QStringLiteral("（服务端不可达）"), controllerFanLX->lastMessage());
            stack->setCurrentWidget(empty);
        } else if (rows.isEmpty()) {
            empty->setError(false);
            empty->setText(emptyText, QString());
            stack->setCurrentWidget(empty);
        } else {
            list->addItems(rows);
            stack->setCurrentWidget(list);
        }
        list->blockSignals(false);
        list->viewport()->update();
    };
    const auto segmentText = [&](const QString &name, int count) {
        return view.reachable ? QStringLiteral("%1（%2）").arg(name).arg(count)
                              : QStringLiteral("%1（–）").arg(name);
    };
    fillPane(incomingListFanLX, incomingEmptyFanLX, incomingStackFanLX, view.incoming,
             QStringLiteral("（没有待处理的申请）"));
    fillPane(outgoingListFanLX, outgoingEmptyFanLX, outgoingStackFanLX, view.outgoing,
             QStringLiteral("（没有等待中的申请）"));
    fillPane(temporaryListFanLX, temporaryEmptyFanLX, temporaryStackFanLX, view.temporary,
             QStringLiteral("（没有临时会话）"));
    if (incomingSegmentFanLX)
        incomingSegmentFanLX->setText(segmentText(QStringLiteral("收到的申请"), view.incoming.size()));
    if (outgoingSegmentFanLX)
        outgoingSegmentFanLX->setText(segmentText(QStringLiteral("我发出的"), view.outgoing.size()));
    if (temporarySegmentFanLX)
        temporarySegmentFanLX->setText(segmentText(QStringLiteral("临时会话"), view.temporary.size()));
    if (temporaryNoteFanLX) {
        temporaryNoteFanLX->setVisible(view.reachable);
        if (view.reachable)
            temporaryNoteFanLX->setText(QStringLiteral("每方向最多 %1 条").arg(view.limit));
    }
    if (view.reachable) {
        requestsHintFanLX->setText(QStringLiteral("%1 · 收到 %2 条、发出 %3 条待处理申请；临时会话每方向最多 %4 条")
                                       .arg(controllerFanLX->currentService())
                                       .arg(view.incoming.size())
                                       .arg(view.outgoing.size())
                                       .arg(view.limit));
    } else {
        requestsHintFanLX->setText(controllerFanLX->lastMessage());
    }
    showBadge(view.reachable ? view.incoming.size() : 0);
    int segment = requestStackFanLX ? requestStackFanLX->currentIndex() : 0;
    if (pickRequestSegmentFanLX) {
        pickRequestSegmentFanLX = false;
        segment = 0;
        if (view.reachable) {
            if (!view.incoming.isEmpty())
                segment = 0;
            else if (!view.temporary.isEmpty())
                segment = 2;
            else if (!view.outgoing.isEmpty())
                segment = 1;
        }
    }
    setRequestSegmentFanLX(segment);
    updateRequestActionsFanLX();
}

void FriendPageFanLX::setRequestSegmentFanLX(int index) {
    if (!requestStackFanLX)
        return;
    if (index < 0 || index > 2)
        index = 0;
    requestStackFanLX->setCurrentIndex(index);
    QPushButton *buttons[] = {incomingSegmentFanLX, outgoingSegmentFanLX, temporarySegmentFanLX};
    for (int i = 0; i < 3; ++i) {
        if (!buttons[i])
            continue;
        buttons[i]->setProperty("active", i == index ? QStringLiteral("true") : QStringLiteral("false"));
        polishPropertyFanLX(buttons[i]);
    }
}

void FriendPageFanLX::refreshCandidatesFanLX() {
    if (!controllerFanLX->opened() || !sourceServiceComboFanLX || !candidateListFanLX)
        return;
    const auto source = sourceServiceComboFanLX->currentText();
    candidateListFanLX->blockSignals(true);
    candidateListFanLX->clear();
    QString emptyText;
    QStringList rows;
    if (source.isEmpty()) {
        emptyText = QStringLiteral("（本人还没有开通其它服务）");
    } else {
        rows = controllerFanLX->crossServiceFriendCandidates(source);
        if (rows.isEmpty())
            emptyText = QStringLiteral("（%1 里没有可换算的好友）").arg(source);
    }
    if (!emptyText.isEmpty() && candidateEmptyFanLX && candidateStackFanLX) {
        candidateEmptyFanLX->setError(false);
        candidateEmptyFanLX->setText(emptyText, QString());
        candidateStackFanLX->setCurrentWidget(candidateEmptyFanLX);
    } else if (candidateStackFanLX) {
        candidateListFanLX->addItems(rows);
        candidateStackFanLX->setCurrentWidget(candidateListFanLX);
    } else {
        candidateListFanLX->addItems(rows);
        if (!emptyText.isEmpty())
            candidateListFanLX->addItem(emptyText);
    }
    candidateListFanLX->blockSignals(false);
}

QWidget *FriendPageFanLX::buildRemarkBodyFanLX() {
    auto *body = new QWidget();
    remarkPeerEditFanLX = new QLineEdit(body);
    remarkPeerEditFanLX->setPlaceholderText(QStringLiteral("好友号码"));
    remarkEditFanLX = new QLineEdit(body);
    remarkEditFanLX->setPlaceholderText(QStringLiteral("备注名"));
    tagEditFanLX = new QLineEdit(body);
    tagEditFanLX->setPlaceholderText(QStringLiteral("分组标签，如 同学"));
    remarkButtonFanLX = friendPrimaryButtonFanLX(QStringLiteral("保存备注/标签"), body);

    auto *form = new QGridLayout();
    form->addWidget(new QLabel(QStringLiteral("好友"), body), 0, 0);
    form->addWidget(remarkPeerEditFanLX, 0, 1);
    form->addWidget(new QLabel(QStringLiteral("备注"), body), 1, 0);
    form->addWidget(remarkEditFanLX, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("标签"), body), 2, 0);
    form->addWidget(tagEditFanLX, 2, 1);
    auto *layout = new QVBoxLayout(body);
    layout->addLayout(form);
    layout->addWidget(remarkButtonFanLX, 0, Qt::AlignRight);
    layout->addStretch(1);

    connect(remarkButtonFanLX, &QPushButton::clicked, this, [this] {
        controllerFanLX->updateFriendRemark(remarkPeerEditFanLX->text().trimmed(),
                                            remarkEditFanLX->text().trimmed(), tagEditFanLX->text().trimmed());
    });
    return body;
}

QWidget *FriendPageFanLX::buildQueryBodyFanLX() {
    auto *body = new QWidget();
    commonPeerEditFanLX = new QLineEdit(body);
    commonPeerEditFanLX->setPlaceholderText(QStringLiteral("同一服务内对方的号码"));
    commonButtonFanLX = friendPrimaryButtonFanLX(QStringLiteral("查询同服务共同好友"), body);
    targetServiceComboFanLX = new QComboBox(body);
    recommendButtonFanLX = friendSecondaryButtonFanLX(QStringLiteral("推荐好友"), body);
    crossCommonButtonFanLX = friendSecondaryButtonFanLX(QStringLiteral("跨服务共同好友"), body);
    recommendButtonFanLX->setToolTip(QStringLiteral("把另一个服务里的好友推荐到当前服务（查询后需手动添加）"));
    resultListFanLX = new QListWidget(body);
    resultListFanLX->setMinimumHeight(140);
    resultHintFanLX = kit::hintLabel(body);

    auto *first = new QHBoxLayout();
    first->addWidget(commonPeerEditFanLX, 1);
    first->addWidget(commonButtonFanLX);
    auto *second = new QHBoxLayout();
    second->addWidget(new QLabel(QStringLiteral("另一个服务"), body));
    second->addWidget(targetServiceComboFanLX);
    second->addWidget(recommendButtonFanLX);
    second->addWidget(crossCommonButtonFanLX);
    second->addStretch(1);

    auto *layout = new QVBoxLayout(body);
    layout->addLayout(first);
    layout->addLayout(second);
    layout->addWidget(resultListFanLX, 1);
    layout->addWidget(resultHintFanLX);

    // 查询结果只是"候选"：领域不会自动建立好友关系，必须切到目标服务显式添加。
    const auto showResult = [this](const QStringList &rows, const QString &note) {
        resultListFanLX->clear();
        resultListFanLX->addItems(rows);
        if (rows.isEmpty())
            resultListFanLX->addItem(QStringLiteral("（没有匹配结果）"));
        resultHintFanLX->setText(note);
    };
    connect(commonButtonFanLX, &QPushButton::clicked, this, [this, showResult] {
        const auto rows = controllerFanLX->queryCommonFriends(commonPeerEditFanLX->text().trimmed());
        showResult(rows, controllerFanLX->lastMessage());
    });
    connect(recommendButtonFanLX, &QPushButton::clicked, this, [this, showResult] {
        const auto rows = controllerFanLX->queryCrossServiceRecommend(targetServiceComboFanLX->currentText());
        showResult(rows, controllerFanLX->lastMessage() +
                             QStringLiteral("；推荐只是候选，需在「添加好友」里发送申请。"));
    });
    connect(crossCommonButtonFanLX, &QPushButton::clicked, this, [this, showResult] {
        const auto rows = controllerFanLX->queryCrossServiceCommon(targetServiceComboFanLX->currentText());
        showResult(rows, controllerFanLX->lastMessage());
    });
    return body;
}

QString FriendPageFanLX::selectedPeerFanLX() const {
    auto *item = friendListFanLX->currentItem();
    return item ? peerOfRowFanLX(item->text()) : QString();
}

void FriendPageFanLX::openRemarkPageFanLX() {
    const auto peer = selectedPeerFanLX();
    if (peer.isEmpty()) {
        listHintFanLX->setText(QStringLiteral("请先在列表里选中一位好友"));
        return;
    }
    remarkPeerEditFanLX->setText(peer);
    remarkEditFanLX->clear();
    tagEditFanLX->clear();
    stackFanLX->setCurrentIndex(kRemarkPage);
}

void FriendPageFanLX::refreshFanLX() {
    if (!controllerFanLX->opened())
        return;
    applyFriendChromeFanLX(this, controllerFanLX->currentService());
    const auto selected = selectedPeerFanLX();
    friendListFanLX->blockSignals(true);
    friendListFanLX->clear();
    friendListFanLX->addItems(controllerFanLX->friendRows());
    for (int index = 0; index < friendListFanLX->count() && !selected.isEmpty(); ++index) {
        if (peerOfRowFanLX(friendListFanLX->item(index)->text()) == selected) {
            friendListFanLX->setCurrentRow(index);
            break;
        }
    }
    friendListFanLX->blockSignals(false);
    const bool loggedIn = controllerFanLX->currentServiceLoggedIn();
    const auto service = controllerFanLX->currentService();
    const auto account = controllerFanLX->currentAccountId();
    if (homeTitleFanLX) {
        homeTitleFanLX->setText(loggedIn ? QStringLiteral("%1 好友 · %2 人").arg(service).arg(
                                               controllerFanLX->friendAccounts().size())
                                         : QStringLiteral("%1 好友").arg(service));
    }
    listHintFanLX->setText(
        loggedIn ? QStringLiteral("%1 · %2").arg(service, account)
                 : QStringLiteral("%1 未登录：请到「账号与服务」确认登录，或切换到已登录的服务").arg(service));
    const auto subtitle = loggedIn ? QStringLiteral("%1 · %2").arg(service, account)
                                   : QStringLiteral("%1 未登录").arg(service);
    for (auto *label : findChildren<QLabel *>(QStringLiteral("friendHeaderSubtitleFanLX")))
        label->setText(subtitle);

    // 跨服务查询的目标只能是"本人另一个下级服务"：微X 是上层身份，没有好友列表；也排除当前服务本身。
    const auto previous = targetServiceComboFanLX->currentText();
    targetServiceComboFanLX->clear();
    for (const auto &name : {QStringLiteral("QQ"), QStringLiteral("微信"), QStringLiteral("微博")})
        if (name != service)
            targetServiceComboFanLX->addItem(name);
    const auto restored = targetServiceComboFanLX->findText(previous);
    if (restored >= 0)
        targetServiceComboFanLX->setCurrentIndex(restored);

    // 跨服务添加的来源：本人已开通的另一个下级服务。
    const auto previousSource = sourceServiceComboFanLX->currentText();
    sourceServiceComboFanLX->blockSignals(true);
    sourceServiceComboFanLX->clear();
    for (const auto &name : controllerFanLX->subscribedChatServices())
        if (name != service)
            sourceServiceComboFanLX->addItem(name);
    const auto sourceIndex = sourceServiceComboFanLX->findText(previousSource);
    if (sourceIndex >= 0)
        sourceServiceComboFanLX->setCurrentIndex(sourceIndex);
    sourceServiceComboFanLX->blockSignals(false);
    refreshCandidatesFanLX();
    applyFriendFilterFanLX();
}

void FriendPageFanLX::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // 角标只在好友页显示时向服务端对一次账。放进 refreshFanLX 会让每次本地刷新都多等一次网络。
    if (!controllerFanLX || !controllerFanLX->opened())
        return;
    QTimer::singleShot(0, this, [this] {
        if (controllerFanLX && controllerFanLX->opened())
            refreshRequestsFanLX();
    });
}

void FriendPageFanLX::showFriendSectionFanLX(int index) {
    if (!stackFanLX || index < 0 || index >= stackFanLX->count())
        return;
    if (index == kAddPage)
        refreshCandidatesFanLX();
    if (index == kRequestsPage)
        pickRequestSegmentFanLX = true;
    stackFanLX->setCurrentIndex(index);
    if (index == kRequestsPage)
        refreshRequestsFanLX();
}

void FriendPageFanLX::applyFriendFilterFanLX() {
    if (!friendListFanLX || !searchEditFanLX)
        return;
    const auto key = searchEditFanLX->text().trimmed();
    int visible = 0;
    for (int index = 0; index < friendListFanLX->count(); ++index) {
        auto *item = friendListFanLX->item(index);
        const auto parts = friendLinePartsFanLX(item->text());
        // 只匹配号码、备注、标签三个字段，不用整行原文。
        const bool match = key.isEmpty() ||
                           (parts.friendRecord && (parts.account.contains(key, Qt::CaseInsensitive) ||
                                                   parts.remark.contains(key, Qt::CaseInsensitive) ||
                                                   parts.tag.contains(key, Qt::CaseInsensitive)));
        item->setHidden(!match);
        if (match)
            ++visible;
    }
    if (auto *current = friendListFanLX->currentItem(); current && current->isHidden()) {
        friendListFanLX->blockSignals(true);
        friendListFanLX->clearSelection();
        friendListFanLX->setCurrentItem(nullptr);
        friendListFanLX->blockSignals(false);
    }
    const bool loggedIn = controllerFanLX->opened() && controllerFanLX->currentServiceLoggedIn();
    const bool noRows = friendListFanLX->count() == 0;
    if (homeListStackFanLX && homeEmptyFanLX && (noRows || visible == 0)) {
        homeListStackFanLX->setCurrentWidget(homeEmptyFanLX);
        if (noRows && !loggedIn) {
            homeEmptyFanLX->setError(false);
            homeEmptyFanLX->setText(QStringLiteral("%1 未登录").arg(controllerFanLX->currentService()),
                                    QStringLiteral("请到「账号与服务」确认登录，或切换到已登录的服务"));
            homeEmptyFanLX->setActionVisible(false);
        } else if (noRows) {
            homeEmptyFanLX->setError(false);
            homeEmptyFanLX->setText(QStringLiteral("还没有好友"),
                                    QStringLiteral("添加一位好友后，会显示在这里。"));
            homeEmptyFanLX->setActionVisible(true);
        } else {
            homeEmptyFanLX->setError(false);
            homeEmptyFanLX->setText(QStringLiteral("没有匹配的好友"),
                                    QStringLiteral("换一个号码、备注或标签再试。"));
            homeEmptyFanLX->setActionVisible(false);
        }
    } else if (homeListStackFanLX) {
        homeListStackFanLX->setCurrentWidget(friendListFanLX);
    }
    syncFriendActionsFanLX();
}

void FriendPageFanLX::updateFriendDetailFanLX() {
    const auto peer = selectedPeerFanLX();
    const bool loggedIn = controllerFanLX->opened() && controllerFanLX->currentServiceLoggedIn();
    const bool canEdit = loggedIn && !peer.isEmpty();
    if (removeButtonFanLX)
        removeButtonFanLX->setEnabled(canEdit);
    if (remarkNavButtonFanLX)
        remarkNavButtonFanLX->setEnabled(canEdit);
    if (detailCommonButtonFanLX)
        detailCommonButtonFanLX->setEnabled(canEdit);
    QString rowText;
    if (!peer.isEmpty() && friendListFanLX) {
        for (int index = 0; index < friendListFanLX->count(); ++index) {
            if (peerOfRowFanLX(friendListFanLX->item(index)->text()) == peer) {
                rowText = friendListFanLX->item(index)->text();
                break;
            }
        }
    }
    auto parts = friendLinePartsFanLX(rowText);
    if (parts.account.isEmpty())
        parts.account = peer;
    if (detailAvatarFanLX)
        detailAvatarFanLX->showPeer(peer, friendAvatarTextFanLX(parts));
    if (detailRemarkFanLX) {
        detailRemarkFanLX->setText(peer.isEmpty() ? QStringLiteral("选择一位好友查看详情")
                                                  : (parts.remark.isEmpty() ? peer : parts.remark));
        detailRemarkFanLX->setAlignment(peer.isEmpty() ? Qt::AlignCenter : Qt::AlignLeft | Qt::AlignVCenter);
        detailRemarkFanLX->setProperty("muted", peer.isEmpty() ? QStringLiteral("true") : QStringLiteral("false"));
        polishPropertyFanLX(detailRemarkFanLX);
    }
    if (detailAccountFanLX) {
        const bool showAccount = !peer.isEmpty() && !parts.remark.isEmpty();
        detailAccountFanLX->setText(showAccount ? peer : QString());
        detailAccountFanLX->setVisible(showAccount);
    }
    if (detailTagFanLX) {
        detailTagFanLX->setText(parts.tag);
        detailTagFanLX->setVisible(!parts.tag.isEmpty());
    }
}

void FriendPageFanLX::confirmRemoveFriendFanLX(const QString &peer) {
    if (peer.isEmpty() || !controllerFanLX->currentServiceLoggedIn())
        return;
    QMessageBox box(friendDialogParentFanLX(this));
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("删除好友"));
    box.setText(QStringLiteral("确定删除好友 %1？").arg(peer));
    auto *remove = box.addButton(QStringLiteral("删除"), QMessageBox::AcceptRole);
    auto *cancel = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(cancel));
    box.setEscapeButton(cancel);
    box.exec();
    if (box.clickedButton() == remove)
        controllerFanLX->removeFriend(peer);
}

void FriendPageFanLX::openCommonQueryFanLX(const QString &peer) {
    if (peer.isEmpty())
        return;
    commonPeerEditFanLX->setText(peer);
    stackFanLX->setCurrentIndex(kQueryPage);
    const auto rows = controllerFanLX->queryCommonFriends(peer);
    const auto note = controllerFanLX->lastMessage();
    showQueryResultFanLX(rows, note, QStringLiteral("与 %1 的同服务共同好友").arg(peer),
                         friendQuerySucceededFanLX(note));
}

void FriendPageFanLX::showQueryResultFanLX(const QStringList &rows, const QString &note, const QString &title,
                                          bool success) {
    Q_UNUSED(title);
    resultListFanLX->clear();
    resultListFanLX->addItems(rows);
    if (rows.isEmpty() && success)
        resultListFanLX->addItem(QStringLiteral("（没有共同好友）"));
    resultHintFanLX->setText(note);
}

void FriendPageFanLX::updateRequestActionsFanLX() {
    const bool loggedIn = controllerFanLX->opened() && controllerFanLX->currentServiceLoggedIn();
    if (acceptButtonFanLX)
        acceptButtonFanLX->setEnabled(loggedIn && !selectedPeerOfFanLX(incomingListFanLX).isEmpty());
    if (rejectButtonFanLX)
        rejectButtonFanLX->setEnabled(loggedIn && !selectedPeerOfFanLX(incomingListFanLX).isEmpty());
    if (cancelButtonFanLX)
        cancelButtonFanLX->setEnabled(loggedIn && !selectedPeerOfFanLX(outgoingListFanLX).isEmpty());
}

FriendPageHeaderFanLX *FriendPageFanLX::installFriendHeaderFanLX(int index, const QString &title) {
    auto *page = stackFanLX ? stackFanLX->widget(index) : nullptr;
    if (!page)
        return nullptr;
    for (auto *button : page->findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (button->property("role").toString() == QLatin1String("back"))
            button->hide();
    }
    for (auto *label : page->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (label->property("role").toString() == QLatin1String("title"))
            label->hide();
    }
    auto *header = new FriendPageHeaderFanLX(title, page);
    connect(header->backButton(), &QPushButton::clicked, this, [this] { stackFanLX->setCurrentIndex(0); });
    if (auto *layout = qobject_cast<QVBoxLayout *>(page->layout()))
        layout->insertWidget(0, header);
    return header;
}

void FriendPageFanLX::syncFriendActionsFanLX() {
    const bool loggedIn = controllerFanLX->opened() && controllerFanLX->currentServiceLoggedIn();
    if (addButtonFanLX)
        addButtonFanLX->setEnabled(loggedIn && peerEditFanLX && !peerEditFanLX->text().trimmed().isEmpty());
    if (requestsNavButtonFanLX)
        requestsNavButtonFanLX->setEnabled(loggedIn);
    if (remarkButtonFanLX)
        remarkButtonFanLX->setEnabled(loggedIn);
    if (commonButtonFanLX)
        commonButtonFanLX->setEnabled(loggedIn);
    if (recommendButtonFanLX)
        recommendButtonFanLX->setEnabled(loggedIn);
    if (crossCommonButtonFanLX)
        crossCommonButtonFanLX->setEnabled(loggedIn);
    if (addFromServiceButtonFanLX)
        addFromServiceButtonFanLX->setEnabled(loggedIn && !selectedPeerOfFanLX(candidateListFanLX).isEmpty());
    updateRequestActionsFanLX();
    updateFriendDetailFanLX();
}

} // namespace fanlxGui

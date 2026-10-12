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
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fanlxGui {

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

void fillListFanLX(QListWidget *list, const QStringList &rows, const QString &placeholder) {
    list->clear();
    list->addItems(rows);
    if (rows.isEmpty())
        list->addItem(placeholder);
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
    auto *rows = new FriendRowDelegateFanLX(this);
    prepareFriendRowListFanLX(friendListFanLX, QStringLiteral("friendListFanLX"));
    prepareFriendRowListFanLX(resultListFanLX, QStringLiteral("friendResultListFanLX"));
    friendListFanLX->setItemDelegate(rows);
    resultListFanLX->setItemDelegate(rows);

    // 当前平台切换与任何好友写操作成功后都要重读列表。
    connect(controllerFanLX, &DomainControllerFanLX::dataChangedFanLX, this, &FriendPageFanLX::refreshFanLX);
    connect(controllerFanLX, &DomainControllerFanLX::identityChangedFanLX, this,
            &FriendPageFanLX::refreshFanLX);
    refreshFanLX();
}

QWidget *FriendPageFanLX::buildHomeFanLX() {
    auto *home = new QWidget();
    auto *layout = new QVBoxLayout(home);

    friendListFanLX = new QListWidget(home);
    friendListFanLX->setMinimumHeight(220);
    listHintFanLX = kit::hintLabel(home);

    auto *addNav = friendPrimaryButtonFanLX(QStringLiteral("＋ 添加好友"), home);
    requestsNavButtonFanLX = friendSecondaryButtonFanLX(QStringLiteral("好友申请  ›"), home);
    remarkNavButtonFanLX = friendSecondaryButtonFanLX(QStringLiteral("备注 / 标签"), home);
    removeButtonFanLX = friendDangerButtonFanLX(QStringLiteral("删除好友"), home);
    auto *queryNav = friendSecondaryButtonFanLX(QStringLiteral("共同好友  ›"), home);
    auto *row = new QHBoxLayout();
    row->addWidget(addNav);
    row->addWidget(requestsNavButtonFanLX);
    row->addWidget(remarkNavButtonFanLX);
    row->addWidget(removeButtonFanLX);
    row->addStretch(1);
    row->addWidget(queryNav);

    layout->addWidget(listHintFanLX);
    layout->addWidget(friendListFanLX, 1);
    layout->addLayout(row);

    connect(addNav, &QPushButton::clicked, this, [this] {
        refreshCandidatesFanLX();
        stackFanLX->setCurrentIndex(kAddPage);
    });
    connect(requestsNavButtonFanLX, &QPushButton::clicked, this, [this] {
        stackFanLX->setCurrentIndex(kRequestsPage);
        refreshRequestsFanLX();
    });
    connect(queryNav, &QPushButton::clicked, this, [this] { stackFanLX->setCurrentIndex(kQueryPage); });
    connect(remarkNavButtonFanLX, &QPushButton::clicked, this, [this] { openRemarkPageFanLX(); });
    // 双击好友直接去编辑备注/标签。
    connect(friendListFanLX, &QListWidget::itemDoubleClicked, this, [this] { openRemarkPageFanLX(); });
    connect(removeButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto peer = selectedPeerFanLX();
        if (peer.isEmpty()) {
            listHintFanLX->setText(QStringLiteral("请先在列表里选中要删除的好友"));
            return;
        }
        if (QMessageBox::question(friendDialogParentFanLX(this), QStringLiteral("删除好友"),
                                  QStringLiteral("确定删除好友 %1？").arg(peer)) == QMessageBox::Yes)
            controllerFanLX->removeFriend(peer);
    });
    return home;
}

QWidget *FriendPageFanLX::buildAddBodyFanLX() {
    auto *body = new QWidget();
    auto *layout = new QVBoxLayout(body);

    // ① 按号码申请：对方同意后才成为好友；同意前双方可临时会话（每方向限额）。
    auto *byNumber = new QGroupBox(QStringLiteral("按号码发送好友申请"), body);
    peerEditFanLX = new QLineEdit(byNumber);
    peerEditFanLX->setPlaceholderText(QStringLiteral("对方在当前服务的号码，如 2026002"));
    noteEditFanLX = new QLineEdit(byNumber);
    noteEditFanLX->setPlaceholderText(QStringLiteral("附言（可选），如：我是你的同学"));
    addButtonFanLX = friendPrimaryButtonFanLX(QStringLiteral("发送好友申请"), byNumber);
    auto *numberForm = new QGridLayout(byNumber);
    numberForm->addWidget(new QLabel(QStringLiteral("号码"), byNumber), 0, 0);
    numberForm->addWidget(peerEditFanLX, 0, 1);
    numberForm->addWidget(new QLabel(QStringLiteral("附言"), byNumber), 1, 0);
    numberForm->addWidget(noteEditFanLX, 1, 1);
    numberForm->addWidget(addButtonFanLX, 1, 2);

    // ② 依据本人另一个服务的好友：源服务好友 → 同一个人 → 他在当前服务的号码。
    auto *byService = new QGroupBox(QStringLiteral("依据本人另一个服务的好友添加"), body);
    sourceServiceComboFanLX = new QComboBox(byService);
    candidateListFanLX = new QListWidget(byService);
    candidateListFanLX->setMinimumHeight(120);
    addFromServiceButtonFanLX = friendPrimaryButtonFanLX(QStringLiteral("向选中的人发送申请"), byService);
    auto *serviceRow = new QHBoxLayout();
    serviceRow->addWidget(new QLabel(QStringLiteral("来源服务"), byService));
    serviceRow->addWidget(sourceServiceComboFanLX);
    serviceRow->addStretch(1);
    serviceRow->addWidget(addFromServiceButtonFanLX);
    auto *serviceLayout = new QVBoxLayout(byService);
    serviceLayout->addLayout(serviceRow);
    serviceLayout->addWidget(candidateListFanLX, 1);

    addHintFanLX = kit::hintLabel(body);
    addHintFanLX->setText(QStringLiteral("好友需要对方同意才会成立；同意之前你们可以在聊天页临时会话（每方向有条数上限）。"));
    layout->addWidget(byNumber);
    layout->addWidget(byService, 1);
    layout->addWidget(addHintFanLX);

    connect(addButtonFanLX, &QPushButton::clicked, this, [this] {
        if (controllerFanLX->sendFriendRequest(peerEditFanLX->text().trimmed(), noteEditFanLX->text())) {
            peerEditFanLX->clear();
            noteEditFanLX->clear();
        }
        addHintFanLX->setText(controllerFanLX->lastMessage());
    });
    connect(peerEditFanLX, &QLineEdit::returnPressed, addButtonFanLX, &QPushButton::click);
    connect(sourceServiceComboFanLX, &QComboBox::currentTextChanged, this,
            [this] { refreshCandidatesFanLX(); });
    connect(addFromServiceButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto peer = selectedPeerOfFanLX(candidateListFanLX);
        if (peer.isEmpty()) {
            addHintFanLX->setText(QStringLiteral("请先在列表里选中来源服务里的一位好友"));
            return;
        }
        controllerFanLX->addFriendFromService(sourceServiceComboFanLX->currentText(), peer,
                                              noteEditFanLX->text());
        addHintFanLX->setText(controllerFanLX->lastMessage());
        refreshCandidatesFanLX();
    });
    return body;
}

QWidget *FriendPageFanLX::buildRequestsBodyFanLX() {
    auto *body = new QWidget();
    incomingListFanLX = new QListWidget(body);
    outgoingListFanLX = new QListWidget(body);
    temporaryListFanLX = new QListWidget(body);
    acceptButtonFanLX = friendPrimaryButtonFanLX(QStringLiteral("同意"), body);
    rejectButtonFanLX = friendDangerButtonFanLX(QStringLiteral("拒绝"), body);
    cancelButtonFanLX = friendDangerButtonFanLX(QStringLiteral("撤回"), body);
    auto *refresh = friendSecondaryButtonFanLX(QStringLiteral("刷新"), body);
    requestsHintFanLX = kit::hintLabel(body);

    auto *incomingBox = new QGroupBox(QStringLiteral("收到的申请（对方 ｜ 附言 ｜ 时间）"), body);
    auto *incomingLayout = new QVBoxLayout(incomingBox);
    incomingLayout->addWidget(incomingListFanLX);
    auto *incomingButtons = new QHBoxLayout();
    incomingButtons->addStretch(1);
    incomingButtons->addWidget(acceptButtonFanLX);
    incomingButtons->addWidget(rejectButtonFanLX);
    incomingLayout->addLayout(incomingButtons);

    auto *outgoingBox = new QGroupBox(QStringLiteral("我发出的申请（等待对方同意）"), body);
    auto *outgoingLayout = new QVBoxLayout(outgoingBox);
    outgoingLayout->addWidget(outgoingListFanLX);
    outgoingLayout->addWidget(cancelButtonFanLX, 0, Qt::AlignRight);

    auto *temporaryBox = new QGroupBox(QStringLiteral("临时会话（非好友，每方向限额）"), body);
    auto *temporaryLayout = new QVBoxLayout(temporaryBox);
    temporaryLayout->addWidget(temporaryListFanLX);

    auto *layout = new QVBoxLayout(body);
    auto *top = new QHBoxLayout();
    top->addWidget(requestsHintFanLX, 1);
    top->addWidget(refresh);
    layout->addLayout(top);
    layout->addWidget(incomingBox, 1);
    layout->addWidget(outgoingBox, 1);
    layout->addWidget(temporaryBox, 1);

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
    connect(refresh, &QPushButton::clicked, this, &FriendPageFanLX::refreshRequestsFanLX);
    return body;
}

void FriendPageFanLX::refreshRequestsFanLX() {
    if (!controllerFanLX->opened())
        return;
    const auto view = controllerFanLX->friendRequestView();
    if (!view.reachable) {
        fillListFanLX(incomingListFanLX, {}, QStringLiteral("（服务端不可达）"));
        fillListFanLX(outgoingListFanLX, {}, QStringLiteral("（服务端不可达）"));
        fillListFanLX(temporaryListFanLX, {}, QStringLiteral("（服务端不可达）"));
        requestsHintFanLX->setText(controllerFanLX->lastMessage());
        return;
    }
    fillListFanLX(incomingListFanLX, view.incoming, QStringLiteral("（没有待处理的申请）"));
    fillListFanLX(outgoingListFanLX, view.outgoing, QStringLiteral("（没有等待中的申请）"));
    fillListFanLX(temporaryListFanLX, view.temporary, QStringLiteral("（没有临时会话）"));
    requestsHintFanLX->setText(QStringLiteral("%1 · 收到 %2 条、发出 %3 条待处理申请；临时会话每方向最多 %4 条")
                                   .arg(controllerFanLX->currentService())
                                   .arg(view.incoming.size())
                                   .arg(view.outgoing.size())
                                   .arg(view.limit));
    if (!view.incoming.isEmpty())
        requestsNavButtonFanLX->setText(QStringLiteral("好友申请（%1）  ›").arg(view.incoming.size()));
    else
        requestsNavButtonFanLX->setText(QStringLiteral("好友申请  ›"));
}

void FriendPageFanLX::refreshCandidatesFanLX() {
    if (!controllerFanLX->opened() || !sourceServiceComboFanLX)
        return;
    const auto source = sourceServiceComboFanLX->currentText();
    if (source.isEmpty()) {
        fillListFanLX(candidateListFanLX, {}, QStringLiteral("（本人还没有开通其它服务）"));
        return;
    }
    fillListFanLX(candidateListFanLX, controllerFanLX->crossServiceFriendCandidates(source),
                  QStringLiteral("（%1 里没有可换算的好友）").arg(source));
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
    friendListFanLX->clear();
    friendListFanLX->addItems(controllerFanLX->friendRows());
    const bool loggedIn = controllerFanLX->currentServiceLoggedIn();
    if (friendListFanLX->count() == 0)
        friendListFanLX->addItem(loggedIn ? QStringLiteral("（当前服务还没有好友）")
                                          : QStringLiteral("（%1 未登录，无法查看好友）").arg(controllerFanLX->currentService()));
    // 未登录的服务没有会话，所有会写或查的按钮都置灰，不给用户点了又报错的机会。
    for (auto *button : {addButtonFanLX, removeButtonFanLX, remarkButtonFanLX, remarkNavButtonFanLX,
                         commonButtonFanLX, recommendButtonFanLX, crossCommonButtonFanLX,
                         requestsNavButtonFanLX, addFromServiceButtonFanLX, acceptButtonFanLX,
                         rejectButtonFanLX, cancelButtonFanLX})
        button->setEnabled(loggedIn);
    for (int index = 0; index < friendListFanLX->count() && !selected.isEmpty(); ++index)
        if (peerOfRowFanLX(friendListFanLX->item(index)->text()) == selected)
            friendListFanLX->setCurrentRow(index);

    const auto service = controllerFanLX->currentService();
    listHintFanLX->setText(
        loggedIn ? QStringLiteral("%1 · %2 　好友 %3 人")
                       .arg(service, controllerFanLX->currentAccountId())
                       .arg(controllerFanLX->friendAccounts().size())
                 : QStringLiteral("%1 未登录：请到「账号与服务」确认登录，或切换到已登录的服务").arg(service));

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
}

} // namespace fanlxGui

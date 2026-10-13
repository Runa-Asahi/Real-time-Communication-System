#include "gui/GroupPageFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/GroupListDelegateFanLX.h"
#include "gui/PageKitFanLX.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fanlxGui {

namespace {

constexpr int kJoinPage = 1;
constexpr int kMemberPage = 2;
constexpr int kDiscussionPage = 3;
constexpr int kSettingsPage = 4;

// 群列表行格式为 "1001 ｜ 群名 ｜ 模式 QQ ｜ 成员 3 人"，群号是第一个分隔符之前的部分。
QString idOfRowFanLX(const QString &row) {
    const auto separator = row.indexOf(QStringLiteral(" ｜ "));
    return separator < 0 ? row : row.left(separator);
}

// 子页面顶部的"操作对象"提示，refreshFanLX 统一更新文字。
QLabel *makeScopeLabelFanLX(QWidget *parent, QList<QLabel *> &registry) {
    auto *label = kit::hintLabel(parent);
    label->setProperty("fanlxKind", QStringLiteral("scope"));
    registry.push_back(label);
    return label;
}

QLabel *pageTitleFanLX(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("pageTitle"));
    return label;
}

QLabel *sectionTitleFanLX(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("sectionTitle"));
    return label;
}

// 输入框不再拉满整行。数字账号 / 群号用 fanlxKind=mono，字号和颜色交给 app.qss。
void tuneFieldFanLX(QLineEdit *edit, bool numeric) {
    edit->setMaximumWidth(480);
    if (numeric)
        edit->setProperty("fanlxKind", QStringLiteral("mono"));
}

void tuneActionButtonFanLX(QPushButton *button, bool expand) {
    // 首页两列和普通操作行都固定为内容宽度：Expanding 会在窗口拉宽时把按钮拉变形。
    (void)expand;
    button->setMinimumSize(120, 32);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

// 列表与「暂无数据」放在同一个布局单元格里互斥显示。QGridLayout 不是控件，不包裹 QListWidget。
void mountListFanLX(QBoxLayout *host, QListWidget *list, int stretch, GroupListDelegateFanLX *delegate) {
    if (delegate != nullptr) {
        list->setItemDelegate(delegate);
        list->setMouseTracking(true);
        if (auto *viewport = list->viewport()) {
            viewport->setAttribute(Qt::WA_Hover, true);
            viewport->setMouseTracking(true);
        }
    }
    auto *label = new QLabel(QStringLiteral("暂无数据"), list->parentWidget());
    label->setAlignment(Qt::AlignCenter);
    label->setProperty("role", QStringLiteral("empty"));
    label->setMinimumHeight(list->minimumHeight());
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *cell = new QGridLayout();
    cell->setContentsMargins(0, 0, 0, 0);
    cell->setSpacing(0);
    cell->setRowStretch(0, 1);
    cell->setColumnStretch(0, 1);
    cell->addWidget(list, 0, 0);
    cell->addWidget(label, 0, 0);
    // 列表析构时 QListWidget::clear() 仍会发出 modelReset。连接挂在后创建的 guard 上，
    // 它会先于内部 model 被销毁并断开槽，避免回调踩进正在析构的列表或已经销毁的标签。
    auto *guard = new QObject(list);
    const auto sync = [list = QPointer<QListWidget>(list), label = QPointer<QLabel>(label)] {
        if (list.isNull() || label.isNull())
            return;
        const bool empty = list->count() == 0;
        list->setVisible(!empty);
        label->setVisible(empty);
        if (empty)
            label->raise();
        else
            list->raise();
    };
    if (auto *model = list->model()) {
        QObject::connect(model, &QAbstractItemModel::modelReset, guard, sync);
        QObject::connect(model, &QAbstractItemModel::rowsInserted, guard,
                         [sync](const QModelIndex &, int, int) { sync(); });
        QObject::connect(model, &QAbstractItemModel::rowsRemoved, guard,
                         [sync](const QModelIndex &, int, int) { sync(); });
    }
    sync();
    host->addLayout(cell, stretch);
}

QFrame *makeCardFanLX(QWidget *parent) {
    auto *card = kit::sectionCard(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    return card;
}

QVBoxLayout *cardLayoutFanLX(QFrame *card) { return static_cast<QVBoxLayout *>(card->layout()); }

// 服务切换时，连同包住分组框的卡片一起显示或隐藏，避免留下空白白卡。
void syncWrappedCardFanLX(QWidget *box, bool visible) {
    if (!box)
        return;
    box->setVisible(visible);
    if (auto *parent = box->parentWidget()) {
        if (parent->objectName() == QLatin1String("card"))
            parent->setVisible(visible);
    }
}

} // namespace

GroupPageFanLX::GroupPageFanLX(DomainControllerFanLX *controller, QWidget *parent)
    : QWidget(parent), controllerFanLX(controller) {
    setObjectName(QStringLiteral("groupPageFanLX"));
    setAttribute(Qt::WA_StyledBackground, true);
    stackFanLX = new QStackedWidget(this);
    kit::setHomePage(stackFanLX, buildHomeFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("加入与待处理"), buildJoinBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("成员管理"), buildMemberBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("子群（讨论组）"), buildDiscussionBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("群设置"), buildSettingsBodyFanLX());

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(stackFanLX);

    // 滚动区底色交给 app.qss。子页面标题改放到返回按钮下方，返回行里那份标题藏起来，文字仍在。
    for (auto *area : findChildren<QScrollArea *>()) {
        area->setObjectName(QStringLiteral("groupScrollFanLX"));
        if (auto *viewport = area->viewport()) {
            viewport->setObjectName(QStringLiteral("groupViewportFanLX"));
            viewport->setAttribute(Qt::WA_StyledBackground, true);
            viewport->setAutoFillBackground(false);
        }
    }
    for (auto *back : findChildren<QPushButton *>()) {
        if (back->property("role").toString() != QLatin1String("back"))
            continue;
        auto *page = back->parentWidget();
        if (!page)
            continue;
        for (auto *label : page->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly)) {
            if (label->property("role").toString() == QLatin1String("title"))
                label->setVisible(false);
        }
    }

    connect(controllerFanLX, &DomainControllerFanLX::dataChangedFanLX, this, &GroupPageFanLX::refreshFanLX);
    connect(controllerFanLX, &DomainControllerFanLX::identityChangedFanLX, this,
            &GroupPageFanLX::refreshFanLX);
    refreshFanLX();
}

QWidget *GroupPageFanLX::buildHomeFanLX() {
    auto *home = new QWidget();
    auto *layout = new QVBoxLayout(home);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(12);

    headerLabelFanLX = new QLabel(home);
    headerLabelFanLX->setProperty("role", QStringLiteral("title"));
    unsupportedLabelFanLX = kit::hintLabel(home);
    unsupportedLabelFanLX->setText(
        QStringLiteral("当前服务没有群功能。请在「账号与服务」里把当前服务切换为 QQ 或微信。"));
    layout->addWidget(headerLabelFanLX);
    layout->addWidget(unsupportedLabelFanLX);

    contentFanLX = new QWidget(home);
    auto *content = new QVBoxLayout(contentFanLX);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(12);

    groupListFanLX = new QListWidget(contentFanLX);
    groupListFanLX->setMinimumHeight(120);

    auto *idCard = makeCardFanLX(contentFanLX);
    groupEditFanLX = new QLineEdit(idCard);
    groupEditFanLX->setPlaceholderText(QStringLiteral("群号（选中列表中的群自动填入，也可手输）"));
    tuneFieldFanLX(groupEditFanLX, true);
    cardLayoutFanLX(idCard)->addWidget(sectionTitleFanLX(QStringLiteral("当前群号"), idCard));
    cardLayoutFanLX(idCard)->addWidget(groupEditFanLX, 0, Qt::AlignLeft);

    auto *memberCaption = new QLabel(QStringLiteral("群成员"), contentFanLX);
    memberCaption->setProperty("role", QStringLiteral("sectionTitle"));
    memberListFanLX = new QListWidget(contentFanLX);
    memberListFanLX->setMinimumHeight(110);

    joinNavFanLX = kit::navButton(QStringLiteral("加入与待处理  ›"), contentFanLX);
    auto *memberNav = kit::navButton(QStringLiteral("成员管理  ›"), contentFanLX);
    discussionNavFanLX = kit::navButton(QStringLiteral("子群  ›"), contentFanLX);
    auto *settingsNav = kit::navButton(QStringLiteral("群设置  ›"), contentFanLX);
    for (auto *button : {joinNavFanLX, memberNav, discussionNavFanLX, settingsNav})
        tuneActionButtonFanLX(button, true);
    auto *entryCard = makeCardFanLX(contentFanLX);
    cardLayoutFanLX(entryCard)->addWidget(sectionTitleFanLX(QStringLiteral("功能入口"), entryCard));
    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->addWidget(joinNavFanLX, 0, 0);
    grid->addWidget(memberNav, 0, 1);
    grid->addWidget(discussionNavFanLX, 1, 0);
    grid->addWidget(settingsNav, 1, 1);
    cardLayoutFanLX(entryCard)->addLayout(grid);

    mountListFanLX(content, groupListFanLX, 1, nullptr);
    content->addWidget(idCard);
    content->addWidget(memberCaption);
    mountListFanLX(content, memberListFanLX, 1,
                   new GroupListDelegateFanLX(GroupListDelegateFanLX::Kind::Member, memberListFanLX));
    content->addWidget(entryCard);
    layout->addWidget(contentFanLX, 1);

    connect(groupListFanLX, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *current, QListWidgetItem *) {
                if (!current || current->text().startsWith(QStringLiteral("（")))
                    return;
                groupEditFanLX->setText(idOfRowFanLX(current->text()));
                refreshFanLX();
            });
    connect(groupEditFanLX, &QLineEdit::editingFinished, this, &GroupPageFanLX::refreshFanLX);
    connect(joinNavFanLX, &QPushButton::clicked, this, [this] { stackFanLX->setCurrentIndex(kJoinPage); });
    connect(memberNav, &QPushButton::clicked, this, [this] { stackFanLX->setCurrentIndex(kMemberPage); });
    connect(discussionNavFanLX, &QPushButton::clicked, this,
            [this] { stackFanLX->setCurrentIndex(kDiscussionPage); });
    connect(settingsNav, &QPushButton::clicked, this, [this] { stackFanLX->setCurrentIndex(kSettingsPage); });
    return home;
}

QWidget *GroupPageFanLX::buildJoinBodyFanLX() {
    auto *body = new QWidget();
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(12);
    auto *heading = new QVBoxLayout();
    heading->setSpacing(4);
    heading->addWidget(pageTitleFanLX(QStringLiteral("加入与待处理"), body));
    heading->addWidget(makeScopeLabelFanLX(body, scopeLabelsFanLX));
    layout->addLayout(heading);

    // ① 邀请他人入群：QQ 与微信都有，但流程不同（提示文字随服务变化，见 applyServiceFanLX）。
    auto *inviteCard = makeCardFanLX(body);
    cardLayoutFanLX(inviteCard)->addWidget(sectionTitleFanLX(QStringLiteral("邀请他人入群"), inviteCard));
    inviteBoxFanLX = new QGroupBox(QStringLiteral("邀请他人入群"), inviteCard);
    inviteTargetEditFanLX = new QLineEdit(inviteBoxFanLX);
    inviteTargetEditFanLX->setPlaceholderText(QStringLiteral("被邀请人的账号"));
    tuneFieldFanLX(inviteTargetEditFanLX, true);
    inviteButtonFanLX = kit::primaryButton(QStringLiteral("邀请入群"), inviteBoxFanLX);
    tuneActionButtonFanLX(inviteButtonFanLX, false);
    inviteHintFanLX = kit::hintLabel(inviteBoxFanLX);
    auto *inviteRow = new QHBoxLayout();
    inviteRow->setSpacing(12);
    inviteRow->addWidget(inviteTargetEditFanLX, 0);
    inviteRow->addWidget(inviteButtonFanLX, 0);
    inviteRow->addStretch(1);
    auto *inviteLayout = new QVBoxLayout(inviteBoxFanLX);
    inviteLayout->setContentsMargins(0, 0, 0, 0);
    inviteLayout->setSpacing(8);
    inviteLayout->addLayout(inviteRow);
    inviteLayout->addWidget(inviteHintFanLX);
    cardLayoutFanLX(inviteCard)->addWidget(inviteBoxFanLX);

    // ② QQ：申请 - 审批。拒绝与批准分开，危险操作单独成组。
    auto *qqCard = makeCardFanLX(body);
    cardLayoutFanLX(qqCard)->addWidget(sectionTitleFanLX(QStringLiteral("申请与审批"), qqCard));
    auto *qqBox = new QGroupBox(QStringLiteral("申请与审批（QQ）"), qqCard);
    qqJoinBoxFanLX = qqBox;
    applyButtonFanLX = kit::navButton(QStringLiteral("申请加入群"), qqBox);
    tuneActionButtonFanLX(applyButtonFanLX, false);
    targetAccountEditFanLX = new QLineEdit(qqBox);
    targetAccountEditFanLX->setPlaceholderText(QStringLiteral("申请人账号（群主 / 管理员审批时填写）"));
    tuneFieldFanLX(targetAccountEditFanLX, true);
    approveButtonFanLX = kit::primaryButton(QStringLiteral("批准入群申请"), qqBox);
    rejectApplyButtonFanLX = kit::dangerButton(QStringLiteral("拒绝入群申请"), qqBox);
    tuneActionButtonFanLX(approveButtonFanLX, false);
    tuneActionButtonFanLX(rejectApplyButtonFanLX, false);
    auto *approveRow = new QHBoxLayout();
    approveRow->setSpacing(12);
    approveRow->addWidget(targetAccountEditFanLX, 0);
    approveRow->addWidget(approveButtonFanLX, 0);
    approveRow->addStretch(1);
    auto *rejectRow = new QHBoxLayout();
    rejectRow->addWidget(rejectApplyButtonFanLX, 0);
    rejectRow->addStretch(1);
    auto *qqLayout = new QVBoxLayout(qqBox);
    qqLayout->setContentsMargins(0, 0, 0, 0);
    qqLayout->setSpacing(10);
    qqLayout->addWidget(applyButtonFanLX, 0, Qt::AlignLeft);
    qqLayout->addLayout(approveRow);
    qqLayout->addSpacing(16);
    qqLayout->addLayout(rejectRow);
    cardLayoutFanLX(qqCard)->addWidget(qqBox);

    // ③ 微信：本人收到的推荐要由本人确认
    auto *wechatCard = makeCardFanLX(body);
    cardLayoutFanLX(wechatCard)->addWidget(sectionTitleFanLX(QStringLiteral("收到的入群推荐"), wechatCard));
    auto *wechatBox = new QGroupBox(QStringLiteral("收到的入群推荐（微信）"), wechatCard);
    wechatJoinBoxFanLX = wechatBox;
    confirmInviteButtonFanLX = kit::primaryButton(QStringLiteral("确认本人的入群推荐"), wechatBox);
    rejectInviteButtonFanLX = kit::dangerButton(QStringLiteral("拒绝本人的入群推荐"), wechatBox);
    tuneActionButtonFanLX(confirmInviteButtonFanLX, false);
    tuneActionButtonFanLX(rejectInviteButtonFanLX, false);
    auto *confirmRow = new QHBoxLayout();
    confirmRow->addWidget(confirmInviteButtonFanLX, 0);
    confirmRow->addStretch(1);
    auto *rejectInviteRow = new QHBoxLayout();
    rejectInviteRow->addWidget(rejectInviteButtonFanLX, 0);
    rejectInviteRow->addStretch(1);
    auto *wechatLayout = new QVBoxLayout(wechatBox);
    wechatLayout->setContentsMargins(0, 0, 0, 0);
    wechatLayout->setSpacing(10);
    wechatLayout->addLayout(confirmRow);
    wechatLayout->addSpacing(16);
    wechatLayout->addLayout(rejectInviteRow);
    cardLayoutFanLX(wechatCard)->addWidget(wechatBox);

    leaveButtonFanLX = kit::dangerButton(QStringLiteral("退出该群"), body);
    tuneActionButtonFanLX(leaveButtonFanLX, false);
    pendingListFanLX = new QListWidget(body);
    pendingListFanLX->setObjectName(QStringLiteral("groupPendingListFanLX"));
    pendingListFanLX->setFrameShape(QFrame::NoFrame);
    pendingListFanLX->setMinimumHeight(110);
    auto *pendingCaption = new QLabel(QStringLiteral("待审批 / 待确认"), body);
    pendingCaption->setProperty("role", QStringLiteral("sectionTitle"));

    layout->addWidget(inviteCard);
    layout->addWidget(qqCard);
    layout->addWidget(wechatCard);
    layout->addWidget(pendingCaption);
    mountListFanLX(layout, pendingListFanLX, 1,
                   new GroupListDelegateFanLX(GroupListDelegateFanLX::Kind::Pending, pendingListFanLX));
    layout->addSpacing(16);
    layout->addWidget(leaveButtonFanLX, 0, Qt::AlignLeft);

    const auto service = [this] { return controllerFanLX->currentService(); };
    const auto group = [this] { return groupIdFanLX(); };
    const auto target = [this] { return targetAccountEditFanLX->text().trimmed(); };
    connect(applyButtonFanLX, &QPushButton::clicked, this,
            [this, service, group] { controllerFanLX->applyToJoinGroup(service(), group()); });
    connect(approveButtonFanLX, &QPushButton::clicked, this, [this, service, group, target] {
        controllerFanLX->approveGroupApplication(service(), group(), target(), false);
    });
    connect(rejectApplyButtonFanLX, &QPushButton::clicked, this, [this, service, group, target] {
        controllerFanLX->approveGroupApplication(service(), group(), target(), true);
    });
    connect(inviteButtonFanLX, &QPushButton::clicked, this, [this, service, group] {
        if (controllerFanLX->inviteGroupMember(service(), group(), inviteTargetEditFanLX->text().trimmed()))
            inviteTargetEditFanLX->clear();
    });
    connect(confirmInviteButtonFanLX, &QPushButton::clicked, this, [this, service, group] {
        controllerFanLX->confirmGroupInvitation(service(), group(), QString(), false);
    });
    connect(rejectInviteButtonFanLX, &QPushButton::clicked, this, [this, service, group] {
        controllerFanLX->confirmGroupInvitation(service(), group(), QString(), true);
    });
    connect(leaveButtonFanLX, &QPushButton::clicked, this, [this, service, group] {
        if (QMessageBox::question(this, QStringLiteral("退出该群"),
                                  QStringLiteral("确定退出群 %1？").arg(group())) == QMessageBox::Yes)
            controllerFanLX->leaveGroup(service(), group());
    });
    return body;
}

QWidget *GroupPageFanLX::buildMemberBodyFanLX() {
    auto *body = new QWidget();
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(12);
    auto *heading = new QVBoxLayout();
    heading->setSpacing(4);
    heading->addWidget(pageTitleFanLX(QStringLiteral("成员管理"), body));
    heading->addWidget(makeScopeLabelFanLX(body, scopeLabelsFanLX));
    layout->addLayout(heading);

    auto *card = makeCardFanLX(body);
    cardLayoutFanLX(card)->addWidget(sectionTitleFanLX(QStringLiteral("成员操作"), card));
    memberTargetEditFanLX = new QLineEdit(card);
    memberTargetEditFanLX->setPlaceholderText(QStringLiteral("目标成员账号"));
    tuneFieldFanLX(memberTargetEditFanLX, true);
    kickButtonFanLX = kit::dangerButton(QStringLiteral("挨 T（移出成员）"), card);
    transferButtonFanLX = kit::navButton(QStringLiteral("转让群主"), card);
    tuneActionButtonFanLX(kickButtonFanLX, false);
    tuneActionButtonFanLX(transferButtonFanLX, false);

    // 管理员制度只有 QQ 群有：微信群只有群主一个特权账号。
    adminBoxFanLX = new QWidget(card);
    assignAdminButtonFanLX = kit::navButton(QStringLiteral("任命管理员"), adminBoxFanLX);
    revokeAdminButtonFanLX = kit::navButton(QStringLiteral("撤销管理员"), adminBoxFanLX);
    tuneActionButtonFanLX(assignAdminButtonFanLX, false);
    tuneActionButtonFanLX(revokeAdminButtonFanLX, false);
    auto *adminRow = new QHBoxLayout(adminBoxFanLX);
    adminRow->setContentsMargins(0, 0, 0, 0);
    adminRow->setSpacing(12);
    adminRow->addWidget(assignAdminButtonFanLX);
    adminRow->addWidget(revokeAdminButtonFanLX);
    adminRow->addStretch(1);

    auto *normalRow = new QHBoxLayout();
    normalRow->setSpacing(12);
    normalRow->addWidget(transferButtonFanLX);
    normalRow->addWidget(adminBoxFanLX);
    normalRow->addStretch(1);
    auto *dangerRow = new QHBoxLayout();
    dangerRow->addWidget(kickButtonFanLX);
    dangerRow->addStretch(1);

    cardLayoutFanLX(card)->addWidget(memberTargetEditFanLX, 0, Qt::AlignLeft);
    cardLayoutFanLX(card)->addLayout(normalRow);
    cardLayoutFanLX(card)->addSpacing(16);
    cardLayoutFanLX(card)->addLayout(dangerRow);
    layout->addWidget(card);
    layout->addStretch(1);

    const auto service = [this] { return controllerFanLX->currentService(); };
    const auto group = [this] { return groupIdFanLX(); };
    const auto target = [this] { return memberTargetEditFanLX->text().trimmed(); };
    connect(kickButtonFanLX, &QPushButton::clicked, this,
            [this, service, group, target] { controllerFanLX->kickGroupMember(service(), group(), target()); });
    connect(assignAdminButtonFanLX, &QPushButton::clicked, this, [this, service, group, target] {
        controllerFanLX->assignGroupAdmin(service(), group(), target(), false);
    });
    connect(revokeAdminButtonFanLX, &QPushButton::clicked, this, [this, service, group, target] {
        controllerFanLX->assignGroupAdmin(service(), group(), target(), true);
    });
    connect(transferButtonFanLX, &QPushButton::clicked, this,
            [this, service, group, target] { controllerFanLX->transferGroupOwner(service(), group(), target()); });
    return body;
}

QWidget *GroupPageFanLX::buildDiscussionBodyFanLX() {
    auto *body = new QWidget();
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(12);
    auto *heading = new QVBoxLayout();
    heading->setSpacing(4);
    heading->addWidget(pageTitleFanLX(QStringLiteral("子群（讨论组）"), body));
    heading->addWidget(makeScopeLabelFanLX(body, scopeLabelsFanLX));
    layout->addLayout(heading);

    auto *card = makeCardFanLX(body);
    cardLayoutFanLX(card)->addWidget(sectionTitleFanLX(QStringLiteral("子群操作"), card));
    discussionIdEditFanLX = new QLineEdit(card);
    discussionIdEditFanLX->setPlaceholderText(QStringLiteral("子群编号，如 D1"));
    tuneFieldFanLX(discussionIdEditFanLX, true);
    discussionTargetEditFanLX = new QLineEdit(card);
    discussionTargetEditFanLX->setPlaceholderText(QStringLiteral("要加入子群的成员账号"));
    tuneFieldFanLX(discussionTargetEditFanLX, true);
    createDiscussionButtonFanLX = kit::primaryButton(QStringLiteral("创建子群/讨论组"), card);
    joinDiscussionButtonFanLX = kit::navButton(QStringLiteral("把成员加入子群"), card);
    tuneActionButtonFanLX(createDiscussionButtonFanLX, false);
    tuneActionButtonFanLX(joinDiscussionButtonFanLX, false);
    discussionListFanLX = new QListWidget(body);
    discussionListFanLX->setObjectName(QStringLiteral("groupDiscussionListFanLX"));
    discussionListFanLX->setFrameShape(QFrame::NoFrame);
    discussionListFanLX->setMinimumHeight(110);

    auto *row = new QHBoxLayout();
    row->setSpacing(12);
    row->addWidget(createDiscussionButtonFanLX);
    row->addWidget(joinDiscussionButtonFanLX);
    row->addStretch(1);
    cardLayoutFanLX(card)->addWidget(discussionIdEditFanLX, 0, Qt::AlignLeft);
    cardLayoutFanLX(card)->addWidget(discussionTargetEditFanLX, 0, Qt::AlignLeft);
    cardLayoutFanLX(card)->addLayout(row);
    layout->addWidget(card);
    auto *listCaption = new QLabel(QStringLiteral("子群记录"), body);
    listCaption->setProperty("role", QStringLiteral("sectionTitle"));
    layout->addWidget(listCaption);
    mountListFanLX(layout, discussionListFanLX, 1,
                   new GroupListDelegateFanLX(GroupListDelegateFanLX::Kind::Discussion, discussionListFanLX));

    const auto service = [this] { return controllerFanLX->currentService(); };
    const auto group = [this] { return groupIdFanLX(); };
    connect(createDiscussionButtonFanLX, &QPushButton::clicked, this, [this, service, group] {
        controllerFanLX->createDiscussionGroup(service(), group(), discussionIdEditFanLX->text().trimmed());
    });
    connect(joinDiscussionButtonFanLX, &QPushButton::clicked, this, [this, service, group] {
        controllerFanLX->joinDiscussionGroup(service(), group(), discussionIdEditFanLX->text().trimmed(),
                                             discussionTargetEditFanLX->text().trimmed());
    });
    return body;
}

QWidget *GroupPageFanLX::buildSettingsBodyFanLX() {
    auto *body = new QWidget();
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(12);
    auto *heading = new QVBoxLayout();
    heading->setSpacing(4);
    heading->addWidget(pageTitleFanLX(QStringLiteral("群设置"), body));
    heading->addWidget(makeScopeLabelFanLX(body, scopeLabelsFanLX));
    layout->addLayout(heading);

    auto *policyCard = makeCardFanLX(body);
    cardLayoutFanLX(policyCard)->addWidget(sectionTitleFanLX(QStringLiteral("群特色"), policyCard));
    policyViewFanLX = new QPlainTextEdit(policyCard);
    policyViewFanLX->setReadOnly(true);
    policyViewFanLX->setMinimumHeight(140);
    auto *policyBox = new QGroupBox(QStringLiteral("群特色"), policyCard);
    auto *policyLayout = new QVBoxLayout(policyBox);
    policyLayout->setContentsMargins(0, 0, 0, 0);
    policyLayout->setSpacing(10);
    policyLayout->addWidget(policyViewFanLX);
    // 动态变换：只替换策略对象，成员、加入序号与子群记录原地保留；
    // 只显示"变成另一种"的那个按钮（见 refreshFanLX）。
    toQqButtonFanLX = kit::navButton(QStringLiteral("切换为 QQ 群管理特色"), policyBox);
    toWechatButtonFanLX = kit::navButton(QStringLiteral("切换为微信群管理特色"), policyBox);
    tuneActionButtonFanLX(toQqButtonFanLX, false);
    tuneActionButtonFanLX(toWechatButtonFanLX, false);
    auto *switchRow = new QHBoxLayout();
    switchRow->setSpacing(12);
    switchRow->addWidget(toQqButtonFanLX);
    switchRow->addWidget(toWechatButtonFanLX);
    switchRow->addStretch(1);
    policyLayout->addLayout(switchRow);
    cardLayoutFanLX(policyCard)->addWidget(policyBox);

    auto *basicCard = makeCardFanLX(body);
    cardLayoutFanLX(basicCard)->addWidget(sectionTitleFanLX(QStringLiteral("群基本操作"), basicCard));
    groupNameEditFanLX = new QLineEdit(basicCard);
    groupNameEditFanLX->setPlaceholderText(QStringLiteral("群名"));
    tuneFieldFanLX(groupNameEditFanLX, false);
    renameButtonFanLX = kit::primaryButton(QStringLiteral("群改名"), basicCard);
    createGroupButtonFanLX = kit::navButton(QStringLiteral("创建群"), basicCard);
    createGroupButtonFanLX->setToolTip(QStringLiteral("使用首页填写的群号创建新群"));
    dissolveButtonFanLX = kit::dangerButton(QStringLiteral("解散该群"), basicCard);
    tuneActionButtonFanLX(renameButtonFanLX, false);
    tuneActionButtonFanLX(createGroupButtonFanLX, false);
    tuneActionButtonFanLX(dissolveButtonFanLX, false);
    auto *nameRow = new QHBoxLayout();
    nameRow->setSpacing(12);
    nameRow->addWidget(groupNameEditFanLX, 0);
    nameRow->addWidget(renameButtonFanLX, 0);
    nameRow->addStretch(1);
    auto *createRow = new QHBoxLayout();
    createRow->addWidget(createGroupButtonFanLX, 0);
    createRow->addStretch(1);
    auto *dangerRow = new QHBoxLayout();
    dangerRow->addWidget(dissolveButtonFanLX, 0);
    dangerRow->addStretch(1);
    cardLayoutFanLX(basicCard)->addLayout(nameRow);
    cardLayoutFanLX(basicCard)->addLayout(createRow);
    cardLayoutFanLX(basicCard)->addSpacing(16);
    cardLayoutFanLX(basicCard)->addLayout(dangerRow);

    layout->addWidget(policyCard);
    layout->addWidget(basicCard);
    layout->addStretch(1);

    const auto service = [this] { return controllerFanLX->currentService(); };
    const auto group = [this] { return groupIdFanLX(); };
    connect(toQqButtonFanLX, &QPushButton::clicked, this,
            [this, service, group] { controllerFanLX->switchGroupMode(service(), group(), true); });
    connect(toWechatButtonFanLX, &QPushButton::clicked, this,
            [this, service, group] { controllerFanLX->switchGroupMode(service(), group(), false); });
    connect(renameButtonFanLX, &QPushButton::clicked, this, [this, service, group] {
        controllerFanLX->renameGroup(service(), group(), groupNameEditFanLX->text().trimmed());
    });
    connect(createGroupButtonFanLX, &QPushButton::clicked, this,
            [this, service, group] { controllerFanLX->createGroup(service(), group()); });
    connect(dissolveButtonFanLX, &QPushButton::clicked, this, [this, service, group] {
        if (QMessageBox::question(this, QStringLiteral("解散该群"),
                                  QStringLiteral("确定解散群 %1？此操作不可恢复。").arg(group())) ==
            QMessageBox::Yes)
            controllerFanLX->dissolveGroup(service(), group());
    });
    return body;
}

QString GroupPageFanLX::groupIdFanLX() const { return groupEditFanLX->text().trimmed(); }

void GroupPageFanLX::applyServiceFanLX(const QString &service) {
    const bool qq = service == QStringLiteral("QQ");
    const bool wechat = service == QStringLiteral("微信");
    const bool supported = qq || wechat;

    // 未登录的服务不能操作群（领域层要求登录态），直接说明原因，而不是让每个按钮点了再报错。
    const bool loggedIn = controllerFanLX->currentServiceLoggedIn();
    const bool usable = supported && loggedIn;
    unsupportedLabelFanLX->setText(
        supported ? QStringLiteral("%1 未登录：请到「账号与服务」确认登录，或切换到已登录的服务。").arg(service)
                  : QStringLiteral("当前服务没有群功能。请在「账号与服务」里把当前服务切换为 QQ 或微信。"));
    unsupportedLabelFanLX->setVisible(!usable);
    contentFanLX->setVisible(usable);
    // QQ：申请-审批、管理员、子群；微信：推荐-确认，只有群主是特权账号。
    syncWrappedCardFanLX(qqJoinBoxFanLX, qq);
    syncWrappedCardFanLX(wechatJoinBoxFanLX, wechat);
    adminBoxFanLX->setVisible(qq);
    discussionNavFanLX->setVisible(qq);
    joinNavFanLX->setText(qq ? QStringLiteral("邀请、申请与审批  ›") : QStringLiteral("邀请与推荐确认  ›"));
    // 邀请他人：两个服务都有，落到各自的入群流程。
    inviteButtonFanLX->setText(QStringLiteral("邀请入群"));
    inviteHintFanLX->setText(qq ? QStringLiteral("QQ 群：邀请后会替对方提交入群申请，需群主或管理员批准后才会入群。")
                                : QStringLiteral("微信群：邀请会发给对方一条推荐，对方本人确认后才会入群。"));

    // 当前服务不支持的子页面不应停留：回到首页。
    const int page = stackFanLX->currentIndex();
    if (page != 0 && (!usable || (page == kDiscussionPage && !qq)))
        stackFanLX->setCurrentIndex(0);
}

void GroupPageFanLX::refreshFanLX() {
    if (!controllerFanLX->opened())
        return;
    const auto service = controllerFanLX->currentService();
    // 应用级样式表已经带上群页规则时，不再套页面样式表。
    // 页面自己的样式表会压过 QApplication::setStyleSheet，服务主题色就会盖住这套固定配色。
    const bool styledByApplication =
        qApp && qApp->styleSheet().contains(QStringLiteral("QWidget#groupPageFanLX"));
    if (!styledByApplication)
        kit::applyAccent(this, service);
    applyServiceFanLX(service);

    // 切换服务后群命名空间变了（QQ 群 / 微信群各存各的），上一个服务选中的群号在这里
    // 可能根本不存在：清掉选择，让下面重新默认选中新服务的第一个群。
    const bool serviceChanged = service != lastServiceFanLX;
    if (serviceChanged) {
        lastServiceFanLX = service;
        groupListFanLX->clear();
        groupEditFanLX->clear();
    }

    // 群列表：只在内容变化时重建，避免每次刷新都把选中项清掉。
    const auto rows = controllerFanLX->groupRows(service);
    if (serviceChanged || groupListFanLX->count() != rows.size()) {
        groupListFanLX->clear();
        groupListFanLX->addItems(rows);
    } else {
        bool same = true;
        for (int index = 0; index < rows.size() && same; ++index)
            same = groupListFanLX->item(index)->text() == rows.at(index);
        if (!same) {
            const auto selected = groupIdFanLX();
            groupListFanLX->clear();
            groupListFanLX->addItems(rows);
            for (int index = 0; index < groupListFanLX->count(); ++index)
                if (idOfRowFanLX(groupListFanLX->item(index)->text()) == selected)
                    groupListFanLX->setCurrentRow(index);
        }
    }

    // 打开页面时默认选中第一个群，免得下面的成员 / 群特色是一片空白。
    if (groupIdFanLX().isEmpty() && !rows.isEmpty()) {
        groupListFanLX->setCurrentRow(0); // 触发 currentItemChanged → 回填群号并再次刷新
        return;
    }

    const auto group = groupIdFanLX();
    headerLabelFanLX->setText(QStringLiteral("%1 群 · %2 　共 %3 个群")
                                  .arg(service, controllerFanLX->currentAccountId())
                                  .arg(rows.size()));
    for (auto *label : scopeLabelsFanLX)
        label->setText(group.isEmpty() ? QStringLiteral("尚未选择群：请先回到首页选中或填写群号")
                                       : QStringLiteral("操作对象: 群 %1").arg(group));

    const auto policy = controllerFanLX->policyText(service, group);
    policyViewFanLX->setPlainText(policy);
    // 只提供"切换为另一种"的按钮：已经是 QQ 特色就不再显示"切换为 QQ"。
    const bool isQqPolicy = policy.contains(QStringLiteral("QQGroupPolicyFanLX"));
    toQqButtonFanLX->setVisible(!isQqPolicy);
    toWechatButtonFanLX->setVisible(isQqPolicy);

    memberListFanLX->clear();
    memberListFanLX->addItems(controllerFanLX->memberRows(service, group));

    pendingListFanLX->clear();
    pendingListFanLX->addItems(controllerFanLX->pendingRows(service, group));

    discussionListFanLX->clear();
    discussionListFanLX->addItems(controllerFanLX->discussionRows(service, group));
}

} // namespace fanlxGui

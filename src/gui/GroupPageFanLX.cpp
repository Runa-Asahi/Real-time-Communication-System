#include "gui/GroupPageFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/PageKitFanLX.h"
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
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
    registry.push_back(label);
    return label;
}

} // namespace

GroupPageFanLX::GroupPageFanLX(DomainControllerFanLX *controller, QWidget *parent)
    : QWidget(parent), controllerFanLX(controller) {
    stackFanLX = new QStackedWidget(this);
    kit::setHomePage(stackFanLX, buildHomeFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("加入与待处理"), buildJoinBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("成员管理"), buildMemberBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("子群（讨论组）"), buildDiscussionBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("群设置"), buildSettingsBodyFanLX());

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->addWidget(stackFanLX);

    connect(controllerFanLX, &DomainControllerFanLX::dataChangedFanLX, this, &GroupPageFanLX::refreshFanLX);
    connect(controllerFanLX, &DomainControllerFanLX::identityChangedFanLX, this,
            &GroupPageFanLX::refreshFanLX);
    refreshFanLX();
}

QWidget *GroupPageFanLX::buildHomeFanLX() {
    auto *home = new QWidget();
    auto *layout = new QVBoxLayout(home);

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

    groupListFanLX = new QListWidget(contentFanLX);
    groupListFanLX->setMinimumHeight(120);
    groupEditFanLX = new QLineEdit(contentFanLX);
    groupEditFanLX->setPlaceholderText(QStringLiteral("群号（选中列表中的群自动填入，也可手输）"));
    memberListFanLX = new QListWidget(contentFanLX);
    memberListFanLX->setMinimumHeight(110);

    joinNavFanLX = kit::navButton(QStringLiteral("加入与待处理  ›"), contentFanLX);
    auto *memberNav = kit::navButton(QStringLiteral("成员管理  ›"), contentFanLX);
    discussionNavFanLX = kit::navButton(QStringLiteral("子群  ›"), contentFanLX);
    auto *settingsNav = kit::navButton(QStringLiteral("群设置  ›"), contentFanLX);
    auto *grid = new QGridLayout();
    grid->addWidget(joinNavFanLX, 0, 0);
    grid->addWidget(memberNav, 0, 1);
    grid->addWidget(discussionNavFanLX, 1, 0);
    grid->addWidget(settingsNav, 1, 1);

    content->addWidget(groupListFanLX, 1);
    content->addWidget(groupEditFanLX);
    content->addWidget(new QLabel(QStringLiteral("群成员"), contentFanLX));
    content->addWidget(memberListFanLX, 1);
    content->addLayout(grid);
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
    layout->addWidget(makeScopeLabelFanLX(body, scopeLabelsFanLX));

    // ① 邀请他人入群：QQ 与微信都有，但流程不同（提示文字随服务变化，见 applyServiceFanLX）。
    inviteBoxFanLX = new QGroupBox(QStringLiteral("邀请他人入群"), body);
    inviteTargetEditFanLX = new QLineEdit(inviteBoxFanLX);
    inviteTargetEditFanLX->setPlaceholderText(QStringLiteral("被邀请人的账号"));
    inviteButtonFanLX = kit::primaryButton(QStringLiteral("邀请入群"), inviteBoxFanLX);
    inviteHintFanLX = kit::hintLabel(inviteBoxFanLX);
    auto *inviteRow = new QHBoxLayout();
    inviteRow->addWidget(inviteTargetEditFanLX, 1);
    inviteRow->addWidget(inviteButtonFanLX);
    auto *inviteLayout = new QVBoxLayout(inviteBoxFanLX);
    inviteLayout->addLayout(inviteRow);
    inviteLayout->addWidget(inviteHintFanLX);

    // ② QQ：申请 - 审批
    auto *qqBox = new QGroupBox(QStringLiteral("申请与审批（QQ）"), body);
    qqJoinBoxFanLX = qqBox;
    applyButtonFanLX = kit::navButton(QStringLiteral("申请加入群"), qqBox);
    applyButtonFanLX->setMinimumHeight(34);
    targetAccountEditFanLX = new QLineEdit(qqBox);
    targetAccountEditFanLX->setPlaceholderText(QStringLiteral("申请人账号（群主 / 管理员审批时填写）"));
    approveButtonFanLX = kit::primaryButton(QStringLiteral("批准入群申请"), qqBox);
    rejectApplyButtonFanLX = kit::dangerButton(QStringLiteral("拒绝入群申请"), qqBox);
    auto *approveRow = new QHBoxLayout();
    approveRow->addWidget(targetAccountEditFanLX, 1);
    approveRow->addWidget(approveButtonFanLX);
    approveRow->addWidget(rejectApplyButtonFanLX);
    auto *qqLayout = new QVBoxLayout(qqBox);
    qqLayout->addWidget(applyButtonFanLX);
    qqLayout->addLayout(approveRow);

    // ③ 微信：本人收到的推荐要由本人确认
    auto *wechatBox = new QGroupBox(QStringLiteral("收到的入群推荐（微信）"), body);
    wechatJoinBoxFanLX = wechatBox;
    confirmInviteButtonFanLX = kit::primaryButton(QStringLiteral("确认本人的入群推荐"), wechatBox);
    rejectInviteButtonFanLX = kit::dangerButton(QStringLiteral("拒绝本人的入群推荐"), wechatBox);
    auto *wechatRow = new QHBoxLayout(wechatBox);
    wechatRow->addWidget(confirmInviteButtonFanLX);
    wechatRow->addWidget(rejectInviteButtonFanLX);
    wechatRow->addStretch(1);

    leaveButtonFanLX = kit::dangerButton(QStringLiteral("退出该群"), body);
    pendingListFanLX = new QListWidget(body);
    pendingListFanLX->setMinimumHeight(110);

    layout->addWidget(inviteBoxFanLX);
    layout->addWidget(qqJoinBoxFanLX);
    layout->addWidget(wechatJoinBoxFanLX);
    layout->addWidget(new QLabel(QStringLiteral("待审批 / 待确认"), body));
    layout->addWidget(pendingListFanLX, 1);
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
    layout->addWidget(makeScopeLabelFanLX(body, scopeLabelsFanLX));

    memberTargetEditFanLX = new QLineEdit(body);
    memberTargetEditFanLX->setPlaceholderText(QStringLiteral("目标成员账号"));
    kickButtonFanLX = kit::dangerButton(QStringLiteral("挨 T（移出成员）"), body);
    transferButtonFanLX = kit::navButton(QStringLiteral("转让群主"), body);
    transferButtonFanLX->setMinimumHeight(34);

    // 管理员制度只有 QQ 群有：微信群只有群主一个特权账号。
    adminBoxFanLX = new QWidget(body);
    assignAdminButtonFanLX = kit::navButton(QStringLiteral("任命管理员"), adminBoxFanLX);
    revokeAdminButtonFanLX = kit::navButton(QStringLiteral("撤销管理员"), adminBoxFanLX);
    assignAdminButtonFanLX->setMinimumHeight(34);
    revokeAdminButtonFanLX->setMinimumHeight(34);
    auto *adminRow = new QHBoxLayout(adminBoxFanLX);
    adminRow->setContentsMargins(0, 0, 0, 0);
    adminRow->addWidget(assignAdminButtonFanLX);
    adminRow->addWidget(revokeAdminButtonFanLX);
    adminRow->addStretch(1);

    auto *row = new QHBoxLayout();
    row->addWidget(kickButtonFanLX);
    row->addWidget(transferButtonFanLX);
    row->addStretch(1);

    layout->addWidget(memberTargetEditFanLX);
    layout->addLayout(row);
    layout->addWidget(adminBoxFanLX);
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
    layout->addWidget(makeScopeLabelFanLX(body, scopeLabelsFanLX));

    discussionIdEditFanLX = new QLineEdit(body);
    discussionIdEditFanLX->setPlaceholderText(QStringLiteral("子群编号，如 D1"));
    discussionTargetEditFanLX = new QLineEdit(body);
    discussionTargetEditFanLX->setPlaceholderText(QStringLiteral("要加入子群的成员账号"));
    createDiscussionButtonFanLX = kit::primaryButton(QStringLiteral("创建子群/讨论组"), body);
    joinDiscussionButtonFanLX = kit::navButton(QStringLiteral("把成员加入子群"), body);
    joinDiscussionButtonFanLX->setMinimumHeight(34);
    discussionListFanLX = new QListWidget(body);
    discussionListFanLX->setMinimumHeight(110);

    auto *row = new QHBoxLayout();
    row->addWidget(createDiscussionButtonFanLX);
    row->addWidget(joinDiscussionButtonFanLX);
    row->addStretch(1);
    layout->addWidget(discussionIdEditFanLX);
    layout->addWidget(discussionTargetEditFanLX);
    layout->addLayout(row);
    layout->addWidget(discussionListFanLX, 1);

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
    layout->addWidget(makeScopeLabelFanLX(body, scopeLabelsFanLX));

    policyViewFanLX = new QPlainTextEdit(body);
    policyViewFanLX->setReadOnly(true);
    policyViewFanLX->setMinimumHeight(140);
    auto *policyBox = new QGroupBox(QStringLiteral("群特色"), body);
    auto *policyLayout = new QVBoxLayout(policyBox);
    policyLayout->addWidget(policyViewFanLX);
    // 动态变换：只替换策略对象，成员、加入序号与子群记录原地保留；
    // 只显示"变成另一种"的那个按钮（见 refreshFanLX）。
    toQqButtonFanLX = kit::navButton(QStringLiteral("切换为 QQ 群管理特色"), policyBox);
    toWechatButtonFanLX = kit::navButton(QStringLiteral("切换为微信群管理特色"), policyBox);
    toQqButtonFanLX->setMinimumHeight(34);
    toWechatButtonFanLX->setMinimumHeight(34);
    auto *switchRow = new QHBoxLayout();
    switchRow->addWidget(toQqButtonFanLX);
    switchRow->addWidget(toWechatButtonFanLX);
    switchRow->addStretch(1);
    policyLayout->addLayout(switchRow);

    groupNameEditFanLX = new QLineEdit(body);
    groupNameEditFanLX->setPlaceholderText(QStringLiteral("群名"));
    renameButtonFanLX = kit::primaryButton(QStringLiteral("群改名"), body);
    createGroupButtonFanLX = kit::navButton(QStringLiteral("创建群"), body);
    createGroupButtonFanLX->setToolTip(QStringLiteral("使用首页填写的群号创建新群"));
    createGroupButtonFanLX->setMinimumHeight(34);
    dissolveButtonFanLX = kit::dangerButton(QStringLiteral("解散该群"), body);
    auto *nameRow = new QHBoxLayout();
    nameRow->addWidget(groupNameEditFanLX, 1);
    nameRow->addWidget(renameButtonFanLX);
    auto *actionRow = new QHBoxLayout();
    actionRow->addWidget(createGroupButtonFanLX);
    actionRow->addWidget(dissolveButtonFanLX);
    actionRow->addStretch(1);

    layout->addWidget(policyBox);
    layout->addLayout(nameRow);
    layout->addLayout(actionRow);
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
    qqJoinBoxFanLX->setVisible(qq);
    wechatJoinBoxFanLX->setVisible(wechat);
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
        if (groupListFanLX->count() == 0)
            groupListFanLX->addItem(QStringLiteral("（%1 上没有本人参与的群）").arg(service));
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
                                       : QStringLiteral("操作对象：群 %1").arg(group));

    const auto policy = controllerFanLX->policyText(service, group);
    policyViewFanLX->setPlainText(policy);
    // 只提供"切换为另一种"的按钮：已经是 QQ 特色就不再显示"切换为 QQ"。
    const bool isQqPolicy = policy.contains(QStringLiteral("QQGroupPolicyFanLX"));
    toQqButtonFanLX->setVisible(!isQqPolicy);
    toWechatButtonFanLX->setVisible(isQqPolicy);

    memberListFanLX->clear();
    memberListFanLX->addItems(controllerFanLX->memberRows(service, group));
    if (memberListFanLX->count() == 0)
        memberListFanLX->addItem(QStringLiteral("（无成员数据：该群不存在或无权查看）"));

    pendingListFanLX->clear();
    pendingListFanLX->addItems(controllerFanLX->pendingRows(service, group));
    if (pendingListFanLX->count() == 0)
        pendingListFanLX->addItem(QStringLiteral("（没有待处理记录）"));

    discussionListFanLX->clear();
    discussionListFanLX->addItems(controllerFanLX->discussionRows(service, group));
    if (discussionListFanLX->count() == 0)
        discussionListFanLX->addItem(QStringLiteral("（没有子群）"));
}

} // namespace fanlxGui

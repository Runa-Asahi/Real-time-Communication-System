#include "gui/AccountPageFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/PageKitFanLX.h"
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QColor>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>

#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fanlxGui {

namespace {

// 服务下拉框的固定项：微X 是上层登录身份（不可"开通"），因此开通/确认/绑定都只列三种下级服务。
QStringList platformNamesFanLX() {
    return {QStringLiteral("QQ"), QStringLiteral("微信"), QStringLiteral("微博")};
}

QComboBox *makePlatformComboFanLX(QWidget *parent) {
    auto *combo = new QComboBox(parent);
    combo->addItems(platformNamesFanLX());
    return combo;
}

// 当前服务下拉框：本人已建号的身份（含微X），用于切换"以谁的身份操作"。
QStringList identityNamesFanLX(DomainControllerFanLX *controller) {
    QStringList names = controller->ownedServices();
    if (names.isEmpty())
        names << QStringLiteral("微X");
    return names;
}

} // namespace

AccountPageFanLX::AccountPageFanLX(DomainControllerFanLX *controller, QWidget *parent)
    : QWidget(parent), controllerFanLX(controller) {
    stackFanLX = new QStackedWidget(this);
    // 先放首页（下标 0），再依次追加四个子页面；首页入口按钮在 buildHomeFanLX 里按下标跳转。
    kit::setHomePage(stackFanLX, buildHomeFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("开通管理"), buildServiceBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("登录管理"), buildLoginBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("绑定管理"), buildBindingBodyFanLX());
    kit::addSubPage(stackFanLX, QStringLiteral("资料与密码"), buildMaintenanceBodyFanLX());

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->addWidget(stackFanLX);

    // 身份变化与任何写操作成功后都重读视图：界面显示的一定是内存里的最新状态。
    connect(controllerFanLX, &DomainControllerFanLX::dataChangedFanLX, this,
            &AccountPageFanLX::refreshFanLX);
    connect(controllerFanLX, &DomainControllerFanLX::identityChangedFanLX, this,
            &AccountPageFanLX::refreshFanLX);
    refreshFanLX();
}

QWidget *AccountPageFanLX::buildHomeFanLX() {
    auto *home = new QWidget();
    auto *layout = new QVBoxLayout(home);

    // 当前身份卡片：一行说明 + 切换下拉框（选中即切换，不需要额外按钮）。
    auto *identityCard = kit::card(home);
    identityLabelFanLX = new QLabel(identityCard);
    identityLabelFanLX->setProperty("role", QStringLiteral("title"));
    platformComboFanLX = new QComboBox(identityCard);
    platformComboFanLX->setMinimumWidth(130);
    auto *cardLayout = new QHBoxLayout(identityCard);
    cardLayout->addWidget(identityLabelFanLX, 1);
    cardLayout->addWidget(new QLabel(QStringLiteral("切换服务"), identityCard));
    cardLayout->addWidget(platformComboFanLX);
    // 选中即切换。切换被拒（比如该服务还没登录）时：下拉框必须回到真正的当前服务，
    // 并明确弹出原因——不能让界面停在用户点的那一项上、却没有任何说法。
    connect(platformComboFanLX, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        const auto service = platformComboFanLX->itemData(index).toString();
        if (service.isEmpty() || controllerFanLX->setCurrentService(service))
            return;
        refreshFanLX();
        QMessageBox::warning(this, QStringLiteral("无法切换服务"), controllerFanLX->lastMessage());
    });
    layout->addWidget(identityCard);

    // 登录态提示条：当前服务未登录 / 已全部注销 时显示，并给出下一步。
    bannerFanLX = new QFrame(home);
    bannerFanLX->setObjectName(QStringLiteral("bannerFanLX"));
    bannerLabelFanLX = new QLabel(bannerFanLX);
    bannerLabelFanLX->setWordWrap(true);
    reloginButtonFanLX = kit::primaryButton(QStringLiteral("重新登录微X"), bannerFanLX);
    auto *bannerLayout = new QHBoxLayout(bannerFanLX);
    bannerLayout->addWidget(bannerLabelFanLX, 1);
    bannerLayout->addWidget(reloginButtonFanLX);
    layout->addWidget(bannerFanLX);
    connect(reloginButtonFanLX, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto password = QInputDialog::getText(
            this, QStringLiteral("重新登录"),
            QStringLiteral("微X号 %1 的登录密码：").arg(controllerFanLX->weixAccountId()),
            QLineEdit::Password, QString(), &ok);
        if (!ok)
            return;
        if (!controllerFanLX->relogin(password))
            QMessageBox::warning(this, QStringLiteral("重新登录失败"), controllerFanLX->lastMessage());
    });

    layout->addWidget(buildProfileCardFanLX(home));

    // 四个入口：2 x 2 网格。
    auto *grid = new QGridLayout();
    const QStringList titles{QStringLiteral("开通管理  ›"), QStringLiteral("登录管理  ›"),
                             QStringLiteral("绑定管理  ›"), QStringLiteral("资料与密码  ›")};
    for (int index = 0; index < titles.size(); ++index) {
        auto *button = kit::navButton(titles.at(index), home);
        grid->addWidget(button, index / 2, index % 2);
        connect(button, &QPushButton::clicked, this, [this, index] { stackFanLX->setCurrentIndex(index + 1); });
    }
    layout->addLayout(grid);
    layout->addStretch(1);
    return home;
}

QWidget *AccountPageFanLX::buildProfileCardFanLX(QWidget *parent) {
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("profileCardFanLX"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 14, 20, 16);
    layout->setSpacing(10);

    const auto labelOf = [frame](const QString &role, const QString &text = QString()) {
        auto *label = new QLabel(text, frame);
        label->setProperty("role", role);
        return label;
    };
    const auto divider = [frame] {
        auto *line = new QFrame(frame);
        line->setProperty("role", QStringLiteral("divider"));
        line->setFixedHeight(1);
        return line;
    };

    layout->addWidget(labelOf(QStringLiteral("section"), QStringLiteral("用户基本信息")));

    // 头像（昵称首字）+ 昵称 + 一行副标题。
    avatarLabelFanLX = labelOf(QStringLiteral("avatar"));
    avatarLabelFanLX->setFixedSize(56, 56);
    avatarLabelFanLX->setAlignment(Qt::AlignCenter);
    nameLabelFanLX = labelOf(QStringLiteral("name"));
    subtitleLabelFanLX = labelOf(QStringLiteral("caption"));
    auto *titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(2);
    titleColumn->addStretch(1);
    titleColumn->addWidget(nameLabelFanLX);
    titleColumn->addWidget(subtitleLabelFanLX);
    titleColumn->addStretch(1);
    auto *header = new QHBoxLayout();
    header->setSpacing(14);
    header->addWidget(avatarLabelFanLX);
    header->addLayout(titleColumn, 1);
    layout->addLayout(header);
    layout->addWidget(divider());

    // 三项统计：大字数值 + 小字说明。
    const auto statTile = [&](QLabel *&valueLabel, const QString &caption) {
        valueLabel = labelOf(QStringLiteral("stat"));
        valueLabel->setAlignment(Qt::AlignCenter);
        auto *captionLabel = labelOf(QStringLiteral("caption"), caption);
        captionLabel->setAlignment(Qt::AlignCenter);
        auto *column = new QVBoxLayout();
        column->setSpacing(0);
        column->addWidget(valueLabel);
        column->addWidget(captionLabel);
        return column;
    };
    auto *stats = new QHBoxLayout();
    stats->addLayout(statTile(tAgeValueFanLX, QStringLiteral("T龄（年）")), 1);
    stats->addLayout(statTile(friendValueFanLX, QStringLiteral("好友")), 1);
    stats->addLayout(statTile(groupValueFanLX, QStringLiteral("群")), 1);
    layout->addLayout(stats);
    layout->addWidget(divider());

    // 详情：两列，每项是"灰色小字标题 + 正常大小的值"。
    const auto detail = [&](QLabel *&valueLabel, const QString &caption) {
        valueLabel = labelOf(QStringLiteral("value"));
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *column = new QVBoxLayout();
        column->setSpacing(1);
        column->addWidget(labelOf(QStringLiteral("caption"), caption));
        column->addWidget(valueLabel);
        return column;
    };
    auto *details = new QGridLayout();
    details->setHorizontalSpacing(24);
    details->setVerticalSpacing(10);
    details->addLayout(detail(birthValueFanLX, QStringLiteral("出生时间")), 0, 0);
    details->addLayout(detail(applyValueFanLX, QStringLiteral("号码申请时间")), 0, 1);
    details->addLayout(detail(locationValueFanLX, QStringLiteral("所在地")), 1, 0);
    details->addLayout(detail(userIdValueFanLX, QStringLiteral("号码归属（后台用户 ID）")), 1, 1);
    details->setColumnStretch(0, 1);
    details->setColumnStretch(1, 1);
    layout->addLayout(details);
    return frame;
}

QWidget *AccountPageFanLX::buildServiceBodyFanLX() {
    auto *body = new QWidget();
    serviceTableFanLX = new QTableWidget(0, 6, body);
    serviceTableFanLX->setHorizontalHeaderLabels({QStringLiteral("服务"), QStringLiteral("号码"),
                                                  QStringLiteral("共用号"), QStringLiteral("已绑定"),
                                                  QStringLiteral("已开通"), QStringLiteral("登录中")});
    serviceTableFanLX->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    serviceTableFanLX->setEditTriggers(QAbstractItemView::NoEditTriggers);
    serviceTableFanLX->setSelectionBehavior(QAbstractItemView::SelectRows);
    serviceTableFanLX->verticalHeader()->hide();
    serviceTableFanLX->setMinimumHeight(170);

    subscribeComboFanLX = makePlatformComboFanLX(body);
    subscribeNumberEditFanLX = new QLineEdit(body);
    subscribeButtonFanLX = kit::primaryButton(QStringLiteral("开通所选服务"), body);
    auto *row = new QHBoxLayout();
    row->addWidget(subscribeComboFanLX);
    row->addWidget(subscribeNumberEditFanLX, 1);
    row->addWidget(subscribeButtonFanLX);

    auto *layout = new QVBoxLayout(body);
    layout->addWidget(serviceTableFanLX);
    layout->addLayout(row);
    layout->addStretch(1);

    // 号码留空时给出建议值：QQ / 微博 沿用微X号（课程约定两者共享号码），微信加 wx_ 前缀。
    const auto suggestedNumber = [this] {
        const auto weix = controllerFanLX->weixAccountId();
        if (subscribeComboFanLX->currentText() == QStringLiteral("微信"))
            return QStringLiteral("wx_") + weix;
        return weix;
    };
    connect(subscribeComboFanLX, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, suggestedNumber] { subscribeNumberEditFanLX->setPlaceholderText(suggestedNumber()); });
    subscribeNumberEditFanLX->setPlaceholderText(suggestedNumber());
    connect(subscribeButtonFanLX, &QPushButton::clicked, this, [this, suggestedNumber] {
        const auto typed = subscribeNumberEditFanLX->text().trimmed();
        controllerFanLX->activateService(subscribeComboFanLX->currentText(),
                                         typed.isEmpty() ? suggestedNumber() : typed);
    });
    return body;
}

QWidget *AccountPageFanLX::buildLoginBodyFanLX() {
    auto *body = new QWidget();
    confirmServiceComboFanLX = makePlatformComboFanLX(body);
    confirmLoginButtonFanLX = kit::primaryButton(QStringLiteral("逐项确认登录"), body);
    // 一键联动：登微X 时已默认联动确认，所以只在"注销过 / 新开通了服务"后有事可做，
    // refreshFanLX 会据此启用或置灰它。
    linkAllButtonFanLX = kit::navButton(QStringLiteral("一键联动登录（本人已开通的全部服务）"), body);
    logoutButtonFanLX = kit::dangerButton(QStringLiteral("注销当前服务"), body);
    logoutAllButtonFanLX = kit::dangerButton(QStringLiteral("全部注销"), body);

    auto *row = new QHBoxLayout();
    row->addWidget(confirmServiceComboFanLX);
    row->addWidget(confirmLoginButtonFanLX);
    row->addStretch(1);
    auto *logoutRow = new QHBoxLayout();
    logoutRow->addWidget(logoutButtonFanLX);
    logoutRow->addWidget(logoutAllButtonFanLX);
    logoutRow->addStretch(1);

    auto *layout = new QVBoxLayout(body);
    layout->addWidget(linkAllButtonFanLX);
    layout->addLayout(row);
    layout->addSpacing(12);
    layout->addLayout(logoutRow);
    layout->addStretch(1);

    // 课设要求 5：登微X 后，本人已开通的其它服务简单确认即视为自动登录。
    connect(linkAllButtonFanLX, &QPushButton::clicked, this,
            [this] { controllerFanLX->confirmAllServices(); });
    connect(confirmLoginButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto service = confirmServiceComboFanLX->currentText();
        controllerFanLX->confirmLoginService(service, controllerFanLX->accountIdOf(service));
    });
    connect(logoutButtonFanLX, &QPushButton::clicked, this,
            [this] { controllerFanLX->logoutCurrent(false); });
    connect(logoutAllButtonFanLX, &QPushButton::clicked, this,
            [this] { controllerFanLX->logoutCurrent(true); });
    return body;
}

QWidget *AccountPageFanLX::buildBindingBodyFanLX() {
    auto *body = new QWidget();
    bindingListFanLX = new QListWidget(body);
    bindingListFanLX->setMinimumHeight(100);
    bindServiceComboFanLX = makePlatformComboFanLX(body);
    bindAccountEditFanLX = new QLineEdit(body);
    bindAccountEditFanLX->setPlaceholderText(QStringLiteral("对方号码（留空 = 本人该服务的号）"));
    bindButtonFanLX = kit::primaryButton(QStringLiteral("绑定"), body);
    unbindButtonFanLX = kit::dangerButton(QStringLiteral("解绑"), body);

    auto *row = new QHBoxLayout();
    row->addWidget(bindServiceComboFanLX);
    row->addWidget(bindAccountEditFanLX, 1);
    row->addWidget(bindButtonFanLX);
    row->addWidget(unbindButtonFanLX);

    auto *layout = new QVBoxLayout(body);
    layout->addWidget(bindingListFanLX);
    layout->addLayout(row);
    layout->addStretch(1);

    const auto targetAccount = [this] {
        const auto service = bindServiceComboFanLX->currentText();
        return bindAccountEditFanLX->text().trimmed().isEmpty()
                   ? controllerFanLX->accountIdOf(service)
                   : bindAccountEditFanLX->text().trimmed();
    };
    connect(bindButtonFanLX, &QPushButton::clicked, this, [this, targetAccount] {
        controllerFanLX->bindService(bindServiceComboFanLX->currentText(), targetAccount(), false);
    });
    connect(unbindButtonFanLX, &QPushButton::clicked, this, [this, targetAccount] {
        controllerFanLX->bindService(bindServiceComboFanLX->currentText(), targetAccount(), true);
    });
    return body;
}

QWidget *AccountPageFanLX::buildMaintenanceBodyFanLX() {
    auto *body = new QWidget();
    auto *layout = new QVBoxLayout(body);

    // 单项修改：两个字段各有自己的保存按钮，改一个不会动另一个。
    nicknameEditFanLX = new QLineEdit(body);
    nicknameEditFanLX->setPlaceholderText(QStringLiteral("新昵称"));
    saveNicknameButtonFanLX = kit::primaryButton(QStringLiteral("保存昵称"), body);
    auto *nicknameRow = new QHBoxLayout();
    nicknameRow->addWidget(nicknameEditFanLX, 1);
    nicknameRow->addWidget(saveNicknameButtonFanLX);

    locationEditFanLX = new QLineEdit(body);
    locationEditFanLX->setPlaceholderText(QStringLiteral("新所在地"));
    saveLocationButtonFanLX = kit::primaryButton(QStringLiteral("保存所在地"), body);
    auto *locationRow = new QHBoxLayout();
    locationRow->addWidget(locationEditFanLX, 1);
    locationRow->addWidget(saveLocationButtonFanLX);

    oldPasswordEditFanLX = new QLineEdit(body);
    oldPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    oldPasswordEditFanLX->setPlaceholderText(QStringLiteral("当前密码"));
    newPasswordEditFanLX = new QLineEdit(body);
    newPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    newPasswordEditFanLX->setPlaceholderText(QStringLiteral("新密码"));
    repeatPasswordEditFanLX = new QLineEdit(body);
    repeatPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    repeatPasswordEditFanLX->setPlaceholderText(QStringLiteral("再次输入新密码"));
    changePasswordButtonFanLX = kit::primaryButton(QStringLiteral("修改密码"), body);

    hintLabelFanLX = kit::hintLabel(body);

    auto *profileBox = new QGroupBox(QStringLiteral("资料（可单项修改）"), body);
    auto *profileLayout = new QVBoxLayout(profileBox);
    profileLayout->addLayout(nicknameRow);
    profileLayout->addLayout(locationRow);
    auto *passwordBox = new QGroupBox(QStringLiteral("密码"), body);
    auto *passwordLayout = new QVBoxLayout(passwordBox);
    passwordLayout->addWidget(oldPasswordEditFanLX);
    passwordLayout->addWidget(newPasswordEditFanLX);
    passwordLayout->addWidget(repeatPasswordEditFanLX);
    passwordLayout->addWidget(changePasswordButtonFanLX, 0, Qt::AlignRight);

    layout->addWidget(profileBox);
    layout->addWidget(passwordBox);
    layout->addWidget(hintLabelFanLX);
    layout->addStretch(1);

    connect(saveNicknameButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto nickname = nicknameEditFanLX->text().trimmed();
        if (nickname.isEmpty()) {
            hintLabelFanLX->setText(QStringLiteral("请先输入新昵称"));
            return;
        }
        // 只提交昵称：所在地传空串 = "这一项不改"。
        if (controllerFanLX->updateProfile(nickname, QString()))
            nicknameEditFanLX->clear();
    });
    connect(saveLocationButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto location = locationEditFanLX->text().trimmed();
        if (location.isEmpty()) {
            hintLabelFanLX->setText(QStringLiteral("请先输入新所在地"));
            return;
        }
        if (controllerFanLX->updateProfile(QString(), location))
            locationEditFanLX->clear();
    });
    connect(changePasswordButtonFanLX, &QPushButton::clicked, this, [this] {
        // 两次输入必须一致：否则用户会用一个自己以为不对的密码锁死账号。
        if (newPasswordEditFanLX->text() != repeatPasswordEditFanLX->text()) {
            hintLabelFanLX->setText(QStringLiteral("两次输入的新密码不一致，未提交"));
            return;
        }
        if (controllerFanLX->changePassword(oldPasswordEditFanLX->text(), newPasswordEditFanLX->text())) {
            oldPasswordEditFanLX->clear();
            newPasswordEditFanLX->clear();
            repeatPasswordEditFanLX->clear();
        }
    });
    return body;
}

void AccountPageFanLX::refreshFanLX() {
    if (!controllerFanLX->opened())
        return;

    kit::applyAccent(this, controllerFanLX->currentService());

    // 当前身份
    identityLabelFanLX->setText(QStringLiteral("%1 · %2")
                                    .arg(controllerFanLX->currentService(), controllerFanLX->currentAccountId()));
    identityLabelFanLX->setToolTip(QStringLiteral("微X号 %1 ｜ 已提交领域事务 %2 次")
                                       .arg(controllerFanLX->weixAccountId())
                                       .arg(controllerFanLX->commitCount()));
    // 切换下拉框：未登录的服务标注"（未登录）"并置灰，选它会被拒绝并给出原因。
    const auto current = controllerFanLX->currentService();
    const auto identities = identityNamesFanLX(controllerFanLX);
    {
        QSignalBlocker blocker(platformComboFanLX);
        platformComboFanLX->clear();
        for (const auto &name : identities) {
            const bool loggedIn = controllerFanLX->isServiceLoggedIn(name);
            platformComboFanLX->addItem(loggedIn ? name : name + QStringLiteral("（未登录）"), name);
            if (!loggedIn)
                platformComboFanLX->setItemData(platformComboFanLX->count() - 1, QColor(QStringLiteral("#9aa0a6")),
                                                Qt::ForegroundRole);
        }
        const int currentIndex = platformComboFanLX->findData(current);
        if (currentIndex >= 0)
            platformComboFanLX->setCurrentIndex(currentIndex);
    }

    const bool anyLogin = controllerFanLX->hasAnyLogin();
    const bool currentLoggedIn = controllerFanLX->currentServiceLoggedIn();
    bannerFanLX->setVisible(!currentLoggedIn);
    reloginButtonFanLX->setVisible(!anyLogin);
    bannerLabelFanLX->setText(
        !anyLogin ? QStringLiteral("已全部注销：当前没有任何登录中的服务。点右侧「重新登录微X」恢复。")
                  : QStringLiteral("%1 未登录：到「登录管理」里确认登录，或在右上角切换到已登录的服务。").arg(current));

    // 一键联动登录：只在"本人已开通但未登录"的服务非空时可用（否则点了也没有任何变化）。
    linkAllButtonFanLX->setEnabled(controllerFanLX->hasUnlinkedServices());

    // 用户基本信息卡片
    const auto profile = controllerFanLX->profileView();
    const auto orDash = [](const QString &text) { return text.trimmed().isEmpty() ? QStringLiteral("—") : text; };
    if (!profile.valid) {
        nameLabelFanLX->setText(QStringLiteral("未登录"));
        subtitleLabelFanLX->clear();
    } else {
        const auto initial = (profile.nickname.isEmpty() ? profile.weixId : profile.nickname).left(1);
        avatarLabelFanLX->setText(initial);
        nameLabelFanLX->setText(orDash(profile.nickname));
        subtitleLabelFanLX->setText(QStringLiteral("微X号 %1 · 当前 %2 %3")
                                        .arg(profile.weixId, profile.service,
                                             profile.accountId.isEmpty() ? QStringLiteral("（未开通）")
                                                                         : profile.accountId));
        tAgeValueFanLX->setText(QString::number(profile.tAgeYears));
        friendValueFanLX->setText(QString::number(profile.friendCount));
        groupValueFanLX->setText(QString::number(profile.groupCount));
        birthValueFanLX->setText(orDash(profile.birthDate));
        applyValueFanLX->setText(orDash(profile.applyDate));
        locationValueFanLX->setText(orDash(profile.location));
        userIdValueFanLX->setText(orDash(profile.userId));
    }

    // 开通管理：四个身份全部列出（未开通的下级服务显示"未开通"），一眼看出还差哪一项。
    const auto rows = controllerFanLX->serviceRows();
    serviceTableFanLX->setRowCount(rows.size());
    for (int index = 0; index < rows.size(); ++index) {
        const auto &row = rows.at(index);
        const auto textOf = [](bool flag) { return flag ? QStringLiteral("是") : QStringLiteral("否"); };
        const QStringList cells{
            row.service + (row.current ? QStringLiteral("（当前）") : QString()) +
                (row.upperLayer ? QStringLiteral("（上层）") : QString()),
            row.exists ? row.accountId : QStringLiteral("（未开通）"),
            row.sharedId ? QStringLiteral("是（QQ 与微博共享号）") : QStringLiteral("否（独立号）"),
            textOf(row.bound), textOf(row.subscribed), textOf(row.loggedIn)};
        for (int column = 0; column < cells.size(); ++column)
            serviceTableFanLX->setItem(index, column, new QTableWidgetItem(cells.at(column)));
    }
    if (serviceTableFanLX->rowCount() > 0)
        serviceTableFanLX->resizeRowsToContents();

    // 绑定管理
    bindingListFanLX->clear();
    bindingListFanLX->addItems(controllerFanLX->bindingRows());
    if (bindingListFanLX->count() == 0)
        bindingListFanLX->addItem(QStringLiteral("（还没有绑定关系）"));
}

} // namespace fanlxGui

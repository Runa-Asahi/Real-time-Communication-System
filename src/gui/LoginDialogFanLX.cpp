#include "gui/LoginDialogFanLX.h"
#include "gui/PageKitFanLX.h"
#include "gui/RegisterDialogFanLX.h"
#include "network/ClientDataPathFanLX.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <filesystem>

namespace fanlxGui {

namespace {

// 登录前还没有当前服务。整窗使用全局品牌色，不再单独用微X紫色。
const QString kLoginAccentFanLX = theme::brand;

// 链接样式的扁平按钮：注册 / 服务器设置。
QPushButton *linkButtonFanLX(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setFlat(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("role", QStringLiteral("back"));
    return button;
}

} // namespace

LoginDialogFanLX::LoginDialogFanLX(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("登录"));
    // 登录窗宽度固定，高度只跟着内容走。可拉伸粗边框在 150% 缩放下会让 Windows
    // 把客户区撑高约一行，从而触发 setGeometry 警告。细对话框边框没有这块隐形边距。
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setFixedWidth(400);
    setStyleSheet(kit::pageStyle(kLoginAccentFanLX) +
                  QStringLiteral("QDialog{background:#F6F7F9;}"
                                 "QCheckBox{color:#5B6472;}"
                                 "QLineEdit,QSpinBox,QComboBox{min-height:32px;}"));

    // ---- 顶部：标识 + 标题 ----
    auto *logo = new QLabel(QStringLiteral("樊"), this);
    logo->setProperty("role", QStringLiteral("avatar"));
    logo->setFixedSize(56, 56);
    logo->setAlignment(Qt::AlignCenter);
    auto *title = new QLabel(QStringLiteral("樊陆旭模拟即时通讯系统"), this);
    title->setProperty("role", QStringLiteral("name"));
    title->setAlignment(Qt::AlignCenter);
    auto *subtitle = new QLabel(QStringLiteral("使用微X号登录，QQ / 微信 / 微博 登录后自动联动"), this);
    subtitle->setProperty("role", QStringLiteral("caption"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);

    // ---- 账号与密码 ----
    accountEditFanLX = new QLineEdit(this);
    accountEditFanLX->setPlaceholderText(QStringLiteral("微X号（也可输入已开通服务的号码）"));
    passwordEditFanLX = new QLineEdit(this);
    passwordEditFanLX->setEchoMode(QLineEdit::Password);
    passwordEditFanLX->setPlaceholderText(QStringLiteral("密码"));

    // 课设要求 5 的"简单确认"落点：勾上就表示同意用这一次输入的微X凭据联动登录
    // 本人已开通的下级服务，因此不需要为每个服务再输一遍密码。
    confirmOthersCheckFanLX = new QCheckBox(QStringLiteral("登录后联动已开通的 QQ / 微信 / 微博"), this);
    confirmOthersCheckFanLX->setChecked(true);
    confirmOthersCheckFanLX->setToolTip(
        QStringLiteral("授权来源就是这一次微X登录；未开通的服务不会被自动开通。\n"
                       "不勾选则只登录微X，其它服务之后可在「账号与服务」里逐项确认。"));

    statusLabelFanLX = new QLabel(this);
    statusLabelFanLX->setWordWrap(true);
    statusLabelFanLX->setVisible(false);

    loginButtonFanLX = kit::primaryButton(QStringLiteral("登录"), this);
    loginButtonFanLX->setMinimumHeight(42);
    loginButtonFanLX->setDefault(true);

    registerButtonFanLX = linkButtonFanLX(QStringLiteral("注册微X账号"), this);
    registerButtonFanLX->setToolTip(
        QStringLiteral("注册只创建微X账号；QQ / 微信 / 微博 在登录后于「开通管理」里自主开通"));
    settingsToggleFanLX = linkButtonFanLX(QStringLiteral("服务器设置"), this);
    auto *links = new QHBoxLayout();
    links->addWidget(registerButtonFanLX);
    links->addStretch(1);
    links->addWidget(settingsToggleFanLX);

    // ---- 服务器与存储（默认收起：绝大多数时候用默认值）----
    hostEditFanLX = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    portSpinFanLX = new QSpinBox(this);
    portSpinFanLX->setRange(1, 65535);
    portSpinFanLX->setValue(9500);
    storageComboFanLX = new QComboBox(this);
    storageComboFanLX->addItem(QStringLiteral("SQLite 聊天库（推荐）"), true);
    storageComboFanLX->addItem(QStringLiteral("旧 JSON 存储（兼容）"), false);
    // 数据根只读展示：真实存储目录由"服务器身份 + 微X号"派生，用户不参与选择。
    dataRootEditFanLX = new QLineEdit(
        QDir::toNativeSeparators(QDir::current().filePath(QStringLiteral("data-im-gui"))), this);
    dataRootEditFanLX->setReadOnly(true);
    dataRootEditFanLX->setToolTip(
        QStringLiteral("数据根目录；账号子目录由登录的微X号自动派生，可用 --data-root 覆盖"));
    derivedPathLabelFanLX = kit::hintLabel(this);

    settingsFrameFanLX = new QFrame(this);
    settingsFrameFanLX->setObjectName(QStringLiteral("cardFanLX"));
    auto *form = new QFormLayout();
    form->addRow(QStringLiteral("服务器"), hostEditFanLX);
    form->addRow(QStringLiteral("端口"), portSpinFanLX);
    form->addRow(QStringLiteral("聊天存储"), storageComboFanLX);
    form->addRow(QStringLiteral("数据根目录"), dataRootEditFanLX);
    auto *settingsLayout = new QVBoxLayout(settingsFrameFanLX);
    settingsLayout->addLayout(form);
    settingsLayout->addWidget(derivedPathLabelFanLX);
    settingsFrameFanLX->setVisible(false);

    // ---- 装配 ----
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 20);
    layout->setSpacing(10);
    layout->addWidget(logo, 0, Qt::AlignHCenter);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(14);
    layout->addWidget(accountEditFanLX);
    layout->addWidget(passwordEditFanLX);
    layout->addWidget(confirmOthersCheckFanLX);
    layout->addWidget(statusLabelFanLX);
    layout->addWidget(loginButtonFanLX);
    layout->addLayout(links);
    layout->addWidget(settingsFrameFanLX);

    // 服务器/微X号任一变化都刷新派生预览：让用户在点"登录"之前就看到数据会落到哪个目录。
    connect(hostEditFanLX, &QLineEdit::textChanged, this, &LoginDialogFanLX::updateDerivedPathFanLX);
    connect(accountEditFanLX, &QLineEdit::textChanged, this, &LoginDialogFanLX::updateDerivedPathFanLX);
    connect(portSpinFanLX, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &LoginDialogFanLX::updateDerivedPathFanLX);
    connect(loginButtonFanLX, &QPushButton::clicked, this, &LoginDialogFanLX::submitFanLX);
    connect(passwordEditFanLX, &QLineEdit::returnPressed, this, &LoginDialogFanLX::submitFanLX);
    connect(accountEditFanLX, &QLineEdit::returnPressed, passwordEditFanLX,
            qOverload<>(&QWidget::setFocus));
    connect(registerButtonFanLX, &QPushButton::clicked, this,
            &LoginDialogFanLX::openRegisterDialogFanLX);
    connect(settingsToggleFanLX, &QPushButton::clicked, this,
            [this] { setSettingsVisibleFanLX(!settingsFrameFanLX->isVisible()); });
    updateDerivedPathFanLX();
    fitHeightToContentsFanLX();
    accountEditFanLX->setFocus();
}

void LoginDialogFanLX::fitHeightToContentsFanLX() {
    QLayout *box = layout();
    if (!box)
        return;
    ensurePolished();
    box->activate();
    // 换行标签的 sizeHint 按「尽量不换行」的宽度算，比固定 400 像素时实际需要的矮。
    // 必须用 minimumHeightForWidth，否则窗口先按矮的高度显示，再被系统拉高。
    int height = 0;
    if (box->hasHeightForWidth())
        height = box->minimumHeightForWidth(400);
    if (height <= 0)
        height = box->sizeHint().height();
    if (height > 0 && (minimumHeight() != height || maximumHeight() != height))
        setFixedHeight(height);
}

void LoginDialogFanLX::setSettingsVisibleFanLX(bool visible) {
    settingsFrameFanLX->setVisible(visible);
    settingsToggleFanLX->setText(visible ? QStringLiteral("收起服务器设置") : QStringLiteral("服务器设置"));
    fitHeightToContentsFanLX();
}

// 提交：先把按钮置为"正在登录"并让界面刷新一次，再进入（会阻塞数秒的）连接与认证，
// 避免用户看着一个没有反应的窗口反复点击。
void LoginDialogFanLX::submitFanLX() {
    if (!loginButtonFanLX->isEnabled())
        return;
    if (accountEditFanLX->text().trimmed().isEmpty() || passwordEditFanLX->text().isEmpty()) {
        showStatusFanLX(QStringLiteral("请输入微X号和密码"), false);
        return;
    }
    statusLabelFanLX->setVisible(false);
    loginButtonFanLX->setEnabled(false);
    loginButtonFanLX->setText(QStringLiteral("正在登录…"));
    fitHeightToContentsFanLX();
    QCoreApplication::processEvents();
    emit submittedFanLX();
}

void LoginDialogFanLX::showStatusFanLX(const QString &text, bool good) {
    statusLabelFanLX->setStyleSheet(good ? QStringLiteral("color:%1;").arg(theme::success)
                                         : QStringLiteral("color:%1;").arg(theme::danger));
    statusLabelFanLX->setText(text);
    statusLabelFanLX->setVisible(true);
    fitHeightToContentsFanLX();
}

// 注册入口：注册产出的是登录身份本身，因此它属于登录界面而不是登录之后的"账号与服务"。
void LoginDialogFanLX::openRegisterDialogFanLX() {
    RegisterDialogFanLX dialog(dataRootEditFanLX->text().trimmed(), hostEditFanLX->text().trimmed(),
                              static_cast<quint16>(portSpinFanLX->value()), this);
    if (dialog.exec() == QDialog::Accepted) {
        // 注册结果里带上"服务端登记"的结论：它决定能否立即用聊天（认证查的是服务端账号库）。
        const auto message =
            dialog.registeredOnServer()
                ? QStringLiteral("注册成功，已填入微X号，请输入密码登录")
                : QStringLiteral("本机注册成功，已填入微X号（服务端登记未成功：%1；登录时会自动重试）")
                      .arg(dialog.registrationDetail());
        prefillRegisteredAccount(dialog.registeredWeixId(), message);
    }
}

void LoginDialogFanLX::prefillRegisteredAccount(const QString &weixId, const QString &message) {
    if (!weixId.isEmpty()) {
        accountEditFanLX->setText(weixId);
        passwordEditFanLX->clear();
        passwordEditFanLX->setFocus();
    }
    showStatusFanLX(message, true);
    updateDerivedPathFanLX();
}

// 派生规则必须与运行时一致，否则界面预览会与真实落盘位置不符——
// 因此这里复用同一个 imClientPathFanLX::clientDirectoryFanLX，而不是在界面里重写一遍拼接。
void LoginDialogFanLX::updateDerivedPathFanLX() {
    const auto root = dataRootEditFanLX->text().trimmed();
    const auto host = hostEditFanLX->text().trimmed();
    const auto account = accountEditFanLX->text().trimmed();
    if (root.isEmpty() || host.isEmpty() || account.isEmpty()) {
        derivedPathLabelFanLX->setText(QStringLiteral("登录后数据目录：填写账号后显示"));
        return;
    }
    try {
        const auto derived = imClientPathFanLX::clientDirectoryFanLX(
            std::filesystem::path(root.toStdWString()), host.toStdString(),
            static_cast<unsigned short>(portSpinFanLX->value()), account.toStdString());
        derivedPathLabelFanLX->setText(QStringLiteral("登录后数据目录：") +
                                       QDir::toNativeSeparators(
                                           QString::fromStdWString(derived.wstring())));
    } catch (const std::exception &) {
        derivedPathLabelFanLX->setText(QStringLiteral("登录后数据目录：参数不完整"));
    }
    if (settingsFrameFanLX->isVisible())
        fitHeightToContentsFanLX();
}

void LoginDialogFanLX::prefill(const QString &host, const QString &port, const QString &account,
                              const QString &password, const QString &dataRoot, bool useChatDatabase) {
    if (!host.isEmpty()) hostEditFanLX->setText(host);
    bool ok = false;
    const auto parsed = port.toUShort(&ok);
    if (ok && parsed != 0) portSpinFanLX->setValue(parsed);
    if (!account.isEmpty()) accountEditFanLX->setText(account);
    if (!password.isEmpty()) passwordEditFanLX->setText(password);
    if (!dataRoot.isEmpty()) dataRootEditFanLX->setText(dataRoot);
    storageComboFanLX->setCurrentIndex(useChatDatabase ? 0 : 1);
    updateDerivedPathFanLX();
}

QString LoginDialogFanLX::host() const { return hostEditFanLX->text().trimmed(); }
quint16 LoginDialogFanLX::port() const { return static_cast<quint16>(portSpinFanLX->value()); }
QString LoginDialogFanLX::account() const { return accountEditFanLX->text().trimmed(); }
QString LoginDialogFanLX::password() const { return passwordEditFanLX->text(); }
QString LoginDialogFanLX::dataRoot() const { return dataRootEditFanLX->text().trimmed(); }
bool LoginDialogFanLX::useChatDatabase() const { return storageComboFanLX->currentData().toBool(); }
bool LoginDialogFanLX::confirmOthers() const { return confirmOthersCheckFanLX->isChecked(); }

void LoginDialogFanLX::showFailure(const QString &reason) {
    // 登录失败时窗口保持打开、输入内容保留，并明确提示"修正后可重试"——
    // 绝不带着一个未认证的身份进主界面，也不会静默丢弃用户输入。
    showStatusFanLX(QStringLiteral("登录失败：") + reason, false);
    loginButtonFanLX->setEnabled(true);
    loginButtonFanLX->setText(QStringLiteral("登录"));
    // 连不上服务器时，用户最需要看到的是服务器设置。
    if (!settingsFrameFanLX->isVisible() &&
        (reason.contains(QStringLiteral("连接")) || reason.contains(QStringLiteral("服务器")) ||
         reason.contains(QStringLiteral("超时"))))
        setSettingsVisibleFanLX(true);
}

} // namespace fanlxGui

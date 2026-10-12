#include "gui/RegisterDialogFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/PageKitFanLX.h"
#include "network/ImClientFanLX.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace fanlxGui {

RegisterDialogFanLX::RegisterDialogFanLX(const QString &dataRoot, const QString &host, quint16 port,
                                       QWidget *parent)
    : QDialog(parent), dataRootFanLX(dataRoot), hostFanLX(host), portFanLX(port) {
    setWindowTitle(QStringLiteral("注册微X账号"));
    setMinimumWidth(420);
    setStyleSheet(kit::pageStyle(kit::accentForService(QStringLiteral("微X"))) +
                  QStringLiteral("QDialog{background:#f7f9fb;}"));

    weixEditFanLX = new QLineEdit(this);
    weixEditFanLX->setPlaceholderText(QStringLiteral("如 2026013，以后用它 + 密码登录"));
    nicknameEditFanLX = new QLineEdit(this);
    birthDateEditFanLX = new QLineEdit(this);
    birthDateEditFanLX->setPlaceholderText(QStringLiteral("YYYY-MM-DD（留空按 2000-01-01）"));
    locationEditFanLX = new QLineEdit(this);
    passwordEditFanLX = new QLineEdit(this);
    passwordEditFanLX->setEchoMode(QLineEdit::Password);
    passwordEditFanLX->setPlaceholderText(QStringLiteral("本人各服务共用这一份密码"));
    repeatEditFanLX = new QLineEdit(this);
    repeatEditFanLX->setEchoMode(QLineEdit::Password);

    auto *form = new QFormLayout();
    form->addRow(QStringLiteral("微X号"), weixEditFanLX);
    form->addRow(QStringLiteral("昵称"), nicknameEditFanLX);
    form->addRow(QStringLiteral("出生时间"), birthDateEditFanLX);
    form->addRow(QStringLiteral("所在地"), locationEditFanLX);
    form->addRow(QStringLiteral("登录密码"), passwordEditFanLX);
    form->addRow(QStringLiteral("再次输入密码"), repeatEditFanLX);

    auto *note = kit::hintLabel(this);
    note->setText(QStringLiteral("注册只创建微X账号，QQ / 微信 / 微博 登录后在「开通管理」里开通。"));

    statusLabelFanLX = new QLabel(this);
    statusLabelFanLX->setWordWrap(true);
    statusLabelFanLX->setStyleSheet(QStringLiteral("color:#b3261e;"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    registerButtonFanLX = buttons->button(QDialogButtonBox::Ok);
    registerButtonFanLX->setText(QStringLiteral("注册"));
    registerButtonFanLX->setProperty("role", QStringLiteral("primary"));
    registerButtonFanLX->setMinimumHeight(34);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addWidget(statusLabelFanLX);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        // 两次密码必须一致：注册是唯一一次设置凭据的机会，打错以后就登不进来了。
        if (passwordEditFanLX->text() != repeatEditFanLX->text()) {
            statusLabelFanLX->setText(QStringLiteral("两次输入的密码不一致，未注册"));
            return;
        }
        QString detail;
        const auto ok = DomainControllerFanLX::registerWeiXUser(
            dataRootFanLX, hostFanLX, portFanLX, weixEditFanLX->text().trimmed(),
            nicknameEditFanLX->text().trimmed(), birthDateEditFanLX->text().trimmed(),
            locationEditFanLX->text().trimmed(), passwordEditFanLX->text(), &detail);
        if (!ok) {
            statusLabelFanLX->setText(detail);
            return;
        }
        registeredFanLX = weixEditFanLX->text().trimmed();
        // 只写本机领域库还不够：聊天认证查的是**服务端**的持久化账号库。
        // 这里顺手把账号登记到服务端，于是"注册完直接登录"不需要重启服务端。
        std::string reason;
        registeredOnServerFanLX = ImClientFanLX::registerAccountFanLX(
            hostFanLX.toStdString(), portFanLX, registeredFanLX.toStdString(),
            passwordEditFanLX->text().toStdString(), nicknameEditFanLX->text().trimmed().toStdString(),
            birthDateEditFanLX->text().trimmed().toStdString(),
            locationEditFanLX->text().trimmed().toStdString(), reason);
        registrationDetailFanLX =
            registeredOnServerFanLX
                ? QStringLiteral("服务端登记成功：%1").arg(QString::fromUtf8(reason.c_str()))
                : QStringLiteral("服务端登记失败：%1（登录时会自动重试登记）")
                      .arg(QString::fromUtf8(reason.c_str()));
        accept();
    });
}

} // namespace fanlxGui

#pragma once
#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;

namespace fanlxGui {

// RegisterDialogFanLX：注册微X账号。
//
// 入口在**登录界面**（"注册微X账号…"按钮），不在登录后的"账号与服务"页面——注册产出的是
// 登录身份本身，登录之后就不再需要它了。注册只创建一个上层身份（微X号 + 密码 + 资料）：
// **不**捆绑任何下级服务账号；QQ / 微信 / 微博 必须由本人登录微X后在「开通管理」里自主开通
// （领域层也只接受微X会话发起的开通请求）。
//
// 注册两步写入：
//   1. 本机领域库（<数据根>/im-client/<服务器>_<端口>/<微X号>/domain/state.json）——资料与好友/群；
//   2. **服务端账号库**：通过 IM 的 REGISTER 登记，由服务端写进它自己的持久化快照——聊天认证查的是这里。
// 第 2 步失败不影响本机注册，但必须在界面上说清楚（登录流程会自动重试登记并给出提示）。
class RegisterDialogFanLX : public QDialog {
    Q_OBJECT

  public:
    RegisterDialogFanLX(const QString &dataRoot, const QString &host, quint16 port,
                        QWidget *parent = nullptr);

    // 注册成功的微X号（供登录窗回填；未成功时为空串）。
    QString registeredWeixId() const { return registeredFanLX; }
    // 是否已同步登记到服务端（false 时登录流程会自动重试登记）。
    bool registeredOnServer() const { return registeredOnServerFanLX; }
    // 服务端登记的一句话结果（成功信息或失败原因）。
    QString registrationDetail() const { return registrationDetailFanLX; }

  private:
    QString dataRootFanLX;
    QString hostFanLX;
    quint16 portFanLX = 0;
    QString registeredFanLX;
    bool registeredOnServerFanLX = false;
    QString registrationDetailFanLX;

    QLineEdit *weixEditFanLX = nullptr;      // 微X号（登录用的唯一账号）
    QLineEdit *nicknameEditFanLX = nullptr;
    QLineEdit *birthDateEditFanLX = nullptr; // 出生时间 YYYY-MM-DD
    QLineEdit *locationEditFanLX = nullptr;
    QLineEdit *passwordEditFanLX = nullptr;
    QLineEdit *repeatEditFanLX = nullptr;
    QLabel *statusLabelFanLX = nullptr;
    QPushButton *registerButtonFanLX = nullptr;
};

} // namespace fanlxGui

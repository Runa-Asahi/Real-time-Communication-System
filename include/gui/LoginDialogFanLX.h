#pragma once
#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace fanlxGui {

// 登录窗口：服务器地址/端口、微X号、密码，以及存储后端选择，并提供"注册微X账号"入口。
// 只负责收集参数；连接与错误分类由控制器给出。
//
// 登录身份只有微X号（上层账号）：QQ / 微信 / 微博 是登录之后在内部开通的下级服务，
// 不是登录入口。密码输入框用 Password 回显模式，但传输与存储都是明文（课程范围内的取舍）。
//
// 注册入口放在这里（而不是登录后的"账号与服务"页面）：注册产出的是登录身份本身，
// 且注册时**不**捆绑任何服务账号——见 RegisterDialogFanLX。
//
// 不提供"选择数据目录"入口：真实存储目录由服务器身份 + 登录微X号派生，
// 窗口只读地预览派生结果（见 updateDerivedPathFanLX），避免"先选路径、后验身份"的顺序倒置，
// 也避免用户误选到其它账号的目录。数据根只作为命令行/测试的覆盖项出现。
class LoginDialogFanLX : public QDialog {
    Q_OBJECT

  public:
    explicit LoginDialogFanLX(QWidget *parent = nullptr);

    // 命令行参数回填（不清空用户已输入内容之外的东西，只在打开前调用）。
    void prefill(const QString &host, const QString &port, const QString &account, const QString &password,
                 const QString &dataRoot, bool useChatDatabase);

    QString host() const;
    quint16 port() const;
    QString account() const;   // 微X号
    QString password() const;
    // 数据根目录（只读展示，不参与用户编辑）。
    QString dataRoot() const;
    bool useChatDatabase() const;
    // 课设要求 5：是否"简单确认"联动登录本人其它已开通服务。
    bool confirmOthers() const;
    // 显示失败原因并允许重试（认证失败、库归属不符等都会走到这里）。
    void showFailure(const QString &reason);
    // 注册成功后回填微X号并提示"请用新账号登录"。
    void prefillRegisteredAccount(const QString &weixId, const QString &message);

  signals:
    void submittedFanLX();

  private:
    // 点「登录」：校验非空、置为"正在登录"后再发出 submittedFanLX。
    void submitFanLX();
    void showStatusFanLX(const QString &text, bool good);
    void setSettingsVisibleFanLX(bool visible);
    // 把窗口高度钉在「当前宽度下内容真正需要的高度」上。
    // 只设 setFixedWidth 时，换行标签的 minimumHeightForWidth 比 sizeHint 更高，
    // Windows 会把窗口拉高并打印 setGeometry 警告。
    void fitHeightToContentsFanLX();
    // 随服务器地址/端口/微X号变化刷新"登录后数据目录"预览。
    void updateDerivedPathFanLX();
    // 打开注册窗口（注册成功后把微X号带回登录框）。
    void openRegisterDialogFanLX();

    QLineEdit *hostEditFanLX = nullptr;
    QSpinBox *portSpinFanLX = nullptr;
    QLineEdit *accountEditFanLX = nullptr;    // 微X号
    QLineEdit *passwordEditFanLX = nullptr;
    QLineEdit *dataRootEditFanLX = nullptr;   // 只读
    QLabel *derivedPathLabelFanLX = nullptr;  // 派生结果预览
    QComboBox *storageComboFanLX = nullptr;
    QCheckBox *confirmOthersCheckFanLX = nullptr;
    QLabel *statusLabelFanLX = nullptr;
    QPushButton *loginButtonFanLX = nullptr;
    QPushButton *registerButtonFanLX = nullptr;
    QPushButton *settingsToggleFanLX = nullptr;
    QFrame *settingsFrameFanLX = nullptr;     // 服务器 / 端口 / 存储（默认收起）
};

} // namespace fanlxGui

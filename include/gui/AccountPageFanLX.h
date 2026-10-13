#pragma once
#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;

class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace fanlxGui {

class DomainControllerFanLX;

// AccountPageFanLX：账号与服务页面。
// 首页是居中的仪表盘：页标题、当前身份栏、左侧个人资料卡、右侧四个管理入口。
// 开通管理 / 登录管理 / 绑定管理 / 资料与密码仍是子页面（点入口跳转，左上角「‹ 返回」）。
//
// 界面只负责收集输入与展示结果；所有领域读写都委托 DomainControllerFanLX。
class AccountPageFanLX : public QWidget {
    Q_OBJECT

  public:
    explicit AccountPageFanLX(DomainControllerFanLX *controller, QWidget *parent = nullptr);

  public slots:
    // 重读全部视图（登录联动、开通、绑定、资料变更后由控制器信号触发）。
    void refreshFanLX();

  private:
    QWidget *buildHomeFanLX();
    QWidget *buildProfileCardFanLX(QWidget *parent);
    QWidget *buildServiceBodyFanLX();
    QWidget *buildLoginBodyFanLX();
    QWidget *buildBindingBodyFanLX();
    QWidget *buildMaintenanceBodyFanLX();

    DomainControllerFanLX *controllerFanLX = nullptr;
    QStackedWidget *stackFanLX = nullptr;

    // 首页：当前身份 + 基本信息
    QComboBox *platformComboFanLX = nullptr;
    QLabel *identityLabelFanLX = nullptr;
    QFrame *bannerFanLX = nullptr;            // 未登录 / 已全部注销 提示条
    QLabel *bannerLabelFanLX = nullptr;
    QPushButton *reloginButtonFanLX = nullptr;
    // 用户基本信息卡片：头像 + 昵称 + 三项统计 + 详情网格
    QLabel *avatarLabelFanLX = nullptr;
    QLabel *nameLabelFanLX = nullptr;
    QLabel *subtitleLabelFanLX = nullptr;
    QLabel *tAgeValueFanLX = nullptr;
    QLabel *friendValueFanLX = nullptr;
    QLabel *groupValueFanLX = nullptr;
    QLabel *birthValueFanLX = nullptr;
    QLabel *applyValueFanLX = nullptr;
    QLabel *locationValueFanLX = nullptr;
    QLabel *userIdValueFanLX = nullptr;
    // 开通管理
    QTableWidget *serviceTableFanLX = nullptr;
    QComboBox *subscribeComboFanLX = nullptr;
    QLineEdit *subscribeNumberEditFanLX = nullptr;
    QPushButton *subscribeButtonFanLX = nullptr;
    // 登录管理
    QComboBox *confirmServiceComboFanLX = nullptr;
    QPushButton *confirmLoginButtonFanLX = nullptr;
    QPushButton *linkAllButtonFanLX = nullptr;
    QPushButton *logoutButtonFanLX = nullptr;
    QPushButton *logoutAllButtonFanLX = nullptr;
    // 绑定管理
    QListWidget *bindingListFanLX = nullptr;
    QComboBox *bindServiceComboFanLX = nullptr;
    QLineEdit *bindAccountEditFanLX = nullptr;
    QPushButton *bindButtonFanLX = nullptr;
    QPushButton *unbindButtonFanLX = nullptr;
    // 资料与密码（昵称与所在地各自一个按钮，可单项修改）
    QLineEdit *nicknameEditFanLX = nullptr;
    QPushButton *saveNicknameButtonFanLX = nullptr;
    QLineEdit *locationEditFanLX = nullptr;
    QPushButton *saveLocationButtonFanLX = nullptr;
    QLineEdit *oldPasswordEditFanLX = nullptr;
    QLineEdit *newPasswordEditFanLX = nullptr;
    QLineEdit *repeatPasswordEditFanLX = nullptr;
    QPushButton *changePasswordButtonFanLX = nullptr;
    QLabel *hintLabelFanLX = nullptr;
};

} // namespace fanlxGui

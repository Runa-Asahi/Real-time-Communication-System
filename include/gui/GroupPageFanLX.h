#pragma once
#include <QList>
#include <QWidget>

class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;

namespace fanlxGui {

class DomainControllerFanLX;

// GroupPageFanLX：群页面。对应课设要求 3 与 6.4：
//   （1）每个微X功能已有 1001~1006 等群号；
//   （2）加入群、退出群、挨 T、查询群成员；
//   （3）不同微X之间的群理念不同（QQ 申请加入 / 微信推荐加入；QQ 有子群与管理员制度，
//       微信只有群主是特权账号）；
//   （4）展示当前群的特色功能，并可在成员数据不受伤害的前提下动态切换为另一类群的管理特色。
//
// 界面按"当前使用的服务"裁剪功能：两种群都能邀请他人入群，但流程不同
//   QQ  ：邀请 = 替对方提交入群申请，由群主/管理员审批；另有主动申请、管理员、子群；
//   微信：邀请 = 向对方发推荐，对方本人确认后入群；只有群主是特权账号。
// 微X / 微博没有群，只给出提示。
// 首页左侧是群列表，右侧是当前群号、成员和入口。具体操作在子页面里。
class GroupPageFanLX : public QWidget {
    Q_OBJECT

  public:
    explicit GroupPageFanLX(DomainControllerFanLX *controller, QWidget *parent = nullptr);

  public slots:
    void refreshFanLX();

  private:
    QWidget *buildHomeFanLX();
    QWidget *buildJoinBodyFanLX();
    QWidget *buildMemberBodyFanLX();
    QWidget *buildDiscussionBodyFanLX();
    QWidget *buildSettingsBodyFanLX();
    // 按服务显示 / 隐藏该服务才有的控件。
    void applyServiceFanLX(const QString &service);
    // 当前选中的群号：列表选中项优先，其次手填的群号。
    QString groupIdFanLX() const;

    DomainControllerFanLX *controllerFanLX = nullptr;
    QStackedWidget *stackFanLX = nullptr;

    // 首页
    QWidget *contentFanLX = nullptr;      // 群列表 + 入口，仅 QQ / 微信 显示
    QLabel *unsupportedLabelFanLX = nullptr;
    QString lastServiceFanLX;             // 上一次刷新时的服务，用于检测服务切换
    QLabel *headerLabelFanLX = nullptr;
    QListWidget *groupListFanLX = nullptr;
    QLineEdit *groupEditFanLX = nullptr;
    QListWidget *memberListFanLX = nullptr;
    QPushButton *joinNavFanLX = nullptr;
    QPushButton *discussionNavFanLX = nullptr;
    QList<QLabel *> scopeLabelsFanLX;     // 子页面顶部的"操作对象：群 xxx"

    // 加入 / 审批 / 推荐
    QGroupBox *inviteBoxFanLX = nullptr;           // QQ / 微信都有的"邀请他人入群"
    QLineEdit *inviteTargetEditFanLX = nullptr;
    QLabel *inviteHintFanLX = nullptr;
    QWidget *qqJoinBoxFanLX = nullptr;
    QWidget *wechatJoinBoxFanLX = nullptr;
    QLineEdit *targetAccountEditFanLX = nullptr;
    QPushButton *applyButtonFanLX = nullptr;       // QQ：申请加入
    QPushButton *approveButtonFanLX = nullptr;     // QQ：批准申请
    QPushButton *rejectApplyButtonFanLX = nullptr; // QQ：拒绝申请
    QPushButton *inviteButtonFanLX = nullptr;      // 邀请入群（QQ：提交申请待审批；微信：推荐待确认）
    QPushButton *confirmInviteButtonFanLX = nullptr;
    QPushButton *rejectInviteButtonFanLX = nullptr;
    QPushButton *leaveButtonFanLX = nullptr;
    QListWidget *pendingListFanLX = nullptr;

    // 成员管理
    QLineEdit *memberTargetEditFanLX = nullptr;
    QPushButton *kickButtonFanLX = nullptr;
    QWidget *adminBoxFanLX = nullptr;              // 仅 QQ：管理员制度
    QPushButton *assignAdminButtonFanLX = nullptr;
    QPushButton *revokeAdminButtonFanLX = nullptr;
    QPushButton *transferButtonFanLX = nullptr;

    // 子群（仅 QQ）
    QLineEdit *discussionIdEditFanLX = nullptr;
    QLineEdit *discussionTargetEditFanLX = nullptr;
    QPushButton *createDiscussionButtonFanLX = nullptr;
    QPushButton *joinDiscussionButtonFanLX = nullptr;
    QListWidget *discussionListFanLX = nullptr;

    // 群设置
    QPlainTextEdit *policyViewFanLX = nullptr;
    QLineEdit *groupNameEditFanLX = nullptr;
    QPushButton *renameButtonFanLX = nullptr;
    QPushButton *createGroupButtonFanLX = nullptr;
    QPushButton *dissolveButtonFanLX = nullptr;
    QPushButton *toQqButtonFanLX = nullptr;
    QPushButton *toWechatButtonFanLX = nullptr;
};

} // namespace fanlxGui

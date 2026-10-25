#pragma once
// PageKitFanLX：三个领域页面（账号与服务 / 好友 / 群）共用的"首页 + 子页面跳转"小工具。
// 首页只放概览与入口按钮，具体操作放进子页面，左上角统一有「‹ 返回」，避免所有控件挤在一屏。
//
// 配色：按钮 / 卡片 / 标题打 role 属性，颜色来自 AppThemeFanLX 这一套品牌色。
// accentForService 只留给小型服务标识。切换服务不再给整页、全部按钮重新上色。
#include "gui/AppThemeFanLX.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>

namespace fanlxGui::kit {

// 服务 -> 主题色。
inline QString accentForService(const QString &service) {
    if (service == QStringLiteral("QQ"))
        return QStringLiteral("#1b8fe0");
    if (service == QStringLiteral("微信"))
        return QStringLiteral("#07a655");
    if (service == QStringLiteral("微博"))
        return QStringLiteral("#e2433a");
    return QStringLiteral("#6a4fd8"); // 微X：上层身份
}

// 页面级样式表。accentName 保留是为了不改调用点；整页不再跟随服务色。
// 色值与 AppThemeFanLX 一致：白卡片、8px 控件、12px 大卡片。
inline QString pageStyle(const QString &accentName) {
    Q_UNUSED(accentName);
    return QStringLiteral(
        "QPushButton[role=\"primary\"]{background:#2563EB;color:#ffffff;border:none;border-radius:8px;"
        "padding:0px 16px;min-height:36px;font-size:14px;font-weight:600;}"
        "QPushButton[role=\"primary\"]:hover{background:#1D4ED8;}"
        "QPushButton[role=\"primary\"]:pressed{background:#1E40AF;}"
        "QPushButton[role=\"primary\"]:disabled{background:#E6E8EE;color:#8A94A3;}"
        "QPushButton[role=\"nav\"]{text-align:left;padding:0px 16px;border:1px solid #E6E8EE;"
        "border-radius:8px;background:#ffffff;font-size:14px;font-weight:600;min-height:36px;color:#1F2329;}"
        "QPushButton[role=\"nav\"]:hover{background:#F3F5F8;border-color:#D7DEE8;}"
        "QPushButton[role=\"nav\"]:disabled{color:#8A94A3;background:#F6F7F9;}"
        "QPushButton[role=\"danger\"]{padding:0px 16px;border:1px solid #FECACA;border-radius:8px;"
        "color:#DC2626;background:#ffffff;min-height:36px;font-size:14px;font-weight:600;}"
        "QPushButton[role=\"danger\"]:hover{background:#FEE2E2;border-color:#DC2626;}"
        "QPushButton[role=\"back\"]{color:#2563EB;padding:4px 8px;border:none;background:transparent;"
        "font-weight:600;font-size:14px;border-radius:8px;}"
        "QPushButton[role=\"back\"]:hover{background:#E8EFFD;}"
        "QFrame#cardFanLX{background:#ffffff;border:1px solid #E6E8EE;border-radius:12px;}"
        "QLabel[role=\"title\"]{font-size:20px;font-weight:600;color:#1F2329;}"
        "QLabel[role=\"hint\"]{color:#5B6472;font-size:13px;}"
        "QFrame#bannerFanLX{background:#FFF4E5;border:1px solid #F3D7B0;border-radius:12px;}"
        "QFrame#bannerFanLX QLabel{color:#B45309;}"
        "QFrame#profileCardFanLX{background:#ffffff;border:1px solid #E6E8EE;border-radius:12px;}"
        "QLabel[role=\"section\"]{font-size:12px;font-weight:600;color:#2563EB;}"
        "QLabel[role=\"avatar\"]{background:#2563EB;color:#ffffff;border-radius:28px;font-size:24px;"
        "font-weight:700;}"
        "QLabel[role=\"name\"]{font-size:20px;font-weight:600;color:#1F2329;}"
        "QLabel[role=\"stat\"]{font-size:26px;font-weight:600;color:#1F2329;}"
        "QLabel[role=\"caption\"]{font-size:12px;color:#8A94A3;}"
        "QLabel[role=\"value\"]{font-size:14px;color:#1F2329;}"
        "QFrame[role=\"divider\"]{background:#E6E8EE;border:none;}"
        "QGroupBox{border:1px solid #E6E8EE;border-radius:12px;margin-top:14px;padding-top:6px;background:#ffffff;}"
        "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 6px;color:#1F2329;font-weight:600;}"
        "QListWidget,QTableWidget,QPlainTextEdit{border:1px solid #E6E8EE;border-radius:12px;"
        "background:#ffffff;color:#1F2329;}"
        "QListWidget::item{padding:8px 10px;}"
        "QListWidget::item:selected,QTableWidget::item:selected{background:#E8EFFD;color:#1F2329;}"
        "QHeaderView::section{background:#F3F5F8;border:none;border-bottom:1px solid #E6E8EE;padding:8px;"
        "color:#5B6472;font-weight:600;}"
        "QLineEdit{border:1px solid #E6E8EE;border-radius:8px;padding:0px 10px;min-height:36px;"
        "background:#ffffff;color:#1F2329;}"
        "QLineEdit:focus{border:1px solid #2563EB;}");
}

// 装上统一的页面样式。service 只保留参数，避免调用点跟着改；不再按服务重绘整页。
inline void applyAccent(QWidget *page, const QString &service) {
    Q_UNUSED(service);
    const auto style = pageStyle(theme::brand);
    if (page->styleSheet() != style)
        page->setStyleSheet(style);
}

// 入口按钮：白底圆角、左侧一道主题色竖条，悬停变浅色。
inline QPushButton *navButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setProperty("role", QStringLiteral("nav"));
    // fanlxKind 与 role 同值，供 resources/app.qss 按属性选择。其它页面仍按 role 配色。
    button->setProperty("fanlxKind", QStringLiteral("nav"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(42);
    return button;
}

// 主操作按钮：主题色底白字。
inline QPushButton *primaryButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setProperty("role", QStringLiteral("primary"));
    button->setProperty("fanlxKind", QStringLiteral("primary"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(theme::controlHeight);
    return button;
}

// 危险操作按钮：红字描边。
inline QPushButton *dangerButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setProperty("role", QStringLiteral("danger"));
    button->setProperty("fanlxKind", QStringLiteral("danger"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(theme::controlHeight);
    return button;
}

// 灰色说明文字（单行短提示，不要写长段落）。
inline QLabel *hintLabel(QWidget *parent) {
    auto *label = new QLabel(parent);
    label->setWordWrap(true);
    label->setProperty("role", QStringLiteral("hint"));
    return label;
}

// 概览卡片：浅色底、左侧主题色竖条，内部由调用者放置控件。
inline QFrame *card(QWidget *parent) {
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("cardFanLX"));
    return frame;
}

// 白底分区卡片。objectName 固定为 card，由 resources/app.qss 绘制边框。
// 不改 card()：账号页和好友页仍用带主题色竖条的 cardFanLX。
inline QFrame *sectionCard(QWidget *parent) {
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("card"));
    frame->setAttribute(Qt::WA_StyledBackground, true);
    return frame;
}

// 页面标题（子页面顶部的大字）。
inline QLabel *titleLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("title"));
    return label;
}

// 追加一个子页面：顶部「‹ 返回」+ 标题，下面是可滚动的内容。返回页面下标。
inline int addSubPage(QStackedWidget *stack, const QString &title, QWidget *body) {
    auto *page = new QWidget(stack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 8, 10, 8);

    auto *back = new QPushButton(QStringLiteral("‹ 返回"), page);
    back->setProperty("role", QStringLiteral("back"));
    back->setCursor(Qt::PointingHandCursor);
    auto *heading = titleLabel(title, page);
    auto *top = new QHBoxLayout();
    top->addWidget(back);
    top->addWidget(heading);
    top->addStretch(1);
    layout->addLayout(top);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);

    QObject::connect(back, &QPushButton::clicked, stack, [stack] { stack->setCurrentIndex(0); });
    return stack->addWidget(page);
}

// 给首页内容套一层可滚动容器并作为 stack 的第 0 页。
inline void setHomePage(QStackedWidget *stack, QWidget *home) {
    auto *scroll = new QScrollArea(stack);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(home);
    stack->insertWidget(0, scroll);
    stack->setCurrentIndex(0);
}

} // namespace fanlxGui::kit

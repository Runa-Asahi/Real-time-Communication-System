#pragma once
// PageKitFanLX：三个领域页面（账号与服务 / 好友 / 群）共用的"首页 + 子页面跳转"小工具。
// 首页只放概览与入口按钮，具体操作放进子页面，左上角统一有「‹ 返回」，避免所有控件挤在一屏。
//
// 配色：按钮 / 卡片 / 标题打 role 属性，颜色来自 AppThemeFanLX 这一套品牌色。
// accentForService 只留给小型服务标识。切换服务不再给整页、全部按钮重新上色。
#include "gui/AppThemeFanLX.h"
#include <QColor>
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

namespace detail {
// 把 color 与白色按 ratio（0~1，越大越接近白色）混合，得到浅色底。
inline QString tint(const QColor &color, double ratio) {
    const auto mixed = [ratio](int channel) { return static_cast<int>(channel + (255 - channel) * ratio); };
    return QColor(mixed(color.red()), mixed(color.green()), mixed(color.blue())).name();
}
} // namespace detail

// 页面级样式表。accentName 保留是为了不改调用点；整页不再跟随服务色。
inline QString pageStyle(const QString &accentName) {
    Q_UNUSED(accentName);
    const QColor accent(theme::brand);
    const auto dark = accent.darker(120).name();
    const auto light = detail::tint(accent, 0.90);
    const auto soft = detail::tint(accent, 0.65);
    return QStringLiteral(
               "QPushButton[role=\"primary\"]{background:%1;color:#ffffff;border:none;border-radius:6px;"
               "padding:4px 16px;}"
               "QPushButton[role=\"primary\"]:hover{background:%2;}"
               "QPushButton[role=\"primary\"]:disabled{background:#c5ccd3;}"
               "QPushButton[role=\"nav\"]{text-align:left;padding:6px 16px;border:1px solid #d9dee3;"
               "border-left:4px solid %1;border-radius:8px;background:#ffffff;font-size:14px;}"
               "QPushButton[role=\"nav\"]:hover{background:%3;border-color:%1;}"
               "QPushButton[role=\"nav\"]:disabled{color:#9aa0a6;background:#f6f8fa;border-left-color:#c5ccd3;}"
               "QPushButton[role=\"danger\"]{padding:4px 16px;border:1px solid #DC2626;border-radius:6px;"
               "color:#DC2626;background:#ffffff;}"
               "QPushButton[role=\"danger\"]:hover{background:#FEE2E2;}"
               "QPushButton[role=\"back\"]{color:%1;padding:4px 8px;border:none;background:transparent;"
               "font-weight:600;}"
               "QPushButton[role=\"back\"]:hover{background:%3;border-radius:6px;}"
               "QFrame#cardFanLX{background:%3;border:1px solid %4;border-left:5px solid %1;"
               "border-radius:10px;}"
               "QLabel[role=\"title\"]{font-size:16px;font-weight:600;color:%2;}"
               "QLabel[role=\"hint\"]{color:#5f6368;}"
               "QFrame#bannerFanLX{background:#fff4e5;border:1px solid #ffd8a8;border-radius:8px;}"
               "QFrame#bannerFanLX QLabel{color:#7a4b00;}"
               "QFrame#profileCardFanLX{background:#ffffff;border:1px solid #e1e4e8;border-radius:12px;}"
               "QLabel[role=\"section\"]{font-size:12px;font-weight:600;color:%1;letter-spacing:1px;}"
               "QLabel[role=\"avatar\"]{background:%1;color:#ffffff;border-radius:28px;font-size:24px;"
               "font-weight:700;}"
               "QLabel[role=\"name\"]{font-size:20px;font-weight:700;color:#202124;}"
               "QLabel[role=\"stat\"]{font-size:26px;font-weight:700;color:%1;}"
               "QLabel[role=\"caption\"]{font-size:12px;color:#80868b;}"
               "QLabel[role=\"value\"]{font-size:14px;color:#202124;}"
               "QFrame[role=\"divider\"]{background:#eceff1;border:none;}"
               "QGroupBox{border:1px solid #d9dee3;border-radius:8px;margin-top:14px;padding-top:6px;}"
               "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 6px;color:%2;font-weight:600;}"
               "QListWidget,QTableWidget,QPlainTextEdit{border:1px solid #d9dee3;border-radius:6px;"
               "background:#ffffff;}"
               "QListWidget::item{padding:4px 6px;}"
               "QListWidget::item:selected,QTableWidget::item:selected{background:%3;color:#202124;}"
               "QHeaderView::section{background:%3;border:none;border-bottom:1px solid %4;padding:4px;"
               "color:%2;font-weight:600;}"
               "QLineEdit{border:1px solid #c7cdd4;border-radius:6px;padding:4px 8px;background:#ffffff;}"
               "QLineEdit:focus{border:1px solid %1;}")
        .arg(accent.name(), dark, light, soft);
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

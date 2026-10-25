#include "gui/MainWindowFanLX.h"
#include "db/ChatRepositoryFanLX.h"
#include "gui/AccountPageFanLX.h"
#include "gui/AppThemeFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/FriendPageFanLX.h"
#include "gui/GroupPageFanLX.h"
#include "gui/HistoryDialogFanLX.h"
#include "network/MessageCodecFanLX.h"
#include <QIcon>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QListView>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QButtonGroup>
#include <QTabBar>
#include <QToolButton>
#include <QTabWidget>
#include <QScrollArea>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>
#include <QDateTime>
#include <QScrollBar>
#include <algorithm>

// 必须写在命名空间外面：Q_INIT_RESOURCE 不能出现在 namespace 里。
// 群页外观在主窗口创建时加载。选择器限定在 #groupPageFanLX，不改其它页签。
static void loadApplicationStyleFanLX() {
    Q_INIT_RESOURCE(app);
    QFile file(QStringLiteral(":/app.qss"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QString style = QString::fromUtf8(file.readAll());
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (app && app->styleSheet() != style)
        app->setStyleSheet(style);
    fanlxGui::theme::apply(app);
}

namespace fanlxGui {

namespace {

// 库里的时间是服务器 UTC（带 Z）。气泡和会话列表要跟本机时钟对齐，只取本地时分。
QString chatClockFanLX(const QString &time) {
    if (time.isEmpty())
        return time;
    const auto utc = QDateTime::fromString(time, Qt::ISODate);
    if (utc.isValid())
        return utc.toLocalTime().toString(QStringLiteral("HH:mm"));
    const auto mark = time.indexOf(QLatin1Char('T'));
    if (mark >= 0 && time.size() >= mark + 6)
        return time.mid(mark + 1, 5);
    return time;
}

// Windows 会在标题末尾再追加一次 applicationDisplayName（Ciallo）。
// 这里只写当前会话和服务，不写产品名，也不写 d|账号|账号 这种会话键。
QString windowCaptionFanLX(const QString &subject, const QString &service) {
    if (subject.isEmpty())
        return service;
    if (service.isEmpty())
        return subject;
    return QStringLiteral("%1 · %2").arg(subject, service);
}

// ---------- 聊天页视觉规范（只作用于聊天页，不影响账号 / 好友 / 群页签） ----------
// 统一在这里定义颜色，样式表与消息 HTML、会话列表绘制共用同一套取值。
namespace chatThemeFanLX {
const QString pageBg = fanlxGui::theme::page;
const QString panelBg = fanlxGui::theme::panel;
const QString border = fanlxGui::theme::line;
const QString canvasBg = fanlxGui::theme::canvas;
const QString textPrimary = fanlxGui::theme::text;
const QString textSecondary = fanlxGui::theme::textSecondary;
const QString textMuted = fanlxGui::theme::textMuted;
const QString accent = fanlxGui::theme::brand;
const QString accentHover = fanlxGui::theme::brandHover;
const QString accentPressed = fanlxGui::theme::brandPressed;
const QString accentSoft = fanlxGui::theme::brandSoft;
const QString selfBubble = fanlxGui::theme::selfBubble;
const QString peerBubble = fanlxGui::theme::panel;
const QString danger = fanlxGui::theme::danger;
const QString dangerSoft = fanlxGui::theme::dangerSoft;
} // namespace chatThemeFanLX

// 聊天页唯一的样式入口：挂在聊天页根控件（#chatPageFanLX）上，用 objectName / 属性选择器
// 限定作用范围，因此不会扩散到其他页签，也不必在各控件上零散 setStyleSheet。
QString chatPageStyleFanLX() {
    using namespace chatThemeFanLX;
    return QStringLiteral(
               "QWidget#chatPageFanLX{background:%1;}"
               // 左右两块白色面板
               "QFrame#chatSidebarFanLX,QFrame#chatPanelFanLX{background:%2;border:1px solid %3;"
               "border-radius:12px;}"
               "QLabel#chatSidebarTitleFanLX{font-size:16px;font-weight:600;color:%4;}"
               "QLabel#chatEmptyFanLX{color:%6;font-size:13px;padding:28px 18px;}"
               "QListView#chatConversationsFanLX{background:transparent;border:none;outline:none;}"
               // 聊天标题区
               "QFrame#chatHeaderFanLX{background:transparent;border:none;border-bottom:1px solid %3;"
               "border-top-left-radius:12px;border-top-right-radius:12px;}"
               "QLabel#chatTitleFanLX{font-size:18px;font-weight:600;color:%4;}"
               "QLabel#chatMetaFanLX{font-size:12px;color:%6;}"
               "QLabel#chatServiceFanLX{font-size:12px;color:%6;}"
               "QLabel#chatServiceFanLX[alertFanLX=\"true\"]{color:#7a4b00;}"
               // 消息区：滚动视口铺底，气泡本身另有圆角样式
               "QScrollArea#chatMessages{background:#EAF0F7;border:none;}"
               "QWidget#chatMessageViewportFanLX{background:#EAF0F7;border:none;}"
               "QWidget#chatMessageHostFanLX{background:#EAF0F7;}"
               // 工具条：次要按钮 + 新消息提示
               "QFrame#chatToolbarFanLX{background:%7;border:none;border-bottom:1px solid %3;}"
               "QPushButton[chatRole=\"secondary\"]{background:%2;color:%5;border:1px solid %3;"
               "border-radius:6px;padding:0 12px;min-height:26px;font-size:12px;}"
               "QPushButton[chatRole=\"secondary\"]:hover{background:%11;color:%8;border-color:#c9d6ef;}"
               "QPushButton[chatRole=\"secondary\"]:pressed{background:#dde7fb;}"
               "QPushButton[chatRole=\"secondary\"]:disabled{background:transparent;color:#b9c0ca;"
               "border-color:#eceff3;}"
               "QLabel#chatNewMessageHintFanLX{background:%11;color:%8;border:1px solid #cddcf8;"
               "border-radius:11px;padding:2px 12px;font-size:12px;}"
               // 输入区
               "QFrame#chatComposerFanLX{background:%2;border:none;border-bottom-left-radius:12px;"
               "border-bottom-right-radius:12px;}"
               "QLineEdit#chatEditorFanLX{background:%7;color:%4;border:1px solid %3;border-radius:8px;"
               "padding:0 12px;min-height:38px;font-size:14px;selection-background-color:#bcd3fb;}"
               "QLineEdit#chatEditorFanLX:hover{border-color:#cdd5df;}"
               "QLineEdit#chatEditorFanLX:focus{background:%2;border:1px solid %8;}"
               "QLabel#chatEditorHintFanLX{font-size:11px;color:%6;}"
               "QPushButton#chatSendButtonFanLX{background:%8;color:#ffffff;border:none;border-radius:8px;"
               "min-height:40px;min-width:88px;padding:0 20px;font-size:14px;font-weight:600;}"
               "QPushButton#chatSendButtonFanLX:hover{background:%9;}"
               "QPushButton#chatSendButtonFanLX:pressed{background:%10;}"
               "QPushButton#chatSendButtonFanLX:disabled{background:#b9cdf5;color:#f4f7fd;}"
               // 分栏手柄：平时只是两块面板之间的留白，悬停时显出浅色提示可拖动
               "QSplitter#chatSplitterFanLX::handle{background:transparent;}"
               "QSplitter#chatSplitterFanLX::handle:hover{background:#dfe5ee;border-radius:3px;}"
               // 细滚动条
               "QWidget#chatPageFanLX QScrollBar:vertical{background:transparent;width:10px;margin:2px;}"
               "QWidget#chatPageFanLX QScrollBar::handle:vertical{background:#cfd6df;border-radius:3px;"
               "min-height:36px;}"
               "QWidget#chatPageFanLX QScrollBar::handle:vertical:hover{background:#b3bcc8;}"
               "QWidget#chatPageFanLX QScrollBar::add-line:vertical,"
               "QWidget#chatPageFanLX QScrollBar::sub-line:vertical{height:0;}"
               "QWidget#chatPageFanLX QScrollBar::add-page:vertical,"
               "QWidget#chatPageFanLX QScrollBar::sub-page:vertical{background:transparent;}")
        .arg(pageBg, panelBg, border, textPrimary, textSecondary, textMuted, canvasBg, accent, accentHover,
             accentPressed, accentSoft);
}

// 主窗口外框（菜单栏 + 页签）与聊天页同一套颜色。
// 样式只挂在菜单栏、三个下拉菜单和页签条上，选择器都带 objectName，
// 因此不会进到登录窗、注册窗、历史查询窗，也不会改写各页签内部控件。
const QString chromeHoverFanLX = QStringLiteral("#e7ebf2");

QString menuBarStyleFanLX() {
    using namespace chatThemeFanLX;
    return QStringLiteral(
               "QMenuBar#mainMenuBarFanLX{background:%1;border:none;padding:0 8px;font-size:12px;}"
               "QMenuBar#mainMenuBarFanLX::item{background:transparent;color:%2;padding:3px 10px;"
               "margin:2px 1px;border:none;border-radius:4px;}"
               "QMenuBar#mainMenuBarFanLX::item:selected{background:%3;color:%4;}"
               "QMenuBar#mainMenuBarFanLX::item:pressed{background:%5;color:%6;}"
               "QMenuBar#mainMenuBarFanLX::item:disabled{background:transparent;color:%7;}")
        .arg(pageBg, textSecondary, chromeHoverFanLX, textPrimary, accentSoft, accent, textMuted);
}

QString menuPopupStyleFanLX() {
    using namespace chatThemeFanLX;
    return QStringLiteral(
               "QMenu#mainMenuFanLX{background:%1;border:1px solid %2;border-radius:8px;padding:4px;}"
               "QMenu#mainMenuFanLX::item{background:transparent;color:%3;padding:6px 18px 6px 12px;"
               "margin:1px 2px;border:none;border-radius:4px;font-size:12px;}"
               "QMenu#mainMenuFanLX::item:selected{background:%4;color:%3;}"
               "QMenu#mainMenuFanLX::item:disabled{background:transparent;color:%5;}"
               "QMenu#mainMenuFanLX::separator{height:1px;background:%2;margin:4px 8px;}")
        .arg(panelBg, border, textPrimary, accentSoft, textMuted);
}

QString tabBarStyleFanLX() {
    using namespace chatThemeFanLX;
    // 页签保持扁平：选中靠蓝色文字、浅蓝底和底边，而不是凸起边框。
    // 不画整条底部分隔线，避免和选中页签的蓝色下划线叠成双线。
    // :first 的左边距用来和菜单文字左缘对齐。
    // 占位符必须从 %1 连续编号：QString::arg 按出现的最小编号依次替换。
    return QStringLiteral(
               "QTabBar#mainTabBarFanLX{background:%1;border:none;qproperty-drawBase:0;}"
               "QTabBar#mainTabBarFanLX::tab{background:transparent;color:%2;border:none;"
               "border-bottom:2px solid transparent;padding:8px 16px 6px 16px;margin:4px 4px 0 4px;"
               "font-size:14px;}"
               "QTabBar#mainTabBarFanLX::tab:first{margin-left:2px;}"
               "QTabBar#mainTabBarFanLX::tab:hover{background:%3;color:%4;}"
               "QTabBar#mainTabBarFanLX::tab:selected{background:%5;color:%6;border-bottom:2px solid %6;}"
               "QTabBar#mainTabBarFanLX::tab:selected:hover{background:%5;color:%6;}"
               "QTabBar#mainTabBarFanLX::tab:focus{background:%3;color:%4;}"
               "QTabBar#mainTabBarFanLX::tab:selected:focus{background:%5;color:%6;"
               "border-bottom:2px solid %6;}")
        .arg(pageBg, textSecondary, chromeHoverFanLX, textPrimary, accentSoft, accent);
}

QString tabWidgetStyleFanLX() {
    using namespace chatThemeFanLX;
    // 只去掉页签外框。选择器限定在这个 QTabWidget 上，不改页内按钮和输入框。
    return QStringLiteral(
               "QTabWidget#mainTabsFanLX{background:%1;border:none;}"
               "QTabWidget#mainTabsFanLX::pane{background:%1;border:none;top:0px;}")
        .arg(pageBg);
}

// 同一会话名始终落到同一种颜色。只用于首字母占位，不表示在线。
QString avatarColorFanLX(const QString &seed) { return theme::avatarColor(seed); }

// 会话列表行：首字母头像、名称、类型、摘要和时间。未读角标在右上。
// 模型里的 "[好友] / [群]" 前缀留给工具提示和其它读者，这里不再画出来。
class ConversationDelegateFanLX : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        (void)index;
        const auto title = QFontMetrics(titleFont(option.font));
        const auto sub = QFontMetrics(subtitleFont(option.font));
        const int titleRow = std::max(title.height(), kIcon);
        return {option.rect.width(), kPadY * 2 + titleRow + kLineGap + sub.height() + kRowGap};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if (!index.isValid())
            return;
        using namespace chatThemeFanLX;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRectF card = QRectF(option.rect).adjusted(kMarginX, kRowGap / 2.0, -kMarginX, -kRowGap / 2.0);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        if (selected || hovered) {
            QPainterPath path;
            path.addRoundedRect(card, 8, 8);
            painter->fillPath(path, QColor(selected ? accentSoft : QStringLiteral("#f4f6f9")));
            if (selected) {
                // 左侧细竖条强调当前会话
                QPainterPath bar;
                bar.addRoundedRect(QRectF(card.left() + 2, card.top() + 10, 3, card.height() - 20), 1.5, 1.5);
                painter->fillPath(bar, QColor(accent));
            }
        }

        const auto titleF = titleFont(option.font);
        const auto subF = subtitleFont(option.font);
        const QFontMetrics titleM(titleF);
        const QFontMetrics subM(subF);
        const auto content = card.toRect().adjusted(kPadX, kPadY - kRowGap / 2, -kPadX, 0);
        const int titleRowH = std::max(titleM.height(), kIcon);

        // 未读角标（右上），数量为 0 时不画。位置仍对着第一行，不跟图标抢地方。
        const auto unread = index.data(ConversationListModelFanLX::UnreadRoleFanLX).toLongLong();
        int badgeWidth = 0;
        if (unread > 0) {
            QFont badgeF = subF;
            badgeF.setPixelSize(11);
            badgeF.setWeight(QFont::DemiBold);
            const QFontMetrics badgeM(badgeF);
            const auto text = unread > 99 ? QStringLiteral("99+") : QString::number(unread);
            const int h = 18;
            badgeWidth = std::max(h, badgeM.horizontalAdvance(text) + 12);
            const QRect badge(content.right() - badgeWidth, content.top() + (titleRowH - h) / 2, badgeWidth, h);
            QPainterPath pill;
            pill.addRoundedRect(badge, h / 2.0, h / 2.0);
            painter->fillPath(pill, QColor(QStringLiteral("#e5484d")));
            painter->setFont(badgeF);
            painter->setPen(Qt::white);
            painter->drawText(badge, Qt::AlignCenter, text);
            badgeWidth += 8;
        }

        const auto kind = index.data(ConversationListModelFanLX::KindTextRoleFanLX).toString();
        auto titleText = index.data(Qt::DisplayRole).toString();
        const auto prefix = QStringLiteral("[") + kind + QStringLiteral("] ");
        if (titleText.startsWith(prefix))
            titleText = titleText.mid(prefix.size());
        const QString seed = titleText.isEmpty() ? kind : titleText;
        const QRect slot(content.left(), content.top() + (titleRowH - kIcon) / 2, kIcon, kIcon);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(avatarColorFanLX(seed)));
        painter->drawEllipse(slot);
        painter->setPen(Qt::white);
        QFont markFont = subF;
        markFont.setPixelSize(13);
        markFont.setWeight(QFont::DemiBold);
        painter->setFont(markFont);
        const QString mark = seed.isEmpty() ? QStringLiteral("?") : seed.left(1);
        painter->drawText(slot, Qt::AlignCenter, mark);
        const int textLeft = slot.right() + 1 + kIconGap;

        const int titleWidth = std::max(0, content.width() - badgeWidth - (textLeft - content.left()));
        const QRect titleRect(textLeft, content.top(), titleWidth, titleRowH);
        painter->setFont(titleF);
        painter->setPen(QColor(textPrimary));
        const QString kindMark = kind.isEmpty() ? QString() : QStringLiteral("  ") + kind;
        const int kindWidth = kindMark.isEmpty() ? 0 : QFontMetrics(subF).horizontalAdvance(kindMark);
        const int nameWidth = std::max(0, titleRect.width() - kindWidth);
        painter->drawText(QRect(titleRect.left(), titleRect.top(), nameWidth, titleRect.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, titleM.elidedText(titleText, Qt::ElideRight, nameWidth));
        if (!kindMark.isEmpty() && nameWidth > 24) {
            painter->setFont(subF);
            painter->setPen(QColor(textMuted));
            painter->drawText(QRect(titleRect.left() + nameWidth, titleRect.top(), kindWidth, titleRect.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, kindMark);
        }

        auto subtitle = index.data(ConversationListModelFanLX::SubtitleRoleFanLX).toString();
        subtitle.replace(QLatin1Char('\n'), QLatin1Char(' '));
        const bool emptySubtitle = subtitle.trimmed().isEmpty();
        const auto when = chatClockFanLX(index.data(ConversationListModelFanLX::TimeRoleFanLX).toString());
        const int timeWidth = when.isEmpty() ? 0 : subM.horizontalAdvance(when) + 8;
        const QRect subRect(textLeft, content.top() + titleRowH + kLineGap,
                            std::max(0, content.right() - textLeft - timeWidth + 1), subM.height());
        painter->setFont(subF);
        painter->setPen(QColor(emptySubtitle ? QStringLiteral("#b4bcc7") : textMuted));
        painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter,
                          subM.elidedText(emptySubtitle ? QStringLiteral("暂无消息") : subtitle, Qt::ElideRight,
                                          subRect.width()));
        if (!when.isEmpty()) {
            painter->drawText(QRect(subRect.right(), subRect.top(), timeWidth, subRect.height()),
                              Qt::AlignRight | Qt::AlignVCenter, when);
        }
        painter->restore();
    }

  private:
    static constexpr int kMarginX = 8;  // 行卡片与列表边缘的距离
    static constexpr int kPadX = 14;    // 行内左右留白
    static constexpr int kPadY = 12;    // 行内上下留白
    static constexpr int kLineGap = 4;  // 标题与摘要的间距
    static constexpr int kRowGap = 4;   // 行与行之间的间距
    static constexpr int kIcon = 36;    // 首字母头像边长
    static constexpr int kIconGap = 10; // 头像和名称的间距

    static QFont titleFont(QFont base) {
        base.setPixelSize(14);
        base.setWeight(QFont::DemiBold);
        return base;
    }
    static QFont subtitleFont(QFont base) {
        base.setPixelSize(12);
        base.setWeight(QFont::Normal);
        return base;
    }
};

// 列表模型每次刷新都会重置（选中状态随之丢失）：按当前会话键把高亮恢复到对应行。
// 只改选中外观，不发 clicked，因此不会重复触发打开会话、标记已读等逻辑。
void syncConversationSelectionFanLX(QListView *view, const ConversationListModelFanLX *model,
                                    const QString &currentKey) {
    if (!view || !model || !view->selectionModel())
        return;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!currentKey.isEmpty() && model->keyAt(row) == currentKey) {
            const auto index = model->index(row, 0);
            view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
            return;
        }
    }
    view->selectionModel()->clearSelection();
}

// 副标题只保留聊天服务和当前账号。两边都空时返回空串，调用方据此收起这一行。
QString chatSubtitleFanLX(const QString &service, const QString &account) {
    if (service.isEmpty())
        return account.isEmpty() ? QString() : QStringLiteral("账号 %1").arg(account);
    if (account.isEmpty())
        return service;
    return QStringLiteral("%1 · 账号 %2").arg(service, account);
}

// 聊天标题用会话名称，不再叠 [好友] / [群] 色标（左侧列表已经标出类型）。
// 群号本身读起来不像名字，因此写成「群聊 1001」；好友与临时会话直接用对端名称。
QString chatConversationTitleFanLX(const QString &key, const QString &account, bool group,
                                   const QString &itemTitle) {
    QString name = itemTitle.trimmed();
    if (name.isEmpty()) {
        const auto parts = key.split(QLatin1Char('|'));
        if (group) {
            name = parts.size() > 1 ? parts.at(1) : key;
        } else if (parts.size() >= 3 && parts.at(0) == QStringLiteral("d")) {
            name = parts.at(1) == account ? parts.at(2) : parts.at(1);
        } else {
            name = key;
        }
    }
    if (name.isEmpty())
        name = key;
    return group ? QStringLiteral("群聊 %1").arg(name) : name;
}

// 服务行的两种形态：正常连接是一行弱化副标题；需要用户处理时换成完整提示。
// alertFanLX 只影响颜色，文案本身由调用方传入，避免把提示写短。
void setChatServiceStatusFanLX(QLabel *label, const QString &text, bool alert) {
    if (!label)
        return;
    const QVariant previous = label->property("alertFanLX");
    const bool modeChanged = !previous.isValid() || previous.toBool() != alert;
    label->setProperty("alertFanLX", alert);
    label->setText(text);
    label->setVisible(!text.isEmpty());
    if (modeChanged) {
        if (auto *style = label->style()) {
            style->unpolish(label);
            style->polish(label);
        }
        label->update();
    }
}

// 只识别「点开会话」这句普通提示。错误、连接状态和领域操作结果都不走这个格式。
bool chatConversationStatusFanLX(const QString &text) {
    return text.startsWith(QStringLiteral("已打开")) && text.contains(QStringLiteral("会话："));
}

void clearChatConversationStatusFanLX(QLabel *status) {
    if (!status || !chatConversationStatusFanLX(status->text()))
        return;
    status->setText(QString());
    status->setStyleSheet(QStringLiteral("color:#1f2328; padding:2px;"));
}

// 只替换我们自己写的 %1/%2/%3。用户昵称里的百分号不会再被 arg 吃掉。
QString spanFanLX(const QString &color, int weight, const QString &plain) {
    QString span = QStringLiteral("<span style=\"color:%1;font-weight:%2;\">%3</span>");
    span.replace(QStringLiteral("%1"), color);
    span.replace(QStringLiteral("%2"), QString::number(weight));
    span.replace(QStringLiteral("%3"), plain.toHtmlEscaped());
    return span;
}

// 消息区宿主。不用 QTextBrowser 表格：气泡是带圆角的控件，正文是纯文本。
// sizeHint 保持一个普通高度，避免滚动区把整个窗口撑到内容那么高。
class MessagePaneFanLX : public QWidget {
  public:
    static constexpr int kCap = 420;
    static constexpr int kSide = 18;
    static constexpr int kPadX = 12;
    static constexpr int kPadY = 10;

    struct MessageRowFanLX {
        QWidget *root = nullptr;
        QLabel *meta = nullptr;
        QFrame *bubble = nullptr;
        QLabel *body = nullptr;
        bool self = false;
        int gapBefore = 12;
        int bubbleW = 0;
        int bubbleH = 0;
        int metaH = 0;
    };

    explicit MessagePaneFanLX(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("chatMessageHostFanLX"));
        setAttribute(Qt::WA_StyledBackground, true);
        setAutoFillBackground(true);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(chatThemeFanLX::canvasBg));
        setPalette(pal);
    }

    QSize sizeHint() const override { return QSize(480, 280); }
    QSize minimumSizeHint() const override { return QSize(0, 0); }

    void showEmptyFanLX(const QString &title, const QString &detail) {
        clearContentFanLX();
        emptyTitleFanLX = makePlainLabelFanLX(title, 16, 600, chatThemeFanLX::textSecondary, this);
        emptyDetailFanLX = makePlainLabelFanLX(detail, 13, 400, chatThemeFanLX::textMuted, this);
        emptyTitleFanLX->setAlignment(Qt::AlignCenter);
        emptyDetailFanLX->setAlignment(Qt::AlignCenter);
        // 父控件已经显示时，后创建的子控件默认是隐藏的，必须显式显示。
        emptyTitleFanLX->show();
        emptyDetailFanLX->show();
        syncFanLX();
    }

    void showMessagesFanLX(const QList<MessageItemFanLX> &items) {
        clearContentFanLX();
        QString previousSender;
        bool previousSelf = false;
        QString previousTime;
        bool hasPrevious = false;
        for (const auto &item : items) {
            const bool continued = hasPrevious && item.self == previousSelf && item.sender == previousSender;
            const bool repeatTime = continued && item.timeText == previousTime;
            const auto shownStatus = theme::shortStatus(item.statusText, item.failed);
            MessageRowFanLX row;
            row.self = item.self;
            row.gapBefore = !hasPrevious ? 0 : (continued ? 4 : 14);
            row.root = new QWidget(this);

            QString meta;
            if (!continued) {
                meta += spanFanLX(item.self ? chatThemeFanLX::accent : chatThemeFanLX::textSecondary, 600, item.sender);
                meta += QStringLiteral("&nbsp;&nbsp;");
                meta += spanFanLX(chatThemeFanLX::textMuted, 400,
                                  item.timeText.isEmpty() ? QStringLiteral("[原始时间未知]")
                                                          : chatClockFanLX(item.timeText));
            } else if (!repeatTime && !item.timeText.isEmpty()) {
                meta += spanFanLX(chatThemeFanLX::textMuted, 400, chatClockFanLX(item.timeText));
            }
            if (!shownStatus.isEmpty()) {
                if (!meta.isEmpty())
                    meta += QStringLiteral("&nbsp;&nbsp;");
                meta += spanFanLX(item.failed ? chatThemeFanLX::danger : chatThemeFanLX::textMuted,
                                  item.failed ? 600 : 400, shownStatus);
            }
            if (!meta.isEmpty()) {
                row.meta = new QLabel(row.root);
                row.meta->setTextFormat(Qt::RichText);
                row.meta->setText(meta);
                row.meta->setWordWrap(true);
                row.meta->setAlignment(item.self ? Qt::AlignRight | Qt::AlignTop : Qt::AlignLeft | Qt::AlignTop);
                row.meta->setStyleSheet(QStringLiteral("font-size:12px;background:transparent;border:none;"));
                if (!item.statusText.isEmpty())
                    row.meta->setToolTip(item.statusText);
            }

            row.bubble = new QFrame(row.root);
            row.bubble->setObjectName(QStringLiteral("chatBubbleFanLX"));
            row.bubble->setFrameShape(QFrame::NoFrame);
            row.bubble->setAttribute(Qt::WA_StyledBackground, true);
            const auto background = item.failed ? chatThemeFanLX::dangerSoft
                                                : (item.self ? chatThemeFanLX::selfBubble : chatThemeFanLX::peerBubble);
            const auto border = (item.self || item.failed)
                                    ? QStringLiteral("none")
                                    : QStringLiteral("1px solid %1").arg(chatThemeFanLX::border);
            // 参考 LINE / WhatsApp：自己的气泡用品牌色、白字，靠近发送方的一角收成小圆角，
            // 形成「尾巴」方向感；对方为白底细边框。
            const auto corners = item.self
                                     ? QStringLiteral("border-radius:16px;border-bottom-right-radius:4px;")
                                     : QStringLiteral("border-radius:16px;border-bottom-left-radius:4px;");
            row.bubble->setStyleSheet(QStringLiteral("QFrame#chatBubbleFanLX{background:%1;border:%2;%3}")
                                          .arg(item.self && !item.failed ? chatThemeFanLX::accent : background,
                                               border, corners));
            if (!item.statusText.isEmpty())
                row.bubble->setToolTip(item.statusText);

            row.body = new QLabel(item.body, row.bubble);
            row.body->setWordWrap(true);
            row.body->setTextFormat(Qt::PlainText);
            row.body->setTextInteractionFlags(Qt::TextSelectableByMouse);
            row.body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            row.body->setStyleSheet(QStringLiteral("color:%1;font-size:14px;background:transparent;border:none;")
                                        .arg(item.self && !item.failed ? QStringLiteral("#FFFFFF")
                                                                       : chatThemeFanLX::textPrimary));
            // 会话打开、历史返回和发送都发生在主窗口显示之后。这时新建的控件不会跟着父控件自动出现。
            row.root->show();
            if (row.meta)
                row.meta->show();
            row.bubble->show();
            row.body->show();
            rowsFanLX.push_back(row);
            previousSender = item.sender;
            previousTime = item.timeText;
            previousSelf = item.self;
            hasPrevious = true;
        }
        syncFanLX();
    }

  protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        syncFanLX();
    }

  private:
    static QLabel *makePlainLabelFanLX(const QString &text, int pixelSize, int weight, const QString &color,
                                      QWidget *parent) {
        auto *label = new QLabel(text, parent);
        label->setWordWrap(true);
        label->setTextFormat(Qt::PlainText);
        label->setStyleSheet(QStringLiteral("color:%1;font-size:%2px;font-weight:%3;background:transparent;")
                                 .arg(color)
                                 .arg(pixelSize)
                                 .arg(weight));
        return label;
    }

    void clearContentFanLX() {
        for (const auto &row : rowsFanLX)
            delete row.root;
        rowsFanLX.clear();
        delete emptyTitleFanLX;
        emptyTitleFanLX = nullptr;
        delete emptyDetailFanLX;
        emptyDetailFanLX = nullptr;
    }

    int measureFanLX(int paneWidth) {
        if (emptyTitleFanLX && emptyDetailFanLX) {
            const int inner = qMax(1, paneWidth - kSide * 2);
            emptyTitleFanLX->ensurePolished();
            emptyDetailFanLX->ensurePolished();
            return kSide * 2 + emptyTitleFanLX->heightForWidth(inner) + 8 + emptyDetailFanLX->heightForWidth(inner);
        }
        const int maxBubble = qMax(kPadX * 2 + 48, qMin(kCap, qMax(96, paneWidth - kSide * 2)));
        int content = kSide;
        for (auto &row : rowsFanLX) {
            int longest = 0;
            row.body->ensurePolished();
            const QFontMetrics metrics(row.body->font());
            for (const auto &line : row.body->text().split(QLatin1Char('\n')))
                longest = qMax(longest, metrics.horizontalAdvance(line));
            int bubbleW = qBound(kPadX * 2 + 24, longest + kPadX * 2, maxBubble);
            int metaH = 0;
            if (row.meta) {
                row.meta->ensurePolished();
                const int natural = qMin(maxBubble, qMax(bubbleW, row.meta->sizeHint().width()));
                bubbleW = qMax(bubbleW, natural);
                bubbleW = qMin(bubbleW, maxBubble);
                metaH = qMax(row.meta->fontMetrics().height(), row.meta->heightForWidth(bubbleW));
            }
            const int inner = qMax(1, bubbleW - kPadX * 2);
            const int textH = qMax(metrics.height(), row.body->heightForWidth(inner)) + 2;
            row.bubbleW = bubbleW;
            row.metaH = metaH;
            row.bubbleH = textH + kPadY * 2;
            content += row.gapBefore + metaH + (metaH > 0 ? 4 : 0) + row.bubbleH;
        }
        return content + kSide;
    }

    void arrangeFanLX(int paneWidth, int viewHeight, int contentH) {
        if (emptyTitleFanLX && emptyDetailFanLX) {
            const int inner = qMax(1, paneWidth - kSide * 2);
            const int titleH = emptyTitleFanLX->heightForWidth(inner);
            const int detailH = emptyDetailFanLX->heightForWidth(inner);
            const int block = titleH + 8 + detailH;
            const int y = qMax(kSide, (viewHeight - block) / 2);
            emptyTitleFanLX->setGeometry(kSide, y, inner, titleH);
            emptyDetailFanLX->setGeometry(kSide, y + titleH + 8, inner, detailH);
            return;
        }
        int y = kSide;
        if (viewHeight > contentH)
            y = viewHeight - contentH + kSide;
        for (auto &row : rowsFanLX) {
            const int block = row.gapBefore + row.metaH + (row.metaH > 0 ? 4 : 0) + row.bubbleH;
            row.root->setGeometry(0, y, paneWidth, block);
            int local = row.gapBefore;
            const int x = row.self ? qMax(kSide, paneWidth - kSide - row.bubbleW) : kSide;
            if (row.meta) {
                row.meta->setGeometry(x, local, row.bubbleW, row.metaH);
                local += row.metaH + 4;
            }
            row.bubble->setGeometry(x, local, row.bubbleW, row.bubbleH);
            row.body->setGeometry(kPadX, kPadY, qMax(1, row.bubbleW - kPadX * 2), qMax(1, row.bubbleH - kPadY * 2));
            y += block;
        }
    }

    void syncFanLX() {
        if (relayoutGuardFanLX || width() < 32)
            return;
        relayoutGuardFanLX = true;
        const int contentH = qMax(1, measureFanLX(width()));
        if (minimumHeight() != contentH)
            setMinimumHeight(contentH);
        arrangeFanLX(width(), qMax(height(), contentH), contentH);
        relayoutGuardFanLX = false;
    }

    QList<MessageRowFanLX> rowsFanLX;
    QLabel *emptyTitleFanLX = nullptr;
    QLabel *emptyDetailFanLX = nullptr;
    bool relayoutGuardFanLX = false;
};

} // namespace

MainWindowFanLX::MainWindowFanLX(ChatControllerFanLX *controller, DomainControllerFanLX *domain,
                                const QString &account, const QString &storageBackend, QWidget *parent)
    : QMainWindow(parent), controllerFanLX(controller), domainFanLX(domain), accountFanLX(account),
      storageBackendFanLX(storageBackend) {
    loadApplicationStyleFanLX();
    setWindowTitle(windowCaptionFanLX(QString(), QStringLiteral("QQ")));
    setWindowIcon(theme::appIcon());
    resize(1180, 760);
    // 逻辑像素。150% / 200% 下窗口仍能落到常见桌面里，具体下限由分栏最小宽度再抬高。
    setMinimumSize(760, 520);
    // 窗口底色与聊天页一致，避免菜单栏和页签之间露出另一块系统灰。
    auto chromePalette = palette();
    chromePalette.setColor(QPalette::Window, QColor(chatThemeFanLX::pageBg));
    setPalette(chromePalette);

    conversationsFanLX = new ConversationListModelFanLX(this);
    messagesFanLX = new MessageListModelFanLX(this);

    tabsFanLX = new QTabWidget(this);
    tabsFanLX->setObjectName(QStringLiteral("mainTabsFanLX"));
    tabsFanLX->setDocumentMode(true); // 去掉页签外围的原生立体框，不改变页签个数与切换
    tabsFanLX->tabBar()->setObjectName(QStringLiteral("mainTabBarFanLX"));
    tabsFanLX->tabBar()->setDrawBase(false);
    tabsFanLX->tabBar()->setExpanding(false); // 按文字宽度排开，不要拉成一排大按钮
    tabsFanLX->tabBar()->setElideMode(Qt::ElideNone);
    tabsFanLX->tabBar()->setUsesScrollButtons(true); // 窗口过窄时滚动，不截断「账号与服务」
    tabsFanLX->tabBar()->setStyleSheet(tabBarStyleFanLX());
    tabsFanLX->setStyleSheet(tabWidgetStyleFanLX());
    buildChatPageFanLX();
    buildDomainPagesFanLX();

    statusFanLX = new QLabel(this);
    statusFanLX->setWordWrap(true);
    statusFanLX->setContentsMargins(12, 4, 12, 6);
    statusFanLX->setStyleSheet(QStringLiteral("color:#1f2328; padding:2px;"));
    // 离开聊天页后，不再把「已打开某某会话」留在账号、好友或群页面底部。
    connect(tabsFanLX, &QTabWidget::currentChanged, this, [this](int index) {
        if (index != 0)
            clearChatConversationStatusFanLX(statusFanLX);
        syncShellFanLX(index);
    });
    buildShellFanLX();
    buildMenusFanLX();

    connect(controllerFanLX, &ChatControllerFanLX::conversationsChangedFanLX, this,
            &MainWindowFanLX::onConversationsChangedFanLX);
    connect(controllerFanLX, &ChatControllerFanLX::messagesChangedFanLX, this,
            &MainWindowFanLX::onMessagesChangedFanLX);
    connect(controllerFanLX, &ChatControllerFanLX::searchLoadedFanLX, this, [this] {
        onHistoryLoadedFanLX(0, controllerFanLX->searchPage().hasMore);
    });
    connect(controllerFanLX, &ChatControllerFanLX::stateChangedFanLX, this,
            &MainWindowFanLX::onStateChangedFanLX);
    connect(controllerFanLX, &ChatControllerFanLX::errorFanLX, this, &MainWindowFanLX::onErrorFanLX);
    connect(controllerFanLX, &ChatControllerFanLX::noticeFanLX, this, [this](const QString &detail) {
        // 提示（不是错误）：连接仍然成功，只是发生了需要用户知情的变化。
        // 这里不碰 serviceLabelFanLX——连接是好的，不能显示成"未连接"。
        statusFanLX->setText(detail);
        statusFanLX->setStyleSheet(QStringLiteral("color:#7a4b00; background:#fff4e5; "
                                                  "border:1px solid #ffd8a8; padding:4px;"));
    });
    connect(controllerFanLX, &ChatControllerFanLX::messageArrivedFanLX, this,
            &MainWindowFanLX::onMessageArrivedFanLX);
    connect(controllerFanLX, &ChatControllerFanLX::directoryFanLX, this, &MainWindowFanLX::onDirectoryFanLX);
    connect(controllerFanLX, &ChatControllerFanLX::busyChangedFanLX, this, [this](bool busy) {
        // 队列满：保留输入文本并提示繁忙，禁止静默丢弃。
        if (busy)
            statusFanLX->setText(QStringLiteral("发送队列繁忙：请稍后重试（输入内容已保留）"));
    });
    if (domainFanLX) {
        connect(domainFanLX, &DomainControllerFanLX::operationFinishedFanLX, this,
                &MainWindowFanLX::onDomainResultFanLX);
        // 当前服务一变，聊天连接就跟着换身份（QQ <-> 微信），列表里的好友与群也随之换分区。
        connect(domainFanLX, &DomainControllerFanLX::identityChangedFanLX, this,
                &MainWindowFanLX::onIdentityChangedFanLX);
    }

    onConversationsChangedFanLX();
    renderMessagesFanLX(); // 首屏即显示设计过的空状态，而不是消息区的默认占位文字
    refreshHeaderFanLX();
    if (domainFanLX)
        statusFanLX->setText(domainFanLX->loadSummary());
}

// 顶部提示条：用于"不是错误但必须让用户知道下一步做什么"的情况，
// 例如新注册的微X号还没有任何下级服务——此时聊天页是未连接状态，用户要去开通管理。
void MainWindowFanLX::showNoticeFanLX(const QString &text) {
    if (text.isEmpty())
        return;
    statusFanLX->setText(text);
    statusFanLX->setStyleSheet(QStringLiteral("color:#7a4b00; background:#fff4e5; "
                                              "border:1px solid #ffd8a8; padding:4px;"));
    // 需要用户去开通或登录时，服务行改成完整提示，不用平常那行副标题盖掉。
    setChatServiceStatusFanLX(serviceLabelFanLX, QStringLiteral("未连接：%1").arg(text), true);
}

// 页签 1：聊天。左会话面板 + 右聊天面板（标题区 / 消息区 / 工具条 / 输入区）。
// 只调整布局与外观：控件实例、信号槽连接与按钮行为与原来一一对应。
void MainWindowFanLX::buildChatPageFanLX() {
    // 聊天页根控件：页面级样式表只挂在这里，作用域限于聊天页。
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("chatPageFanLX"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setStyleSheet(chatPageStyleFanLX());

    // ---- 左侧：会话面板 ----
    auto *sidebar = new QFrame(page);
    sidebar->setObjectName(QStringLiteral("chatSidebarFanLX"));
    auto *sidebarTitle = new QLabel(QStringLiteral("会话"), sidebar);
    sidebarTitle->setObjectName(QStringLiteral("chatSidebarTitleFanLX"));

    conversationViewFanLX = new QListView(sidebar);
    conversationViewFanLX->setObjectName(QStringLiteral("chatConversationsFanLX"));
    conversationViewFanLX->setModel(conversationsFanLX);
    conversationViewFanLX->setUniformItemSizes(false);
    conversationViewFanLX->setMinimumWidth(240);
    conversationViewFanLX->setFrameShape(QFrame::NoFrame);
    conversationViewFanLX->setItemDelegate(new ConversationDelegateFanLX(conversationViewFanLX));
    conversationViewFanLX->setMouseTracking(true);
    conversationViewFanLX->viewport()->setAttribute(Qt::WA_Hover, true);
    conversationViewFanLX->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    conversationViewFanLX->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    conversationViewFanLX->setCursor(Qt::PointingHandCursor);

    emptyConversationsFanLX = new QLabel(QStringLiteral("还没有会话\n好友和群会出现在这里"), sidebar);
    emptyConversationsFanLX->setObjectName(QStringLiteral("chatEmptyFanLX"));
    emptyConversationsFanLX->setAlignment(Qt::AlignCenter);
    emptyConversationsFanLX->setWordWrap(true);
    emptyConversationsFanLX->hide();
    auto *sidebarHead = new QVBoxLayout();
    sidebarHead->setContentsMargins(22, 18, 22, 10);
    sidebarHead->addWidget(sidebarTitle);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 8);
    sidebarLayout->setSpacing(0);
    sidebarLayout->addLayout(sidebarHead);
    sidebarLayout->addWidget(emptyConversationsFanLX);
    sidebarLayout->addWidget(conversationViewFanLX, 1);

    // ---- 右侧：聊天面板 ----
    auto *panel = new QFrame(page);
    panel->setObjectName(QStringLiteral("chatPanelFanLX"));
    panel->setMinimumWidth(420);

    // 标题区：会话名称（主）+ 一行服务/账号副标题（次）。类型色标只留在左侧列表。
    auto *header = new QFrame(panel);
    header->setObjectName(QStringLiteral("chatHeaderFanLX"));
    auto *chatTitle = new QLabel(header);
    chatTitle->setObjectName(QStringLiteral("chatTitleFanLX"));
    chatTitle->setTextFormat(Qt::PlainText);
    chatTitle->setWordWrap(true); // 名称过长时换行，而不是把右侧面板撑宽
    chatTitle->setText(QStringLiteral("未选择会话"));
    // 统计行不再单独展示（存储、会话数、全局未读）。控件保留，避免改成员布局接口。
    headerFanLX = new QLabel(header);
    headerFanLX->setObjectName(QStringLiteral("chatMetaFanLX"));
    headerFanLX->setWordWrap(true);
    headerFanLX->setVisible(false);
    // 正常连接时这一行就是副标题；未连接 / 切换失败时换成完整提示。初始文案由 refreshHeader 填。
    serviceLabelFanLX = new QLabel(header);
    serviceLabelFanLX->setObjectName(QStringLiteral("chatServiceFanLX"));
    serviceLabelFanLX->setTextFormat(Qt::PlainText);
    serviceLabelFanLX->setWordWrap(true);
    serviceLabelFanLX->setVisible(false);
    auto *titleColumn = new QVBoxLayout();
    titleColumn->setContentsMargins(0, 0, 0, 0);
    titleColumn->setSpacing(2);
    titleColumn->addWidget(chatTitle);
    // headerFanLX 不进布局：账号与服务只占 serviceLabel 这一行，避免标题下空出一条。
    titleColumn->addWidget(serviceLabelFanLX);
    auto *historyButton = new QPushButton(QStringLiteral("聊天记录"), header);
    historyButton->setProperty("chatRole", QStringLiteral("secondary"));
    historyButton->setCursor(Qt::PointingHandCursor);
    historyButton->setFocusPolicy(Qt::StrongFocus);
    historyButton->setToolTip(QStringLiteral("按会话、发送者和关键词查询历史消息"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 14, 16, 12);
    headerLayout->setSpacing(12);
    headerLayout->addLayout(titleColumn, 1);
    headerLayout->addWidget(historyButton, 0, Qt::AlignTop);
    connect(historyButton, &QPushButton::clicked, this, &MainWindowFanLX::onSearchFanLX);

    // 消息区：圆角气泡放在滚动视口里。视口宽度跟着窗口走，气泡最宽 420。
    messageViewFanLX = new QScrollArea(panel);
    messageViewFanLX->setObjectName(QStringLiteral("chatMessages"));
    messageViewFanLX->setFrameShape(QFrame::NoFrame);
    messageViewFanLX->setWidgetResizable(true);
    messageViewFanLX->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    messageViewFanLX->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    if (auto *viewport = messageViewFanLX->viewport()) {
        viewport->setObjectName(QStringLiteral("chatMessageViewportFanLX"));
        viewport->setAttribute(Qt::WA_StyledBackground, true);
        viewport->setAutoFillBackground(true);
        auto pal = viewport->palette();
        pal.setColor(QPalette::Window, QColor(chatThemeFanLX::canvasBg));
        viewport->setPalette(pal);
    }
    messageViewFanLX->setWidget(new MessagePaneFanLX());

    // 工具条：查看更早 / 返回最新（左）、新消息提示（中）、重连（右）——都是次要操作
    earlierButtonFanLX = new QPushButton(QStringLiteral("查看更早"), panel);
    latestButtonFanLX = new QPushButton(QStringLiteral("返回最新"), panel);
    reconnectButtonFanLX = new QPushButton(QStringLiteral("重连"), panel);
    for (auto *button : {earlierButtonFanLX, latestButtonFanLX, reconnectButtonFanLX}) {
        button->setProperty("chatRole", QStringLiteral("secondary"));
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    earlierButtonFanLX->setEnabled(false);
    latestButtonFanLX->setEnabled(false);
    newMessageHintFanLX = new QLabel(panel);
    newMessageHintFanLX->setObjectName(QStringLiteral("chatNewMessageHintFanLX"));
    newMessageHintFanLX->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    newMessageHintFanLX->setVisible(false);

    auto *toolbar = new QFrame(panel);
    toolbar->setObjectName(QStringLiteral("chatToolbarFanLX"));
    auto *toolRow = new QHBoxLayout(toolbar);
    toolRow->setContentsMargins(20, 8, 20, 8);
    toolRow->setSpacing(8);
    toolRow->addWidget(earlierButtonFanLX);
    toolRow->addWidget(latestButtonFanLX);
    toolRow->addStretch(1);
    toolRow->addWidget(newMessageHintFanLX);
    toolRow->addStretch(1);
    toolRow->addWidget(reconnectButtonFanLX);

    // 输入区：输入框（主）+ 字节提示（辅）+ 发送按钮（主操作）
    editorFanLX = new QLineEdit(panel);
    editorFanLX->setObjectName(QStringLiteral("chatEditorFanLX"));
    editorFanLX->setPlaceholderText(QStringLiteral("输入中文消息后回车或点击发送"));
    // 协议上限是 UTF-8 字节数，而 QLineEdit::setMaxLength 按 UTF-16 单元计：
    // 中文一个字 1 个单元但 3 个字节，所以只靠 setMaxLength 会放行约 3 倍超限的内容。
    // 真正的裁剪在 onEditorChangedFanLX 里按字节做，这里只挡住极端粘贴。
    editorFanLX->setMaxLength(static_cast<int>(MessageCodecFanLX::maxText));
    editorFanLX->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    editorHintFanLX = new QLabel(panel);
    editorHintFanLX->setObjectName(QStringLiteral("chatEditorHintFanLX"));
    editorHintFanLX->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    editorHintFanLX->setText(QStringLiteral("%1 / %2 字节").arg(0).arg(static_cast<int>(MessageCodecFanLX::maxText)));
    sendButtonFanLX = new QPushButton(QStringLiteral("发送"), panel);
    sendButtonFanLX->setObjectName(QStringLiteral("chatSendButtonFanLX"));
    sendButtonFanLX->setCursor(Qt::PointingHandCursor);
    sendButtonFanLX->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *composer = new QFrame(panel);
    composer->setObjectName(QStringLiteral("chatComposerFanLX"));
    auto *editorRow = new QHBoxLayout(composer);
    editorRow->setContentsMargins(20, 14, 20, 16);
    editorRow->setSpacing(12);
    editorRow->addWidget(editorFanLX, 1);
    editorRow->addWidget(editorHintFanLX);
    editorRow->addWidget(sendButtonFanLX);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(1, 1, 1, 1); // 让出 1px 面板边框
    panelLayout->setSpacing(0);
    panelLayout->addWidget(header);
    panelLayout->addWidget(messageViewFanLX, 1);
    panelLayout->addWidget(toolbar);
    panelLayout->addWidget(composer);

    auto *splitter = new QSplitter(Qt::Horizontal, page);
    splitter->setObjectName(QStringLiteral("chatSplitterFanLX"));
    splitter->setHandleWidth(12);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(sidebar);
    splitter->addWidget(panel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 880});

    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(12, 12, 12, 12);
    pageLayout->setSpacing(0);
    pageLayout->addWidget(splitter);

    tabsFanLX->addTab(page, QStringLiteral("聊天"));

    connect(conversationViewFanLX, &QListView::clicked, this, &MainWindowFanLX::onConversationSelectedFanLX);
    connect(sendButtonFanLX, &QPushButton::clicked, this, &MainWindowFanLX::onSendFanLX);
    connect(editorFanLX, &QLineEdit::textChanged, this, &MainWindowFanLX::onEditorChangedFanLX);
    connect(editorFanLX, &QLineEdit::returnPressed, this, &MainWindowFanLX::onSendFanLX);
    connect(earlierButtonFanLX, &QPushButton::clicked, this, &MainWindowFanLX::onEarlierFanLX);
    connect(latestButtonFanLX, &QPushButton::clicked, this, &MainWindowFanLX::onLatestFanLX);
    connect(reconnectButtonFanLX, &QPushButton::clicked, this, &MainWindowFanLX::onReconnectFanLX);
}

// 页签 2~4：账号与服务 / 好友 / 群。领域层不可用时（只读恢复失败等）不建这些页签，
// 并在状态栏说明原因，而不是给出一个点了就报错的空界面。
void MainWindowFanLX::buildDomainPagesFanLX() {
    if (!domainFanLX || !domainFanLX->opened())
        return;
    accountPageFanLX = new AccountPageFanLX(domainFanLX, this);
    friendPageFanLX = new FriendPageFanLX(domainFanLX, this);
    groupPageFanLX = new GroupPageFanLX(domainFanLX, this);
    tabsFanLX->addTab(accountPageFanLX, QStringLiteral("账号与服务"));
    tabsFanLX->addTab(friendPageFanLX, QStringLiteral("好友"));
    tabsFanLX->addTab(groupPageFanLX, QStringLiteral("群"));
    // 好友动态（收到申请 / 被同意 / 被删除）：服务端经聊天通道推送。先对账本机好友清单，
    // 再刷新申请页——对方同意后好友列表立刻出现，不必重新登录。
    connect(controllerFanLX, &ChatControllerFanLX::friendNoticeFanLX, this, [this](const QString &detail) {
        domainFanLX->onFriendNoticeFanLX();
        friendPageFanLX->refreshRequestsFanLX();
        statusFanLX->setText(QStringLiteral("好友动态：") + detail);
    });
}

void MainWindowFanLX::buildMenusFanLX() {
    // 顶部菜单：聊天工具与领域维护都在界面上直接可达。
    // 菜单仍是 QMenuBar / QMenu，这里只统一外观。动作、快捷方式和创建条件不变。
    auto *bar = menuBar();
    bar->setObjectName(QStringLiteral("mainMenuBarFanLX"));
    bar->setNativeMenuBar(false); // 画在窗口里，局部样式才能生效（Windows 上本来也不是系统菜单栏）
    bar->setStyleSheet(menuBarStyleFanLX());
    const auto tuneMenu = [](QMenu *menu) {
        menu->setObjectName(QStringLiteral("mainMenuFanLX"));
        menu->setStyleSheet(menuPopupStyleFanLX());
    };

    auto *tools = bar->addMenu(QStringLiteral("工具"));
    tuneMenu(tools);
    // 聊天记录留在聊天标题栏的按钮上。菜单里再放一份会和那个按钮打开同一个窗口。
    auto *directoryAction =
        tools->addAction(QStringLiteral("刷新授权目录"), this, [this] { controllerFanLX->requestDirectory(); });
    directoryAction->setStatusTip(QStringLiteral(
        "向聊天服务器要当前服务可聊天的好友、群和临时会话额度，并补进左侧会话列表。不改好友页和群页。"));
    auto *conversationAction =
        tools->addAction(QStringLiteral("刷新会话列表"), this, [this] { controllerFanLX->requestConversations(); });
    conversationAction->setStatusTip(
        QStringLiteral("从本机聊天库重读会话摘要和未读数。不读取领域库，也不向服务器要授权目录。"));

    if (domainFanLX && domainFanLX->opened()) {
        auto *domainMenu = bar->addMenu(QStringLiteral("领域数据"));
        tuneMenu(domainMenu);
        auto *reloadDomain = domainMenu->addAction(QStringLiteral("重新读取好友与群列表"), this, [this] {
            emit domainFanLX->dataChangedFanLX();
            statusFanLX->setText(QStringLiteral("已重新读取内存中的领域数据（源文件在启动时已加载）"));
        });
        reloadDomain->setStatusTip(QStringLiteral(
            "把内存里已经加载的好友页和群页再画一遍。不读文件，也不向聊天服务器要授权目录。"));
        domainMenu->addAction(QStringLiteral("立即写回文件（收尾提交）"), this, [this] {
            const auto directory = domainFanLX->directory();
            domainFanLX->saveAndClose();
            statusFanLX->setText(QStringLiteral("已收尾提交并关闭领域库：%1。"
                                                "继续操作前请重新启动客户端。")
                                     .arg(directory));
        });
        domainMenu->addAction(QStringLiteral("查询缓存统计…"), this, [this] {
            QMessageBox::information(this, QStringLiteral("查询缓存统计"), domainFanLX->cacheSummary());
        });
        domainMenu->addAction(QStringLiteral("领域数据库位置…"), this, [this] {
            QMessageBox::information(this, QStringLiteral("领域数据库位置"),
                                     QStringLiteral("领域库目录：\n%1\n\n"
                                                    "其中 state.json 保存用户资料、开通、绑定、好友与群；"
                                                    "每次写操作都经候选事务原子提交，退出时再收尾提交一次。\n\n"
                                                    "加载结果：%2")
                                         .arg(domainFanLX->directory(), domainFanLX->loadSummary()));
        });
    }

    auto *help = bar->addMenu(QStringLiteral("帮助"));
    tuneMenu(help);
    help->addAction(QStringLiteral("功能与课设条款对照…"), this, &MainWindowFanLX::showAboutFanLX);
}

void MainWindowFanLX::buildShellFanLX() {
    tabsFanLX->tabBar()->hide();
    tabsFanLX->tabBar()->setFixedHeight(0);

    auto *rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("navRailFanLX"));
    rail->setFixedWidth(76);
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(8, 12, 8, 12);
    railLayout->setSpacing(4);
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    for (int i = 0; i < tabsFanLX->count(); ++i) {
        auto *button = new QToolButton(rail);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setToolTip(tabsFanLX->tabText(i));
        button->setAccessibleName(tabsFanLX->tabText(i));
        const auto full = tabsFanLX->tabText(i);
        button->setText(full == QStringLiteral("账号与服务") ? QStringLiteral("账号") : full);
        const char *iconName = "chat";
        if (i == 1)
            iconName = "account";
        else if (i == 2)
            iconName = "people";
        else if (i == 3)
            iconName = "group";
        button->setIcon(theme::lineIcon(QString::fromLatin1(iconName)));
        button->setIconSize(QSize(20, 20));
        button->setProperty("navRole", QStringLiteral("item"));
        button->setFixedSize(60, 58);
        group->addButton(button, i);
        railLayout->addWidget(button, 0, Qt::AlignHCenter);
        navButtonsFanLX.append(button);
    }
    railLayout->addStretch(1);
    connect(group, &QButtonGroup::idClicked, this, [this](int id) { selectPageFanLX(id); });

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("appHeaderFanLX"));
    header->setFixedHeight(52);
    moduleTitleFanLX = new QLabel(header);
    moduleTitleFanLX->setObjectName(QStringLiteral("moduleTitleFanLX"));
    identityLabelFanLX = new QLabel(header);
    identityLabelFanLX->setObjectName(QStringLiteral("identityChipFanLX"));
    identityLabelFanLX->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0);
    headerLayout->setSpacing(12);
    headerLayout->addWidget(moduleTitleFanLX, 1);
    headerLayout->addWidget(identityLabelFanLX);

    auto *body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(header);
    bodyLayout->addWidget(tabsFanLX, 1);
    bodyLayout->addWidget(statusFanLX);

    auto *shell = new QWidget(this);
    shell->setObjectName(QStringLiteral("appShellFanLX"));
    shell->setAttribute(Qt::WA_StyledBackground, true);
    auto *shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(rail);
    shellLayout->addWidget(body, 1);
    using namespace chatThemeFanLX;
    shell->setStyleSheet(QStringLiteral(
                              "QWidget#appShellFanLX{background:%1;}"
                              "QWidget#navRailFanLX{background:%2;border-right:1px solid %3;}"
                              "QFrame#appHeaderFanLX{background:%2;border-bottom:1px solid %3;}"
                              "QLabel#moduleTitleFanLX{font-size:16px;font-weight:600;color:%4;background:transparent;}"
                              "QLabel#identityChipFanLX{color:%5;font-size:12px;background:transparent;}"
                              "QToolButton[navRole=\"item\"]{background:transparent;color:%5;border:none;"
                              "border-radius:8px;padding:4px 2px;font-size:11px;}"
                              "QToolButton[navRole=\"item\"]:hover{background:#F3F5F8;color:%4;}"
                              "QToolButton[navRole=\"item\"]:checked{background:%6;color:%7;}"
                              "QToolButton[navRole=\"item\"]:focus{border:1px solid %7;}")
                              .arg(pageBg, panelBg, border, textPrimary, textSecondary, accentSoft, accent));
    setCentralWidget(shell);
    if (navButtonsFanLX.size() > 2 && friendPageFanLX) {
        friendNavBadgeFanLX = new QLabel(navButtonsFanLX.at(2));
        friendNavBadgeFanLX->setObjectName(QStringLiteral("navRequestBadgeFanLX"));
        friendNavBadgeFanLX->setAlignment(Qt::AlignCenter);
        friendNavBadgeFanLX->setFixedHeight(16);
        friendNavBadgeFanLX->setMinimumWidth(16);
        friendNavBadgeFanLX->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        friendNavBadgeFanLX->setStyleSheet(QStringLiteral(
            "background:#DC2626;color:#ffffff;border-radius:8px;font-size:10px;font-weight:700;padding:0 4px;"));
        friendNavBadgeFanLX->hide();
        connect(friendPageFanLX, &FriendPageFanLX::incomingRequestsChangedFanLX, this, [this](int count) {
            if (!friendNavBadgeFanLX)
                return;
            if (count <= 0) {
                friendNavBadgeFanLX->hide();
                return;
            }
            friendNavBadgeFanLX->setText(count > 99 ? QStringLiteral("99+") : QString::number(count));
            friendNavBadgeFanLX->adjustSize();
            friendNavBadgeFanLX->setFixedHeight(16);
            const auto *button = navButtonsFanLX.at(2);
            friendNavBadgeFanLX->move(qMax(0, button->width() - friendNavBadgeFanLX->width() - 4), 2);
            friendNavBadgeFanLX->show();
            friendNavBadgeFanLX->raise();
        });
        connect(friendPageFanLX, &FriendPageFanLX::chatWithPeerFanLX, this,
                [this](const QString &peer, bool temporary) {
                    if (peer.isEmpty() || !controllerFanLX)
                        return;
                    if (temporary) {
                        if (controllerFanLX->openTemporaryConversation(peer).isEmpty()) {
                            statusFanLX->setText(QStringLiteral("暂时不能打开和 %1 的会话").arg(peer));
                            return;
                        }
                    } else {
                        auto self = controllerFanLX->account();
                        if (self.isEmpty() && domainFanLX)
                            self = domainFanLX->currentAccountId();
                        if (self.isEmpty() || self == peer) {
                            statusFanLX->setText(QStringLiteral("暂时不能打开和 %1 的会话").arg(peer));
                            selectPageFanLX(0);
                            return;
                        }
                        const auto key = QString::fromStdString(chatDbFanLX::conversationKeyFanLX(
                            chatDbFanLX::ScopeFanLX::Direct, self.toStdString(), peer.toStdString()));
                        controllerFanLX->openConversation(key);
                    }
                    selectPageFanLX(0);
                    statusFanLX->setText(temporary ? QStringLiteral("已打开临时会话：%1").arg(peer)
                                                   : QStringLiteral("已打开好友会话：%1").arg(peer));
                });
    }
    if (!navButtonsFanLX.isEmpty())
        navButtonsFanLX.first()->setChecked(true);
    syncShellFanLX(tabsFanLX->currentIndex());
}

void MainWindowFanLX::syncShellFanLX(int index) {
    if (!tabsFanLX || index < 0 || index >= tabsFanLX->count())
        return;
    if (index < navButtonsFanLX.size() && navButtonsFanLX.at(index) && !navButtonsFanLX.at(index)->isChecked()) {
        QSignalBlocker blocker(navButtonsFanLX.at(index));
        navButtonsFanLX.at(index)->setChecked(true);
    }
    if (moduleTitleFanLX)
        moduleTitleFanLX->setText(tabsFanLX->tabText(index));
    if (!identityLabelFanLX)
        return;
    QString service;
    QString account = accountFanLX;
    if (domainFanLX && domainFanLX->opened()) {
        service = domainFanLX->currentService();
        const auto current = domainFanLX->currentAccountId();
        if (!current.isEmpty())
            account = current;
    } else if (controllerFanLX) {
        service = controllerFanLX->serviceDisplayName();
        if (!controllerFanLX->account().isEmpty())
            account = controllerFanLX->account();
    }
    identityLabelFanLX->setText(service.isEmpty() ? account : QStringLiteral("%1 · %2").arg(service, account));
}

void MainWindowFanLX::selectPageFanLX(int index) {
    if (tabsFanLX && index >= 0 && index < tabsFanLX->count())
        tabsFanLX->setCurrentIndex(index);
}

void MainWindowFanLX::closeEvent(QCloseEvent *event) {
    // 关窗即收尾提交：即使进程随后被结束，已确认的领域操作也已经在文件里。
    if (domainFanLX)
        domainFanLX->saveAndClose();
    QMainWindow::closeEvent(event);
}

void MainWindowFanLX::onDomainResultFanLX(bool ok, const QString &message) {
    statusFanLX->setText((ok ? QStringLiteral("✔ ") : QStringLiteral("✘ ")) + message);
}

// "当前服务"变化：聊天连接必须跟着换成该服务的身份。
// 三种服务不共享好友与群，因此这里不是"只换个标题"——连接本身要按 (服务, 号码) 重建，
// 服务端也只会返回该服务的好友与群目录。
void MainWindowFanLX::onIdentityChangedFanLX() {
    if (!domainFanLX)
        return;
    const auto service = domainFanLX->currentService();
    const auto accountId = domainFanLX->currentAccountId();
    if (!domainFanLX->currentServiceLoggedIn()) {
        // 当前服务已注销（或尚未登录）：聊天连接必须跟着断开，否则界面上还以该身份收发消息。
        // 重新登录后 identityChanged 再次发出，走下面的正常切换以重连。
        controllerFanLX->stop();
        const auto hint = QStringLiteral("%1 已注销或未登录：聊天已断开。请在「账号与服务」里重新登录或切换到已登录的服务")
                              .arg(service);
        setChatServiceStatusFanLX(serviceLabelFanLX,
                                  QStringLiteral("当前服务 %1（%2）｜ %3").arg(service, accountId, hint), true);
        statusFanLX->setText(hint);
        refreshHeaderFanLX();
        return;
    }
    if (!DomainControllerFanLX::chatCapable(service)) {
        // 现在只有微X（上层登录身份）会走到这里：QQ / 微信 / 微博 三种服务都有聊天通道。
        controllerFanLX->stop();
        const auto hint = service == QStringLiteral("微X")
                              ? QStringLiteral("微X 是上层登录身份，没有聊天通道："
                                               "请在「账号与服务」里开通并切换到 QQ / 微信 / 微博")
                              : QStringLiteral("%1 不提供聊天：请在「账号与服务」里切换到 QQ / 微信 / 微博")
                                    .arg(service);
        setChatServiceStatusFanLX(serviceLabelFanLX,
                                  QStringLiteral("当前服务 %1（%2）｜ %3").arg(service, accountId, hint), true);
        statusFanLX->setText(hint);
        refreshHeaderFanLX();
        return;
    }
    QString detail;
    auto switched = controllerFanLX->switchIdentity(service, accountId, &detail);
    if (!switched) {
        // 领域层刚开通/刚登录这个服务，而**服务端**的账号库里还没有它（认证查的是服务端那份）。
        // 这里把本机状态登记一次再重试连接——这样"在窗口里开通 QQ 后聊天自动连上"，
        // 不需要退出重进，更不需要重启服务端（服务端每次认证都读穿持久化存储）。
        QString why;
        if (domainFanLX->pushAccountToServer(&why)) {
            switched = controllerFanLX->switchIdentity(service, accountId, &detail);
        } else {
            detail = QStringLiteral("%1（服务端登记失败：%2）").arg(detail, why);
        }
    }
    if (!switched) {
        setChatServiceStatusFanLX(serviceLabelFanLX,
                                  QStringLiteral("当前服务 %1（%2）｜ 聊天未连接：%3")
                                      .arg(service, accountId, detail),
                                  true);
        statusFanLX->setText(QStringLiteral("聊天未连接：") + detail);
        refreshHeaderFanLX();
        return;
    }
    // 切换成功：回到一行副标题。好友/群的颜色标志留在左侧列表，这里不再解释。
    statusFanLX->setText(QStringLiteral("聊天已切换到 %1（%2）：好友与群按服务分别存放")
                             .arg(service, accountId));
    setChatServiceStatusFanLX(serviceLabelFanLX, chatSubtitleFanLX(service, accountId), false);
    refreshHeaderFanLX();
}

void MainWindowFanLX::showAboutFanLX() {
    QMessageBox::information(
        this, QStringLiteral("功能与课设条款对照"),
        QStringLiteral(
            "① 用户基本信息：微X号、昵称、出生时间、T龄（号码申请时间）、所在地、好友列表、群列表\n"
            "   —— 账号与服务页签。\n"
            "② 好友管理：添加 / 修改备注标签 / 删除 / 查询，以及同服务共同好友、跨服务推荐与共同好友\n"
            "   —— 好友页签；好友按服务分别存放（QQ 的朋友不会出现在微信列表里）。\n"
            "③ 群管理：群号 1001-1006，加入群（QQ 申请-审批 / 微信推荐-确认）、退出群、挨 T、"
            "查询群成员；QQ 群支持子群与管理員制度，微信群只有群主是特权账号\n"
            "   —— 群页签；QQ 群与微信群各有自己的命名空间。\n"
            "④ 开通管理：微X 是上层账号，登录之后在页签里自主开通 QQ / 微信 / 微博 三种服务\n"
            "   —— 账号与服务页签；注册入口在登录界面，注册时不捆绑任何服务账号。\n"
            "⑤ 登录管理：登微X 后，本人已开通的服务简单确认即自动登录 —— "
            "登录界面的确认勾选与账号与服务页签。\n"
            "⑥ 展示要求：启动时从文件加载、写操作即时写回、关窗收尾提交；群管理特色可动态切换且成员数据"
            "不受伤害；聊天使用 TCP 长度帧（中心转发），会话列表用图标区分好友与群\n"
            "   —— 领域数据菜单、群页签与聊天页签。"));
}

void MainWindowFanLX::refreshHeaderFanLX() {
    storageBackendFanLX = controllerFanLX->storageBackend();
    // 标题栏与页头给用户看的是中文服务名（QQ / 微信 / 微博），控制器内部仍用存储名。
    const auto chatService = controllerFanLX->serviceDisplayName();
    // 聊天身份优先用当前连接的账号（切换微信等服务后号码可能与微X号不同）。
    const auto chatAccount =
        controllerFanLX->account().isEmpty() ? accountFanLX : controllerFanLX->account();
    // 存储后端、会话总数、全局未读不再放进标题。未读在左侧角标，且不是当前会话的未读数。
    const auto subtitle = chatSubtitleFanLX(chatService, chatAccount);
    headerFanLX->clear();
    headerFanLX->setVisible(false);
    // 服务行正在显示未连接 / 切换失败等提示时，不要用副标题把它盖掉。
    if (serviceLabelFanLX && !serviceLabelFanLX->property("alertFanLX").toBool())
        setChatServiceStatusFanLX(serviceLabelFanLX, subtitle, false);
    const auto current = controllerFanLX->currentConversation();
    // 聊天标题和窗口标题用同一句人话：好友名或「群聊 1001」，不带 [好友] 和会话键。
    QString titleText = QStringLiteral("未选择会话");
    if (!current.isEmpty()) {
        QString itemTitle;
        bool group = controllerFanLX->currentConversationIsGroup();
        for (const auto &item : controllerFanLX->conversations()) {
            if (item.key == current) {
                itemTitle = item.title;
                group = item.group;
                break;
            }
        }
        titleText = chatConversationTitleFanLX(current, chatAccount, group, itemTitle);
    }
    if (auto *chatTitle = findChild<QLabel *>(QStringLiteral("chatTitleFanLX")))
        chatTitle->setText(titleText);
    setWindowTitle(windowCaptionFanLX(current.isEmpty() ? QString() : titleText, chatService));
    if (tabsFanLX)
        syncShellFanLX(tabsFanLX->currentIndex());
}

void MainWindowFanLX::onConversationsChangedFanLX() {
    conversationsFanLX->setItems(controllerFanLX->conversations());
    syncConversationSelectionFanLX(conversationViewFanLX, conversationsFanLX,
                                   controllerFanLX->currentConversation());
    const bool empty = conversationsFanLX->rowCount() == 0;
    if (emptyConversationsFanLX)
        emptyConversationsFanLX->setVisible(empty);
    if (conversationViewFanLX)
        conversationViewFanLX->setVisible(!empty);
    refreshHeaderFanLX();
}

void MainWindowFanLX::onMessagesChangedFanLX() {
    renderMessagesFanLX();
    syncConversationSelectionFanLX(conversationViewFanLX, conversationsFanLX,
                                   controllerFanLX->currentConversation());
    earlierButtonFanLX->setEnabled(controllerFanLX->historyHasMore());
    latestButtonFanLX->setEnabled(!controllerFanLX->currentConversation().isEmpty());
    refreshHeaderFanLX();
}

// 消息区渲染：自己的消息靠右、浅蓝圆角；收到的靠左、白底圆角；失败用浅红。
// 短状态只改展示。正文是纯文本，用户内容里的 < 和 % 按原样显示。
void MainWindowFanLX::renderMessagesFanLX() {
    auto *pane = static_cast<MessagePaneFanLX *>(messageViewFanLX->widget());
    if (!pane)
        return;
    const auto *bar = messageViewFanLX->verticalScrollBar();
    const int saved = bar ? bar->value() : 0;
    const bool follow = controllerFanLX->followingLatest() && !hasPendingNewMessageFanLX;
    const auto &items = controllerFanLX->messages();
    if (items.isEmpty()) {
        const bool noConversation = controllerFanLX->currentConversation().isEmpty();
        pane->showEmptyFanLX(noConversation ? QStringLiteral("选择左侧会话开始聊天") : QStringLiteral("暂无消息"),
                             noConversation ? QStringLiteral("单聊与群聊都在左侧列表中，点击任意一项即可打开。")
                                           : QStringLiteral("当前会话没有记录，或历史尚未加载完成。"));
    } else {
        pane->showMessagesFanLX(items);
    }
    const int generation = messageViewFanLX->property("fanlxScrollGen").toInt() + 1;
    messageViewFanLX->setProperty("fanlxScrollGen", generation);
    const auto applyScroll = [view = messageViewFanLX, saved, follow, generation] {
        if (!view || view->property("fanlxScrollGen").toInt() != generation)
            return;
        auto *scrollBar = view->verticalScrollBar();
        if (!scrollBar)
            return;
        scrollBar->setValue(follow ? scrollBar->maximum() : qMin(saved, scrollBar->maximum()));
    };
    applyScroll();
    QTimer::singleShot(0, messageViewFanLX, applyScroll);
}

void MainWindowFanLX::onConversationSelectedFanLX(const QModelIndex &index) {
    if (!index.isValid())
        return;
    const auto key = conversationsFanLX->keyAt(index.row());
    controllerFanLX->openConversation(key);
    hasPendingNewMessageFanLX = false;
    newMessageHintFanLX->setVisible(false);
    // 这里不能 markRead：openConversation 刚清空消息并异步请求历史，messages() 还是空的，
    // 序号只能读到 0。已读要等当前会话的最新一页真正返回后再提交。
    // 打开会话时明确说出这是群还是好友（与列表里的标志同源）。
    // 只在聊天页显示；人已经离开聊天页时，不把这句普通提示写进底部状态栏。
    const auto kind = controllerFanLX->currentConversationKindText();
    if (!kind.isEmpty() && tabsFanLX && tabsFanLX->currentIndex() == 0)
        statusFanLX->setText(QStringLiteral("已打开%1会话：%2").arg(kind, key));
    if (historyDialogFanLX)
        historyDialogFanLX->setCurrentConversation(key);
}

// 按 UTF-8 **字节数**把输入裁剪到单条消息上限，并显示已用字节。
// 必须自己算字节：Qt 的 setMaxLength 按 UTF-16 单元计，而中文一个字 3 字节，
// 只靠 setMaxLength 会放行约 3 倍超限的内容，最后在发送时被拒。
void MainWindowFanLX::onEditorChangedFanLX(const QString &text) {
    const auto limit = static_cast<int>(MessageCodecFanLX::maxText);
    const auto codePoints = text.toUcs4();
    QList<char32_t> kept;
    int used = 0;
    bool truncated = false;
    for (const char32_t code : codePoints) {
        const auto width = QString::fromUcs4(&code, 1).toUtf8().size();
        if (used + width > limit) {
            truncated = true;
            break;
        }
        kept.append(code);
        used += width;
    }
    if (truncated) {
        // setText 会再次触发本槽：屏蔽信号避免递归。
        QSignalBlocker blocker(editorFanLX);
        editorFanLX->setText(QString::fromUcs4(kept.constData(), kept.size()));
    }
    editorHintFanLX->setText(QStringLiteral("%1 / %2 字节").arg(used).arg(limit));
    if (truncated)
        statusFanLX->setText(QStringLiteral("已达单条消息上限 %1 字节（中文一个字约 3 字节），超出部分未输入")
                                 .arg(limit));
}

void MainWindowFanLX::onSendFanLX() {
    const auto text = editorFanLX->text();
    if (text.trimmed().isEmpty())
        return;
    if (domainFanLX && !DomainControllerFanLX::chatCapable(domainFanLX->currentService())) {
        // 没有聊天通道时不假装发送：明确告诉用户该切到哪个服务。
        statusFanLX->setText(QStringLiteral("当前服务 %1 没有聊天通道：请先开通并切换到 QQ / 微信 / 微博（输入内容已保留）")
                                 .arg(domainFanLX->currentService()));
        return;
    }
    const auto key = controllerFanLX->currentConversation();
    if (key.isEmpty()) {
        statusFanLX->setText(QStringLiteral("请先在左侧选择一个会话（单聊或群聊）"));
        return;
    }
    const auto sequence = controllerFanLX->messages().isEmpty() ? 0 : controllerFanLX->messages().last().sequence;
    if (!controllerFanLX->sendText(key, sequence, text)) {
        // 失败或繁忙都不清空输入：用户可选择重试或复制文本。
        // 控制器给出具体原因时优先显示它，避免被"繁忙或存储不可用"这类通用文案覆盖。
        const auto reason = controllerFanLX->lastRejection();
        statusFanLX->setText(reason.isEmpty()
                                 ? QStringLiteral("消息未提交（繁忙或存储不可用），输入内容已保留")
                                 : reason + QStringLiteral("（输入内容已保留）"));
        return;
    }
    editorFanLX->clear();
    hasPendingNewMessageFanLX = false;
    newMessageHintFanLX->setVisible(false);
    statusFanLX->setText(QStringLiteral("已提交保存请求：结果将在消息列表中更新"));
}

void MainWindowFanLX::onEarlierFanLX() {
    if (!controllerFanLX->requestEarlier())
        statusFanLX->setText(QStringLiteral("没有更早的记录，或已有查询在进行"));
}

void MainWindowFanLX::onLatestFanLX() {
    controllerFanLX->requestLatest();
    hasPendingNewMessageFanLX = false;
    newMessageHintFanLX->setVisible(false);
}

void MainWindowFanLX::onReconnectFanLX() {
    controllerFanLX->reconnect();
    statusFanLX->setText(QStringLiteral("已请求重连；未发送的消息会在连接恢复后自动补发"));
}

void MainWindowFanLX::onSearchFanLX() {
    if (!historyDialogFanLX) {
        historyDialogFanLX = new HistoryDialogFanLX(this);
        connect(historyDialogFanLX, &HistoryDialogFanLX::searchRequestedFanLX, this,
                [this] { showHistoryResultsFanLX(); });
        connect(historyDialogFanLX, &HistoryDialogFanLX::cancelRequestedFanLX,
                controllerFanLX, &ChatControllerFanLX::cancelSearch);
        connect(historyDialogFanLX, &HistoryDialogFanLX::earlierRequestedFanLX, this, [this] {
            historyDialogFanLX->setBusy(true);
            historyDialogFanLX->setSummary(QStringLiteral("查询中 · 正在加载下一页"));
            if (!controllerFanLX->searchEarlier()) {
                // 请求没有发出，上一页仍然有效，不能把它画成查询失败或零结果。
                historyDialogFanLX->setBusy(false);
                historyDialogFanLX->setSummary(QStringLiteral("下一页未能提交，请重试"));
            }
        });
    }
    QStringList keys;
    for (const auto &item : controllerFanLX->conversations())
        keys << item.key;
    historyDialogFanLX->setConversations(keys);
    // 没有当前会话时也要落到「所有会话」，不能沿用上一次打开时的选择。
    historyDialogFanLX->setCurrentConversation(controllerFanLX->currentConversation());
    historyDialogFanLX->show();
    historyDialogFanLX->raise();
}

void MainWindowFanLX::showHistoryResultsFanLX() {
    if (!historyDialogFanLX)
        return;
    const auto error = historyDialogFanLX->filterErrorFanLX();
    if (!error.isEmpty()) {
        // 校验失败只提示，不把上一次的结果清成「没有记录」。
        historyDialogFanLX->setSummary(error);
        return;
    }
    const auto key = historyDialogFanLX->conversationKey();
    const auto sender = historyDialogFanLX->senderFilter();
    const auto keyword = historyDialogFanLX->keyword();
    const auto from = historyDialogFanLX->fromText();
    const auto to = historyDialogFanLX->toText();
    const auto limit = historyDialogFanLX->pageSize();
    QString note = QStringLiteral("查询中 · 每页 %1 条（上限 200），结果按服务器时间倒序").arg(limit);
    if (historyDialogFanLX->useTimeRange())
        note += QStringLiteral("；时间范围 %1 ~ %2").arg(from, to);
    if (key.isEmpty())
        note += QStringLiteral("；当前为跨会话检索");
    historyDialogFanLX->setBusy(true);
    historyDialogFanLX->setSummary(note);
    if (!controllerFanLX->searchHistory(key, sender, keyword, limit, from, to))
        historyDialogFanLX->setQueryFailed(QStringLiteral("查询未能提交：界面繁忙或聊天库不可用"));
}

void MainWindowFanLX::onHistoryLoadedFanLX(qint64 queryId, bool hasMore) {
    (void)queryId;
    // 控制器已按 queryId 丢掉过期结果。这里再挡住取消或重置之后迟到的信号。
    if (!historyDialogFanLX || !historyDialogFanLX->isSearchInFlight())
        return;
    QList<HistoryResultRowFanLX> rows;
    for (const auto &item : controllerFanLX->searchPage().messages) {
        HistoryResultRowFanLX row;
        row.serverTime = item.acceptedAtUtc.empty() ? QString() : QString::fromStdString(item.acceptedAtUtc);
        row.sender = QString::fromStdString(item.sender);
        row.conversationKey = QString::fromStdString(item.conversationKey);
        row.body = QString::fromStdString(item.body);
        rows.append(row);
    }
    historyDialogFanLX->setResultRows(rows);
    historyDialogFanLX->setBusy(false);
    historyDialogFanLX->setHasMore(hasMore);
    if (rows.isEmpty()) {
        historyDialogFanLX->setSummary(QStringLiteral("没有找到匹配的聊天记录。可以放宽筛选条件后再查询。"));
        return;
    }
    historyDialogFanLX->setSummary(QStringLiteral("本页 %1 条%2")
                                       .arg(rows.size())
                                       .arg(hasMore ? QStringLiteral("，仍有更早的记录，可查看下一页")
                                                    : QStringLiteral("，已到最后一页")));
}

void MainWindowFanLX::onDirectoryFanLX(const QList<QString> &friends, const QList<QString> &groups) {
    friendsFanLX = friends;
    groupsFanLX = groups;
    // 目录只展示服务端授权结果；本地不推断群资格。服务端按"当前服务"给出目录，
    // 因此这里的数量就是该服务的好友与群，不会混入别的服务。
    statusFanLX->setText(QStringLiteral("服务器授权目录（%1）：好友 %2 个，群 %3 个")
                             .arg(controllerFanLX->serviceDisplayName())
                             .arg(friends.size())
                             .arg(groups.size()));
}

void MainWindowFanLX::onStateChangedFanLX(ConnectionStateFanLX state, const QString &detail) {
    const auto text = [state] {
        switch (state) {
        case ConnectionStateFanLX::Online: return QStringLiteral("在线");
        case ConnectionStateFanLX::Connecting: return QStringLiteral("连接中");
        case ConnectionStateFanLX::Reconnecting: return QStringLiteral("重连中");
        case ConnectionStateFanLX::Disconnected: return QStringLiteral("已断开");
        }
        return QStringLiteral("未知");
    }();
    statusFanLX->setText(QStringLiteral("连接状态：%1（%2）｜存储 %3")
                             .arg(text, detail, storageBackendFanLX));
    refreshHeaderFanLX();
}

void MainWindowFanLX::onErrorFanLX(const QString &detail) {
    statusFanLX->setText(QStringLiteral("错误：") + detail);
    if (historyDialogFanLX && historyDialogFanLX->isVisible())
        historyDialogFanLX->setQueryFailed(detail);
}

void MainWindowFanLX::onMessageArrivedFanLX(const QString &conversationKey, qint64 sequence) {
    (void)sequence;
    // 查看旧记录时不强制跳到底部：只提示"有新消息"。
    if (conversationKey != controllerFanLX->currentConversation())
        return;
    if (controllerFanLX->followingLatest()) return;
    hasPendingNewMessageFanLX = true;
    newMessageHintFanLX->setText(QStringLiteral("有新消息（点击「返回最新」查看）"));
    newMessageHintFanLX->setVisible(true);
}

} // namespace fanlxGui

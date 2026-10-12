#include "gui/MainWindowFanLX.h"
#include "gui/AccountPageFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/FriendPageFanLX.h"
#include "gui/GroupPageFanLX.h"
#include "gui/HistoryDialogFanLX.h"
#include "network/MessageCodecFanLX.h"
#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>
#include <QDateTime>
#include <QScrollBar>
#include <QTextCursor>
#include <algorithm>

namespace fanlxGui {

namespace {

QString escapeHtmlFanLX(const QString &text) {
    auto escaped = text.toHtmlEscaped();
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return escaped;
}

// ---------- 聊天页视觉规范（只作用于聊天页，不影响账号 / 好友 / 群页签） ----------
// 统一在这里定义颜色，样式表与消息 HTML、会话列表绘制共用同一套取值。
namespace chatThemeFanLX {
const QString pageBg = QStringLiteral("#f3f5f8");      // 页面底色：柔和浅灰
const QString panelBg = QStringLiteral("#ffffff");     // 面板：白色
const QString border = QStringLiteral("#e3e7ed");      // 面板边框 / 分隔线
const QString canvasBg = QStringLiteral("#f7f9fb");    // 消息区底色（略深于面板，衬托白色气泡）
const QString textPrimary = QStringLiteral("#1f2329"); // 正文
const QString textSecondary = QStringLiteral("#5b6472");
const QString textMuted = QStringLiteral("#8a94a3");   // 时间、状态等辅助信息
const QString accent = QStringLiteral("#2f6fed");      // 克制的主色蓝
const QString accentHover = QStringLiteral("#2961d6");
const QString accentPressed = QStringLiteral("#2253ba");
const QString accentSoft = QStringLiteral("#eaf1fe");  // 选中 / 提示的浅蓝底
const QString selfBubble = QStringLiteral("#dcebff");  // 自己发送的气泡
const QString peerBubble = QStringLiteral("#ffffff");  // 收到的气泡
const QString danger = QStringLiteral("#d93025");      // 发送失败
const QString dangerSoft = QStringLiteral("#fdeeee");
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
               "QListView#chatConversationsFanLX{background:transparent;border:none;outline:none;}"
               // 聊天标题区
               "QFrame#chatHeaderFanLX{background:transparent;border:none;border-bottom:1px solid %3;"
               "border-top-left-radius:12px;border-top-right-radius:12px;}"
               "QLabel#chatTitleFanLX{font-size:17px;font-weight:600;color:%4;}"
               "QLabel#chatMetaFanLX{font-size:12px;color:%5;}"
               "QLabel#chatServiceFanLX{font-size:12px;color:%6;}"
               // 消息区
               "QTextBrowser#chatMessages{background:%7;border:none;}"
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

// 会话列表行：两行布局（"[群] 1001" 标题 + 最后一条消息摘要）与未读角标。
// 标题文字与颜色完全来自模型（DisplayRole 的类型前缀 + ForegroundRole 的类型颜色），
// 因此 [好友] 蓝 / [群] 紫 / [临时] 橙 的含义与原来一致，只是排版更舒展。
class ConversationDelegateFanLX : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        (void)index;
        const auto title = QFontMetrics(titleFont(option.font));
        const auto sub = QFontMetrics(subtitleFont(option.font));
        return {option.rect.width(), kPadY * 2 + title.height() + kLineGap + sub.height() + kRowGap};
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

        // 未读角标（右上），数量为 0 时不画
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
            const QRect badge(content.right() - badgeWidth, content.top() + (titleM.height() - h) / 2, badgeWidth, h);
            QPainterPath pill;
            pill.addRoundedRect(badge, h / 2.0, h / 2.0);
            painter->fillPath(pill, QColor(QStringLiteral("#e5484d")));
            painter->setFont(badgeF);
            painter->setPen(Qt::white);
            painter->drawText(badge, Qt::AlignCenter, text);
            badgeWidth += 8;
        }

        // 标题：模型给出的"[类型] 名称"，用模型给出的类型颜色
        const auto titleText = index.data(Qt::DisplayRole).toString();
        const auto brush = index.data(Qt::ForegroundRole).value<QBrush>();
        const QColor titleColor = brush.style() == Qt::NoBrush ? QColor(textPrimary) : brush.color();
        const QRect titleRect(content.left(), content.top(), content.width() - badgeWidth, titleM.height());
        painter->setFont(titleF);
        painter->setPen(titleColor);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          titleM.elidedText(titleText, Qt::ElideRight, titleRect.width()));

        // 摘要：最后一条消息（没有时给出柔和的占位文字）
        auto subtitle = index.data(ConversationListModelFanLX::SubtitleRoleFanLX).toString();
        subtitle.replace(QLatin1Char('\n'), QLatin1Char(' '));
        const bool emptySubtitle = subtitle.trimmed().isEmpty();
        const QRect subRect(content.left(), titleRect.bottom() + 1 + kLineGap, content.width(), subM.height());
        painter->setFont(subF);
        painter->setPen(QColor(emptySubtitle ? QStringLiteral("#b4bcc7") : textMuted));
        painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter,
                          subM.elidedText(emptySubtitle ? QStringLiteral("暂无消息") : subtitle, Qt::ElideRight,
                                          subRect.width()));
        painter->restore();
    }

  private:
    static constexpr int kMarginX = 8;  // 行卡片与列表边缘的距离
    static constexpr int kPadX = 14;    // 行内左右留白
    static constexpr int kPadY = 12;    // 行内上下留白
    static constexpr int kLineGap = 4;  // 标题与摘要的间距
    static constexpr int kRowGap = 4;   // 行与行之间的间距

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

} // namespace

MainWindowFanLX::MainWindowFanLX(ChatControllerFanLX *controller, DomainControllerFanLX *domain,
                                const QString &account, const QString &storageBackend, QWidget *parent)
    : QMainWindow(parent), controllerFanLX(controller), domainFanLX(domain), accountFanLX(account),
      storageBackendFanLX(storageBackend) {
    setWindowTitle(QStringLiteral("樊陆旭模拟即时通讯系统 — QQ 客户端"));
    resize(1180, 760);

    conversationsFanLX = new ConversationListModelFanLX(this);
    messagesFanLX = new MessageListModelFanLX(this);

    tabsFanLX = new QTabWidget(this);
    buildChatPageFanLX();
    buildDomainPagesFanLX();

    statusFanLX = new QLabel(this);
    statusFanLX->setWordWrap(true);
    statusFanLX->setStyleSheet(QStringLiteral("color:#1f2328; padding:2px;"));

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(tabsFanLX, 1);
    layout->addWidget(statusFanLX);
    setCentralWidget(central);

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
    serviceLabelFanLX->setText(QStringLiteral("未连接：%1").arg(text));
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

    auto *sidebarHead = new QVBoxLayout();
    sidebarHead->setContentsMargins(22, 18, 22, 10);
    sidebarHead->addWidget(sidebarTitle);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 8);
    sidebarLayout->setSpacing(0);
    sidebarLayout->addLayout(sidebarHead);
    sidebarLayout->addWidget(conversationViewFanLX, 1);

    // ---- 右侧：聊天面板 ----
    auto *panel = new QFrame(page);
    panel->setObjectName(QStringLiteral("chatPanelFanLX"));
    panel->setMinimumWidth(420);

    // 标题区：当前会话（主）+ 账号与服务信息（次）
    auto *header = new QFrame(panel);
    header->setObjectName(QStringLiteral("chatHeaderFanLX"));
    auto *chatTitle = new QLabel(header);
    chatTitle->setObjectName(QStringLiteral("chatTitleFanLX"));
    chatTitle->setTextFormat(Qt::RichText);
    chatTitle->setWordWrap(true); // 名称过长时换行，而不是把右侧面板撑宽
    chatTitle->setText(QStringLiteral("未选择会话"));
    headerFanLX = new QLabel(header);
    headerFanLX->setObjectName(QStringLiteral("chatMetaFanLX"));
    headerFanLX->setWordWrap(true);
    // 聊天页顶部固定显示：本连接属于哪个服务，以及列表里两种标志的含义。
    serviceLabelFanLX = new QLabel(header);
    serviceLabelFanLX->setObjectName(QStringLiteral("chatServiceFanLX"));
    serviceLabelFanLX->setWordWrap(true);
    serviceLabelFanLX->setText(
        QStringLiteral("会话列表标志：<span style='color:#0b6bcb'>[好友]</span> 一对一的联系人 ｜ "
                       "<span style='color:#7b1fa2'>[群]</span> 群聊；两种会话在列表里用不同颜色区分。"));
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(24, 16, 24, 14);
    headerLayout->setSpacing(4);
    headerLayout->addWidget(chatTitle);
    headerLayout->addWidget(headerFanLX);
    headerLayout->addWidget(serviceLabelFanLX);

    // 消息区
    messageViewFanLX = new QTextBrowser(panel);
    messageViewFanLX->setObjectName(QStringLiteral("chatMessages"));
    messageViewFanLX->setOpenExternalLinks(false);
    messageViewFanLX->setPlaceholderText(QStringLiteral("选择左侧会话开始聊天"));
    messageViewFanLX->setFrameShape(QFrame::NoFrame);
    messageViewFanLX->document()->setDocumentMargin(20);
    messageViewFanLX->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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
    auto *tools = menuBar()->addMenu(QStringLiteral("工具"));
    tools->addAction(QStringLiteral("聊天记录查询…"), this, &MainWindowFanLX::onSearchFanLX);
    tools->addAction(QStringLiteral("刷新授权目录"), this, [this] { controllerFanLX->requestDirectory(); });
    tools->addAction(QStringLiteral("刷新会话列表"), this, [this] { controllerFanLX->requestConversations(); });

    if (domainFanLX && domainFanLX->opened()) {
        auto *domainMenu = menuBar()->addMenu(QStringLiteral("领域数据"));
        domainMenu->addAction(QStringLiteral("重新读取好友与群列表"), this, [this] {
            emit domainFanLX->dataChangedFanLX();
            statusFanLX->setText(QStringLiteral("已重新读取内存中的领域数据（源文件在启动时已加载）"));
        });
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

    auto *help = menuBar()->addMenu(QStringLiteral("帮助"));
    help->addAction(QStringLiteral("功能与课设条款对照…"), this, &MainWindowFanLX::showAboutFanLX);
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
        serviceLabelFanLX->setText(QStringLiteral("当前服务 %1（%2）｜ %3").arg(service, accountId, hint));
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
        serviceLabelFanLX->setText(QStringLiteral("当前服务 %1（%2）｜ %3").arg(service, accountId, hint));
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
        serviceLabelFanLX->setText(QStringLiteral("当前服务 %1（%2）｜ 聊天未连接：%3")
                                       .arg(service, accountId, detail));
        statusFanLX->setText(QStringLiteral("聊天未连接：") + detail);
        refreshHeaderFanLX();
        return;
    }
    serviceLabelFanLX->setText(
        QStringLiteral("当前聊天服务：%1（%2）｜ 只显示该服务的好友与群 ｜ 标志："
                       "<span style='color:#0b6bcb'>[好友]</span> / "
                       "<span style='color:#7b1fa2'>[群]</span>")
            .arg(service, accountId));
    statusFanLX->setText(QStringLiteral("聊天已切换到 %1（%2）：好友与群按服务分别存放")
                             .arg(service, accountId));
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
            "不受伤害；聊天使用 TCP 长度帧（中心转发），会话列表用 [好友] / [群] 与颜色区分\n"
            "   —— 领域数据菜单、群页签与聊天页签。"));
}

void MainWindowFanLX::refreshHeaderFanLX() {
    storageBackendFanLX = controllerFanLX->storageBackend();
    // 标题栏与页头给用户看的是中文服务名（QQ / 微信 / 微博），控制器内部仍用存储名。
    const auto chatService = controllerFanLX->serviceDisplayName();
    headerFanLX->setText(QStringLiteral("账号 %1 · 聊天服务 %2 · 存储 %3 · 会话 %4 · 未读 %5")
                             .arg(accountFanLX, chatService, storageBackendFanLX)
                             .arg(controllerFanLX->conversations().size())
                             .arg(controllerFanLX->totalUnread()));
    const auto current = controllerFanLX->currentConversation();
    // 聊天标题区：当前会话的"[类型] 名称"，类型标志沿用列表里的类型颜色。
    if (auto *chatTitle = findChild<QLabel *>(QStringLiteral("chatTitleFanLX"))) {
        QString titleHtml = QStringLiteral("未选择会话");
        if (!current.isEmpty()) {
            QString name = current;
            QString kindText = controllerFanLX->currentConversationKindText();
            QString kindColor;
            for (const auto &item : controllerFanLX->conversations()) {
                if (item.key == current) {
                    name = item.title;
                    kindText = item.kindText;
                    kindColor = item.kindColor;
                    break;
                }
            }
            titleHtml = name.toHtmlEscaped();
            if (!kindText.isEmpty()) {
                const auto tag = QStringLiteral("[%1]").arg(kindText).toHtmlEscaped();
                titleHtml = (kindColor.isEmpty()
                                 ? tag
                                 : QStringLiteral("<span style='color:%1'>%2</span>")
                                       .arg(kindColor.toHtmlEscaped(), tag)) +
                            QStringLiteral(" ") + titleHtml;
            }
        }
        chatTitle->setText(titleHtml);
    }
    if (!current.isEmpty()) {
        // 标题栏也带上类型标志：窗口标题一眼能看出当前打开的是群还是好友。
        const auto kind = controllerFanLX->currentConversationKindText();
        const auto title = kind.isEmpty() ? current : QStringLiteral("[%1] %2").arg(kind, current);
        setWindowTitle(QStringLiteral("樊陆旭模拟即时通讯系统 — %1（%2）").arg(title, chatService));
    } else {
        setWindowTitle(QStringLiteral("樊陆旭模拟即时通讯系统 — %1 客户端").arg(chatService));
    }
}

void MainWindowFanLX::onConversationsChangedFanLX() {
    conversationsFanLX->setItems(controllerFanLX->conversations());
    syncConversationSelectionFanLX(conversationViewFanLX, conversationsFanLX,
                                   controllerFanLX->currentConversation());
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

// 消息区渲染：继续使用 QTextBrowser 的 Qt 富文本，只用其稳定支持的表格、单元格底色、
// 对齐、内边距与基础字体样式（不依赖 Flexbox / Grid / 圆角）。
// 自己发送的消息靠右、浅蓝底；收到的消息靠左、白底；发送失败用浅红底与红色状态。
// 所有来自数据的文字（发送者、时间、状态、正文）都经过 HTML 转义。
void MainWindowFanLX::renderMessagesFanLX() {
    using namespace chatThemeFanLX;
    QString html;
    for (const auto &item : controllerFanLX->messages()) {
        const auto sender = QStringLiteral("<span style='color:%1; font-weight:600;'>%2</span>")
                                .arg(item.self ? accent : textSecondary, item.sender.toHtmlEscaped());
        const auto time = item.timeText.isEmpty()
                              ? QStringLiteral("<span style='color:%1'>[原始时间未知]</span>").arg(textMuted)
                              : QStringLiteral("<span style='color:%1'>%2</span>")
                                    .arg(textMuted, item.timeText.toHtmlEscaped());
        const auto status = item.statusText.isEmpty()
                                ? QString()
                                : (item.failed
                                       ? QStringLiteral("&nbsp;&nbsp;<span style='color:%1; font-weight:600;'>"
                                                        "✕ %2</span>")
                                       : QStringLiteral("&nbsp;&nbsp;<span style='color:%1'>%2</span>"))
                                      .arg(item.failed ? danger : textMuted, item.statusText.toHtmlEscaped());
        const auto align = item.self ? QStringLiteral("right") : QStringLiteral("left");
        const auto bubble = item.failed ? dangerSoft : (item.self ? selfBubble : peerBubble);
        // 收到的白色气泡在浅灰底上加一圈浅边框，保证层次；自己的蓝色气泡不需要。
        const auto bubbleBorder = item.self || item.failed
                                      ? QStringLiteral("border='0'")
                                      : QStringLiteral("border='1' style='border-color:#e6eaf0; "
                                                       "border-style:solid; border-collapse:collapse;'");
        // 外层两列表格：留白列（约 28%）把气泡限制在舒适的阅读宽度内，并决定靠左还是靠右。
        const auto spacer = QStringLiteral("<td width='28%'></td>");
        const auto content =
            QStringLiteral("<td align='%1' valign='top'>"
                           "<p align='%1' style='margin:0 2px 5px 2px; font-size:12px;'>%2&nbsp;&nbsp;%3%4</p>"
                           "<table align='%1' cellspacing='0' cellpadding='11' bgcolor='%5' %6>"
                           "<tr><td style='color:%7; font-size:14px; line-height:140%;'>%8</td></tr>"
                           "</table></td>")
                .arg(align, sender, time, status, bubble, bubbleBorder, textPrimary, escapeHtmlFanLX(item.body));
        html += QStringLiteral("<table width='100%' cellspacing='0' cellpadding='0' style='margin-bottom:16px;'>"
                               "<tr>%1</tr></table>")
                    .arg(item.self ? spacer + content : content + spacer);
    }
    if (html.isEmpty()) {
        // 空状态：未选会话与"已选会话但没有记录"分开说明。
        const bool noConversation = controllerFanLX->currentConversation().isEmpty();
        const auto title = noConversation ? QStringLiteral("选择左侧会话开始聊天") : QStringLiteral("暂无消息");
        const auto detail = noConversation
                                ? QStringLiteral("单聊与群聊都在左侧列表中，点击任意一项即可打开。")
                                : QStringLiteral("当前会话没有记录，或历史尚未加载完成。");
        html = QStringLiteral("<table width='100%' cellspacing='0' cellpadding='0' style='margin-top:96px;'>"
                              "<tr><td align='center'>"
                              "<p align='center' style='font-size:16px; font-weight:600; color:%1;'>%2</p>"
                              "<p align='center' style='margin-top:6px; font-size:13px; color:%3;'>%4</p>"
                              "</td></tr></table>")
                   .arg(textSecondary, title, textMuted, detail);
    }
    const auto scroll = messageViewFanLX->verticalScrollBar()->value();
    messageViewFanLX->setHtml(html);
    if (controllerFanLX->followingLatest() && !hasPendingNewMessageFanLX)
        messageViewFanLX->moveCursor(QTextCursor::End);
    else messageViewFanLX->verticalScrollBar()->setValue(scroll);
}

void MainWindowFanLX::onConversationSelectedFanLX(const QModelIndex &index) {
    if (!index.isValid())
        return;
    const auto key = conversationsFanLX->keyAt(index.row());
    controllerFanLX->openConversation(key);
    hasPendingNewMessageFanLX = false;
    newMessageHintFanLX->setVisible(false);
    const auto sequence = controllerFanLX->messages().isEmpty()
                              ? 0
                              : controllerFanLX->messages().last().sequence;
    if (sequence > 0)
        controllerFanLX->markRead(key, sequence);
    // 打开会话时明确说出这是群还是好友（与列表里的标志同源）。
    const auto kind = controllerFanLX->currentConversationKindText();
    if (!kind.isEmpty())
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
            if (!controllerFanLX->searchEarlier()) {
                historyDialogFanLX->setBusy(false);
                historyDialogFanLX->setSummary(QStringLiteral("下一页未能提交，请重试"));
            }
        });
    }
    QStringList keys;
    for (const auto &item : controllerFanLX->conversations())
        keys << item.key;
    historyDialogFanLX->setConversations(keys);
    const auto current = controllerFanLX->currentConversation();
    if (!current.isEmpty())
        historyDialogFanLX->setCurrentConversation(current);
    historyDialogFanLX->show();
    historyDialogFanLX->raise();
}

void MainWindowFanLX::showHistoryResultsFanLX() {
    if (!historyDialogFanLX)
        return;
    const auto key = historyDialogFanLX->conversationKey();
    const auto sender = historyDialogFanLX->senderFilter();
    const auto keyword = historyDialogFanLX->keyword();
    if (key.isEmpty() && sender.isEmpty() && keyword.isEmpty() && !historyDialogFanLX->useTimeRange()) {
        historyDialogFanLX->setSummary(QStringLiteral("请指定会话，或填写发送者/关键词做跨会话检索"));
        return;
    }
    QString from, to;
    if (historyDialogFanLX->useTimeRange()) {
        from = historyDialogFanLX->fromText(); to = historyDialogFanLX->toText();
        const auto valid = [](const QString &text) {
            if (text.isEmpty()) return true;
            const auto date = QDateTime::fromString(text, Qt::ISODate);
            return date.isValid() && date.toUTC().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ")) == text;
        };
        if ((from.isEmpty() && to.isEmpty()) || !valid(from) || !valid(to) ||
            (!from.isEmpty() && !to.isEmpty() && from > to)) {
            historyDialogFanLX->setSummary(QStringLiteral("请输入有效 UTC 时间 YYYY-MM-DDTHH:mm:ssZ，起始不得晚于结束"));
            return;
        }
    }
    const auto limit = historyDialogFanLX->pageSize();
    historyDialogFanLX->setBusy(true);
    if (!controllerFanLX->searchHistory(key, sender, keyword, limit, from, to)) {
        historyDialogFanLX->setBusy(false);
        historyDialogFanLX->setSummary(QStringLiteral("查询未能提交：界面繁忙或聊天库不可用"));
        return;
    }
    QString note = QStringLiteral("每页 %1 条（上限 200）；结果按服务器时间倒序").arg(limit);
    if (historyDialogFanLX->useTimeRange())
        note += QStringLiteral("；时间范围 %1 ~ %2").arg(historyDialogFanLX->fromText(),
                                                          historyDialogFanLX->toText());
    if (key.isEmpty())
        note += QStringLiteral("；当前为跨会话检索");
    historyDialogFanLX->setSummary(note);
}

void MainWindowFanLX::onHistoryLoadedFanLX(qint64 queryId, bool hasMore) {
    (void)queryId;
    if (!historyDialogFanLX)
        return;
    QString text;
    for (const auto &item : controllerFanLX->searchPage().messages) {
        text += QStringLiteral("%1  %2  %3%4\n")
                    .arg(item.acceptedAtUtc.empty() ? QStringLiteral("[原始时间未知]") : QString::fromStdString(item.acceptedAtUtc),
                         QString::fromStdString(item.sender), QString::fromStdString(item.body),
                         QStringLiteral("  [") + QString::fromStdString(item.conversationKey) + QStringLiteral("]"));
    }
    if (text.isEmpty())
        text = QStringLiteral("（没有匹配的记录）");
    historyDialogFanLX->setResultsText(text);
    historyDialogFanLX->setBusy(false);
    historyDialogFanLX->setHasMore(hasMore);
    historyDialogFanLX->setSummary(QStringLiteral("已加载 %1 条%2")
                                       .arg(controllerFanLX->searchPage().messages.size())
                                       .arg(hasMore ? QStringLiteral("；点击「下一页」继续")
                                                    : QStringLiteral("；已到最早一条")));
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
    if (historyDialogFanLX && historyDialogFanLX->isVisible()) {
        historyDialogFanLX->setBusy(false);
        historyDialogFanLX->setSummary(QStringLiteral("查询失败：") + detail);
    }
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

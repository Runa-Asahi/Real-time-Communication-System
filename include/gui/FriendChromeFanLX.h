#pragma once
// 好友页专用外观。样式选择器都挂在 QWidget#friendPageFanLX 上。
// kit::applyAccent 会整页替换样式表，所以这里不单独追加：
// 调用方要把 kit::pageStyle(accent) 和 friendChromeStyleFanLX(accent) 拼成一份再 setStyleSheet。
#include "gui/PageKitFanLX.h"
#include <QAbstractItemView>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace fanlxGui {

inline QColor friendMixWhiteFanLX(const QColor &color, double ratio) {
    const auto channel = [ratio](int value) {
        return static_cast<int>(value + (255 - value) * ratio);
    };
    return QColor(channel(color.red()), channel(color.green()), channel(color.blue()));
}

// 好友列表、查询结果共用的一行原文。备注和标签只在「备注：」「标签：」字段里取值，
// 「（无）」视为空，避免搜索「备注 / 标签 / 无」时命中每一行。
struct FriendLinePartsFanLX {
    bool placeholder = false;
    bool friendRecord = false;
    QString account;
    QString remark;
    QString tag;
    QString subtitle;
};

inline QString friendFieldValueFanLX(const QString &part, const QString &prefix) {
    if (!part.startsWith(prefix))
        return {};
    const auto value = part.mid(prefix.size()).trimmed();
    return value == QStringLiteral("（无）") ? QString() : value;
}

inline FriendLinePartsFanLX friendLinePartsFanLX(const QString &text) {
    FriendLinePartsFanLX parts;
    if (text.isEmpty() || text.startsWith(QStringLiteral("（"))) {
        parts.placeholder = true;
        parts.subtitle = text;
        return parts;
    }
    const auto fields = text.split(QStringLiteral(" ｜ "));
    parts.account = fields.value(0).trimmed();
    const auto remark = friendFieldValueFanLX(fields.value(1), QStringLiteral("备注："));
    const auto tag = friendFieldValueFanLX(fields.value(2), QStringLiteral("标签："));
    if (fields.value(1).startsWith(QStringLiteral("备注："))) {
        parts.friendRecord = true;
        parts.remark = remark;
        parts.tag = tag;
        return parts;
    }
    if (fields.size() > 1)
        parts.subtitle = fields.mid(1).join(QStringLiteral(" ｜ "));
    return parts;
}

inline QColor friendAvatarColorFanLX(const QString &account) {
    static const QColor colors[] = {QColor(QStringLiteral("#6E95D4")), QColor(QStringLiteral("#5EAE96")),
                                    QColor(QStringLiteral("#E09A4E")), QColor(QStringLiteral("#9A84C4")),
                                    QColor(QStringLiteral("#D67B8E")), QColor(QStringLiteral("#5AA8C4")),
                                    QColor(QStringLiteral("#8EAA55")), QColor(QStringLiteral("#C98A62"))};
    return colors[qHash(account) % 8];
}

inline QString friendAvatarTextFanLX(const FriendLinePartsFanLX &parts) {
    if (!parts.remark.isEmpty())
        return parts.remark.left(1);
    return parts.account.size() >= 2 ? parts.account.right(2) : parts.account;
}

inline QString friendChromeStyleFanLX(const QString &accentName) {
    const QColor accent(accentName);
    QString style = QStringLiteral(
        "QWidget#friendPageFanLX{background:#f3f5f8;color:#1F2329;"
        "font-family:\"Microsoft YaHei UI\";font-size:14px;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"primary\"]{background:@@ACCENT@@;color:#ffffff;"
        "border:1px solid @@ACCENT@@;border-radius:8px;padding:0px 16px;min-height:36px;"
        "font-size:14px;font-weight:600;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"primary\"]:hover{background:@@HOVER@@;"
        "border-color:@@HOVER@@;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"primary\"]:pressed{background:@@PRESSED@@;"
        "border-color:@@PRESSED@@;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"primary\"]:focus{border:2px solid @@PRESSED@@;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"primary\"]:disabled{background:#E5E7EB;"
        "color:#9CA3AF;border-color:#E5E7EB;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"secondary\"]{background:#ffffff;color:#1F2329;"
        "border:1px solid #E5E7EB;border-radius:8px;padding:0px 16px;min-height:36px;"
        "font-size:14px;font-weight:600;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"secondary\"]:hover{background:#F4F7FB;"
        "border-color:#D7DEE8;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"secondary\"]:pressed{background:#E7EEF8;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"secondary\"]:focus{border:1px solid @@ACCENT@@;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"secondary\"]:disabled{background:#F8F9FB;"
        "color:#9CA3AF;border-color:#E5E7EB;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"danger\"]{background:#ffffff;color:#D93025;"
        "border:1px solid #F0B4B0;border-radius:8px;padding:0px 16px;min-height:36px;"
        "font-size:14px;font-weight:600;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"danger\"]:hover{background:#FDECEA;"
        "border-color:#D93025;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"danger\"]:pressed{background:#F8D4D1;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"danger\"]:focus{border:1px solid #D93025;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"danger\"]:disabled{background:#ffffff;"
        "color:#E5B7B3;border-color:#F6DDDB;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"link\"]{background:transparent;color:@@ACCENT@@;"
        "border:none;border-radius:8px;padding:0px 8px;min-height:36px;font-size:14px;font-weight:600;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"link\"]:hover{background:@@SOFT@@;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"link\"]:pressed{background:@@SOFT@@;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"link\"]:focus{border:1px solid @@ACCENT@@;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"link\"]:disabled{color:#9CA3AF;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLineEdit{min-height:38px;border:1px solid #E5E7EB;border-radius:8px;"
        "padding:0px 10px;background:#ffffff;color:#1F2329;font-size:14px;"
        "selection-background-color:@@SOFT@@;selection-color:#1F2329;}"
        "QWidget#friendPageFanLX QLineEdit:focus{border:1px solid @@ACCENT@@;}"
        "QWidget#friendPageFanLX QLineEdit:disabled{background:#F8F9FB;color:#9CA3AF;}"
        "QWidget#friendPageFanLX QComboBox{min-height:38px;border:1px solid #E5E7EB;border-radius:8px;"
        "padding:0px 10px;background:#ffffff;color:#1F2329;font-size:14px;}"
        "QWidget#friendPageFanLX QComboBox:focus{border:1px solid @@ACCENT@@;}"
        "QWidget#friendPageFanLX QComboBox::drop-down{border:none;width:22px;}"
        "QWidget#friendPageFanLX QComboBox QAbstractItemView{background:#ffffff;color:#1F2329;"
        "selection-background-color:@@SOFT@@;selection-color:#1F2329;border:1px solid #E5E7EB;}"
        "QWidget#friendPageFanLX QLabel[role=\"hint\"]{color:#6B7280;font-size:13px;}"
        "QWidget#friendPageFanLX QFrame#friendCardFanLX{background:#ffffff;border:1px solid #E5E7EB;"
        "border-radius:12px;}"
        "QWidget#friendPageFanLX QLabel#friendCardTitleFanLX{color:#1F2329;font-size:15px;font-weight:600;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendHeaderTitleFanLX{color:#1F2329;font-size:20px;font-weight:600;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendHeaderSubtitleFanLX{color:#6B7280;font-size:12px;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendEmptyTitleFanLX{color:#1F2329;font-size:16px;font-weight:600;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendEmptyTitleFanLX[error=\"true\"]{color:#D93025;}"
        "QWidget#friendPageFanLX QLabel#friendEmptyBodyFanLX{color:#6B7280;font-size:13px;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"segment\"]{background:transparent;color:#6B7280;"
        "border:none;border-bottom:2px solid transparent;border-radius:0px;padding:0px 14px;"
        "min-height:36px;font-size:14px;font-weight:600;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"segment\"]:hover{color:#1F2329;}"
        "QWidget#friendPageFanLX QPushButton[friendRole=\"segment\"][active=\"true\"]{color:@@ACCENT@@;"
        "border-bottom:2px solid @@ACCENT@@;}"
        "QWidget#friendPageFanLX QLabel#friendHomeTitleFanLX{color:#1F2329;font-size:20px;font-weight:600;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendDetailRemarkFanLX{color:#1F2329;font-size:18px;font-weight:600;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendDetailRemarkFanLX[muted=\"true\"]{color:#6B7280;font-size:14px;"
        "font-weight:400;}"
        "QWidget#friendPageFanLX QLabel#friendDetailAccountFanLX{color:#6B7280;font-size:13px;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendBadgeFanLX{background:#D93025;color:#ffffff;border-radius:9px;"
        "padding:0px 6px;font-size:11px;font-weight:700;}"
        "QWidget#friendPageFanLX QLabel#friendFieldFanLX{color:#6B7280;font-size:12px;background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendNoticeFanLX{background:#F4F7FB;color:#1F2329;"
        "border:1px solid #E5E7EB;border-radius:8px;padding:10px 12px;font-size:13px;}"
        "QWidget#friendPageFanLX QLabel#friendInlineResultFanLX{color:#6B7280;font-size:13px;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendInlineResultFanLX[tone=\"ok\"]{color:#16A34A;}"
        "QWidget#friendPageFanLX QLabel#friendInlineResultFanLX[tone=\"bad\"]{color:#D93025;}"
        "QWidget#friendPageFanLX QLabel#friendTagChipFanLX{background:#F3F4F6;color:#6B7280;"
        "border-radius:10px;padding:2px 8px;font-size:12px;}"
        "QWidget#friendPageFanLX QLabel#friendResultTitleFanLX{color:#1F2329;font-size:16px;font-weight:600;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QLabel#friendTemporaryNoteFanLX{color:#6B7280;font-size:12px;"
        "background:transparent;}"
        "QWidget#friendPageFanLX QFrame#friendDetailFanLX{background:#ffffff;border:1px solid #E5E7EB;"
        "border-radius:12px;}"
        "QWidget#friendPageFanLX QListWidget#friendListFanLX,"
        "QWidget#friendPageFanLX QListWidget#friendResultListFanLX,"
        "QWidget#friendPageFanLX QListWidget#friendCandidateListFanLX,"
        "QWidget#friendPageFanLX QListWidget#friendIncomingListFanLX,"
        "QWidget#friendPageFanLX QListWidget#friendOutgoingListFanLX,"
        "QWidget#friendPageFanLX QListWidget#friendTemporaryListFanLX{border:1px solid #E5E7EB;"
        "border-radius:12px;background:#ffffff;outline:none;}"
        "QWidget#friendPageFanLX QListWidget#friendListFanLX::item,"
        "QWidget#friendPageFanLX QListWidget#friendResultListFanLX::item,"
        "QWidget#friendPageFanLX QListWidget#friendCandidateListFanLX::item,"
        "QWidget#friendPageFanLX QListWidget#friendIncomingListFanLX::item,"
        "QWidget#friendPageFanLX QListWidget#friendOutgoingListFanLX::item,"
        "QWidget#friendPageFanLX QListWidget#friendTemporaryListFanLX::item{background:transparent;"
        "color:#1F2329;border:none;padding:0px;}"
        "QWidget#friendPageFanLX QListWidget#friendListFanLX::item:selected,"
        "QWidget#friendPageFanLX QListWidget#friendResultListFanLX::item:selected,"
        "QWidget#friendPageFanLX QListWidget#friendCandidateListFanLX::item:selected,"
        "QWidget#friendPageFanLX QListWidget#friendIncomingListFanLX::item:selected,"
        "QWidget#friendPageFanLX QListWidget#friendOutgoingListFanLX::item:selected,"
        "QWidget#friendPageFanLX QListWidget#friendTemporaryListFanLX::item:selected{background:transparent;"
        "color:#1F2329;}");
    style.replace(QStringLiteral("@@ACCENT@@"), accent.name());
    style.replace(QStringLiteral("@@HOVER@@"), accent.darker(115).name());
    style.replace(QStringLiteral("@@PRESSED@@"), accent.darker(130).name());
    style.replace(QStringLiteral("@@SOFT@@"), friendMixWhiteFanLX(accent, 0.88).name());
    return style;
}

inline void applyFriendChromeFanLX(QWidget *page, const QString &service) {
    if (!page)
        return;
    const auto accent = kit::accentForService(service);
    const auto style = kit::pageStyle(accent) + friendChromeStyleFanLX(accent);
    if (page->styleSheet() != style)
        page->setStyleSheet(style);
    auto palette = page->palette();
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#9CA3AF")));
    page->setPalette(palette);
}

// 确认框如果挂在带样式表的好友页上，会继承按钮和输入框规则。
// 主窗口本身没有样式表时挂到主窗口；否则不指定父控件，避免把好友页样式带进去。
inline QWidget *friendDialogParentFanLX(QWidget *page) {
    if (!page)
        return nullptr;
    QWidget *top = page->window();
    if (top && top != page && top->styleSheet().isEmpty())
        return top;
    return nullptr;
}

inline QPushButton *friendRoleButtonFanLX(const QString &text, const QString &role, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setProperty("friendRole", role);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(36);
    button->setFocusPolicy(Qt::StrongFocus);
    return button;
}

inline QPushButton *friendPrimaryButtonFanLX(const QString &text, QWidget *parent) {
    return friendRoleButtonFanLX(text, QStringLiteral("primary"), parent);
}

inline QPushButton *friendSecondaryButtonFanLX(const QString &text, QWidget *parent) {
    return friendRoleButtonFanLX(text, QStringLiteral("secondary"), parent);
}

inline QPushButton *friendDangerButtonFanLX(const QString &text, QWidget *parent) {
    return friendRoleButtonFanLX(text, QStringLiteral("danger"), parent);
}

inline QPushButton *friendLinkButtonFanLX(const QString &text, QWidget *parent) {
    return friendRoleButtonFanLX(text, QStringLiteral("link"), parent);
}

inline QPushButton *friendSegmentButtonFanLX(const QString &text, QWidget *parent) {
    auto *button = friendRoleButtonFanLX(text, QStringLiteral("segment"), parent);
    button->setProperty("active", QStringLiteral("false"));
    return button;
}

class FriendCardFanLX : public QFrame {
  public:
    explicit FriendCardFanLX(const QString &title, QWidget *parent = nullptr) : QFrame(parent) {
        setObjectName(QStringLiteral("friendCardFanLX"));
        setFrameShape(QFrame::NoFrame);
        setAttribute(Qt::WA_StyledBackground, true);
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(20, 18, 20, 18);
        outer->setSpacing(12);
        title_ = new QLabel(title, this);
        title_->setObjectName(QStringLiteral("friendCardTitleFanLX"));
        outer->addWidget(title_);
        if (title.isEmpty())
            title_->hide();
        auto *host = new QWidget(this);
        body_ = new QVBoxLayout(host);
        body_->setContentsMargins(0, 0, 0, 0);
        body_->setSpacing(8);
        outer->addWidget(host);
    }

    QVBoxLayout *bodyLayout() const { return body_; }
    void setTitle(const QString &text) {
        title_->setText(text);
        title_->setVisible(!text.isEmpty());
    }

  private:
    QLabel *title_ = nullptr;
    QVBoxLayout *body_ = nullptr;
};

class FriendPageHeaderFanLX : public QWidget {
  public:
    explicit FriendPageHeaderFanLX(const QString &title, QWidget *parent = nullptr) : QWidget(parent) {
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(12);
        back_ = friendLinkButtonFanLX(QStringLiteral("‹ 好友"), this);
        auto *titles = new QVBoxLayout();
        titles->setContentsMargins(0, 0, 0, 0);
        titles->setSpacing(2);
        title_ = new QLabel(title, this);
        title_->setObjectName(QStringLiteral("friendHeaderTitleFanLX"));
        subtitle_ = new QLabel(this);
        subtitle_->setObjectName(QStringLiteral("friendHeaderSubtitleFanLX"));
        titles->addWidget(title_);
        titles->addWidget(subtitle_);
        trailing_ = new QHBoxLayout();
        trailing_->setContentsMargins(0, 0, 0, 0);
        trailing_->setSpacing(8);
        row->addWidget(back_, 0, Qt::AlignVCenter);
        row->addLayout(titles, 1);
        row->addLayout(trailing_);
    }

    QPushButton *backButton() const { return back_; }
    QLabel *subtitleLabel() const { return subtitle_; }
    QHBoxLayout *trailingLayout() const { return trailing_; }
    void setSubtitle(const QString &text) { subtitle_->setText(text); }

  private:
    QPushButton *back_ = nullptr;
    QLabel *title_ = nullptr;
    QLabel *subtitle_ = nullptr;
    QHBoxLayout *trailing_ = nullptr;
};

class FriendEmptyMarkFanLX : public QWidget {
  public:
    explicit FriendEmptyMarkFanLX(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(56, 56); }
    void setError(bool error) {
        error_ = error;
        update();
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(error_ ? QColor(QStringLiteral("#FDECEA")) : QColor(QStringLiteral("#EEF2F6")));
        painter.drawEllipse(rect().adjusted(1, 1, -1, -1));
        painter.setPen(QPen(error_ ? QColor(QStringLiteral("#D93025")) : QColor(QStringLiteral("#6B7280")), 1.6));
        painter.setBrush(Qt::NoBrush);
        if (error_) {
            painter.drawLine(QPointF(28, 16), QPointF(28, 32));
            painter.setBrush(error_ ? QColor(QStringLiteral("#D93025")) : QColor(QStringLiteral("#6B7280")));
            painter.drawEllipse(QPointF(28, 39), 1.6, 1.6);
            return;
        }
        painter.drawEllipse(QRectF(20, 14, 16, 16));
        painter.drawArc(QRectF(12, 32, 32, 24), 20 * 16, 140 * 16);
    }

  private:
    bool error_ = false;
};

class FriendEmptyStateFanLX : public QWidget {
  public:
    FriendEmptyStateFanLX(const QString &title, const QString &body, QWidget *parent = nullptr)
        : QWidget(parent) {
        setObjectName(QStringLiteral("friendEmptyStateFanLX"));
        layout_ = new QVBoxLayout(this);
        layout_->setContentsMargins(24, 24, 24, 24);
        layout_->setSpacing(8);
        mark_ = new FriendEmptyMarkFanLX(this);
        title_ = new QLabel(title, this);
        title_->setObjectName(QStringLiteral("friendEmptyTitleFanLX"));
        title_->setAlignment(Qt::AlignCenter);
        body_ = new QLabel(body, this);
        body_->setObjectName(QStringLiteral("friendEmptyBodyFanLX"));
        body_->setWordWrap(true);
        body_->setAlignment(Qt::AlignCenter);
        layout_->addStretch(1);
        layout_->addWidget(mark_, 0, Qt::AlignHCenter);
        layout_->addWidget(title_);
        layout_->addWidget(body_);
        layout_->addStretch(1);
    }

    void setText(const QString &title, const QString &body) {
        title_->setText(title);
        body_->setText(body);
    }
    QPushButton *setAction(const QString &text) {
        if (!action_) {
            action_ = friendPrimaryButtonFanLX(text, this);
            layout_->insertWidget(layout_->count() - 1, action_, 0, Qt::AlignHCenter);
        } else {
            action_->setText(text);
            action_->show();
        }
        return action_;
    }
    void setActionVisible(bool visible) {
        if (action_)
            action_->setVisible(visible);
    }
    void setError(bool error) {
        mark_->setError(error);
        title_->setProperty("error", error ? QStringLiteral("true") : QStringLiteral("false"));
        title_->style()->unpolish(title_);
        title_->style()->polish(title_);
        title_->update();
    }

  private:
    QVBoxLayout *layout_ = nullptr;
    FriendEmptyMarkFanLX *mark_ = nullptr;
    QLabel *title_ = nullptr;
    QLabel *body_ = nullptr;
    QPushButton *action_ = nullptr;
};

class FriendRowDelegateFanLX : public QStyledItemDelegate {
  public:
    explicit FriendRowDelegateFanLX(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    // 临时会话行画「已发 n/N」。0 表示还不知道上限，不画这枚标记。
    void setDirectionLimit(int limit) { directionLimit_ = limit; }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const auto parts = friendLinePartsFanLX(index.data(Qt::DisplayRole).toString());
        return QSize(option.rect.width(), parts.placeholder ? 44 : 64);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const auto parts = friendLinePartsFanLX(index.data(Qt::DisplayRole).toString());
        painter->save();
        painter->setClipRect(option.rect);
        painter->setRenderHint(QPainter::Antialiasing, true);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);
        painter->fillRect(option.rect, selected ? QColor(QStringLiteral("#E8F1FC"))
                                    : hover    ? QColor(QStringLiteral("#F7F9FC"))
                                               : QColor(QStringLiteral("#ffffff")));
        if (parts.placeholder) {
            painter->setPen(QColor(QStringLiteral("#6B7280")));
            QFont font = option.font;
            font.setPixelSize(13);
            painter->setFont(font);
            painter->drawText(option.rect.adjusted(16, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft,
                              parts.subtitle);
            painter->restore();
            return;
        }

        const QRect avatar(option.rect.left() + 12, option.rect.top() + (option.rect.height() - 40) / 2, 40, 40);
        painter->setPen(Qt::NoPen);
        painter->setBrush(friendAvatarColorFanLX(parts.account));
        painter->drawEllipse(avatar);
        painter->setPen(Qt::white);
        QFont avatarFont = option.font;
        avatarFont.setPixelSize(parts.remark.isEmpty() ? 12 : 16);
        avatarFont.setWeight(QFont::DemiBold);
        painter->setFont(avatarFont);
        painter->drawText(avatar, Qt::AlignCenter, friendAvatarTextFanLX(parts));

        const int textLeft = avatar.right() + 12;
        const int textWidth = qMax(0, option.rect.right() - 12 - textLeft);
        QString secondary;
        QString chipText;
        if (parts.friendRecord) {
            secondary = parts.remark.isEmpty() ? QString() : parts.account;
            chipText = parts.tag;
        } else {
            secondary = parts.subtitle;
            const auto marker = QStringLiteral("我已发 ");
            const auto at = directionLimit_ > 0 ? secondary.indexOf(marker) : -1;
            if (at >= 0) {
                const auto sent = secondary.mid(at + marker.size()).section(QLatin1Char(' '), 0, 0);
                if (!sent.isEmpty())
                    chipText = QStringLiteral("已发 %1/%2").arg(sent).arg(directionLimit_);
            }
        }
        const QString primary = parts.remark.isEmpty() ? parts.account : parts.remark;
        const bool hasSecondary = !secondary.isEmpty() || !chipText.isEmpty();
        QFont primaryFont = option.font;
        primaryFont.setPixelSize(14);
        primaryFont.setWeight(QFont::DemiBold);
        QFontMetrics primaryFm(primaryFont);
        painter->setFont(primaryFont);
        painter->setPen(QColor(QStringLiteral("#1F2329")));
        const QRect primaryRect(textLeft, hasSecondary ? option.rect.top() + 12 : option.rect.top(), textWidth,
                                hasSecondary ? 20 : option.rect.height());
        painter->drawText(primaryRect, Qt::AlignLeft | Qt::AlignVCenter,
                          primaryFm.elidedText(primary, Qt::ElideRight, textWidth));
        if (!hasSecondary) {
            painter->restore();
            return;
        }

        QFont secondaryFont = option.font;
        secondaryFont.setPixelSize(12);
        QFontMetrics secondaryFm(secondaryFont);
        int chipWidth = 0;
        if (!chipText.isEmpty())
            chipWidth = secondaryFm.horizontalAdvance(chipText) + 16;
        const int gap = !secondary.isEmpty() && chipWidth > 0 ? 8 : 0;
        const int natural = secondary.isEmpty() ? 0 : secondaryFm.horizontalAdvance(secondary);
        const int accountWidth = qMin(natural, qMax(0, textWidth - chipWidth - gap));
        painter->setFont(secondaryFont);
        painter->setPen(QColor(QStringLiteral("#6B7280")));
        const QRect accountRect(textLeft, option.rect.top() + 34, qMax(accountWidth, 0), 18);
        if (!secondary.isEmpty())
            painter->drawText(accountRect, Qt::AlignLeft | Qt::AlignVCenter,
                              secondaryFm.elidedText(secondary, Qt::ElideRight, accountWidth));
        if (!chipText.isEmpty()) {
            const QRect chip(textLeft + (secondary.isEmpty() ? 0 : accountWidth + gap), option.rect.top() + 33,
                             chipWidth, 20);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(QStringLiteral("#F3F4F6")));
            painter->drawRoundedRect(chip, 10, 10);
            painter->setPen(QColor(QStringLiteral("#6B7280")));
            painter->setFont(secondaryFont);
            painter->drawText(chip, Qt::AlignCenter, chipText);
        }
        painter->restore();
    }

  private:
    int directionLimit_ = 0;
};

inline void prepareFriendRowListFanLX(QListWidget *list, const QString &objectName) {
    if (!list)
        return;
    list->setObjectName(objectName);
    list->setFrameShape(QFrame::NoFrame);
    list->setMouseTracking(true);
    list->viewport()->setAttribute(Qt::WA_Hover, true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setSpacing(0);
}

} // namespace fanlxGui

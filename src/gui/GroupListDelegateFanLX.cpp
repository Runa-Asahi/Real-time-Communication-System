#include "gui/GroupListDelegateFanLX.h"

#include <QFontMetrics>
#include <QPainter>

namespace fanlxGui {

namespace {

QColor roleDotColorFanLX(const QString &text) {
    if (text.contains(QStringLiteral("群主")))
        return QColor(QStringLiteral("#f5a623"));
    if (text.contains(QStringLiteral("管理员")))
        return QColor(QStringLiteral("#c0c4cc"));
    return QColor(QStringLiteral("#909399"));
}

QString archiveBadgeFanLX(const QString &text, QColor *background, QColor *foreground) {
    const bool yes = text.contains(QStringLiteral("已归档：是")) || text.contains(QStringLiteral("已归档: 是"));
    const bool no = text.contains(QStringLiteral("已归档：否")) || text.contains(QStringLiteral("已归档: 否"));
    if (!yes && !no)
        return {};
    if (yes) {
        *background = QColor(QStringLiteral("#52c41a"));
        *foreground = QColor(QStringLiteral("#ffffff"));
        return QStringLiteral("是");
    }
    *background = QColor(QStringLiteral("#c0c4cc"));
    *foreground = QColor(QStringLiteral("#606266"));
    return QStringLiteral("否");
}

void paintMemberRowFanLX(QPainter *painter, const QStyleOptionViewItem &option, const QString &text) {
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hover = option.state.testFlag(QStyle::State_MouseOver);
    QColor background(QStringLiteral("#ffffff"));
    if (selected)
        background = QColor(QStringLiteral("#e3eeff"));
    else if (hover)
        background = QColor(QStringLiteral("#f0f7ff"));
    painter->fillRect(option.rect, background);
    if (selected) {
        painter->fillRect(QRect(option.rect.left(), option.rect.top(), 3, option.rect.height()),
                          QColor(QStringLiteral("#2f81f7")));
    }

    constexpr int kDot = 8;
    const int dotX = option.rect.left() + 14;
    const int dotY = option.rect.center().y() - kDot / 2;
    painter->setPen(Qt::NoPen);
    painter->setBrush(roleDotColorFanLX(text));
    painter->drawEllipse(QRect(dotX, dotY, kDot, kDot));

    QRect textRect = option.rect.adjusted(dotX - option.rect.left() + kDot + 8, 0, -10, 0);
    painter->setFont(option.font);
    painter->setPen(selected ? QColor(QStringLiteral("#1f2328")) : QColor(QStringLiteral("#606266")));
    const auto shown = QFontMetrics(option.font).elidedText(text, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, shown);
}

void paintCardRowFanLX(QPainter *painter, const QStyleOptionViewItem &option, const QString &text,
                       const QString &badge, const QColor &badgeBackground, const QColor &badgeForeground) {
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hover = option.state.testFlag(QStyle::State_MouseOver);
    QColor background(QStringLiteral("#ffffff"));
    if (selected)
        background = QColor(QStringLiteral("#e3eeff"));
    else if (hover)
        background = QColor(QStringLiteral("#f7f9fc"));

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(background);
    const QRect card = option.rect.adjusted(4, 3, -4, -4);
    painter->drawRoundedRect(card, 6, 6);

    painter->setRenderHint(QPainter::Antialiasing, false);
    const int lineWidth = option.rect.width() - 20;
    if (lineWidth > 0) {
        painter->fillRect(QRect(option.rect.left() + 10, option.rect.bottom(), lineWidth, 1),
                          QColor(QStringLiteral("#ebeef5")));
    }

    QRect textRect = card.adjusted(12, 0, -12, 0);
    const QFontMetrics metrics(option.font);
    if (!badge.isEmpty()) {
        const int badgeWidth = metrics.horizontalAdvance(badge) + 18;
        constexpr int kGap = 10;
        constexpr int kBadgeHeight = 18;
        if (textRect.width() > badgeWidth + kGap + 16) {
            const QRect badgeRect(textRect.right() - badgeWidth + 1, textRect.center().y() - kBadgeHeight / 2,
                                  badgeWidth, kBadgeHeight);
            const int textWidth = metrics.horizontalAdvance(text);
            // 文字已经占满行尾时不画胶囊，避免盖住原文。
            if (textRect.left() + textWidth + kGap <= badgeRect.left()) {
                painter->setRenderHint(QPainter::Antialiasing, true);
                painter->setPen(Qt::NoPen);
                painter->setBrush(badgeBackground);
                painter->drawRoundedRect(badgeRect, kBadgeHeight / 2, kBadgeHeight / 2);
                painter->setPen(badgeForeground);
                painter->setFont(option.font);
                painter->drawText(badgeRect, Qt::AlignCenter, badge);
                textRect.setRight(badgeRect.left() - kGap);
            }
        }
    }

    painter->setFont(option.font);
    painter->setPen(QColor(QStringLiteral("#606266")));
    const auto shown = metrics.elidedText(text, Qt::ElideRight, qMax(0, textRect.width()));
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, shown);
}

} // namespace

GroupListDelegateFanLX::GroupListDelegateFanLX(Kind kind, QObject *parent)
    : QStyledItemDelegate(parent), kindFanLX(kind) {}

void GroupListDelegateFanLX::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const {
    const QString text = index.data(Qt::DisplayRole).toString();
    painter->save();
    painter->setClipRect(option.rect);
    if (kindFanLX == Kind::Member) {
        paintMemberRowFanLX(painter, option, text);
        painter->restore();
        return;
    }

    QString badge;
    QColor badgeBackground;
    QColor badgeForeground(QStringLiteral("#ffffff"));
    if (kindFanLX == Kind::Pending && text.contains(QStringLiteral("Pending"))) {
        badge = QStringLiteral("Pending");
        badgeBackground = QColor(QStringLiteral("#f5a623"));
    } else if (kindFanLX == Kind::Discussion) {
        badge = archiveBadgeFanLX(text, &badgeBackground, &badgeForeground);
    }
    paintCardRowFanLX(painter, option, text, badge, badgeBackground, badgeForeground);
    painter->restore();
}

} // namespace fanlxGui

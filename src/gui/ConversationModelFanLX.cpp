#include "gui/ConversationModelFanLX.h"
#include <QBrush>
#include <QColor>

namespace fanlxGui {

ConversationListModelFanLX::ConversationListModelFanLX(QObject *parent) : QAbstractListModel(parent) {}

int ConversationListModelFanLX::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(itemsFanLX.size());
}

QVariant ConversationListModelFanLX::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= itemsFanLX.size())
        return {};
    const auto &item = itemsFanLX.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRoleFanLX:
        // 标题统一带类型前缀，任意视图（列表、下拉框、状态栏）都不会丢掉"群 / 好友"的标志。
        return QStringLiteral("[") + item.kindText + QStringLiteral("] ") + item.title;
    case KeyRoleFanLX: return item.key;
    case UnreadRoleFanLX: return item.unread;
    case GroupRoleFanLX: return item.group;
    case SubtitleRoleFanLX: return item.subtitle;
    case TimeRoleFanLX: return item.timeText;
    case KindTextRoleFanLX: return item.kindText;
    // 群用紫色、好友用蓝色：即使不看文字也能分辨（色弱用户仍可读前缀文字）。
    case Qt::ForegroundRole: return QBrush(QColor(item.kindColor));
    case Qt::ToolTipRole:
        return QStringLiteral("%1：%2").arg(item.kindText, item.title);
    default: return {};
    }
}

QHash<int, QByteArray> ConversationListModelFanLX::roleNames() const {
    return {{TitleRoleFanLX, "title"},      {KeyRoleFanLX, "key"},      {UnreadRoleFanLX, "unread"},
            {GroupRoleFanLX, "group"},      {SubtitleRoleFanLX, "subtitle"},
            {TimeRoleFanLX, "time"},        {KindTextRoleFanLX, "kind"}};
}

void ConversationListModelFanLX::setItems(const QList<ConversationItemFanLX> &items) {
    beginResetModel();
    itemsFanLX = items;
    endResetModel();
}

QString ConversationListModelFanLX::keyAt(int row) const {
    if (row < 0 || row >= itemsFanLX.size())
        return {};
    return itemsFanLX.at(row).key;
}

MessageListModelFanLX::MessageListModelFanLX(QObject *parent) : QAbstractListModel(parent) {}

int MessageListModelFanLX::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(messagesFanLX.size());
}

QVariant MessageListModelFanLX::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= messagesFanLX.size())
        return {};
    const auto &item = messagesFanLX.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case BodyRoleFanLX: return item.body;
    case SenderRoleFanLX: return item.sender;
    case TimeRoleFanLX: return item.timeText;
    case StatusRoleFanLX: return item.statusText;
    case SelfRoleFanLX: return item.self;
    case FailedRoleFanLX: return item.failed;
    case PendingRoleFanLX: return item.pending;
    case SequenceRoleFanLX: return item.sequence;
    default: return {};
    }
}

QHash<int, QByteArray> MessageListModelFanLX::roleNames() const {
    return {{SenderRoleFanLX, "sender"},   {BodyRoleFanLX, "body"},     {TimeRoleFanLX, "time"},
            {StatusRoleFanLX, "status"},   {SelfRoleFanLX, "self"},     {FailedRoleFanLX, "failed"},
            {PendingRoleFanLX, "pending"}, {SequenceRoleFanLX, "sequence"}};
}

void MessageListModelFanLX::setMessages(const QList<MessageItemFanLX> &messages) {
    beginResetModel();
    messagesFanLX = messages;
    endResetModel();
}

void MessageListModelFanLX::appendMessages(const QList<MessageItemFanLX> &messages) {
    if (messages.isEmpty())
        return;
    const auto first = static_cast<int>(messagesFanLX.size());
    beginInsertRows(QModelIndex(), first, first + static_cast<int>(messages.size()) - 1);
    messagesFanLX.append(messages);
    endInsertRows();
}

qint64 MessageListModelFanLX::sequenceAt(int row) const {
    if (row < 0 || row >= messagesFanLX.size())
        return 0;
    return messagesFanLX.at(row).sequence;
}

} // namespace fanlxGui

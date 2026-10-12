#pragma once
#include "gui/ChatControllerFanLX.h"
#include <QAbstractListModel>
#include <QObject>
#include <vector>

namespace fanlxGui {

// 会话列表模型：只做展示映射，不做任何网络/数据库调用（数据由控制器推入）。
// 群与好友在这里用**显示文案 + 前景色**区分（见 ChatControllerFanLX::applyKindFanLX），
// 因此列表行呈现为"[群] 1001"（紫）与"[好友] 2026002"（蓝），不需要读会话键就能分辨。
class ConversationListModelFanLX : public QAbstractListModel {
    Q_OBJECT

  public:
    enum RoleFanLX {
        KeyRoleFanLX = Qt::UserRole + 1,
        UnreadRoleFanLX,
        GroupRoleFanLX,
        SubtitleRoleFanLX,
        TimeRoleFanLX,
        TitleRoleFanLX,
        KindTextRoleFanLX
    };

    explicit ConversationListModelFanLX(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setItems(const QList<ConversationItemFanLX> &items);
    QString keyAt(int row) const;

  private:
    QList<ConversationItemFanLX> itemsFanLX;
};

// 消息列表模型：每条消息带上状态文案，界面直接显示"待发送/服务器已接收/失败"。
class MessageListModelFanLX : public QAbstractListModel {
    Q_OBJECT

  public:
    enum RoleFanLX {
        SenderRoleFanLX = Qt::UserRole + 1,
        BodyRoleFanLX,
        TimeRoleFanLX,
        StatusRoleFanLX,
        SelfRoleFanLX,
        FailedRoleFanLX,
        PendingRoleFanLX,
        SequenceRoleFanLX
    };

    explicit MessageListModelFanLX(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setMessages(const QList<MessageItemFanLX> &messages);
    void appendMessages(const QList<MessageItemFanLX> &messages);
    qint64 sequenceAt(int row) const;

  private:
    QList<MessageItemFanLX> messagesFanLX;
};

} // namespace fanlxGui

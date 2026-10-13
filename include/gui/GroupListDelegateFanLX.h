#pragma once
#include <QStyledItemDelegate>

namespace fanlxGui {

// 群页列表的绘制层。只画圆点、行背景、分隔线和状态胶囊。
// 不改 item 的 text()，也不重写 sizeHint，行高仍由样式表的 padding 决定。
class GroupListDelegateFanLX : public QStyledItemDelegate {
  public:
    enum class Kind { Member, Pending, Discussion };

    explicit GroupListDelegateFanLX(Kind kind, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

  private:
    Kind kindFanLX;
};

} // namespace fanlxGui

#include "gui/AccountPageFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/PageKitFanLX.h"
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QColor>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>

#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QStyledItemDelegate>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fanlxGui {

namespace {

// 服务下拉框的固定项：微X 是上层登录身份（不可"开通"），因此开通/确认/绑定都只列三种下级服务。
QStringList platformNamesFanLX() {
    return {QStringLiteral("QQ"), QStringLiteral("微信"), QStringLiteral("微博")};
}

QComboBox *makePlatformComboFanLX(QWidget *parent) {
    auto *combo = new QComboBox(parent);
    combo->addItems(platformNamesFanLX());
    return combo;
}

// 当前服务下拉框：本人已建号的身份（含微X），用于切换"以谁的身份操作"。
QStringList identityNamesFanLX(DomainControllerFanLX *controller) {
    QStringList names = controller->ownedServices();
    if (names.isEmpty())
        names << QStringLiteral("微X");
    return names;
}

// 只作用于账号首页。颜色用 AppThemeFanLX 的同一套色值。
// 不改 PageKit 的 nav 角色，因此好友页和群页保持原样。
QString accountHomeStyleFanLX() {
    return QStringLiteral(
        "QWidget#accountHomeFanLX{background:#F6F7F9;}"
        "QLabel#accountPageTitleFanLX{font-size:22px;font-weight:700;color:#1F2329;background:transparent;letter-spacing:0.2px;}"
        "QFrame#accountIdentityFanLX{background:#FFFFFF;border:1px solid #E6E8EE;border-radius:8px;}"
        "QLabel#accountIdentityTextFanLX{font-size:15px;font-weight:600;color:#1F2329;background:transparent;}"
        "QLabel#accountIdentityHintFanLX{font-size:12px;color:#5B6472;background:transparent;}"
        "QFrame#accountIdentityFanLX QComboBox{background:#FFFFFF;color:#1F2329;border:1px solid #E6E8EE;"
        "border-radius:8px;padding:0px 10px;min-height:36px;}"
        "QFrame#accountIdentityFanLX QComboBox:hover{border-color:#D7DEE8;}"
        "QFrame#accountIdentityFanLX QComboBox:focus{border-color:#2563EB;}"
        "QFrame#accountIdentityFanLX QComboBox::drop-down{border:none;width:22px;}"
        "QFrame#accountIdentityFanLX QComboBox QAbstractItemView{background:#FFFFFF;color:#1F2329;"
        "selection-background-color:#E8EFFD;selection-color:#1F2329;border:1px solid #E6E8EE;}"
        "QFrame#accountProfileFanLX{background:#FFFFFF;border:1px solid #E6E8EE;border-radius:8px;}"
        "QLabel#accountProfileHeadingFanLX{font-size:13px;font-weight:600;color:#5B6472;background:transparent;}"
        "QFrame#accountProfileFanLX QLabel[role=\"name\"]{font-size:18px;font-weight:600;color:#1F2329;"
        "background:transparent;}"
        "QFrame#accountProfileFanLX QLabel[role=\"caption\"]{font-size:12px;color:#8A94A3;background:transparent;}"
        "QFrame#accountProfileFanLX QLabel[role=\"value\"]{font-size:14px;color:#1F2329;background:transparent;}"
        "QLabel#accountStatValueFanLX{font-size:18px;font-weight:600;color:#1F2329;background:transparent;}"
        "QLabel#accountStatCaptionFanLX{font-size:12px;color:#8A94A3;background:transparent;}"
        "QFrame#accountHairlineFanLX{background:#E6E8EE;border:none;max-height:1px;}"
        "QWidget#accountHomeFanLX QFrame#bannerFanLX{background:#FFF4E5;border:1px solid #F3D7B0;"
        "border-radius:8px;}"
        "QWidget#accountHomeFanLX QFrame#bannerFanLX QLabel{color:#B45309;background:transparent;}"
        "QWidget#accountHomeFanLX QPushButton[role=\"primary\"]{background:#2563EB;color:#FFFFFF;"
        "border:none;border-radius:8px;padding:0px 16px;min-height:36px;font-size:14px;font-weight:600;}"
        "QWidget#accountHomeFanLX QPushButton[role=\"primary\"]:hover{background:#1D4ED8;}"
        "QWidget#accountHomeFanLX QPushButton[role=\"primary\"]:pressed{background:#1E40AF;}"
        "QWidget#accountHomeFanLX QPushButton[role=\"primary\"]:disabled{background:#E6E8EE;color:#8A94A3;}"
        "QFrame#accountEntriesFanLX{background:#FFFFFF;border:1px solid #E6E8EE;border-radius:8px;}"
        "QPushButton[role=\"accountEntry\"]{background:transparent;color:#1F2329;border:none;"
        "border-radius:0px;padding:0px;text-align:left;}"
        "QPushButton[role=\"accountEntry\"][lined=\"true\"]{border-bottom:1px solid #E6E8EE;}"
        "QPushButton[role=\"accountEntry\"][edge=\"first\"]{border-top-left-radius:8px;"
        "border-top-right-radius:8px;}"
        "QPushButton[role=\"accountEntry\"][edge=\"last\"]{border-bottom-left-radius:8px;"
        "border-bottom-right-radius:8px;}"
        "QPushButton[role=\"accountEntry\"]:hover{background:#F3F5F8;}"
        "QPushButton[role=\"accountEntry\"]:pressed{background:#E8EFFD;}"
        "QPushButton[role=\"accountEntry\"]:focus{background:#E8EFFD;}"
        "QLabel#accountEntryTitleFanLX{font-size:15px;font-weight:600;color:#1F2329;background:transparent;}"
        "QLabel#accountEntryDetailFanLX{font-size:12px;color:#8A94A3;background:transparent;line-height:1.35;}"
        "QLabel#accountEntryArrowFanLX{font-size:18px;color:#8A94A3;background:transparent;}");
}

// 资料卡与下方操作区的宽度分配。窄于 780px 时改成上下排列。
class AccountColumnsFanLX : public QWidget {
  public:
    explicit AccountColumnsFanLX(QWidget *parent = nullptr) : QWidget(parent) {
        grid_ = new QGridLayout(this);
        grid_->setContentsMargins(0, 0, 0, 0);
        grid_->setHorizontalSpacing(16);
        grid_->setVerticalSpacing(16);
    }

    void setPanels(QWidget *profile, QWidget *entries) {
        profile_ = profile;
        entries_ = entries;
        profile_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        entries_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        apply(false);
    }

    void setStacked(bool stacked) {
        if (!profile_ || !entries_ || stacked == stacked_)
            return;
        apply(stacked);
    }

  private:
    void apply(bool stacked) {
        stacked_ = stacked;
        grid_->removeWidget(profile_);
        grid_->removeWidget(entries_);
        entries_->setMinimumWidth(0);
        entries_->setMaximumWidth(QWIDGETSIZE_MAX);
        grid_->setRowStretch(0, 0);
        grid_->setRowStretch(1, 0);
        if (stacked) {
            profile_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            entries_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            grid_->addWidget(profile_, 0, 0);
            grid_->addWidget(entries_, 1, 0);
            grid_->setColumnStretch(0, 1);
            grid_->setColumnStretch(1, 0);
        } else {
            // 右栏宽度跟群页一样固定，窗口拉宽时只让左侧资料卡变宽、变高。
            entries_->setFixedWidth(360);
            entries_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            profile_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            grid_->addWidget(profile_, 0, 0);
            grid_->addWidget(entries_, 0, 1);
            grid_->setColumnStretch(0, 1);
            grid_->setColumnStretch(1, 0);
            grid_->setRowStretch(0, 1);
        }
    }

    QGridLayout *grid_ = nullptr;
    QWidget *profile_ = nullptr;
    QWidget *entries_ = nullptr;
    bool stacked_ = false;
};

// 首页铺满浅灰区域。宽于 780 时右栏固定 360，和群页一样；更窄时改成上下排列。
class AccountHomeFanLX : public QWidget {
  public:
    explicit AccountHomeFanLX(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("accountHomeFanLX"));
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(accountHomeStyleFanLX());
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    AccountColumnsFanLX *columnsFanLX = nullptr;

  protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (columnsFanLX)
            columnsFanLX->setStacked(width() < 780);
    }
};

// 开通管理表格的稳定行高。四个已知身份大约 220px，能完整放进子页面，不必再套一层纵向滚动。
constexpr int serviceRowHeightFanLX = 44;
// 行数超过这个上限时，表格才使用自己的纵向滚动条，避免把整页撑到无法回退。
constexpr int serviceTableCapRowsFanLX = 12;

// 按表头、行高、边框和可能出现的横向滚动条计算表格高度。
// 常规行数把高度锁死，纵向滚动条关闭；超出上限后改为限定高度并允许表格内部滚动。
void fitServiceTableFanLX(QTableWidget *table) {
    if (!table || table->property("fittingFanLX").toBool())
        return;
    table->setProperty("fittingFanLX", true);

    auto *vertical = table->verticalHeader();
    vertical->setSectionResizeMode(QHeaderView::Fixed);
    vertical->setDefaultSectionSize(serviceRowHeightFanLX);
    for (int row = 0; row < table->rowCount(); ++row)
        table->setRowHeight(row, serviceRowHeightFanLX);

    auto *horizontal = table->horizontalHeader();
    int headerHeight = qMax(horizontal->sizeHint().height(), horizontal->height());
    if (headerHeight <= 0)
        headerHeight = 40;
    const int frame = table->frameWidth() * 2;
    const int rows = table->rowCount();
    int extra = 0;
    if (auto *bar = table->horizontalScrollBar()) {
        const int viewportWidth = table->viewport() ? table->viewport()->width() : 0;
        const bool overflow = bar->maximum() > 0 ||
                              (viewportWidth > 0 && horizontal->length() > viewportWidth + 1);
        if (overflow)
            extra = qMax(bar->sizeHint().height(), 16);
    }

    const int content = headerHeight + rows * serviceRowHeightFanLX + frame + extra + 4;
    const int cap = headerHeight + serviceTableCapRowsFanLX * serviceRowHeightFanLX + frame + extra + 4;
    const int height = qMax(content, headerHeight + frame + 4);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    if (rows == 0 || content <= cap) {
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        table->setMinimumHeight(height);
        table->setMaximumHeight(height);
    } else {
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        const int visibleRows = qMin(6, rows);
        const int minHeight = headerHeight + visibleRows * serviceRowHeightFanLX + frame + extra + 4;
        table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        table->setMinimumHeight(minHeight);
        table->setMaximumHeight(cap);
    }
    table->setProperty("fittingFanLX", false);
}

// 样式表会盖住 QTableWidgetItem 的背景色，所以当前行和选中行在这里绘制。
class ServiceRowDelegateFanLX : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const bool marked = index.data(Qt::UserRole).toBool();
        const bool selected = (opt.state & QStyle::State_Selected) != 0;
        QColor fill(theme::panel);
        if (selected || marked)
            fill = QColor(theme::brandSoft);
        painter->save();
        painter->fillRect(opt.rect, fill);
        if (marked && index.column() == 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(theme::brand));
            painter->drawRoundedRect(QRectF(opt.rect.left() + 2, opt.rect.top() + 10, 3,
                                            qMax(8, opt.rect.height() - 20)),
                                     1.5, 1.5);
        }
        QColor ink(theme::text);
        const auto foreground = index.data(Qt::ForegroundRole);
        if (foreground.canConvert<QBrush>())
            ink = foreground.value<QBrush>().color();
        else if (foreground.canConvert<QColor>())
            ink = foreground.value<QColor>();
        painter->setPen(ink);
        const int textLeft = marked && index.column() == 0 ? 14 : 8;
        const QRect textRect = opt.rect.adjusted(textLeft, 0, -8, -1);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, textRect.width()));
        painter->setPen(QColor(theme::line));
        painter->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());
        painter->restore();
    }
};

// 宽度变化后重新计算。横向滚动条是否出现取决于当时的列宽，必须在表格自己的尺寸确定后再量。
class ServiceTableFanLX : public QTableWidget {
  public:
    explicit ServiceTableFanLX(QWidget *parent = nullptr) : QTableWidget(0, 6, parent) {}

  protected:
    void resizeEvent(QResizeEvent *event) override {
        // 高度修正会再次进入这里。基类必须先跑完，否则视口停在旧高度，最后一行会被裁掉。
        QTableWidget::resizeEvent(event);
        if (!property("fittingFanLX").toBool())
            fitServiceTableFanLX(this);
    }

    void showEvent(QShowEvent *event) override {
        QTableWidget::showEvent(event);
        ensurePolished();
        fitServiceTableFanLX(this);
    }
};

// 只作用于开通管理内容区。登录、绑定、资料子页面不在这棵控件树里。
QString serviceBodyStyleFanLX() {
    return QStringLiteral(
        "QWidget#accountServiceBodyFanLX{background:#F6F7F9;}"
        "QFrame#serviceListCardFanLX,QFrame#serviceOpenCardFanLX{background:#FFFFFF;"
        "border:1px solid #E6E8EE;border-radius:8px;}"
        "QLabel#serviceCardTitleFanLX{font-size:15px;font-weight:600;color:#1F2329;background:transparent;}"
        "QLabel#serviceFieldCaptionFanLX{font-size:12px;color:#5B6472;background:transparent;}"
        "QLabel#serviceOpenHintFanLX{font-size:12px;color:#8A94A3;background:transparent;}"
        "QFrame#serviceOpenCardFanLX QComboBox{background:#FFFFFF;color:#1F2329;border:1px solid #E6E8EE;"
        "border-radius:8px;padding:0px 10px;min-height:36px;}"
        "QFrame#serviceOpenCardFanLX QComboBox:hover{border-color:#D7DEE8;}"
        "QFrame#serviceOpenCardFanLX QComboBox:focus{border-color:#2563EB;}"
        "QFrame#serviceOpenCardFanLX QComboBox::drop-down{border:none;width:22px;}"
        "QFrame#serviceOpenCardFanLX QComboBox QAbstractItemView{background:#FFFFFF;color:#1F2329;"
        "selection-background-color:#E8EFFD;selection-color:#1F2329;border:1px solid #E6E8EE;}"
        "QFrame#serviceOpenCardFanLX QLineEdit{border:1px solid #E6E8EE;border-radius:8px;padding:0px 10px;"
        "background:#FFFFFF;color:#1F2329;min-height:36px;}"
        "QFrame#serviceOpenCardFanLX QLineEdit:focus{border:1px solid #2563EB;}"
        "QWidget#accountServiceBodyFanLX QPushButton[role=\"primary\"]{background:#2563EB;color:#FFFFFF;"
        "border:none;border-radius:8px;padding:0px 16px;min-height:36px;font-size:14px;font-weight:600;}"
        "QWidget#accountServiceBodyFanLX QPushButton[role=\"primary\"]:hover{background:#1D4ED8;}"
        "QWidget#accountServiceBodyFanLX QPushButton[role=\"primary\"]:pressed{background:#1E40AF;}"
        "QWidget#accountServiceBodyFanLX QPushButton[role=\"primary\"]:disabled{background:#E6E8EE;color:#8A94A3;}"
        "QTableWidget#serviceAccountTableFanLX{background:#FFFFFF;alternate-background-color:#FFFFFF;"
        "border:none;gridline-color:transparent;font-size:13px;color:#1F2329;"
        "selection-background-color:#E8EFFD;selection-color:#1F2329;}"
        "QTableWidget#serviceAccountTableFanLX::item{border:none;border-bottom:1px solid #E6E8EE;padding:0px 8px;}"
        "QTableWidget#serviceAccountTableFanLX::item:selected{background:#E8EFFD;color:#1F2329;}"
        "QTableWidget#serviceAccountTableFanLX QHeaderView::section{background:#F3F5F8;color:#5B6472;"
        "border:none;border-bottom:1px solid #E6E8EE;padding:8px;font-size:12px;font-weight:600;}");
}

// 左表右表单。窄于 1000px 时改为上下排列，避免六列被 35% 侧栏挤到无法阅读。
// 外层已经是子页面的 QScrollArea，这里不再加底部 stretch，多余高度留在灰色页面上。
class AccountServiceBodyFanLX : public QWidget {
  public:
    explicit AccountServiceBodyFanLX(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("accountServiceBodyFanLX"));
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(serviceBodyStyleFanLX());
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        grid_ = new QGridLayout(this);
        grid_->setContentsMargins(2, 4, 2, 12);
        grid_->setHorizontalSpacing(16);
        grid_->setVerticalSpacing(16);
        grid_->setRowStretch(0, 0);
        grid_->setRowStretch(1, 0);
    }

    void setCards(QWidget *listCard, QWidget *openCard) {
        list_ = listCard;
        open_ = openCard;
        list_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        open_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        open_->setMinimumWidth(260);
        place(false);
    }

  protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        const bool stacked = width() < 1000;
        if (stacked != stacked_)
            place(stacked);
    }

  private:
    void place(bool stacked) {
        stacked_ = stacked;
        if (!list_ || !open_ || !grid_)
            return;
        grid_->removeWidget(list_);
        grid_->removeWidget(open_);
        if (stacked) {
            grid_->addWidget(list_, 0, 0, Qt::AlignTop);
            grid_->addWidget(open_, 1, 0, Qt::AlignTop);
            grid_->setColumnStretch(0, 1);
            grid_->setColumnStretch(1, 0);
        } else {
            grid_->addWidget(list_, 0, 0);
            grid_->addWidget(open_, 0, 1);
            grid_->setColumnStretch(0, 1);
            grid_->setColumnStretch(1, 1);
            grid_->setRowStretch(0, 0);
        }
    }

    QGridLayout *grid_ = nullptr;
    QWidget *list_ = nullptr;
    QWidget *open_ = nullptr;
    bool stacked_ = false;
};

// 登录 / 绑定 / 资料三个子页面共用。样式挂在本页自己的 objectName 上，不改 PageKit。
QString accountManageStyleFanLX() {
    return QStringLiteral(
        "QWidget#accountManageBodyFanLX{background:#F6F7F9;}"
        "QFrame#accountManageCardFanLX{background:#FFFFFF;border:1px solid #E6E8EE;border-radius:8px;}"
        "QLabel#accountManageTitleFanLX{font-size:15px;font-weight:600;color:#1F2329;background:transparent;}"
        "QLabel#accountManageHintFanLX{font-size:12px;color:#8A94A3;background:transparent;}"
        "QLabel#accountManageFieldFanLX{font-size:12px;color:#5B6472;background:transparent;}"
        "QLabel#accountManageResultFanLX{font-size:13px;color:#5B6472;background:transparent;}"
        "QLabel#accountManageResultFanLX[tone=\"bad\"]{color:#DC2626;}"
        "QWidget#accountManageBodyFanLX QLineEdit{background:#FFFFFF;color:#1F2329;border:1px solid #E6E8EE;"
        "border-radius:8px;padding:0px 10px;min-height:36px;}"
        "QWidget#accountManageBodyFanLX QLineEdit:focus{border:1px solid #2563EB;}"
        "QWidget#accountManageBodyFanLX QComboBox{background:#FFFFFF;color:#1F2329;border:1px solid #E6E8EE;"
        "border-radius:8px;padding:0px 10px;min-height:36px;}"
        "QWidget#accountManageBodyFanLX QComboBox:hover{border-color:#D7DEE8;}"
        "QWidget#accountManageBodyFanLX QComboBox:focus{border-color:#2563EB;}"
        "QWidget#accountManageBodyFanLX QComboBox::drop-down{border:none;width:22px;}"
        "QWidget#accountManageBodyFanLX QComboBox QAbstractItemView{background:#FFFFFF;color:#1F2329;"
        "selection-background-color:#E8EFFD;selection-color:#1F2329;border:1px solid #E6E8EE;}"
        "QWidget#accountManageBodyFanLX QPushButton[role=\"primary\"]{background:#2563EB;color:#FFFFFF;"
        "border:none;border-radius:8px;padding:0px 16px;min-height:36px;font-size:14px;font-weight:600;}"
        "QWidget#accountManageBodyFanLX QPushButton[role=\"primary\"]:hover{background:#1D4ED8;}"
        "QWidget#accountManageBodyFanLX QPushButton[role=\"primary\"]:pressed{background:#1E40AF;}"
        "QWidget#accountManageBodyFanLX QPushButton[role=\"primary\"]:disabled{background:#E6E8EE;color:#8A94A3;}"
        "QWidget#accountManageBodyFanLX QPushButton[role=\"danger\"]{background:transparent;color:#DC2626;"
        "border:none;border-radius:8px;padding:0px 12px;min-height:36px;font-size:14px;font-weight:600;}"
        "QWidget#accountManageBodyFanLX QPushButton[role=\"danger\"]:hover{background:#FEE2E2;}"
        "QWidget#accountManageBodyFanLX QPushButton[role=\"danger\"]:pressed{background:#FEE2E2;}"
        "QWidget#accountManageBodyFanLX QPushButton[role=\"danger\"]:disabled{color:#FECACA;background:transparent;}"
        "QListWidget#bindingListFanLX{background:transparent;border:none;outline:none;padding:0px;}"
        "QListWidget#bindingListFanLX::item{background:transparent;border:none;padding:0px 0px 10px 0px;}"
        "QListWidget#bindingListFanLX::item:selected{background:transparent;color:#1F2329;}"
        "QWidget#bindingRowFanLX{background:#F8FAFC;border:1px solid #E6E8EE;border-radius:12px;}"
        "QWidget#bindingSideFanLX{background:#FFFFFF;border:1px solid #E6E8EE;border-radius:10px;}"
        "QLabel#bindingServiceFanLX{font-size:11px;font-weight:600;color:#8A94A3;letter-spacing:0.4px;"
        "background:transparent;}"
        "QLabel#bindingAccountFanLX{font-size:14px;font-weight:600;color:#1F2329;background:transparent;}"
        "QLabel#bindingIdFanLX{font-size:12px;color:#5B6472;background:transparent;}"
        "QLabel#bindingMarkFanLX{font-size:11px;font-weight:600;color:#2563EB;background:#E8EFFD;"
        "border:1px solid #D6E4FF;border-radius:12px;padding:5px 12px;}"
        "QLabel#bindingEmptyFanLX{font-size:13px;color:#8A94A3;background:transparent;}");
}

void setAccountHintFanLX(QLabel *label, const QString &text, const QString &tone) {
    if (!label)
        return;
    label->setText(text);
    label->setProperty("tone", tone);
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
}

// 子页标题自己带样式，避免 refresh 时 applyAccent 把整页样式表换掉后标题丢失颜色。
QString accountSubHeaderStyleFanLX() {
    return QStringLiteral(
        "QWidget#accountSubHeaderFanLX{background:transparent;}"
        "QPushButton#accountSubBackFanLX{background:transparent;color:#2563EB;border:none;"
        "border-radius:8px;padding:4px 8px;font-size:14px;font-weight:600;}"
        "QPushButton#accountSubBackFanLX:hover{background:#E8EFFD;}"
        "QPushButton#accountSubBackFanLX:pressed{background:#E8EFFD;}"
        "QLabel#accountSubTitleFanLX{color:#1F2329;font-size:20px;font-weight:600;background:transparent;}"
        "QLabel#accountHeaderSubtitleFanLX{color:#5B6472;font-size:12px;background:transparent;}");
}

class AccountSubHeaderFanLX : public QWidget {
  public:
    explicit AccountSubHeaderFanLX(const QString &title, QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("accountSubHeaderFanLX"));
        setStyleSheet(accountSubHeaderStyleFanLX());
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(4, 0, 4, 8);
        row->setSpacing(12);
        back_ = new QPushButton(QStringLiteral("‹ 账号"), this);
        back_->setObjectName(QStringLiteral("accountSubBackFanLX"));
        back_->setCursor(Qt::PointingHandCursor);
        back_->setFocusPolicy(Qt::StrongFocus);
        auto *titles = new QVBoxLayout();
        titles->setContentsMargins(0, 0, 0, 0);
        titles->setSpacing(0);
        auto *titleLabel = new QLabel(title, this);
        titleLabel->setObjectName(QStringLiteral("accountSubTitleFanLX"));
        auto *subtitle = new QLabel(this);
        subtitle->setObjectName(QStringLiteral("accountHeaderSubtitleFanLX"));
        titles->addWidget(titleLabel);
        titles->addWidget(subtitle);
        row->addWidget(back_, 0, Qt::AlignVCenter);
        row->addLayout(titles, 1);
    }

    QPushButton *backButton() const { return back_; }

  private:
    QPushButton *back_ = nullptr;
};

constexpr int bindingRowHeightFanLX = 96;
constexpr int bindingListCapRowsFanLX = 6;

QFrame *manageCardFanLX(QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("accountManageCardFanLX"));
    card->setFrameShape(QFrame::NoFrame);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    return card;
}

QLabel *manageTitleFanLX(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("accountManageTitleFanLX"));
    return label;
}

// 宽屏时内容最多 1240px 并居中。外层子页面已经能滚动，这里不再把空白 stretch 塞进卡片里。
class AccountManagePageFanLX : public QWidget {
  public:
    explicit AccountManagePageFanLX(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("accountManageBodyFanLX"));
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(accountManageStyleFanLX());
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(2, 4, 2, 16);
        outer->setSpacing(0);
        auto *center = new QHBoxLayout();
        center->setContentsMargins(0, 0, 0, 0);
        column_ = new QWidget(this);
        // 最小宽度保持 0，滚动区域才能在窄窗口里把页面缩小；超过 1240 后两侧空白把内容留在中间。
        column_->setMinimumWidth(0);
        column_->setMaximumWidth(1040);
        column_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        columnLayout_ = new QVBoxLayout(column_);
        columnLayout_->setContentsMargins(0, 0, 0, 0);
        columnLayout_->setSpacing(14);
        center->addStretch(0);
        center->addWidget(column_, 1, Qt::AlignTop);
        center->addStretch(0);
        outer->addLayout(center);
    }

    QWidget *column() const { return column_; }
    QVBoxLayout *columnLayout() const { return columnLayout_; }

  private:
    QWidget *column_ = nullptr;
    QVBoxLayout *columnLayout_ = nullptr;
};

// 两张卡片并排。宽度不够时改为上下排列，避免把表单压扁。
class AccountSplitFanLX : public QWidget {
  public:
    explicit AccountSplitFanLX(QWidget *parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        grid_ = new QGridLayout(this);
        grid_->setContentsMargins(0, 0, 0, 0);
        grid_->setHorizontalSpacing(16);
        grid_->setVerticalSpacing(16);
        grid_->setRowStretch(0, 0);
        grid_->setRowStretch(1, 0);
    }

    void setPanels(QWidget *left, QWidget *right, int leftStretch, int rightStretch, int stackBelow) {
        left_ = left;
        right_ = right;
        leftStretch_ = leftStretch;
        rightStretch_ = rightStretch;
        stackBelow_ = stackBelow;
        left_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        right_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        place(false);
    }

  protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        const bool stacked = width() < stackBelow_;
        if (stacked != stacked_)
            place(stacked);
    }

  private:
    void place(bool stacked) {
        stacked_ = stacked;
        if (!left_ || !right_ || !grid_)
            return;
        grid_->removeWidget(left_);
        grid_->removeWidget(right_);
        if (stacked) {
            grid_->addWidget(left_, 0, 0, Qt::AlignTop);
            grid_->addWidget(right_, 1, 0, Qt::AlignTop);
            grid_->setColumnStretch(0, 1);
            grid_->setColumnStretch(1, 0);
        } else {
            grid_->addWidget(left_, 0, 0, Qt::AlignTop);
            grid_->addWidget(right_, 0, 1, Qt::AlignTop);
            grid_->setColumnStretch(0, leftStretch_);
            grid_->setColumnStretch(1, rightStretch_);
        }
    }

    QGridLayout *grid_ = nullptr;
    QWidget *left_ = nullptr;
    QWidget *right_ = nullptr;
    int leftStretch_ = 1;
    int rightStretch_ = 1;
    int stackBelow_ = 900;
    bool stacked_ = false;
};

void fitBindingListFanLX(QListWidget *list) {
    if (!list)
        return;
    const int count = qMax(1, list->count());
    const int frame = list->frameWidth() * 2;
    const auto heightFor = [&](int rows) {
        return rows * bindingRowHeightFanLX + frame;
    };
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (count <= bindingListCapRowsFanLX) {
        const int height = heightFor(count);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        list->setMinimumHeight(height);
        list->setMaximumHeight(height);
    } else {
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        list->setMinimumHeight(heightFor(4));
        list->setMaximumHeight(heightFor(bindingListCapRowsFanLX));
    }
}

// 一侧账号：上排服务名徽章，下排号码；外层白卡片，避免「服务账号（号码）」挤在一行。
QWidget *bindingSideFanLX(const QString &service, const QString &accountId, const QString &role,
                          QWidget *parent) {
    auto *box = new QWidget(parent);
    box->setObjectName(QStringLiteral("bindingSideFanLX"));
    box->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);
    auto *cap = new QLabel(role + QStringLiteral(" · ") + service, box);
    cap->setObjectName(QStringLiteral("bindingServiceFanLX"));
    auto *text = new QLabel(accountId, box);
    text->setObjectName(QStringLiteral("bindingAccountFanLX"));
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->setWordWrap(true);
    layout->addWidget(cap);
    layout->addWidget(text);
    return box;
}

QWidget *bindingPairRowFanLX(const QString &text, QWidget *parent) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("bindingRowFanLX"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    static const QRegularExpression pattern(
        QStringLiteral("^(.+?)（([^）]+)） <--> (.+?)（([^）]+)）$"));
    const auto match = pattern.match(text);
    if (!match.hasMatch()) {
        auto *plain = new QLabel(text, row);
        plain->setObjectName(QStringLiteral("bindingAccountFanLX"));
        plain->setWordWrap(true);
        layout->addWidget(plain);
        return row;
    }
    // 左：服务账号号码 + 服务名；中：圆角「已绑定」芯片；右：关联账号。
    // DomainController 格式：服务名（号码） <--> 对端服务名（对端号码）
    layout->addWidget(bindingSideFanLX(match.captured(1), match.captured(2),
                                       QStringLiteral("服务账号"), row),
                      1);
    auto *mark = new QLabel(QStringLiteral("↔ 已绑定"), row);
    mark->setObjectName(QStringLiteral("bindingMarkFanLX"));
    mark->setAlignment(Qt::AlignCenter);
    mark->setAttribute(Qt::WA_StyledBackground, true);
    mark->setMinimumWidth(72);
    layout->addWidget(mark, 0, Qt::AlignVCenter);
    layout->addWidget(bindingSideFanLX(match.captured(3), match.captured(4),
                                       QStringLiteral("关联账号"), row),
                      1);
    return row;
}

QWidget *bindingEmptyRowFanLX(QWidget *parent) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("bindingRowFanLX"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(16, 18, 16, 18);
    layout->setSpacing(6);
    auto *label = new QLabel(QStringLiteral("还没有绑定关系"), row);
    label->setObjectName(QStringLiteral("bindingAccountFanLX"));
    label->setAlignment(Qt::AlignCenter);
    auto *hint = new QLabel(QStringLiteral("在「管理绑定」里选择服务并填写对方号码，绑定后会显示在这里。"),
                            row);
    hint->setObjectName(QStringLiteral("bindingEmptyFanLX"));
    hint->setWordWrap(true);
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    layout->addWidget(hint);
    return row;
}

void refillBindingListFanLX(QListWidget *list, const QStringList &rows) {
    if (!list)
        return;
    list->clear();
    if (rows.isEmpty()) {
        auto *item = new QListWidgetItem(QStringLiteral("（还没有绑定关系）"), list);
        item->setFlags(Qt::ItemIsEnabled);
        item->setSizeHint(QSize(0, bindingRowHeightFanLX));
        list->setItemWidget(item, bindingEmptyRowFanLX(list));
    } else {
        for (const auto &text : rows) {
            auto *item = new QListWidgetItem(text, list);
            item->setSizeHint(QSize(0, bindingRowHeightFanLX));
            list->setItemWidget(item, bindingPairRowFanLX(text, list));
        }
    }
    fitBindingListFanLX(list);
}

} // namespace

AccountPageFanLX::AccountPageFanLX(DomainControllerFanLX *controller, QWidget *parent)
    : QWidget(parent), controllerFanLX(controller) {
    auto pagePalette = palette();
    pagePalette.setColor(QPalette::Window, QColor(QStringLiteral("#F6F7F9")));
    setAutoFillBackground(true);
    setPalette(pagePalette);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(buildHomeFanLX());
    const QColor pageBg(QStringLiteral("#F6F7F9"));
    scroll->viewport()->setAutoFillBackground(true);
    auto viewportPalette = scroll->viewport()->palette();
    viewportPalette.setColor(QPalette::Window, pageBg);
    viewportPalette.setColor(QPalette::Base, pageBg);
    scroll->viewport()->setPalette(viewportPalette);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(scroll);

    // 身份变化与任何写操作成功后都重读视图：界面显示的一定是内存里的最新状态。
    connect(controllerFanLX, &DomainControllerFanLX::dataChangedFanLX, this,
            &AccountPageFanLX::refreshFanLX);
    connect(controllerFanLX, &DomainControllerFanLX::identityChangedFanLX, this,
            &AccountPageFanLX::refreshFanLX);
    refreshFanLX();
}

QWidget *AccountPageFanLX::buildHomeFanLX() {
    auto *home = new AccountHomeFanLX();
    auto *columnLayout = new QVBoxLayout(home);
    columnLayout->setContentsMargins(16, 12, 16, 16);
    columnLayout->setSpacing(12);

    // 当前身份栏：服务名、账号与切换下拉框放在同一条里。选中即切换。
    // 切换被拒（比如该服务还没登录）时，下拉框必须回到真正的当前服务，并弹出原因。
    auto *identity = new QFrame(home);
    identity->setObjectName(QStringLiteral("accountIdentityFanLX"));
    identityLabelFanLX = new QLabel(identity);
    identityLabelFanLX->setObjectName(QStringLiteral("accountIdentityTextFanLX"));
    identityLabelFanLX->setWordWrap(true);
    platformComboFanLX = new QComboBox(identity);
    platformComboFanLX->setMinimumWidth(168);
    auto *identityText = new QVBoxLayout();
    identityText->setSpacing(2);
    auto *identityHint = new QLabel(QStringLiteral("当前身份"), identity);
    identityHint->setObjectName(QStringLiteral("accountIdentityHintFanLX"));
    identityText->addWidget(identityHint);
    identityText->addWidget(identityLabelFanLX);
    auto *switchHint = new QLabel(QStringLiteral("切换服务"), identity);
    switchHint->setObjectName(QStringLiteral("accountIdentityHintFanLX"));
    auto *identityRow = new QHBoxLayout(identity);
    identityRow->setContentsMargins(16, 12, 14, 12);
    identityRow->setSpacing(12);
    identityRow->addLayout(identityText, 1);
    identityRow->addWidget(switchHint);
    identityRow->addWidget(platformComboFanLX);
    connect(platformComboFanLX, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        const auto service = platformComboFanLX->itemData(index).toString();
        if (service.isEmpty() || controllerFanLX->setCurrentService(service))
            return;
        refreshFanLX();
        QMessageBox::warning(this, QStringLiteral("无法切换服务"), controllerFanLX->lastMessage());
    });
    columnLayout->addWidget(identity);

    // 登录态提示条：当前服务未登录 / 已全部注销 时显示，并给出下一步。
    bannerFanLX = new QFrame(home);
    bannerFanLX->setObjectName(QStringLiteral("bannerFanLX"));
    bannerLabelFanLX = new QLabel(bannerFanLX);
    bannerLabelFanLX->setWordWrap(true);
    reloginButtonFanLX = kit::primaryButton(QStringLiteral("重新登录微X"), bannerFanLX);
    auto *bannerLayout = new QHBoxLayout(bannerFanLX);
    bannerLayout->setContentsMargins(14, 10, 12, 10);
    bannerLayout->addWidget(bannerLabelFanLX, 1);
    bannerLayout->addWidget(reloginButtonFanLX);
    columnLayout->addWidget(bannerFanLX);
    connect(reloginButtonFanLX, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto password = QInputDialog::getText(
            this, QStringLiteral("重新登录"),
            QStringLiteral("微X号 %1 的登录密码：").arg(controllerFanLX->weixAccountId()),
            QLineEdit::Password, QString(), &ok);
        if (!ok)
            return;
        if (!controllerFanLX->relogin(password))
            QMessageBox::warning(this, QStringLiteral("重新登录失败"), controllerFanLX->lastMessage());
    });

    columnLayout->addWidget(buildProfileCardFanLX(home));

    auto *mid = new QHBoxLayout();
    mid->setSpacing(12);
    auto *serviceColumn = new QVBoxLayout();
    serviceColumn->setSpacing(12);
    serviceColumn->addWidget(buildServiceBodyFanLX());
    serviceColumn->addWidget(buildLoginBodyFanLX());
    auto *binding = buildBindingBodyFanLX();
    binding->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mid->addLayout(serviceColumn, 3);
    mid->addWidget(binding, 2);
    columnLayout->addLayout(mid);
    columnLayout->addWidget(buildMaintenanceBodyFanLX());
    return home;
}

QWidget *AccountPageFanLX::buildProfileCardFanLX(QWidget *parent) {
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("accountProfileFanLX"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(8);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    const auto labelOf = [frame](const QString &role, const QString &text = QString()) {
        auto *label = new QLabel(text, frame);
        label->setProperty("role", role);
        return label;
    };

    auto *heading = new QLabel(QStringLiteral("用户基本信息"), frame);
    heading->setObjectName(QStringLiteral("accountProfileHeadingFanLX"));
    avatarLabelFanLX = labelOf(QStringLiteral("avatar"));
    avatarLabelFanLX->setFixedSize(36, 36);
    avatarLabelFanLX->setAlignment(Qt::AlignCenter);
    avatarLabelFanLX->setAttribute(Qt::WA_StyledBackground, true);
    nameLabelFanLX = labelOf(QStringLiteral("name"));
    subtitleLabelFanLX = labelOf(QStringLiteral("caption"));
    auto *who = new QVBoxLayout();
    who->setSpacing(0);
    who->addWidget(nameLabelFanLX);
    who->addWidget(subtitleLabelFanLX);
    auto *head = new QHBoxLayout();
    head->setSpacing(10);
    head->addWidget(heading);
    head->addWidget(avatarLabelFanLX);
    head->addLayout(who, 1);
    layout->addLayout(head);

    // 统计和详情排成同一行，不再各占一块。
    const auto fact = [&](QLabel *&valueLabel, const QString &caption, bool stat) {
        auto *tile = new QWidget(frame);
        auto *tileLayout = new QHBoxLayout(tile);
        tileLayout->setContentsMargins(0, 0, 0, 0);
        tileLayout->setSpacing(4);
        auto *captionLabel = new QLabel(caption, tile);
        captionLabel->setObjectName(stat ? QStringLiteral("accountStatCaptionFanLX")
                                         : QStringLiteral("accountProfileHeadingFanLX"));
        valueLabel = new QLabel(tile);
        valueLabel->setObjectName(stat ? QStringLiteral("accountStatValueFanLX")
                                       : QStringLiteral("accountIdentityTextFanLX"));
        valueLabel->setProperty("role", QStringLiteral("value"));
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        tileLayout->addWidget(captionLabel);
        tileLayout->addWidget(valueLabel);
        return tile;
    };
    auto *facts = new QHBoxLayout();
    facts->setSpacing(16);
    facts->addWidget(fact(tAgeValueFanLX, QStringLiteral("T龄（年）"), true));
    facts->addWidget(fact(friendValueFanLX, QStringLiteral("好友"), true));
    facts->addWidget(fact(groupValueFanLX, QStringLiteral("群"), true));
    facts->addWidget(fact(birthValueFanLX, QStringLiteral("出生日期"), false));
    facts->addWidget(fact(applyValueFanLX, QStringLiteral("号码申请日期"), false));
    facts->addWidget(fact(locationValueFanLX, QStringLiteral("所在地"), false));
    facts->addWidget(fact(userIdValueFanLX, QStringLiteral("后台用户 ID"), false));
    facts->addStretch(1);
    layout->addLayout(facts);
    return frame;
}

QWidget *AccountPageFanLX::buildServiceBodyFanLX() {
    auto *body = new QWidget(this);
    body->setObjectName(QStringLiteral("accountServiceBodyFanLX"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(serviceBodyStyleFanLX());

    auto *listCard = new QFrame(body);
    listCard->setObjectName(QStringLiteral("serviceListCardFanLX"));
    listCard->setFrameShape(QFrame::NoFrame);
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(16, 16, 16, 12);
    listLayout->setSpacing(12);
    auto *listTitle = new QLabel(QStringLiteral("开通管理"), listCard);
    listTitle->setObjectName(QStringLiteral("serviceCardTitleFanLX"));
    serviceTableFanLX = new ServiceTableFanLX(listCard);
    serviceTableFanLX->setObjectName(QStringLiteral("serviceAccountTableFanLX"));
    serviceTableFanLX->setHorizontalHeaderLabels({QStringLiteral("服务"), QStringLiteral("号码"),
                                                  QStringLiteral("号码关系"), QStringLiteral("绑定状态"),
                                                  QStringLiteral("开通状态"), QStringLiteral("登录状态")});
    serviceTableFanLX->setFrameShape(QFrame::NoFrame);
    serviceTableFanLX->setShowGrid(false);
    serviceTableFanLX->setWordWrap(false);
    serviceTableFanLX->setTextElideMode(Qt::ElideRight);
    serviceTableFanLX->setAlternatingRowColors(false);
    serviceTableFanLX->setEditTriggers(QAbstractItemView::NoEditTriggers);
    serviceTableFanLX->setSelectionMode(QAbstractItemView::SingleSelection);
    serviceTableFanLX->setSelectionBehavior(QAbstractItemView::SelectRows);
    serviceTableFanLX->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    serviceTableFanLX->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    serviceTableFanLX->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    serviceTableFanLX->setCornerButtonEnabled(false);
    serviceTableFanLX->setFocusPolicy(Qt::StrongFocus);
    serviceTableFanLX->setItemDelegate(new ServiceRowDelegateFanLX(serviceTableFanLX));
    serviceTableFanLX->verticalHeader()->hide();
    serviceTableFanLX->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    serviceTableFanLX->verticalHeader()->setDefaultSectionSize(serviceRowHeightFanLX);
    auto *header = serviceTableFanLX->horizontalHeader();
    header->setStretchLastSection(false);
    header->setHighlightSections(false);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setMinimumSectionSize(76);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    auto *openEntry = kit::primaryButton(QStringLiteral("开通新服务"), listCard);
    openEntry->setMinimumHeight(32);
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(12);
    titleRow->addWidget(listTitle, 0, Qt::AlignVCenter);
    titleRow->addWidget(openEntry, 0, Qt::AlignVCenter);
    titleRow->addStretch(1);
    listLayout->addLayout(titleRow);
    listLayout->addWidget(serviceTableFanLX);

    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("accountServiceBodyFanLX"));
    dialog->setWindowTitle(QStringLiteral("开通新服务"));
    dialog->setModal(true);
    dialog->setFixedWidth(380);
    dialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog->setStyleSheet(serviceBodyStyleFanLX() + QStringLiteral("QDialog{background:#F6F7F9;}"));
    auto *openCard = new QFrame(dialog);
    openCard->setObjectName(QStringLiteral("serviceOpenCardFanLX"));
    openCard->setFrameShape(QFrame::NoFrame);
    auto *openLayout = new QVBoxLayout(openCard);
    openLayout->setContentsMargins(16, 16, 16, 16);
    openLayout->setSpacing(8);
    auto *openTitle = new QLabel(QStringLiteral("开通新服务"), openCard);
    openTitle->setObjectName(QStringLiteral("serviceCardTitleFanLX"));
    auto *typeCaption = new QLabel(QStringLiteral("服务类型"), openCard);
    typeCaption->setObjectName(QStringLiteral("serviceFieldCaptionFanLX"));
    subscribeComboFanLX = makePlatformComboFanLX(openCard);
    subscribeComboFanLX->setMinimumHeight(36);
    auto *numberCaption = new QLabel(QStringLiteral("服务号码"), openCard);
    numberCaption->setObjectName(QStringLiteral("serviceFieldCaptionFanLX"));
    subscribeNumberEditFanLX = new QLineEdit(openCard);
    subscribeNumberEditFanLX->setMinimumHeight(36);
    auto *hint = new QLabel(QStringLiteral("号码可以留空，留空时使用推荐号码。"), openCard);
    hint->setObjectName(QStringLiteral("serviceOpenHintFanLX"));
    hint->setWordWrap(true);
    subscribeButtonFanLX = kit::primaryButton(QStringLiteral("开通所选服务"), openCard);
    subscribeButtonFanLX->setMinimumHeight(36);
    openLayout->addWidget(openTitle);
    openLayout->addWidget(typeCaption);
    openLayout->addWidget(subscribeComboFanLX);
    openLayout->addWidget(numberCaption);
    openLayout->addWidget(subscribeNumberEditFanLX);
    openLayout->addWidget(hint);
    openLayout->addWidget(subscribeButtonFanLX);
    auto *dialogLayout = new QVBoxLayout(dialog);
    dialogLayout->setContentsMargins(16, 16, 16, 16);
    dialogLayout->addWidget(openCard);
    connect(openEntry, &QPushButton::clicked, dialog, &QDialog::exec);

    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->addWidget(listCard);

    // 号码留空时给出建议值：QQ / 微博 沿用微X号（课程约定两者共享号码），微信加 wx_ 前缀。
    const auto suggestedNumber = [this] {
        const auto weix = controllerFanLX->weixAccountId();
        if (subscribeComboFanLX->currentText() == QStringLiteral("微信"))
            return QStringLiteral("wx_") + weix;
        return weix;
    };
    connect(subscribeComboFanLX, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, suggestedNumber] { subscribeNumberEditFanLX->setPlaceholderText(suggestedNumber()); });
    subscribeNumberEditFanLX->setPlaceholderText(suggestedNumber());
    connect(subscribeButtonFanLX, &QPushButton::clicked, this, [this, suggestedNumber] {
        const auto typed = subscribeNumberEditFanLX->text().trimmed();
        controllerFanLX->activateService(subscribeComboFanLX->currentText(),
                                         typed.isEmpty() ? suggestedNumber() : typed);
    });
    return body;
}

QWidget *AccountPageFanLX::buildLoginBodyFanLX() {
    auto *card = manageCardFanLX(this);
    card->setStyleSheet(accountManageStyleFanLX());
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);
    layout->addWidget(manageTitleFanLX(QStringLiteral("登录管理"), card));
    confirmServiceComboFanLX = makePlatformComboFanLX(card);
    confirmServiceComboFanLX->setMinimumHeight(32);
    confirmLoginButtonFanLX = kit::primaryButton(QStringLiteral("逐项确认登录"), card);
    linkAllButtonFanLX = kit::primaryButton(QStringLiteral("一键联动登录"), card);
    confirmLoginButtonFanLX->setMinimumHeight(32);
    linkAllButtonFanLX->setMinimumHeight(32);
    auto *confirmRow = new QHBoxLayout();
    confirmRow->setSpacing(8);
    confirmRow->addWidget(confirmServiceComboFanLX, 1);
    confirmRow->addWidget(confirmLoginButtonFanLX);
    confirmRow->addWidget(linkAllButtonFanLX);
    layout->addLayout(confirmRow);
    logoutButtonFanLX = kit::dangerButton(QStringLiteral("注销当前服务"), card);
    logoutAllButtonFanLX = kit::dangerButton(QStringLiteral("全部注销"), card);
    logoutButtonFanLX->setMinimumHeight(32);
    logoutAllButtonFanLX->setMinimumHeight(32);
    auto *logoutRow = new QHBoxLayout();
    logoutRow->setSpacing(8);
    logoutRow->addWidget(logoutButtonFanLX);
    logoutRow->addWidget(logoutAllButtonFanLX);
    logoutRow->addStretch(1);
    layout->addLayout(logoutRow);

    connect(linkAllButtonFanLX, &QPushButton::clicked, this,
            [this] { controllerFanLX->confirmAllServices(); });
    connect(confirmLoginButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto service = confirmServiceComboFanLX->currentText();
        controllerFanLX->confirmLoginService(service, controllerFanLX->accountIdOf(service));
    });
    connect(logoutButtonFanLX, &QPushButton::clicked, this,
            [this] { controllerFanLX->logoutCurrent(false); });
    connect(logoutAllButtonFanLX, &QPushButton::clicked, this,
            [this] { controllerFanLX->logoutCurrent(true); });
    return card;
}

QWidget *AccountPageFanLX::buildBindingBodyFanLX() {
    auto *card = manageCardFanLX(this);
    card->setStyleSheet(accountManageStyleFanLX());
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);
    layout->addWidget(manageTitleFanLX(QStringLiteral("绑定管理"), card));
    bindServiceComboFanLX = makePlatformComboFanLX(card);
    bindServiceComboFanLX->setMinimumHeight(36);
    layout->addWidget(bindServiceComboFanLX);
    bindAccountEditFanLX = new QLineEdit(card);
    bindAccountEditFanLX->setPlaceholderText(QStringLiteral("对方号码，留空则使用本人号码"));
    bindAccountEditFanLX->setMinimumHeight(36);
    layout->addWidget(bindAccountEditFanLX);
    bindButtonFanLX = kit::primaryButton(QStringLiteral("绑定"), card);
    unbindButtonFanLX = kit::dangerButton(QStringLiteral("解绑"), card);
    bindButtonFanLX->setMinimumHeight(36);
    unbindButtonFanLX->setMinimumHeight(36);
    auto *actions = new QHBoxLayout();
    actions->setSpacing(8);
    actions->addWidget(bindButtonFanLX, 1);
    actions->addWidget(unbindButtonFanLX, 1);
    layout->addLayout(actions);
    bindingListFanLX = new QListWidget(card);
    bindingListFanLX->setObjectName(QStringLiteral("bindingListFanLX"));
    bindingListFanLX->setFrameShape(QFrame::NoFrame);
    bindingListFanLX->setSpacing(0);
    bindingListFanLX->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bindingListFanLX->setSelectionMode(QAbstractItemView::NoSelection);
    bindingListFanLX->setFocusPolicy(Qt::NoFocus);
    bindingListFanLX->setMaximumHeight(96);
    layout->addWidget(bindingListFanLX);
    layout->addStretch(1);
    refillBindingListFanLX(bindingListFanLX, {});

    const auto targetAccount = [this] {
        const auto service = bindServiceComboFanLX->currentText();
        return bindAccountEditFanLX->text().trimmed().isEmpty()
                   ? controllerFanLX->accountIdOf(service)
                   : bindAccountEditFanLX->text().trimmed();
    };
    connect(bindButtonFanLX, &QPushButton::clicked, this, [this, targetAccount] {
        controllerFanLX->bindService(bindServiceComboFanLX->currentText(), targetAccount(), false);
    });
    connect(unbindButtonFanLX, &QPushButton::clicked, this, [this, targetAccount] {
        controllerFanLX->bindService(bindServiceComboFanLX->currentText(), targetAccount(), true);
    });
    return card;
}

QWidget *AccountPageFanLX::buildMaintenanceBodyFanLX() {
    auto *card = manageCardFanLX(this);
    card->setStyleSheet(accountManageStyleFanLX());
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);
    layout->addWidget(manageTitleFanLX(QStringLiteral("资料与密码"), card));
    nicknameEditFanLX = new QLineEdit(card);
    nicknameEditFanLX->setPlaceholderText(QStringLiteral("新昵称"));
    nicknameEditFanLX->setMinimumHeight(36);
    saveNicknameButtonFanLX = kit::primaryButton(QStringLiteral("保存昵称"), card);
    saveNicknameButtonFanLX->setMinimumHeight(36);
    locationEditFanLX = new QLineEdit(card);
    locationEditFanLX->setPlaceholderText(QStringLiteral("新所在地"));
    locationEditFanLX->setMinimumHeight(36);
    saveLocationButtonFanLX = kit::primaryButton(QStringLiteral("保存所在地"), card);
    saveLocationButtonFanLX->setMinimumHeight(36);
    oldPasswordEditFanLX = new QLineEdit(card);
    oldPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    oldPasswordEditFanLX->setPlaceholderText(QStringLiteral("当前密码"));
    oldPasswordEditFanLX->setMinimumHeight(36);
    newPasswordEditFanLX = new QLineEdit(card);
    newPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    newPasswordEditFanLX->setPlaceholderText(QStringLiteral("新密码"));
    newPasswordEditFanLX->setMinimumHeight(36);
    repeatPasswordEditFanLX = new QLineEdit(card);
    repeatPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    repeatPasswordEditFanLX->setPlaceholderText(QStringLiteral("再次输入新密码"));
    repeatPasswordEditFanLX->setMinimumHeight(36);
    changePasswordButtonFanLX = kit::primaryButton(QStringLiteral("修改密码"), card);
    changePasswordButtonFanLX->setMinimumHeight(36);
    auto *fields = new QHBoxLayout();
    fields->setSpacing(8);
    fields->addWidget(nicknameEditFanLX, 1);
    fields->addWidget(saveNicknameButtonFanLX);
    fields->addSpacing(8);
    fields->addWidget(locationEditFanLX, 1);
    fields->addWidget(saveLocationButtonFanLX);
    fields->addSpacing(8);
    fields->addWidget(oldPasswordEditFanLX, 1);
    fields->addWidget(newPasswordEditFanLX, 1);
    fields->addWidget(repeatPasswordEditFanLX, 1);
    fields->addWidget(changePasswordButtonFanLX);
    layout->addLayout(fields);
    hintLabelFanLX = kit::hintLabel(card);
    hintLabelFanLX->setObjectName(QStringLiteral("accountManageResultFanLX"));
    layout->addWidget(hintLabelFanLX);

    connect(saveNicknameButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto nickname = nicknameEditFanLX->text().trimmed();
        if (nickname.isEmpty()) {
            setAccountHintFanLX(hintLabelFanLX, QStringLiteral("请先输入新昵称"), QStringLiteral("bad"));
            return;
        }
        // 只提交昵称：所在地传空串 = "这一项不改"。
        if (controllerFanLX->updateProfile(nickname, QString())) {
            nicknameEditFanLX->clear();
            setAccountHintFanLX(hintLabelFanLX, QString(), QString());
        }
    });
    connect(saveLocationButtonFanLX, &QPushButton::clicked, this, [this] {
        const auto location = locationEditFanLX->text().trimmed();
        if (location.isEmpty()) {
            setAccountHintFanLX(hintLabelFanLX, QStringLiteral("请先输入新所在地"), QStringLiteral("bad"));
            return;
        }
        if (controllerFanLX->updateProfile(QString(), location)) {
            locationEditFanLX->clear();
            setAccountHintFanLX(hintLabelFanLX, QString(), QString());
        }
    });
    connect(changePasswordButtonFanLX, &QPushButton::clicked, this, [this] {
        // 两次输入必须一致：否则用户会用一个自己以为不对的密码锁死账号。
        if (newPasswordEditFanLX->text() != repeatPasswordEditFanLX->text()) {
            setAccountHintFanLX(hintLabelFanLX, QStringLiteral("两次输入的新密码不一致，未提交"),
                                QStringLiteral("bad"));
            return;
        }
        if (controllerFanLX->changePassword(oldPasswordEditFanLX->text(), newPasswordEditFanLX->text())) {
            oldPasswordEditFanLX->clear();
            newPasswordEditFanLX->clear();
            repeatPasswordEditFanLX->clear();
            setAccountHintFanLX(hintLabelFanLX, QString(), QString());
        }
    });
    return card;
}

void AccountPageFanLX::refreshFanLX() {
    if (!controllerFanLX->opened())
        return;

    kit::applyAccent(this, controllerFanLX->currentService());

    const bool anyLogin = controllerFanLX->hasAnyLogin();
    const bool currentLoggedIn = controllerFanLX->currentServiceLoggedIn();
    const auto serviceName = controllerFanLX->currentService();
    const auto accountName = controllerFanLX->currentAccountId();
    const auto headerSubtitle = currentLoggedIn ? QStringLiteral("%1 · %2").arg(serviceName, accountName)
                                                : QStringLiteral("%1 未登录").arg(serviceName);
    for (auto *label : findChildren<QLabel *>(QStringLiteral("accountHeaderSubtitleFanLX")))
        label->setText(headerSubtitle);

    // 当前身份
    identityLabelFanLX->setText(QStringLiteral("%1 · %2").arg(serviceName, accountName));
    identityLabelFanLX->setToolTip(QStringLiteral("微X号 %1 ｜ 已提交领域事务 %2 次")
                                       .arg(controllerFanLX->weixAccountId())
                                       .arg(controllerFanLX->commitCount()));
    // 切换下拉框：未登录的服务标注"（未登录）"并置灰，选它会被拒绝并给出原因。
    const auto current = controllerFanLX->currentService();
    const auto identities = identityNamesFanLX(controllerFanLX);
    {
        QSignalBlocker blocker(platformComboFanLX);
        platformComboFanLX->clear();
        for (const auto &name : identities) {
            const bool loggedIn = controllerFanLX->isServiceLoggedIn(name);
            platformComboFanLX->addItem(loggedIn ? name : name + QStringLiteral("（未登录）"), name);
            if (!loggedIn)
                platformComboFanLX->setItemData(platformComboFanLX->count() - 1, QColor(theme::textMuted),
                                                Qt::ForegroundRole);
        }
        const int currentIndex = platformComboFanLX->findData(current);
        if (currentIndex >= 0)
            platformComboFanLX->setCurrentIndex(currentIndex);
    }

    bannerFanLX->setVisible(!currentLoggedIn);
    reloginButtonFanLX->setVisible(!anyLogin);
    bannerLabelFanLX->setText(
        !anyLogin ? QStringLiteral("已全部注销：当前没有任何登录中的服务。点右侧「重新登录微X」恢复。")
                  : QStringLiteral("%1 未登录：到「登录管理」里确认登录，或在右上角切换到已登录的服务。")
                        .arg(serviceName));

    // 一键联动登录：只在"本人已开通但未登录"的服务非空时可用（否则点了也没有任何变化）。
    linkAllButtonFanLX->setEnabled(controllerFanLX->hasUnlinkedServices());

    // 用户基本信息卡片
    const auto profile = controllerFanLX->profileView();
    const auto orDash = [](const QString &text) { return text.trimmed().isEmpty() ? QStringLiteral("—") : text; };
    const auto paintAvatar = [this](const QString &seed, const QString &glyph) {
        avatarLabelFanLX->setText(glyph);
        // 控件自己的样式表盖过页面样式，头像底色才跟聊天、好友列表是同一套。
        avatarLabelFanLX->setStyleSheet(
            QStringLiteral("background:%1;color:#ffffff;border-radius:18px;font-size:16px;font-weight:600;")
                .arg(seed.isEmpty() ? theme::textMuted : theme::avatarColor(seed)));
    };
    if (!profile.valid) {
        nameLabelFanLX->setText(QStringLiteral("未登录"));
        subtitleLabelFanLX->clear();
        paintAvatar(QString(), QString());
    } else {
        const auto initial = (profile.nickname.isEmpty() ? profile.weixId : profile.nickname).left(1);
        paintAvatar(profile.weixId.isEmpty() ? profile.accountId : profile.weixId, initial);
        nameLabelFanLX->setText(orDash(profile.nickname));
        subtitleLabelFanLX->setText(QStringLiteral("微X号 %1 · 当前 %2 %3")
                                        .arg(profile.weixId, profile.service,
                                             profile.accountId.isEmpty() ? QStringLiteral("（未开通）")
                                                                         : profile.accountId));
        tAgeValueFanLX->setText(QString::number(profile.tAgeYears));
        friendValueFanLX->setText(QString::number(profile.friendCount));
        groupValueFanLX->setText(QString::number(profile.groupCount));
        birthValueFanLX->setText(orDash(profile.birthDate));
        applyValueFanLX->setText(orDash(profile.applyDate));
        locationValueFanLX->setText(orDash(profile.location));
        userIdValueFanLX->setText(orDash(profile.userId));
    }

    // 开通管理：四个身份全部列出。这里只改单元格文案和行高，判断仍用 serviceRows() 的原字段。
    const auto rows = controllerFanLX->serviceRows();
    serviceTableFanLX->setRowCount(rows.size());
    for (int index = 0; index < rows.size(); ++index) {
        const auto &row = rows.at(index);
        const QStringList cells{
            row.service + (row.current ? QStringLiteral("（当前）") : QString()) +
                (row.upperLayer ? QStringLiteral("（上层）") : QString()),
            row.exists ? row.accountId : QStringLiteral("（未开通）"),
            row.sharedId ? QStringLiteral("QQ 与微博共享号") : QStringLiteral("独立号"),
            row.bound ? QStringLiteral("已绑定") : QStringLiteral("未绑定"),
            row.subscribed ? QStringLiteral("已开通") : QStringLiteral("未开通"),
            row.loggedIn ? QStringLiteral("已登录") : QStringLiteral("未登录")};
        const QColor currentBackground(QStringLiteral("#E8EFFD"));
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setData(Qt::UserRole, row.current);
            if (column >= 3 && cells.at(column).startsWith(QStringLiteral("已")))
                item->setForeground(QColor(theme::success));
            else if (column >= 3 && cells.at(column).startsWith(QStringLiteral("未")))
                item->setForeground(QColor(theme::textMuted));
            if (row.current)
                item->setBackground(currentBackground);
            serviceTableFanLX->setItem(index, column, item);
        }
    }
    fitServiceTableFanLX(serviceTableFanLX);

    // 绑定管理：条目仍来自 bindingRows()，只把同一条关系拆成左右两个账号来显示。
    refillBindingListFanLX(bindingListFanLX, controllerFanLX->bindingRows());
}

} // namespace fanlxGui

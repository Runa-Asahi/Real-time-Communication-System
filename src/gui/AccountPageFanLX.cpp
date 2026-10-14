#include "gui/AccountPageFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "gui/PageKitFanLX.h"
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
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
        "QLabel#accountPageTitleFanLX{font-size:20px;font-weight:600;color:#1F2329;background:transparent;}"
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
        "QLabel#accountEntryDetailFanLX{font-size:12px;color:#5B6472;background:transparent;}"
        "QLabel#accountEntryArrowFanLX{font-size:18px;color:#8A94A3;background:transparent;}");
}

// 右侧四个入口：真正的按钮，标题、说明和箭头只是按钮里的标签。
QPushButton *accountEntryButtonFanLX(const QString &title, const QString &detail, QWidget *parent) {
    auto *button = new QPushButton(parent);
    button->setProperty("role", QStringLiteral("accountEntry"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setMinimumHeight(76);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *row = new QHBoxLayout(button);
    row->setContentsMargins(16, 14, 14, 14);
    row->setSpacing(12);
    auto *textColumn = new QVBoxLayout();
    textColumn->setSpacing(4);
    auto *titleLabel = new QLabel(title, button);
    titleLabel->setObjectName(QStringLiteral("accountEntryTitleFanLX"));
    auto *detailLabel = new QLabel(detail, button);
    detailLabel->setObjectName(QStringLiteral("accountEntryDetailFanLX"));
    detailLabel->setWordWrap(true);
    textColumn->addWidget(titleLabel);
    textColumn->addWidget(detailLabel);
    auto *arrow = new QLabel(QStringLiteral("›"), button);
    arrow->setObjectName(QStringLiteral("accountEntryArrowFanLX"));
    arrow->setAlignment(Qt::AlignCenter);
    for (auto *label : {titleLabel, detailLabel, arrow}) {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        label->setFocusPolicy(Qt::NoFocus);
    }
    row->addLayout(textColumn, 1);
    row->addWidget(arrow);
    return button;
}

// 资料卡与入口的两列容器。窄于 780px 时改成上下排列，避免两侧被挤扁。
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
        profile_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        entries_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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
        if (stacked) {
            grid_->addWidget(profile_, 0, 0, Qt::AlignTop);
            grid_->addWidget(entries_, 1, 0, Qt::AlignTop);
            grid_->setColumnStretch(0, 1);
            grid_->setColumnStretch(1, 0);
        } else {
            grid_->addWidget(profile_, 0, 0, Qt::AlignTop);
            grid_->addWidget(entries_, 0, 1, Qt::AlignTop);
            grid_->setColumnStretch(0, 58);
            grid_->setColumnStretch(1, 42);
        }
    }

    QGridLayout *grid_ = nullptr;
    QWidget *profile_ = nullptr;
    QWidget *entries_ = nullptr;
    bool stacked_ = false;
};

// 首页根：内容最宽 1040，宽屏居中；宽度变化时通知两列改为上下排列。
class AccountHomeFanLX : public QWidget {
  public:
    explicit AccountHomeFanLX(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("accountHomeFanLX"));
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(accountHomeStyleFanLX());
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

    QWidget *columnFanLX = nullptr;
    AccountColumnsFanLX *columnsFanLX = nullptr;

  protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (!columnFanLX)
            return;
        const auto margins = layout() ? layout()->contentsMargins() : QMargins();
        const int available = qMax(280, width() - margins.left() - margins.right());
        const int target = qMin(1040, available);
        if (columnFanLX->minimumWidth() != target || columnFanLX->maximumWidth() != target)
            columnFanLX->setFixedWidth(target);
        if (columnsFanLX)
            columnsFanLX->setStacked(target < 780);
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
        list_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        open_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
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
            grid_->addWidget(list_, 0, 0, Qt::AlignTop);
            grid_->addWidget(open_, 0, 1, Qt::AlignTop);
            grid_->setColumnStretch(0, 65);
            grid_->setColumnStretch(1, 35);
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
        "QListWidget#bindingListFanLX{background:transparent;border:none;outline:none;}"
        "QListWidget#bindingListFanLX::item{background:transparent;border:none;padding:0px;}"
        "QListWidget#bindingListFanLX::item:selected{background:transparent;color:#1F2329;}"
        "QWidget#bindingRowFanLX{background:transparent;border:none;border-bottom:1px solid #E6E8EE;"
        "border-radius:0px;}"
        "QLabel#bindingServiceFanLX{font-size:12px;color:#8A94A3;background:transparent;}"
        "QLabel#bindingAccountFanLX{font-size:14px;font-weight:600;color:#1F2329;background:transparent;}"
        "QLabel#bindingMarkFanLX{font-size:12px;font-weight:600;color:#2563EB;background:#E8EFFD;"
        "border-radius:10px;padding:2px 8px;}"
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

// 藏起 PageKit 自带的「‹ 返回」和 16px 标题，换成本页的标题栏。原控件仍留在树上。
void installAccountHeaderFanLX(QStackedWidget *stack, int index, const QString &title) {
    auto *page = stack ? stack->widget(index) : nullptr;
    if (!page)
        return;
    for (auto *button : page->findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (button->property("role").toString() == QLatin1String("back"))
            button->hide();
    }
    for (auto *label : page->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (label->property("role").toString() == QLatin1String("title"))
            label->hide();
    }
    auto *header = new AccountSubHeaderFanLX(title, page);
    QObject::connect(header->backButton(), &QPushButton::clicked, stack, [stack] { stack->setCurrentIndex(0); });
    if (auto *layout = qobject_cast<QVBoxLayout *>(page->layout()))
        layout->insertWidget(0, header);
}

constexpr int bindingRowHeightFanLX = 72;
constexpr int bindingListCapRowsFanLX = 8;

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

QLabel *manageHintFanLX(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("accountManageHintFanLX"));
    label->setWordWrap(true);
    return label;
}

QLabel *manageFieldFanLX(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("accountManageFieldFanLX"));
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
        column_->setMaximumWidth(1240);
        column_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        columnLayout_ = new QVBoxLayout(column_);
        columnLayout_->setContentsMargins(0, 0, 0, 0);
        columnLayout_->setSpacing(16);
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

QWidget *bindingSideFanLX(const QString &caption, const QString &value, QWidget *parent) {
    auto *box = new QWidget(parent);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto *cap = new QLabel(caption, box);
    cap->setObjectName(QStringLiteral("bindingServiceFanLX"));
    auto *text = new QLabel(value, box);
    text->setObjectName(QStringLiteral("bindingAccountFanLX"));
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(cap);
    layout->addWidget(text);
    return box;
}

QWidget *bindingPairRowFanLX(const QString &text, QWidget *parent) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("bindingRowFanLX"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);
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
    layout->addWidget(bindingSideFanLX(QStringLiteral("服务账号"),
                                       QStringLiteral("%1（%2）").arg(match.captured(1), match.captured(2)), row),
                      1);
    auto *mark = new QLabel(QStringLiteral("已绑定"), row);
    mark->setObjectName(QStringLiteral("bindingMarkFanLX"));
    mark->setAlignment(Qt::AlignCenter);
    mark->setAttribute(Qt::WA_StyledBackground, true);
    layout->addWidget(mark, 0, Qt::AlignVCenter);
    layout->addWidget(bindingSideFanLX(QStringLiteral("关联账号"),
                                       QStringLiteral("%1（%2）").arg(match.captured(3), match.captured(4)), row),
                      1);
    return row;
}

QWidget *bindingEmptyRowFanLX(QWidget *parent) {
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("bindingRowFanLX"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(12, 8, 12, 8);
    auto *label = new QLabel(QStringLiteral("还没有绑定关系。建立后会显示在这里。"), row);
    label->setObjectName(QStringLiteral("bindingEmptyFanLX"));
    label->setWordWrap(true);
    layout->addWidget(label);
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

    stackFanLX = new QStackedWidget(this);
    // 先放首页（下标 0），再依次追加四个子页面；首页入口按钮在 buildHomeFanLX 里按下标跳转。
    kit::setHomePage(stackFanLX, buildHomeFanLX());
    if (auto *scroll = qobject_cast<QScrollArea *>(stackFanLX->widget(0))) {
        scroll->viewport()->setAutoFillBackground(true);
        auto viewportPalette = scroll->viewport()->palette();
        viewportPalette.setColor(QPalette::Window, QColor(QStringLiteral("#F6F7F9")));
        scroll->viewport()->setPalette(viewportPalette);
    }
    installAccountHeaderFanLX(stackFanLX,
                              kit::addSubPage(stackFanLX, QStringLiteral("开通管理"), buildServiceBodyFanLX()),
                              QStringLiteral("开通管理"));
    installAccountHeaderFanLX(stackFanLX,
                              kit::addSubPage(stackFanLX, QStringLiteral("登录管理"), buildLoginBodyFanLX()),
                              QStringLiteral("登录管理"));
    installAccountHeaderFanLX(stackFanLX,
                              kit::addSubPage(stackFanLX, QStringLiteral("绑定管理"), buildBindingBodyFanLX()),
                              QStringLiteral("绑定管理"));
    installAccountHeaderFanLX(stackFanLX,
                              kit::addSubPage(stackFanLX, QStringLiteral("资料与密码"), buildMaintenanceBodyFanLX()),
                              QStringLiteral("资料与密码"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->addWidget(stackFanLX);

    // 身份变化与任何写操作成功后都重读视图：界面显示的一定是内存里的最新状态。
    connect(controllerFanLX, &DomainControllerFanLX::dataChangedFanLX, this,
            &AccountPageFanLX::refreshFanLX);
    connect(controllerFanLX, &DomainControllerFanLX::identityChangedFanLX, this,
            &AccountPageFanLX::refreshFanLX);
    refreshFanLX();
}

QWidget *AccountPageFanLX::buildHomeFanLX() {
    auto *home = new AccountHomeFanLX();
    auto *outer = new QVBoxLayout(home);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(0);

    auto *column = new QWidget(home);
    column->setFixedWidth(1040);
    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("账号与服务"), column);
    title->setObjectName(QStringLiteral("accountPageTitleFanLX"));
    columnLayout->addWidget(title);

    // 当前身份栏：服务名、账号与切换下拉框放在同一条里。选中即切换。
    // 切换被拒（比如该服务还没登录）时，下拉框必须回到真正的当前服务，并弹出原因。
    auto *identity = new QFrame(column);
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
    bannerFanLX = new QFrame(column);
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

    auto *columns = new AccountColumnsFanLX(column);
    auto *profile = buildProfileCardFanLX(columns);
    auto *entries = new QFrame(columns);
    entries->setObjectName(QStringLiteral("accountEntriesFanLX"));
    entries->setFrameShape(QFrame::NoFrame);
    entries->setAttribute(Qt::WA_StyledBackground, true);
    auto *entryLayout = new QVBoxLayout(entries);
    entryLayout->setContentsMargins(0, 0, 0, 0);
    entryLayout->setSpacing(0);
    const QStringList titles{QStringLiteral("开通管理"), QStringLiteral("登录管理"),
                             QStringLiteral("绑定管理"), QStringLiteral("资料与密码")};
    const QStringList details{
        QStringLiteral("开通 QQ、微信或微博，并查看号码与开通状态。"),
        QStringLiteral("确认登录、联动已开通的服务，或注销登录。"),
        QStringLiteral("查看、建立或解除各服务号码的绑定。"),
        QStringLiteral("分别修改昵称、所在地，或更换登录密码。")};
    for (int index = 0; index < titles.size(); ++index) {
        auto *button = accountEntryButtonFanLX(titles.at(index), details.at(index), entries);
        button->setProperty("lined", index + 1 < titles.size() ? QStringLiteral("true") : QStringLiteral("false"));
        button->setProperty("edge", index == 0 ? QStringLiteral("first")
                                                : index + 1 == titles.size() ? QStringLiteral("last")
                                                                              : QStringLiteral("mid"));
        entryLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, index] { stackFanLX->setCurrentIndex(index + 1); });
    }
    columns->setPanels(profile, entries);
    columnLayout->addWidget(columns);

    auto *center = new QHBoxLayout();
    center->setContentsMargins(0, 0, 0, 0);
    center->addStretch(1);
    center->addWidget(column, 0, Qt::AlignTop);
    center->addStretch(1);
    outer->addLayout(center);
    outer->addStretch(1);

    home->columnFanLX = column;
    home->columnsFanLX = columns;
    return home;
}

QWidget *AccountPageFanLX::buildProfileCardFanLX(QWidget *parent) {
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("accountProfileFanLX"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(16);

    auto *heading = new QLabel(QStringLiteral("用户基本信息"), frame);
    heading->setObjectName(QStringLiteral("accountProfileHeadingFanLX"));
    layout->addWidget(heading);

    const auto labelOf = [frame](const QString &role, const QString &text = QString()) {
        auto *label = new QLabel(text, frame);
        label->setProperty("role", role);
        return label;
    };

    // 头像（昵称首字）+ 昵称 + 一行账号副标题。
    avatarLabelFanLX = labelOf(QStringLiteral("avatar"));
    avatarLabelFanLX->setFixedSize(48, 48);
    avatarLabelFanLX->setAlignment(Qt::AlignCenter);
    avatarLabelFanLX->setAttribute(Qt::WA_StyledBackground, true);
    nameLabelFanLX = labelOf(QStringLiteral("name"));
    nameLabelFanLX->setWordWrap(true);
    subtitleLabelFanLX = labelOf(QStringLiteral("caption"));
    subtitleLabelFanLX->setWordWrap(true);
    auto *titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(2);
    titleColumn->addStretch(1);
    titleColumn->addWidget(nameLabelFanLX);
    titleColumn->addWidget(subtitleLabelFanLX);
    titleColumn->addStretch(1);
    auto *header = new QHBoxLayout();
    header->setSpacing(14);
    header->addWidget(avatarLabelFanLX);
    header->addLayout(titleColumn, 1);
    layout->addLayout(header);

    // 三项统计直接排在资料卡里，不再套一层浅底小卡片。
    const auto statTile = [&](QLabel *&valueLabel, const QString &caption) {
        auto *tile = new QWidget(frame);
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(0, 0, 0, 0);
        tileLayout->setSpacing(2);
        valueLabel = new QLabel(tile);
        valueLabel->setObjectName(QStringLiteral("accountStatValueFanLX"));
        valueLabel->setAlignment(Qt::AlignCenter);
        auto *captionLabel = new QLabel(caption, tile);
        captionLabel->setObjectName(QStringLiteral("accountStatCaptionFanLX"));
        captionLabel->setAlignment(Qt::AlignCenter);
        tileLayout->addWidget(valueLabel);
        tileLayout->addWidget(captionLabel);
        return tile;
    };
    auto *stats = new QHBoxLayout();
    stats->setSpacing(12);
    stats->addWidget(statTile(tAgeValueFanLX, QStringLiteral("T龄（年）")), 1);
    stats->addWidget(statTile(friendValueFanLX, QStringLiteral("好友")), 1);
    stats->addWidget(statTile(groupValueFanLX, QStringLiteral("群")), 1);
    layout->addLayout(stats);
    auto *hairline = new QFrame(frame);
    hairline->setObjectName(QStringLiteral("accountHairlineFanLX"));
    hairline->setFrameShape(QFrame::NoFrame);
    hairline->setFixedHeight(1);
    layout->addWidget(hairline);

    // 详情两列：标签是次要色，值是正文色。出生与申请都是日期，不用「时间」。
    const auto detail = [&](QLabel *&valueLabel, const QString &caption) {
        valueLabel = labelOf(QStringLiteral("value"));
        valueLabel->setWordWrap(true);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *column = new QVBoxLayout();
        column->setSpacing(2);
        column->addWidget(labelOf(QStringLiteral("caption"), caption));
        column->addWidget(valueLabel);
        return column;
    };
    auto *details = new QGridLayout();
    details->setHorizontalSpacing(24);
    details->setVerticalSpacing(12);
    details->addLayout(detail(birthValueFanLX, QStringLiteral("出生日期")), 0, 0);
    details->addLayout(detail(applyValueFanLX, QStringLiteral("号码申请日期")), 0, 1);
    details->addLayout(detail(locationValueFanLX, QStringLiteral("所在地")), 1, 0);
    details->addLayout(detail(userIdValueFanLX, QStringLiteral("后台用户 ID")), 1, 1);
    details->setColumnStretch(0, 1);
    details->setColumnStretch(1, 1);
    layout->addLayout(details);
    return frame;
}

QWidget *AccountPageFanLX::buildServiceBodyFanLX() {
    auto *body = new AccountServiceBodyFanLX();

    auto *listCard = new QFrame(body);
    listCard->setObjectName(QStringLiteral("serviceListCardFanLX"));
    listCard->setFrameShape(QFrame::NoFrame);
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(16, 16, 16, 12);
    listLayout->setSpacing(12);
    auto *listTitle = new QLabel(QStringLiteral("服务账号"), listCard);
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
    listLayout->addWidget(listTitle);
    listLayout->addWidget(serviceTableFanLX);

    auto *openCard = new QFrame(body);
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
    subscribeComboFanLX->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *numberCaption = new QLabel(QStringLiteral("服务号码"), openCard);
    numberCaption->setObjectName(QStringLiteral("serviceFieldCaptionFanLX"));
    subscribeNumberEditFanLX = new QLineEdit(openCard);
    subscribeNumberEditFanLX->setMinimumHeight(36);
    subscribeNumberEditFanLX->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *hint = new QLabel(QStringLiteral("号码可以留空，留空时使用推荐号码。"), openCard);
    hint->setObjectName(QStringLiteral("serviceOpenHintFanLX"));
    hint->setWordWrap(true);
    subscribeButtonFanLX = kit::primaryButton(QStringLiteral("开通所选服务"), openCard);
    subscribeButtonFanLX->setMinimumHeight(36);
    subscribeButtonFanLX->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    openLayout->addWidget(openTitle);
    openLayout->addSpacing(4);
    openLayout->addWidget(typeCaption);
    openLayout->addWidget(subscribeComboFanLX);
    openLayout->addWidget(numberCaption);
    openLayout->addWidget(subscribeNumberEditFanLX);
    openLayout->addWidget(hint);
    openLayout->addSpacing(4);
    openLayout->addWidget(subscribeButtonFanLX);

    static_cast<AccountServiceBodyFanLX *>(body)->setCards(listCard, openCard);

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
    auto *page = new AccountManagePageFanLX();
    auto *host = page->column();

    auto *confirmCard = manageCardFanLX(host);
    auto *confirmLayout = new QVBoxLayout(confirmCard);
    confirmLayout->setContentsMargins(20, 18, 20, 18);
    confirmLayout->setSpacing(8);
    confirmLayout->addWidget(manageTitleFanLX(QStringLiteral("登录服务"), confirmCard));
    confirmLayout->addWidget(manageHintFanLX(QStringLiteral("选择服务后，确认该服务的登录身份。"), confirmCard));
    confirmLayout->addSpacing(4);
    confirmLayout->addWidget(manageFieldFanLX(QStringLiteral("服务"), confirmCard));
    confirmServiceComboFanLX = makePlatformComboFanLX(confirmCard);
    confirmServiceComboFanLX->setMinimumHeight(36);
    confirmLayout->addWidget(confirmServiceComboFanLX);
    confirmLayout->addSpacing(4);
    confirmLoginButtonFanLX = kit::primaryButton(QStringLiteral("逐项确认登录"), confirmCard);
    confirmLoginButtonFanLX->setMinimumHeight(36);
    confirmLayout->addWidget(confirmLoginButtonFanLX);

    auto *linkCard = manageCardFanLX(host);
    auto *linkLayout = new QVBoxLayout(linkCard);
    linkLayout->setContentsMargins(20, 18, 20, 18);
    linkLayout->setSpacing(8);
    linkLayout->addWidget(manageTitleFanLX(QStringLiteral("一键联动登录"), linkCard));
    linkLayout->addWidget(manageHintFanLX(
        QStringLiteral("一次确认本人已经开通的全部服务。没有待确认的服务时，按钮会停用。"), linkCard));
    linkLayout->addSpacing(8);
    // 一键联动：登微X 时已默认联动确认，所以只在"注销过 / 新开通了服务"后有事可做，
    // refreshFanLX 会据此启用或置灰它。
    linkAllButtonFanLX = kit::primaryButton(QStringLiteral("一键联动登录"), linkCard);
    linkAllButtonFanLX->setMinimumHeight(36);
    linkLayout->addWidget(linkAllButtonFanLX);

    auto *split = new AccountSplitFanLX(host);
    split->setPanels(confirmCard, linkCard, 1, 1, 880);
    page->columnLayout()->addWidget(split);

    auto *logoutCard = manageCardFanLX(host);
    auto *logoutLayout = new QVBoxLayout(logoutCard);
    logoutLayout->setContentsMargins(20, 18, 20, 18);
    logoutLayout->setSpacing(8);
    logoutLayout->addWidget(manageTitleFanLX(QStringLiteral("注销"), logoutCard));
    logoutLayout->addWidget(manageHintFanLX(
        QStringLiteral("注销当前服务只退出正在使用的身份。全部注销会退出所有已登录服务。"), logoutCard));
    logoutButtonFanLX = kit::dangerButton(QStringLiteral("注销当前服务"), logoutCard);
    logoutAllButtonFanLX = kit::dangerButton(QStringLiteral("全部注销"), logoutCard);
    logoutButtonFanLX->setMinimumHeight(36);
    logoutAllButtonFanLX->setMinimumHeight(36);
    auto *logoutRow = new QHBoxLayout();
    logoutRow->setSpacing(12);
    logoutRow->addWidget(logoutButtonFanLX);
    logoutRow->addWidget(logoutAllButtonFanLX);
    logoutRow->addStretch(1);
    logoutLayout->addSpacing(4);
    logoutLayout->addLayout(logoutRow);
    page->columnLayout()->addWidget(logoutCard);

    // 课设要求 5：登微X 后，本人已开通的其它服务简单确认即视为自动登录。
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
    return page;
}

QWidget *AccountPageFanLX::buildBindingBodyFanLX() {
    auto *page = new AccountManagePageFanLX();
    auto *host = page->column();

    auto *listCard = manageCardFanLX(host);
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(20, 18, 20, 16);
    listLayout->setSpacing(8);
    listLayout->addWidget(manageTitleFanLX(QStringLiteral("已绑定关系"), listCard));
    listLayout->addWidget(manageHintFanLX(QStringLiteral("本人各服务号码之间已经建立的对应关系。"), listCard));
    bindingListFanLX = new QListWidget(listCard);
    bindingListFanLX->setObjectName(QStringLiteral("bindingListFanLX"));
    bindingListFanLX->setFrameShape(QFrame::NoFrame);
    bindingListFanLX->setSpacing(0);
    bindingListFanLX->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bindingListFanLX->setSelectionMode(QAbstractItemView::NoSelection);
    bindingListFanLX->setFocusPolicy(Qt::NoFocus);
    listLayout->addWidget(bindingListFanLX);
    refillBindingListFanLX(bindingListFanLX, {});

    auto *formCard = manageCardFanLX(host);
    auto *formLayout = new QVBoxLayout(formCard);
    formLayout->setContentsMargins(20, 18, 20, 18);
    formLayout->setSpacing(8);
    formLayout->addWidget(manageTitleFanLX(QStringLiteral("管理绑定"), formCard));
    formLayout->addWidget(manageHintFanLX(
        QStringLiteral("填写要关联的对方号码。留空时使用本人在该服务下的号码。"), formCard));
    formLayout->addSpacing(4);
    formLayout->addWidget(manageFieldFanLX(QStringLiteral("服务"), formCard));
    bindServiceComboFanLX = makePlatformComboFanLX(formCard);
    bindServiceComboFanLX->setMinimumHeight(36);
    formLayout->addWidget(bindServiceComboFanLX);
    formLayout->addWidget(manageFieldFanLX(QStringLiteral("对方号码"), formCard));
    bindAccountEditFanLX = new QLineEdit(formCard);
    bindAccountEditFanLX->setPlaceholderText(QStringLiteral("留空则使用本人号码"));
    bindAccountEditFanLX->setMinimumHeight(36);
    formLayout->addWidget(bindAccountEditFanLX);
    formLayout->addSpacing(4);
    bindButtonFanLX = kit::primaryButton(QStringLiteral("绑定"), formCard);
    unbindButtonFanLX = kit::dangerButton(QStringLiteral("解绑"), formCard);
    bindButtonFanLX->setMinimumHeight(36);
    unbindButtonFanLX->setMinimumHeight(36);
    auto *actions = new QHBoxLayout();
    actions->setSpacing(12);
    actions->addWidget(bindButtonFanLX, 1);
    actions->addWidget(unbindButtonFanLX, 1);
    formLayout->addLayout(actions);

    auto *split = new AccountSplitFanLX(host);
    split->setPanels(listCard, formCard, 58, 42, 920);
    page->columnLayout()->addWidget(split);

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
    return page;
}

QWidget *AccountPageFanLX::buildMaintenanceBodyFanLX() {
    auto *page = new AccountManagePageFanLX();
    auto *host = page->column();

    // 单项修改：两个字段各有自己的保存按钮，改一个不会动另一个。
    auto *profileCard = manageCardFanLX(host);
    auto *profileLayout = new QVBoxLayout(profileCard);
    profileLayout->setContentsMargins(20, 18, 20, 18);
    profileLayout->setSpacing(8);
    profileLayout->addWidget(manageTitleFanLX(QStringLiteral("个人资料"), profileCard));
    profileLayout->addWidget(
        manageHintFanLX(QStringLiteral("昵称和所在地分开保存，修改一项不会改动另一项。"), profileCard));
    profileLayout->addSpacing(4);
    profileLayout->addWidget(manageFieldFanLX(QStringLiteral("昵称"), profileCard));
    nicknameEditFanLX = new QLineEdit(profileCard);
    nicknameEditFanLX->setPlaceholderText(QStringLiteral("输入新昵称"));
    nicknameEditFanLX->setMinimumHeight(36);
    saveNicknameButtonFanLX = kit::primaryButton(QStringLiteral("保存昵称"), profileCard);
    saveNicknameButtonFanLX->setMinimumHeight(36);
    profileLayout->addWidget(nicknameEditFanLX);
    profileLayout->addWidget(saveNicknameButtonFanLX, 0, Qt::AlignLeft);
    profileLayout->addWidget(manageFieldFanLX(QStringLiteral("所在地"), profileCard));
    locationEditFanLX = new QLineEdit(profileCard);
    locationEditFanLX->setPlaceholderText(QStringLiteral("输入新所在地"));
    locationEditFanLX->setMinimumHeight(36);
    saveLocationButtonFanLX = kit::primaryButton(QStringLiteral("保存所在地"), profileCard);
    saveLocationButtonFanLX->setMinimumHeight(36);
    profileLayout->addWidget(locationEditFanLX);
    profileLayout->addWidget(saveLocationButtonFanLX, 0, Qt::AlignLeft);

    auto *passwordCard = manageCardFanLX(host);
    auto *passwordLayout = new QVBoxLayout(passwordCard);
    passwordLayout->setContentsMargins(20, 18, 20, 18);
    passwordLayout->setSpacing(8);
    passwordLayout->addWidget(manageTitleFanLX(QStringLiteral("修改密码"), passwordCard));
    passwordLayout->addWidget(
        manageHintFanLX(QStringLiteral("输入当前密码，新密码需要再填写一次。"), passwordCard));
    passwordLayout->addSpacing(4);
    passwordLayout->addWidget(manageFieldFanLX(QStringLiteral("当前密码"), passwordCard));
    oldPasswordEditFanLX = new QLineEdit(passwordCard);
    oldPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    oldPasswordEditFanLX->setPlaceholderText(QStringLiteral("当前密码"));
    oldPasswordEditFanLX->setMinimumHeight(36);
    passwordLayout->addWidget(oldPasswordEditFanLX);
    passwordLayout->addWidget(manageFieldFanLX(QStringLiteral("新密码"), passwordCard));
    newPasswordEditFanLX = new QLineEdit(passwordCard);
    newPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    newPasswordEditFanLX->setPlaceholderText(QStringLiteral("新密码"));
    newPasswordEditFanLX->setMinimumHeight(36);
    passwordLayout->addWidget(newPasswordEditFanLX);
    passwordLayout->addWidget(manageFieldFanLX(QStringLiteral("确认新密码"), passwordCard));
    repeatPasswordEditFanLX = new QLineEdit(passwordCard);
    repeatPasswordEditFanLX->setEchoMode(QLineEdit::Password);
    repeatPasswordEditFanLX->setPlaceholderText(QStringLiteral("再次输入新密码"));
    repeatPasswordEditFanLX->setMinimumHeight(36);
    passwordLayout->addWidget(repeatPasswordEditFanLX);
    passwordLayout->addSpacing(4);
    changePasswordButtonFanLX = kit::primaryButton(QStringLiteral("修改密码"), passwordCard);
    changePasswordButtonFanLX->setMinimumHeight(36);
    passwordLayout->addWidget(changePasswordButtonFanLX, 0, Qt::AlignLeft);

    auto *split = new AccountSplitFanLX(host);
    split->setPanels(profileCard, passwordCard, 1, 1, 900);
    page->columnLayout()->addWidget(split);

    hintLabelFanLX = kit::hintLabel(host);
    hintLabelFanLX->setObjectName(QStringLiteral("accountManageResultFanLX"));
    page->columnLayout()->addWidget(hintLabelFanLX);

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
    return page;
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
            QStringLiteral("background:%1;color:#ffffff;border-radius:24px;font-size:20px;font-weight:600;")
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

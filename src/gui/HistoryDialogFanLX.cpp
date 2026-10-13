#include "gui/HistoryDialogFanLX.h"

#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>

namespace fanlxGui {
namespace {

constexpr int kResultRowCapFanLX = 120;

QTableWidgetItem *resultItemFanLX(const QString &text) {
    auto *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    item->setToolTip(text);
    return item;
}

QLabel *fieldLabelFanLX(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("historyField"));
    return label;
}

} // namespace

QString HistoryDialogFanLX::conversationLabelFanLX(const QString &key) {
    if (key.isEmpty())
        return QStringLiteral("所有会话（跨会话检索）");
    if (key.startsWith(QStringLiteral("g|"))) {
        const auto id = key.mid(2);
        if (!id.isEmpty() && !id.contains(QLatin1Char('|')))
            return QStringLiteral("群聊 · %1").arg(id);
    }
    if (key.startsWith(QStringLiteral("d|"))) {
        const auto parts = key.mid(2).split(QLatin1Char('|'));
        if (parts.size() == 2 && !parts.at(0).isEmpty() && !parts.at(1).isEmpty())
            return QStringLiteral("好友会话 · %1 ↔ %2").arg(parts.at(0), parts.at(1));
    }
    return QStringLiteral("未列出的会话");
}

void HistoryDialogFanLX::configureUtcEditorFanLX(QDateTimeEdit *edit) {
    edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    edit->setCalendarPopup(true);
    // Qt 6.7 起用 setTimeZone。更早的 6.2 接口只有 setTimeSpec，6.10 起后者被弃用。
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    edit->setTimeZone(QTimeZone::utc());
#else
    edit->setTimeSpec(Qt::UTC);
#endif
}

HistoryDialogFanLX::HistoryDialogFanLX(QWidget *parent) : QDialog(parent) {
    setObjectName(QStringLiteral("historyDialogFanLX"));
    setWindowTitle(QStringLiteral("聊天记录查询"));
    setMinimumSize(760, 560);
    resize(1000, 720);
    applyStyleFanLX();

    auto *title = new QLabel(QStringLiteral("聊天记录查询"), this);
    title->setObjectName(QStringLiteral("historyTitle"));
    auto *subtitle = new QLabel(QStringLiteral("按会话、发送者、关键词或服务器时间筛选历史消息。"), this);
    subtitle->setObjectName(QStringLiteral("historySubtitle"));
    subtitle->setWordWrap(true);

    conversationComboFanLX = new QComboBox(this);
    conversationComboFanLX->setObjectName(QStringLiteral("historyConversation"));
    conversationComboFanLX->setEditable(false);
    conversationComboFanLX->setInsertPolicy(QComboBox::NoInsert);
    conversationComboFanLX->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    conversationComboFanLX->setToolTip(QStringLiteral("选择一个会话，或选择所有会话做跨会话检索"));
    senderEditFanLX = new QLineEdit(this);
    senderEditFanLX->setObjectName(QStringLiteral("historySender"));
    senderEditFanLX->setPlaceholderText(QStringLiteral("可留空"));
    senderEditFanLX->setClearButtonEnabled(true);
    keywordEditFanLX = new QLineEdit(this);
    keywordEditFanLX->setObjectName(QStringLiteral("historyKeyword"));
    keywordEditFanLX->setPlaceholderText(QStringLiteral("可留空"));
    keywordEditFanLX->setClearButtonEnabled(true);
    pageSpinFanLX = new QSpinBox(this);
    pageSpinFanLX->setObjectName(QStringLiteral("historyPageSize"));
    pageSpinFanLX->setRange(1, 200);
    pageSpinFanLX->setValue(50);
    pageSpinFanLX->setSuffix(QStringLiteral(" 条"));
    pageSpinFanLX->setFixedWidth(120);
    timeCheckFanLX = new QCheckBox(QStringLiteral("按服务器 UTC 时间筛选"), this);
    timeCheckFanLX->setObjectName(QStringLiteral("historyTimeEnabled"));
    fromEditFanLX = new QDateTimeEdit(this);
    fromEditFanLX->setObjectName(QStringLiteral("historyFrom"));
    toEditFanLX = new QDateTimeEdit(this);
    toEditFanLX->setObjectName(QStringLiteral("historyTo"));
    configureUtcEditorFanLX(fromEditFanLX);
    configureUtcEditorFanLX(toEditFanLX);
    assignDefaultRangeFanLX();
    syncTimeEditorsEnabledFanLX();

    auto *filterCard = new QFrame(this);
    filterCard->setObjectName(QStringLiteral("historyFilterCard"));
    filterCard->setFrameShape(QFrame::NoFrame);
    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    grid->addWidget(fieldLabelFanLX(QStringLiteral("会话范围"), filterCard), 0, 0);
    grid->addWidget(conversationComboFanLX, 0, 1);
    grid->addWidget(fieldLabelFanLX(QStringLiteral("发送者账号"), filterCard), 0, 2);
    grid->addWidget(senderEditFanLX, 0, 3);
    grid->addWidget(fieldLabelFanLX(QStringLiteral("关键词"), filterCard), 1, 0);
    grid->addWidget(keywordEditFanLX, 1, 1);
    grid->addWidget(fieldLabelFanLX(QStringLiteral("每页条数"), filterCard), 1, 2);
    grid->addWidget(pageSpinFanLX, 1, 3, Qt::AlignLeft);
    grid->addWidget(timeCheckFanLX, 2, 1, 1, 3);
    grid->addWidget(fieldLabelFanLX(QStringLiteral("起始时间"), filterCard), 3, 0);
    grid->addWidget(fromEditFanLX, 3, 1);
    grid->addWidget(fieldLabelFanLX(QStringLiteral("结束时间"), filterCard), 3, 2);
    grid->addWidget(toEditFanLX, 3, 3);
    auto *hint = new QLabel(
        QStringLiteral("关键词按字面内容匹配，% 和 _ 不作为通配符。时间以服务器 UTC 为准；未勾选时间范围时，起止时间不会提交。"),
        filterCard);
    hint->setObjectName(QStringLiteral("historyHint"));
    hint->setWordWrap(true);
    auto *filterLayout = new QVBoxLayout(filterCard);
    filterLayout->setContentsMargins(16, 14, 16, 14);
    filterLayout->setSpacing(10);
    filterLayout->addLayout(grid);
    filterLayout->addWidget(hint);

    searchButtonFanLX = new QPushButton(QStringLiteral("查询"), this);
    searchButtonFanLX->setObjectName(QStringLiteral("historySearch"));
    searchButtonFanLX->setDefault(true);
    auto *resetButton = new QPushButton(QStringLiteral("重置条件"), this);
    resetButton->setObjectName(QStringLiteral("historyReset"));
    auto *cancelButton = new QPushButton(QStringLiteral("取消查询"), this);
    cancelButton->setObjectName(QStringLiteral("historyCancel"));
    earlierButtonFanLX = new QPushButton(QStringLiteral("下一页"), this);
    earlierButtonFanLX->setObjectName(QStringLiteral("historyEarlier"));
    earlierButtonFanLX->setEnabled(false);
    auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);
    closeButton->setObjectName(QStringLiteral("historyClose"));
    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    buttons->addWidget(searchButtonFanLX);
    buttons->addWidget(resetButton);
    buttons->addWidget(cancelButton);
    buttons->addStretch(1);
    buttons->addWidget(earlierButtonFanLX);
    buttons->addWidget(closeButton);

    auto *resultCard = new QFrame(this);
    resultCard->setObjectName(QStringLiteral("historyResultCard"));
    resultCard->setFrameShape(QFrame::NoFrame);
    auto *resultTitle = new QLabel(QStringLiteral("查询结果"), resultCard);
    resultTitle->setObjectName(QStringLiteral("historyResultTitle"));
    summaryLabelFanLX = new QLabel(resultCard);
    summaryLabelFanLX->setObjectName(QStringLiteral("historySummary"));
    summaryLabelFanLX->setWordWrap(true);
    summaryLabelFanLX->setText(QStringLiteral("每页默认 50 条，上限 200 条。结果按服务器时间倒序。"));
    auto *resultHeader = new QHBoxLayout();
    resultHeader->setSpacing(12);
    resultHeader->addWidget(resultTitle);
    resultHeader->addWidget(summaryLabelFanLX, 1);

    auto *statusPage = new QWidget(resultCard);
    statusPage->setObjectName(QStringLiteral("historyStatusPage"));
    statusTitleFanLX = new QLabel(statusPage);
    statusTitleFanLX->setObjectName(QStringLiteral("historyEmpty"));
    statusTitleFanLX->setAlignment(Qt::AlignCenter);
    statusTitleFanLX->setWordWrap(true);
    statusDetailFanLX = new QLabel(statusPage);
    statusDetailFanLX->setObjectName(QStringLiteral("historyStatusDetail"));
    statusDetailFanLX->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    statusDetailFanLX->setWordWrap(true);
    auto *statusLayout = new QVBoxLayout(statusPage);
    statusLayout->setContentsMargins(24, 28, 24, 28);
    statusLayout->addStretch(1);
    statusLayout->addWidget(statusTitleFanLX);
    statusLayout->addWidget(statusDetailFanLX);
    statusLayout->addStretch(2);

    resultsTableFanLX = new QTableWidget(0, 4, resultCard);
    resultsTableFanLX->setObjectName(QStringLiteral("historyResults"));
    resultsTableFanLX->setHorizontalHeaderLabels(
        {QStringLiteral("服务器时间"), QStringLiteral("发送者账号"), QStringLiteral("会话"),
         QStringLiteral("消息内容")});
    resultsTableFanLX->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTableFanLX->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTableFanLX->setSelectionMode(QAbstractItemView::ExtendedSelection);
    resultsTableFanLX->setWordWrap(true);
    resultsTableFanLX->setAlternatingRowColors(true);
    resultsTableFanLX->setShowGrid(false);
    resultsTableFanLX->setSortingEnabled(false);
    resultsTableFanLX->setDragDropMode(QAbstractItemView::NoDragDrop);
    resultsTableFanLX->verticalHeader()->setVisible(false);
    resultsTableFanLX->verticalHeader()->setDefaultSectionSize(36);
    resultsTableFanLX->verticalHeader()->setMinimumSectionSize(32);
    auto *header = resultsTableFanLX->horizontalHeader();
    header->setStretchLastSection(true);
    header->setHighlightSections(false);
    header->setMinimumSectionSize(96);
    header->setSectionResizeMode(0, QHeaderView::Interactive);
    header->setSectionResizeMode(1, QHeaderView::Interactive);
    header->setSectionResizeMode(2, QHeaderView::Interactive);
    header->setSectionResizeMode(3, QHeaderView::Stretch);
    header->resizeSection(0, 188);
    header->resizeSection(1, 140);
    header->resizeSection(2, 220);
    auto *copy = new QAction(QStringLiteral("复制选中内容"), resultsTableFanLX);
    copy->setShortcut(QKeySequence::Copy);
    copy->setShortcutContext(Qt::WidgetShortcut);
    resultsTableFanLX->addAction(copy);
    resultsTableFanLX->setContextMenuPolicy(Qt::ActionsContextMenu);

    resultStackFanLX = new QStackedWidget(resultCard);
    resultStackFanLX->addWidget(statusPage);
    resultStackFanLX->addWidget(resultsTableFanLX);
    resultStackFanLX->setCurrentWidget(statusPage);
    auto *resultLayout = new QVBoxLayout(resultCard);
    resultLayout->setContentsMargins(16, 14, 16, 12);
    resultLayout->setSpacing(8);
    resultLayout->addLayout(resultHeader);
    resultLayout->addWidget(resultStackFanLX, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(filterCard);
    layout->addLayout(buttons);
    layout->addWidget(resultCard, 1);

    conversationComboFanLX->addItem(conversationLabelFanLX(QString()), QString());
    showStatusPageFanLX(QStringLiteral("尚未查询"),
                        QStringLiteral("设置条件后点击「查询」。结果按服务器时间倒序显示。"));

    connect(searchButtonFanLX, &QPushButton::clicked, this, &HistoryDialogFanLX::searchRequestedFanLX);
    connect(earlierButtonFanLX, &QPushButton::clicked, this, &HistoryDialogFanLX::earlierRequestedFanLX);
    connect(resetButton, &QPushButton::clicked, this, &HistoryDialogFanLX::resetFiltersFanLX);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, [this] {
        emit cancelRequestedFanLX();
        resultsTableFanLX->setRowCount(0);
        showStatusPageFanLX(QStringLiteral("查询已取消"),
                            QStringLiteral("已中断当前历史查询。条件仍保留，可以修改后重新查询。"));
        setBusy(false);
        setHasMore(false);
        setSummary(QStringLiteral("查询已取消。可以修改条件后重新查询。"));
    });
    connect(timeCheckFanLX, &QCheckBox::toggled, this, [this](bool) { syncTimeEditorsEnabledFanLX(); });
    connect(copy, &QAction::triggered, this, &HistoryDialogFanLX::copySelectedFanLX);
    connect(this, &QDialog::finished, this, [this] {
        if (searchInFlightFanLX) {
            resultsTableFanLX->setRowCount(0);
            showStatusPageFanLX(QStringLiteral("查询已取消"),
                                QStringLiteral("已中断当前历史查询。条件仍保留，可以修改后重新查询。"));
            searchInFlightFanLX = false;
            hasMoreFanLX = false;
            earlierButtonFanLX->setEnabled(false);
        }
        emit cancelRequestedFanLX();
    });
}

void HistoryDialogFanLX::applyStyleFanLX() {
    setStyleSheet(QStringLiteral(
        "QDialog#historyDialogFanLX { background:#f3f5f8; color:#1f2329; font-size:13px; }"
        "QDialog#historyDialogFanLX QLabel#historyTitle { font-size:20px; font-weight:600; color:#1f2329; }"
        "QDialog#historyDialogFanLX QLabel#historyResultTitle { font-size:15px; font-weight:600; color:#1f2329; }"
        "QDialog#historyDialogFanLX QLabel#historySubtitle,"
        "QDialog#historyDialogFanLX QLabel#historyHint,"
        "QDialog#historyDialogFanLX QLabel#historySummary,"
        "QDialog#historyDialogFanLX QLabel#historyStatusDetail { color:#5b6472; font-size:12px; }"
        "QDialog#historyDialogFanLX QLabel#historyField { color:#1f2329; font-weight:600; }"
        "QDialog#historyDialogFanLX QLabel#historyEmpty { font-size:15px; font-weight:600; color:#1f2329; }"
        "QFrame#historyFilterCard, QFrame#historyResultCard {"
        "  background:#ffffff; border:1px solid #e3e7ed; border-radius:8px; }"
        "QDialog#historyDialogFanLX QLineEdit,"
        "QDialog#historyDialogFanLX QComboBox,"
        "QDialog#historyDialogFanLX QDateTimeEdit,"
        "QDialog#historyDialogFanLX QSpinBox {"
        "  background:#ffffff; border:1px solid #e3e7ed; border-radius:6px; padding:2px 8px;"
        "  min-height:28px; color:#1f2329; }"
        "QDialog#historyDialogFanLX QLineEdit:focus,"
        "QDialog#historyDialogFanLX QComboBox:focus,"
        "QDialog#historyDialogFanLX QDateTimeEdit:focus,"
        "QDialog#historyDialogFanLX QSpinBox:focus { border:1px solid #2f6fed; }"
        "QDialog#historyDialogFanLX QLineEdit:disabled,"
        "QDialog#historyDialogFanLX QDateTimeEdit:disabled { background:#f3f5f8; color:#8a94a3; }"
        "QDialog#historyDialogFanLX QComboBox::drop-down, QDialog#historyDialogFanLX QDateTimeEdit::drop-down {"
        "  border:none; width:22px; }"
        "QDialog#historyDialogFanLX QComboBox::down-arrow, QDialog#historyDialogFanLX QDateTimeEdit::down-arrow {"
        "  width:0; height:0; border-left:4px solid transparent; border-right:4px solid transparent;"
        "  border-top:5px solid #5b6472; }"
        "QDialog#historyDialogFanLX QPushButton {"
        "  min-height:34px; padding:0 14px; border-radius:6px; border:1px solid #e3e7ed;"
        "  background:#ffffff; color:#1f2329; }"
        "QDialog#historyDialogFanLX QPushButton:hover { background:#f7f9fc; }"
        "QDialog#historyDialogFanLX QPushButton:pressed { background:#eef1f6; }"
        "QDialog#historyDialogFanLX QPushButton:disabled { color:#8a94a3; background:#f3f5f8; }"
        "QDialog#historyDialogFanLX QPushButton#historySearch {"
        "  background:#2f6fed; border:1px solid #2f6fed; color:#ffffff; font-weight:600; }"
        "QDialog#historyDialogFanLX QPushButton#historySearch:hover { background:#2961d6; border-color:#2961d6; }"
        "QDialog#historyDialogFanLX QPushButton#historySearch:pressed { background:#2253ba; border-color:#2253ba; }"
        "QDialog#historyDialogFanLX QPushButton#historyCancel {"
        "  color:#9a4d3a; border:1px solid #e7c7bc; background:#ffffff; }"
        "QDialog#historyDialogFanLX QPushButton#historyCancel:hover { background:#fff6f3; }"
        "QDialog#historyDialogFanLX QTableWidget {"
        "  background:#ffffff; border:none; alternate-background-color:#f8fafc;"
        "  selection-background-color:#eaf1fe; selection-color:#1f2329; }"
        "QDialog#historyDialogFanLX QHeaderView::section {"
        "  background:#f7f9fc; color:#5b6472; border:none; border-bottom:1px solid #e3e7ed;"
        "  padding:6px 8px; font-weight:600; }"));
}

void HistoryDialogFanLX::assignDefaultRangeFanLX() {
    const auto end = QDateTime::currentDateTimeUtc();
    toEditFanLX->setDateTime(end);
    fromEditFanLX->setDateTime(end.addDays(-7));
}

void HistoryDialogFanLX::syncTimeEditorsEnabledFanLX() {
    const auto enabled = timeCheckFanLX->isChecked();
    fromEditFanLX->setEnabled(enabled);
    toEditFanLX->setEnabled(enabled);
}

void HistoryDialogFanLX::showStatusPageFanLX(const QString &title, const QString &detail) {
    statusTitleFanLX->setText(title);
    statusDetailFanLX->setText(detail);
    resultStackFanLX->setCurrentIndex(0);
}

int HistoryDialogFanLX::findKeyFanLX(const QString &key) const {
    return conversationComboFanLX->findData(key);
}

void HistoryDialogFanLX::setConversations(const QStringList &keys) {
    const auto current = conversationKey();
    QSignalBlocker blocker(conversationComboFanLX);
    conversationComboFanLX->clear();
    conversationComboFanLX->addItem(conversationLabelFanLX(QString()), QString());
    QSet<QString> seen;
    for (const auto &key : keys) {
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        conversationComboFanLX->addItem(conversationLabelFanLX(key), key);
    }
    // 刷新列表时保留当前选择。对不上的键单独补一条，避免改去查另一个会话。
    if (!current.isEmpty() && !seen.contains(current))
        conversationComboFanLX->addItem(conversationLabelFanLX(current), current);
    const auto index = findKeyFanLX(current);
    conversationComboFanLX->setCurrentIndex(index >= 0 ? index : 0);
}

void HistoryDialogFanLX::setCurrentConversation(const QString &key) {
    preferredKeyFanLX = key;
    auto index = findKeyFanLX(key);
    if (index < 0 && !key.isEmpty()) {
        conversationComboFanLX->addItem(conversationLabelFanLX(key), key);
        index = conversationComboFanLX->count() - 1;
    }
    if (index < 0)
        index = 0;
    conversationComboFanLX->setCurrentIndex(index);
}

QString HistoryDialogFanLX::conversationKey() const {
    const auto data = conversationComboFanLX->currentData();
    return data.isValid() ? data.toString() : QString();
}

QString HistoryDialogFanLX::senderFilter() const { return senderEditFanLX->text().trimmed(); }

QString HistoryDialogFanLX::keyword() const { return keywordEditFanLX->text(); }

int HistoryDialogFanLX::pageSize() const { return pageSpinFanLX->value(); }

bool HistoryDialogFanLX::useTimeRange() const { return timeCheckFanLX->isChecked(); }

QString HistoryDialogFanLX::utcTextFanLX(const QDateTimeEdit *edit) const {
    // 与提交前的校验使用同一格式。控件时区是 UTC，因此这里不会把本地偏移再换一次。
    return edit->dateTime().toUTC().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"));
}

QString HistoryDialogFanLX::fromText() const {
    return timeCheckFanLX->isChecked() ? utcTextFanLX(fromEditFanLX) : QString();
}

QString HistoryDialogFanLX::toText() const {
    return timeCheckFanLX->isChecked() ? utcTextFanLX(toEditFanLX) : QString();
}

QString HistoryDialogFanLX::filterErrorFanLX() const {
    const auto key = conversationKey();
    if (key.isEmpty() && senderFilter().isEmpty() && keyword().isEmpty() && !useTimeRange())
        return QStringLiteral("跨会话检索需要至少一个条件：发送者、关键词或时间范围。");
    if (!useTimeRange())
        return {};
    const auto from = fromText();
    const auto to = toText();
    const auto valid = [](const QString &text) {
        if (text.isEmpty())
            return true;
        const auto date = QDateTime::fromString(text, Qt::ISODate);
        return date.isValid() && date.toUTC().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ")) == text;
    };
    if ((from.isEmpty() && to.isEmpty()) || !valid(from) || !valid(to) ||
        (!from.isEmpty() && !to.isEmpty() && from > to))
        return QStringLiteral("起始时间不能晚于结束时间，并且需要能转换成服务器 UTC（YYYY-MM-DDTHH:mm:ssZ）。");
    return {};
}

void HistoryDialogFanLX::setSummary(const QString &text) { summaryLabelFanLX->setText(text); }

void HistoryDialogFanLX::setResultsText(const QString &text) {
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("（没有匹配的记录）")) {
        setResultRows({});
        return;
    }
    HistoryResultRowFanLX row;
    row.body = text;
    setResultRows({row});
}

void HistoryDialogFanLX::setResultRows(const QList<HistoryResultRowFanLX> &rows) {
    resultsTableFanLX->setSortingEnabled(false);
    resultsTableFanLX->clearContents();
    resultsTableFanLX->setRowCount(rows.size());
    if (rows.isEmpty()) {
        showStatusPageFanLX(QStringLiteral("没有找到匹配的聊天记录"),
                            QStringLiteral("可以检查会话、发送者、关键词或时间范围，也可以放宽筛选后再查询。"));
        return;
    }
    for (int index = 0; index < rows.size(); ++index) {
        const auto &row = rows.at(index);
        const auto time = row.serverTime.isEmpty() ? QStringLiteral("原始时间未知") : row.serverTime;
        // 结果行上的空会话键不是「所有会话」这个筛选项。
        const auto conversation = row.conversationKey.isEmpty() ? QStringLiteral("未列出的会话")
                                                                : conversationLabelFanLX(row.conversationKey);
        resultsTableFanLX->setItem(index, 0, resultItemFanLX(time));
        resultsTableFanLX->setItem(index, 1, resultItemFanLX(row.sender));
        resultsTableFanLX->setItem(index, 2, resultItemFanLX(conversation));
        resultsTableFanLX->setItem(index, 3, resultItemFanLX(row.body));
    }
    resultsTableFanLX->resizeRowsToContents();
    for (int index = 0; index < resultsTableFanLX->rowCount(); ++index) {
        if (resultsTableFanLX->rowHeight(index) > kResultRowCapFanLX)
            resultsTableFanLX->setRowHeight(index, kResultRowCapFanLX);
    }
    resultStackFanLX->setCurrentWidget(resultsTableFanLX);
}

void HistoryDialogFanLX::setHasMore(bool more) {
    hasMoreFanLX = more;
    earlierButtonFanLX->setEnabled(more && !searchInFlightFanLX);
}

void HistoryDialogFanLX::setBusy(bool busy) {
    // 查询按钮保持可用：可以改条件后立刻重查，过期结果由控制器的 queryId 丢弃。
    searchInFlightFanLX = busy;
    searchButtonFanLX->setEnabled(true);
    earlierButtonFanLX->setEnabled(!busy && hasMoreFanLX);
    if (busy) {
        showStatusPageFanLX(QStringLiteral("查询中"),
                            QStringLiteral("正在检索历史消息。可以修改条件后再次查询，或点击「取消查询」。"));
        return;
    }
    // 提交失败时调用方只结束忙碌。已有行还在，就回到表格，而不是停在「查询中」。
    if (resultsTableFanLX->rowCount() > 0)
        resultStackFanLX->setCurrentWidget(resultsTableFanLX);
}

void HistoryDialogFanLX::setQueryFailed(const QString &detail) {
    if (!searchInFlightFanLX)
        return;
    const auto reason = detail.isEmpty() ? QStringLiteral("查询没有完成。") : detail;
    resultsTableFanLX->setRowCount(0);
    showStatusPageFanLX(QStringLiteral("查询失败"), reason);
    setBusy(false);
    setHasMore(false);
    setSummary(QStringLiteral("查询失败：") + reason);
}

void HistoryDialogFanLX::resetFiltersFanLX() {
    if (searchInFlightFanLX)
        emit cancelRequestedFanLX();
    searchInFlightFanLX = false;
    senderEditFanLX->clear();
    keywordEditFanLX->clear();
    timeCheckFanLX->setChecked(false);
    pageSpinFanLX->setValue(50);
    assignDefaultRangeFanLX();
    syncTimeEditorsEnabledFanLX();
    auto index = findKeyFanLX(preferredKeyFanLX);
    if (index < 0)
        index = 0;
    conversationComboFanLX->setCurrentIndex(index);
    resultsTableFanLX->setRowCount(0);
    setHasMore(false);
    showStatusPageFanLX(QStringLiteral("尚未查询"),
                        QStringLiteral("条件已重置。设置筛选后点击「查询」，结果按服务器时间倒序显示。"));
    setSummary(QStringLiteral("条件已重置。每页 50 条，时间范围未启用。"));
}

void HistoryDialogFanLX::copySelectedFanLX() {
    const auto indexes = resultsTableFanLX->selectionModel()->selectedRows();
    if (indexes.isEmpty())
        return;
    QList<int> rows;
    rows.reserve(indexes.size());
    for (const auto &index : indexes)
        rows.append(index.row());
    std::sort(rows.begin(), rows.end());
    QStringList lines;
    for (const auto row : rows) {
        QStringList cells;
        for (int column = 0; column < resultsTableFanLX->columnCount(); ++column) {
            const auto *item = resultsTableFanLX->item(row, column);
            cells.append(item ? item->text() : QString());
        }
        lines.append(cells.join(QLatin1Char('\t')));
    }
    QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

} // namespace fanlxGui

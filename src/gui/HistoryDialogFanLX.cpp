#include "gui/HistoryDialogFanLX.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QTextBrowser>

namespace fanlxGui {

HistoryDialogFanLX::HistoryDialogFanLX(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("聊天记录查询"));
    resize(720, 420);

    conversationComboFanLX = new QComboBox(this);
    conversationComboFanLX->setEditable(true);
    conversationComboFanLX->setToolTip(QStringLiteral("留空并填写发送者或关键词可跨会话检索"));
    senderEditFanLX = new QLineEdit(this);
    senderEditFanLX->setPlaceholderText(QStringLiteral("发送者 QQ 号（可留空）"));
    keywordEditFanLX = new QLineEdit(this);
    keywordEditFanLX->setPlaceholderText(QStringLiteral("关键词（按字面量匹配，% 与 _ 不当作通配符）"));
    timeCheckFanLX = new QCheckBox(QStringLiteral("按服务器时间过滤"), this);
    fromEditFanLX = new QLineEdit(this);
    fromEditFanLX->setObjectName(QStringLiteral("historyFrom"));
    fromEditFanLX->setPlaceholderText(QStringLiteral("起始，如 2026-09-01T00:00:00Z"));
    toEditFanLX = new QLineEdit(this);
    toEditFanLX->setObjectName(QStringLiteral("historyTo"));
    timeCheckFanLX->setObjectName(QStringLiteral("historyTimeEnabled"));
    toEditFanLX->setPlaceholderText(QStringLiteral("结束，如 2026-09-30T23:59:59Z"));
    pageSpinFanLX = new QSpinBox(this);
    pageSpinFanLX->setRange(1, 200);
    pageSpinFanLX->setValue(50);

    auto *form = new QFormLayout();
    form->addRow(QStringLiteral("会话"), conversationComboFanLX);
    form->addRow(QStringLiteral("发送者"), senderEditFanLX);
    form->addRow(QStringLiteral("关键词"), keywordEditFanLX);
    form->addRow(timeCheckFanLX, fromEditFanLX);
    form->addRow(QStringLiteral("结束时间"), toEditFanLX);
    form->addRow(QStringLiteral("每页条数"), pageSpinFanLX);

    searchButtonFanLX = new QPushButton(QStringLiteral("查询"), this);
    searchButtonFanLX->setObjectName(QStringLiteral("historySearch"));
    earlierButtonFanLX = new QPushButton(QStringLiteral("下一页"), this);
    earlierButtonFanLX->setEnabled(false);
    auto *cancel = new QPushButton(QStringLiteral("取消查询"), this);
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    auto *buttons = new QHBoxLayout();
    buttons->addWidget(searchButtonFanLX);
    buttons->addWidget(earlierButtonFanLX);
    buttons->addWidget(cancel);
    buttons->addStretch(1);
    buttons->addWidget(close);

    summaryLabelFanLX = new QLabel(this);
    summaryLabelFanLX->setWordWrap(true);
    summaryLabelFanLX->setText(QStringLiteral("每页默认 50 条，上限 200 条；结果按服务器时间倒序。"));
    resultsLabelFanLX = new QTextBrowser(this);
    resultsLabelFanLX->setObjectName(QStringLiteral("historyResults"));
    resultsLabelFanLX->setStyleSheet(QStringLiteral("font-family:Consolas,monospace;"));

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
    layout->addWidget(summaryLabelFanLX);
    layout->addWidget(resultsLabelFanLX, 1);

    connect(searchButtonFanLX, &QPushButton::clicked, this, &HistoryDialogFanLX::searchRequestedFanLX);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(earlierButtonFanLX, &QPushButton::clicked, this, &HistoryDialogFanLX::earlierRequestedFanLX);
    connect(cancel, &QPushButton::clicked, this, [this] {
        emit cancelRequestedFanLX(); setBusy(false); setHasMore(false);
        setSummary(QStringLiteral("查询已取消"));
    });
    connect(this, &QDialog::finished, this, &HistoryDialogFanLX::cancelRequestedFanLX);
}

void HistoryDialogFanLX::setConversations(const QStringList &keys) {
    const auto current = conversationComboFanLX->currentText();
    conversationComboFanLX->clear();
    conversationComboFanLX->addItem(QString());
    conversationComboFanLX->addItems(keys);
    if (!current.isEmpty())
        conversationComboFanLX->setCurrentText(current);
}

void HistoryDialogFanLX::setCurrentConversation(const QString &key) {
    conversationComboFanLX->setCurrentText(key);
}

QString HistoryDialogFanLX::conversationKey() const { return conversationComboFanLX->currentText().trimmed(); }
QString HistoryDialogFanLX::senderFilter() const { return senderEditFanLX->text().trimmed(); }
QString HistoryDialogFanLX::keyword() const { return keywordEditFanLX->text(); }
int HistoryDialogFanLX::pageSize() const { return pageSpinFanLX->value(); }
bool HistoryDialogFanLX::useTimeRange() const { return timeCheckFanLX->isChecked(); }
QString HistoryDialogFanLX::fromText() const { return fromEditFanLX->text().trimmed(); }
QString HistoryDialogFanLX::toText() const { return toEditFanLX->text().trimmed(); }

void HistoryDialogFanLX::setSummary(const QString &text) { summaryLabelFanLX->setText(text); }
void HistoryDialogFanLX::setResultsText(const QString &text) { resultsLabelFanLX->setPlainText(text); }
void HistoryDialogFanLX::setHasMore(bool more) { earlierButtonFanLX->setEnabled(more); }

void HistoryDialogFanLX::setBusy(bool busy) {
    searchButtonFanLX->setEnabled(true); // 允许修改条件后立即重查，旧结果由 queryId 作废。
    if (busy) earlierButtonFanLX->setEnabled(false);
    if (busy)
        summaryLabelFanLX->setText(QStringLiteral("查询中…（界面保持可操作，可随时取消或改条件重查）"));
}

} // namespace fanlxGui

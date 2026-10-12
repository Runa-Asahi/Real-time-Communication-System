#pragma once
#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextBrowser;

namespace fanlxGui {

// 历史查询窗口：会话、发送者、关键词、时间范围、每页条数（默认 50，上限 200）。
// 结果由控制器以 History 事件返回，窗口只负责收集条件与展示结果。
class HistoryDialogFanLX : public QDialog {
    Q_OBJECT

  public:
    explicit HistoryDialogFanLX(QWidget *parent = nullptr);

    void setConversations(const QStringList &keys);
    void setCurrentConversation(const QString &key);
    QString conversationKey() const;
    QString senderFilter() const;
    QString keyword() const;
    int pageSize() const;
    // 时间范围（闭区间）；启用时间过滤时才生效。
    bool useTimeRange() const;
    QString fromText() const;
    QString toText() const;

    void setSummary(const QString &text);
    void setResultsText(const QString &text);
    void setBusy(bool busy);
    void setHasMore(bool more);

  signals:
    void searchRequestedFanLX();
    void earlierRequestedFanLX();
    void cancelRequestedFanLX();

  private:
    QComboBox *conversationComboFanLX = nullptr;
    QLineEdit *senderEditFanLX = nullptr;
    QLineEdit *keywordEditFanLX = nullptr;
    QCheckBox *timeCheckFanLX = nullptr;
    QLineEdit *fromEditFanLX = nullptr;
    QLineEdit *toEditFanLX = nullptr;
    QSpinBox *pageSpinFanLX = nullptr;
    QPushButton *searchButtonFanLX = nullptr;
    QLabel *summaryLabelFanLX = nullptr;
    QTextBrowser *resultsLabelFanLX = nullptr;
    QPushButton *earlierButtonFanLX = nullptr;
};

} // namespace fanlxGui

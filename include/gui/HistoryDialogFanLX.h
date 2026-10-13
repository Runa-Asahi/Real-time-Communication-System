#pragma once
#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

namespace fanlxGui {

// 一条历史查询结果。serverTime 使用服务端原始时间字符串，空表示未知，界面不得另填本地时间。
struct HistoryResultRowFanLX {
    QString serverTime;
    QString sender;
    QString conversationKey;
    QString body;
};

// 历史查询窗口：会话、发送者、关键词、服务器 UTC 时间范围、每页条数（默认 50，上限 200）。
// 查询仍由控制器异步执行。窗口只收集条件、展示状态和结果。
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
    // 时间范围（闭区间）。未启用时 fromText/toText 为空，控件里的默认值不会进入查询。
    bool useTimeRange() const;
    QString fromText() const;
    QString toText() const;
    // 空字符串表示条件可以提交。跨会话必须另有发送者、关键词或时间；时间文本仍须满足 UTC 往返。
    QString filterErrorFanLX() const;

    void setSummary(const QString &text);
    // 兼容旧调用：纯文本落入消息列，不按 HTML 解析。空文本和「（没有匹配的记录）」显示空状态。
    void setResultsText(const QString &text);
    void setResultRows(const QList<HistoryResultRowFanLX> &rows);
    void setBusy(bool busy);
    void setHasMore(bool more);
    // 仅在查询进行中替换结果区。无匹配的成功结果不是失败。
    void setQueryFailed(const QString &detail);
    bool isSearchInFlight() const { return searchInFlightFanLX; }

  signals:
    void searchRequestedFanLX();
    void earlierRequestedFanLX();
    void cancelRequestedFanLX();

  private:
    void applyStyleFanLX();
    void assignDefaultRangeFanLX();
    void syncTimeEditorsEnabledFanLX();
    void showStatusPageFanLX(const QString &title, const QString &detail);
    void resetFiltersFanLX();
    void copySelectedFanLX();
    QString utcTextFanLX(const QDateTimeEdit *edit) const;
    int findKeyFanLX(const QString &key) const;
    static QString conversationLabelFanLX(const QString &key);
    static void configureUtcEditorFanLX(QDateTimeEdit *edit);

    QComboBox *conversationComboFanLX = nullptr;
    QLineEdit *senderEditFanLX = nullptr;
    QLineEdit *keywordEditFanLX = nullptr;
    QCheckBox *timeCheckFanLX = nullptr;
    QDateTimeEdit *fromEditFanLX = nullptr;
    QDateTimeEdit *toEditFanLX = nullptr;
    QSpinBox *pageSpinFanLX = nullptr;
    QPushButton *searchButtonFanLX = nullptr;
    QPushButton *earlierButtonFanLX = nullptr;
    QLabel *summaryLabelFanLX = nullptr;
    QLabel *statusTitleFanLX = nullptr;
    QLabel *statusDetailFanLX = nullptr;
    QStackedWidget *resultStackFanLX = nullptr;
    QTableWidget *resultsTableFanLX = nullptr;
    QString preferredKeyFanLX;
    bool searchInFlightFanLX = false;
    bool hasMoreFanLX = false;
};

} // namespace fanlxGui

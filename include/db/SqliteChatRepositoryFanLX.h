#pragma once
#include "db/ChatRepositoryFanLX.h"
#include "db/SqliteHandleFanLX.h"

namespace chatDbFanLX {

// 客户端聊天库的 SQLite 实现。连接与语句由本对象独占：
// 只允许数据库工作线程（或单线程测试）调用，禁止从 UI 线程直接调用。
class SqliteChatRepositoryFanLX final : public ChatRepositoryFanLX {
  public:
    struct ConfigFanLX {
        std::filesystem::path databasePath;
        std::string serverInstanceId;
        std::string ownerAccount;
        std::string deviceId;
    };

    explicit SqliteChatRepositoryFanLX(ConfigFanLX config);
    ~SqliteChatRepositoryFanLX() override;
    SqliteChatRepositoryFanLX(const SqliteChatRepositoryFanLX &) = delete;
    SqliteChatRepositoryFanLX &operator=(const SqliteChatRepositoryFanLX &) = delete;

    std::int64_t appendPending(const std::string &conversationKey, ScopeFanLX scope,
                               const std::string &target, const std::string &body,
                               const std::string &requestId, std::string &requestIdOut) override;
    void markAccepted(const std::string &requestId, std::int64_t messageId,
                      std::int64_t conversationSequence, const std::string &acceptedAtUtc) override;
    void markFailed(const std::string &requestId, const std::string &reason) override;
    bool appendDelivered(const MessageRowFanLX &row) override;

    std::vector<PendingMessageFanLX> pending() const override;
    std::int64_t lastConversationSequence(const std::string &conversationKey) const override;
    void markRead(const std::string &conversationKey, std::int64_t sequence) override;
    std::int64_t lastReadSequence(const std::string &conversationKey) const override;

    HistoryPageFanLX queryHistory(const HistoryQueryFanLX &query) const override;
    std::vector<ConversationRowFanLX> conversations() const override;
    void forgetConversations(const std::vector<std::string> &keepKeys) override;

    std::int64_t messageCount() const override;
    std::int64_t nextLocalSequence() const override;
    std::string serverInstanceId() const override { return configFanLX.serverInstanceId; }
    std::string ownerAccount() const override { return configFanLX.ownerAccount; }
    std::string deviceId() const override { return deviceIdFanLX; }

    void importLegacyMessage(const MessageRowFanLX &row) override;
    void rebuildSyncCursorsFanLX() override;

    // 测试与诊断：分页查询的执行计划（验证 keyset 分页走覆盖索引）。
    std::string explainPlanFanLX(const HistoryQueryFanLX &query) const;
    // 测量专用：把随后的写入合并到单个事务（不是接收路径）。
    void beginBulkImportFanLX();
    void endBulkImportFanLX();

  private:
    void ensureConversationFanLX(const std::string &key, ScopeFanLX scope, const std::string &target);
    std::int64_t consumeDeviceSequenceFanLX();
    std::int64_t readCounterFanLX(const std::string &key) const;

    ConfigFanLX configFanLX;
    sqliteFanLX::ConnectionFanLX connectionFanLX;
    std::string deviceIdFanLX;
    bool bulkImportFanLX = false;
};

} // namespace chatDbFanLX

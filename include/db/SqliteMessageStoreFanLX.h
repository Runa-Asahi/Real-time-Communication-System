#pragma once
#include "db/ChatRepositoryFanLX.h"
#include "db/SqliteHandleFanLX.h"
#include <map>
#include <optional>

namespace chatDbFanLX {

// 服务端接收请求：recipients 为发送时解析出的收件人快照（含发送者）。
struct ServerAcceptRequestFanLX {
    ScopeFanLX scope = ScopeFanLX::Direct;
    std::string sender;
    std::string target; // 单聊为对端账号，群聊为群号
    std::string requestId;
    std::string body;
    std::vector<std::string> recipients;
};

struct ServerAcceptResultFanLX {
    MessageRowFanLX message; // 已填 messageId / conversationSequence / acceptedAtUtc
    bool replayed = false;   // true 表示同 requestId 命中旧结果，未新增行
};

// 投递候选：只表示"按游标判断尚未投递"；群可见性由调用方按当前成员关系再次判定。
struct ServerDeliveryCandidateFanLX {
    MessageRowFanLX message;
    std::string conversationKey;
};

// 连接内投递游标：会话键 → 已确认的最大会话序号。
using DeliveryCursorMapFanLX = std::map<std::string, std::int64_t>;

// 服务端消息库。独占连接，只允许服务端控制线程（或单线程测试）调用。
class SqliteMessageStoreFanLX {
  public:
    SqliteMessageStoreFanLX(const std::filesystem::path &databasePath, const std::string &serverInstanceId);
    ~SqliteMessageStoreFanLX();
    SqliteMessageStoreFanLX(const SqliteMessageStoreFanLX &) = delete;
    SqliteMessageStoreFanLX &operator=(const SqliteMessageStoreFanLX &) = delete;

    // 一个事务内：分配 messageId、分配会话序号、写收件人快照、推进全局计数。
    // 同 requestId 同内容返回旧结果（replayed）；内容冲突抛 ChatConflictFanLX。
    // 只有提交成功返回后，调用方才允许发送 SEND_ACK。
    ServerAcceptResultFanLX accept(const ServerAcceptRequestFanLX &request);
    std::optional<ServerAcceptResultFanLX> findByIdempotencyKey(const std::string &sender,
                                                               const std::string &requestId) const;
    // 按服务器消息编号回查（校验 ACK 所属会话用；客户端无法伪造编号）。
    std::optional<MessageRowFanLX> findById(std::int64_t messageId) const;

    // 按 messageId 升序返回游标之后、且收件人包含 account 的消息。
    std::vector<ServerDeliveryCandidateFanLX> pendingFor(const std::string &account,
                                                        const DeliveryCursorMapFanLX &cursors,
                                                        std::size_t limit = 256) const;
    // 该账号全部已确认进度：连接建立时读取，保证重连/重启后按库内游标继续而不是重放。
    DeliveryCursorMapFanLX deliveryCursorsFor(const std::string &account) const;
    void advanceDeliveryCursor(const std::string &account, const std::string &conversationKey,
                               std::int64_t sequence);
    // 会话序号与编号查询（界面/诊断用）。
    std::int64_t conversationSequenceOnServer(const std::string &conversationKey) const;
    std::int64_t nextMessageId() const;
    std::int64_t messageCount() const;
    std::string serverInstanceId() const { return instanceIdFanLX; }
    std::string integrityCheck() const;

    // 迁移专用：按旧存储已确认的编号与序号原样导入，并把会话计数抬到不小于已导入序号；
    // 全局编号抬升由 raiseMessageCounterFanLX 完成。业务路径禁止调用。
    void importLegacyMessage(const ServerAcceptRequestFanLX &request, std::int64_t messageId,
                             std::int64_t conversationSequence, const std::vector<std::string> &recipients);
    void raiseMessageCounterFanLX(std::int64_t nextMessageId);

  private:
    std::string instanceIdFanLX;
    sqliteFanLX::ConnectionFanLX connectionFanLX;
};

} // namespace chatDbFanLX

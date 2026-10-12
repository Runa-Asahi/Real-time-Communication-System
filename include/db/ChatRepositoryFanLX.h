#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// 聊天记录数据库（客户端）。
//
// 表结构要点：
// - messages：本地待发送消息在数据库中真正存在（message_id 为 NULL），因此"待发送"这件事
//   不依赖内存队列；服务端编号由 SEND_ACK 回填。
// - conversations/sync_cursors：会话目录与已确认进度，用于增量同步与未读统计。
//
// 全部写入都在数据库工作线程中执行，本类不做线程同步。
namespace chatDbFanLX {

class ChatRepositoryErrorFanLX : public std::runtime_error {
  public:
    enum class KindFanLX { Storage, Conflict, Unavailable, Unsupported };
    ChatRepositoryErrorFanLX(KindFanLX kind, std::string message, int code = 0);
    KindFanLX kind() const noexcept { return kindFanLX; }
    int code() const noexcept { return codeFanLX; }

  private:
    KindFanLX kindFanLX;
    int codeFanLX;
};

class ChatConflictFanLX : public ChatRepositoryErrorFanLX {
  public:
    explicit ChatConflictFanLX(const std::string &message);
};

class ChatMigrationErrorFanLX : public ChatRepositoryErrorFanLX {
  public:
    explicit ChatMigrationErrorFanLX(const std::string &message);
};

// 会话范围：值直接落库，禁止改动既有字符串。
enum class ScopeFanLX { Direct, Group };

// 消息状态：Pending 表示等待服务器确认；SentAck 表示服务器已接收；Received 表示对端发来的。
enum class MessageStateFanLX { Pending, SentAck, Received, Failed };

struct MessageRowFanLX {
    std::string conversationKey;
    std::int64_t localSequence = 0;      // 会话内本地序号，主键的一部分
    std::optional<std::int64_t> messageId; // 服务器编号；待发送消息为 nullopt
    std::int64_t conversationSequence = 0; // 服务器分配的会话序号；待发送为 0
    std::string sender;
    std::string recipient;
    std::string body;
    std::string requestId;               // 幂等键；对端投递为空
    std::string acceptedAtUtc;           // 服务器时间；未知时为空（不伪造）
    MessageStateFanLX state = MessageStateFanLX::Received;
    std::string failureReason;
};

// 待发送记录：outbox 视图，供重发与界面显示。
struct PendingMessageFanLX {
    std::string requestId;
    std::int64_t localSequence = 0;
    ScopeFanLX scope = ScopeFanLX::Direct;
    std::string conversationKey; // 权威会话标识；收件人由它按方向推导
    std::string target;          // 由 conversationKey 推导出的收件人（不是会话目录里的原始字段）
    std::string body;
    std::string failureReason;
};

struct HistoryQueryFanLX {
    std::string conversationKey;         // 空 = 跨会话检索（需给出发送者或关键词）
    std::string sender;                  // 空 = 不限
    std::string keyword;                 // 空 = 不限；字面量匹配（%/_ 不解释为通配符）
    std::string beforeAcceptedAtUtc;     // 空 = 不限；按服务器时间倒序翻页
    std::int64_t beforeSequence = 0;     // keyset 游标；0 = 第一页
    std::int64_t limit = 50;             // 每页条数
    bool ascending = false;              // true = 会话序号升序（用于"返回最新"）
    std::string fromAcceptedAtUtc;       // 闭区间；未知时间不匹配
    std::string toAcceptedAtUtc;
    std::int64_t beforeLocalSequence = 0; // 跨会话同秒记录的唯一分页边界
};

inline constexpr int defaultPageSizeFanLX = 50;
inline constexpr int maxPageSizeFanLX = 200;

struct HistoryPageFanLX {
    std::vector<MessageRowFanLX> messages;
    bool hasMore = false;
    std::int64_t nextBeforeSequence = 0;      // 下一页游标
    std::string nextBeforeAcceptedAtUtc;      // 跨会话检索时的下一页游标
    std::int64_t nextBeforeLocalSequence = 0;
};

struct ConversationRowFanLX {
    std::string conversationKey;
    ScopeFanLX scope = ScopeFanLX::Direct;
    std::int64_t lastConversationSequence = 0;
    std::int64_t lastReadSequence = 0;
    std::int64_t unreadCount = 0;
    std::string lastMessageBody;
    std::string lastSender;
};

// 客户端聊天库接口。实现由数据库工作线程独占。
class ChatRepositoryFanLX {
  public:
    virtual ~ChatRepositoryFanLX() = default;

    // 记录一条本地发送消息（状态 Pending，尚无服务器编号）。返回本地序号。
    // requestId 为空时按"设备 + 本次分配的本地序号"生成，并回填到 requestIdOut：
    // 请求幂等键必须与序号绑定，调用方据此发 SEND，重连后可用同一键安全重放。
    virtual std::int64_t appendPending(const std::string &conversationKey, ScopeFanLX scope,
                                       const std::string &target, const std::string &body,
                                       const std::string &requestId, std::string &requestIdOut) = 0;
    // SEND_ACK：回填服务器编号与会话序号并转为 SentAck。
    virtual void markAccepted(const std::string &requestId, std::int64_t messageId,
                              std::int64_t conversationSequence, const std::string &acceptedAtUtc) = 0;
    virtual void markFailed(const std::string &requestId, const std::string &reason) = 0;

    // 记录一条投递消息。返回 true 表示新增，false 表示重复投递（内容一致）。
    // 同一 messageId 内容不一致必须抛 ChatConflictFanLX。
    virtual bool appendDelivered(const MessageRowFanLX &row) = 0;

    virtual std::vector<PendingMessageFanLX> pending() const = 0;
    virtual std::int64_t lastConversationSequence(const std::string &conversationKey) const = 0;
    virtual void markRead(const std::string &conversationKey, std::int64_t sequence) = 0;
    virtual std::int64_t lastReadSequence(const std::string &conversationKey) const = 0;

    virtual HistoryPageFanLX queryHistory(const HistoryQueryFanLX &query) const = 0;
    virtual std::vector<ConversationRowFanLX> conversations() const = 0;
    // 按当前是否仍属于授权会话清理本地目录（离开群后不再显示）。
    virtual void forgetConversations(const std::vector<std::string> &keepKeys) = 0;

    virtual std::int64_t messageCount() const = 0;
    virtual std::int64_t nextLocalSequence() const = 0;
    virtual std::string serverInstanceId() const = 0;
    virtual std::string ownerAccount() const = 0;
    virtual std::string deviceId() const = 0;
    // SQL 迁移接口：按已确认内容原样导入旧记录（允许乱序），随后重算同步游标。
    virtual void importLegacyMessage(const MessageRowFanLX &row) = 0;
    virtual void rebuildSyncCursorsFanLX() = 0;
};

// 会话键与网络层保持同一表示：单聊按账号字典序，群聊按群号。
std::string conversationKeyFanLX(ScopeFanLX scope, const std::string &self, const std::string &peer);
std::string scopeTextFanLX(ScopeFanLX scope);
ScopeFanLX scopeFromTextFanLX(const std::string &text);
std::string stateTextFanLX(MessageStateFanLX state);
MessageStateFanLX stateFromTextFanLX(const std::string &text);
// 幂等键：设备标识 + 本地序号。
std::string buildRequestIdFanLX(const std::string &deviceId, std::int64_t localSequence);
// 转义 LIKE 的通配符，使关键词按字面量匹配。
std::string escapeLikeFanLX(const std::string &keyword);
std::string utcNowFanLX();
std::string toUtcTextFanLX(std::int64_t unixSeconds);

} // namespace chatDbFanLX

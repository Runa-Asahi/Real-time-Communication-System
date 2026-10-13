#include "db/SqliteChatRepositoryFanLX.h"
#include "db/ChatSchemaStoreFanLX.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <limits>
#include <map>
#include <set>
#include <sqlite3.h>

namespace chatDbFanLX {

namespace {

constexpr const char *messageColumnsFanLX =
    "device_sequence, conversation_key, message_id, conversation_sequence, sender, recipient,"
    " request_id, body, accepted_at_utc, state, failure_reason";

MessageRowFanLX readMessageRowFanLX(sqliteFanLX::StatementFanLX &statement) {
    MessageRowFanLX row;
    row.localSequence = statement.columnInt64(0);
    row.conversationKey = statement.columnText(1);
    if (!statement.columnIsNull(2))
        row.messageId = statement.columnInt64(2);
    row.conversationSequence = statement.columnInt64(3);
    row.sender = statement.columnText(4);
    row.recipient = statement.columnText(5);
    row.requestId = statement.columnText(6);
    row.body = statement.columnText(7);
    row.acceptedAtUtc = statement.columnIsNull(8) ? std::string() : statement.columnText(8);
    row.state = stateFromTextFanLX(statement.columnText(9));
    row.failureReason = statement.columnText(10);
    return row;
}

std::int64_t unixNowSecFanLX() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// 会话对端必须按方向推导：自己发的看收件人，收到的看发送者。
// 直接取 recipient 会把 DELIVER 的收件人（= 本人）当成对端，算出 d|自己|自己 这种错键。
std::string peerOfRowFanLX(const MessageRowFanLX &row, const std::string &owner) {
    return row.sender == owner ? row.recipient : row.sender;
}

ScopeFanLX scopeOfKeyFanLX(const std::string &conversationKey) {
    return conversationKey.rfind("g|", 0) == 0 ? ScopeFanLX::Group : ScopeFanLX::Direct;
}

} // namespace

std::string scopeTextFanLX(ScopeFanLX scope) { return scope == ScopeFanLX::Group ? "group" : "direct"; }

ScopeFanLX scopeFromTextFanLX(const std::string &text) {
    if (text == "group") return ScopeFanLX::Group;
    if (text == "direct") return ScopeFanLX::Direct;
    throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "未知会话范围：" + text);
}

std::string stateTextFanLX(MessageStateFanLX state) {
    switch (state) {
    case MessageStateFanLX::Pending: return "pending";
    case MessageStateFanLX::SentAck: return "sent_ack";
    case MessageStateFanLX::Received: return "received";
    case MessageStateFanLX::Failed: return "failed";
    }
    return "received";
}

MessageStateFanLX stateFromTextFanLX(const std::string &text) {
    if (text == "pending") return MessageStateFanLX::Pending;
    if (text == "sent_ack") return MessageStateFanLX::SentAck;
    if (text == "received") return MessageStateFanLX::Received;
    if (text == "failed") return MessageStateFanLX::Failed;
    throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "未知消息状态：" + text);
}

std::string conversationKeyFanLX(ScopeFanLX scope, const std::string &self, const std::string &peer) {
    if (scope == ScopeFanLX::Group)
        return "g|" + peer;
    // 单聊键与网络层一致：双方账号字典序，保证两端算出同一把键。
    return "d|" + std::min(self, peer) + "|" + std::max(self, peer);
}

ChatRepositoryErrorFanLX::ChatRepositoryErrorFanLX(KindFanLX kind, std::string message, int code)
    : std::runtime_error(std::move(message)), kindFanLX(kind), codeFanLX(code) {}

ChatConflictFanLX::ChatConflictFanLX(const std::string &message)
    : ChatRepositoryErrorFanLX(KindFanLX::Conflict, message) {}

ChatMigrationErrorFanLX::ChatMigrationErrorFanLX(const std::string &message)
    : ChatRepositoryErrorFanLX(KindFanLX::Unsupported, message) {}

std::string escapeLikeFanLX(const std::string &keyword) {
    std::string escaped;
    escaped.reserve(keyword.size() + 8);
    for (const char character : keyword) {
        if (character == '\\' || character == '%' || character == '_')
            escaped.push_back('\\');
        escaped.push_back(character);
    }
    return escaped;
}

std::string toUtcTextFanLX(std::int64_t unixSeconds) {
    const std::time_t moment = static_cast<std::time_t>(unixSeconds);
    std::tm parts{};
    if (gmtime_s(&parts, &moment) != 0)
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "时间转换失败");
    char buffer[32]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &parts) == 0)
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "时间格式化失败");
    return buffer;
}

std::string utcNowFanLX() {
    const auto now = std::chrono::system_clock::now();
    return toUtcTextFanLX(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
}

std::string buildRequestIdFanLX(const std::string &deviceId, std::int64_t localSequence) {
    if (deviceId.empty() || localSequence <= 0)
        throw std::invalid_argument("requestId 构造参数非法");
    return deviceId + "-" + std::to_string(localSequence);
}

SqliteChatRepositoryFanLX::SqliteChatRepositoryFanLX(ConfigFanLX config)
    : configFanLX(std::move(config)), connectionFanLX(configFanLX.databasePath),
      deviceIdFanLX(configFanLX.deviceId) {
    if (configFanLX.databasePath.empty() || configFanLX.ownerAccount.empty() ||
        configFanLX.deviceId.empty() || configFanLX.serverInstanceId.empty())
        throw std::invalid_argument("聊天库配置缺失");
    applyClientSchemaFanLX(connectionFanLX, configFanLX.serverInstanceId, configFanLX.ownerAccount,
                           configFanLX.deviceId);
}

SqliteChatRepositoryFanLX::~SqliteChatRepositoryFanLX() = default;

std::int64_t SqliteChatRepositoryFanLX::readCounterFanLX(const std::string &key) const {
    sqliteFanLX::StatementFanLX statement(connectionFanLX,
                                         "SELECT value FROM schema_meta WHERE key = ?1");
    sqliteFanLX::bindAllFanLX(statement, key);
    if (!statement.step())
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage,
                                      "聊天库缺少计数元数据：" + key);
    return std::stoll(statement.columnText(0));
}

std::int64_t SqliteChatRepositoryFanLX::nextLocalSequence() const {
    // 计数器必须显式持久化：按"最大已用序号 + 1"推导会在最后一条被清理后重复分配。
    return readCounterFanLX("next_local_sequence");
}

std::int64_t SqliteChatRepositoryFanLX::consumeDeviceSequenceFanLX() {
    const std::int64_t current = readCounterFanLX("next_local_sequence");
    if (current <= 0 || current == std::numeric_limits<std::int64_t>::max())
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage,
                                      "本地序号已耗尽或非法");
    sqliteFanLX::StatementFanLX update(connectionFanLX,
                                      "UPDATE schema_meta SET value = ?1 WHERE key = 'next_local_sequence'");
    sqliteFanLX::bindAllFanLX(update, std::to_string(current + 1));
    sqliteFanLX::runStatementFanLX(update);
    return current;
}

void SqliteChatRepositoryFanLX::ensureConversationFanLX(const std::string &key, ScopeFanLX scope,
                                                        const std::string &target) {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, "INSERT INTO conversations(conversation_key, scope, target) VALUES(?1, ?2, ?3)"
                         " ON CONFLICT(conversation_key) DO NOTHING");
    sqliteFanLX::bindAllFanLX(statement, key, scopeTextFanLX(scope), target);
    sqliteFanLX::runStatementFanLX(statement);
    sqliteFanLX::StatementFanLX cursor(
        connectionFanLX, "INSERT INTO sync_cursors(conversation_key, last_sequence, last_message_id,"
                         " read_sequence) VALUES(?1, 0, 0, 0) ON CONFLICT(conversation_key) DO NOTHING");
    sqliteFanLX::bindAllFanLX(cursor, key);
    sqliteFanLX::runStatementFanLX(cursor);
}

std::int64_t SqliteChatRepositoryFanLX::appendPending(const std::string &conversationKey, ScopeFanLX scope,
                                                      const std::string &target, const std::string &body,
                                                      const std::string &requestId, std::string &requestIdOut) {
    if (conversationKey.empty() || target.empty())
        throw std::invalid_argument("待发送记录缺少会话或目标");
    sqliteFanLX::TransactionFanLX transaction(connectionFanLX);

    // 幂等：同 requestId 同内容直接返回旧序号；内容不同必须拒绝，不能覆盖用户已输入的消息。
    if (!requestId.empty()) {
        sqliteFanLX::StatementFanLX duplicate(
            connectionFanLX, "SELECT device_sequence, conversation_key, body FROM messages"
                             " WHERE sender = ?1 AND request_id = ?2");
        sqliteFanLX::bindAllFanLX(duplicate, configFanLX.ownerAccount, requestId);
        if (duplicate.step()) {
            if (duplicate.columnText(1) != conversationKey || duplicate.columnText(2) != body)
                throw ChatConflictFanLX("同一 requestId 已存在且内容不同：" + requestId);
            // 幂等命中：回传既有幂等键（而不是新生成的），调用方可安全重放同一条消息。
            requestIdOut = requestId;
            return duplicate.columnInt64(0);
        }
        duplicate.reset();
    }

    const auto sequence = consumeDeviceSequenceFanLX();
    // requestId 与本次分配的本地序号绑定：同一设备内唯一且可稳定重放。
    const auto effectiveRequestId =
        requestId.empty() ? buildRequestIdFanLX(deviceIdFanLX, sequence) : requestId;
    ensureConversationFanLX(conversationKey, scope, target);
    sqliteFanLX::StatementFanLX insert(
        connectionFanLX, "INSERT INTO messages(device_sequence, conversation_key, message_id,"
                         " conversation_sequence, sender, recipient, request_id, body, accepted_at_utc,"
                         " state, received_at_unix, failure_reason)"
                         " VALUES(?1, ?2, NULL, 0, ?3, ?4, ?5, ?6, NULL, 'pending', ?7, '')");
    sqliteFanLX::bindAllFanLX(insert, sequence, conversationKey, configFanLX.ownerAccount, target,
                             effectiveRequestId, body, unixNowSecFanLX());
    sqliteFanLX::runStatementFanLX(insert);
    requestIdOut = effectiveRequestId;
    // 消息与计数器同事务提交：提交成功后才允许向用户确认"已保存待发送"。
    transaction.commit();
    return sequence;
}

void SqliteChatRepositoryFanLX::markAccepted(const std::string &requestId, std::int64_t messageId,
                                             std::int64_t conversationSequence,
                                             const std::string &acceptedAtUtc) {
    if (requestId.empty() || messageId <= 0 || conversationSequence <= 0)
        throw std::invalid_argument("SEND_ACK 参数非法");
    sqliteFanLX::TransactionFanLX transaction(connectionFanLX);

    // 先定位本地消息（会话键在 UPDATE 之后不再是 pending，无法再按原条件查回）。
    sqliteFanLX::StatementFanLX locate(
        connectionFanLX, "SELECT conversation_key, message_id, conversation_sequence, state FROM messages"
                         " WHERE sender = ?1 AND request_id = ?2");
    sqliteFanLX::bindAllFanLX(locate, configFanLX.ownerAccount, requestId);
    if (!locate.step())
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage,
                                      "SEND_ACK 对应的本地消息不存在：" + requestId);
    const auto conversationKey = locate.columnText(0);
    const bool alreadyAccepted = locate.columnText(3) == "sent_ack";
    if (alreadyAccepted) {
        // 允许重复 ACK（网络重放）：编号与序号一致视为成功，不一致必须报错。
        const bool same = !locate.columnIsNull(1) && locate.columnInt64(1) == messageId &&
                          locate.columnInt64(2) == conversationSequence;
        if (!same)
            throw ChatConflictFanLX("SEND_ACK 与本地记录不一致：" + requestId);
        return;
    }

    sqliteFanLX::StatementFanLX update(
        connectionFanLX, "UPDATE messages SET message_id = ?1, conversation_sequence = ?2,"
                         " accepted_at_utc = ?3, state = 'sent_ack', failure_reason = ''"
                         " WHERE sender = ?4 AND request_id = ?5");
    sqliteFanLX::bindAllFanLX(update, messageId, conversationSequence,
                             acceptedAtUtc.empty() ? utcNowFanLX() : acceptedAtUtc, configFanLX.ownerAccount,
                             requestId);
    sqliteFanLX::runStatementFanLX(update);
    if (connectionFanLX.changes() != 1)
        throw ChatConflictFanLX("SEND_ACK 未能更新本地记录：" + requestId);

    // 注意 last_sequence 的语义：它是"本会话见过的最大序号"，**不是**"已连续收到的序号"。
    // 自己发出的消息也会把它推到该消息自身的序号，因此它**不能**用来判定投递是否乱序
    // （两端共用一条单调计数器，序号空洞是正常现象，见 appendDelivered 的说明）。
    sqliteFanLX::StatementFanLX advance(
        connectionFanLX,
        "UPDATE sync_cursors SET"
        " last_sequence = CASE WHEN last_sequence < ?1 THEN ?1 ELSE last_sequence END,"
        " last_message_id = CASE WHEN last_message_id < ?2 THEN ?2 ELSE last_message_id END"
        " WHERE conversation_key = ?3");
    sqliteFanLX::bindAllFanLX(advance, conversationSequence, messageId, conversationKey);
    sqliteFanLX::runStatementFanLX(advance);
    transaction.commit();
}

void SqliteChatRepositoryFanLX::markFailed(const std::string &requestId, const std::string &reason) {
    if (requestId.empty())
        throw std::invalid_argument("requestId 不能为空");
    sqliteFanLX::TransactionFanLX transaction(connectionFanLX);
    // 失败消息保留在库中（state=failed）并记录原因：不静默丢弃输入，但也不再被 pending() 取出重发。
    sqliteFanLX::StatementFanLX update(
        connectionFanLX, "UPDATE messages SET state = 'failed', failure_reason = ?1"
                         " WHERE sender = ?2 AND request_id = ?3 AND state IN ('pending','failed')");
    sqliteFanLX::bindAllFanLX(update, reason, configFanLX.ownerAccount, requestId);
    sqliteFanLX::runStatementFanLX(update);
    transaction.commit();
}

bool SqliteChatRepositoryFanLX::appendDelivered(const MessageRowFanLX &row) {
    if (!row.messageId || *row.messageId <= 0 || row.conversationSequence <= 0 || row.sender.empty())
        throw std::invalid_argument("投递消息缺少服务器编号或会话序号");
    const auto scope = scopeOfKeyFanLX(row.conversationKey);
    const auto key = row.conversationKey.empty()
                         ? conversationKeyFanLX(scope, configFanLX.ownerAccount,
                                                peerOfRowFanLX(row, configFanLX.ownerAccount))
                         : row.conversationKey;

    std::optional<sqliteFanLX::TransactionFanLX> transaction;
    if (!bulkImportFanLX)
        transaction.emplace(connectionFanLX);

    // 重复投递必须先比对内容，禁止用 INSERT OR IGNORE 掩盖 messageId 内容冲突。
    sqliteFanLX::StatementFanLX existing(
        connectionFanLX, "SELECT conversation_key, conversation_sequence, sender, recipient, body"
                         " FROM messages WHERE message_id = ?1");
    sqliteFanLX::bindAllFanLX(existing, *row.messageId);
    if (existing.step()) {
        const bool same = existing.columnText(0) == key &&
                          existing.columnInt64(1) == row.conversationSequence &&
                          existing.columnText(2) == row.sender &&
                          existing.columnText(3) == row.recipient && existing.columnText(4) == row.body;
        if (!same)
            throw ChatConflictFanLX("messageId " + std::to_string(*row.messageId) +
                                    " 内容冲突：本地这条编号的内容与服务端投递不一致");
        return false; // 内容一致的重复投递不新增行
    }
    existing.reset();

    // 不再用"序号必须大于 sync_cursors.last_sequence"来拒绝投递。
    //
    // 为什么必须去掉：单聊序号是**两端共用的一条单调计数器**（conversationKeyFanLX 把双方账号排序，
    // 两个方向算出同一把键；服务端 conversations.next_sequence 按该键分配，且表上有
    // UNIQUE(conversation_key, conversation_sequence)）。而客户端的 last_sequence 不只由收到的
    // 消息推进——自己发出的消息经 markAccepted 也会把它推到**该消息自身的序号**。
    // 于是"自己这边已经到 15、对端更早的 8 还没投到"是完全正常的局面，老规则会永久拒绝那条 8：
    // 客户端不落库、也不回 ACK（投递游标只由 ACK 推进）→ 服务端每次 SYNC 都重投 → 10 秒未 ACK
    // 关会话重连 → 对端永远发不进来、自己却发得出去（单向故障）。单调水位无法表达"空洞"，
    // 因此它不能充当投递的准入条件。
    //
    // 现在只拒绝真正的冲突：同一会话序号已被**另一条**消息占用（重复投递已由上面的 messageId
    // 去重拦下，走到这里就说明 messageId 不同而序号相同）。晚到的空洞一律允许补齐——历史不应因为
    // "晚到"而丢消息。last_sequence 退化为"见过的最大序号"，仅供展示、排序与已读游标夹取使用。
    sqliteFanLX::StatementFanLX clash(
        connectionFanLX, "SELECT message_id FROM messages"
                         " WHERE conversation_key = ?1 AND conversation_sequence = ?2");
    sqliteFanLX::bindAllFanLX(clash, key, row.conversationSequence);
    if (clash.step())
        throw ChatConflictFanLX("会话序号 " + std::to_string(row.conversationSequence) +
                                " 已被另一条消息占用（会话 " + key + "）");

    ensureConversationFanLX(key, scope, row.recipient);

    // 自己发送的消息：把本地待发行升级为已确认，而不是插入第二行（历史不应出现重复）。
    bool promoted = false;
    if (!row.requestId.empty() && row.sender == configFanLX.ownerAccount) {
        sqliteFanLX::StatementFanLX promote(
            connectionFanLX, "UPDATE messages SET message_id = ?1, conversation_sequence = ?2,"
                             " accepted_at_utc = ?3, state = 'sent_ack', failure_reason = ''"
                             " WHERE sender = ?4 AND request_id = ?5"
                             " AND state IN ('pending','failed')");
        sqliteFanLX::bindAllFanLX(promote, *row.messageId, row.conversationSequence,
                                 row.acceptedAtUtc.empty() ? utcNowFanLX() : row.acceptedAtUtc,
                                 configFanLX.ownerAccount, row.requestId);
        sqliteFanLX::runStatementFanLX(promote);
        promoted = connectionFanLX.changes() == 1;
    }

    const auto sequence = consumeDeviceSequenceFanLX();
    if (!promoted) {
        sqliteFanLX::StatementFanLX insert(
            connectionFanLX, "INSERT INTO messages(device_sequence, conversation_key, message_id,"
                             " conversation_sequence, sender, recipient, request_id, body, accepted_at_utc,"
                             " state, received_at_unix, failure_reason)"
                             " VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)");
        sqliteFanLX::bindAllFanLX(insert, sequence, key, *row.messageId, row.conversationSequence, row.sender,
                                 row.recipient, row.requestId, row.body,
                                 row.acceptedAtUtc.empty() ? utcNowFanLX() : row.acceptedAtUtc,
                                 stateTextFanLX(row.state), unixNowSecFanLX(), row.failureReason);
        sqliteFanLX::runStatementFanLX(insert);
    }

    sqliteFanLX::StatementFanLX advance(
        connectionFanLX,
        "UPDATE sync_cursors SET"
        " last_sequence = CASE WHEN last_sequence < ?1 THEN ?1 ELSE last_sequence END,"
        " last_message_id = CASE WHEN last_message_id < ?2 THEN ?2 ELSE last_message_id END"
        " WHERE conversation_key = ?3");
    sqliteFanLX::bindAllFanLX(advance, row.conversationSequence, *row.messageId, key);
    sqliteFanLX::runStatementFanLX(advance);
    // 插入消息与推进游标同事务提交：成功后调用方才允许回 ACK。
    if (transaction)
        transaction->commit();
    return true;
}

std::vector<PendingMessageFanLX> SqliteChatRepositoryFanLX::pending() const {
    // 只取 'pending'：'failed' 是"已经确定发不出去"的终态（服务器拒绝 / 无法组帧），
    // 若把它也算作待发送，每次重连都会重放这些消息，形成无休止的重发与报错。
    // 失败原因随消息保存在库里（见 markFailed），在消息列表可见，不再自动重发。
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX,
        "SELECT m.request_id, m.device_sequence, c.scope, c.conversation_key, m.body, m.failure_reason"
        " FROM messages AS m JOIN conversations AS c ON c.conversation_key = m.conversation_key"
        " WHERE m.state = 'pending' ORDER BY m.device_sequence");
    std::vector<PendingMessageFanLX> rows;
    while (statement.step()) {
        PendingMessageFanLX row;
        row.requestId = statement.columnText(0);
        row.localSequence = statement.columnInt64(1);
        row.scope = scopeFromTextFanLX(statement.columnText(2));
        row.conversationKey = statement.columnText(3);
        row.body = statement.columnText(4);
        row.failureReason = statement.columnText(5);
        // 收件人必须由会话键按方向推导：会话目录里的 target 只是建会话时写入的字段，
        // 由对端先建会话时它等于对端而不是本端要发的对象。群聊的对端就是群号。
        if (row.scope == ScopeFanLX::Group) {
            row.target = row.conversationKey.size() > 2 ? row.conversationKey.substr(2) : std::string();
        } else {
            const auto first = row.conversationKey.find('|');
            const auto second = first == std::string::npos
                                    ? std::string::npos
                                    : row.conversationKey.find('|', first + 1);
            if (first == std::string::npos || second == std::string::npos)
                throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage,
                                              "会话键格式非法：" + row.conversationKey);
            const auto left = row.conversationKey.substr(first + 1, second - first - 1);
            const auto right = row.conversationKey.substr(second + 1);
            row.target = left == configFanLX.ownerAccount ? right : left;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

std::int64_t SqliteChatRepositoryFanLX::lastConversationSequence(const std::string &conversationKey) const {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, "SELECT last_sequence FROM sync_cursors WHERE conversation_key = ?1");
    sqliteFanLX::bindAllFanLX(statement, conversationKey);
    return statement.step() ? statement.columnInt64(0) : 0;
}

void SqliteChatRepositoryFanLX::markRead(const std::string &conversationKey, std::int64_t sequence) {
    if (conversationKey.empty() || sequence < 0)
        throw std::invalid_argument("已读游标参数非法");
    // 已读位置只允许单调前进，避免旧界面回调把未读数改回增大。
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, "UPDATE sync_cursors SET read_sequence = ?1"
                         " WHERE conversation_key = ?2 AND read_sequence < ?1");
    sqliteFanLX::bindAllFanLX(statement, sequence, conversationKey);
    sqliteFanLX::runStatementFanLX(statement);
}

std::int64_t SqliteChatRepositoryFanLX::lastReadSequence(const std::string &conversationKey) const {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, "SELECT read_sequence FROM sync_cursors WHERE conversation_key = ?1");
    sqliteFanLX::bindAllFanLX(statement, conversationKey);
    return statement.step() ? statement.columnInt64(0) : 0;
}

HistoryPageFanLX SqliteChatRepositoryFanLX::queryHistory(const HistoryQueryFanLX &query) const {
    // 两种模式：
    // ① 指定会话键：按会话序号 keyset 分页；
    // ② 会话键为空但给出发送者/关键词：跨会话检索（界面"全局搜索"），按服务器时间再按会话键排序。
    const bool crossConversation = query.conversationKey.empty();
    if (crossConversation && query.sender.empty() && query.keyword.empty() &&
        query.fromAcceptedAtUtc.empty() && query.toAcceptedAtUtc.empty())
        throw std::invalid_argument("历史查询必须指定会话，或给出发送者/关键词做跨会话检索");
    if (crossConversation && query.beforeSequence > 0)
        throw std::invalid_argument("跨会话检索不支持会话序号游标，请改用时间游标");
    const int limit = static_cast<int>(std::clamp<std::int64_t>(query.limit, 1, maxPageSizeFanLX));

    // 恒真占位写法：每个 ?n 都出现在 SQL 文本里，参数序号与绑定顺序天然一致
    // （曾经按条件拼接分支导致 ?2..?7 错位，出现"按发送者过滤查不到历史"的静默错误）。
    std::string sql = "SELECT ";
    sql += messageColumnsFanLX;
    sql += " FROM messages WHERE (?1 = '' OR conversation_key = ?1)";
    sql += " AND (?2 = '' OR sender = ?2)";
    sql += " AND (?3 = '' OR fanlx_fold(body) LIKE fanlx_fold(?3) ESCAPE '\\')";
    sql += " AND (?4 = 0 OR conversation_sequence < ?4)";
    sql += " AND (?7 = '' OR accepted_at_utc >= ?7) AND (?8 = '' OR accepted_at_utc <= ?8)";
    if (crossConversation)
        sql += " AND ((?9 = 0 AND (?5 = '' OR accepted_at_utc < ?5)) OR"
               " (?9 > 0 AND (COALESCE(accepted_at_utc, '') < ?5 OR"
               " (COALESCE(accepted_at_utc, '') = ?5 AND device_sequence < ?9))))";
    else
        sql += " AND (?5 = '' OR accepted_at_utc < ?5) AND ?9 >= 0";
    if (crossConversation)
        sql += " ORDER BY COALESCE(accepted_at_utc, '') DESC, device_sequence DESC";
    else
        sql += query.ascending ? " ORDER BY conversation_sequence ASC, device_sequence ASC"
                               : " ORDER BY conversation_sequence DESC, device_sequence DESC";
    sql += " LIMIT ?6";

    sqliteFanLX::StatementFanLX statement(connectionFanLX, sql);
    sqliteFanLX::bindAllFanLX(statement, query.conversationKey, query.sender,
                             query.keyword.empty() ? std::string()
                                                   : "%" + escapeLikeFanLX(query.keyword) + "%",
                             query.beforeSequence, query.beforeAcceptedAtUtc,
                             static_cast<std::int64_t>(limit + 1), query.fromAcceptedAtUtc,
                             query.toAcceptedAtUtc, query.beforeLocalSequence);

    HistoryPageFanLX page;
    while (statement.step())
        page.messages.push_back(readMessageRowFanLX(statement));
    if (static_cast<int>(page.messages.size()) > limit) {
        page.messages.pop_back();
        page.hasMore = true;
    }
    if (!page.messages.empty()) {
        // keyset 游标：直接使用本页边界，避免 OFFSET 在持续写入时跳页或漏页。
        page.nextBeforeSequence = page.messages.back().conversationSequence;
        page.nextBeforeAcceptedAtUtc = page.messages.back().acceptedAtUtc;
        page.nextBeforeLocalSequence = page.messages.back().localSequence;
    } else if (query.beforeSequence > 0) {
        page.nextBeforeSequence = query.beforeSequence;
    }
    return page;
}

std::vector<ConversationRowFanLX> SqliteChatRepositoryFanLX::conversations() const {
    // 未读数单独聚合，避免与外层 LEFT JOIN 混用时列序号错位。
    std::map<std::string, std::int64_t> unread;
    {
        sqliteFanLX::StatementFanLX count(
            connectionFanLX,
            "SELECT m.conversation_key, COUNT(*) FROM messages AS m"
            " JOIN sync_cursors AS s ON s.conversation_key = m.conversation_key"
            " WHERE m.conversation_sequence > s.read_sequence AND m.sender <> ?1"
            " GROUP BY m.conversation_key");
        sqliteFanLX::bindAllFanLX(count, configFanLX.ownerAccount);
        while (count.step())
            unread[count.columnText(0)] = count.columnInt64(1);
    }

    sqliteFanLX::StatementFanLX statement(
        connectionFanLX,
        "SELECT c.conversation_key, c.scope, s.last_sequence, s.read_sequence,"
        "       COALESCE(("
        "         SELECT m.body FROM messages AS m WHERE m.conversation_key = c.conversation_key"
        "           AND m.conversation_sequence = s.last_sequence AND s.last_sequence > 0"
        "         ORDER BY m.device_sequence DESC LIMIT 1), ''),"
        "       COALESCE(("
        "         SELECT m.sender FROM messages AS m WHERE m.conversation_key = c.conversation_key"
        "           AND m.conversation_sequence = s.last_sequence AND s.last_sequence > 0"
        "         ORDER BY m.device_sequence DESC LIMIT 1), '')"
        "  FROM conversations AS c"
        "  JOIN sync_cursors AS s ON s.conversation_key = c.conversation_key"
        " ORDER BY s.last_sequence DESC, c.conversation_key ASC");
    std::vector<ConversationRowFanLX> rows;
    while (statement.step()) {
        ConversationRowFanLX row;
        row.conversationKey = statement.columnText(0);
        row.scope = scopeFromTextFanLX(statement.columnText(1));
        row.lastConversationSequence = statement.columnInt64(2);
        row.lastReadSequence = statement.columnInt64(3);
        row.lastMessageBody = statement.columnText(4);
        row.lastSender = statement.columnText(5);
        const auto found = unread.find(row.conversationKey);
        row.unreadCount = found == unread.end() ? 0 : found->second;
        rows.push_back(std::move(row));
    }
    return rows;
}

void SqliteChatRepositoryFanLX::forgetConversations(const std::vector<std::string> &keepKeys) {
    // 只清理本地目录：服务器仍是唯一权威，客户端不"发明"会话。
    std::vector<std::string> existing;
    {
        sqliteFanLX::StatementFanLX statement(connectionFanLX, "SELECT conversation_key FROM conversations");
        while (statement.step())
            existing.push_back(statement.columnText(0));
    }
    const std::set<std::string> keep(keepKeys.begin(), keepKeys.end());
    std::vector<std::string> drop;
    for (const auto &key : existing)
        if (!keep.count(key))
            drop.push_back(key);
    if (drop.empty())
        return;
    sqliteFanLX::TransactionFanLX transaction(connectionFanLX);
    for (const auto &key : drop) {
        sqliteFanLX::StatementFanLX cursor(connectionFanLX,
                                          "DELETE FROM sync_cursors WHERE conversation_key = ?1");
        sqliteFanLX::bindAllFanLX(cursor, key);
        sqliteFanLX::runStatementFanLX(cursor);
        sqliteFanLX::StatementFanLX messages(connectionFanLX,
                                            "DELETE FROM messages WHERE conversation_key = ?1");
        sqliteFanLX::bindAllFanLX(messages, key);
        sqliteFanLX::runStatementFanLX(messages);
        sqliteFanLX::StatementFanLX conversation(connectionFanLX,
                                                "DELETE FROM conversations WHERE conversation_key = ?1");
        sqliteFanLX::bindAllFanLX(conversation, key);
        sqliteFanLX::runStatementFanLX(conversation);
    }
    transaction.commit();
}

std::int64_t SqliteChatRepositoryFanLX::messageCount() const {
    sqliteFanLX::StatementFanLX statement(connectionFanLX, "SELECT COUNT(*) FROM messages");
    return statement.step() ? statement.columnInt64(0) : 0;
}

void SqliteChatRepositoryFanLX::importLegacyMessage(const MessageRowFanLX &row) {
    // 迁移路径：按旧存储内容原样导入（允许乱序），游标最后由 rebuildSyncCursorsFanLX 重算。
    const auto scope = scopeOfKeyFanLX(row.conversationKey);
    const auto key = row.conversationKey.empty()
                         ? conversationKeyFanLX(scope, configFanLX.ownerAccount,
                                                peerOfRowFanLX(row, configFanLX.ownerAccount))
                         : row.conversationKey;
    ensureConversationFanLX(key, scope, row.recipient);
    const auto sequence = consumeDeviceSequenceFanLX();
    sqliteFanLX::StatementFanLX insert(
        connectionFanLX, "INSERT INTO messages(device_sequence, conversation_key, message_id,"
                         " conversation_sequence, sender, recipient, request_id, body, accepted_at_utc,"
                         " state, received_at_unix, failure_reason)"
                         " VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)");
    insert.bind(1, sequence);
    insert.bind(2, key);
    // 旧记录没有服务器编号时写 NULL：不占用编号空间，也不伪装成已确认。
    if (row.messageId && *row.messageId > 0)
        insert.bind(3, *row.messageId);
    else
        insert.bindNull(3);
    insert.bind(4, row.conversationSequence);
    insert.bind(5, row.sender);
    insert.bind(6, row.recipient);
    insert.bind(7, row.requestId);
    insert.bind(8, row.body);
    // 旧 JSON 没有可信服务器时间：留空而不是伪造成迁移时刻。
    if (row.acceptedAtUtc.empty())
        insert.bindNull(9);
    else
        insert.bind(9, row.acceptedAtUtc);
    insert.bind(10, stateTextFanLX(row.state));
    insert.bind(11, unixNowSecFanLX());
    insert.bind(12, row.failureReason);
    sqliteFanLX::runStatementFanLX(insert);
}

void SqliteChatRepositoryFanLX::rebuildSyncCursorsFanLX() {
    connectionFanLX.execute(
        "UPDATE sync_cursors SET"
        " last_sequence = COALESCE((SELECT MAX(m.conversation_sequence) FROM messages AS m"
        "   WHERE m.conversation_key = sync_cursors.conversation_key), 0),"
        " last_message_id = COALESCE((SELECT MAX(m.message_id) FROM messages AS m"
        "   WHERE m.conversation_key = sync_cursors.conversation_key), 0)");
    connectionFanLX.execute("UPDATE sync_cursors SET read_sequence = 0 WHERE read_sequence > last_sequence");
}

std::string SqliteChatRepositoryFanLX::explainPlanFanLX(const HistoryQueryFanLX &query) const {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX,
        "EXPLAIN QUERY PLAN SELECT message_id FROM messages WHERE conversation_key = ?1"
        " AND conversation_sequence < ?2 ORDER BY conversation_sequence DESC, device_sequence DESC LIMIT ?3");
    sqliteFanLX::bindAllFanLX(statement, query.conversationKey, query.beforeSequence,
                             static_cast<std::int64_t>(query.limit));
    std::string plan;
    while (statement.step()) {
        if (!plan.empty()) plan += " | ";
        plan += statement.columnText(3);
    }
    return plan;
}

void SqliteChatRepositoryFanLX::beginBulkImportFanLX() {
    if (bulkImportFanLX)
        throw std::logic_error("批量导入已经开始");
    connectionFanLX.begin();
    bulkImportFanLX = true;
}

void SqliteChatRepositoryFanLX::endBulkImportFanLX() {
    if (!bulkImportFanLX)
        throw std::logic_error("批量导入尚未开始");
    bulkImportFanLX = false;
    connectionFanLX.commit();
}

} // namespace chatDbFanLX

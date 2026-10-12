#include "db/SqliteMessageStoreFanLX.h"
#include "db/ChatSchemaStoreFanLX.h"
#include "db/SqliteChatRepositoryFanLX.h"
#include <algorithm>
#include <limits>
#include <sqlite3.h>

namespace chatDbFanLX {

namespace {

constexpr const char *serverMessageColumnsFanLX =
    "message_id, conversation_key, conversation_sequence, sender, recipient, request_id, body,"
    " accepted_at_utc";

MessageRowFanLX readServerMessageFanLX(sqliteFanLX::StatementFanLX &statement) {
    MessageRowFanLX row;
    row.messageId = statement.columnInt64(0);
    row.conversationKey = statement.columnText(1);
    row.conversationSequence = statement.columnInt64(2);
    row.sender = statement.columnText(3);
    row.recipient = statement.columnText(4);
    row.requestId = statement.columnText(5);
    row.body = statement.columnText(6);
    row.acceptedAtUtc = statement.columnText(7);
    row.state = MessageStateFanLX::SentAck;
    return row;
}

std::int64_t readCounterFanLX(sqliteFanLX::ConnectionFanLX &connection, const std::string &key) {
    sqliteFanLX::StatementFanLX statement(connection, "SELECT value FROM schema_meta WHERE key = ?1");
    sqliteFanLX::bindAllFanLX(statement, key);
    if (!statement.step())
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "缺少计数元数据：" + key);
    return std::stoll(statement.columnText(0));
}

void writeCounterFanLX(sqliteFanLX::ConnectionFanLX &connection, const std::string &key,
                       std::int64_t value) {
    sqliteFanLX::StatementFanLX statement(connection, "UPDATE schema_meta SET value = ?1 WHERE key = ?2");
    sqliteFanLX::bindAllFanLX(statement, std::to_string(value), key);
    sqliteFanLX::runStatementFanLX(statement);
    if (connection.changes() != 1)
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "计数更新失败：" + key);
}

// 分配会话序号：序号耗尽必须拒绝，不能回绕。
std::int64_t allocateConversationSequenceFanLX(sqliteFanLX::ConnectionFanLX &connection,
                                               const std::string &key, ScopeFanLX scope,
                                               const std::string &target) {
    sqliteFanLX::StatementFanLX insert(
        connection, "INSERT INTO conversations(conversation_key, scope, target, next_sequence)"
                    " VALUES(?1, ?2, ?3, 0) ON CONFLICT(conversation_key) DO NOTHING");
    sqliteFanLX::bindAllFanLX(insert, key, scopeTextFanLX(scope), target);
    sqliteFanLX::runStatementFanLX(insert);

    sqliteFanLX::StatementFanLX read(connection,
                                     "SELECT next_sequence FROM conversations WHERE conversation_key = ?1");
    sqliteFanLX::bindAllFanLX(read, key);
    if (!read.step())
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "会话目录缺失：" + key);
    const std::int64_t current = read.columnInt64(0);
    if (current < 0 || current == std::numeric_limits<std::int64_t>::max())
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "会话序号耗尽：" + key);
    read.reset();

    sqliteFanLX::StatementFanLX update(
        connection, "UPDATE conversations SET next_sequence = ?1 WHERE conversation_key = ?2");
    sqliteFanLX::bindAllFanLX(update, current + 1, key);
    sqliteFanLX::runStatementFanLX(update);
    return current + 1;
}

void insertRecipientsFanLX(sqliteFanLX::ConnectionFanLX &connection, std::int64_t messageId,
                           const std::vector<std::string> &recipients) {
    sqliteFanLX::StatementFanLX statement(
        connection, "INSERT INTO recipients(message_id, account) VALUES(?1, ?2)"
                    " ON CONFLICT(message_id, account) DO NOTHING");
    for (const auto &account : recipients) {
        if (account.empty())
            continue;
        statement.bind(1, messageId);
        statement.bind(2, account);
        sqliteFanLX::runStatementFanLX(statement);
        statement.reset();
    }
}

} // namespace

SqliteMessageStoreFanLX::SqliteMessageStoreFanLX(const std::filesystem::path &databasePath,
                                                 const std::string &serverInstanceId)
    : instanceIdFanLX(serverInstanceId), connectionFanLX(databasePath) {
    if (databasePath.empty() || serverInstanceId.empty())
        throw std::invalid_argument("服务端消息库配置缺失");
    applyServerSchemaFanLX(connectionFanLX, serverInstanceId);
}

SqliteMessageStoreFanLX::~SqliteMessageStoreFanLX() = default;

ServerAcceptResultFanLX SqliteMessageStoreFanLX::accept(const ServerAcceptRequestFanLX &request) {
    if (request.sender.empty() || request.target.empty() || request.requestId.empty())
        throw std::invalid_argument("服务端接收请求缺少发送者、目标或 requestId");
    if (request.recipients.empty())
        throw std::invalid_argument("服务端接收请求缺少收件人快照");

    const auto key = conversationKeyFanLX(request.scope, request.sender, request.target);
    if (const auto existing = findByIdempotencyKey(request.sender, request.requestId)) {
        const auto &saved = existing->message;
        if (saved.conversationKey != key || saved.body != request.body || saved.recipient != request.target)
            throw ChatConflictFanLX("同一 requestId 不允许修改消息内容：" + request.requestId);
        return *existing; // 重放：返回旧结果，不新增行、不消耗序号
    }

    sqliteFanLX::TransactionFanLX transaction(connectionFanLX);
    const std::int64_t messageId = readCounterFanLX(connectionFanLX, "next_message_id");
    if (messageId <= 0 || messageId == std::numeric_limits<std::int64_t>::max())
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "消息编号耗尽或非法");
    const std::int64_t sequence =
        allocateConversationSequenceFanLX(connectionFanLX, key, request.scope, request.target);
    const std::string acceptedAt = utcNowFanLX();

    sqliteFanLX::StatementFanLX insert(
        connectionFanLX, "INSERT INTO messages(message_id, conversation_key, conversation_sequence, sender,"
                         " recipient, request_id, body, accepted_at_utc)"
                         " VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
    sqliteFanLX::bindAllFanLX(insert, messageId, key, sequence, request.sender, request.target,
                             request.requestId, request.body, acceptedAt);
    sqliteFanLX::runStatementFanLX(insert);
    insertRecipientsFanLX(connectionFanLX, messageId, request.recipients);
    // 编号最后推进：失败即整体回滚，不会留下"已用编号但无消息"的空洞。
    writeCounterFanLX(connectionFanLX, "next_message_id", messageId + 1);
    transaction.commit();

    ServerAcceptResultFanLX result;
    result.message.messageId = messageId;
    result.message.conversationKey = key;
    result.message.conversationSequence = sequence;
    result.message.sender = request.sender;
    result.message.recipient = request.target;
    result.message.requestId = request.requestId;
    result.message.body = request.body;
    result.message.acceptedAtUtc = acceptedAt;
    result.message.state = MessageStateFanLX::SentAck;
    return result;
}

std::optional<ServerAcceptResultFanLX> SqliteMessageStoreFanLX::findByIdempotencyKey(
    const std::string &sender, const std::string &requestId) const {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, std::string("SELECT ") + serverMessageColumnsFanLX +
                             " FROM messages WHERE sender = ?1 AND request_id = ?2");
    sqliteFanLX::bindAllFanLX(statement, sender, requestId);
    if (!statement.step())
        return std::nullopt;
    ServerAcceptResultFanLX result;
    result.message = readServerMessageFanLX(statement);
    result.replayed = true;
    return result;
}

std::optional<MessageRowFanLX> SqliteMessageStoreFanLX::findById(std::int64_t messageId) const {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, std::string("SELECT ") + serverMessageColumnsFanLX +
                             " FROM messages WHERE message_id = ?1");
    sqliteFanLX::bindAllFanLX(statement, messageId);
    if (!statement.step())
        return std::nullopt;
    return readServerMessageFanLX(statement);
}

std::vector<ServerDeliveryCandidateFanLX> SqliteMessageStoreFanLX::pendingFor(
    const std::string &account, const DeliveryCursorMapFanLX &cursors, std::size_t limit) const {
    // 按 messageId 正序扫描（保证先投递早消息），会话游标在内存中比对，避免按会话多次查询。
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, std::string("SELECT m.message_id, m.conversation_key, m.conversation_sequence,") +
                             " m.sender, m.recipient, m.request_id, m.body, m.accepted_at_utc"
                             " FROM messages AS m JOIN recipients AS r ON r.message_id = m.message_id"
                             " WHERE r.account = ?1 ORDER BY m.message_id ASC LIMIT ?2");
    sqliteFanLX::bindAllFanLX(statement, account, static_cast<std::int64_t>(std::max<std::size_t>(limit, 1)));

    std::vector<ServerDeliveryCandidateFanLX> rows;
    while (statement.step()) {
        ServerDeliveryCandidateFanLX candidate;
        candidate.message = readServerMessageFanLX(statement);
        candidate.conversationKey = candidate.message.conversationKey;
        const auto found = cursors.find(candidate.conversationKey);
        const std::int64_t acked = found == cursors.end() ? 0 : found->second;
        if (candidate.message.conversationSequence <= acked)
            continue;
        rows.push_back(std::move(candidate));
    }
    return rows;
}

DeliveryCursorMapFanLX SqliteMessageStoreFanLX::deliveryCursorsFor(const std::string &account) const {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, "SELECT conversation_key, last_ack_sequence FROM delivery_cursors"
                         " WHERE account = ?1");
    sqliteFanLX::bindAllFanLX(statement, account);
    DeliveryCursorMapFanLX cursors;
    while (statement.step())
        cursors[statement.columnText(0)] = statement.columnInt64(1);
    return cursors;
}

void SqliteMessageStoreFanLX::advanceDeliveryCursor(const std::string &account,
                                                    const std::string &conversationKey,
                                                    std::int64_t sequence) {
    if (account.empty() || conversationKey.empty() || sequence <= 0)
        throw std::invalid_argument("投递游标参数非法");
    // 只允许单调前进：乱序或重放 ACK 不得把游标改回小值。
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, "INSERT INTO delivery_cursors(account, conversation_key, last_ack_sequence)"
                         " VALUES(?1, ?2, ?3)"
                         " ON CONFLICT(account, conversation_key) DO UPDATE SET"
                         " last_ack_sequence = MAX(delivery_cursors.last_ack_sequence,"
                         " excluded.last_ack_sequence)");
    sqliteFanLX::bindAllFanLX(statement, account, conversationKey, sequence);
    sqliteFanLX::runStatementFanLX(statement);
}

std::int64_t SqliteMessageStoreFanLX::conversationSequenceOnServer(
    const std::string &conversationKey) const {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, "SELECT next_sequence FROM conversations WHERE conversation_key = ?1");
    sqliteFanLX::bindAllFanLX(statement, conversationKey);
    return statement.step() ? statement.columnInt64(0) : 0;
}

std::int64_t SqliteMessageStoreFanLX::nextMessageId() const {
    sqliteFanLX::StatementFanLX statement(
        connectionFanLX, "SELECT value FROM schema_meta WHERE key = 'next_message_id'");
    if (!statement.step())
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage, "缺少消息编号元数据");
    return std::stoll(statement.columnText(0));
}

std::int64_t SqliteMessageStoreFanLX::messageCount() const {
    sqliteFanLX::StatementFanLX statement(connectionFanLX, "SELECT COUNT(*) FROM messages");
    return statement.step() ? statement.columnInt64(0) : 0;
}

std::string SqliteMessageStoreFanLX::integrityCheck() const {
    sqliteFanLX::StatementFanLX statement(connectionFanLX, "PRAGMA integrity_check");
    return statement.step() ? statement.columnText(0) : std::string("(空结果)");
}

void SqliteMessageStoreFanLX::importLegacyMessage(const ServerAcceptRequestFanLX &request,
                                                  std::int64_t messageId,
                                                  std::int64_t conversationSequence,
                                                  const std::vector<std::string> &recipients) {
    if (messageId <= 0 || conversationSequence <= 0)
        throw ChatMigrationErrorFanLX("旧服务端消息编号或会话序号非法，拒绝迁移");
    const auto key = conversationKeyFanLX(request.scope, request.sender, request.target);
    // 会话目录必须先存在（messages.conversation_key 有外键约束）。
    allocateConversationSequenceFanLX(connectionFanLX, key, request.scope, request.target);
    // 把会话计数抬到不小于已导入序号，避免后续 accept 与旧序号撞唯一键。
    sqliteFanLX::StatementFanLX bump(connectionFanLX,
                                    "UPDATE conversations SET next_sequence = ?1"
                                    " WHERE conversation_key = ?2 AND next_sequence < ?1");
    sqliteFanLX::bindAllFanLX(bump, conversationSequence, key);
    sqliteFanLX::runStatementFanLX(bump);

    sqliteFanLX::StatementFanLX insert(
        connectionFanLX, "INSERT INTO messages(message_id, conversation_key, conversation_sequence, sender,"
                         " recipient, request_id, body, accepted_at_utc)"
                         " VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
    sqliteFanLX::bindAllFanLX(insert, messageId, key, conversationSequence, request.sender, request.target,
                             request.requestId, request.body, utcNowFanLX());
    sqliteFanLX::runStatementFanLX(insert);
    insertRecipientsFanLX(connectionFanLX, messageId, recipients);
}

void SqliteMessageStoreFanLX::raiseMessageCounterFanLX(std::int64_t nextMessageId) {
    if (nextMessageId <= 0)
        throw std::invalid_argument("消息计数必须为正数");
    const std::int64_t current = readCounterFanLX(connectionFanLX, "next_message_id");
    if (nextMessageId <= current)
        return; // 只允许抬高：回退会造成编号复用
    sqliteFanLX::TransactionFanLX transaction(connectionFanLX);
    writeCounterFanLX(connectionFanLX, "next_message_id", nextMessageId);
    transaction.commit();
}

} // namespace chatDbFanLX

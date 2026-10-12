#include "db/ChatSchemaFanLX.h"

namespace chatDbFanLX {

const std::vector<std::string> &clientSchemaStatementsFanLX() {
    static const std::vector<std::string> statements{
        "CREATE TABLE IF NOT EXISTS schema_meta("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");",
        // 会话目录：保存 scope/target，便于"只显示仍属授权会话"的清理与界面展示。
        "CREATE TABLE IF NOT EXISTS conversations("
        "  conversation_key TEXT PRIMARY KEY,"
        "  scope TEXT NOT NULL CHECK(scope IN ('direct','group')),"
        "  target TEXT NOT NULL"
        ");",
        // 消息表要点：
        // - device_sequence 是主键：本地单调序号，本地待发送消息由它定序；
        // - message_id 可空：NULL 表示服务器尚未确认（不能用 INTEGER PRIMARY KEY，
        //   否则 SQLite 会把显式 NULL 换成 rowid，"待发送"就会伪装成已有服务器编号）；
        // - conversation_sequence 为服务器分配的会话内序号，待发送消息为 0。
        "CREATE TABLE IF NOT EXISTS messages("
        "  device_sequence INTEGER PRIMARY KEY CHECK(device_sequence > 0),"
        "  conversation_key TEXT NOT NULL REFERENCES conversations(conversation_key),"
        "  message_id INTEGER CHECK(message_id IS NULL OR message_id > 0),"
        "  conversation_sequence INTEGER NOT NULL DEFAULT 0 CHECK(conversation_sequence >= 0),"
        "  sender TEXT NOT NULL,"
        "  recipient TEXT NOT NULL,"
        "  request_id TEXT NOT NULL DEFAULT '',"
        "  body TEXT NOT NULL,"
        "  accepted_at_utc TEXT,"
        "  state TEXT NOT NULL CHECK(state IN ('pending','sent_ack','received','failed')),"
        "  received_at_unix INTEGER NOT NULL CHECK(received_at_unix >= 0),"
        "  failure_reason TEXT NOT NULL DEFAULT ''"
        ");",
        // 服务器编号唯一（仅对已确认消息生效）；重复投递必须命中这一约束而不是新增行。
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_messages_server_id"
        "  ON messages(message_id) WHERE message_id IS NOT NULL;",
        // 幂等键按发送者隔离：对端投递不携带 requestId，不能与本人发出的键冲突。
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_messages_sender_request"
        "  ON messages(sender, request_id) WHERE request_id <> '';",
        // 会话时间线索引：keyset 分页（会话 + 服务器序号）走这条覆盖索引。
        "CREATE INDEX IF NOT EXISTS idx_messages_timeline"
        "  ON messages(conversation_key, conversation_sequence, device_sequence);",
        // 跨会话检索（全局搜索）按服务器时间排序。
        "CREATE INDEX IF NOT EXISTS idx_messages_accepted_at ON messages(accepted_at_utc, conversation_key);",
        "CREATE INDEX IF NOT EXISTS idx_messages_state ON messages(state, device_sequence);",
        "CREATE TABLE IF NOT EXISTS sync_cursors("
        "  conversation_key TEXT PRIMARY KEY REFERENCES conversations(conversation_key),"
        "  last_sequence INTEGER NOT NULL DEFAULT 0 CHECK(last_sequence >= 0),"
        "  last_message_id INTEGER NOT NULL DEFAULT 0 CHECK(last_message_id >= 0),"
        "  read_sequence INTEGER NOT NULL DEFAULT 0 CHECK(read_sequence >= 0)"
        ");",
    };
    return statements;
}

const std::vector<std::string> &serverSchemaStatementsFanLX() {
    static const std::vector<std::string> statements{
        "CREATE TABLE IF NOT EXISTS schema_meta("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");",
        "CREATE TABLE IF NOT EXISTS conversations("
        "  conversation_key TEXT PRIMARY KEY,"
        "  scope TEXT NOT NULL CHECK(scope IN ('direct','group')),"
        "  target TEXT NOT NULL,"
        "  next_sequence INTEGER NOT NULL DEFAULT 0 CHECK(next_sequence >= 0)"
        ");",
        // 服务端消息：(sender, request_id) 唯一是幂等键，重复发送必须返回旧结果。
        "CREATE TABLE IF NOT EXISTS messages("
        "  message_id INTEGER PRIMARY KEY CHECK(message_id > 0),"
        "  conversation_key TEXT NOT NULL REFERENCES conversations(conversation_key),"
        "  conversation_sequence INTEGER NOT NULL CHECK(conversation_sequence > 0),"
        "  sender TEXT NOT NULL,"
        "  recipient TEXT NOT NULL,"
        "  request_id TEXT NOT NULL,"
        "  body TEXT NOT NULL,"
        "  accepted_at_utc TEXT NOT NULL,"
        "  UNIQUE(sender, request_id),"
        "  UNIQUE(conversation_key, conversation_sequence)"
        ");",
        "CREATE INDEX IF NOT EXISTS idx_server_timeline"
        "  ON messages(conversation_key, conversation_sequence);",
        // 收件人快照：发送时的可见范围，不随后续成员变动改写历史。
        "CREATE TABLE IF NOT EXISTS recipients("
        "  message_id INTEGER NOT NULL REFERENCES messages(message_id) ON DELETE CASCADE,"
        "  account TEXT NOT NULL,"
        "  PRIMARY KEY(message_id, account)"
        ");",
        "CREATE INDEX IF NOT EXISTS idx_recipients_account ON recipients(account, message_id);",
        // 投递游标：只由收到 ACK 推进；未 ACK 的消息必须重放。
        "CREATE TABLE IF NOT EXISTS delivery_cursors("
        "  account TEXT NOT NULL,"
        "  conversation_key TEXT NOT NULL,"
        "  last_ack_sequence INTEGER NOT NULL DEFAULT 0 CHECK(last_ack_sequence >= 0),"
        "  PRIMARY KEY(account, conversation_key)"
        ");",
    };
    return statements;
}

} // namespace chatDbFanLX

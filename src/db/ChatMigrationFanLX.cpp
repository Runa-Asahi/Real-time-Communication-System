#include "db/ChatMigrationFanLX.h"
#include "db/ChatSchemaFanLX.h"
#include "db/ChatSchemaStoreFanLX.h"
#include "db/SqliteChatRepositoryFanLX.h"
#include "db/SqliteMessageStoreFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <algorithm>
#include <ctime>
#include <set>

namespace chatDbFanLX {

namespace {

namespace fs = std::filesystem;

// 旧 JSON 服务端只服务 QQ 一条通道，而新版服务端内部一律用"服务名|号码/群号"作标识
// （QQ 与微博按课程约定共用号码，必须按通道分开）。迁移时统一补上 QQ 前缀，
// 迁移后的数据与实时写入的数据才落在同一套键上。
inline constexpr const char *legacyChannelPrefixFanLX = "QQ|";

std::string qualifyAccountFanLX(const std::string &account) {
    return account.empty() ? account : legacyChannelPrefixFanLX + account;
}

// 旧会话键 → 新版会话键：
//   旧 "d|2026001|2026002" → 新 "d|QQ|2026001|QQ|2026002"（会话键两端都带通道前缀）
//   旧 "g|1001"            → 新 "g|QQ|1001"
std::string qualifyConversationKeyFanLX(const std::string &legacyKey) {
    if (legacyKey.rfind("g|", 0) == 0)
        return "g|" + qualifyAccountFanLX(legacyKey.substr(2));
    if (legacyKey.rfind("d|", 0) == 0) {
        const auto rest = legacyKey.substr(2);
        const auto bar = rest.find('|');
        if (bar == std::string::npos)
            return legacyKey;
        return "d|" + qualifyAccountFanLX(rest.substr(0, bar)) + "|" +
               qualifyAccountFanLX(rest.substr(bar + 1));
    }
    return legacyKey;
}

// 迁移标记文件：与新库同目录、同名派生，只有全部校验通过后才写入。
fs::path markerPathFanLX(const fs::path &databasePath) {
    return databasePath.parent_path() / (databasePath.filename().string() + ".migration.json");
}

bool readMarkerFanLX(const fs::path &databasePath, nlohmann::json &out) {
    const auto marker = markerPathFanLX(databasePath);
    if (!fs::exists(marker))
        return false;
    try {
        out = nlohmann::json::parse(AtomicFileWriterFanLX::read(marker));
    } catch (const std::exception &) {
        // 标记损坏等同于未完成迁移：宁可重跑，也不承认半成品。
        return false;
    }
    return out.value("done", false);
}

void writeMarkerFanLX(const fs::path &databasePath, const nlohmann::json &details) {
    auto payload = details;
    payload["done"] = true;
    const auto marker = markerPathFanLX(databasePath);
    const auto temporary = fs::path(marker.string() + ".tmp." + std::to_string(GetCurrentProcessId()));
    AtomicFileWriterFanLX::writeNew(temporary, payload.dump(2));
    if (!MoveFileExW(temporary.c_str(), marker.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw ChatMigrationErrorFanLX("迁移完成标记写入失败，Win32=" + std::to_string(GetLastError()));
    }
}

// 关闭连接后清理 WAL 边车文件，再把新库原子发布到目标路径。
void publishDatabaseFanLX(const fs::path &staging, const fs::path &target) {
    std::error_code ignored;
    fs::remove(fs::path(staging.string() + "-wal"), ignored);
    fs::remove(fs::path(staging.string() + "-shm"), ignored);
    fs::remove(target, ignored);
    fs::rename(staging, target);
}

void requireFileFanLX(const fs::path &path, const char *label) {
    if (!fs::exists(path))
        throw ChatMigrationErrorFanLX(std::string("旧存储") + label + "不存在：" + path.u8string());
}

void verifyDatabaseFanLX(const fs::path &staging) {
    sqliteFanLX::ConnectionFanLX verify(staging);
    sqliteFanLX::StatementFanLX integrity(verify, "PRAGMA integrity_check");
    if (!integrity.step() || integrity.columnText(0) != "ok")
        throw ChatMigrationErrorFanLX("迁移后 integrity_check 未通过");
    integrity.reset();
    sqliteFanLX::StatementFanLX foreign(verify, "PRAGMA foreign_key_check");
    if (foreign.step())
        throw ChatMigrationErrorFanLX("迁移后 foreign_key_check 发现越界引用");
}

void cleanupStagingFanLX(const fs::path &staging) {
    std::error_code ignored;
    fs::remove(staging, ignored);
    fs::remove(fs::path(staging.string() + "-wal"), ignored);
    fs::remove(fs::path(staging.string() + "-shm"), ignored);
}

// 旧 JSON 消息节点 → 仓储记录。旧格式没有可信服务器时间：accepted_at_utc 保持为空，
// 由界面明确标注"原始时间未知"，不得伪造成发送时间或迁移时刻。
MessageRowFanLX legacyRowFanLX(const nlohmann::json &node, const std::string &owner, ScopeFanLX scope) {
    MessageRowFanLX row;
    const auto messageId = node.value("messageId", std::int64_t{0});
    const auto conversationSeq = node.value("conversationSeq", std::int64_t{0});
    if (messageId > 0)
        row.messageId = messageId;
    row.conversationSequence = conversationSeq;
    row.sender = node.value("sender", std::string());
    row.recipient = node.value("recipient", std::string());
    row.requestId = node.value("requestId", std::string());
    row.body = node.value("text", std::string());
    row.acceptedAtUtc.clear();
    row.state = row.sender == owner ? MessageStateFanLX::SentAck : MessageStateFanLX::Received;
    if (messageId <= 0)
        row.state = MessageStateFanLX::Pending; // 待发送：不得伪装成已确认
    row.conversationKey =
        conversationKeyFanLX(scope, owner, row.sender == owner ? row.recipient : row.sender);
    return row;
}

} // namespace

bool ChatMigrationFanLX::clientMigrationDone(const fs::path &databasePath) {
    nlohmann::json marker;
    return readMarkerFanLX(databasePath, marker) && fs::exists(databasePath);
}

bool ChatMigrationFanLX::serverMigrationDone(const fs::path &databasePath) {
    nlohmann::json marker;
    return readMarkerFanLX(databasePath, marker) && fs::exists(databasePath);
}

MigrationReportFanLX ChatMigrationFanLX::migrateClient(const ClientSourceFanLX &source) {
    MigrationReportFanLX report;
    requireFileFanLX(source.legacyJsonPath, "客户端快照");
    if (clientMigrationDone(source.databasePath)) {
        report.alreadyDone = true;
        report.message = "客户端聊天记录已完成迁移，直接使用 SQLite 库：" + source.databasePath.u8string();
        return report;
    }

    nlohmann::json legacy = nlohmann::json::parse(AtomicFileWriterFanLX::read(source.legacyJsonPath));
    if (legacy.value("schema", std::string()) != "fanlx-im-client-1")
        throw ChatMigrationErrorFanLX("旧客户端存储 schema 不符，拒绝迁移");
    if (legacy.value("account", std::string()) != source.ownerAccount)
        throw ChatMigrationErrorFanLX("旧客户端存储账号与本次登录账号不符，拒绝迁移");
    for (const char *field : {"outbox", "inbox", "cursors"}) {
        if (!legacy.contains(field) || !legacy[field].is_object())
            throw ChatMigrationErrorFanLX(std::string("旧客户端存储缺少字段：") + field);
    }

    const auto staging = fs::path(source.databasePath.string() + ".migrating");
    cleanupStagingFanLX(staging);

    std::int64_t migratedMessages = 0;
    std::int64_t migratedOutbox = 0;
    std::set<std::string> seenRequestIds;
    std::set<std::int64_t> seenMessageIds;
    std::set<std::string> missingCursorKeys;

    try {
        {
            SqliteChatRepositoryFanLX repository({staging, source.serverInstanceId, source.ownerAccount,
                                                  source.deviceId});

            // ① 待发送与已确认的本机消息：按原 requestId 与内容原样导入。
            std::vector<nlohmann::json> outboxRows;
            for (const auto &entry : legacy.at("outbox"))
                outboxRows.push_back(entry);
            std::sort(outboxRows.begin(), outboxRows.end(), [](const auto &left, const auto &right) {
                return left.value("sequence", std::int64_t{0}) < right.value("sequence", std::int64_t{0});
            });
            for (const auto &node : outboxRows) {
                const auto requestId = node.value("requestId", std::string());
                if (requestId.empty())
                    throw ChatMigrationErrorFanLX("旧 outbox 存在缺少 requestId 的记录");
                if (!seenRequestIds.insert(requestId).second)
                    throw ChatMigrationErrorFanLX("旧 outbox 存在重复 requestId：" + requestId);
                const auto scope = node.value("scope", std::string("direct")) == "group"
                                       ? ScopeFanLX::Group
                                       : ScopeFanLX::Direct;
                if (node.value("recipient", std::string()).empty())
                    throw ChatMigrationErrorFanLX("旧 outbox 存在缺少收件人的记录");
                auto row = legacyRowFanLX(node, source.ownerAccount, scope);
                row.requestId = requestId;
                if (row.messageId) {
                    if (!seenMessageIds.insert(*row.messageId).second)
                        throw ChatMigrationErrorFanLX("旧存储存在重复 messageId：" +
                                                      std::to_string(*row.messageId));
                }
                repository.importLegacyMessage(row);
                ++migratedMessages;
                ++migratedOutbox;
            }

            // ② 已落库的接收消息：按 messageId 升序导入（允许乱序，游标最后统一重算）。
            std::vector<nlohmann::json> inboxRows;
            for (const auto &entry : legacy.at("inbox"))
                inboxRows.push_back(entry);
            std::sort(inboxRows.begin(), inboxRows.end(), [](const auto &left, const auto &right) {
                return left.value("messageId", std::int64_t{0}) < right.value("messageId", std::int64_t{0});
            });
            for (const auto &node : inboxRows) {
                auto row = legacyRowFanLX(node, source.ownerAccount, ScopeFanLX::Direct);
                if (!row.messageId || row.conversationSequence <= 0)
                    throw ChatMigrationErrorFanLX("旧 inbox 记录缺少 messageId 或会话序号");
                if (!seenMessageIds.insert(*row.messageId).second)
                    throw ChatMigrationErrorFanLX("旧存储存在重复 messageId：" +
                                                  std::to_string(*row.messageId));
                repository.importLegacyMessage(row);
                ++migratedMessages;
            }

            // ③ 旧游标核对：旧记录允许"已确认但未入库"，只报告不伪造。
            for (const auto &entry : legacy.at("cursors").items()) {
                const auto legacyCursor = entry.value().get<std::int64_t>();
                if (legacyCursor > 0)
                    missingCursorKeys.insert(entry.key());
            }
            repository.rebuildSyncCursorsFanLX();
            for (const auto &key : missingCursorKeys) {
                if (repository.lastConversationSequence(key) == 0)
                    report.warnings.push_back("旧游标 " + key + " 无对应消息，未导入（不伪造历史）");
            }
            report.clientMessages = migratedMessages;
            report.clientOutbox = migratedOutbox;
        }

        verifyDatabaseFanLX(staging);
        publishDatabaseFanLX(staging, source.databasePath);
        writeMarkerFanLX(source.databasePath,
                         {{"kind", "im-client"},
                          {"owner_account", source.ownerAccount},
                          {"server_instance_id", source.serverInstanceId},
                          {"source", source.legacyJsonPath.filename().string()},
                          {"messages", migratedMessages},
                          {"outbox", migratedOutbox},
                          {"migrated_at_utc", utcNowFanLX()}});
    } catch (...) {
        // 失败不留半成品：删除临时库，原 JSON 保持不变，可修复后重试。
        cleanupStagingFanLX(staging);
        throw;
    }

    report.migrated = true;
    report.message = "客户端聊天记录已迁移：" + std::to_string(report.clientMessages) + " 条消息，" +
                     std::to_string(report.clientOutbox) + " 条待发送；原 JSON 保留为只读备份，未回写。";
    report.warnings.push_back("旧 JSON 没有可信服务器时间：这些记录的 accepted_at_utc 为空，界面按"
                             "\"原始时间未知\"显示，不伪造发送时间");
    return report;
}

MigrationReportFanLX ChatMigrationFanLX::migrateServer(const ServerSourceFanLX &source) {
    MigrationReportFanLX report;
    requireFileFanLX(source.legacyJsonPath, "服务端日志");
    if (serverMigrationDone(source.databasePath)) {
        report.alreadyDone = true;
        report.message = "服务端聊天记录已完成迁移，直接使用 SQLite 库：" + source.databasePath.u8string();
        return report;
    }

    nlohmann::json legacy = nlohmann::json::parse(AtomicFileWriterFanLX::read(source.legacyJsonPath));
    if (legacy.value("schema", std::string()) != "fanlx-im-server-1")
        throw ChatMigrationErrorFanLX("旧服务端存储 schema 不符，拒绝迁移");
    if (!legacy.contains("messages") || !legacy.at("messages").is_array() || !legacy.contains("acks") ||
        !legacy.at("acks").is_object())
        throw ChatMigrationErrorFanLX("旧服务端存储结构不完整，拒绝迁移");

    const auto staging = fs::path(source.databasePath.string() + ".migrating");
    cleanupStagingFanLX(staging);

    std::int64_t migratedMessages = 0;
    std::int64_t migratedCursors = 0;
    try {
        {
            SqliteMessageStoreFanLX store(staging, source.serverInstanceId);

            struct LegacyRowFanLX {
                std::int64_t messageId = 0;
                std::int64_t conversationSequence = 0;
                ServerAcceptRequestFanLX request;
            };
            std::vector<LegacyRowFanLX> rows;
            for (const auto &entry : legacy.at("messages")) {
                if (!entry.contains("message") || !entry.contains("recipients"))
                    throw ChatMigrationErrorFanLX("旧服务端消息缺少 message 或 recipients 字段");
                const auto &node = entry.at("message");
                LegacyRowFanLX row;
                row.messageId = node.value("messageId", std::int64_t{0});
                row.conversationSequence = node.value("conversationSeq", std::int64_t{0});
                if (row.messageId <= 0 || row.conversationSequence <= 0)
                    throw ChatMigrationErrorFanLX("旧服务端消息缺少编号或会话序号");
                row.request.sender = qualifyAccountFanLX(node.value("sender", std::string()));
                row.request.target = qualifyAccountFanLX(node.value("recipient", std::string()));
                row.request.body = node.value("text", std::string());
                row.request.requestId = node.value("requestId", std::string());
                row.request.scope = node.value("scope", std::string("direct")) == "group"
                                        ? ScopeFanLX::Group
                                        : ScopeFanLX::Direct;
                if (row.request.sender.empty() || row.request.target.empty() ||
                    row.request.requestId.empty())
                    throw ChatMigrationErrorFanLX("旧服务端消息缺少发送者、目标或 requestId");
                for (const auto &account : entry.at("recipients"))
                    row.request.recipients.push_back(qualifyAccountFanLX(account.get<std::string>()));
                if (row.request.recipients.empty())
                    throw ChatMigrationErrorFanLX("旧服务端消息缺少收件人快照");
                rows.push_back(std::move(row));
            }
            // 旧日志按 messageId 单调递增；按原编号升序导入并核对严格单调。
            std::sort(rows.begin(), rows.end(),
                      [](const auto &left, const auto &right) { return left.messageId < right.messageId; });
            std::int64_t previous = 0;
            for (const auto &row : rows) {
                if (row.messageId <= previous)
                    throw ChatMigrationErrorFanLX("旧服务端日志编号非单调，拒绝迁移");
                previous = row.messageId;
                store.importLegacyMessage(row.request, row.messageId, row.conversationSequence,
                                          row.request.recipients);
                ++migratedMessages;
            }
            // 旧编号可能是 1..N 连续段：抬到 N+1，避免迁移后重复分配同一编号。
            if (previous > 0)
                store.raiseMessageCounterFanLX(previous + 1);
            for (const auto &entry : legacy.at("acks").items()) {
                const auto account = qualifyAccountFanLX(entry.key());
                if (!entry.value().is_object())
                    throw ChatMigrationErrorFanLX("旧服务端 ACK 结构非法：" + entry.key());
                for (const auto &cursor : entry.value().items()) {
                    const auto sequence = cursor.value().get<std::int64_t>();
                    if (sequence <= 0)
                        continue;
                    // 旧键是纯会话键：补上通道前缀后才与新版一致。
                    store.advanceDeliveryCursor(account, qualifyConversationKeyFanLX(cursor.key()), sequence);
                    ++migratedCursors;
                }
            }
            report.serverMessages = migratedMessages;
            report.serverCursors = migratedCursors;
        }

        verifyDatabaseFanLX(staging);
        publishDatabaseFanLX(staging, source.databasePath);
        writeMarkerFanLX(source.databasePath,
                         {{"kind", "im-server"},
                          {"server_instance_id", source.serverInstanceId},
                          {"source", source.legacyJsonPath.filename().string()},
                          {"messages", migratedMessages},
                          {"cursors", migratedCursors},
                          {"migrated_at_utc", utcNowFanLX()}});
    } catch (...) {
        cleanupStagingFanLX(staging);
        throw;
    }

    report.migrated = true;
    report.message = "服务端聊天记录已迁移：" + std::to_string(report.serverMessages) + " 条消息，" +
                     std::to_string(report.serverCursors) + " 条投递游标；原 JSON 保留为只读备份，未回写。";
    if (report.serverMessages > 0)
        report.warnings.push_back("旧 JSON 没有可信服务器时间：迁移记录的 accepted_at_utc 记为迁移时刻，"
                                 "只能作为迁移标记，不能当作原始发送时间");
    return report;
}

} // namespace chatDbFanLX

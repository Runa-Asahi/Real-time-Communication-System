#include "db/ChatSchemaStoreFanLX.h"
#include "db/ChatRepositoryFanLX.h"
#include "db/ChatSchemaFanLX.h"
#include "db/SqliteHandleFanLX.h"
#include <optional>
#include <sqlite3.h>

namespace chatDbFanLX {

namespace {

std::optional<std::string> readMeta(sqliteFanLX::ConnectionFanLX &connection, const std::string &key) {
    sqliteFanLX::StatementFanLX statement(connection, "SELECT value FROM schema_meta WHERE key = ?1");
    sqliteFanLX::bindAllFanLX(statement, key);
    if (statement.step())
        return statement.columnText(0);
    return std::nullopt;
}

void writeMeta(sqliteFanLX::ConnectionFanLX &connection, const std::string &key, const std::string &value) {
    sqliteFanLX::StatementFanLX statement(
        connection, "INSERT INTO schema_meta(key, value) VALUES(?1, ?2)"
                    " ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    sqliteFanLX::bindAllFanLX(statement, key, value);
    sqliteFanLX::runStatementFanLX(statement);
}

bool hasMetaRowFanLX(sqliteFanLX::ConnectionFanLX &connection, const std::string &key) {
    sqliteFanLX::StatementFanLX statement(connection, "SELECT 1 FROM schema_meta WHERE key = ?1");
    sqliteFanLX::bindAllFanLX(statement, key);
    return statement.step();
}

// 校验版本：0/缺失视为全新库；高于本代码支持版本一律拒绝，禁止旧代码写新结构。
int requireSupportedVersion(sqliteFanLX::ConnectionFanLX &connection, int supported) {
    const auto text = readMeta(connection, "schema_version");
    if (!text)
        return 0;
    int version = 0;
    try {
        std::size_t used = 0;
        version = std::stoi(*text, &used);
        if (used != text->size() || version < 0)
            throw std::invalid_argument("schema_version 非数字");
    } catch (const std::exception &) {
        throw ChatMigrationErrorFanLX("数据库 schema_version 元数据损坏：" + *text);
    }
    if (version > supported)
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Unsupported,
                                      "数据库 schema 版本 " + std::to_string(version) + " 高于本程序支持的 " +
                                          std::to_string(supported) + "，请升级客户端后再打开");
    return version;
}

void requireIdentity(sqliteFanLX::ConnectionFanLX &connection, const std::string &key,
                     const std::string &expected, const std::string &label) {
    const auto value = readMeta(connection, key);
    if (!value || *value != expected)
        throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Unavailable,
                                      "聊天数据库" + label + "不符（字段 " + key + "）：库内为 '" +
                                          (value ? *value : std::string("(缺失)")) + "'，本次连接为 '" +
                                          expected + "'");
}

SchemaResultFanLX applySchema(sqliteFanLX::ConnectionFanLX &connection,
                             const std::vector<std::string> &statements, int supported,
                             const std::vector<std::pair<std::string, std::string>> &identity,
                             const std::vector<std::pair<std::string, std::string>> &defaults) {
    sqliteFanLX::TransactionFanLX transaction(connection);
    // 建表必须先于读取版本：全新库里 schema_meta 还不存在，先查询会得到空结果而误判。
    for (const auto &statement : statements)
        connection.execute(statement);
    const bool fresh = !hasMetaRowFanLX(connection, "schema_version");
    const int version = requireSupportedVersion(connection, supported);
    if (fresh || version == 0) {
        writeMeta(connection, "schema_version", std::to_string(supported));
        for (const auto &[key, value] : identity)
            writeMeta(connection, key, value);
        // 计数器与序号在新建库时给初值；它们随后由业务推进，不能再当作固定身份校验。
        for (const auto &[key, value] : defaults)
            writeMeta(connection, key, value);
        transaction.commit();
        return SchemaResultFanLX::Created;
    }
    // 已存在的库必须逐项核对身份，防止把别人的库当成自己的续写。
    for (const auto &[key, value] : identity) {
        if (key == "schema_version")
            continue;
        requireIdentity(connection, key, value, "归属");
    }
    // 缺失的计数器（例如早期库）补初值，但不覆盖已有值。
    for (const auto &[key, value] : defaults) {
        if (!hasMetaRowFanLX(connection, key))
            writeMeta(connection, key, value);
    }
    transaction.commit();
    return version == supported ? SchemaResultFanLX::UpToDate : SchemaResultFanLX::Migrated;
}

} // namespace

SchemaResultFanLX applyClientSchemaFanLX(sqliteFanLX::ConnectionFanLX &connection,
                                         const std::string &serverInstanceId, const std::string &ownerAccount,
                                         const std::string &deviceId) {
    if (serverInstanceId.empty() || ownerAccount.empty() || deviceId.empty())
        throw std::invalid_argument("聊天数据库身份参数缺失");
    return applySchema(connection, clientSchemaStatementsFanLX(), clientSchemaVersionFanLX,
                       {{"schema_version", std::to_string(clientSchemaVersionFanLX)},
                        {"server_instance_id", serverInstanceId},
                        {"owner_account", ownerAccount},
                        {"device_id", deviceId}},
                       {{"next_local_sequence", "1"}});
}

SchemaResultFanLX applyServerSchemaFanLX(sqliteFanLX::ConnectionFanLX &connection,
                                        const std::string &serverInstanceId) {
    if (serverInstanceId.empty())
        throw std::invalid_argument("服务端聊天数据库缺少 instanceId");
    return applySchema(connection, serverSchemaStatementsFanLX(), serverSchemaVersionFanLX,
                       {{"schema_version", std::to_string(serverSchemaVersionFanLX)},
                        {"server_instance_id", serverInstanceId}},
                       {{"next_message_id", "1"}});
}
std::string metaValueFanLX(sqliteFanLX::ConnectionFanLX &connection, const std::string &key) {
    return readMeta(connection, key).value_or(std::string());
}

void setMetaValueFanLX(sqliteFanLX::ConnectionFanLX &connection, const std::string &key,
                       const std::string &value) {
    writeMeta(connection, key, value);
}

} // namespace chatDbFanLX

#pragma once
#include <string>

namespace sqliteFanLX {
class ConnectionFanLX;
}

namespace chatDbFanLX {

// 建表/校验/迁移的统一入口。只有版本一致或可正向升级时才返回；
// 结构更新集中在 applySchemaFanLX，禁止在仓储实现里临时拼接 DDL。
enum class SchemaResultFanLX { Created, UpToDate, Migrated };

// 幂等：已存在则校验版本；缺失则整段建表并写入版本与身份元数据。
SchemaResultFanLX applyClientSchemaFanLX(sqliteFanLX::ConnectionFanLX &connection,
                                         const std::string &serverInstanceId, const std::string &ownerAccount,
                                         const std::string &deviceId);
SchemaResultFanLX applyServerSchemaFanLX(sqliteFanLX::ConnectionFanLX &connection,
                                        const std::string &serverInstanceId);

// 读取/写入 schema_meta 键值。缺失返回空字符串；写入在同一连接上立即提交。
std::string metaValueFanLX(sqliteFanLX::ConnectionFanLX &connection, const std::string &key);
void setMetaValueFanLX(sqliteFanLX::ConnectionFanLX &connection, const std::string &key,
                       const std::string &value);

} // namespace chatDbFanLX

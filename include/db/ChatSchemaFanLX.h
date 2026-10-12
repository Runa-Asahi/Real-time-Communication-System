#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace sqliteFanLX {
class ConnectionFanLX;
}

namespace chatDbFanLX {

// 客户端与服务端各自的 schema 版本：版本不同即需要迁移；未知的更高版本一律拒绝打开。
inline constexpr int clientSchemaVersionFanLX = 1;
inline constexpr int serverSchemaVersionFanLX = 1;
inline constexpr const char *legacyMarkerFileFanLX = "chat.db.migration.json";

// 建表语句集（全部 IF NOT EXISTS，可重复调用）。
const std::vector<std::string> &clientSchemaStatementsFanLX();
const std::vector<std::string> &serverSchemaStatementsFanLX();

} // namespace chatDbFanLX

#pragma once
#include "db/ChatRepositoryFanLX.h"
#include <filesystem>
#include <string>
#include <vector>

namespace chatDbFanLX {

// 迁移报告：只统计真实完成的动作；未迁移项必须显式说明原因，禁止伪造。
struct MigrationReportFanLX {
    bool migrated = false;    // 本次调用是否真的执行了迁移
    bool alreadyDone = false; // 之前已完成迁移（标记文件存在）
    std::string message;      // 可直接显示的中文说明
    std::int64_t clientMessages = 0;
    std::int64_t clientOutbox = 0;
    std::int64_t serverMessages = 0;
    std::int64_t serverCursors = 0;
    std::vector<std::string> warnings;
};

// 把旧 IM JSON 存储迁移到 SQLite。约束：
// - 离线迁移，调用方需已持有数据目录独占锁，禁止运行中的客户端/服务端同时写 JSON；
// - 在临时库中单事务导入，随后做 integrity_check / foreign_key_check 与行数核对；
// - 只有全部校验通过才把新库发布到目标路径并写入完成标记；
// - 失败时删除临时库并保留原 JSON，可修复后重试；原 JSON 留作只读备份，不回写。
class ChatMigrationFanLX {
  public:
    struct ClientSourceFanLX {
        std::filesystem::path legacyJsonPath; // im-client/state.json
        std::filesystem::path databasePath;   // chat.db
        std::string serverInstanceId;
        std::string ownerAccount;
        std::string deviceId;
    };
    struct ServerSourceFanLX {
        std::filesystem::path legacyJsonPath; // im-server/state.json
        std::filesystem::path databasePath;
        std::string serverInstanceId;
    };

    static MigrationReportFanLX migrateClient(const ClientSourceFanLX &source);
    static MigrationReportFanLX migrateServer(const ServerSourceFanLX &source);

    // 迁移是否已完成（标记文件存在且数据库存在）。
    static bool clientMigrationDone(const std::filesystem::path &databasePath);
    static bool serverMigrationDone(const std::filesystem::path &databasePath);
};

} // namespace chatDbFanLX

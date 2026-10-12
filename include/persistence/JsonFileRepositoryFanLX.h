#pragma once
#include "persistence/CommitStageFanLX.h"
#include "persistence/DataDirectoryLockFanLX.h"
#include "persistence/RepositoryFanLX.h"
class JsonFileRepositoryFanLX final : public RepositoryFanLX {
    std::filesystem::path directoryFanLX;
    DataDirectoryLockFanLX lockFanLX;
    bool writableFanLX = false;
    bool closedFanLX = false;
    std::uint64_t generationFanLX = 0;
    std::string committedBytesFanLX;
    LoadStatusFanLX statusFanLX = LoadStatusFanLX::Empty;
    CommitHookFanLX hookFanLX;

  public:
    explicit JsonFileRepositoryFanLX(std::filesystem::path directory, CommitHookFanLX hook = {});
    LoadResultFanLX load() override;
    void initialize() override;
    LoadResultFanLX recover() override;
    void commit(const PlatformSnapshotFanLX &snapshot) override;
    bool writable() const noexcept override {
        return writableFanLX && !closedFanLX;
    }
    void close() noexcept override;
    const std::filesystem::path &directory() const noexcept {
        return directoryFanLX;
    }
    // 主快照文件是否与"最近一次加载/提交的字节"不同。
    // 供"服务端无状态"的读穿查询使用：只有真的变了才重新解析，避免每次调用都读盘。
    bool changedOnDisk() const;
};

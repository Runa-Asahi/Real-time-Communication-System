#include "persistence/JsonFileRepositoryFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include "persistence/PersistenceErrorFanLX.h"
#include "persistence/SchemaValidatorFanLX.h"
#include "persistence/SnapshotCodecFanLX.h"
#include "infra/UtcClockFanLX.h"
#include <chrono>

JsonFileRepositoryFanLX::JsonFileRepositoryFanLX(std::filesystem::path directory, CommitHookFanLX hook)
    : directoryFanLX(std::filesystem::weakly_canonical(std::filesystem::absolute(directory))),
      lockFanLX(directoryFanLX), hookFanLX(std::move(hook)) {}
LoadResultFanLX JsonFileRepositoryFanLX::load() {
    if (closedFanLX)
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::Storage, "仓储已关闭");
    writableFanLX = false;
    const auto main = directoryFanLX / L"state.json", prev = directoryFanLX / L"state.json.prev";
    std::string reason;
    if (std::filesystem::exists(main)) {
        try {
            auto bytes = AtomicFileWriterFanLX::read(main);
            auto s = SnapshotCodecFanLX::parse(bytes);
            if (s.generation == 0)
                throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::InvalidData, "主快照未提交");
            if (std::filesystem::exists(prev)) {
                // 合法备份可能与旧主同代，但不能比主文件更新。
                try {
                    auto p = SnapshotCodecFanLX::parse(AtomicFileWriterFanLX::read(prev));
                    if (p.generation > s.generation)
                        return {statusFanLX = LoadStatusFanLX::Failed, {}, "主备代数矛盾"};
                } catch (const PersistenceErrorFanLX &) { /* 主文件合法时允许损坏旧备份，下一次提交重建。 */
                }
            }
            generationFanLX = s.generation;
            committedBytesFanLX = std::move(bytes);
            writableFanLX = true;
            return {statusFanLX = LoadStatusFanLX::Loaded, std::move(s), "主快照加载成功"};
        } catch (const PersistenceErrorFanLX &e) {
            reason = e.what();
            if (e.code == PersistenceErrorCodeFanLX::UnsupportedSchema)
                return {statusFanLX = LoadStatusFanLX::UnsupportedSchema, {}, reason};
        }
    }
    if (std::filesystem::exists(prev)) {
        try {
            auto s = SnapshotCodecFanLX::parse(AtomicFileWriterFanLX::read(prev));
            if (s.generation == 0)
                throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::InvalidData, "备份未提交");
            generationFanLX = s.generation;
            return {statusFanLX = LoadStatusFanLX::RecoveredReadOnly, std::move(s),
                    "回退到上一有效代；可能丢失最近提交。确认恢复前只读。" + reason};
        } catch (const std::exception &e) {
            reason += e.what();
        }
    }
    if (std::filesystem::exists(main) || std::filesystem::exists(prev))
        return {statusFanLX = LoadStatusFanLX::Failed, {}, reason};
    for (const auto &file : std::filesystem::directory_iterator(directoryFanLX))
        if (file.path().filename() != L".lock")
            return {statusFanLX = LoadStatusFanLX::UncommittedOnly, {}, "存在未提交文件，需要显式初始化"};
    return {statusFanLX = LoadStatusFanLX::Empty, {}, "空目录，需要显式初始化"};
}
// 主快照是否与"最近一次加载/提交的字节"不同。
// 只做一次读盘比较：调用方（服务端读穿查询）据此决定要不要重新解析，
// 因此正常情况下每次调用只多一次文件读取，不会重复构建领域对象。
bool JsonFileRepositoryFanLX::changedOnDisk() const {
    if (closedFanLX)
        return false;
    const auto main = directoryFanLX / L"state.json";
    if (!std::filesystem::exists(main))
        return !committedBytesFanLX.empty();
    return AtomicFileWriterFanLX::read(main) != committedBytesFanLX;
}

void JsonFileRepositoryFanLX::initialize() {    if (closedFanLX ||
        (statusFanLX != LoadStatusFanLX::Empty && statusFanLX != LoadStatusFanLX::UncommittedOnly))
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::RecoveryRequired, "当前状态不允许初始化");
    if (std::filesystem::exists(directoryFanLX / L"state.json") ||
        std::filesystem::exists(directoryFanLX / L"state.json.prev"))
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::RecoveryRequired, "不能覆盖已有数据初始化");
    generationFanLX = 0;
    committedBytesFanLX.clear();
    writableFanLX = true;
}
void JsonFileRepositoryFanLX::commit(const PlatformSnapshotFanLX &snapshot) {
    if (!writable())
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::RecoveryRequired, "当前数据目录不可写");
    // 仓储接口也可能被测试替身/后台调用；先校验原始枚举，避免编码时丢失非法值。
    SchemaValidatorFanLX::validate(snapshot);
    auto bytes = SnapshotCodecFanLX::encode(snapshot).dump(2);
    SnapshotCodecFanLX::parse(bytes); // 与加载使用同一边界，不产生无法重启的快照。
    if (snapshot.generation == 0 || snapshot.parentGeneration != generationFanLX ||
        snapshot.generation - 1 != generationFanLX)
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::InvalidData, "提交基础代数冲突");
    const auto main = directoryFanLX / L"state.json";
    if ((generationFanLX == 0 && std::filesystem::exists(main)) ||
        (generationFanLX != 0 && AtomicFileWriterFanLX::read(main) != committedBytesFanLX)) {
        writableFanLX = false;
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::CommitIndeterminate,
                                    "主文件被外部修改，停止写入");
    }
    try {
        AtomicFileWriterFanLX::publish(directoryFanLX, bytes, true, hookFanLX);
    } catch (...) {
        // 发布 API/提交点后的故障可能已写入：核验实际内容后才报告结果。
        try {
            if (std::filesystem::exists(main) && AtomicFileWriterFanLX::read(main) == bytes) {
                committedBytesFanLX.swap(bytes);
                generationFanLX = snapshot.generation;
                return;
            }
            const bool unchanged = generationFanLX == 0
                                       ? !std::filesystem::exists(main)
                                       : AtomicFileWriterFanLX::read(main) == committedBytesFanLX;
            if (!unchanged) {
                writableFanLX = false;
                throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::CommitIndeterminate, "提交结果不确定");
            }
        } catch (const PersistenceErrorFanLX &e) {
            if (e.code == PersistenceErrorCodeFanLX::CommitIndeterminate)
                throw;
            writableFanLX = false;
            throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::CommitIndeterminate, "无法核验提交结果");
        }
        throw;
    }
    committedBytesFanLX.swap(bytes);
    generationFanLX = snapshot.generation;
}
LoadResultFanLX JsonFileRepositoryFanLX::recover() {
    if (closedFanLX)
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::Storage, "仓储已关闭");
    writableFanLX = false;
    const auto prev = directoryFanLX / L"state.json.prev", main = directoryFanLX / L"state.json";
    const auto bytes = AtomicFileWriterFanLX::read(prev);
    auto restored = SnapshotCodecFanLX::parse(bytes);
    if (restored.generation == 0)
        throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::InvalidData, "不能恢复未提交备份");
    auto nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::string original;
    if (std::filesystem::exists(main)) {
        original = AtomicFileWriterFanLX::read(main);
        const auto evidence = directoryFanLX / ("state.damaged." + nonce);
        AtomicFileWriterFanLX::writeNew(evidence, original);
        if (AtomicFileWriterFanLX::read(evidence) != original)
            throw PersistenceErrorFanLX(PersistenceErrorCodeFanLX::Storage, "恢复证据校验失败");
    }
    nlohmann::json record = {{"sourceGeneration", restored.generation},
                             {"oldGeneration", nullptr},
                             {"time", utcNowFanLX()},
                             {"reason", "explicit recovery from previous snapshot"}};
    if (!original.empty()) {
        try {
            record["oldGeneration"] = SnapshotCodecFanLX::parse(original).generation;
        } catch (const PersistenceErrorFanLX &) {
            // 损坏/未知版本的原文件无法可信解读时保留 null，完整原字节另存证据。
        }
    }
    AtomicFileWriterFanLX::writeNew(directoryFanLX / ("recovery." + nonce + ".json"), record.dump(2));
    // 不能把损坏主文件备份到 prev，恢复分支始终保留有效备份。
    AtomicFileWriterFanLX::publish(directoryFanLX, bytes, false, hookFanLX);
    return load();
}
void JsonFileRepositoryFanLX::close() noexcept {
    writableFanLX = false;
    closedFanLX = true;
    lockFanLX.close();
}

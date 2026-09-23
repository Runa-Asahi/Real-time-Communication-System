#pragma once
#include "infra/ArcCacheFanLX.h"
#include "infra/MemoryPoolFanLX.h"
#include "infra/QueryResultFanLX.h"
#include "infra/QueryCacheMetricsFanLX.h"
#include <memory>
#include <string>
#include <vector>

// 非权威结果缓存。调用方把查询类型、服务、参数、局部版本编码进 key，先鉴权再访问。
// 结果对象来自统一对象池；幽灵只保留 key，不持有结果。返回值复制，不能逃逸池生命周期。
class QueryCacheFanLX {
    using PoolFanLX = MemoryPoolFanLX<QueryResultFanLX, 64>;
    using HandleFanLX = std::shared_ptr<const QueryResultFanLX>;
    PoolFanLX poolFanLX; // 析构顺序：ARC 先销毁所有句柄，再销毁池。
    ArcCacheFanLX<std::string, HandleFanLX> cacheFanLX;
    QueryCacheMetricsFanLX metricsFanLX;
    bool enabledFanLX = true;
  public:
    explicit QueryCacheFanLX(std::size_t entries = 256, std::size_t bytes = 256 * 1024)
        : cacheFanLX(entries, bytes) {}
    QueryCacheFanLX(const QueryCacheFanLX &) = delete;
    QueryCacheFanLX &operator=(const QueryCacheFanLX &) = delete;
    std::optional<std::vector<std::string>> get(const std::string &key) {
        if (!enabledFanLX) return std::nullopt;
        try {
            if (auto value = cacheFanLX.get(key)) {
                ++metricsFanLX.hits;
                return (*value)->values;
            }
        } catch (...) { ++metricsFanLX.failures; cacheFanLX.clear(); }
        ++metricsFanLX.misses;
        return std::nullopt;
    }
    void put(const std::string &key, const std::vector<std::string> &values) noexcept {
        if (!enabledFanLX) return;
        try {
            // 逻辑载荷统计包含键、vector 的已申请字符串槽和字符串容量。
            // 不冒充进程 RSS；池保留块另行报告，链表/哈希/控制块的实现开销另列边界。
            std::size_t bytes = sizeof(QueryResultFanLX) + key.size() + values.capacity() * sizeof(std::string);
            for (const auto &value : values) bytes += value.capacity() + 1;
            auto *raw = poolFanLX.create(QueryResultFanLX{values});
            HandleFanLX handle(raw, [this](const QueryResultFanLX *p) {
                poolFanLX.destroy(const_cast<QueryResultFanLX *>(p));
            });
            if (cacheFanLX.put(key, std::move(handle), bytes)) ++metricsFanLX.puts;
        } catch (...) { ++metricsFanLX.failures; cacheFanLX.clear(); }
        // 缓存分配失败只降低命中率，不改变已经计算出的业务结果。
    }
    void clear() noexcept { cacheFanLX.clear(); }
    void setEnabled(bool enabled) noexcept { clear(); enabledFanLX = enabled; }
    QueryCacheMetricsFanLX metrics() const noexcept {
        auto result = metricsFanLX;
        result.entries = cacheFanLX.size(); result.bytes = cacheFanLX.bytes();
        result.poolBlocks = poolFanLX.blocks(); result.poolReservedBytes = poolFanLX.reservedBytes();
        result.poolCreates = poolFanLX.allocations();
        return result;
    }
};

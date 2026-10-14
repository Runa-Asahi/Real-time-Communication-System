#pragma once
#include <cstddef>
struct QueryCacheMetricsFanLX {
    std::size_t hits = 0, misses = 0, puts = 0, failures = 0;
    std::size_t entries = 0, bytes = 0, poolBlocks = 0, poolReservedBytes = 0, poolCreates = 0;
    // poolCreates 是成功构造次数。poolReuses 是其中复用已有槽位的次数。
    // poolBlocks / poolReservedBytes 是池块本身，不是进程 RSS。
    std::size_t poolReuses = 0, poolLive = 0, poolPeakLive = 0, poolRejectedDestroys = 0;
};

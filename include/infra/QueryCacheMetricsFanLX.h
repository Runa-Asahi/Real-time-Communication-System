#pragma once
#include <cstddef>
struct QueryCacheMetricsFanLX {
    std::size_t hits = 0, misses = 0, puts = 0, failures = 0;
    std::size_t entries = 0, bytes = 0, poolBlocks = 0, poolReservedBytes = 0, poolCreates = 0;
};

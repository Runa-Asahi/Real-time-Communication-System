// 对象池与普通 new/delete 的对照微基准。
// 同一对象类型、同一操作次数、同一校验和。输出实测时间，不预设谁更快。
#include "infra/MemoryPoolFanLX.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

namespace {
struct PayloadFanLX {
    int id = 0;
    std::string body;
    PayloadFanLX(int value, std::string text) : id(value), body(std::move(text)) {}
};
constexpr int kOpsFanLX = 200000;
constexpr int kBlockFanLX = 64;

std::uint64_t checksumFanLX(int id, const std::string &body) {
    return static_cast<std::uint64_t>(id) + static_cast<unsigned char>(body.empty() ? 0 : body.front());
}
} // namespace

int main() {
    const std::string text(32, 'm');
    std::uint64_t pooledSum = 0;
    std::uint64_t plainSum = 0;
    const auto pooledStart = std::chrono::steady_clock::now();
    {
        MemoryPoolFanLX<PayloadFanLX, kBlockFanLX> pool;
        for (int i = 0; i < kOpsFanLX; ++i) {
            auto *item = pool.create(i, text);
            pooledSum += checksumFanLX(item->id, item->body);
            if (!pool.destroy(item))
                return 1;
        }
        if (pool.live() != 0 || pool.constructions() != static_cast<std::size_t>(kOpsFanLX))
            return 2;
        const auto pooledUs = std::chrono::duration<double, std::micro>(
                                  std::chrono::steady_clock::now() - pooledStart)
                                  .count();
        std::cout << "pool_us=" << pooledUs << " ops=" << kOpsFanLX << " constructions=" << pool.constructions()
                  << " reuses=" << pool.reuses() << " blocks=" << pool.blocks()
                  << " reserved_bytes=" << pool.reservedBytes() << " peak_live=" << pool.peakLive()
                  << " checksum=" << pooledSum << '\n';
    }
    const auto plainStart = std::chrono::steady_clock::now();
    for (int i = 0; i < kOpsFanLX; ++i) {
        auto *item = new PayloadFanLX(i, text);
        plainSum += checksumFanLX(item->id, item->body);
        delete item;
    }
    const auto plainUs =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - plainStart).count();
    std::cout << "plain_us=" << plainUs << " ops=" << kOpsFanLX << " checksum=" << plainSum << '\n';
    if (pooledSum != plainSum)
        return 3;
    return 0;
}

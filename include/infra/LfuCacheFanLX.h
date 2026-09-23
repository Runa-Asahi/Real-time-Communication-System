#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_map>

// 独立 LFU 计数器，不保存 Value。ARC 直接读取本计数作为 L2 淘汰依据，避免两套账漂移。
// 平均频次超限时整体向上取半；每 128 次访问额外老化，防止历史热点永久滞留。
template <class Key, class Hash = std::hash<Key>> class LfuFrequencyFanLX {
    std::unordered_map<Key, std::size_t, Hash> frequenciesFanLX;
    std::size_t totalAccessFanLX = 0, maxAverageAccessFanLX, touchesFanLX = 0, decaysFanLX = 0;
    static constexpr std::size_t counterLimitFanLX = std::numeric_limits<std::size_t>::max() / 4;
    void halveFanLX() noexcept {
        totalAccessFanLX = 0;
        for (auto &item : frequenciesFanLX) {
            item.second = item.second / 2 + item.second % 2; // 避免 (n+1) 溢出。
            totalAccessFanLX += item.second;
        }
        ++decaysFanLX;
    }
    void normalizeFanLX() noexcept {
        // 不用整数平均截断，也不乘 size，避免掩盖小数超限或乘法溢出。
        while (!frequenciesFanLX.empty() &&
               (totalAccessFanLX / size() > maxAverageAccessFanLX ||
                (totalAccessFanLX / size() == maxAverageAccessFanLX && totalAccessFanLX % size())))
            halveFanLX();
    }
  public:
    explicit LfuFrequencyFanLX(std::size_t maxAverageAccess = 32)
        : maxAverageAccessFanLX(maxAverageAccess ? maxAverageAccess : 32) {}
    void touch(const Key &key) {
        if (++touchesFanLX % 128 == 0) halveFanLX();
        while (totalAccessFanLX >= counterLimitFanLX) halveFanLX();
        auto &count = frequenciesFanLX[key];
        ++count; ++totalAccessFanLX;
        normalizeFanLX();
    }
    void set(const Key &key, std::size_t count) {
        erase(key);
        count = std::max<std::size_t>(1, std::min(count, counterLimitFanLX));
        while (totalAccessFanLX > std::numeric_limits<std::size_t>::max() - count) halveFanLX();
        frequenciesFanLX.emplace(key, count);
        totalAccessFanLX += count;
        normalizeFanLX();
    }
    std::size_t frequency(const Key &key) const {
        auto it = frequenciesFanLX.find(key);
        return it == frequenciesFanLX.end() ? 0 : it->second;
    }
    void erase(const Key &key) {
        auto it = frequenciesFanLX.find(key);
        if (it == frequenciesFanLX.end()) return;
        totalAccessFanLX -= it->second;
        frequenciesFanLX.erase(it);
        normalizeFanLX(); // 删除低频项也可能使剩余平均频率超限。
    }
    void clear() noexcept { frequenciesFanLX.clear(); totalAccessFanLX = touchesFanLX = decaysFanLX = 0; }
    std::size_t size() const noexcept { return frequenciesFanLX.size(); }
    std::size_t averageAccessCount() const noexcept { return size() ? totalAccessFanLX / size() : 0; }
    std::size_t totalAccessCount() const noexcept { return totalAccessFanLX; }
    std::size_t decayCount() const noexcept { return decaysFanLX; }
};

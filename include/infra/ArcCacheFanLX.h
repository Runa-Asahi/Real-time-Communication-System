#pragma once
#include "infra/LfuCacheFanLX.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

// ARC + LRU-K + LFU 复合策略：四条链表、唯一键路由、统一有效载荷预算。
// L1 为短期数据，L2 为反复访问的数据；B1/B2 只留键、最近 K 次逻辑访问时刻及频次快照。
// p 使用 ARC 比例步长调整；L2 用带衰减的 LFU 替代标准 ARC 的 LRU，因此明确称为复合 ARC。
// 淘汰同频项时优先最久未访问者。L1 晋升要求最近 K 次历史已齐，幽灵回源保留访问历史。
template <class Key, class Value, class Hash = std::hash<Key>> class ArcCacheFanLX {
    enum class LevelFanLX { L1, L2, B1, B2 };
    struct EntryFanLX {
        Key key;
        std::optional<Value> value;
        std::size_t bytes = 0, freq = 1;
        LevelFanLX level = LevelFanLX::L1;
        std::vector<std::uint64_t> history;
        bool ghostObserved = false;
    };
    using ListFanLX = std::list<EntryFanLX>;
    using IteratorFanLX = typename ListFanLX::iterator;
    ListFanLX l1FanLX, l2FanLX, b1FanLX, b2FanLX;
    std::unordered_map<Key, IteratorFanLX, Hash> routeFanLX;
    LfuFrequencyFanLX<Key, Hash> lfuFanLX;
    std::size_t capacityFanLX, maxBytesFanLX, pFanLX = 0, kFanLX, usedBytesFanLX = 0;
    std::uint64_t tickFanLX = 0;
    ListFanLX &listFanLX(LevelFanLX level) {
        switch (level) {
        case LevelFanLX::L1: return l1FanLX;
        case LevelFanLX::L2: return l2FanLX;
        case LevelFanLX::B1: return b1FanLX;
        case LevelFanLX::B2: return b2FanLX;
        }
        throw std::logic_error("缓存分区无效");
    }
    static bool ghostFanLX(LevelFanLX level) noexcept {
        return level == LevelFanLX::B1 || level == LevelFanLX::B2;
    }
    void recordAccessFanLX(IteratorFanLX node) {
        if (tickFanLX == std::numeric_limits<std::uint64_t>::max()) {
            // 极端运行期溢出时丢弃历史时刻，避免回溯次序反转；有效值不受影响。
            for (auto &entry : routeFanLX) entry.second->history.clear();
            tickFanLX = 0;
        }
        if (node->history.size() == kFanLX) node->history.erase(node->history.begin());
        node->history.push_back(++tickFanLX);
        if (node->freq < std::numeric_limits<std::size_t>::max()) ++node->freq;
    }
    void adaptFanLX(IteratorFanLX node) {
        if (node->ghostObserved) return;
        if (node->level == LevelFanLX::B1) {
            const auto delta = std::max<std::size_t>(1, b2FanLX.size() / b1FanLX.size());
            pFanLX += std::min(delta, capacityFanLX - pFanLX);
        } else {
            const auto delta = std::max<std::size_t>(1, b1FanLX.size() / b2FanLX.size());
            pFanLX -= std::min(delta, pFanLX);
        }
        node->ghostObserved = true; // get 未命中再 put 回源，同一请求只调节一次。
    }
    void dropFanLX(IteratorFanLX node) {
        auto &list = listFanLX(node->level);
        if (!ghostFanLX(node->level)) usedBytesFanLX -= node->bytes;
        if (node->level == LevelFanLX::L2) lfuFanLX.erase(node->key);
        routeFanLX.erase(node->key);
        list.erase(node);
    }
    void trimGhostsFanLX() {
        while (ghostSize() > capacityFanLX) {
            auto &list = b2FanLX.empty() ? b1FanLX : b2FanLX;
            dropFanLX(std::prev(list.end()));
        }
    }
    void replaceFanLX(bool fromB2) {
        const bool fromL1 = !l1FanLX.empty() &&
            (l2FanLX.empty() || l1FanLX.size() > pFanLX || (fromB2 && l1FanLX.size() == pFanLX));
        auto &source = fromL1 ? l1FanLX : l2FanLX;
        auto victim = std::prev(source.end());
        if (!fromL1) {
            // 遍历从 LRU 端开始，同频不替换候选，使旧热点自然退出。
            for (auto it = source.rbegin(); it != source.rend(); ++it)
                if (lfuFanLX.frequency(it->key) < lfuFanLX.frequency(victim->key))
                    victim = std::prev(it.base());
            victim->freq = lfuFanLX.frequency(victim->key);
            lfuFanLX.erase(victim->key);
        }
        usedBytesFanLX -= victim->bytes;
        victim->value.reset(); // 幽灵不持有 Value，也及时归还结果对象池槽位。
        victim->bytes = 0;
        victim->ghostObserved = false;
        victim->level = fromL1 ? LevelFanLX::B1 : LevelFanLX::B2;
        auto &dest = fromL1 ? b1FanLX : b2FanLX;
        dest.splice(dest.begin(), source, victim);
    }
  public:
    explicit ArcCacheFanLX(std::size_t maxEntries, std::size_t maxBytes, std::size_t k = 2,
                          std::size_t maxAverageFreq = 32)
        : lfuFanLX(maxAverageFreq), capacityFanLX(maxEntries), maxBytesFanLX(maxBytes),
          kFanLX(std::max<std::size_t>(1, k)) {
        if (kFanLX > 64) throw std::invalid_argument("LRU-K 历史长度范围为 1..64");
        routeFanLX.reserve(maxEntries);
    }
    ArcCacheFanLX(const ArcCacheFanLX &) = delete;
    ArcCacheFanLX &operator=(const ArcCacheFanLX &) = delete;
    std::optional<Value> get(const Key &key) {
        try {
            auto found = routeFanLX.find(key);
            if (found == routeFanLX.end()) return std::nullopt;
            auto node = found->second;
            if (ghostFanLX(node->level)) {
                if (!node->ghostObserved) { recordAccessFanLX(node); adaptFanLX(node); }
                return std::nullopt;
            }
            auto result = node->value; // 先复制结果；后续缓存维护失败时可由业务回源。
            recordAccessFanLX(node);
            if (node->level == LevelFanLX::L1 && node->history.size() >= kFanLX) {
                lfuFanLX.set(key, node->freq);
                l2FanLX.splice(l2FanLX.begin(), l1FanLX, node);
                node->level = LevelFanLX::L2;
            } else if (node->level == LevelFanLX::L2) {
                lfuFanLX.touch(key);
                l2FanLX.splice(l2FanLX.begin(), l2FanLX, node);
            } else l1FanLX.splice(l1FanLX.begin(), l1FanLX, node);
            return result;
        } catch (...) { clear(); throw; } // 分配失败丢弃非权威缓存，不能留下失配路由。
    }
    bool put(Key key, Value value, std::size_t bytes = sizeof(Value)) {
        if (!capacityFanLX || bytes > maxBytesFanLX) return false;
        try {
            auto found = routeFanLX.find(key);
            bool fromB2 = false;
            IteratorFanLX node;
            if (found != routeFanLX.end() && ghostFanLX(found->second->level)) {
                node = found->second;
                fromB2 = node->level == LevelFanLX::B2;
                if (!node->ghostObserved) { recordAccessFanLX(node); adaptFanLX(node); }
                auto &old = listFanLX(node->level);
                node->value.emplace(std::move(value));
                node->bytes = bytes;
                const bool promote = node->history.size() >= kFanLX;
                if (promote) lfuFanLX.set(key, node->freq);
                auto &dest = promote ? l2FanLX : l1FanLX;
                dest.splice(dest.begin(), old, node);
                node->level = promote ? LevelFanLX::L2 : LevelFanLX::L1;
                node->ghostObserved = false;
            } else {
                if (found != routeFanLX.end()) dropFanLX(found->second);
                l1FanLX.push_front({std::move(key), std::move(value), bytes, 0, LevelFanLX::L1, {}, false});
                node = l1FanLX.begin();
                routeFanLX.emplace(node->key, node);
                recordAccessFanLX(node);
            }
            // 新键本身也可能被淘汰；保存键值用于最终判定。
            const Key insertedKey = node->key;
            if (usedBytesFanLX > std::numeric_limits<std::size_t>::max() - bytes) {
                clear(); // 极端逻辑字节输入不可表示时拒绝，绝不让账目回绕。
                return false;
            }
            usedBytesFanLX += bytes;
            while (size() > capacityFanLX || usedBytesFanLX > maxBytesFanLX)
                replaceFanLX(fromB2);
            trimGhostsFanLX();
            return contains(insertedKey);
        } catch (...) { clear(); throw; }
    }
    void erase(const Key &key) {
        auto found = routeFanLX.find(key);
        if (found != routeFanLX.end()) dropFanLX(found->second);
    }
    void clear() noexcept {
        routeFanLX.clear(); l1FanLX.clear(); l2FanLX.clear(); b1FanLX.clear(); b2FanLX.clear();
        lfuFanLX.clear(); usedBytesFanLX = pFanLX = 0; tickFanLX = 0;
    }
    bool contains(const Key &key) const {
        auto found = routeFanLX.find(key);
        return found != routeFanLX.end() && !ghostFanLX(found->second->level);
    }
    std::size_t size() const noexcept { return l1FanLX.size() + l2FanLX.size(); }
    std::size_t bytes() const noexcept { return usedBytesFanLX; }
    std::size_t l1Size() const noexcept { return l1FanLX.size(); }
    std::size_t l2Size() const noexcept { return l2FanLX.size(); }
    std::size_t ghostSize() const noexcept { return b1FanLX.size() + b2FanLX.size(); }
    std::size_t targetL1() const noexcept { return pFanLX; }
    std::size_t averageFrequency() const noexcept { return lfuFanLX.averageAccessCount(); }
    std::size_t averageL2Frequency() const noexcept { return averageFrequency(); }
    std::size_t l2Frequency(const Key &key) const { return lfuFanLX.frequency(key); }
    std::size_t ghostFrequency(const Key &key) const {
        auto found = routeFanLX.find(key);
        return found != routeFanLX.end() && ghostFanLX(found->second->level) ? found->second->freq : 0;
    }
    std::size_t l2TotalFrequency() const noexcept { return lfuFanLX.totalAccessCount(); }
    std::size_t decayCount() const noexcept { return lfuFanLX.decayCount(); }
};

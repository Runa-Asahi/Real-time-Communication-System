#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

// 2Q / LRU-K：新数据进入 Buffer，K 次访问后晋升 Main，抵抗一次性顺序扫描。
// 两条 std::list 共用唯一哈希路由；splice 晋升不分配结点且保持迭代器有效。
// 两分区条数/字节预算独立：Buffer 淘汰不影响 Main，Main 淘汰绝不降级。
template <class Key, class Value, class Hash = std::hash<Key>> class LruCacheFanLX {
    enum class CacheLevelFanLX { Buffer, Main };
    struct EntryFanLX {
        Key key;
        Value value;
        std::size_t bytes = 0, freq = 1;
        CacheLevelFanLX level = CacheLevelFanLX::Buffer;
    };
    using ListFanLX = std::list<EntryFanLX>;
    ListFanLX bufferListFanLX, mainListFanLX;
    std::unordered_map<Key, typename ListFanLX::iterator, Hash> routeFanLX;
    std::size_t bufferMaxEntriesFanLX, mainMaxEntriesFanLX, bufferMaxBytesFanLX, mainMaxBytesFanLX;
    std::size_t bufferBytesFanLX = 0, mainBytesFanLX = 0, kFanLX;
    void trimFanLX(ListFanLX &list, std::size_t &used, std::size_t entries, std::size_t bytes) {
        while (list.size() > entries || used > bytes) {
            auto node = std::prev(list.end());
            routeFanLX.erase(node->key); // 销毁结点前清路由，避免悬垂迭代器。
            used -= node->bytes;
            list.erase(node);
        }
    }
  public:
    explicit LruCacheFanLX(std::size_t maxEntries, std::size_t maxBytes, std::size_t k = 2)
        : bufferMaxEntriesFanLX(maxEntries ? std::max<std::size_t>(1, (maxEntries + 3) / 4) : 0),
          mainMaxEntriesFanLX(maxEntries - bufferMaxEntriesFanLX),
          bufferMaxBytesFanLX(maxBytes ? std::max<std::size_t>(1, (maxBytes + 3) / 4) : 0),
          mainMaxBytesFanLX(maxBytes - bufferMaxBytesFanLX),
          kFanLX(std::max<std::size_t>(1, k)) { routeFanLX.reserve(maxEntries); }
    LruCacheFanLX(const LruCacheFanLX &) = delete;
    LruCacheFanLX &operator=(const LruCacheFanLX &) = delete;
    std::optional<Value> get(const Key &key) {
        auto found = routeFanLX.find(key);
        if (found == routeFanLX.end()) return std::nullopt;
        auto node = found->second;
        Value result = node->value;
        if (node->freq < std::numeric_limits<std::size_t>::max()) ++node->freq;
        if (node->level == CacheLevelFanLX::Buffer) {
            if (node->freq >= kFanLX) {
                bufferBytesFanLX -= node->bytes;
                mainBytesFanLX += node->bytes;
                node->level = CacheLevelFanLX::Main;
                mainListFanLX.splice(mainListFanLX.begin(), bufferListFanLX, node);
                trimFanLX(mainListFanLX, mainBytesFanLX, mainMaxEntriesFanLX, mainMaxBytesFanLX);
            } else bufferListFanLX.splice(bufferListFanLX.begin(), bufferListFanLX, node);
        } else mainListFanLX.splice(mainListFanLX.begin(), mainListFanLX, node);
        return result;
    }
    bool put(Key key, Value value, std::size_t bytes = sizeof(Value)) {
        if (!bufferMaxEntriesFanLX || bytes > bufferMaxBytesFanLX) return false;
        erase(key);
        bufferListFanLX.push_front({std::move(key), std::move(value), bytes, 1, CacheLevelFanLX::Buffer});
        // 登记失败撤销结点；字节账目尚未发布。
        try { routeFanLX.emplace(bufferListFanLX.front().key, bufferListFanLX.begin()); }
        catch (...) { bufferListFanLX.pop_front(); throw; }
        bufferBytesFanLX += bytes;
        trimFanLX(bufferListFanLX, bufferBytesFanLX, bufferMaxEntriesFanLX, bufferMaxBytesFanLX);
        return true;
    }
    void erase(const Key &key) {
        auto found = routeFanLX.find(key);
        if (found == routeFanLX.end()) return;
        auto node = found->second;
        routeFanLX.erase(found);
        if (node->level == CacheLevelFanLX::Buffer) {
            bufferBytesFanLX -= node->bytes;
            bufferListFanLX.erase(node);
        } else { mainBytesFanLX -= node->bytes; mainListFanLX.erase(node); }
    }
    void clear() noexcept {
        routeFanLX.clear(); bufferListFanLX.clear(); mainListFanLX.clear();
        bufferBytesFanLX = mainBytesFanLX = 0;
    }
    std::size_t size() const noexcept { return routeFanLX.size(); }
    std::size_t bytes() const noexcept { return bufferBytesFanLX + mainBytesFanLX; }
    std::size_t bufferSize() const noexcept { return bufferListFanLX.size(); }
    std::size_t mainSize() const noexcept { return mainListFanLX.size(); }
    std::size_t bufferBytes() const noexcept { return bufferBytesFanLX; }
    std::size_t mainBytes() const noexcept { return mainBytesFanLX; }
};

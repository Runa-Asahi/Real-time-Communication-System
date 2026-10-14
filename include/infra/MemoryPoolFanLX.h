#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <vector>

// 对象池：按块申请槽位，placement new 构造，销毁后归还空闲链表。
//
// 线程边界：本池不是分配器，也不加锁。调用方必须把 create/destroy/析构串行化。
// QueryCacheFanLX 把池和缓存放在同一对象里，由持有缓存的那一个线程使用。
//
// 块在池析构前不归还操作系统。空闲槽位只说明可以复用，不说明应该释放整块。
// reservedBytes() 只统计这些槽位数组本身，不是进程 RSS，也不含 T 内部的堆分配。
template <class T, std::size_t BlockSize = 256> class MemoryPoolFanLX {
    static_assert(BlockSize > 0, "内存池块大小必须为正");
    struct NodeFanLX {
        NodeFanLX *next = nullptr;
        bool live = false;
        bool constructed = false; // 成功构造过，再次取出才算复用。
        alignas(T) std::byte storage[sizeof(T)];
    };
    std::vector<std::unique_ptr<NodeFanLX[]>> blocksFanLX;
    NodeFanLX *freeListFanLX = nullptr;
    std::size_t liveFanLX = 0, peakFanLX = 0, constructionsFanLX = 0, reusesFanLX = 0;
    std::size_t rejectedFanLX = 0;

    void growFanLX() {
        auto block = std::make_unique<NodeFanLX[]>(BlockSize);
        auto *nodes = block.get();
        // 块进入容器之后才挂上链表，push_back 失败时不会留下悬垂结点。
        blocksFanLX.push_back(std::move(block));
        for (std::size_t i = 0; i < BlockSize; ++i) {
            nodes[i].next = freeListFanLX;
            freeListFanLX = &nodes[i];
        }
    }
    // 只比较块范围和槽位对齐，不逐个扫描槽位状态。
    NodeFanLX *findNodeFanLX(T *object) {
        if (!object)
            return nullptr;
        // 转成整数再比较。无关指针做 < 比较是未定义行为，而 destroy 恰恰要拒绝外来指针。
        const auto address = reinterpret_cast<std::uintptr_t>(object);
        for (auto &block : blocksFanLX) {
            const auto base = reinterpret_cast<std::uintptr_t>(block.get());
            const auto span = BlockSize * sizeof(NodeFanLX);
            if (address < base || address - base >= span)
                continue;
            const auto delta = static_cast<std::size_t>(address - base);
            if (delta % sizeof(NodeFanLX) != offsetof(NodeFanLX, storage))
                continue;
            const auto index = delta / sizeof(NodeFanLX);
            if (index >= BlockSize)
                continue;
            auto *node = &block[index];
            if (reinterpret_cast<std::uintptr_t>(node->storage) != address)
                continue;
            return node;
        }
        return nullptr;
    }

  public:
    MemoryPoolFanLX() = default;
    MemoryPoolFanLX(const MemoryPoolFanLX &) = delete;
    MemoryPoolFanLX &operator=(const MemoryPoolFanLX &) = delete;
    ~MemoryPoolFanLX() {
        // 调用方忘了 destroy 时，仍析构存活对象，释放 string 等内部资源。
        for (auto &block : blocksFanLX) {
            for (std::size_t i = 0; i < BlockSize; ++i) {
                if (!block[i].live)
                    continue;
                std::launder(reinterpret_cast<T *>(block[i].storage))->~T();
                block[i].live = false;
            }
        }
    }
    template <class... Args> T *create(Args &&...args) {
        if (!freeListFanLX)
            growFanLX();
        auto *node = freeListFanLX;
        freeListFanLX = node->next;
        node->next = nullptr;
        try {
            auto *object = ::new (node->storage) T(std::forward<Args>(args)...);
            if (node->constructed)
                ++reusesFanLX;
            node->constructed = true;
            node->live = true;
            ++constructionsFanLX;
            if (++liveFanLX > peakFanLX)
                peakFanLX = liveFanLX;
            return object;
        } catch (...) {
            node->live = false;
            node->next = freeListFanLX;
            freeListFanLX = node; // 构造失败必须归还槽位，且不能算作一次构造成功。
            throw;
        }
    }
    // 只销毁本池里仍存活的对象。空指针、外部指针和重复归还返回 false，不改链表。
    // T 的析构函数不得抛出异常。
    bool destroy(T *object) noexcept {
        auto *node = findNodeFanLX(object);
        if (!node || !node->live) {
            ++rejectedFanLX;
            return false;
        }
        std::launder(object)->~T();
        node->live = false;
        --liveFanLX;
        node->next = freeListFanLX;
        freeListFanLX = node;
        return true;
    }
    std::size_t live() const noexcept { return liveFanLX; }
    std::size_t peakLive() const noexcept { return peakFanLX; }
    // 成功构造次数。历史名称 allocations() 与此相同，不是向系统申请内存的次数。
    std::size_t constructions() const noexcept { return constructionsFanLX; }
    std::size_t allocations() const noexcept { return constructionsFanLX; }
    std::size_t reuses() const noexcept { return reusesFanLX; }
    std::size_t blocks() const noexcept { return blocksFanLX.size(); }
    std::size_t blockAllocations() const noexcept { return blocksFanLX.size(); }
    std::size_t rejectedDestroys() const noexcept { return rejectedFanLX; }
    std::size_t reservedBytes() const noexcept { return blocks() * BlockSize * sizeof(NodeFanLX); }
};

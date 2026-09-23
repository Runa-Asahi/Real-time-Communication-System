#pragma once
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>

// 对象池：批量申请结点，placement new 构造，销毁后归还空闲链表。
// 每块所有结点均可使用；C++17 的 new Node[] 自动满足过对齐类型的要求。
template <class T, std::size_t BlockSize = 256> class MemoryPoolFanLX {
    static_assert(BlockSize > 0, "内存池块大小必须为正");
    struct NodeFanLX {
        NodeFanLX *next = nullptr;
        bool live = false;
        alignas(T) std::byte storage[sizeof(T)];
    };
    std::vector<std::unique_ptr<NodeFanLX[]>> blocksFanLX;
    NodeFanLX *freeListFanLX = nullptr;
    std::size_t liveFanLX = 0, peakFanLX = 0, createsFanLX = 0;
    void growFanLX() {
        auto block = std::make_unique<NodeFanLX[]>(BlockSize);
        auto *nodes = block.get();
        // vector 扩容成功后才发布结点，分配失败不留下悬垂链表。
        blocksFanLX.push_back(std::move(block));
        for (std::size_t i = 0; i < BlockSize; ++i) {
            nodes[i].next = freeListFanLX;
            freeListFanLX = &nodes[i];
        }
    }
  public:
    MemoryPoolFanLX() = default;
    MemoryPoolFanLX(const MemoryPoolFanLX &) = delete;
    MemoryPoolFanLX &operator=(const MemoryPoolFanLX &) = delete;
    ~MemoryPoolFanLX() {
        // 未显式归还的对象也执行析构，释放字符串等对象内部资源。
        for (auto &block : blocksFanLX)
            for (std::size_t i = 0; i < BlockSize; ++i)
                if (block[i].live)
                    std::launder(reinterpret_cast<T *>(block[i].storage))->~T();
    }
    template <class... Args> T *create(Args &&...args) {
        if (!freeListFanLX) growFanLX();
        auto *node = freeListFanLX;
        freeListFanLX = node->next;
        try {
            auto *object = ::new (node->storage) T(std::forward<Args>(args)...);
            node->live = true;
            ++createsFanLX;
            if (++liveFanLX > peakFanLX) peakFanLX = liveFanLX;
            return object;
        } catch (...) {
            node->next = freeListFanLX;
            freeListFanLX = node; // 构造失败必须归还槽位。
            throw;
        }
    }
    // 只归还本池创建且尚未销毁的对象；不是跨线程分配器。
    void destroy(T *object) noexcept {
        if (!object) return;
        object->~T();
        auto *node = reinterpret_cast<NodeFanLX *>(reinterpret_cast<std::byte *>(object) -
                                                  offsetof(NodeFanLX, storage));
        node->live = false;
        --liveFanLX;
        node->next = freeListFanLX;
        freeListFanLX = node;
    }
    std::size_t live() const noexcept { return liveFanLX; }
    std::size_t peakLive() const noexcept { return peakFanLX; }
    std::size_t allocations() const noexcept { return createsFanLX; }
    std::size_t blocks() const noexcept { return blocksFanLX.size(); }
    std::size_t reservedBytes() const noexcept { return blocks() * BlockSize * sizeof(NodeFanLX); }
};

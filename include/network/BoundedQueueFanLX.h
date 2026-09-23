#pragma once
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>

// 接收线程不等待 UI 腾位置。队列满时上层断开连接并报告原因，禁止无限积压。
template <class ValueFanLX> class BoundedQueueFanLX {
    std::size_t capacityFanLX;
    std::deque<ValueFanLX> valuesFanLX;
    mutable std::mutex mutexFanLX;

  public:
    explicit BoundedQueueFanLX(std::size_t capacity) : capacityFanLX(capacity) {
        if (!capacity)
            throw std::invalid_argument("队列容量必须大于零");
    }
    bool push(ValueFanLX value) {
        std::lock_guard<std::mutex> lock(mutexFanLX);
        if (valuesFanLX.size() == capacityFanLX)
            return false;
        valuesFanLX.push_back(std::move(value));
        return true;
    }
    std::optional<ValueFanLX> pop() {
        std::lock_guard<std::mutex> lock(mutexFanLX);
        if (valuesFanLX.empty())
            return std::nullopt;
        auto result = std::move(valuesFanLX.front());
        valuesFanLX.pop_front();
        return result;
    }
};

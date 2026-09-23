#pragma once
#include <stdexcept>
// 过对齐、析构计数与构造失败试件。
struct alignas(128) PoolProbeFanLX {
    int *liveFanLX;
    explicit PoolProbeFanLX(int *live, bool fail = false) : liveFanLX(live) {
        if (fail) throw std::runtime_error("模拟构造失败");
        ++*liveFanLX;
    }
    ~PoolProbeFanLX() { --*liveFanLX; }
};

#pragma once
#include "network/SocketRuntimeFanLX.h"
#include <utility>

// Socket 只能有一个所有者；移动传递所有权，异常路径也能自动 closesocket。
class SocketHandleFanLX {
    SOCKET valueFanLX = INVALID_SOCKET;

  public:
    explicit SocketHandleFanLX(SOCKET value = INVALID_SOCKET) noexcept : valueFanLX(value) {}
    ~SocketHandleFanLX() { reset(); }
    SocketHandleFanLX(const SocketHandleFanLX &) = delete;
    SocketHandleFanLX &operator=(const SocketHandleFanLX &) = delete;
    SocketHandleFanLX(SocketHandleFanLX &&other) noexcept
        : valueFanLX(std::exchange(other.valueFanLX, INVALID_SOCKET)) {}
    SocketHandleFanLX &operator=(SocketHandleFanLX &&other) noexcept {
        if (this != &other) {
            reset();
            valueFanLX = std::exchange(other.valueFanLX, INVALID_SOCKET);
        }
        return *this;
    }
    SOCKET get() const noexcept { return valueFanLX; }
    void reset() noexcept {
        if (valueFanLX != INVALID_SOCKET)
            closesocket(valueFanLX);
        valueFanLX = INVALID_SOCKET;
    }
};

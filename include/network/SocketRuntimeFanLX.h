#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>

// 每个网络所有者持有一个 Winsock 引用，最后一个销毁后才清理运行库。
class SocketRuntimeFanLX {
  public:
    SocketRuntimeFanLX() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            throw std::runtime_error("Winsock 初始化失败");
    }
    ~SocketRuntimeFanLX() { WSACleanup(); }
    SocketRuntimeFanLX(const SocketRuntimeFanLX &) = delete;
    SocketRuntimeFanLX &operator=(const SocketRuntimeFanLX &) = delete;
};

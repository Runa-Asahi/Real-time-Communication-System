#pragma once
#include "network/TcpPeerFanLX.h"

class TcpListenerFanLX {
    SocketRuntimeFanLX runtimeFanLX;
    SocketHandleFanLX socketFanLX;

  public:
    // 默认只监听本机；局域网演示须显式指定网卡 IPv4 或 0.0.0.0。
    explicit TcpListenerFanLX(unsigned short port, const std::string &ipv4 = "127.0.0.1");
    unsigned short port() const;
    std::unique_ptr<TcpPeerFanLX> accept(std::chrono::milliseconds timeout = std::chrono::seconds(10),
                                         std::size_t queueCapacity = 128);
};

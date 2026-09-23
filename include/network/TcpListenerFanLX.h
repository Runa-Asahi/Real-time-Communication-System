#pragma once
#include "network/TcpPeerFanLX.h"
#include <map>
struct PlatformStateFanLX;

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

// tick 与领域状态修改必须在同一控制线程执行。网络线程只负责帧接收。
class ImServerFanLX {
    struct Impl;
    std::unique_ptr<Impl> implFanLX;
  public:
    ImServerFanLX(const PlatformStateFanLX &state, const std::filesystem::path &directory,
                  std::map<std::string, std::string> credentials, unsigned short port,
                  const std::string &ipv4 = "127.0.0.1");
    ~ImServerFanLX();
    unsigned short port() const;
    void tick();
    std::size_t connectionsAccepted() const;
};

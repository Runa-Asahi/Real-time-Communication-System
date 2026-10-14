#pragma once
#include "network/ChatMessageFanLX.h"
#include <chrono>
#include <memory>
#include <optional>
#include <string>

// 客户端之间的 TCP 直连。监听、拨号和握手都在本对象的线程里完成，
// 调用方只在自己的工作线程上 poll / sendData，不要在 Qt 主线程上等待对端。
//
// 本对象不信任 TCP 对端自称的账号。只有服务端事先下发、且未过期的一次性票据
// 能通过握手。票据不写入日志。
class DirectLinkFanLX {
  public:
    struct StatsFanLX {
        std::size_t listenFailures = 0;
        std::size_t connectAttempts = 0;
        std::size_t connectSuccess = 0;
        std::size_t connectFail = 0;
        std::size_t rejected = 0;
        std::size_t dataSent = 0;
        std::size_t dataReceived = 0;
    };
    struct EventFanLX {
        enum class KindFanLX { Ready, Failed, Data, Closed };
        KindFanLX kind = KindFanLX::Failed;
        std::string peer;
        std::string ticket;
        std::string detail;
        ChatMessageFanLX message;
    };

    DirectLinkFanLX();
    ~DirectLinkFanLX();
    DirectLinkFanLX(const DirectLinkFanLX &) = delete;
    DirectLinkFanLX &operator=(const DirectLinkFanLX &) = delete;

    void bindIdentity(std::string account, std::string service);
    // 监听 127.0.0.1 上的临时端口。失败只记录原因，不抛异常。
    // 环境变量 FANLX_P2P_BIND 可改成一块真实网卡的 IPv4，供同一局域网演示；
    // 0.0.0.0 会被拒绝。公网 NAT 不在这里处理。
    bool start(unsigned short port = 0);
    void stop() noexcept;
    bool listening() const;
    unsigned short listenPort() const;
    std::string advertisedEndpoint() const;
    std::string listenError() const;

    // 以下方法只允许在 poll 所在的那一个线程调用。
    void expect(const std::string &peer, const std::string &ticket);
    void connectPeer(const std::string &peer, const std::string &ticket, const std::string &endpoint);
    void closePeer(const std::string &peer);
    void closePeers() noexcept;
    bool sendData(const std::string &peer, const ChatMessageFanLX &message);
    std::optional<EventFanLX> poll();
    StatsFanLX stats() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> implFanLX;
};

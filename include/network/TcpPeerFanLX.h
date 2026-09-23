#pragma once
#include "network/BoundedQueueFanLX.h"
#include "network/ChatMessageFanLX.h"
#include "network/SocketHandleFanLX.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <filesystem>
#include <vector>

// 控制线程负责 connect/send/close，唯一后台线程负责 recv。禁止多个控制线程并用。
// close 先 shutdown 唤醒接收，再 join，最后关闭句柄，避免句柄复用竞争。
class TcpPeerFanLX {
    SocketRuntimeFanLX runtimeFanLX;
    SocketHandleFanLX socketFanLX;
    BoundedQueueFanLX<ChatMessageFanLX> incomingFanLX;
    std::atomic<bool> stoppedFanLX{false};
    std::atomic<bool> endedFanLX{false};
    mutable std::mutex errorMutexFanLX;
    std::string errorFanLX;
    std::chrono::milliseconds timeoutFanLX;
    std::thread receiverFanLX;
    void receiveLoopFanLX() noexcept;
    void finishFanLX(const char *reason) noexcept;

  public:
    explicit TcpPeerFanLX(SocketHandleFanLX socket, std::size_t queueCapacity = 128,
                          std::chrono::milliseconds timeout = std::chrono::seconds(5));
    ~TcpPeerFanLX() { close(); }
    TcpPeerFanLX(const TcpPeerFanLX &) = delete;
    TcpPeerFanLX &operator=(const TcpPeerFanLX &) = delete;
    static std::unique_ptr<TcpPeerFanLX> connect(const std::string &ipv4, unsigned short port,
                                                 std::chrono::milliseconds timeout = std::chrono::seconds(5),
                                                 std::size_t queueCapacity = 128);
    void send(const ChatMessageFanLX &message);
    std::optional<ChatMessageFanLX> poll() { return incomingFanLX.pop(); }
    bool ended() const noexcept { return endedFanLX.load(); }
    std::string error() const;
    void close() noexcept;
};

// 一个账号一个长连接。工作线程独占 socket；公开方法以锁保护持久化 outbox/inbox。
class ImClientFanLX {
    struct Impl;
    std::unique_ptr<Impl> implFanLX;
  public:
    ImClientFanLX(std::string host, unsigned short port, std::string account, std::string token,
                  const std::filesystem::path &directory);
    ~ImClientFanLX();
    ImClientFanLX(const ImClientFanLX &) = delete;
    ImClientFanLX &operator=(const ImClientFanLX &) = delete;
    std::string send(const std::string &target, const std::string &text, bool group = false);
    void queryOnline(const std::string &friendAccount);
    std::vector<ChatMessageFanLX> history() const;
    std::optional<ChatMessageFanLX> pollEvent();
    std::size_t pending() const;
    bool online() const;
    std::string error() const;
    void reconnect();
};

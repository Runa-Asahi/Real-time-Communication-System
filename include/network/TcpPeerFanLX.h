#pragma once
#include "network/BoundedQueueFanLX.h"
#include "network/ChatMessageFanLX.h"
#include "network/MessageMailboxFanLX.h"
#include "network/SocketHandleFanLX.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <filesystem>
#include <vector>

// 控制线程负责 connect/send/close，唯一后台线程负责 recv。禁止多个控制线程并用。
// close 先 shutdown 唤醒接收，再 join，最后关闭句柄，避免句柄复用竞争。
//
// 发送路径：帧先进入每连接有界待发送缓冲（MessageMailboxFanLX），由控制线程推进。
// send() 保持"同步写完或抛异常"的既有语义；trySend()/flushSend() 是非阻塞接口，
// 供服务端与 GUI 客户端在单次 tick 中按预算推进，不为慢端阻塞。
class TcpPeerFanLX : public MessageMailboxFanLX::SinkFanLX {
    SocketRuntimeFanLX runtimeFanLX;
    SocketHandleFanLX socketFanLX;
    BoundedQueueFanLX<ChatMessageFanLX> incomingFanLX;
    std::atomic<bool> stoppedFanLX{false};
    std::atomic<bool> endedFanLX{false};
    mutable std::mutex errorMutexFanLX;
    std::string errorFanLX;
    std::chrono::milliseconds timeoutFanLX;
    std::thread receiverFanLX;
    std::unique_ptr<MessageMailboxFanLX> mailboxFanLX;
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

    // MessageMailboxFanLX::SinkFanLX：只做一次非阻塞 send 尝试，不抛异常。
    int writeBytes(const char *data, int size) noexcept override;
    std::string lastError() const override { return error(); }

    // 同步发送：入队 + 有界推进；失败抛异常（既有调用方语义不变）。
    void send(const ChatMessageFanLX &message);
    // 入队后立即返回。false 表示连接已停止或待发送队列已满（慢端）。
    bool trySend(const ChatMessageFanLX &message);
    // 按预算推进待发送缓冲。false 表示连接不可再用。
    bool flushSend(unsigned budget = 16);
    std::size_t queuedFrames() const noexcept { return mailboxFanLX ? mailboxFanLX->frameCount() : 0; }
    std::size_t rejectedSends() const noexcept { return mailboxFanLX ? mailboxFanLX->rejectedCount() : 0; }

    std::optional<ChatMessageFanLX> poll() { return incomingFanLX.pop(); }
    bool ended() const noexcept { return endedFanLX.load(); }
    std::string error() const;
    void close() noexcept;
};

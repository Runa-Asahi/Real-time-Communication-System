#include "network/TcpPeerFanLX.h"
#include "SocketIoFanLX.h"
#include "network/MessageCodecFanLX.h"
#include <algorithm>

namespace {
// 非阻塞写入：只尝试一次，把结果编码成 SinkFanLX 约定的返回值。
int writeOnceFanLX(SOCKET socket, const char *data, int size, int &lastError) noexcept {
    const int count = ::send(socket, data, size, 0);
    if (count == SOCKET_ERROR) {
        const int code = WSAGetLastError();
        if (code == WSAEWOULDBLOCK || code == WSAENOBUFS)
            return 0; // 稍后再试，不算失败
        lastError = code;
        return -1;
    }
    return count;
}
} // namespace

TcpPeerFanLX::TcpPeerFanLX(SocketHandleFanLX socket, std::size_t queueCapacity,
                           std::chrono::milliseconds timeout)
    : socketFanLX(std::move(socket)), incomingFanLX(queueCapacity), timeoutFanLX(timeout) {
    if (socketFanLX.get() == INVALID_SOCKET || timeout.count() <= 0)
        throw std::invalid_argument("连接句柄或超时非法");
    socketIoFanLX::nonblocking(socketFanLX.get());
    mailboxFanLX = std::make_unique<MessageMailboxFanLX>(*this);
    receiverFanLX = std::thread(&TcpPeerFanLX::receiveLoopFanLX, this);
}
std::unique_ptr<TcpPeerFanLX> TcpPeerFanLX::connect(const std::string &ipv4, unsigned short port,
                                                    std::chrono::milliseconds timeout,
                                                    std::size_t queueCapacity) {
    if (!port || timeout.count() <= 0)
        throw std::invalid_argument("端口/超时非法");
    SocketRuntimeFanLX runtime;
    SocketHandleFanLX socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket.get() == INVALID_SOCKET)
        socketIoFanLX::fail("创建连接失败");
    socketIoFanLX::nonblocking(socket.get());
    const auto endpoint = socketIoFanLX::address(ipv4, port);
    if (::connect(socket.get(), reinterpret_cast<const sockaddr *>(&endpoint), sizeof(endpoint)) ==
        SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
            socketIoFanLX::fail("连接失败");
        socketIoFanLX::wait(socket.get(), true, std::chrono::steady_clock::now() + timeout);
        int error = 0, length = sizeof(error);
        if (getsockopt(socket.get(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &length) ==
            SOCKET_ERROR)
            socketIoFanLX::fail("读取连接状态失败");
        if (error)
            throw std::runtime_error("连接失败 Winsock=" + std::to_string(error));
    }
    return std::make_unique<TcpPeerFanLX>(std::move(socket), queueCapacity, timeout);
}
int TcpPeerFanLX::writeBytes(const char *data, int size) noexcept {
    if (stoppedFanLX || endedFanLX || socketFanLX.get() == INVALID_SOCKET)
        return -1;
    int lastError = 0;
    const int written = writeOnceFanLX(socketFanLX.get(), data, size, lastError);
    if (written < 0)
        finishFanLX("发送失败");
    return written;
}
void TcpPeerFanLX::send(const ChatMessageFanLX &message) {
    if (!trySend(message))
        throw std::runtime_error("发送队列已满或连接已关闭");
    if (!mailboxFanLX->flushBlocking())
        throw std::runtime_error(mailboxFanLX->failure().empty() ? "发送失败" : mailboxFanLX->failure());
}
bool TcpPeerFanLX::trySend(const ChatMessageFanLX &message) {
    if (stoppedFanLX || endedFanLX || !mailboxFanLX)
        return false;
    return mailboxFanLX->enqueue(message);
}
bool TcpPeerFanLX::flushSend(unsigned budget) { return mailboxFanLX && mailboxFanLX->flush(budget); }
void TcpPeerFanLX::finishFanLX(const char *reason) noexcept {
    try {
        std::lock_guard<std::mutex> lock(errorMutexFanLX);
        errorFanLX = reason;
    } catch (...) { /* 即使分配失败，ended 仍可靠通知主线程结束。 */
    }
    shutdown(socketFanLX.get(), SD_BOTH);
    endedFanLX = true;
}
void TcpPeerFanLX::receiveLoopFanLX() noexcept {
    try {
        while (!stoppedFanLX) {
            std::array<unsigned char, 4> header{};
            std::string payload;
            std::size_t offset = 0, expected = header.size();
            bool readingHeader = true, started = false;
            auto deadline = std::chrono::steady_clock::time_point::max();
            for (;;) {
                if (stoppedFanLX)
                    return;
                if (started && std::chrono::steady_clock::now() >= deadline)
                    throw std::runtime_error("接收半帧超时");
                if (!socketIoFanLX::ready(socketFanLX.get(), false))
                    continue;
                char *destination = readingHeader ? reinterpret_cast<char *>(header.data()) : payload.data();
                const int count =
                    recv(socketFanLX.get(), destination + offset, static_cast<int>(expected - offset), 0);
                if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
                    continue;
                if (count == 0) {
                    finishFanLX(started ? "对端在半帧中断连" : "对端已断开");
                    return;
                }
                if (count < 0)
                    socketIoFanLX::fail("接收失败");
                if (!started) {
                    started = true;
                    deadline = std::chrono::steady_clock::now() + timeoutFanLX;
                }
                offset += static_cast<std::size_t>(count);
                if (offset != expected)
                    continue;
                if (readingHeader) {
                    expected = MessageCodecFanLX::payloadSize(header);
                    payload.resize(expected);
                    readingHeader = false;
                    offset = 0;
                } else
                    break;
            }
            if (!incomingFanLX.push(MessageCodecFanLX::decode(payload))) {
                finishFanLX("接收队列已满，请读取消息后重新连接");
                return;
            }
        }
    } catch (const std::exception &error) {
        finishFanLX(error.what());
    } catch (...) {
        finishFanLX("接收线程异常");
    }
}
std::string TcpPeerFanLX::error() const {
    std::lock_guard<std::mutex> lock(errorMutexFanLX);
    return errorFanLX;
}
void TcpPeerFanLX::close() noexcept {
    stoppedFanLX = true;
    if (mailboxFanLX)
        mailboxFanLX->close();
    if (socketFanLX.get() != INVALID_SOCKET)
        shutdown(socketFanLX.get(), SD_BOTH);
    if (receiverFanLX.joinable())
        receiverFanLX.join();
    socketFanLX.reset();
    endedFanLX = true;
}

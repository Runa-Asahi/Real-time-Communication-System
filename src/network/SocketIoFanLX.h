#pragma once
#include "network/SocketHandleFanLX.h"
#include <chrono>
#include <string>
#include "network/MessageCodecFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include <nlohmann/json.hpp>

namespace socketIoFanLX {
inline nlohmann::json messageJson(const ChatMessageFanLX &m) {
    return nlohmann::json::parse(MessageCodecFanLX::encode(m).substr(4));
}
inline ChatMessageFanLX message(const nlohmann::json &j) {
    return MessageCodecFanLX::decode(j.dump());
}
inline ChatMessageFanLX control(const std::string &type) {
    ChatMessageFanLX m;
    m.type = type;
    return m;
}
inline void commit(const std::filesystem::path &dir, nlohmann::json &live, nlohmann::json next) {
    const auto bytes = next.dump();
    // 与原快照读取上限一致；拒绝新增而不是 ACK 后在重启时无法读取。
    if (bytes.size() > 64u * 1024u * 1024u) throw std::runtime_error("IM 存储已满");
    AtomicFileWriterFanLX::publish(dir, bytes, false);
    live.swap(next);
}
inline void fail(const char *operation) {
    throw std::runtime_error(std::string(operation) + " Winsock=" + std::to_string(WSAGetLastError()));
}
inline void nonblocking(SOCKET socket) {
    u_long enabled = 1;
    if (ioctlsocket(socket, FIONBIO, &enabled) == SOCKET_ERROR)
        fail("设置非阻塞失败");
}
inline sockaddr_in address(const std::string &ipv4, unsigned short port) {
    sockaddr_in result{};
    result.sin_family = AF_INET;
    result.sin_port = htons(port);
    if (inet_pton(AF_INET, ipv4.c_str(), &result.sin_addr) != 1)
        throw std::invalid_argument("请输入数字 IPv4 地址");
    return result;
}
// 每次只等待 50 ms，接收线程可快速观察退出标记；外层维护整个操作的总截止时间。
inline bool ready(SOCKET socket, bool writing) {
    fd_set normal, errors;
    FD_ZERO(&normal);
    FD_SET(socket, &normal);
    FD_ZERO(&errors);
    FD_SET(socket, &errors);
    timeval timeout{0, 50000};
    const int result = select(0, writing ? nullptr : &normal, writing ? &normal : nullptr, &errors, &timeout);
    if (result == SOCKET_ERROR)
        fail("等待网络事件失败");
    return result > 0;
}
inline void wait(SOCKET socket, bool writing, std::chrono::steady_clock::time_point deadline) {
    while (std::chrono::steady_clock::now() < deadline)
        if (ready(socket, writing))
            return;
    throw std::runtime_error("网络操作超时");
}
} // namespace socketIoFanLX

#include "network/TcpListenerFanLX.h"
#include "SocketIoFanLX.h"

TcpListenerFanLX::TcpListenerFanLX(unsigned short port, const std::string &ipv4)
    : socketFanLX(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
    if (socketFanLX.get() == INVALID_SOCKET)
        socketIoFanLX::fail("创建监听套接字失败");
    const BOOL exclusive = TRUE;
    if (setsockopt(socketFanLX.get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char *>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR)
        socketIoFanLX::fail("设置独占端口失败");
    const auto endpoint = socketIoFanLX::address(ipv4, port);
    if (bind(socketFanLX.get(), reinterpret_cast<const sockaddr *>(&endpoint), sizeof(endpoint)) ==
        SOCKET_ERROR)
        socketIoFanLX::fail("绑定端口失败");
    if (listen(socketFanLX.get(), SOMAXCONN) == SOCKET_ERROR)
        socketIoFanLX::fail("监听失败");
    socketIoFanLX::nonblocking(socketFanLX.get());
}
unsigned short TcpListenerFanLX::port() const {
    sockaddr_in endpoint{};
    int size = sizeof(endpoint);
    if (getsockname(socketFanLX.get(), reinterpret_cast<sockaddr *>(&endpoint), &size) == SOCKET_ERROR)
        socketIoFanLX::fail("读取监听端口失败");
    return ntohs(endpoint.sin_port);
}
std::unique_ptr<TcpPeerFanLX> TcpListenerFanLX::accept(std::chrono::milliseconds timeout,
                                                       std::size_t queueCapacity) {
    if (timeout.count() <= 0)
        throw std::invalid_argument("超时必须为正数");
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        socketIoFanLX::wait(socketFanLX.get(), false, deadline);
        SocketHandleFanLX accepted(::accept(socketFanLX.get(), nullptr, nullptr));
        if (accepted.get() != INVALID_SOCKET)
            return std::make_unique<TcpPeerFanLX>(std::move(accepted), queueCapacity);
        if (WSAGetLastError() != WSAEWOULDBLOCK)
            socketIoFanLX::fail("接受连接失败");
    }
}

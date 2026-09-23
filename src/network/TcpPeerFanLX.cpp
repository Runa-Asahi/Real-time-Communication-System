#include "network/TcpPeerFanLX.h"
#include "SocketIoFanLX.h"
#include "network/MessageCodecFanLX.h"
#include <algorithm>

TcpPeerFanLX::TcpPeerFanLX(SocketHandleFanLX socket, std::size_t queueCapacity,
                           std::chrono::milliseconds timeout)
    : socketFanLX(std::move(socket)), incomingFanLX(queueCapacity), timeoutFanLX(timeout) {
    if (socketFanLX.get() == INVALID_SOCKET || timeout.count() <= 0)
        throw std::invalid_argument("连接句柄或超时非法");
    socketIoFanLX::nonblocking(socketFanLX.get());
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
void TcpPeerFanLX::send(const ChatMessageFanLX &message) {
    const auto frame = MessageCodecFanLX::encode(message);
    const auto deadline = std::chrono::steady_clock::now() + timeoutFanLX;
    try {
        std::size_t offset = 0;
        while (offset < frame.size()) {
            if (stoppedFanLX || endedFanLX)
                throw std::runtime_error("连接已关闭");
            socketIoFanLX::wait(socketFanLX.get(), true, deadline);
            const int count =
                ::send(socketFanLX.get(), frame.data() + offset, static_cast<int>(frame.size() - offset), 0);
            if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
                continue;
            if (count <= 0)
                socketIoFanLX::fail("发送失败");
            offset += static_cast<std::size_t>(count); // 短写时继续发送剩余字节。
        }
    } catch (...) {
        // 半帧发送失败后连接不可再用，否则下一帧可能被解释成上一帧的尾部。
        close();
        throw;
    }
}
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
    if (socketFanLX.get() != INVALID_SOCKET)
        shutdown(socketFanLX.get(), SD_BOTH);
    if (receiverFanLX.joinable())
        receiverFanLX.join();
    socketFanLX.reset();
    endedFanLX = true;
}

#include "persistence/DataDirectoryLockFanLX.h"
#include <random>
#include <set>
#include <condition_variable>

struct ImClientFanLX::Impl {
    using Clock = std::chrono::steady_clock;
    std::string host, account, token;
    unsigned short port;
    std::filesystem::path directory;
    DataDirectoryLockFanLX directoryLock;
    mutable std::mutex mutex;
    std::condition_variable wake;
    nlohmann::json db;
    BoundedQueueFanLX<ChatMessageFanLX> events{256};
    std::deque<ChatMessageFanLX> commands;
    std::string lastError;
    std::atomic<bool> stop{false}, active{false}, reset{false};
    std::thread worker;

    Impl(std::string ip, unsigned short p, std::string user, std::string secret, const std::filesystem::path &dir)
        : host(std::move(ip)), account(std::move(user)), token(std::move(secret)), port(p), directory(dir), directoryLock(dir) {
        if (account.empty() || !port || token.empty()) throw std::invalid_argument("IM 连接参数缺失");
        auto login = socketIoFanLX::control("LOGIN"); login.sender = account; login.text = token;
        MessageCodecFanLX::encode(login);
        if (std::filesystem::exists(dir / "state.json")) {
            db = nlohmann::json::parse(AtomicFileWriterFanLX::read(dir / "state.json"));
            if (db.at("schema") != "fanlx-im-client-1" || db.at("account") != account ||
                db.at("host") != host || db.at("port") != port || !db.at("outbox").is_object() ||
                !db.at("inbox").is_object() || !db.at("cursors").is_object() || db.at("device").get<std::string>().empty())
                throw std::runtime_error("IM 客户端存储身份或格式不符");
            for (const auto &row : db.at("outbox")) socketIoFanLX::message(row);
            for (const auto &row : db.at("inbox")) socketIoFanLX::message(row);
        } else {
            std::random_device random;
            std::string device;
            for (int i = 0; i < 8; ++i) device += std::to_string(random()) + "-";
            db = {{"schema", "fanlx-im-client-1"}, {"account", account}, {"host", host}, {"port", port},
                  {"device", device}, {"next", 1u}, {"outbox", nlohmann::json::object()},
                  {"inbox", nlohmann::json::object()}, {"cursors", nlohmann::json::object()}};
            socketIoFanLX::commit(directory, db, db);
        }
        worker = std::thread([this] { run(); });
    }
    ~Impl() {
        stop = true; wake.notify_all();
        if (worker.joinable()) worker.join();
    }
    void error(const std::string &text) {
        std::lock_guard<std::mutex> guard(mutex); lastError = text;
    }
    void received(TcpPeerFanLX &peer, const ChatMessageFanLX &m, std::set<std::string> &sent) {
        if (m.type == "LOGIN_OK") {
            active = true; error("");
            peer.send(socketIoFanLX::control("SYNC"));
        } else if (m.type == "SEND_ACK") {
            std::lock_guard<std::mutex> guard(mutex);
            if (db["outbox"].contains(m.requestId)) {
                const auto original = socketIoFanLX::message(db["outbox"][m.requestId]);
                if (m.sender != account || original.text != m.text || original.recipient != m.recipient ||
                    original.scope != m.scope || !m.messageId || !m.conversationSeq)
                    throw std::runtime_error("SEND_ACK 与 outbox 不符");
                auto next = db; next["outbox"].erase(m.requestId);
                socketIoFanLX::commit(directory, db, std::move(next));
            }
            sent.erase(m.requestId);
        } else if (m.type == "DELIVER") {
            if (!active || !m.messageId || !m.conversationSeq || m.sender.empty() ||
                (m.scope == "direct" && m.sender != account && m.recipient != account))
                throw std::runtime_error("投递目标/序号非法");
            bool inserted = false;
            {
                std::lock_guard<std::mutex> guard(mutex);
                const auto key = std::to_string(m.messageId);
                if (db["inbox"].contains(key)) {
                    if (db["inbox"][key] != socketIoFanLX::messageJson(m))
                        throw std::runtime_error("messageId 内容冲突");
                } else {
                    auto next = db;
                    next["inbox"][key] = socketIoFanLX::messageJson(m);
                    const auto conversation = m.scope == "group" ? "g|" + m.recipient :
                        "d|" + std::min(m.sender, m.recipient) + "|" + std::max(m.sender, m.recipient);
                    const auto cursor = next["cursors"].value(conversation, std::uint64_t{0});
                    if (m.conversationSeq <= cursor) throw std::runtime_error("会话投递乱序");
                    // 单服务器按日志顺序发送；群成员可见区间允许序号空洞。
                    next["cursors"][conversation] = m.conversationSeq;
                    socketIoFanLX::commit(directory, db, std::move(next));
                    inserted = true;
                }
            }
            if (inserted) events.push(m); // 通知溢出只丢提示，history 是持久化事实源。
            auto ack = socketIoFanLX::control("ACK"); ack.messageId = m.messageId; peer.send(ack);
        } else if (m.type == "ERROR") {
            error(m.text); events.push(m);
        } else if (m.type == "PRESENCE" || m.type == "SYNC_END") events.push(m);
        else if (m.type != "PONG") throw std::runtime_error("服务端响应类型非法");
    }
    void run() noexcept {
        unsigned backoff = 100;
        while (!stop) {
            try {
                reset = false;
                auto peer = TcpPeerFanLX::connect(host, port, std::chrono::seconds(2));
                auto login = socketIoFanLX::control("LOGIN"); login.sender = account; login.text = token; peer->send(login);
                auto last = Clock::now(), ping = last;
                std::set<std::string> sent;
                while (!stop && !reset && !peer->ended()) {
                    for (int budget = 0; budget < 32; ++budget) {
                        auto m = peer->poll(); if (!m) break;
                        last = Clock::now(); received(*peer, *m, sent);
                    }
                    if (active) {
                        backoff = 100;
                        std::optional<ChatMessageFanLX> outbound;
                        {
                            std::lock_guard<std::mutex> guard(mutex);
                            // 按本地提交顺序发送；拒绝的消息仍保留在 outbox。
                            for (const auto &row : db.at("outbox")) {
                                auto m = socketIoFanLX::message(row);
                                if (!sent.count(m.requestId) && (!outbound || m.sequence < outbound->sequence)) outbound = m;
                            }
                            if (!outbound && !commands.empty()) { outbound = commands.front(); commands.pop_front(); }
                        }
                        if (outbound) { peer->send(*outbound); if (outbound->type == "SEND") sent.insert(outbound->requestId); }
                    }
                    if (Clock::now() - ping >= std::chrono::seconds(5)) {
                        if (active) peer->send(socketIoFanLX::control("PING"));
                        ping = Clock::now();
                    }
                    if (Clock::now() - last > std::chrono::seconds(15)) throw std::runtime_error("心跳/登录超时，正在重连");
                    std::unique_lock<std::mutex> guard(mutex);
                    wake.wait_for(guard, std::chrono::milliseconds(10), [this] { return stop.load() || reset.load(); });
                }
                if (!stop) error(peer->error().empty() ? "连接重置，正在重连" : peer->error());
            } catch (const std::exception &e) { error(e.what()); }
              catch (...) { error("IM 工作线程异常"); }
            active = false;
            std::unique_lock<std::mutex> guard(mutex);
            // 有界指数退避 + 抖动，析构无需等待完整退避周期。
            wake.wait_for(guard, std::chrono::milliseconds(backoff + GetTickCount64() % 97), [this] { return stop.load(); });
            backoff = std::min(backoff * 2, 5000u);
        }
        active = false;
    }
};
ImClientFanLX::ImClientFanLX(std::string host, unsigned short port, std::string account, std::string token,
    const std::filesystem::path &dir) : implFanLX(std::make_unique<Impl>(std::move(host), port, std::move(account), std::move(token), dir)) {}
ImClientFanLX::~ImClientFanLX() = default;
std::string ImClientFanLX::send(const std::string &target, const std::string &text, bool group) {
    auto &p = *implFanLX;
    std::lock_guard<std::mutex> guard(p.mutex);
    auto next = p.db;
    const auto seq = next.at("next").get<std::uint64_t>();
    if (seq == std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("客户端序号耗尽");
    auto m = socketIoFanLX::control("SEND"); m.sender = p.account; m.recipient = target; m.text = text;
    m.sequence = seq;
    m.requestId = next.at("device").get<std::string>() + std::to_string(seq);
    m.scope = group ? "group" : "direct";
    next["next"] = seq + 1; next["outbox"][m.requestId] = socketIoFanLX::messageJson(m);
    socketIoFanLX::commit(p.directory, p.db, std::move(next));
    p.wake.notify_all(); return m.requestId;
}
void ImClientFanLX::queryOnline(const std::string &user) {
    auto &p = *implFanLX; std::lock_guard<std::mutex> guard(p.mutex);
    if (p.commands.size() >= 128) throw std::runtime_error("在线查询队列已满");
    auto m = socketIoFanLX::control("QUERY"); m.recipient = user;
    MessageCodecFanLX::encode(m); p.commands.push_back(m); p.wake.notify_all();
}
std::vector<ChatMessageFanLX> ImClientFanLX::history() const {
    auto &p = *implFanLX; std::lock_guard<std::mutex> guard(p.mutex);
    std::vector<ChatMessageFanLX> result;
    for (const auto &row : p.db.at("inbox")) result.push_back(socketIoFanLX::message(row));
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.messageId < b.messageId; });
    return result;
}
std::optional<ChatMessageFanLX> ImClientFanLX::pollEvent() { return implFanLX->events.pop(); }
std::size_t ImClientFanLX::pending() const { std::lock_guard<std::mutex> guard(implFanLX->mutex); return implFanLX->db.at("outbox").size(); }
bool ImClientFanLX::online() const { return implFanLX->active; }
std::string ImClientFanLX::error() const { std::lock_guard<std::mutex> guard(implFanLX->mutex); return implFanLX->lastError; }
void ImClientFanLX::reconnect() { implFanLX->reset = true; implFanLX->wake.notify_all(); }

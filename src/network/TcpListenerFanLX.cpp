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

#include "persistence/PlatformStateFanLX.h"
#include "persistence/DataDirectoryLockFanLX.h"
#include "persistence/PersistenceErrorFanLX.h"
#include "friend/IdentityResolverFanLX.h"
#include <set>
#include <limits>

struct ImServerFanLX::Impl {
    using Clock = std::chrono::steady_clock;
    struct Session {
        std::unique_ptr<TcpPeerFanLX> peer;
        std::string user;
        Clock::time_point last = Clock::now();
        std::uint64_t inFlight = 0;
        Clock::time_point sentAt = Clock::now();
        bool syncing = false;
    };
    const PlatformStateFanLX &state;
    std::filesystem::path directory;
    DataDirectoryLockFanLX lock;
    std::map<std::string, std::string> credentials;
    TcpListenerFanLX listener;
    nlohmann::json db;
    std::vector<std::unique_ptr<Session>> sessions;
    std::size_t accepted = 0;

    Impl(const PlatformStateFanLX &s, const std::filesystem::path &dir,
         std::map<std::string, std::string> tokens, unsigned short port, const std::string &ip)
        : state(s), directory(dir), lock(dir), credentials(std::move(tokens)), listener(port, ip) {
        for (const auto &[user, token] : credentials) {
            if (token.size() < 16 || token.size() > 4096 || !subscribed(user))
                throw std::runtime_error("IM 凭证需为已开通 QQ 的账号及至少16字符的令牌");
        }
        if (std::filesystem::exists(dir / "state.json")) {
            db = nlohmann::json::parse(AtomicFileWriterFanLX::read(dir / "state.json"));
            if (db.at("schema") != "fanlx-im-server-1" || !db.at("messages").is_array() ||
                !db.at("acks").is_object() || !db.at("seqs").is_object())
                throw std::runtime_error("IM 服务端存储格式错误");
            std::uint64_t previous = 0;
            for (const auto &row : db.at("messages")) {
                auto m = socketIoFanLX::message(row.at("message"));
                if (m.type != "DELIVER" || m.messageId <= previous || !row.at("recipients").is_array())
                    throw std::runtime_error("IM 消息日志损坏，拒绝回退已确认数据");
                previous = m.messageId;
            }
            if (db.at("next").get<std::uint64_t>() <= previous) throw std::runtime_error("IM 消息编号损坏");
        } else {
            db = {{"schema", "fanlx-im-server-1"}, {"next", 1u}, {"messages", nlohmann::json::array()},
                  {"acks", nlohmann::json::object()}, {"seqs", nlohmann::json::object()}};
            socketIoFanLX::commit(directory, db, db);
        }
    }
    bool subscribed(const std::string &u) const {
        return state.accounts.hasAccount(ServiceTypeFanLX::QQ, u) &&
               state.accounts.isAccountSubscribed(ServiceTypeFanLX::QQ, u);
    }
    bool friends(const std::string &a, const std::string &b) const {
        if (a == b || !subscribed(a) || !subscribed(b)) return false;
        IdentityResolverFanLX ids(state.accounts);
        return state.friends.contains(ids.accountKey(ServiceTypeFanLX::QQ, a),
                                      ids.accountKey(ServiceTypeFanLX::QQ, b));
    }
    bool member(const std::string &u, const std::string &group) const {
        try {
            return subscribed(u) && state.groups.get({GroupModeFanLX::QQ, group}).hasMember(
                state.accounts.accountByAccountId(ServiceTypeFanLX::QQ, u).userId);
        } catch (const std::out_of_range &) { return false; }
    }
    bool online(const std::string &u) const {
        for (const auto &s : sessions) if (s->user == u && !s->peer->ended()) return true;
        return false;
    }
    static std::string conversation(const ChatMessageFanLX &m) {
        if (m.scope == "group") return "g|" + m.recipient;
        return "d|" + std::min(m.sender, m.recipient) + "|" + std::max(m.sender, m.recipient);
    }
    void write(Session &s, const ChatMessageFanLX &m) {
        try { s.peer->send(m); }
        catch (...) { s.peer->close(); }
    }
    void presence(const std::string &u, bool active) {
        auto event = socketIoFanLX::control("PRESENCE");
        event.sender = u; event.text = active ? "online" : "offline";
        for (auto &s : sessions) if (!s->user.empty() && friends(s->user, u)) write(*s, event);
    }
    void handle(Session &s, ChatMessageFanLX m) {
        if (m.type == "LOGIN") {
            if (!s.user.empty()) throw std::runtime_error("已经登录");
            const auto it = credentials.find(m.sender);
            if (it == credentials.end() || it->second != m.text || !subscribed(m.sender))
                throw std::runtime_error("认证失败");
            // 单设备策略：拒绝第二个活跃会话；旧会话失效后重试，避免两端相互踢线。
            if (online(m.sender)) throw std::runtime_error("账号已有在线会话");
            s.user = m.sender;
            write(s, socketIoFanLX::control("LOGIN_OK"));
            presence(s.user, true);
            for (const auto &[u, token] : credentials) {
                (void)token;
                if (!friends(s.user, u)) continue;
                auto e = socketIoFanLX::control("PRESENCE"); e.sender = u;
                e.text = online(u) ? "online" : "offline"; write(s, e);
            }
            return;
        }
        if (s.user.empty()) throw std::runtime_error("请先认证");
        if (!subscribed(s.user)) throw std::runtime_error("账号服务已停用");
        if (!m.sender.empty() && m.sender != s.user) throw std::runtime_error("禁止伪造发送者");
        m.sender = s.user;
        if (m.type == "PING") { write(s, socketIoFanLX::control("PONG")); return; }
        if (m.type == "QUERY") {
            if (!friends(s.user, m.recipient)) throw std::runtime_error("只能查询好友在线状态");
            auto reply = socketIoFanLX::control("PRESENCE"); reply.sender = m.recipient;
            reply.text = online(m.recipient) ? "online" : "offline"; write(s, reply); return;
        }
        if (m.type == "SYNC") { s.syncing = true; return; }
        if (m.type == "ACK") {
            if (m.messageId == 0 || m.messageId != s.inFlight) throw std::runtime_error("ACK 不是当前投递消息");
            auto next = db;
            for (const auto &row : db.at("messages")) {
                const auto delivered = socketIoFanLX::message(row.at("message"));
                if (delivered.messageId != m.messageId) continue;
                // 客户端不能设置 cursor；服务器只推进实际投递且得到 ACK 的会话序号。
                next["acks"][s.user][conversation(delivered)] = delivered.conversationSeq;
                break;
            }
            socketIoFanLX::commit(directory, db, std::move(next));
            s.inFlight = 0;
            return;
        }
        if (m.type != "SEND") throw std::runtime_error("客户端请求类型非法");
        for (const auto &row : db.at("messages")) {
            const auto saved = socketIoFanLX::message(row.at("message"));
            if (saved.sender != s.user || saved.requestId != m.requestId) continue;
            if (saved.recipient != m.recipient || saved.text != m.text || saved.scope != m.scope)
                throw std::runtime_error("同一 requestId 不允许修改消息");
            auto reply = saved; reply.type = "SEND_ACK"; write(s, reply); return;
        }
        std::set<std::string> recipients{s.user};
        if (m.scope == "direct") {
            if (!friends(s.user, m.recipient)) throw std::runtime_error("单聊目标必须是好友");
            recipients.insert(m.recipient);
        } else {
            if (!member(s.user, m.recipient)) throw std::runtime_error("发送者不是群成员");
            for (const auto &uid : state.groups.get({GroupModeFanLX::QQ, m.recipient}).members()) {
                const auto *account = state.accounts.findAccount(uid, ServiceTypeFanLX::QQ);
                if (account && subscribed(account->accountId)) recipients.insert(account->accountId);
            }
        }
        auto next = db;
        m.messageId = next.at("next").get<std::uint64_t>();
        if (m.messageId == std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("消息编号耗尽");
        next["next"] = m.messageId + 1;
        const auto key = conversation(m);
        auto seq = next["seqs"].value(key, std::uint64_t{0});
        if (seq == std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("会话序号耗尽");
        m.conversationSeq = seq + 1; m.sequence = 0; m.type = "DELIVER";
        next["seqs"][key] = m.conversationSeq;
        next["messages"].push_back({{"message", socketIoFanLX::messageJson(m)}, {"recipients", recipients}});
        // 消息、幂等键、群序号和收件人快照一次原子落盘后才 ACK/投递。
        socketIoFanLX::commit(directory, db, std::move(next));
        m.type = "SEND_ACK"; write(s, m);
    }
    void deliver(Session &s) {
        if (s.user.empty() || s.inFlight || s.peer->ended()) return;
        for (const auto &row : db.at("messages")) {
            const auto &recipients = row.at("recipients");
            if (std::find(recipients.begin(), recipients.end(), s.user) == recipients.end()) continue;
            const auto m = socketIoFanLX::message(row.at("message"));
            if (m.scope == "group" && !member(s.user, m.recipient)) continue;
            const auto key = conversation(m);
            const auto ack = db.at("acks").contains(s.user) ? db.at("acks").at(s.user).value(key, std::uint64_t{0}) : 0;
            if (m.conversationSeq <= ack) continue;
            s.inFlight = m.messageId; s.sentAt = Clock::now(); write(s, m); return;
        }
        if (s.syncing) { write(s, socketIoFanLX::control("SYNC_END")); s.syncing = false; }
    }
    void tick() {
        // accept 超时不是错误；其他错误由调用方处理，避免吞掉监听故障。
        try {
            auto peer = listener.accept(std::chrono::milliseconds(1));
            ++accepted;
            if (sessions.size() >= 128) peer->close();
            else { auto s = std::make_unique<Session>(); s->peer = std::move(peer); sessions.push_back(std::move(s)); }
        } catch (const std::runtime_error &e) {
            if (std::string(e.what()) != "网络操作超时") throw;
        }
        for (auto &s : sessions) {
            if (s->peer->ended()) continue;
            for (int budget = 0; budget < 32; ++budget) {
                auto m = s->peer->poll(); if (!m) break;
                s->last = Clock::now();
                try { handle(*s, *m); }
                catch (const PersistenceErrorFanLX &e) {
                    auto error = socketIoFanLX::control("ERROR");
                    error.requestId = m->requestId; error.text = e.what(); write(*s, error);
                    s->peer->close(); // 提交失败：重连重放，不能让 ACK 窗口永久卡住。
                    break;
                }
                catch (const std::exception &e) {
                    auto error = socketIoFanLX::control("ERROR");
                    error.requestId = m->requestId; error.text = e.what(); write(*s, error);
                    // 存储错误不能当作成功；发送方 outbox 保留，显式重试。
                    if (s->user.empty()) s->peer->close();
                }
            }
            if (Clock::now() - s->last > std::chrono::seconds(20) ||
                (s->inFlight && Clock::now() - s->sentAt > std::chrono::seconds(10))) s->peer->close();
            else deliver(*s);
        }
        for (auto it = sessions.begin(); it != sessions.end();) {
            if (!(*it)->peer->ended()) { ++it; continue; }
            auto user = (*it)->user;
            it = sessions.erase(it);
            if (!user.empty() && !online(user)) presence(user, false);
        }
    }
};
ImServerFanLX::ImServerFanLX(const PlatformStateFanLX &s, const std::filesystem::path &dir,
    std::map<std::string, std::string> tokens, unsigned short p, const std::string &ip)
    : implFanLX(std::make_unique<Impl>(s, dir, std::move(tokens), p, ip)) {}
ImServerFanLX::~ImServerFanLX() = default;
unsigned short ImServerFanLX::port() const { return implFanLX->listener.port(); }
void ImServerFanLX::tick() { implFanLX->tick(); }
std::size_t ImServerFanLX::connectionsAccepted() const { return implFanLX->accepted; }

#include "network/ImServerFanLX.h"
#include "SocketIoFanLX.h"
#include "db/ChatSchemaStoreFanLX.h"
#include "db/SqliteChatRepositoryFanLX.h"
#include "db/SqliteMessageStoreFanLX.h"
#include "friend/IdentityResolverFanLX.h"
#include "network/MessageCodecFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include "persistence/DataDirectoryLockFanLX.h"
#include "persistence/NamedFilePublisherFanLX.h"
#include "persistence/PersistenceErrorFanLX.h"
#include "persistence/PlatformStateFanLX.h"
#include "network/FriendRequestBookFanLX.h"
#include "network/P2pProtocolFanLX.h"
#include "network/TemporaryChatQuotaFanLX.h"
#include "infra/UtcClockFanLX.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>

using namespace chatDbFanLX;

namespace {

// 服务器实例标识：同 host/port 换库后 messageId 会重新从 1 开始，
// 客户端必须能识别"这是另一个实例"，否则新旧历史会混在一起。
std::string loadOrCreateInstanceIdFanLX(const std::filesystem::path &directory,
                                        const std::string &requested) {
    const auto path = directory / "server-identity.json";
    if (std::filesystem::exists(path)) {
        const auto saved = nlohmann::json::parse(AtomicFileWriterFanLX::read(path));
        const auto instanceId = saved.value("instance_id", std::string());
        if (instanceId.empty())
            throw std::runtime_error("服务器实例标识文件损坏");
        if (!requested.empty() && requested != instanceId)
            throw std::runtime_error("服务器实例标识与既有数据不符：期望 " + instanceId);
        return instanceId;
    }
    std::string instanceId = requested;
    if (instanceId.empty()) {
        std::random_device random;
        instanceId = "srv-";
        for (int index = 0; index < 4; ++index)
            instanceId += std::to_string(random()) + "-";
    }
    NamedFilePublisherFanLX::publish(path, nlohmann::json{{"instance_id", instanceId}}.dump(2));
    return instanceId;
}

// 授权目录：只返回服务端确认过的联系人与群，客户端不推断资格。
// 受限于单帧文本上限：过大时明确报错（分块传输留待后续版本），不静默截断。
std::string buildDirectoryJsonFanLX(const nlohmann::json &payload) {
    auto text = payload.dump();
    if (text.size() > 4000)
        throw std::runtime_error("授权目录过大，暂不支持单帧传输");
    return text;
}

// 平台快照里的已知群键（服务端加载时已按快照重建，这里是权威来源）。
std::vector<GroupKeyFanLX> groupKeysFanLX(const PlatformStateFanLX &state) {
    return state.groups.groupKeys();
}

// 服务端内部的"通道标识"：<服务名>|<号码>。
//
// 为什么需要它：QQ 与微博按课程约定共用同一个号码，两条聊天通道的号码字符串**完全相同**。
// 消息库的收件人快照、投递游标与会话序号如果只用号码做键，QQ 的消息就会被投递到同号码的
// 微博连接上，两个通道的会话序号也会互相顶掉。因此服务端内部一律用带服务前缀的标识
// （账号 id 与群号都适用），只在生成出站帧时再剥回纯号码——客户端看到的一直是纯号码。
std::string channelIdFanLX(ServiceTypeFanLX service, const std::string &id) {
    return serviceNameFanLX(service) + "|" + id;
}
std::string plainIdFanLX(const std::string &channelId) {
    const auto bar = channelId.find('|');
    return bar == std::string::npos ? channelId : channelId.substr(bar + 1);
}

// 旧 JSON 后端的会话键同样按通道分开：<服务名>|<原会话键>。
// 群聊的 recipient 是群号（QQ 群 1001 与微信群 1001 是两回事），因此群号也带服务前缀。
std::string channelKeyFanLX(ServiceTypeFanLX service, const ChatMessageFanLX &message) {
    if (message.scope == "group")
        return channelIdFanLX(service, "g|" + message.recipient);
    return channelIdFanLX(service, "d|" + std::min(message.sender, message.recipient) + "|" +
                                       std::max(message.sender, message.recipient));
}

// 服务端会话键的解析结果：是否群聊 + 去掉通道前缀后的纯标识。
struct ChannelKeyPartsFanLX {
    bool group = false;
    std::string plain;
};

// 两条后端生成键的形状不同，这里统一认：
//   SQLite：conversationKeyFanLX(scope, target) → 群聊 "g|<通道>|<群号>"、单聊 "d|<通道>|…"；
//   旧 JSON：channelKeyFanLX                    → 群聊 "<通道>|g|<群号>"、单聊 "<通道>|d|…"。
ChannelKeyPartsFanLX parseChannelKeyFanLX(const std::string &key) {
    ChannelKeyPartsFanLX parts;
    if (key.rfind("g|", 0) == 0) { // g|<通道>|<群号>
        parts.group = true;
        parts.plain = plainIdFanLX(key.substr(2));
        return parts;
    }
    const auto body = plainIdFanLX(key); // 先剥掉通道前缀
    if (body.rfind("g|", 0) == 0) {
        parts.group = true;
        parts.plain = body.substr(2);
        return parts;
    }
    parts.plain = body;
    return parts;
}

} // namespace

struct ImServerFanLX::Impl {
    using Clock = std::chrono::steady_clock;
    struct Session {
        std::unique_ptr<TcpPeerFanLX> peer;
        std::string user;
        // 本会话所属服务（"QQ" / "Wechat"）：一条连接只服务一个服务，
        // 好友、群、在线状态与投递全部在该服务的分区里判定。
        ServiceTypeFanLX service = ServiceTypeFanLX::QQ;
        Clock::time_point last = Clock::now();
        std::uint64_t inFlight = 0;
        Clock::time_point sentAt = Clock::now();
        bool syncing = false;
        DeliveryCursorMapFanLX cursors; // 本次连接内已确认的会话进度
        // 客户端自己宣告的可达端点（ep:ipv4:port）。空表示只走中转。
        // 不用本连接的源地址替换它：源地址常常是 NAT 或回环，不等于对端能连上的地址。
        std::string directEndpoint;
        bool inFlightDirect = false;
    };
    enum class RouteStateFanLX { RelayOnly, DirectConnecting, DirectReady, DirectDegraded };
    struct PairRouteFanLX {
        RouteStateFanLX state = RouteStateFanLX::RelayOnly;
        std::string ticket;
        std::string connector;
        std::string acceptor;
        Clock::time_point offerAt = Clock::now();
        Clock::time_point retryAfter = Clock::time_point::min();
    };
    struct TicketFanLX {
        std::string service;
        std::string connector;
        std::string acceptor;
        Clock::time_point expires = Clock::now();
        bool connectorReady = false;
        bool acceptorReady = false;
    };
    const PlatformStateFanLX &state;
    std::filesystem::path directory;
    DataDirectoryLockFanLX lock;
    TcpListenerFanLX listener;
    OptionsFanLX options;
    // 好友申请簿与临时会话额度：服务端中转并落盘（两种聊天后端共用）。
    FriendRequestBookFanLX requests;
    TemporaryChatQuotaFanLX quota;
    std::string instanceId;
    // SQLite 后端（单一控制线程独占，不需要额外工作线程）。
    std::unique_ptr<SqliteMessageStoreFanLX> store;
    bool useSqlite = false;
    // 旧 JSON 后端。
    nlohmann::json db;
    std::vector<std::unique_ptr<Session>> sessions;
    std::size_t accepted = 0;
    std::size_t slowDisconnects = 0;
    std::map<std::string, PairRouteFanLX> routes;
    std::unordered_map<std::string, TicketFanLX> tickets;
    std::set<std::uint64_t> directTried;
    DirectRouteStatsFanLX routeStats;

    Impl(const PlatformStateFanLX &s, const std::filesystem::path &dir, unsigned short port,
         const std::string &ip, OptionsFanLX serverOptions)
        : state(s), directory(dir), lock(dir), listener(port, ip), options(std::move(serverOptions)),
          requests(directory / "friend-requests.json"),
          quota(directory / "temporary-chat.json", options.temporaryMessageLimit) {
        // 先于聊天后端加载：SQLite 分支会提前返回，申请簿与额度必须在那之前就绪。
        requests.load();
        quota.load();
        instanceId = loadOrCreateInstanceIdFanLX(directory, options.instanceId);
        useSqlite = !options.chatDatabasePath.empty();
        if (useSqlite) {
            store = std::make_unique<SqliteMessageStoreFanLX>(options.chatDatabasePath, instanceId);
            return;
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
            if (db.at("next").get<std::uint64_t>() <= previous)
                throw std::runtime_error("IM 消息编号损坏");
        } else {
            db = {{"schema", "fanlx-im-server-1"}, {"next", 1u}, {"messages", nlohmann::json::array()},
                  {"acks", nlohmann::json::object()}, {"seqs", nlohmann::json::object()}};
            socketIoFanLX::commit(directory, db, db);
        }
    }

    // 通道服务名 -> 枚举：聊天通道只有 QQ / 微信 / 微博（微X 是上层登录身份，不承载聊天）。
    static ServiceTypeFanLX parseChannelServiceFanLX(const std::string &name) {
        if (name == "QQ")
            return ServiceTypeFanLX::QQ;
        if (name == "Wechat")
            return ServiceTypeFanLX::Wechat;
        if (name == "Weibo")
            return ServiceTypeFanLX::Weibo;
        throw std::runtime_error("聊天通道只支持 QQ / 微信 / 微博（微X 是上层登录身份）");
    }
    bool subscribed(const Session &s, const std::string &u) const {
        return state.accounts.hasAccount(s.service, u) &&
               state.accounts.isAccountSubscribed(s.service, u);
    }
    bool friends(const Session &s, const std::string &a, const std::string &b) const {
        if (a == b || !subscribed(s, a) || !subscribed(s, b)) return false;
        IdentityResolverFanLX ids(state.accounts);
        // 好友只在本服务的分区里查：QQ 的连接看不到微信的好友，反之亦然。
        return state.friends.directory(s.service).contains(ids.accountKey(s.service, a),
                                                           ids.accountKey(s.service, b));
    }
    // 服务 → 群命名空间：只有 QQ 与微信有群（微博只有好友与单聊）。
    // 非群服务返回 nullopt 风格的 false，调用方据此给出"该服务没有群"。
    static bool modeOfFanLX(ServiceTypeFanLX service, GroupModeFanLX &out) {
        if (!groupCapableFanLX(service))
            return false;
        out = service == ServiceTypeFanLX::Wechat ? GroupModeFanLX::Wechat : GroupModeFanLX::QQ;
        return true;
    }
    bool member(const Session &s, const std::string &u, const std::string &group) const {
        GroupModeFanLX mode{};
        if (!modeOfFanLX(s.service, mode))
            return false; // 微博没有群：任何"群成员"判定都为假
        try {
            return subscribed(s, u) && state.groups.get({mode, group}).hasMember(
                state.accounts.accountByAccountId(s.service, u).userId);
        } catch (const std::out_of_range &) { return false; }
    }
    // 在线判定按 (服务, 账号)：不同服务里同名号码互不相干。
    bool online(ServiceTypeFanLX service, const std::string &u) const {
        for (const auto &other : sessions)
            if (other->service == service && other->user == u && !other->peer->ended()) return true;
        return false;
    }
    // 会话键一律经 channelKeyFanLX（按通道限定）：它同时服务 SQLite 与旧 JSON 两条路径，
    // 因此这里不再保留"不带通道"的会话键算法，避免两处规则漂移。
    static ScopeFanLX scopeOf(const ChatMessageFanLX &m) {
        return m.scope == "group" ? ScopeFanLX::Group : ScopeFanLX::Direct;
    }
    // 非阻塞写：先入队再按预算推进；队列满即判定为慢端。
    // 出站帧统一打上会话所属服务：客户端据此（以及连接本身）把消息归到正确的服务。
    void write(Session &s, const ChatMessageFanLX &m) {
        if (s.peer->ended())
            return;
        auto out = m;
        out.service = serviceNameFanLX(s.service);
        if (!s.peer->trySend(out)) {
            ++slowDisconnects;
            s.peer->close();
            return;
        }
        if (!s.peer->flushSend(8)) {
            s.peer->close();
        }
    }
    // 在线状态只在本服务的会话之间广播（QQ 的上线不会推给微信连接）。
    void presence(ServiceTypeFanLX service, const std::string &u, bool active) {
        auto event = socketIoFanLX::control("PRESENCE");
        event.sender = u;
        event.text = active ? "online" : "offline";
        for (auto &other : sessions)
            if (!other->user.empty() && other->service == service && friends(*other, other->user, u))
                write(*other, event);
    }
    // 群成员快照：发送时解析一次，历史不随后续成员变动改写。
    // 微博（没有群）恒返回空集合，由调用方报"该服务没有群"。
    std::set<std::string> groupRecipients(const Session &s, const std::string &group) const {
        std::set<std::string> recipients;
        GroupModeFanLX mode{};
        if (!modeOfFanLX(s.service, mode))
            return recipients;
        try {
            for (const auto &uid : state.groups.get({mode, group}).members()) {
                const auto *account = state.accounts.findAccount(uid, s.service);
                if (account && subscribed(s, account->accountId)) recipients.insert(account->accountId);
            }
        } catch (const std::out_of_range &) { /* 群不存在：返回空集合，由调用方报错 */ }
        return recipients;
    }
    // 授权目录：本服务的好友 + （有群的服务）本命名空间下已加入的群。
    // 三种服务各有各的目录，"QQ 的连接看到微信好友"在数据来源上就不成立；微博的群列表恒为空。
    void sendDirectory(Session &s) {
        nlohmann::json friendsList = nlohmann::json::array();
        for (const auto &id : friendIdsOf(s.service, s.user))
            friendsList.push_back(id);
        nlohmann::json groupsList = nlohmann::json::array();
        GroupModeFanLX mode{};
        if (subscribed(s, s.user) && modeOfFanLX(s.service, mode)) {
            const auto userId = state.accounts.accountByAccountId(s.service, s.user).userId;
            for (const auto &groupKey : groupKeysFanLX(state)) {
                if (groupKey.mode != mode)
                    continue; // 只列本服务命名空间的群
                try {
                    if (state.groups.get(groupKey).hasMember(userId))
                        groupsList.push_back(groupKey.id);
                } catch (const std::exception &) { /* 单个群异常不影响整体目录 */ }
            }
        }
        std::sort(groupsList.begin(), groupsList.end());
        groupsList.erase(std::unique(groupsList.begin(), groupsList.end()), groupsList.end());
        auto reply = socketIoFanLX::control("DIRECTORY_LIST");
        reply.text = buildDirectoryJsonFanLX(
            {{"schema", "fanlx-directory-1"},
             {"service", serviceNameFanLX(s.service)},
             {"friends", friendsList},
             {"groups", groupsList},
             // 非好友临时会话：服务端授权的对端 + 我方向对方还能发几条。
             {"temporary", temporaryListFanLX(s.service, s.user, false)}});
        write(s, reply);
    }

    // ---- 好友申请 / 临时会话（课设新增）----
    bool subscribedIn(ServiceTypeFanLX service, const std::string &u) const {
        return state.accounts.hasAccount(service, u) && state.accounts.isAccountSubscribed(service, u);
    }
    bool friendsIn(ServiceTypeFanLX service, const std::string &a, const std::string &b) const {
        if (a == b || !subscribedIn(service, a) || !subscribedIn(service, b)) return false;
        IdentityResolverFanLX ids(state.accounts);
        return state.friends.directory(service).contains(ids.accountKey(service, a),
                                                         ids.accountKey(service, b));
    }
    // user 在该服务的好友号码（已开通的），按号码排序。
    std::vector<std::string> friendIdsOf(ServiceTypeFanLX service, const std::string &user) const {
        std::vector<std::string> out;
        if (!subscribedIn(service, user))
            return out;
        IdentityResolverFanLX ids(state.accounts);
        for (const auto &peerKey : state.friends.directory(service).peers(ids.accountKey(service, user))) {
            // 好友键 → 归属用户 → 该用户的同服务账号：避免从拼接键里解析 ID。
            const auto peerUser = ids.userForAccount(peerKey);
            if (peerUser.empty())
                continue;
            const auto *account = state.accounts.findAccount(peerUser, service);
            if (!account || !state.accounts.isAccountSubscribed(service, account->accountId))
                continue;
            out.push_back(account->accountId);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    // 临时会话列表：只列"仍不是好友"的对端；detailed 时附带双向计数（给好友页显示）。
    // 最多 20 条，保证目录帧不超过单帧上限。
    nlohmann::json temporaryListFanLX(ServiceTypeFanLX service, const std::string &user, bool detailed) const {
        nlohmann::json list = nlohmann::json::array();
        const auto name = serviceNameFanLX(service);
        for (const auto &peer : quota.peersOf(name, user)) {
            if (list.size() >= 20)
                break;
            if (!subscribedIn(service, peer) || friendsIn(service, user, peer))
                continue;
            nlohmann::json row{{"peer", peer}, {"remaining", quota.remaining(name, user, peer)}};
            if (detailed) {
                row["sent"] = quota.sent(name, user, peer);
                row["received"] = quota.sent(name, peer, user);
            }
            list.push_back(std::move(row));
        }
        return list;
    }
    // 给某账号在该服务的全部在线会话推一条好友动态，并刷新它们的授权目录。
    void notifyFriendFanLX(ServiceTypeFanLX service, const std::string &user, const std::string &from,
                           const std::string &text) {
        for (auto &other : sessions) {
            if (other->user != user || other->service != service || other->peer->ended())
                continue;
            if (!text.empty()) {
                auto notice = socketIoFanLX::control("FRIEND_NOTICE");
                notice.sender = from;
                notice.text = text;
                write(*other, notice);
            }
            refreshOneDirectoryFanLX(*other);
        }
    }
    void refreshOneDirectoryFanLX(Session &s) {
        if (s.user.empty() || s.peer->ended())
            return;
        try {
            sendDirectory(s);
        } catch (const std::exception &) { /* 目录过大等：不影响本次操作结果，下次登录再取 */ }
    }
    void refreshDirectoriesFanLX(ServiceTypeFanLX service, const std::string &user) {
        notifyFriendFanLX(service, user, std::string(), std::string());
    }
    // 两人成为好友：先写账号库（权威副本），成功后再结清申请与临时额度。
    void becomeFriendsFanLX(ServiceTypeFanLX service, const std::string &a, const std::string &b) {
        if (!options.updateFriendship)
            throw std::runtime_error("本服务端未启用好友申请落地（请用 --im-server 启动）");
        const auto failure = options.updateFriendship(serviceNameFanLX(service), a, b, true);
        if (!failure.empty())
            throw std::runtime_error(failure);
        if (options.refreshAccounts)
            options.refreshAccounts();
        quota.clearPair(serviceNameFanLX(service), a, b);
        quota.save();
    }

    // 非好友单聊的准入：对端必须是本服务的有效账号，且本方向临时额度未用完。
    // 返回 true 表示这是一条临时会话消息（成功入库后要计数）。
    bool admitTemporaryFanLX(const Session &s, const ChatMessageFanLX &m) const {
        if (friends(s, s.user, m.recipient))
            return false;
        if (m.recipient == s.user || !subscribed(s, m.recipient))
            throw std::runtime_error("单聊目标不存在或未开通该服务");
        const auto name = serviceNameFanLX(s.service);
        if (!quota.consumed(name, s.user, m.recipient, m.requestId) &&
            quota.remaining(name, s.user, m.recipient) <= 0)
            throw std::runtime_error("临时会话已达上限（每方向 " + std::to_string(quota.limit()) +
                                     " 条）：请先发送好友申请并等待对方同意");
        return true;
    }
    // 临时消息入库后计数（按 requestId 幂等），并让双方的目录显示最新剩余条数。
    void recordTemporaryFanLX(Session &s, const ChatMessageFanLX &m) {
        if (!quota.consume(serviceNameFanLX(s.service), s.user, m.recipient, m.requestId, ::utcNowFanLX()))
            return; // 重放：已经计过
        quota.save();
        refreshDirectoriesFanLX(s.service, m.recipient);
        refreshOneDirectoryFanLX(s);
    }

    // FRIEND_RESULT 的应答体：本人的待处理申请、好友与临时会话；超出单帧上限时逐步裁剪。
    std::string friendResultFanLX(ServiceTypeFanLX service, const std::string &self,
                                  const std::string &message) const {
        const auto name = serviceNameFanLX(service);
        const auto rowsOf = [](const std::vector<FriendRequestFanLX> &list, bool incoming) {
            nlohmann::json rows = nlohmann::json::array();
            for (const auto &request : list)
                rows.push_back({{"peer", incoming ? request.from : request.to},
                                {"note", request.note},
                                {"time", request.requestedAtUtc}});
            return rows;
        };
        nlohmann::json reply{{"ok", true},
                             {"message", message},
                             {"service", name},
                             {"incoming", rowsOf(requests.incoming(name, self), true)},
                             {"outgoing", rowsOf(requests.outgoing(name, self), false)},
                             {"friends", friendIdsOf(service, self)},
                             {"temporary", temporaryListFanLX(service, self, true)},
                             {"limit", quota.limit()},
                             {"friendsComplete", true}};
        // 单帧文本上限约 4096：先去掉附言，再从尾部裁剪列表（目录完整版仍走 DIRECTORY_LIST）。
        // 好友列表一旦被裁剪就标记为不完整：客户端只能据它补登，不能据它删除本机关系。
        const char *lists[] = {"friends", "temporary", "outgoing", "incoming"};
        if (reply.dump().size() > 3900) {
            for (auto &row : reply["incoming"]) row["note"] = "";
            for (auto &row : reply["outgoing"]) row["note"] = "";
        }
        for (const char *key : lists) {
            while (reply.dump().size() > 3900 && !reply[key].empty()) {
                reply[key].erase(reply[key].size() - 1);
                if (std::string(key) == "friends")
                    reply["friendsComplete"] = false;
            }
        }
        return reply.dump();
    }

    // 跨服务添加：把"本人在 viaService 的好友 viaPeer"换算成他在 service 的号码。
    // 换算在服务端做（它有全部账号的权威副本），并且先确认这段好友关系确实存在，
    // 不能拿任意号码冒充"另一个服务的好友"。
    std::string resolveViaFanLX(ServiceTypeFanLX service, const std::string &self,
                                const std::string &viaServiceName, const std::string &viaPeer) const {
        ServiceTypeFanLX via{};
        if (!serviceFromNameFanLX(viaServiceName, via) || !chatCapableFanLX(via))
            throw std::runtime_error("来源服务只能是 QQ / 微信 / 微博");
        if (via == service)
            throw std::runtime_error("来源服务与当前服务相同：直接按号码申请即可");
        const auto &selfUser = state.accounts.accountByAccountId(service, self).userId;
        const auto *selfVia = state.accounts.findAccount(selfUser, via);
        if (!selfVia || !subscribedIn(via, selfVia->accountId))
            throw std::runtime_error("本人尚未开通" + serviceDisplayNameFanLX(via));
        if (!friendsIn(via, selfVia->accountId, viaPeer))
            throw std::runtime_error(viaPeer + " 不是本人在" + serviceDisplayNameFanLX(via) + "的好友");
        const auto &peerUser = state.accounts.accountByAccountId(via, viaPeer).userId;
        const auto *target = state.accounts.findAccount(peerUser, service);
        if (!target || !subscribedIn(service, target->accountId))
            throw std::runtime_error("对方还没有开通" + serviceDisplayNameFanLX(service) +
                                     "：请让对方先开通，再依据" + serviceDisplayNameFanLX(via) + "好友添加");
        return target->accountId;
    }

    // FRIEND 请求：凭据随报文（与 LOOKUP 一样不建立会话），sender 为对端号码。
    void handleFriendFanLX(Session &s, const ChatMessageFanLX &m) {
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(m.text);
        } catch (const std::exception &) {
            throw std::runtime_error("好友请求需要本人凭据");
        }
        if (!body.is_object())
            throw std::runtime_error("好友请求格式错误");
        const auto service = parseChannelServiceFanLX(m.service);
        const auto self = body.value("self", std::string());
        const auto password = body.value("password", std::string());
        const auto op = body.value("op", std::string());
        const auto note = body.value("note", std::string());
        const auto via = body.value("via", std::string());
        std::string peer = m.sender;
        // 先读穿账号库：别的客户端刚注册 / 刚开通的账号也要认得。
        if (options.refreshAccounts)
            options.refreshAccounts();
        if (self.empty() || !state.accounts.verifyPassword(service, self, password))
            throw std::runtime_error("用户名或密码错误（好友操作需要本人当前服务的凭据）");
        if (!subscribedIn(service, self))
            throw std::runtime_error("本人尚未开通" + serviceDisplayNameFanLX(service));
        // 跨服务添加：sender 是本人在 via 服务里的好友号码，换算成他在当前服务的号码后按普通申请处理。
        if (!via.empty()) {
            if (op != "request")
                throw std::runtime_error("只有发起申请可以依据另一个服务的好友");
            peer = resolveViaFanLX(service, self, via, peer);
        }
        const auto name = serviceNameFanLX(service);
        const auto now = ::utcNowFanLX();
        const auto peerValid = [&]() {
            if (peer.empty() || peer == self)
                throw std::runtime_error("请填写对方号码（不能是自己）");
            if (!subscribedIn(service, peer))
                throw std::runtime_error("服务端没有这个号码，或对方未开通" + serviceDisplayNameFanLX(service));
        };
        std::string message;
        if (op == "list") {
            message = "已刷新";
        } else if (op == "request") {
            peerValid();
            if (friendsIn(service, self, peer))
                throw std::runtime_error("你们已经是好友");
            if (requests.hasPending(name, peer, self)) {
                // 对方早已向我申请：互相申请即视为同意，直接成为好友。
                becomeFriendsFanLX(service, self, peer);
                requests.resolvePair(name, self, peer, now);
                requests.save();
                message = "对方也向你发过申请，已直接成为好友";
                notifyFriendFanLX(service, peer, self, self + " 已同意你的好友申请，你们已成为好友");
            } else {
                const auto outcome = requests.submit(name, self, peer, note, now);
                requests.save();
                message = outcome == FriendRequestBookFanLX::SubmitOutcomeFanLX::Refreshed
                              ? "已重新发送好友申请，等待对方同意"
                              : "好友申请已发送，等待对方同意";
                notifyFriendFanLX(service, peer, self,
                                  self + " 请求添加你为好友" + (note.empty() ? std::string() : "：" + note));
            }
        } else if (op == "accept") {
            peerValid();
            if (!requests.hasPending(name, peer, self))
                throw std::runtime_error("没有来自 " + peer + " 的待处理好友申请");
            becomeFriendsFanLX(service, self, peer);
            requests.resolvePair(name, self, peer, now);
            requests.save();
            message = "已同意，你们已成为好友";
            notifyFriendFanLX(service, peer, self, self + " 已同意你的好友申请，你们已成为好友");
        } else if (op == "reject") {
            peerValid();
            requests.respond(name, peer, self, false, now);
            requests.save();
            message = "已拒绝该好友申请";
            notifyFriendFanLX(service, peer, self, self + " 拒绝了你的好友申请");
        } else if (op == "cancel") {
            peerValid();
            requests.cancel(name, self, peer, now);
            requests.save();
            message = "已撤回好友申请";
            notifyFriendFanLX(service, peer, self, self + " 撤回了好友申请");
        } else if (op == "remove") {
            peerValid();
            if (!friendsIn(service, self, peer))
                throw std::runtime_error("对方不是你的好友");
            if (!options.updateFriendship)
                throw std::runtime_error("本服务端未启用好友申请落地（请用 --im-server 启动）");
            const auto failure = options.updateFriendship(name, self, peer, false);
            if (!failure.empty())
                throw std::runtime_error(failure);
            if (options.refreshAccounts)
                options.refreshAccounts();
            message = "已删除好友";
            notifyFriendFanLX(service, peer, self, self + " 已将你从好友中移除");
        } else {
            throw std::runtime_error("未知的好友操作：" + op);
        }
        // 本人的在线聊天通道（若有）同步刷新目录：新好友立刻可聊、被删好友立刻变回临时会话。
        if (op != "list")
            refreshDirectoriesFanLX(service, self);
        auto answer = socketIoFanLX::control("FRIEND_RESULT");
        answer.sender = peer;
        auto result = nlohmann::json::parse(friendResultFanLX(service, self, message));
        result["peer"] = peer; // 跨服务添加时客户端据此得知换算后的号码
        answer.text = result.dump();
        write(s, answer);
    }

    void handle(Session &s, ChatMessageFanLX m) {
        // 账号登记：账号层的"自举"消息，未认证也能发。服务端把它写进**持久化账号库**
        // （OptionsFanLX::registerAccount），因此注册之后不需要重启服务端。
        if (m.type == "REGISTER") {
            if (!s.user.empty()) throw std::runtime_error("已登录的连接请勿重复登记账号");
            if (!options.registerAccount)
                throw std::runtime_error("本服务端未启用账号登记（请用 --im-server 启动）");
            if (m.sender.empty() || m.text.size() > 2048)
                throw std::runtime_error("登记请求缺少微X号或资料过长");
            nlohmann::json profile;
            try {
                profile = nlohmann::json::parse(m.text);
            } catch (const std::exception &) {
                throw std::runtime_error("登记请求不是合法 JSON");
            }
            const auto read = [&profile](const char *key) {
                return profile.contains(key) && profile.at(key).is_string()
                           ? profile.at(key).get<std::string>()
                           : std::string();
            };
            const auto password = read("password");
            if (password.empty()) throw std::runtime_error("登记请求缺少密码");
            // 客户端本机已开通的下级服务：一并登记到服务端，否则登录仍会撞上"未开通"。
            std::vector<std::pair<std::string, std::string>> services;
            if (profile.contains("services") && profile.at("services").is_array()) {
                for (const auto &entry : profile.at("services")) {
                    if (!entry.is_object() || !entry.contains("service") || !entry.contains("accountId"))
                        continue;
                    if (!entry.at("service").is_string() || !entry.at("accountId").is_string())
                        continue;
                    services.emplace_back(entry.at("service").get<std::string>(),
                                          entry.at("accountId").get<std::string>());
                }
            }
            // 好友关系不再随登记"补登"：添加好友必须走 FRIEND 申请 → 对方同意（服务端中转）。
            // 报文里即使带了 friends 字段也一律忽略，避免绕过对方同意这一步。
            const std::vector<std::pair<std::string, std::string>> friends;
            const auto failure = options.registerAccount(m.sender, password, read("nickname"),
                                                        read("birthDate"), read("location"), services,
                                                        friends);
            if (!failure.empty())
                throw std::runtime_error(failure);
            // 登记成功后立刻读穿一次：内存里的账号库与持久化存储保持一致。
            if (options.refreshAccounts)
                options.refreshAccounts();
            auto ok = socketIoFanLX::control("REGISTER_OK");
            ok.sender = m.sender;
            ok.text = "微X账号已登记到服务端（持久化存储），可直接登录";
            write(s, ok);
            return;
        }
        // 账号查询：添加好友前用。本机领域库往往不认识"别人在别的客户端注册的账号"，
        // 因此由服务端（谁存在、谁开通了什么的权威副本）回答一个最小事实。
        // 为什么不用"先 LOGIN 再查询"：服务端有单设备策略，而查询方此刻**正连着自己的聊天通道**，
        // 再登录一次会被判成"账号已有在线会话"。所以这里改成"查询报文里带上本人账号与密码"，
        // 服务端只做一次凭据校验（不建立会话）：既不与单设备策略打架，也不给匿名枚举留口子。
        if (m.type == "LOOKUP") {
            if (m.sender.empty()) throw std::runtime_error("查询账号缺少号码");
            nlohmann::json proof;
            try {
                proof = nlohmann::json::parse(m.text);
            } catch (const std::exception &) {
                throw std::runtime_error("查询账号需要本人凭据");
            }
            const auto service = parseChannelServiceFanLX(m.service);
            const auto self = proof.value("self", std::string());
            const auto password = proof.value("password", std::string());
            if (self.empty() || !state.accounts.verifyPassword(service, self, password))
                throw std::runtime_error("用户名或密码错误（查询账号需要本人当前服务的凭据）");
            if (options.refreshAccounts)
                options.refreshAccounts();
            nlohmann::json reply{{"exists", false}};
            if (state.accounts.hasAccount(service, m.sender)) {
                const auto &account = state.accounts.accountByAccountId(service, m.sender);
                reply["exists"] = true;
                reply["service"] = serviceNameFanLX(service);
                reply["accountId"] = m.sender;
                reply["userId"] = account.userId;
                reply["nickname"] = state.accounts.profile(account.userId).nickname;
                reply["subscribed"] = state.accounts.isAccountSubscribed(service, m.sender);
            }
            auto answer = socketIoFanLX::control("LOOKUP_RESULT");
            answer.sender = m.sender;
            answer.text = reply.dump();
            write(s, answer);
            return;
        }
        // 好友申请（服务端中转）：发起 / 同意 / 拒绝 / 撤回 / 删除 / 列表。
        // 与 LOOKUP 同样是一次性凭据请求，不与聊天通道的单设备策略冲突。
        if (m.type == "FRIEND") {
            if (!s.user.empty()) throw std::runtime_error("好友操作请使用独立的短连接");
            handleFriendFanLX(s, m);
            return;
        }
        if (m.type == "LOGIN") {
            if (!s.user.empty()) throw std::runtime_error("已经登录");
            // 认证前先"读穿"账号库：权威副本在持久化存储里，
            // 服务端不把"启动那一刻"的快照当成全部事实（见 OptionsFanLX::refreshAccounts）。
            if (options.refreshAccounts)
                options.refreshAccounts();
            // 用户名 + 密码认证：sender 是该服务的账号，text 是用户自选密码，service 是通道服务。
            // 账号库就是领域状态里的 accounts（注册/开通时写入，随快照持久化），
            // 因此没有令牌表、也没有预共享密钥——服务端不再单独持有任何口令副本。
            const auto service = parseChannelServiceFanLX(m.service);
            s.service = service;
            // 密码比对走 verifyPassword，账号不存在与密码错误回同一句话。
            if (!state.accounts.verifyPassword(s.service, m.sender, m.text)) {
                // 但如果这个号码其实是一个"密码正确、已在服务端登记"的微X号，那就不是密码问题：
                // 服务端还没有为它开通这个服务账号（客户端"注册了、也开通了，却登不上"最常见的原因）。
                // 这里只对"知道微X号且知道密码"的人多说一句，不额外泄露信息。
                if (s.service != ServiceTypeFanLX::WeiX &&
                    state.accounts.verifyPassword(ServiceTypeFanLX::WeiX, m.sender, m.text))
                    throw std::runtime_error("服务端还没有为微X号 " + m.sender + " 开通 " +
                                             serviceDisplayNameFanLX(s.service) +
                                             " 服务（客户端会带着已开通服务重新登记，服务端读穿存储，无需重启）");
                throw std::runtime_error("用户名或密码错误");
            }
            // 密码正确但该服务没开通：与"密码错"分开说，便于演示时定位。
            if (!subscribed(s, m.sender))
                throw std::runtime_error("该账号未开通 " + serviceDisplayNameFanLX(s.service) + " 服务");
            // 单设备策略：拒绝第二个活跃会话；旧会话失效后重试，避免两端相互踢线。
            if (online(s.service, m.sender)) throw std::runtime_error("账号已有在线会话");
            s.user = m.sender;
            // LOGIN_OK 携带服务器实例标识：客户端据此校验本地聊天库归属。
            auto ok = socketIoFanLX::control("LOGIN_OK");
            ok.requestId = instanceId;
            ok.protocolVersion = imProtocolVersionCurrentFanLX;
            write(s, ok);
            presence(s.service, s.user, true);
            // 好友在线快照：从"本服务的好友分区"出发，而不是从"账号库里所有人"出发。
            // 好友键 → 归属用户 → 该用户的同服务账号（不从拼接键里解析 ID）。
            IdentityResolverFanLX ids(state.accounts);
            for (const auto &peerKey :
                 state.friends.directory(s.service).peers(ids.accountKey(s.service, s.user))) {
                const auto peerUser = ids.userForAccount(peerKey);
                if (peerUser.empty())
                    continue;
                const auto *account = state.accounts.findAccount(peerUser, s.service);
                // friends() 同时确认双方账号都已存在且已开通本服务，与广播侧的口径保持一致。
                if (!account || !friends(s, s.user, account->accountId))
                    continue;
                auto e = socketIoFanLX::control("PRESENCE");
                e.sender = account->accountId;
                e.text = online(s.service, account->accountId) ? "online" : "offline";
                write(s, e);
            }
            return;
        }
        if (s.user.empty()) throw std::runtime_error("请先认证");
        if (!subscribed(s, s.user)) throw std::runtime_error("账号服务已停用");
        if (!m.sender.empty() && m.sender != s.user) throw std::runtime_error("禁止伪造发送者");
        m.sender = s.user;
        if (m.type == "PING") {
            write(s, socketIoFanLX::control("PONG"));
            return;
        }
        if (m.type == "DIRECTORY") { sendDirectory(s); return; }
        if (m.type == "QUERY") {
            if (!friends(s, s.user, m.recipient)) throw std::runtime_error("只能查询好友在线状态");
            auto reply = socketIoFanLX::control("PRESENCE");
            reply.sender = m.recipient;
            reply.text = online(s.service, m.recipient) ? "online" : "offline";
            write(s, reply);
            return;
        }
        if (m.type == "SYNC") { s.syncing = true; loadCursors(s); return; }
        if (m.type == "ACK") { handleAck(s, m); return; }
        if (p2pFanLX::isDirectTypeFanLX(m.type)) { handleDirect(s, m); return; }
        if (m.type != "SEND") throw std::runtime_error("客户端请求类型非法");
        handleSend(s, std::move(m));
    }

    void loadCursors(Session &s) {
        // 已确认进度以数据库为准：重连或服务端重启后必须按库内游标继续，而不是从零重放。
        // 游标按"通道 + 账号"存放，因此同号码的 QQ 与微博两条通道各有各的进度。
        if (useSqlite)
            s.cursors = store->deliveryCursorsFor(channelIdFanLX(s.service, s.user));
    }

    void handleAck(Session &s, const ChatMessageFanLX &m) {
        if (m.messageId == 0 || m.messageId != s.inFlight)
            throw std::runtime_error("ACK 不是当前投递消息");
        // 会话序号是推进投递游标的唯一依据。缺失时必须明确拒绝，而不是让该会话永久停在
        // "在途"状态——曾经因为客户端 ACK 不带序号，服务端推进游标失败后静默卡住，
        // 导致同一会话的后续消息全部无法投递（往返消息超时）。
        if (m.conversationSeq == 0)
            throw std::runtime_error("ACK 缺少会话序号");
        if (useSqlite) {
            // 会话键优先取自服务器内部的在途登记；登记缺失时按键值从库中回查
            // （消息编号本身已用 s.inFlight 校验过，客户端无法借此伪造其它会话的游标）。
            auto key = inFlightConversation(s);
            if (key.empty()) {
                const auto stored = store->findById(m.messageId);
                if (stored)
                    key = stored->conversationKey;
            }
            if (key.empty())
                throw std::runtime_error("ACK 对应的投递记录缺失");
            store->advanceDeliveryCursor(channelIdFanLX(s.service, s.user), key,
                                         static_cast<std::int64_t>(m.conversationSeq));
            s.cursors[key] = static_cast<std::int64_t>(m.conversationSeq);
        } else {
            auto next = db;
            for (const auto &row : db.at("messages")) {
                const auto delivered = socketIoFanLX::message(row.at("message"));
                if (delivered.messageId != m.messageId) continue;
                next["acks"][channelIdFanLX(s.service, s.user)][channelKeyFanLX(s.service, delivered)] =
                    delivered.conversationSeq;
                break;
            }
            socketIoFanLX::commit(directory, db, std::move(next));
        }
        directTried.erase(s.inFlight);
        inFlightKeys.erase(s.inFlight);
        s.inFlight = 0;
        s.inFlightDirect = false;
    }

    // 当前在途投递所属会话（服务器内部登记，客户端不能指定）。
    std::map<std::uint64_t, std::string> inFlightKeys;
    std::string inFlightConversation(Session &s) const {
        const auto found = inFlightKeys.find(s.inFlight);
        return found == inFlightKeys.end() ? std::string() : found->second;
    }

    // 单聊路由：RelayOnly → DirectConnecting → DirectReady，失败进入 DirectDegraded。
    // 消息在 DirectReady 时先发 P2P_ARM（text 只有哈希，正文不进服务器帧），
    // 接收方落库后仍走原来的 ACK。3 秒没有 ACK 就用同一 messageId 改发 DELIVER（RelayFallback）。
    // 群、离线、非好友、临时会话，以及发送方自己的历史回显，始终 DELIVER。
    Session *findSession(ServiceTypeFanLX service, const std::string &user) {
        for (auto &other : sessions)
            if (other->service == service && other->user == user && !other->peer->ended())
                return other.get();
        return nullptr;
    }
    std::string pairKeyFanLX(ServiceTypeFanLX service, const std::string &a, const std::string &b) const {
        return serviceNameFanLX(service) + "\n" + std::min(a, b) + "\n" + std::max(a, b);
    }
    static std::string makeTicketFanLX() {
        std::random_device random;
        std::mt19937_64 generator(random());
        std::ostringstream out;
        out << std::hex << std::setfill('0') << std::setw(16) << generator() << std::setw(16) << generator();
        return out.str();
    }
    void degradeRoute(PairRouteFanLX &route, bool countFail) {
        if (route.state != RouteStateFanLX::DirectConnecting && route.state != RouteStateFanLX::DirectReady)
            return;
        route.state = RouteStateFanLX::DirectDegraded;
        route.retryAfter = Clock::now() + std::chrono::seconds(8);
        route.ticket.clear();
        if (countFail)
            ++routeStats.directFail;
    }
    void degradeUser(ServiceTypeFanLX service, const std::string &user, bool countFail) {
        const auto prefix = serviceNameFanLX(service) + "\n";
        for (auto &entry : routes) {
            if (entry.first.rfind(prefix, 0) != 0)
                continue;
            const auto rest = entry.first.substr(prefix.size());
            const auto split = rest.find('\n');
            if (split == std::string::npos)
                continue;
            const auto lo = rest.substr(0, split);
            const auto hi = rest.substr(split + 1);
            if (lo == user || hi == user)
                degradeRoute(entry.second, countFail);
        }
    }
    void sweepRoutes() {
        for (auto &entry : routes) {
            if (entry.second.state == RouteStateFanLX::DirectConnecting &&
                Clock::now() - entry.second.offerAt > std::chrono::seconds(5)) {
                degradeRoute(entry.second, true);
                std::cerr << "fanlx-p2p route=DirectDegraded reason=timeout peers=" << entry.second.connector
                          << "/" << entry.second.acceptor << '\n';
            }
        }
        for (auto it = tickets.begin(); it != tickets.end();) {
            if (Clock::now() >= it->second.expires && !it->second.connectorReady)
                it = tickets.erase(it);
            else
                ++it;
        }
    }
    void considerOffers(Session &s) {
        if (s.user.empty() || s.directEndpoint.empty())
            return;
        for (const auto &id : friendIdsOf(s.service, s.user)) {
            auto *peer = findSession(s.service, id);
            if (!peer || peer->directEndpoint.empty() || !friends(s, s.user, id))
                continue;
            auto &route = routes[pairKeyFanLX(s.service, s.user, id)];
            if (route.state == RouteStateFanLX::DirectReady || route.state == RouteStateFanLX::DirectConnecting)
                continue;
            if (route.state == RouteStateFanLX::DirectDegraded && Clock::now() < route.retryAfter)
                continue;
            const bool selfConnects = s.user < id;
            route.connector = selfConnects ? s.user : id;
            route.acceptor = selfConnects ? id : s.user;
            route.ticket = makeTicketFanLX();
            route.state = RouteStateFanLX::DirectConnecting;
            route.offerAt = Clock::now();
            TicketFanLX ticket;
            ticket.service = serviceNameFanLX(s.service);
            ticket.connector = route.connector;
            ticket.acceptor = route.acceptor;
            ticket.expires = Clock::now() + std::chrono::seconds(30);
            tickets[route.ticket] = ticket;
            auto *connector = selfConnects ? &s : peer;
            auto *acceptor = selfConnects ? peer : &s;
            auto offer = socketIoFanLX::control(p2pFanLX::offerTypeFanLX);
            offer.sender = connector->user;
            offer.recipient = acceptor->user;
            offer.requestId = route.ticket;
            offer.text = acceptor->directEndpoint;
            write(*connector, offer);
            auto expect = socketIoFanLX::control(p2pFanLX::expectTypeFanLX);
            expect.sender = acceptor->user;
            expect.recipient = connector->user;
            expect.requestId = route.ticket;
            expect.text = connector->directEndpoint;
            write(*acceptor, expect);
            std::cerr << "fanlx-p2p route=DirectConnecting service=" << ticket.service << " peers="
                      << route.connector << "/" << route.acceptor << '\n';
        }
    }
    void handleDirect(Session &s, const ChatMessageFanLX &m) {
        if (m.type == p2pFanLX::helloTypeFanLX) {
            std::string ip;
            unsigned short port = 0;
            std::string canonical;
            if (!m.text.empty() && p2pFanLX::parseEndpointFanLX(m.text, ip, port))
                canonical = p2pFanLX::formatEndpointFanLX(ip, port);
            else if (!m.text.empty())
                std::cerr << "fanlx-p2p hello-rejected account=" << s.user << '\n';
            if (canonical != s.directEndpoint)
                degradeUser(s.service, s.user, false);
            s.directEndpoint = canonical;
            if (!canonical.empty())
                considerOffers(s);
            return;
        }
        if (m.type == p2pFanLX::readyTypeFanLX) {
            auto found = tickets.find(m.requestId);
            if (found == tickets.end() || Clock::now() >= found->second.expires ||
                found->second.service != serviceNameFanLX(s.service))
                return;
            auto &ticket = found->second;
            if (s.user == ticket.connector)
                ticket.connectorReady = true;
            else if (s.user == ticket.acceptor)
                ticket.acceptorReady = true;
            else
                return;
            if (!ticket.connectorReady || !ticket.acceptorReady)
                return;
            auto &route = routes[pairKeyFanLX(s.service, ticket.connector, ticket.acceptor)];
            if (route.ticket != m.requestId || route.state == RouteStateFanLX::DirectReady)
                return;
            route.state = RouteStateFanLX::DirectReady;
            ++routeStats.directSuccess;
            std::cerr << "fanlx-p2p route=DirectReady service=" << ticket.service << " peers="
                      << ticket.connector << "/" << ticket.acceptor << '\n';
            return;
        }
        if (m.type == p2pFanLX::failTypeFanLX) {
            const auto reason = m.text == "timeout" || m.text == "connect" || m.text == "rejected" ||
                                        m.text == "closed"
                                    ? m.text
                                    : std::string("fail");
            auto found = tickets.find(m.requestId);
            if (found != tickets.end()) {
                if (s.user != found->second.connector && s.user != found->second.acceptor)
                    return;
                const auto service = parseChannelServiceFanLX(found->second.service);
                auto route = routes.find(pairKeyFanLX(service, found->second.connector, found->second.acceptor));
                if (route != routes.end())
                    degradeRoute(route->second, true);
                tickets.erase(found);
            } else if (!m.recipient.empty()) {
                auto route = routes.find(pairKeyFanLX(s.service, s.user, m.recipient));
                if (route != routes.end())
                    degradeRoute(route->second, true);
            }
            std::cerr << "fanlx-p2p route=DirectDegraded reason=" << reason << '\n';
            return;
        }
    }
    bool canArm(const Session &recipient, const ChatMessageFanLX &out) const {
        if (out.scope != "direct" || out.messageId == 0 || recipient.user.empty())
            return false;
        if (recipient.user == out.sender || recipient.user != out.recipient)
            return false;
        if (recipient.directEndpoint.empty() || directTried.count(out.messageId))
            return false;
        if (!friends(recipient, out.sender, out.recipient))
            return false;
        const Session *sender = nullptr;
        for (const auto &other : sessions)
            if (other->service == recipient.service && other->user == out.sender && !other->peer->ended())
                sender = other.get();
        if (!sender || sender->directEndpoint.empty())
            return false;
        const auto found = routes.find(pairKeyFanLX(recipient.service, out.sender, out.recipient));
        return found != routes.end() && found->second.state == RouteStateFanLX::DirectReady;
    }
    void ship(Session &s, ChatMessageFanLX out, const std::string &conversationKey, bool group) {
        const auto body = out.text;
        if (!group && canArm(s, out)) {
            out.type = p2pFanLX::armTypeFanLX;
            out.text = p2pFanLX::hashBodyFanLX(body);
            s.inFlightDirect = true;
            directTried.insert(out.messageId);
            ++routeStats.p2pMessages;
            std::cerr << "fanlx-p2p deliver route=p2p messageId=" << out.messageId << '\n';
            if (auto *sender = findSession(s.service, out.sender)) {
                auto push = out;
                push.type = p2pFanLX::pushTypeFanLX;
                write(*sender, push);
            }
        } else {
            out.type = "DELIVER";
            out.text = body;
            s.inFlightDirect = false;
            ++routeStats.relayMessages;
            if (group || out.scope == "group")
                ++routeStats.groupRelayMessages;
            else {
                ++routeStats.relayDirectMessages;
                if (s.user != out.sender)
                    ++routeStats.peerRelayBodies;
            }
            std::cerr << "fanlx-p2p deliver route=relay messageId=" << out.messageId << '\n';
        }
        if (!out.messageId)
            return;
        s.inFlight = out.messageId;
        s.sentAt = Clock::now();
        inFlightKeys[s.inFlight] = conversationKey;
        write(s, out);
    }
    void relayArmed(Session &s) {
        ChatMessageFanLX out;
        bool found = false;
        if (useSqlite) {
            const auto row = store->findById(static_cast<std::int64_t>(s.inFlight));
            if (row && row->messageId) {
                out = socketIoFanLX::control("DELIVER");
                out.sender = plainIdFanLX(row->sender);
                out.recipient = plainIdFanLX(row->recipient);
                out.scope = "direct";
                out.messageId = s.inFlight;
                out.conversationSeq = static_cast<std::uint64_t>(row->conversationSequence);
                out.requestId = row->requestId;
                out.text = row->body;
                out.acceptedAtUtc = row->acceptedAtUtc;
                out.protocolVersion = imProtocolVersionV3FanLX;
                found = true;
            }
        } else {
            for (const auto &row : db.at("messages")) {
                auto stored = socketIoFanLX::message(row.at("message"));
                if (stored.messageId != s.inFlight)
                    continue;
                out = stored;
                out.type = "DELIVER";
                found = true;
                break;
            }
        }
        s.inFlightDirect = false;
        if (!found)
            return;
        ++routeStats.fallbacks;
        ++routeStats.relayMessages;
        ++routeStats.relayDirectMessages;
        ++routeStats.peerRelayBodies;
        write(s, out);
        auto route = routes.find(pairKeyFanLX(s.service, out.sender, out.recipient));
        if (route != routes.end())
            degradeRoute(route->second, true);
        std::cerr << "fanlx-p2p deliver route=RelayFallback messageId=" << out.messageId << '\n';
    }

    void handleSend(Session &s, ChatMessageFanLX m) {
        if (useSqlite) {
            ServerAcceptRequestFanLX request;
            request.scope = scopeOf(m);
            // 进库的一律是"通道标识"（服务名|号码）：QQ 与微博共用号码时两条通道的
            // 收件人、会话键与序号必须互不相干。出站帧仍然用纯号码（见 deliver）。
            request.sender = channelIdFanLX(s.service, s.user);
            request.target = channelIdFanLX(s.service, m.recipient);
            request.requestId = m.requestId;
            request.body = m.text;
            bool temporary = false;
            if (m.scope == "direct") {
                // 非好友允许临时会话（每方向限额），好友不受限。
                temporary = admitTemporaryFanLX(s, m);
                request.recipients = {request.sender, request.target};
            } else {
                if (!groupCapableFanLX(s.service))
                    throw std::runtime_error(serviceDisplayNameFanLX(s.service) + "没有群功能");
                if (!member(s, s.user, m.recipient)) throw std::runtime_error("发送者不是群成员");
                const auto members = groupRecipients(s, m.recipient);
                if (members.empty()) throw std::runtime_error("群不存在或没有可见成员");
                for (const auto &member : members)
                    request.recipients.push_back(channelIdFanLX(s.service, member));
            }
            // 提交成功返回后才发 ACK；同一 requestId 重放返回旧结果。
            const auto result = store->accept(request);
            if (!result.message.messageId)
                throw std::runtime_error("服务端消息编号缺失");
            m.messageId = static_cast<std::uint64_t>(*result.message.messageId);
            m.conversationSeq = static_cast<std::uint64_t>(result.message.conversationSequence);
            m.sequence = 0;
            m.acceptedAtUtc = result.message.acceptedAtUtc; // 服务器时间，客户端不自行编造
            m.protocolVersion = imProtocolVersionV3FanLX;
            // 入库成功后才计临时额度（按 requestId 幂等，重放不重复扣）；
            // 同时先把目录推给接收方，使其在投递前就认这条临时会话。
            if (temporary)
                recordTemporaryFanLX(s, m);
            m.type = "SEND_ACK";
            write(s, m);
            return;
        }
        // ---- 旧 JSON 后端（保持既有行为） ----
        // 幂等键按通道查找：不同通道里同号码的两个人的 requestId 不能互相命中。
        for (const auto &row : db.at("messages")) {
            const auto saved = socketIoFanLX::message(row.at("message"));
            if (saved.sender != s.user || saved.requestId != m.requestId) continue;
            if (saved.recipient != m.recipient || saved.text != m.text || saved.scope != m.scope)
                throw std::runtime_error("同一 requestId 不允许修改消息");
            auto reply = saved;
            reply.type = "SEND_ACK";
            write(s, reply);
            return;
        }
        std::set<std::string> recipients{channelIdFanLX(s.service, s.user)};
        bool temporary = false;
        if (m.scope == "direct") {
            temporary = admitTemporaryFanLX(s, m);
            recipients.insert(channelIdFanLX(s.service, m.recipient));
        } else {
            if (!groupCapableFanLX(s.service))
                throw std::runtime_error(serviceDisplayNameFanLX(s.service) + "没有群功能");
            if (!member(s, s.user, m.recipient)) throw std::runtime_error("发送者不是群成员");
            const auto members = groupRecipients(s, m.recipient);
            for (const auto &member : members)
                recipients.insert(channelIdFanLX(s.service, member));
        }
        auto next = db;
        m.messageId = next.at("next").get<std::uint64_t>();
        if (m.messageId == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("消息编号耗尽");
        next["next"] = m.messageId + 1;
        const auto key = channelKeyFanLX(s.service, m); // 会话序号同样按通道分开
        auto seq = next["seqs"].value(key, std::uint64_t{0});
        if (seq == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("会话序号耗尽");
        m.conversationSeq = seq + 1;
        m.sequence = 0;
        m.type = "DELIVER";
        next["seqs"][key] = m.conversationSeq;
        next["messages"].push_back({{"message", socketIoFanLX::messageJson(m)}, {"recipients", recipients}});
        // 消息、幂等键、群序号和收件人快照一次原子落盘后才 ACK/投递。
        socketIoFanLX::commit(directory, db, std::move(next));
        if (temporary)
            recordTemporaryFanLX(s, m);
        m.type = "SEND_ACK";
        write(s, m);
    }

    void deliver(Session &s) {
        if (s.user.empty() || s.inFlight || s.peer->ended())
            return;
        if (useSqlite) {
            // 只取本通道（服务 + 号码）的待投递消息：同号码的 QQ 与微博互不干涉。
            const auto candidates = store->pendingFor(channelIdFanLX(s.service, s.user), {}, 64);
            for (const auto &candidate : candidates) {
                const auto &row = candidate.message;
                const auto found = s.cursors.find(candidate.conversationKey);
                const std::int64_t acked = found == s.cursors.end() ? 0 : found->second;
                if (row.conversationSequence <= acked) continue;
                // 群消息在投递时再次确认可见性：离开群后不再补发历史。
                // 会话键带通道前缀（"g|QQ|1001"），因此先解析出群号再判定成员关系。
                const auto parts = parseChannelKeyFanLX(candidate.conversationKey);
                if (parts.group && !member(s, s.user, parts.plain))
                    continue;
                ChatMessageFanLX out = socketIoFanLX::control("DELIVER");
                // 出站帧回到纯号码：客户端按自己所在的服务解释它们，不需要知道通道前缀。
                out.sender = plainIdFanLX(row.sender);
                out.recipient = plainIdFanLX(row.recipient);
                out.scope = parts.group ? "group" : "direct";
                out.messageId = static_cast<std::uint64_t>(*row.messageId);
                out.conversationSeq = static_cast<std::uint64_t>(row.conversationSequence);
                out.requestId = row.requestId;
                out.text = row.body;
                out.acceptedAtUtc = row.acceptedAtUtc;
                out.protocolVersion = imProtocolVersionV3FanLX;
                ship(s, out, candidate.conversationKey, parts.group);
                return;
            }
            if (s.syncing) {
                write(s, socketIoFanLX::control("SYNC_END"));
                s.syncing = false;
            }
            return;
        }
        const auto channelUser = channelIdFanLX(s.service, s.user);
        for (const auto &row : db.at("messages")) {
            const auto &recipients = row.at("recipients");
            if (std::find(recipients.begin(), recipients.end(), channelUser) == recipients.end()) continue;
            const auto m = socketIoFanLX::message(row.at("message"));
            if (m.scope == "group" && !member(s, s.user, m.recipient)) continue;
            const auto key = channelKeyFanLX(s.service, m);
            const auto ack = db.at("acks").contains(channelUser)
                                 ? db.at("acks").at(channelUser).value(key, std::uint64_t{0})
                                 : 0;
            if (m.conversationSeq <= ack) continue;
            ship(s, m, key, m.scope == "group");
            return;
        }
        if (s.syncing) {
            write(s, socketIoFanLX::control("SYNC_END"));
            s.syncing = false;
        }
    }

    void tick() {
        // accept 超时不是错误；其他错误由调用方处理，避免吞掉监听故障。
        try {
            auto peer = listener.accept(std::chrono::milliseconds(1));
            ++accepted;
            if (sessions.size() >= 128) peer->close();
            else {
                auto s = std::make_unique<Session>();
                s->peer = std::move(peer);
                sessions.push_back(std::move(s));
            }
        } catch (const std::runtime_error &error) {
            if (std::string(error.what()) != "网络操作超时") throw;
        }
        sweepRoutes();
        for (auto &s : sessions) {
            if (s->peer->ended()) continue;
            for (int budget = 0; budget < 32; ++budget) {
                auto m = s->peer->poll();
                if (!m) break;
                s->last = Clock::now();
                try {
                    handle(*s, *m);
                } catch (const PersistenceErrorFanLX &error) {
                    auto failure = socketIoFanLX::control("ERROR");
                    failure.requestId = m->requestId;
                    failure.text = error.what();
                    write(*s, failure);
                    s->peer->close(); // 提交失败：重连重放，不能让 ACK 窗口永久卡住。
                    break;
                } catch (const std::exception &error) {
                    auto failure = socketIoFanLX::control("ERROR");
                    failure.requestId = m->requestId;
                    failure.text = error.what();
                    write(*s, failure);
                    // 存储错误不能当作成功；发送方 outbox 保留，显式重试。
                    if (s->user.empty()) s->peer->close();
                }
            }
            if (Clock::now() - s->last > std::chrono::seconds(20) ||
                (s->inFlight && Clock::now() - s->sentAt > std::chrono::seconds(10))) {
                s->peer->close();
            } else {
                // 直连 3 秒未确认就改发同一条的中转正文，不等到 10 秒把整条连接关掉。
                if (s->inFlightDirect && s->inFlight &&
                    Clock::now() - s->sentAt > std::chrono::seconds(3))
                    relayArmed(*s);
                deliver(*s);
            }
            // 每轮都推进待发送缓冲：慢客户端只影响它自己，不阻塞其他用户。
            if (!s->peer->ended() && !s->peer->flushSend(16)) {
                s->peer->close();
            }
        }
        for (auto it = sessions.begin(); it != sessions.end();) {
            if (!(*it)->peer->ended()) { ++it; continue; }
            auto user = (*it)->user;
            const auto service = (*it)->service;
            if ((*it)->inFlight)
                inFlightKeys.erase((*it)->inFlight);
            if (!user.empty())
                degradeUser(service, user, false);
            it = sessions.erase(it);
            // 会话对象已被销毁，只剩身份（服务 + 账号）可用：同一服务里没有其它在线会话时才播报下线。
            if (!user.empty() && !online(service, user)) presence(service, user, false);
        }
    }

    // 按 (服务, 账号) 判断在线，不需要 Session 句柄（会话移除后仍可判定）。
    bool onlineServiceFanLX(ServiceTypeFanLX service, const std::string &u) const {
        for (const auto &other : sessions)
            if (other->service == service && other->user == u && !other->peer->ended()) return true;
        return false;
    }
};

ImServerFanLX::ImServerFanLX(const PlatformStateFanLX &s, const std::filesystem::path &dir,
                             unsigned short p, const std::string &ip)
    : implFanLX(std::make_unique<Impl>(s, dir, p, ip, OptionsFanLX{})) {}

ImServerFanLX::ImServerFanLX(const PlatformStateFanLX &s, const std::filesystem::path &dir,
                             unsigned short p, const std::string &ip, OptionsFanLX options)
    : implFanLX(std::make_unique<Impl>(s, dir, p, ip, std::move(options))) {}

ImServerFanLX::~ImServerFanLX() = default;
unsigned short ImServerFanLX::port() const { return implFanLX->listener.port(); }
void ImServerFanLX::tick() { implFanLX->tick(); }
std::size_t ImServerFanLX::connectionsAccepted() const { return implFanLX->accepted; }
std::string ImServerFanLX::instanceId() const { return implFanLX->instanceId; }
std::string ImServerFanLX::storageBackend() const { return implFanLX->useSqlite ? "sqlite" : "json"; }
std::size_t ImServerFanLX::slowPeerDisconnects() const { return implFanLX->slowDisconnects; }
ImServerFanLX::DirectRouteStatsFanLX ImServerFanLX::directRouteStats() const { return implFanLX->routeStats; }

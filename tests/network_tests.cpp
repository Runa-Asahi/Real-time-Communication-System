#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network/ImClientFanLX.h"
#include "network/ImServerFanLX.h"
#include "network/MessageCodecFanLX.h"
#include "network/ServerAccountRegistrarFanLX.h"
#include "network/TcpListenerFanLX.h"
#include "friend/IdentityResolverFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include "ui/QQChatSessionFanLX.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>

namespace {
using namespace std::chrono_literals;
const AccountRefFanLX owner{ServiceTypeFanLX::QQ, "2026001"};
const AccountRefFanLX friendAccount{ServiceTypeFanLX::QQ, "2026002"};
template <class FnFanLX> void failsFanLX(FnFanLX fn) {
    bool thrown = false;
    try {
        fn();
    } catch (const std::exception &) {
        thrown = true;
    }
    assert(thrown);
}
template <class FnFanLX> void awaitFanLX(FnFanLX predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 8s;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("测试等待超时");
        std::this_thread::sleep_for(1ms);
    }
}
void initializeFanLX(PersistenceApplicationFanLX &app) {
    app.initialize();
    app.importInitial(demoSeedFanLX());
}
SocketHandleFanLX rawConnectFanLX(unsigned short port) {
    SocketHandleFanLX socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    assert(socket.get() != INVALID_SOCKET);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    assert(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
    assert(::connect(socket.get(), reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
    return socket;
}
void rawSendFanLX(SOCKET socket, const std::string &data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        const auto count = ::send(socket, data.data() + offset, static_cast<int>(data.size() - offset), 0);
        assert(count > 0);
        offset += static_cast<std::size_t>(count);
    }
}
std::string frameFanLX(const std::string &payload) {
    std::string result(4, '\0');
    for (int i = 0; i < 4; ++i)
        result[i] = static_cast<char>((payload.size() >> (24 - i * 8)) & 255);
    return result + payload;
}
void rejectedWireFanLX(const std::string &wire, bool disconnect = false) {
    TcpListenerFanLX listener(0);
    auto raw = rawConnectFanLX(listener.port());
    auto receiver = listener.accept();
    rawSendFanLX(raw.get(), wire);
    if (disconnect)
        shutdown(raw.get(), SD_SEND);
    awaitFanLX([&] { return receiver->ended(); });
    assert(!receiver->poll() && !receiver->error().empty());
}
void childFanLX(unsigned short port, const std::filesystem::path &dir) {
    PersistenceApplicationFanLX app(dir);
    initializeFanLX(app);
    app.login(friendAccount, demoPasswordFanLX(friendAccount.accountId));
    const auto before = SnapshotCodecFanLX::encode(app.snapshot());
    QQChatSessionFanLX chat(app, friendAccount, owner, TcpPeerFanLX::connect("127.0.0.1", port));
    for (int i = 1; i <= 100; ++i) {
        std::optional<ChatMessageFanLX> message;
        awaitFanLX([&] {
            message = chat.poll();
            return message.has_value();
        });
        assert(message->text == "中文消息" + std::to_string(i));
        assert(message->sequence == static_cast<std::uint64_t>(i));
        chat.send("收到：" + message->text);
    }
    assert(SnapshotCodecFanLX::encode(app.snapshot()) == before);
    app.saveAndClose();
}

void imTestsFanLX(const std::filesystem::path &root) {
    PersistenceApplicationFanLX app(root / "domain");
    initializeFanLX(app);
    // 认证凭据直接取自领域账号库（演示数据的密码规则 pw-<QQ号>）：
    // 服务端不再接受任何测试专用令牌表，登录就是"用户名 + 密码"。
    auto server = std::make_unique<ImServerFanLX>(app.state(), root / "server", 0);
    const auto port = server->port();
    // 同步推进：每轮 tick 服务端并检查谓词。label 用于在超时时指出卡在哪一步。
    auto pump = [&](auto predicate, [[maybe_unused]] const char *label = "") {
        const auto deadline = std::chrono::steady_clock::now() + 12s;
        while (!predicate()) {
            if (std::chrono::steady_clock::now() > deadline) {
                std::cout << "[pump timeout] " << label << std::endl;
                throw std::runtime_error("IM 测试超时");
            }
            server->tick();
        }
    };
    auto client = [&](const std::string &id) {
        return std::make_unique<ImClientFanLX>("127.0.0.1", port, id, demoPasswordFanLX(id), root / id);
    };
    auto a = client("2026001");
    pump([&] { return a->online(); }, "a-online");
    a->send("2026002", "offline direct");
    a->send("1001", "offline group", true);
    pump([&] { return a->pending() == 0 && a->history().size() == 2; }, "a-two-messages");
    assert(server->connectionsAccepted() == 1); // 多消息只 connect 一次。
    auto b = client("2026002"); auto c = client("2026003");
    pump([&] { return b->history().size() == 2 && c->history().size() == 1; }, "bc-history");
    assert(c->history().front().scope == "group");
    // 非好友既收不到上下线广播，也查不到状态。
    while (auto e = c->pollEvent()) assert(e->type != "PRESENCE");
    c->queryOnline("2026001");
    pump([&] { return c->error().find("只能查询好友") != std::string::npos; }, "c-forbidden-query");
    bool online = false;
    a->queryOnline("2026002");
    pump([&] {
        while (auto e = a->pollEvent()) if (e->type == "PRESENCE" && e->sender == "2026002" && e->text == "online") online = true;
        return online;
    }, "a-sees-b-online");
    b.reset();
    bool offline = false;
    pump([&] {
        while (auto e = a->pollEvent()) if (e->type == "PRESENCE" && e->sender == "2026002" && e->text == "offline") offline = true;
        return offline;
    }, "a-sees-b-offline");
    a->send("2026002", "offline after disconnect");
    a->send("1001", "group increment", true);
    pump([&] { return a->pending() == 0 && c->history().size() == 2 && a->history().size() == 4; }, "rejoin");
    b = client("2026002");
    pump([&] { return b->history().size() == 4; }, "b-catch-up");
    auto history = b->history();
    assert(history[1].conversationSeq == 1 && history[3].conversationSeq == 2);
    // 服务端重启期间发送，outbox 保留；自动重连后补发。
    server.reset();
    a->send("2026002", "restart one"); a->send("2026002", "restart two");
    a.reset(); a = client("2026001"); // 未发出的 outbox 随客户端生命周期恢复。
    server = std::make_unique<ImServerFanLX>(app.state(), root / "server", port);
    pump([&] { return a->pending() == 0 && b->history().size() == 6 && a->history().size() == 6; },
         "restart-replay");
    const auto accepted = server->connectionsAccepted();
    a->reconnect();
    pump([&] { return server->connectionsAccepted() > accepted && a->online(); }, "explicit-reconnect");
    for (int i = 0; i < 8; ++i) server->tick();
    assert(a->history().size() == 6 && b->history().size() == 6);
    b.reset();
    for (int i = 0; i < 3; ++i) server->tick();
    b = client("2026002");
    pump([&] { return b->online(); }, "b-online-again");
    assert(b->history().size() == 6); // 客户端进程生命周期重建仍保留 inbox。
    const auto liveConnections = server->connectionsAccepted();
    b->send("2026001", "online direct reply");
    pump([&] { return b->pending() == 0 && a->history().size() == 7 && b->history().size() == 7; }, "online-reply");
    assert(server->connectionsAccepted() == liveConnections);
    a.reset(); b.reset(); c.reset(); server.reset();

    // 人工控制两个 ACK 丢失窗口，使用真实 TCP 帧而不是模拟存储函数。
    server = std::make_unique<ImServerFanLX>(app.state(), root / "ack-server", 0);
    const auto rawPort = server->port();
    auto control = [](const std::string &type) { ChatMessageFanLX m; m.type = type; return m; };
    auto receive = [&](TcpPeerFanLX &peer, const std::string &type) {
        std::optional<ChatMessageFanLX> found;
        pump([&] {
            while (auto m = peer.poll()) if (m->type == type) { found = *m; return true; }
            return false;
        }, "receive-message");
        return *found;
    };
    auto login = [&](const std::string &user) {
        auto peer = TcpPeerFanLX::connect("127.0.0.1", rawPort);
        auto m = control("LOGIN"); m.sender = user; m.text = demoPasswordFanLX(user); peer->send(m);
        receive(*peer, "LOGIN_OK"); return peer;
    };
    auto sender = login("2026001");
    auto send = control("SEND"); send.recipient = "2026002"; send.text = "ACK loss"; send.requestId = "stable-1";
    sender->send(send);
    auto first = receive(*sender, "SEND_ACK");
    // 模拟发送端没有处理/持久化 SEND_ACK 就退出。
    sender.reset();
    for (int i = 0; i < 3; ++i) server->tick();
    sender = login("2026001"); sender->send(send);
    assert(receive(*sender, "SEND_ACK").messageId == first.messageId);
    auto receiver = login("2026002");
    receiver->send(control("SYNC"));
    const auto delivered = receive(*receiver, "DELIVER");
    assert(delivered.messageId == first.messageId);
    // ACK 丢失 + 服务端重启：未确认投递必须重放。
    receiver.reset(); sender.reset(); server.reset();
    server = std::make_unique<ImServerFanLX>(app.state(), root / "ack-server", rawPort);
    receiver = login("2026002"); receiver->send(control("SYNC"));
    assert(receive(*receiver, "DELIVER").messageId == delivered.messageId);
    // ACK 必须携带服务器分配的会话序号：服务端以它推进投递游标。
    // 只带 messageId 的 ACK 会被明确拒绝（避免会话永久停在"在途"），因此这里按协议补齐字段。
    auto ack = control("ACK");
    ack.messageId = delivered.messageId;
    ack.conversationSeq = delivered.conversationSeq;
    receiver->send(ack);
    receive(*receiver, "SYNC_END");
    receiver.reset(); for (int i = 0; i < 3; ++i) server->tick();
    receiver = login("2026002"); receiver->send(control("SYNC"));
    receive(*receiver, "SYNC_END"); // 已持久化 ACK 不再重放。
    sender = login("2026001");
    send.text = "same id different payload"; sender->send(send);
    assert(receive(*sender, "ERROR").text.find("requestId") != std::string::npos);
    send.requestId = "spoof"; send.sender = "2026002"; sender->send(send);
    assert(receive(*sender, "ERROR").text.find("伪造") != std::string::npos);
    auto stranger = login("2026004");
    send.sender.clear(); send.scope = "group"; send.recipient = "1001"; send.requestId = "not-member";
    stranger->send(send);
    assert(receive(*stranger, "ERROR").text.find("群成员") != std::string::npos);
    ack.messageId = 999999; stranger->send(ack);
    assert(receive(*stranger, "ERROR").text.find("ACK") != std::string::npos);
    // 写盘失败不得发送成功 ACK；恢复后相同 requestId 可安全重试。
    auto diskMessage = control("SEND"); diskMessage.recipient = "2026002";
    diskMessage.requestId = "storage-failure"; diskMessage.text = "retry after disk failure";
    const auto store = root / "ack-server" / "state.json";
    assert(SetFileAttributesW(store.c_str(), FILE_ATTRIBUTE_READONLY));
    sender->send(diskMessage);
    receive(*sender, "ERROR");
    assert(SetFileAttributesW(store.c_str(), FILE_ATTRIBUTE_NORMAL));
    sender.reset(); for (int i = 0; i < 3; ++i) server->tick();
    sender = login("2026001"); sender->send(diskMessage);
    const auto diskAck = receive(*sender, "SEND_ACK");
    assert(diskAck.messageId == first.messageId + 1);
    const auto pendingDelivery = receive(*receiver, "DELIVER");
    receiver.reset(); for (int i = 0; i < 3; ++i) server->tick();
    // 构造“已提交 inbox，尚未送出 ACK 即崩溃”的磁盘状态。
    // 先由真实客户端创建合法存储，但不给服务端 tick，不处理登录或投递。
    const auto crashDir = root / "crashed-client";
    { ImClientFanLX init("127.0.0.1", rawPort, "2026002", demoPasswordFanLX("2026002"), crashDir); }
    auto saved = nlohmann::json::parse(AtomicFileWriterFanLX::read(crashDir / "state.json"));
    saved["inbox"][std::to_string(pendingDelivery.messageId)] = nlohmann::json::parse(MessageCodecFanLX::encode(pendingDelivery).substr(4));
    saved["cursors"]["d|2026001|2026002"] = pendingDelivery.conversationSeq;
    AtomicFileWriterFanLX::publish(crashDir, saved.dump(), false);
    ImClientFanLX recovered("127.0.0.1", rawPort, "2026002", demoPasswordFanLX("2026002"), crashDir);
    pump([&] { return recovered.online(); }, "recovered-online");
    bool synced = false;
    pump([&] {
        while (auto e = recovered.pollEvent()) if (e->type == "SYNC_END") synced = true;
        return synced;
    }, "recovered-sync");
    assert(recovered.history().size() == 1); // 重放被去重，但再次 ACK，服务端完成同步。
    // 用户名对、密码错必须被拒绝；错误文本统一为"用户名或密码错误"。
    auto bad = TcpPeerFanLX::connect("127.0.0.1", rawPort);
    auto invalid = control("LOGIN"); invalid.sender = "2026003"; invalid.text = "bad-token"; bad->send(invalid);
    assert(receive(*bad, "ERROR").text == "用户名或密码错误");
    // 换库重绑：服务端数据目录被重建后实例号必然变，本地聊天库属于"上一个实例"。
    // 旧实现会抛"聊天库归属不符"把用户永久挡在登录之外（连不上、进不了主界面、不停重连）；
    // 现在必须"改名归档旧库 + 按当前实例新建"，登录继续，并把这件事作为提示告诉用户。
    // 本用例只用 2026003 / 2026005：它们在本文件其余用例里都没有登录过，
    // 不会撞上服务端的单设备策略（同账号第二个活跃会话会被拒）。
    // 造法与真实事故一致：手工写一份实例号已退役的本地库。
    const auto rebindDir = root / "rebind";
    std::filesystem::create_directories(rebindDir);
    const auto writeStaleStore = [&](const std::string &instanceId, const std::string &owner) {
        std::ofstream(rebindDir / "client-identity.json")
            << nlohmann::json{{"schema", "fanlx-im-client-identity-1"},
                              {"account", owner},
                              {"server_instance_id", instanceId},
                              {"host", "127.0.0.1"},
                              {"port", rawPort},
                              {"device", "device-from-retired-instance"}}
                   .dump(2);
        std::ofstream(rebindDir / "chat.db", std::ios::binary) << "not-a-real-database";
        std::ofstream(rebindDir / "chat.db-wal", std::ios::binary) << "";
    };
    const auto clientDb = ImClientFanLX::ChatDbConfigFanLX{true, {}};
    // 不变量 1：目录被误用（不是这个账号）必须继续拒绝——这是真正的配置错误，不能静默改绑。
    writeStaleStore("srv-retired-0001", "2026004");
    {
        ImClientFanLX misuse("127.0.0.1", rawPort, "2026003", demoPasswordFanLX("2026003"),
                             rebindDir, clientDb);
        bool rejected = false;
        pump([&] {
            while (auto e = misuse.pollClientEvent())
                if (e->detail.find("聊天库归属不符") != std::string::npos) rejected = true;
            return rejected;
        }, "rebind-misuse-rejected");
        assert(!misuse.online());
    }
    // 不变量 2：只有实例号过期（账号 / 服务器 / 格式都对）→ 归档旧库后继续登录。
    writeStaleStore("srv-retired-0001", "2026005");
    auto rebound = std::make_unique<ImClientFanLX>("127.0.0.1", rawPort, "2026005",
                                                   demoPasswordFanLX("2026005"), rebindDir, clientDb);
    pump([&] { return rebound->online(); }, "rebind-online");
    // 旧文件必须改名保留（而不是删除）：库本体、WAL 旁文件与身份文件。
    std::size_t archived = 0;
    for (const auto &entry : std::filesystem::directory_iterator(rebindDir))
        if (entry.path().filename().string().find(".legacy-") != std::string::npos)
            ++archived;
    assert(archived >= 3);
    const auto rebindIdentity =
        nlohmann::json::parse(AtomicFileWriterFanLX::read(rebindDir / "client-identity.json"));
    assert(rebindIdentity.at("server_instance_id").get<std::string>() == server->instanceId());
    assert(rebindIdentity.at("device").get<std::string>() == "device-from-retired-instance");
    // 归档必须可见（Notice 事件），不能静默。
    bool noticed = false;
    pump([&] {
        while (auto e = rebound->pollClientEvent())
            if (e->kind == ImClientEventFanLX::KindFanLX::Notice) noticed = true;
        return noticed;
    }, "rebind-notice");
    std::cout << "IM long connection/offline/group/presence/restart/ACK/rebind tests passed\n";
}

// 好友申请与临时会话：添加好友必须"一方申请、对方同意"，同意前允许每方向限额的临时对话；
// 还可以依据本人另一个服务的好友发起申请（服务端按"同一个人"换算号码）。
// 服务端由后台线程独占 tick（好友操作是同步短连接，主线程等待应答时必须有人推进服务端）。
void friendRequestTestsFanLX(const std::filesystem::path &root) {
    PersistenceApplicationFanLX app(root / "domain");
    initializeFanLX(app);
    const ServerAccountRegistrarFanLX registrar(app);
    ImServerFanLX::OptionsFanLX options;
    options.refreshAccounts = [&registrar] { (void)registrar.refreshAccounts(); };
    options.updateFriendship = [&registrar](const std::string &service, const std::string &self,
                                            const std::string &peer, bool add) {
        return registrar.updateFriendshipFanLX(service, self, peer, add);
    };
    options.temporaryMessageLimit = 2;
    ImServerFanLX server(app.state(), root / "server", 0, "127.0.0.1", options);
    const auto port = server.port();
    std::atomic<bool> stop{false};
    std::thread ticker([&] {
        while (!stop.load()) {
            server.tick();
            std::this_thread::sleep_for(1ms);
        }
    });
    const auto waitFor = [](TcpPeerFanLX &peer, const std::string &type) {
        const auto deadline = std::chrono::steady_clock::now() + 8s;
        while (std::chrono::steady_clock::now() < deadline) {
            while (auto m = peer.poll())
                if (m->type == type || m->type == "ERROR") return *m;
            std::this_thread::sleep_for(1ms);
        }
        throw std::runtime_error("等待 " + type + " 超时");
    };
    const auto login = [&](const std::string &service, const std::string &user, const std::string &weix) {
        auto peer = TcpPeerFanLX::connect("127.0.0.1", port);
        ChatMessageFanLX m; m.type = "LOGIN"; m.service = service; m.sender = user; m.text = demoPasswordFanLX(weix);
        peer->send(m);
        assert(waitFor(*peer, "LOGIN_OK").type == "LOGIN_OK");
        return peer;
    };
    const auto sendDirect = [&](TcpPeerFanLX &peer, const std::string &service, const std::string &to,
                                const std::string &requestId) {
        ChatMessageFanLX m; m.type = "SEND"; m.service = service; m.recipient = to;
        m.text = "临时消息 " + requestId; m.requestId = requestId;
        peer.send(m);
        return waitFor(peer, "SEND_ACK");
    };
    const auto friendOp = [&](const std::string &service, const std::string &self, const std::string &weix,
                              const std::string &op, const std::string &peer, const std::string &via = {}) {
        std::string json, reason;
        bool answered = false;
        const bool ok = ImClientFanLX::friendOperationFanLX("127.0.0.1", port, service, self,
                                                            demoPasswordFanLX(weix), op, peer, "我是同学", json,
                                                            reason, via, &answered);
        assert(answered);
        return std::make_pair(ok, ok ? nlohmann::json::parse(json) : nlohmann::json{{"reason", reason}});
    };
    const auto friendsOf = [](const nlohmann::json &result) {
        return result.at("friends").get<std::vector<std::string>>();
    };
    const auto contains = [](const std::vector<std::string> &list, const std::string &value) {
        return std::find(list.begin(), list.end(), value) != list.end();
    };

    // ① 非好友：每方向限额 2 条临时消息；第 3 条被拒并提示先发好友申请；重放同一 requestId 不重复扣额度。
    auto six = login("QQ", "2026006", "2026006");
    auto seven = login("QQ", "2026007", "2026007");
    assert(sendDirect(*six, "QQ", "2026007", "tmp-1").type == "SEND_ACK");
    assert(sendDirect(*six, "QQ", "2026007", "tmp-2").type == "SEND_ACK");
    const auto over = sendDirect(*six, "QQ", "2026007", "tmp-3");
    assert(over.type == "ERROR" && over.text.find("临时会话已达上限") != std::string::npos);
    assert(sendDirect(*six, "QQ", "2026007", "tmp-1").type == "SEND_ACK");
    // 反方向有自己的额度：对方回复不受我方用满的影响。
    assert(sendDirect(*seven, "QQ", "2026006", "back-1").type == "SEND_ACK");

    // ② 申请：发出后对方同意前**不是**好友；对方的待处理列表里能看到附言。
    auto [requested, requestResult] = friendOp("QQ", "2026006", "2026006", "request", "2026007");
    assert(requested && requestResult.at("message").get<std::string>().find("等待对方同意") != std::string::npos);
    assert(!contains(friendsOf(requestResult), "2026007"));
    auto [listed, listResult] = friendOp("QQ", "2026007", "2026007", "list", "");
    assert(listed && listResult.at("incoming").size() == 1);
    assert(listResult.at("incoming")[0].at("peer") == "2026006");
    assert(listResult.at("incoming")[0].at("note") == "我是同学");
    // 发起方不能替对方同意。
    assert(!friendOp("QQ", "2026006", "2026006", "accept", "2026007").first);

    // ③ 同意：双方成为好友，临时额度清除，之后单聊不再受限。
    auto [accepted, acceptResult] = friendOp("QQ", "2026007", "2026007", "accept", "2026006");
    assert(accepted && contains(friendsOf(acceptResult), "2026006"));
    for (int i = 0; i < 4; ++i)
        assert(sendDirect(*six, "QQ", "2026007", "friend-" + std::to_string(i)).type == "SEND_ACK");
    assert(!friendOp("QQ", "2026006", "2026006", "request", "2026007").first); // 已是好友

    // ④ 跨服务：在微信里依据本人的 QQ 好友 2026007 发申请，服务端换算成他的微信号 wx_2026007。
    auto [via, viaResult] = friendOp("Wechat", "wx_2026006", "2026006", "request", "2026007", "QQ");
    assert(via && viaResult.at("peer") == "wx_2026007");
    assert(!contains(friendsOf(viaResult), "wx_2026007")); // 仍需对方同意
    auto [wxAccepted, wxResult] = friendOp("Wechat", "wx_2026007", "2026007", "accept", "wx_2026006");
    assert(wxAccepted && contains(friendsOf(wxResult), "wx_2026006"));
    // 不能拿"不是我 QQ 好友"的号码冒充跨服务依据。
    const auto forged = friendOp("Wechat", "wx_2026006", "2026006", "request", "2026008", "QQ");
    assert(!forged.first && forged.second.at("reason").get<std::string>().find("不是本人") != std::string::npos);

    // ⑤ 拒绝 / 撤回：都不会建立关系。
    assert(friendOp("QQ", "2026006", "2026006", "request", "2026008").first);
    assert(friendOp("QQ", "2026008", "2026008", "reject", "2026006").first);
    assert(friendOp("QQ", "2026006", "2026006", "request", "2026009").first);
    auto [cancelled, cancelResult] = friendOp("QQ", "2026006", "2026006", "cancel", "2026009");
    assert(cancelled && cancelResult.at("outgoing").empty());
    assert(!contains(friendsOf(cancelResult), "2026008") && !contains(friendsOf(cancelResult), "2026009"));

    // ⑥ 删除好友：关系解除后回到临时会话，额度重新计算。
    auto [removed, removeResult] = friendOp("QQ", "2026006", "2026006", "remove", "2026007");
    assert(removed && !contains(friendsOf(removeResult), "2026007"));
    assert(sendDirect(*six, "QQ", "2026007", "again-1").type == "SEND_ACK");
    assert(sendDirect(*six, "QQ", "2026007", "again-2").type == "SEND_ACK");
    assert(sendDirect(*six, "QQ", "2026007", "again-3").type == "ERROR");

    six.reset();
    seven.reset();
    stop = true;
    ticker.join();
    // 好友关系落在服务端持久化账号库里：微信分区出现 6↔7，QQ 分区已解除。
    IdentityResolverFanLX ids(app.state().accounts);
    assert(app.state().friends.directory(ServiceTypeFanLX::Wechat)
               .contains(ids.accountKey(ServiceTypeFanLX::Wechat, "wx_2026006"),
                         ids.accountKey(ServiceTypeFanLX::Wechat, "wx_2026007")));
    assert(!app.state().friends.directory(ServiceTypeFanLX::QQ)
                .contains(ids.accountKey(ServiceTypeFanLX::QQ, "2026006"),
                          ids.accountKey(ServiceTypeFanLX::QQ, "2026007")));
    std::cout << "friend request / temporary chat / cross-service tests passed\n";
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc == 3) {
        try {
            childFanLX(static_cast<unsigned short>(std::stoi(argv[1])), std::filesystem::path(argv[2]));
            return 0;
        } catch (const std::exception &error) {
            std::cerr << error.what();
            return 1;
        }
    }
    const auto root =
        std::filesystem::temp_directory_path() /
        ("fanlx-network-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    try {
        if (argc == 2 && std::wstring(argv[1]) == L"--im") {
            imTestsFanLX(root / "im");
            friendRequestTestsFanLX(root / "friend-requests");
            return 0;
        }
        SocketRuntimeFanLX runtime;
        // T14a 编码边界：零/超长帧、非法 UTF-8、类型、版本、深嵌套和控制符均拒绝。
        const ChatMessageFanLX valid{"2026001", "2026002", 1, "中文测试"};
        const auto encoded = MessageCodecFanLX::encode(valid);
        assert(MessageCodecFanLX::decode(encoded.substr(4)).text == valid.text);
        failsFanLX([&] { MessageCodecFanLX::encode({"a", "b", 0, "x"}); });
        failsFanLX([&] { MessageCodecFanLX::encode({"a", "b", 1, std::string(4097, 'x')}); });
        failsFanLX([&] { MessageCodecFanLX::encode({"a", "b", 1, "\x1b[2J"}); });
        failsFanLX([&] { MessageCodecFanLX::encode({"a", "b", 1, "\xff"}); });
        // T14b 发送前预检：必须与上面的组帧校验同口径。预检放行而组帧失败的文本会成为"毒消息"——
        // 它先被持久化成"待发送"，之后每次重连都被重放并抛异常，客户端永远无法上线。
        assert(MessageCodecFanLX::textIssueFanLX("").empty() == false);
        assert(MessageCodecFanLX::textIssueFanLX("你好").empty());
        assert(MessageCodecFanLX::textIssueFanLX("line1\nline2\ttab").empty());
        assert(MessageCodecFanLX::textIssueFanLX(std::string(4096, 'x')).empty());
        assert(!MessageCodecFanLX::textIssueFanLX(std::string(4097, 'x')).empty());
        assert(!MessageCodecFanLX::textIssueFanLX("\x1b[2J").empty());
        assert(!MessageCodecFanLX::textIssueFanLX("\xff").empty());
        // 上限按**字节**计：1365 个汉字（4095 字节）放行，再加一个就超限。
        {
            std::string chinese;
            while (chinese.size() + 3 <= MessageCodecFanLX::maxText) chinese += "\xE4\xBD\xA0"; // "你"
            assert(MessageCodecFanLX::textIssueFanLX(chinese).empty());
            assert(!MessageCodecFanLX::textIssueFanLX(chinese + "\xE4\xBD\xA0").empty());
        }
        for (const auto &payload : {"{}", "{\"x\":[],\"x\":2}", "{\"x\":1,\"x\":2}", "[[]]"})
            failsFanLX([&] { MessageCodecFanLX::decode(payload); });
        rejectedWireFanLX(std::string(4, '\0'));
        rejectedWireFanLX(std::string(4, '\xff'));
        rejectedWireFanLX(frameFanLX("{\"text\":\"\xff\"}"));
        auto bad = nlohmann::json::parse(encoded.substr(4));
        bad["version"] = 2;
        rejectedWireFanLX(frameFanLX(bad.dump()));
        bad["version"] = 1;
        bad["sequence"] = -1;
        rejectedWireFanLX(frameFanLX(bad.dump()));
        rejectedWireFanLX(encoded.substr(0, 2), true);
        rejectedWireFanLX(encoded.substr(0, encoded.size() - 1), true);

        // T14b 实际 TCP 任意拆包/粘包：逐字节发第一帧，再一次写入两帧。
        {
            TcpListenerFanLX listener(0);
            auto raw = rawConnectFanLX(listener.port());
            auto receiver = listener.accept();
            for (char byte : encoded) {
                rawSendFanLX(raw.get(), std::string(1, byte));
                std::this_thread::sleep_for(1ms);
            }
            rawSendFanLX(raw.get(), MessageCodecFanLX::encode({"2026001", "2026002", 2, "第二条"}) +
                                        MessageCodecFanLX::encode({"2026001", "2026002", 3, "第三条"}));
            for (std::uint64_t i = 1; i <= 3; ++i) {
                std::optional<ChatMessageFanLX> message;
                awaitFanLX([&] {
                    message = receiver->poll();
                    return message.has_value();
                });
                assert(message->sequence == i);
            }
            shutdown(raw.get(), SD_SEND);
            awaitFanLX([&] { return receiver->ended(); });
            assert(receiver->error() == "对端已断开");
        }
        // T14c 有界队列、端口占用、监听超时、无人连接时的析构、重复关闭。
        {
            TcpListenerFanLX listener(0);
            failsFanLX([&] { TcpListenerFanLX duplicate(listener.port()); });
            failsFanLX([&] { listener.accept(30ms); });
            auto raw = rawConnectFanLX(listener.port());
            auto receiver = listener.accept(1s, 2);
            rawSendFanLX(raw.get(), encoded + encoded + encoded);
            awaitFanLX([&] { return receiver->ended(); });
            assert(receiver->error().find("队列已满") != std::string::npos);
            assert(receiver->poll() && receiver->poll() && !receiver->poll());
            receiver->close();
            receiver->close();
            failsFanLX([&] { receiver->send(valid); });
        }
        // 已接收部分帧但不再有字节时，总截止时间触发，不会永久占用线程。
        {
            TcpListenerFanLX listener(0);
            auto raw = rawConnectFanLX(listener.port());
            auto receiver = listener.accept();
            rawSendFanLX(raw.get(), encoded.substr(0, 1));
            awaitFanLX([&] { return receiver->ended(); });
            assert(receiver->error().find("超时") != std::string::npos);
        }
        failsFanLX([&] { TcpPeerFanLX::connect("not-an-ip", 1234); });
        unsigned short unused = 0;
        {
            TcpListenerFanLX listener(0);
            unused = listener.port();
        }
        failsFanLX([&] { TcpPeerFanLX::connect("127.0.0.1", unused, 200ms); });

        // T14d 两个真实进程、不同数据目录、100 条双向中文消息，顺序和内容逐条比较。
        {
            PersistenceApplicationFanLX app(root / "A");
            initializeFanLX(app);
            app.login(owner, demoPasswordFanLX(owner.accountId));
            const auto before = SnapshotCodecFanLX::encode(app.snapshot());
            TcpListenerFanLX listener(0);
            wchar_t executable[32768];
            const auto length = GetModuleFileNameW(nullptr, executable, 32768);
            assert(length > 0 && length < 32768);
            std::wstring command = L"\"" + std::wstring(executable, length) + L"\" " +
                                   std::to_wstring(listener.port()) + L" \"" + (root / "B").wstring() + L"\"";
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            assert(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                                  nullptr, nullptr, &startup, &process));
            try {
                QQChatSessionFanLX chat(app, owner, friendAccount, listener.accept());
                for (int i = 1; i <= 100; ++i)
                    chat.send("中文消息" + std::to_string(i));
                for (int i = 1; i <= 100; ++i) {
                    std::optional<ChatMessageFanLX> message;
                    awaitFanLX([&] {
                        message = chat.poll();
                        return message.has_value();
                    });
                    assert(message->text == "收到：中文消息" + std::to_string(i));
                    assert(message->sequence == static_cast<std::uint64_t>(i));
                }
                assert(SnapshotCodecFanLX::encode(app.snapshot()) == before);
                assert(WaitForSingleObject(process.hProcess, 8000) == WAIT_OBJECT_0);
                DWORD code = 99;
                assert(GetExitCodeProcess(process.hProcess, &code) && code == 0);
            } catch (...) {
                TerminateProcess(process.hProcess, 98);
                WaitForSingleObject(process.hProcess, 2000);
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                throw;
            }
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            app.saveAndClose();
        }
        // T14e 应用鉴权：缓存预热也不能绕过注销/删好友；账号和序号伪造在主线程拒绝。
        {
            PersistenceApplicationFanLX app(root / "auth");
            initializeFanLX(app);
            failsFanLX([&] { QQChatSessionFanLX::authorize(app, owner, friendAccount); });
            app.login(owner, demoPasswordFanLX(owner.accountId), true);
            failsFanLX([&] {
                QQChatSessionFanLX::authorize(app, {ServiceTypeFanLX::Wechat, "wx_2026001"}, friendAccount);
            });
            failsFanLX([&] { QQChatSessionFanLX::authorize(app, owner, {ServiceTypeFanLX::QQ, "2026003"}); });
            for (const auto &invalid : {ChatMessageFanLX{"2026003", "2026001", 1, "伪造"},
                                        ChatMessageFanLX{"2026002", "2026001", 2, "跳号"}}) {
                TcpListenerFanLX listener(0);
                auto remote = TcpPeerFanLX::connect("127.0.0.1", listener.port());
                QQChatSessionFanLX chat(app, owner, friendAccount, listener.accept());
                remote->send(invalid);
                bool rejected = false;
                awaitFanLX([&] {
                    try {
                        chat.poll();
                    } catch (...) {
                        rejected = true;
                    }
                    return rejected;
                });
                assert(chat.ended());
            }
            TcpListenerFanLX listener(0);
            auto remote = TcpPeerFanLX::connect("127.0.0.1", listener.port());
            QQChatSessionFanLX chat(app, owner, friendAccount, listener.accept());
            app.removeFriend(owner, friendAccount);
            failsFanLX([&] { chat.send("不能越权"); });
            app.login(friendAccount, demoPasswordFanLX(friendAccount.accountId));
            app.addFriend(owner, friendAccount);
            app.logout(owner);
            failsFanLX([&] { chat.poll(); });
            app.saveAndClose();
        }
        std::filesystem::remove_all(root); // 只清理本测试唯一临时目录，失败时保留现场。
        std::cout << "T14: framing, limits, disconnect, two-process Chinese chat and authorization passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << " evidence=" << root.u8string() << '\n';
        return 1;
    }
}

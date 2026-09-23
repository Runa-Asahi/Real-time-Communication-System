#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network/MessageCodecFanLX.h"
#include "network/TcpListenerFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include "ui/QQChatSessionFanLX.h"
#include <cassert>
#include <functional>
#include <iostream>

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
    app.login(friendAccount);
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
    const std::map<std::string, std::string> tokens{
        {"2026001", "test-token-a-0123456789"}, {"2026002", "test-token-b-0123456789"},
        {"2026003", "test-token-c-0123456789"}, {"2026004", "test-token-d-0123456789"}};
    auto server = std::make_unique<ImServerFanLX>(app.state(), root / "server", tokens, 0);
    const auto port = server->port();
    auto pump = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + 12s;
        while (!predicate()) {
            if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("IM 测试超时");
            server->tick();
        }
    };
    auto client = [&](const std::string &id) {
        return std::make_unique<ImClientFanLX>("127.0.0.1", port, id, tokens.at(id), root / id);
    };
    auto a = client("2026001");
    pump([&] { return a->online(); });
    a->send("2026002", "offline direct");
    a->send("1001", "offline group", true);
    pump([&] { return a->pending() == 0 && a->history().size() == 2; });
    assert(server->connectionsAccepted() == 1); // 多消息只 connect 一次。
    auto b = client("2026002"); auto c = client("2026003");
    pump([&] { return b->history().size() == 2 && c->history().size() == 1; });
    assert(c->history().front().scope == "group");
    // 非好友既收不到上下线广播，也查不到状态。
    while (auto e = c->pollEvent()) assert(e->type != "PRESENCE");
    c->queryOnline("2026001");
    pump([&] { return c->error().find("只能查询好友") != std::string::npos; });
    bool online = false;
    a->queryOnline("2026002");
    pump([&] {
        while (auto e = a->pollEvent()) if (e->type == "PRESENCE" && e->sender == "2026002" && e->text == "online") online = true;
        return online;
    });
    b.reset();
    bool offline = false;
    pump([&] {
        while (auto e = a->pollEvent()) if (e->type == "PRESENCE" && e->sender == "2026002" && e->text == "offline") offline = true;
        return offline;
    });
    a->send("2026002", "offline after disconnect");
    a->send("1001", "group increment", true);
    pump([&] { return a->pending() == 0 && c->history().size() == 2 && a->history().size() == 4; });
    b = client("2026002");
    pump([&] { return b->history().size() == 4; });
    auto history = b->history();
    assert(history[1].conversationSeq == 1 && history[3].conversationSeq == 2);
    // 服务端重启期间发送，outbox 保留；自动重连后补发。
    server.reset();
    a->send("2026002", "restart one"); a->send("2026002", "restart two");
    a.reset(); a = client("2026001"); // 未发出的 outbox 随客户端生命周期恢复。
    server = std::make_unique<ImServerFanLX>(app.state(), root / "server", tokens, port);
    pump([&] { return a->pending() == 0 && b->history().size() == 6 && a->history().size() == 6; });
    const auto accepted = server->connectionsAccepted();
    a->reconnect();
    pump([&] { return server->connectionsAccepted() > accepted && a->online(); });
    for (int i = 0; i < 8; ++i) server->tick();
    assert(a->history().size() == 6 && b->history().size() == 6);
    b.reset();
    for (int i = 0; i < 3; ++i) server->tick();
    b = client("2026002");
    pump([&] { return b->online(); });
    assert(b->history().size() == 6); // 客户端进程生命周期重建仍保留 inbox。
    const auto liveConnections = server->connectionsAccepted();
    b->send("2026001", "online direct reply");
    pump([&] { return b->pending() == 0 && a->history().size() == 7 && b->history().size() == 7; });
    assert(server->connectionsAccepted() == liveConnections);
    a.reset(); b.reset(); c.reset(); server.reset();

    // 人工控制两个 ACK 丢失窗口，使用真实 TCP 帧而不是模拟存储函数。
    server = std::make_unique<ImServerFanLX>(app.state(), root / "ack-server", tokens, 0);
    const auto rawPort = server->port();
    auto control = [](const std::string &type) { ChatMessageFanLX m; m.type = type; return m; };
    auto receive = [&](TcpPeerFanLX &peer, const std::string &type) {
        std::optional<ChatMessageFanLX> found;
        pump([&] {
            while (auto m = peer.poll()) if (m->type == type) { found = *m; return true; }
            return false;
        });
        return *found;
    };
    auto login = [&](const std::string &user) {
        auto peer = TcpPeerFanLX::connect("127.0.0.1", rawPort);
        auto m = control("LOGIN"); m.sender = user; m.text = tokens.at(user); peer->send(m);
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
    server = std::make_unique<ImServerFanLX>(app.state(), root / "ack-server", tokens, rawPort);
    receiver = login("2026002"); receiver->send(control("SYNC"));
    assert(receive(*receiver, "DELIVER").messageId == delivered.messageId);
    auto ack = control("ACK"); ack.messageId = delivered.messageId; receiver->send(ack);
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
    { ImClientFanLX init("127.0.0.1", rawPort, "2026002", tokens.at("2026002"), crashDir); }
    auto saved = nlohmann::json::parse(AtomicFileWriterFanLX::read(crashDir / "state.json"));
    saved["inbox"][std::to_string(pendingDelivery.messageId)] = nlohmann::json::parse(MessageCodecFanLX::encode(pendingDelivery).substr(4));
    saved["cursors"]["d|2026001|2026002"] = pendingDelivery.conversationSeq;
    AtomicFileWriterFanLX::publish(crashDir, saved.dump(), false);
    ImClientFanLX recovered("127.0.0.1", rawPort, "2026002", tokens.at("2026002"), crashDir);
    pump([&] { return recovered.online(); });
    bool synced = false;
    pump([&] { while (auto e = recovered.pollEvent()) if (e->type == "SYNC_END") synced = true; return synced; });
    assert(recovered.history().size() == 1); // 重放被去重，但再次 ACK，服务端完成同步。
    auto bad = TcpPeerFanLX::connect("127.0.0.1", rawPort);
    auto invalid = control("LOGIN"); invalid.sender = "2026003"; invalid.text = "bad-token"; bad->send(invalid);
    assert(receive(*bad, "ERROR").text == "认证失败");
    std::cout << "IM long connection/offline/group/presence/restart/ACK tests passed\n";
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
        if (argc == 2 && std::wstring(argv[1]) == L"--im") { imTestsFanLX(root / "im"); return 0; }
        SocketRuntimeFanLX runtime;
        // T14a 编码边界：零/超长帧、非法 UTF-8、类型、版本、深嵌套和控制符均拒绝。
        const ChatMessageFanLX valid{"2026001", "2026002", 1, "中文测试"};
        const auto encoded = MessageCodecFanLX::encode(valid);
        assert(MessageCodecFanLX::decode(encoded.substr(4)).text == valid.text);
        failsFanLX([&] { MessageCodecFanLX::encode({"a", "b", 0, "x"}); });
        failsFanLX([&] { MessageCodecFanLX::encode({"a", "b", 1, std::string(4097, 'x')}); });
        failsFanLX([&] { MessageCodecFanLX::encode({"a", "b", 1, "\x1b[2J"}); });
        failsFanLX([&] { MessageCodecFanLX::encode({"a", "b", 1, "\xff"}); });
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
            app.login(owner);
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
            app.login(owner, true);
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
            app.login(friendAccount);
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

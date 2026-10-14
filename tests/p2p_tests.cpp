#ifdef NDEBUG
#undef NDEBUG
#endif
#include "network/DirectLinkFanLX.h"
#include "network/ImClientFanLX.h"
#include "network/ImServerFanLX.h"
#include "network/MessageCodecFanLX.h"
#include "network/P2pProtocolFanLX.h"
#include "network/TcpListenerFanLX.h"
#include "network/TcpPeerFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;

const AccountRefFanLX owner{ServiceTypeFanLX::QQ, "2026001"};

void requireFanLX(bool condition, const char *label) {
    if (!condition)
        throw std::runtime_error(std::string("断言失败: ") + label);
}

template <class FnFanLX> void mustThrowFanLX(FnFanLX fn) {
    bool thrown = false;
    try {
        fn();
    } catch (const std::exception &) {
        thrown = true;
    }
    requireFanLX(thrown, "应当抛出异常");
}

void initializeFanLX(PersistenceApplicationFanLX &app) {
    app.initialize();
    app.importInitial(demoSeedFanLX());
}

bool containsTextFanLX(const std::vector<ChatMessageFanLX> &history, const std::string &text) {
    for (const auto &message : history)
        if (message.text == text)
            return true;
    return false;
}

bool establishedToPortFanLX(unsigned short listenPort) {
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0);
    if (!size)
        return false;
    std::vector<unsigned char> buffer(size);
    if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0) !=
        NO_ERROR)
        return false;
    auto *table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID *>(buffer.data());
    const auto pid = GetCurrentProcessId();
    for (DWORD index = 0; index < table->dwNumEntries; ++index) {
        const auto &row = table->table[index];
        if (row.dwOwningPid != pid || row.dwState != MIB_TCP_STATE_ESTAB)
            continue;
        const auto local = ntohs(static_cast<unsigned short>(row.dwLocalPort));
        const auto remote = ntohs(static_cast<unsigned short>(row.dwRemotePort));
        if (local == listenPort || remote == listenPort)
            return true;
    }
    return false;
}

struct ServerFanLX {
    PersistenceApplicationFanLX app;
    std::unique_ptr<ImServerFanLX> server;
    explicit ServerFanLX(const std::filesystem::path &root, bool sqlite = false) : app(root / "domain") {
        initializeFanLX(app);
        if (!sqlite) {
            server = std::make_unique<ImServerFanLX>(app.state(), root / "server", 0);
            return;
        }
        ImServerFanLX::OptionsFanLX options;
        options.chatDatabasePath = root / "server-chat.db";
        server = std::make_unique<ImServerFanLX>(app.state(), root / "server", 0, "127.0.0.1", options);
    }
    unsigned short port() const { return server->port(); }
    template <class FnFanLX> void pump(FnFanLX predicate, const char *label, std::chrono::milliseconds budget = 12s) {
        const auto deadline = std::chrono::steady_clock::now() + budget;
        while (!predicate()) {
            if (std::chrono::steady_clock::now() > deadline) {
                std::cout << "[pump timeout] " << label << std::endl;
                throw std::runtime_error(std::string("P2P 测试超时: ") + label);
            }
            server->tick();
            std::this_thread::sleep_for(2ms);
        }
    }
};

std::unique_ptr<ImClientFanLX> clientFanLX(unsigned short port, const std::string &id,
                                           const std::filesystem::path &root, bool direct = true,
                                           bool sqlite = false, const std::string &service = "QQ") {
    if (!sqlite)
        return std::make_unique<ImClientFanLX>("127.0.0.1", port, id, demoPasswordFanLX(id), root / id, service,
                                               direct);
    ImClientFanLX::ChatDbConfigFanLX database;
    database.enabled = true;
    return std::make_unique<ImClientFanLX>("127.0.0.1", port, id, demoPasswordFanLX(id), root / id, database,
                                           service, direct);
}

ChatMessageFanLX controlFanLX(const std::string &type) {
    ChatMessageFanLX message;
    message.type = type;
    return message;
}

void protocolTestsFanLX() {
    std::string ip;
    unsigned short port = 0;
    requireFanLX(p2pFanLX::parseEndpointFanLX("ep:127.0.0.1:9000", ip, port) && ip == "127.0.0.1" && port == 9000,
                 "合法端点");
    requireFanLX(!p2pFanLX::parseEndpointFanLX("ep:0.0.0.0:1", ip, port), "拒绝通配地址");
    requireFanLX(!p2pFanLX::parseEndpointFanLX("ep:localhost:1", ip, port), "拒绝域名");
    requireFanLX(!p2pFanLX::parseEndpointFanLX("127.0.0.1:1", ip, port), "拒绝缺少前缀");
    requireFanLX(p2pFanLX::hashBodyFanLX("中文") == p2pFanLX::hashBodyFanLX("中文"), "哈希稳定");
    requireFanLX(p2pFanLX::hashBodyFanLX("中文") != p2pFanLX::hashBodyFanLX("中文 "), "哈希区分正文");
    requireFanLX(p2pFanLX::hashBodyFanLX("hello").rfind("h:", 0) == 0, "哈希前缀");

    ChatMessageFanLX hello;
    hello.type = "P2P_HELLO";
    hello.sender = "2026001";
    hello.text = "ep:127.0.0.1:9";
    const auto frame = MessageCodecFanLX::encode(hello);
    const auto decoded = MessageCodecFanLX::decode(frame.substr(4));
    requireFanLX(decoded.type == "P2P_HELLO" && decoded.text == hello.text, "直连帧往返");
    hello.type = "NOT_A_TYPE";
    mustThrowFanLX([&] { MessageCodecFanLX::encode(hello); });

    TcpListenerFanLX held(0);
    DirectLinkFanLX blocked;
    blocked.bindIdentity("2026001", "QQ");
    requireFanLX(!blocked.start(held.port()), "端口冲突不能假装监听成功");
    requireFanLX(blocked.listenPort() == 0 && blocked.stats().listenFailures >= 1, "端口冲突计数");

    DirectLinkFanLX link;
    link.bindIdentity("2026002", "QQ");
    requireFanLX(link.start(), "临时端口可以监听");
    link.expect("2026009", "ticket-real");
    auto forged = TcpPeerFanLX::connect("127.0.0.1", link.listenPort(), 1s);
    auto auth = controlFanLX("P2P_AUTH");
    auth.sender = "2026009";
    auth.recipient = "2026002";
    auth.requestId = "forged-ticket";
    auth.service = "QQ";
    forged->send(auth);
    bool rejected = false;
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        while (auto event = link.poll())
            requireFanLX(event->kind != DirectLinkFanLX::EventFanLX::KindFanLX::Ready, "伪造票据不能就绪");
        if (forged->ended() || link.stats().rejected >= 1) {
            rejected = true;
            break;
        }
        std::this_thread::sleep_for(5ms);
    }
    requireFanLX(rejected, "伪造票据被拒绝");
    auto garbage = TcpPeerFanLX::connect("127.0.0.1", link.listenPort(), 1s);
    garbage->send(controlFanLX("PING"));
    for (int i = 0; i < 50; ++i) {
        while (link.poll()) {
        }
        std::this_thread::sleep_for(10ms);
    }
    requireFanLX(link.listening(), "异常帧不能把监听打掉");
}

void happyPathFanLX(const std::filesystem::path &root, bool sqlite) {
    ServerFanLX host(root, sqlite);
    auto left = clientFanLX(host.port(), "2026001", root, true, sqlite);
    auto right = clientFanLX(host.port(), "2026002", root, true, sqlite);
    host.pump([&] { return host.server->directRouteStats().directSuccess >= 1; }, "direct-ready");
    requireFanLX(establishedToPortFanLX(left->directListenPort()) ||
                     establishedToPortFanLX(right->directListenPort()),
                 "两个客户端之间有 TCP 连接");
    const char *texts[] = {"p2p-1", "p2p-2", "p2p-3"};
    for (const char *text : texts)
        left->send("2026002", text);
    host.pump(
        [&] {
            return containsTextFanLX(right->history(), "p2p-3") && containsTextFanLX(left->history(), "p2p-3") &&
                   right->history().size() >= 3 && left->history().size() >= 3;
        },
        "three-messages");
    std::vector<ChatMessageFanLX> got;
    for (const auto &message : right->history())
        if (message.text.rfind("p2p-", 0) == 0)
            got.push_back(message);
    requireFanLX(got.size() == 3, "接收方没有重复消息");
    for (std::size_t i = 1; i < got.size(); ++i) {
        requireFanLX(got[i].messageId > got[i - 1].messageId, "消息编号递增");
        requireFanLX(got[i].conversationSeq > got[i - 1].conversationSeq, "会话序号递增");
    }
    requireFanLX(containsTextFanLX(left->history(), "p2p-1") && containsTextFanLX(left->history(), "p2p-2"),
                 "发送方历史一致");
    const auto serverStats = host.server->directRouteStats();
    const auto received = right->directStats();
    requireFanLX(serverStats.p2pMessages >= 3, "服务器统计了直连投递");
    requireFanLX(serverStats.peerRelayBodies == 0, "这三条文没有经服务器把正文发给接收方");
    requireFanLX(serverStats.fallbacks == 0, "确认路径没有触发回退");
    requireFanLX(received.p2pReceived >= 3 && left->directStats().p2pSent >= 3, "正文经过直连套接字");
}

void relayWhenDirectDisabledFanLX(const std::filesystem::path &root) {
    ServerFanLX host(root, false);
    auto left = clientFanLX(host.port(), "2026001", root, false);
    auto right = clientFanLX(host.port(), "2026002", root, false);
    host.pump([&] { return left->online() && right->online(); }, "relay-online");
    left->send("2026002", "relay-only");
    host.pump([&] { return containsTextFanLX(right->history(), "relay-only"); }, "relay-delivered");
    const auto stats = host.server->directRouteStats();
    requireFanLX(stats.p2pMessages == 0 && stats.peerRelayBodies >= 1, "未监听时走中转");
}

void handshakeFailureFallsBackFanLX(const std::filesystem::path &root) {
    ServerFanLX host(root, false);
    auto sender = clientFanLX(host.port(), "2026001", root, true);
    host.pump([&] { return sender->online(); }, "sender-online");
    auto receiver = TcpPeerFanLX::connect("127.0.0.1", host.port(), 2s);
    auto login = controlFanLX("LOGIN");
    login.sender = "2026002";
    login.text = demoPasswordFanLX("2026002");
    receiver->send(login);
    bool helloSent = false;
    bool sent = false;
    ChatMessageFanLX delivered;
    host.pump(
        [&] {
            while (auto message = receiver->poll()) {
                if (message->type == "LOGIN_OK" && !helloSent) {
                    auto hello = controlFanLX("P2P_HELLO");
                    hello.sender = "2026002";
                    hello.text = "ep:127.0.0.1:1";
                    receiver->send(hello);
                    helloSent = true;
                }
                if (message->type == "DELIVER" && message->text == "after-fail")
                    delivered = *message;
            }
            if (helloSent && !sent && host.server->directRouteStats().directFail >= 1) {
                sender->send("2026002", "after-fail");
                sent = true;
            }
            return delivered.messageId != 0;
        },
        "handshake-fallback",
        15s);
    requireFanLX(delivered.text == "after-fail" && host.server->directRouteStats().p2pMessages == 0,
                 "握手失败后正文由服务器送达");
}

void lostAckFallsBackFanLX(const std::filesystem::path &root) {
    ServerFanLX host(root, false);
    DirectLinkFanLX acceptor;
    acceptor.bindIdentity("2026002", "QQ");
    requireFanLX(acceptor.start(), "接收方监听");
    auto sender = clientFanLX(host.port(), "2026001", root, true);
    auto receiver = TcpPeerFanLX::connect("127.0.0.1", host.port(), 2s);
    auto login = controlFanLX("LOGIN");
    login.sender = "2026002";
    login.text = demoPasswordFanLX("2026002");
    receiver->send(login);
    bool announced = false;
    bool readySent = false;
    bool sent = false;
    bool secondSent = false;
    ChatMessageFanLX arm;
    ChatMessageFanLX delivered;
    ChatMessageFanLX second;
    host.pump(
        [&] {
            while (auto message = receiver->poll()) {
                if (message->type == "LOGIN_OK" && !announced) {
                    auto hello = controlFanLX("P2P_HELLO");
                    hello.sender = "2026002";
                    hello.text = acceptor.advertisedEndpoint();
                    receiver->send(hello);
                    announced = true;
                } else if (message->type == "P2P_EXPECT") {
                    acceptor.expect(message->recipient, message->requestId);
                } else if (message->type == "P2P_ARM" && message->requestId.size()) {
                    arm = *message;
                } else if (message->type == "DELIVER" && message->text == "needs-relay") {
                    delivered = *message;
                } else if (message->type == "DELIVER" && message->text == "after-cursor") {
                    second = *message;
                }
            }
            while (auto event = acceptor.poll()) {
                if (event->kind == DirectLinkFanLX::EventFanLX::KindFanLX::Ready && !readySent) {
                    auto ready = controlFanLX("P2P_READY");
                    ready.sender = "2026002";
                    ready.recipient = "2026001";
                    ready.requestId = event->ticket;
                    receiver->send(ready);
                    readySent = true;
                }
            }
            if (!sent && host.server->directRouteStats().directSuccess >= 1) {
                sender->send("2026002", "needs-relay");
                sent = true;
            }
            if (delivered.messageId && !secondSent) {
                auto ack = controlFanLX("ACK");
                ack.messageId = delivered.messageId;
                ack.conversationSeq = delivered.conversationSeq;
                receiver->send(ack);
                sender->send("2026002", "after-cursor");
                secondSent = true;
            }
            return second.messageId != 0;
        },
        "lost-ack-fallback",
        15s);
    requireFanLX(arm.messageId != 0 && arm.text == p2pFanLX::hashBodyFanLX("needs-relay"), "协商帧只有哈希");
    requireFanLX(arm.text != "needs-relay" && delivered.text == "needs-relay", "超时后中转补发正文");
    requireFanLX(host.server->directRouteStats().fallbacks >= 1, "回退计数");
    requireFanLX(second.text == "after-cursor" && second.conversationSeq > delivered.conversationSeq,
                 "游标没有卡住");
}

void quitAndReplayFanLX(const std::filesystem::path &root) {
    ServerFanLX host(root, false);
    auto left = clientFanLX(host.port(), "2026001", root, true);
    auto right = clientFanLX(host.port(), "2026002", root, true);
    host.pump([&] { return host.server->directRouteStats().directSuccess >= 1; }, "quit-ready");
    left->send("2026002", "before-quit");
    host.pump([&] { return containsTextFanLX(right->history(), "before-quit"); }, "before-quit-arrived");
    right.reset();
    host.pump([&] { return true; }, "drop-session");
    for (int i = 0; i < 20; ++i)
        host.server->tick();
    left->send("2026002", "while-offline");
    host.pump([&] { return left->pending() == 0; }, "offline-accepted");
    right = clientFanLX(host.port(), "2026002", root, true);
    host.pump([&] { return containsTextFanLX(right->history(), "while-offline"); }, "replay-after-login");
    std::size_t copies = 0;
    for (const auto &message : right->history())
        if (message.text == "before-quit")
            ++copies;
    requireFanLX(copies == 1, "重新登录没有把旧消息复制一份");
}

void groupAndAccessFanLX(const std::filesystem::path &root) {
    ServerFanLX host(root, false);
    auto left = clientFanLX(host.port(), "2026001", root, true);
    auto right = clientFanLX(host.port(), "2026002", root, true);
    auto groupMember = clientFanLX(host.port(), "2026003", root, false);
    auto weibo = clientFanLX(host.port(), "2026002", root / "weibo", false, false, "Weibo");
    host.pump([&] { return left->online() && right->online() && groupMember->online() && weibo->online(); },
              "group-online");
    host.pump([&] { return host.server->directRouteStats().directSuccess >= 1; }, "group-direct-ready", 8s);
    const auto before = host.server->directRouteStats().p2pMessages;
    left->send("1001", "group-stays-relay", true);
    host.pump([&] { return containsTextFanLX(groupMember->history(), "group-stays-relay"); }, "group-delivered");
    const auto after = host.server->directRouteStats();
    requireFanLX(after.groupRelayMessages >= 1 && after.p2pMessages == before, "群消息仍走中转");
    left->send("2026002", "qq-only-body");
    host.pump([&] { return containsTextFanLX(right->history(), "qq-only-body"); }, "qq-dm");
    for (int i = 0; i < 30; ++i)
        host.server->tick();
    requireFanLX(!containsTextFanLX(weibo->history(), "qq-only-body"), "微博通道看不到 QQ 直连消息");

    auto stranger = TcpPeerFanLX::connect("127.0.0.1", host.port(), 2s);
    auto login = controlFanLX("LOGIN");
    login.sender = "2026004";
    login.text = demoPasswordFanLX("2026004");
    stranger->send(login);
    bool loggedIn = false;
    ChatMessageFanLX error;
    host.pump(
        [&] {
            while (auto message = stranger->poll()) {
                if (message->type == "LOGIN_OK" && !loggedIn) {
                    auto send = controlFanLX("SEND");
                    send.sender = "2026004";
                    send.recipient = "1001";
                    send.scope = "group";
                    send.text = "not-member";
                    send.requestId = "stranger-group";
                    stranger->send(send);
                    loggedIn = true;
                }
                if (message->type == "ERROR")
                    error = *message;
            }
            return error.type == "ERROR";
        },
        "stranger-rejected");
    requireFanLX(error.text.find("群成员") != std::string::npos, "非成员不能借直连绕过群权限");
    requireFanLX(!containsTextFanLX(groupMember->history(), "not-member"), "越权群消息没有入库");
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("fanlx-p2p-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    try {
        protocolTestsFanLX();
        happyPathFanLX(root / "json", false);
        happyPathFanLX(root / "sqlite", true);
        relayWhenDirectDisabledFanLX(root / "relay");
        handshakeFailureFallsBackFanLX(root / "handshake");
        lostAckFallsBackFanLX(root / "fallback");
        quitAndReplayFanLX(root / "quit");
        groupAndAccessFanLX(root / "group");
    } catch (...) {
        std::filesystem::remove_all(root);
        throw;
    }
    std::filesystem::remove_all(root);
    std::cout << "p2p tests passed\n";
    return 0;
}

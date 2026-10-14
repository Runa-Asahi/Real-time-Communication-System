#include "network/DirectLinkFanLX.h"
#include "network/P2pProtocolFanLX.h"
#include "network/TcpListenerFanLX.h"
#include "SocketIoFanLX.h"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

std::string bindAddressFanLX() {
    const char *configured = std::getenv("FANLX_P2P_BIND");
    if (!configured || !*configured)
        return "127.0.0.1";
    std::string ip;
    unsigned short ignored = 0;
    // 用同一套解析拒绝 0.0.0.0 和域名。端口只是占位，解析完丢掉。
    if (!p2pFanLX::parseEndpointFanLX(std::string("ep:") + configured + ":1", ip, ignored))
        return "127.0.0.1";
    return ip;
}
} // namespace

struct DirectLinkFanLX::Impl {
    struct ExpectationFanLX {
        std::string ticket;
        std::uint64_t generation = 0;
        Clock::time_point expires = Clock::now();
        bool connector = false;
    };
    struct LinkFanLX {
        std::string peer;
        std::string ticket;
        std::uint64_t generation = 0;
        std::unique_ptr<TcpPeerFanLX> socket;
        bool inbound = false;
        bool ready = false;
        bool dropped = false;
        bool held = false;
        ChatMessageFanLX heldAuth;
        Clock::time_point deadline = Clock::now();
    };
    struct JobFanLX {
        std::string peer;
        std::string ticket;
        std::string ip;
        unsigned short port = 0;
        std::uint64_t generation = 0;
    };
    struct ConnectResultFanLX {
        std::string peer;
        std::string ticket;
        std::uint64_t generation = 0;
        std::unique_ptr<TcpPeerFanLX> socket;
        std::string error;
    };

    std::string account;
    std::string service = "QQ";
    std::string bindIp = "127.0.0.1";
    std::string endpoint;
    std::string listenError;
    unsigned short port = 0;
    std::atomic<bool> running{false};
    std::unique_ptr<TcpListenerFanLX> listener;
    std::thread acceptThread;
    std::thread connectThread;

    mutable std::mutex mutex;
    std::condition_variable jobsReady;
    std::deque<JobFanLX> jobs;
    std::vector<std::unique_ptr<TcpPeerFanLX>> accepted;
    std::vector<ConnectResultFanLX> results;
    std::map<std::string, ExpectationFanLX> expects;
    std::map<std::string, LinkFanLX> links;
    std::vector<LinkFanLX> inbound;
    std::deque<EventFanLX> events;
    std::atomic<std::size_t> listenFailures{0};
    std::atomic<std::size_t> connectAttempts{0};
    std::atomic<std::size_t> connectSuccess{0};
    std::atomic<std::size_t> connectFail{0};
    std::atomic<std::size_t> rejected{0};
    std::atomic<std::size_t> dataSent{0};
    std::atomic<std::size_t> dataReceived{0};
    bool stopRequested = false;

    ~Impl() { stop(); }

    void logFanLX(const std::string &detail) const {
        std::cerr << "fanlx-p2p client " << detail << '\n';
    }

    void stop() noexcept {
        {
            std::lock_guard<std::mutex> guard(mutex);
            stopRequested = true;
            running = false;
        }
        jobsReady.notify_all();
        if (acceptThread.joinable())
            acceptThread.join();
        if (connectThread.joinable())
            connectThread.join();
        listener.reset();
        std::lock_guard<std::mutex> guard(mutex);
        accepted.clear();
        results.clear();
        jobs.clear();
        links.clear();
        inbound.clear();
        endpoint.clear();
        port = 0;
    }

    void acceptLoop() noexcept {
        while (running.load()) {
            try {
                auto peer = listener->accept(std::chrono::milliseconds(200));
                std::lock_guard<std::mutex> guard(mutex);
                if (accepted.size() >= 8) {
                    ++rejected;
                    continue; // unique_ptr 离开作用域即关闭，避免异常连接堆住进程。
                }
                accepted.push_back(std::move(peer));
            } catch (const std::exception &error) {
                if (!running.load())
                    break;
                if (std::string(error.what()) == "网络操作超时")
                    continue;
                std::lock_guard<std::mutex> guard(mutex);
                listenError = error.what();
            } catch (...) {
                if (!running.load())
                    break;
            }
        }
    }

    void connectLoop() noexcept {
        for (;;) {
            JobFanLX job;
            {
                std::unique_lock<std::mutex> guard(mutex);
                jobsReady.wait(guard, [&] { return stopRequested || !jobs.empty(); });
                if (stopRequested && jobs.empty())
                    return;
                if (jobs.empty())
                    continue;
                job = std::move(jobs.front());
                jobs.pop_front();
                ++connectAttempts;
            }
            ConnectResultFanLX result;
            result.peer = job.peer;
            result.ticket = job.ticket;
            result.generation = job.generation;
            try {
                result.socket = TcpPeerFanLX::connect(job.ip, job.port, std::chrono::milliseconds(800));
            } catch (const std::exception &error) {
                result.error = error.what();
            } catch (...) {
                result.error = "connect";
            }
            std::lock_guard<std::mutex> guard(mutex);
            if (stopRequested)
                return;
            results.push_back(std::move(result));
        }
    }

    void pushEvent(EventFanLX event) { events.push_back(std::move(event)); }

    void failPeer(const std::string &peer, const std::string &ticket, const std::string &reason) {
        ++connectFail;
        EventFanLX event;
        event.kind = EventFanLX::KindFanLX::Failed;
        event.peer = peer;
        event.ticket = ticket;
        event.detail = reason;
        pushEvent(std::move(event));
        logFanLX("route=DirectDegraded peer=" + peer + " reason=" + reason);
    }

    bool ticketCurrent(const std::string &peer, const std::string &ticket, std::uint64_t generation) const {
        const auto found = expects.find(peer);
        return found != expects.end() && found->second.ticket == ticket &&
               found->second.generation == generation && Clock::now() < found->second.expires;
    }

    void sendAuth(LinkFanLX &link) {
        auto auth = socketIoFanLX::control(p2pFanLX::authTypeFanLX);
        auth.sender = account;
        auth.recipient = link.peer;
        auth.requestId = link.ticket;
        auth.service = service;
        auth.scope = "direct";
        link.socket->send(auth);
        link.deadline = Clock::now() + std::chrono::seconds(3);
    }

    void takeIo() {
        std::vector<std::unique_ptr<TcpPeerFanLX>> fresh;
        std::vector<ConnectResultFanLX> done;
        {
            std::lock_guard<std::mutex> guard(mutex);
            fresh.swap(accepted);
            done.swap(results);
        }
        for (auto &peer : fresh) {
            if (inbound.size() >= 8) {
                ++rejected;
                continue;
            }
            LinkFanLX link;
            link.socket = std::move(peer);
            link.inbound = true;
            link.deadline = Clock::now() + std::chrono::seconds(3);
            inbound.push_back(std::move(link));
        }
        for (auto &result : done) {
            if (!ticketCurrent(result.peer, result.ticket, result.generation))
                continue; // 已被更新的协商取代，不能把失败算到新票据上。
            if (!result.socket || result.socket->ended()) {
                failPeer(result.peer, result.ticket,
                         result.error.find("超时") != std::string::npos ? "timeout" : "connect");
                continue;
            }
            LinkFanLX link;
            link.peer = result.peer;
            link.ticket = result.ticket;
            link.generation = result.generation;
            link.socket = std::move(result.socket);
            try {
                sendAuth(link);
            } catch (const std::exception &) {
                failPeer(result.peer, result.ticket, "connect");
                continue;
            }
            ++connectSuccess;
            links[result.peer] = std::move(link);
        }
    }

    void dropLink(LinkFanLX &link, const std::string &reason, bool notify) {
        if (link.dropped)
            return;
        link.dropped = true;
        if (notify && !link.peer.empty()) {
            if (link.ready) {
                EventFanLX event;
                event.kind = EventFanLX::KindFanLX::Closed;
                event.peer = link.peer;
                event.ticket = link.ticket;
                event.detail = reason;
                pushEvent(std::move(event));
                logFanLX("route=DirectDegraded peer=" + link.peer + " reason=" + reason);
            } else {
                failPeer(link.peer, link.ticket, reason);
            }
        }
        if (link.socket)
            link.socket->close();
    }

    bool acceptAuth(LinkFanLX &link, const ChatMessageFanLX &message) {
        if (message.type != p2pFanLX::authTypeFanLX || message.requestId.empty() ||
            message.sender.empty() || message.recipient != account || message.service != service)
            return false;
        const auto found = expects.find(message.sender);
        if (found == expects.end() || found->second.connector || found->second.ticket != message.requestId ||
            Clock::now() >= found->second.expires)
            return false;
        link.peer = message.sender;
        link.ticket = message.requestId;
        link.generation = found->second.generation;
        link.ready = true;
        auto ok = socketIoFanLX::control(p2pFanLX::authOkTypeFanLX);
        ok.sender = account;
        ok.recipient = link.peer;
        ok.requestId = link.ticket;
        ok.service = service;
        link.socket->send(ok);
        expects.erase(found);
        EventFanLX event;
        event.kind = EventFanLX::KindFanLX::Ready;
        event.peer = link.peer;
        event.ticket = link.ticket;
        pushEvent(std::move(event));
        logFanLX("route=DirectReady peer=" + link.peer);
        return true;
    }

    void onFrame(LinkFanLX &link, const ChatMessageFanLX &message) {
        if (!link.ready && link.inbound) {
            if (message.type == p2pFanLX::authTypeFanLX && expects.empty()) {
                // 票据帧可能比 TCP 握手晚到。先留着，等到截止时间再拒绝。
                link.held = true;
                link.heldAuth = message;
                return;
            }
            if (!acceptAuth(link, message)) {
                ++rejected;
                link.dropped = true;
                link.socket->close();
                link.deadline = Clock::time_point::min();
            }
            return;
        }
        if (!link.ready && message.type == p2pFanLX::authOkTypeFanLX) {
            if (message.requestId != link.ticket || message.sender != link.peer ||
                message.recipient != account || !ticketCurrent(link.peer, link.ticket, link.generation)) {
                ++rejected;
                dropLink(link, "rejected", true);
                return;
            }
            link.ready = true;
            expects.erase(link.peer);
            EventFanLX event;
            event.kind = EventFanLX::KindFanLX::Ready;
            event.peer = link.peer;
            event.ticket = link.ticket;
            pushEvent(std::move(event));
            logFanLX("route=DirectReady peer=" + link.peer);
            return;
        }
        if (link.ready && message.type == p2pFanLX::dataTypeFanLX) {
            if (message.sender != link.peer || message.recipient != account || message.messageId == 0) {
                ++rejected;
                return;
            }
            ++dataReceived;
            EventFanLX event;
            event.kind = EventFanLX::KindFanLX::Data;
            event.peer = link.peer;
            event.message = message;
            pushEvent(std::move(event));
            return;
        }
        ++rejected;
        dropLink(link, "rejected", true);
    }

    void pollLink(LinkFanLX &link) {
        if (!link.socket)
            return;
        if (link.held && !link.ready) {
            try {
                if (acceptAuth(link, link.heldAuth))
                    link.held = false;
            } catch (const std::exception &) {
                dropLink(link, "rejected", true);
                return;
            }
        }
        if (!link.ready && Clock::now() > link.deadline) {
            dropLink(link, "timeout", true);
            return;
        }
        for (int budget = 0; budget < 16; ++budget) {
            auto message = link.socket->poll();
            if (!message)
                break;
            try {
                onFrame(link, *message);
            } catch (const std::exception &) {
                dropLink(link, "rejected", true);
                return;
            }
            if (!link.socket || link.socket->ended())
                break;
        }
        if (link.socket && !link.socket->ended())
            link.socket->flushSend(8);
        if (link.socket && link.socket->ended())
            dropLink(link, "closed", link.ready || !link.peer.empty());
    }
};

DirectLinkFanLX::DirectLinkFanLX() : implFanLX(std::make_unique<Impl>()) {}
DirectLinkFanLX::~DirectLinkFanLX() { stop(); }

void DirectLinkFanLX::bindIdentity(std::string account, std::string service) {
    implFanLX->account = std::move(account);
    if (!service.empty())
        implFanLX->service = std::move(service);
}

bool DirectLinkFanLX::start(unsigned short port) {
    auto &impl = *implFanLX;
    if (impl.running.load())
        return true;
    impl.bindIp = bindAddressFanLX();
    impl.stopRequested = false;
    try {
        impl.listener = std::make_unique<TcpListenerFanLX>(port, impl.bindIp);
        impl.port = impl.listener->port();
        impl.endpoint = p2pFanLX::formatEndpointFanLX(impl.bindIp, impl.port);
        impl.listenError.clear();
        impl.running = true;
        impl.acceptThread = std::thread([this] { implFanLX->acceptLoop(); });
        impl.connectThread = std::thread([this] { implFanLX->connectLoop(); });
        impl.logFanLX("listen " + impl.endpoint);
        return true;
    } catch (const std::exception &error) {
        impl.listener.reset();
        impl.endpoint.clear();
        impl.port = 0;
        impl.running = false;
        impl.listenError = error.what();
        ++impl.listenFailures;
        impl.logFanLX("listen-failed");
        return false;
    }
}

void DirectLinkFanLX::stop() noexcept {
    if (implFanLX)
        implFanLX->stop();
}

bool DirectLinkFanLX::listening() const { return implFanLX->running.load(); }
unsigned short DirectLinkFanLX::listenPort() const { return implFanLX->port; }
std::string DirectLinkFanLX::advertisedEndpoint() const { return implFanLX->endpoint; }
std::string DirectLinkFanLX::listenError() const {
    std::lock_guard<std::mutex> guard(implFanLX->mutex);
    return implFanLX->listenError;
}

void DirectLinkFanLX::expect(const std::string &peer, const std::string &ticket) {
    auto &impl = *implFanLX;
    Impl::ExpectationFanLX expectation;
    expectation.ticket = ticket;
    expectation.generation = ++impl.expects[peer].generation;
    expectation.expires = Clock::now() + std::chrono::seconds(30);
    expectation.connector = false;
    const auto generation = expectation.generation;
    impl.expects[peer] = std::move(expectation);
    impl.expects[peer].generation = generation;
    auto found = impl.links.find(peer);
    if (found != impl.links.end()) {
        found->second.socket->close();
        impl.links.erase(found);
    }
}

void DirectLinkFanLX::connectPeer(const std::string &peer, const std::string &ticket,
                                  const std::string &endpoint) {
    auto &impl = *implFanLX;
    std::string ip;
    unsigned short port = 0;
    if (!p2pFanLX::parseEndpointFanLX(endpoint, ip, port)) {
        impl.failPeer(peer, ticket, "connect");
        return;
    }
    Impl::ExpectationFanLX expectation;
    expectation.ticket = ticket;
    expectation.generation = ++impl.expects[peer].generation;
    expectation.expires = Clock::now() + std::chrono::seconds(30);
    expectation.connector = true;
    const auto generation = expectation.generation;
    impl.expects[peer] = expectation;
    impl.expects[peer].generation = generation;
    auto existing = impl.links.find(peer);
    if (existing != impl.links.end()) {
        existing->second.socket->close();
        impl.links.erase(existing);
    }
    Impl::JobFanLX job;
    job.peer = peer;
    job.ticket = ticket;
    job.ip = ip;
    job.port = port;
    job.generation = generation;
    {
        std::lock_guard<std::mutex> guard(impl.mutex);
        impl.jobs.push_back(std::move(job));
    }
    impl.jobsReady.notify_one();
}

void DirectLinkFanLX::closePeer(const std::string &peer) {
    auto &impl = *implFanLX;
    impl.expects.erase(peer);
    auto found = impl.links.find(peer);
    if (found == impl.links.end())
        return;
    found->second.socket->close();
    impl.links.erase(found);
}

void DirectLinkFanLX::closePeers() noexcept {
    auto &impl = *implFanLX;
    impl.expects.clear();
    impl.links.clear();
    impl.inbound.clear();
}

bool DirectLinkFanLX::sendData(const std::string &peer, const ChatMessageFanLX &message) {
    auto &impl = *implFanLX;
    auto found = impl.links.find(peer);
    if (found == impl.links.end() || !found->second.ready || !found->second.socket ||
        found->second.socket->ended())
        return false;
    if (!found->second.socket->trySend(message))
        return false;
    if (!found->second.socket->flushSend(8))
        return false;
    ++impl.dataSent;
    return true;
}

std::optional<DirectLinkFanLX::EventFanLX> DirectLinkFanLX::poll() {
    auto &impl = *implFanLX;
    impl.takeIo();
    for (auto iterator = impl.inbound.begin(); iterator != impl.inbound.end();) {
        impl.pollLink(*iterator);
        if (!iterator->socket || iterator->socket->ended() || iterator->deadline == Clock::time_point::min()) {
            iterator = impl.inbound.erase(iterator);
            continue;
        }
        if (iterator->ready && !iterator->peer.empty()) {
            auto existing = impl.links.find(iterator->peer);
            if (existing != impl.links.end() && existing->second.socket)
                existing->second.socket->close();
            impl.links.insert_or_assign(iterator->peer, std::move(*iterator));
            iterator = impl.inbound.erase(iterator);
            continue;
        }
        ++iterator;
    }
    for (auto iterator = impl.links.begin(); iterator != impl.links.end();) {
        impl.pollLink(iterator->second);
        if (!iterator->second.socket || iterator->second.socket->ended())
            iterator = impl.links.erase(iterator);
        else
            ++iterator;
    }
    if (impl.events.empty())
        return std::nullopt;
    auto event = std::move(impl.events.front());
    impl.events.pop_front();
    return event;
}

DirectLinkFanLX::StatsFanLX DirectLinkFanLX::stats() const {
    auto &impl = *implFanLX;
    StatsFanLX stats;
    stats.listenFailures = impl.listenFailures.load();
    stats.connectAttempts = impl.connectAttempts.load();
    stats.connectSuccess = impl.connectSuccess.load();
    stats.connectFail = impl.connectFail.load();
    stats.rejected = impl.rejected.load();
    stats.dataSent = impl.dataSent.load();
    stats.dataReceived = impl.dataReceived.load();
    return stats;
}

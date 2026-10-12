#include "network/ImClientFanLX.h"
#include "SocketIoFanLX.h"
#include "db/ChatSchemaStoreFanLX.h"
#include "db/DatabaseWorkerFanLX.h"
#include "db/SqliteChatRepositoryFanLX.h"
#include "infra/UtcClockFanLX.h"
#include "network/MessageCodecFanLX.h"
#include "network/TcpPeerFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include "persistence/DataDirectoryLockFanLX.h"
#include "persistence/NamedFilePublisherFanLX.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <limits>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <system_error>
#include <thread>

using namespace chatDbFanLX;

namespace {

constexpr std::size_t eventQueueCapacityFanLX = 2048;
constexpr std::size_t commandQueueCapacityFanLX = 128;

std::string randomDeviceIdFanLX() {
    std::random_device random;
    std::string device;
    for (int index = 0; index < 4; ++index)
        device += std::to_string(random()) + "-";
    return device;
}

// 归档后缀：与工程其它"改名保留而不是删除"的约定一致（见 run-im-server.ps1 的
// <数据目录>.legacy-<时间戳>）。UTC 时钟去掉 '-' 与 ':' 后就是合法且可排序的文件名后缀。
// 必须写 ::utcNowFanLX：本文件 using 了 chatDbFanLX，那个命名空间里也有一个同名函数。
std::string legacyStampFanLX() {
    std::string stamp = ::utcNowFanLX();
    stamp.erase(std::remove_if(stamp.begin(), stamp.end(),
                               [](char value) { return value == '-' || value == ':'; }),
                stamp.end());
    return stamp.empty() ? std::string("legacy") : stamp;
}

ScopeFanLX scopeOfMessageFanLX(const ChatMessageFanLX &message) {
    return message.scope == "group" ? ScopeFanLX::Group : ScopeFanLX::Direct;
}

} // namespace

struct ImClientFanLX::Impl {
    using Clock = std::chrono::steady_clock;

    // 网络命令：工作线程只执行网络动作，不做任何磁盘操作。
    struct CommandFanLX {
        enum class KindFanLX { Send, Sync };
        KindFanLX kind = KindFanLX::Send;
        ChatMessageFanLX message;
    };

    std::string host, account, password;
    // 本连接所属服务（"QQ" / "Wechat"）：出站帧统一带上它，服务端据此选择好友/群分区。
    std::string service;
    unsigned short port = 0;
    std::filesystem::path directory;
    std::unique_ptr<DataDirectoryLockFanLX> directoryLock;
    nlohmann::json identity;
    std::string device;
    std::string instanceId;
    bool sqliteEnabled = false;

    std::unique_ptr<DatabaseWorkerFanLX> database;
    mutable std::mutex databaseMutex; // 保护 database 指针与"主线程直接读仓储"的调用
    std::atomic<bool> sqliteReady{false};
    std::string storageError;
    // 存储初始化在工作线程内只尝试一次：失败后不再反复重试，错误经事件队列报告给界面。
    bool storageAttempted = false;
    bool storageErrorSet = false;

    // 工作线程调用：按服务器实例号打开聊天库。
    // 只对 SQLite 后端使用（旧 JSON 后端已在构造函数内完成初始化）。
    void prepareStorageFanLX() noexcept {
        storageAttempted = true;
        if (!sqliteEnabled)
            return;
        try {
            openSqliteFanLX(instanceId);
            // 打开库之后、第一次补发之前，先把历史遗留的"毒消息"清掉。
            sweepUnsendablePendingFanLX();
        } catch (const std::exception &error) {
            storageError = error.what();
            storageErrorSet = true;
            ImClientEventFanLX out;
            out.kind = ImClientEventFanLX::KindFanLX::Error;
            out.detail = storageError;
            pushEvent(std::move(out));
        } catch (...) {
            storageError = "聊天存储初始化失败";
            storageErrorSet = true;
            ImClientEventFanLX out;
            out.kind = ImClientEventFanLX::KindFanLX::Error;
            out.detail = storageError;
            pushEvent(std::move(out));
        }
    }

    mutable std::mutex jsonMutex;
    nlohmann::json legacyDb;
    bool legacyEnabled = false;

    std::mutex commandMutex;
    std::condition_variable commandReady;
    std::deque<CommandFanLX> commands;

    mutable std::mutex eventMutex;
    std::deque<ImClientEventFanLX> events;
    std::size_t droppedEvents = 0;

    // 命令关联表：数据库结果回到网络动作需要知道原始命令。
    std::mutex correlationMutex;
    std::map<std::int64_t, ChatMessageFanLX> pendingDeliveries;
    std::map<std::int64_t, std::string> pendingSends; // commandId → requestId
    std::map<std::int64_t, std::string> sendByRequest; // requestId → commandId（供 SEND_ACK 关联）

    std::atomic<std::int64_t> nextQueryId{1};
    std::atomic<std::int64_t> nextCommandId{1};
    std::atomic<bool> stop{false}, active{false}, reset{false};
    mutable std::mutex errorMutex;
    std::string lastError;
    std::string lastReportedError;
    // 服务器对本次登录的明确拒绝（"用户名或密码错误"等）：与"网络断了"区分开，
    // 界面凭它把登录失败留在登录窗口，而不是进了主界面再报错。
    bool loginRejected = false;
    std::string loginRejectReason;
    std::thread worker;

    Impl(std::string ip, unsigned short p, std::string user, std::string secret,
         const std::filesystem::path &dir, bool chatDbEnabled, std::string channelService)
        : host(std::move(ip)), account(std::move(user)), password(std::move(secret)),
          service(std::move(channelService)), port(p), directory(dir), sqliteEnabled(chatDbEnabled) {
        if (account.empty() || !port || password.empty())
            throw std::invalid_argument("IM 连接参数缺失");
        if (service != "QQ" && service != "Wechat" && service != "Weibo")
            throw std::invalid_argument("聊天通道只支持 QQ / 微信 / 微博（微X 是上层登录身份）");
        std::error_code ignored;
        std::filesystem::create_directories(directory, ignored);
        directoryLock = std::make_unique<DataDirectoryLockFanLX>(directory);
        // 旧 JSON 后端保持"构造即就绪"的既有语义（既有调用方与回归测试依赖它，且不涉及网络/SQLite）。
        // SQLite 后端需要先与服务器握手取得实例号，因此放到工作线程，保证界面线程的启动是即时的。
        if (!sqliteEnabled)
            openLegacyFanLX();
        worker = std::thread([this] { run(); });
    }

    ~Impl() {
        stop = true;
        commandReady.notify_all();
        {
            std::lock_guard<std::mutex> guard(databaseMutex);
            if (database)
                database->requestStop();
        }
        if (database) {
            database->join();
            database.reset();
        }
        if (worker.joinable())
            worker.join();
    }

    // ---- 工具 ----
    void error(const std::string &text) {
        std::lock_guard<std::mutex> guard(errorMutex);
        lastError = text;
    }
    // 服务器明确拒绝登录：记下原因供界面显示（此时连接会被服务端关闭，不会进入在线态）。
    void rejectLogin(const std::string &text) {
        std::lock_guard<std::mutex> guard(errorMutex);
        loginRejected = true;
        loginRejectReason = text;
    }
    bool loginWasRejected(std::string *reason) const {
        std::lock_guard<std::mutex> guard(errorMutex);
        if (reason)
            *reason = loginRejectReason;
        return loginRejected;
    }
    std::string errorText() const {
        std::lock_guard<std::mutex> guard(errorMutex);
        return lastError;
    }
    // 连接层错误去重后作为事件报告：界面据此显示"连接失败/重连中"，
    // 而不是让用户误以为已经在线；同一原因只报一次，避免重连风暴刷屏。
    void reportConnectionErrorFanLX() {
        const auto text = errorText();
        if (text.empty() || text == lastReportedError)
            return;
        lastReportedError = text;
        ImClientEventFanLX out;
        out.kind = ImClientEventFanLX::KindFanLX::Connection;
        out.detail = text;
        pushEvent(std::move(out));
    }
    void pushEvent(ImClientEventFanLX event) {
        std::lock_guard<std::mutex> guard(eventMutex);
        if (events.size() >= eventQueueCapacityFanLX) {
            // 事件丢失只影响提示；历史事实源始终是数据库，界面可重新查询补齐。
            ++droppedEvents;
            return;
        }
        events.push_back(std::move(event));
    }
    bool pushCommand(CommandFanLX command) {
        {
            std::lock_guard<std::mutex> guard(commandMutex);
            if (stop.load() || commands.size() >= commandQueueCapacityFanLX)
                return false;
            commands.push_back(std::move(command));
        }
        commandReady.notify_all();
        return true;
    }
    bool popCommand(CommandFanLX &out) {
        std::lock_guard<std::mutex> guard(commandMutex);
        if (commands.empty())
            return false;
        out = std::move(commands.front());
        commands.pop_front();
        return true;
    }
    // 一次性修好历史库里的"毒消息"：把永远无法组帧的待发送记录标记为失败。
    //
    // 为什么需要这一步：入口预检（submitSend → textIssueFanLX）只能拦住**新**消息。
    // 旧版本已经把这类记录持久化成了 state='pending'，它们会在每次重连时被 replayOutboxFanLX
    // 重放并抛异常，让客户端永远无法上线。在这里集中修好，用户不需要外部 SQLite 工具、也不用删库。
    //
    // 修完的结果是"标记失败"而不是"删除"：失败原因随消息留在库里、在消息列表可见，
    // 符合"不静默丢弃用户输入"的原则；且 pending() 只看 state='pending'，不会再被重发。
    // 调用时机必须在 replayOutboxFanLX 之前（见 prepareStorageFanLX）。
    void sweepUnsendablePendingFanLX() {
        if (!sqliteReady)
            return;
        std::vector<PendingMessageFanLX> pending;
        {
            std::lock_guard<std::mutex> guard(databaseMutex);
            if (!database)
                return;
            pending = database->repository().pending();
        }
        for (const auto &entry : pending) {
            const auto issue = MessageCodecFanLX::textIssueFanLX(entry.body);
            if (issue.empty())
                continue;
            DbCommandFanLX command;
            command.kind = DbCommandFanLX::KindFanLX::MarkFailed;
            command.requestId = entry.requestId;
            command.reason = "历史遗留消息无法发送，已标记失败：" + issue;
            postCommandFanLX(command);
            database->tryPost(command);
        }
    }

    // 把属于"上一个服务端实例"的本地聊天库整批改名归档——**不删除**任何文件。
    //
    // 为什么要归档一组文件而不是只改身份文件：chat.db 的 schema_meta 里也存了同一个实例号
    // （见 ChatSchemaStoreFanLX::requireIdentity），只换 client-identity.json 会被库本身再拦一次。
    // 归档对象是库本体 + WAL/SHM 旁文件 + 身份文件。
    //
    // 调用时机在构造 SqliteChatRepositoryFanLX 之前：此刻没有任何数据库句柄，改名的又只是目录内的
    // 文件，不触碰 DataDirectoryLockFanLX 持有的 .lock，因此不会与目录独占锁冲突。
    void archiveStaleChatStoreFanLX(const std::string &oldInstanceId, const std::string &newInstanceId) {
        const auto suffix = ".legacy-" + legacyStampFanLX();
        for (const char *name : {"chat.db", "chat.db-wal", "chat.db-shm", "client-identity.json"}) {
            const auto source = directory / name;
            if (!std::filesystem::exists(source))
                continue;
            std::error_code failure;
            std::filesystem::rename(source, directory / (std::string(name) + suffix), failure);
            if (failure)
                throw std::runtime_error("旧聊天库归档失败（" + std::string(name) +
                                         "）：" + failure.message());
        }
        // 归档是有副作用的动作，必须让人看见，不能静默。用 Notice 而不是 Error：
        // 登录是成功的，这不是错误，只是"本地历史被隔离了"这件事需要用户知情。
        ImClientEventFanLX notice;
        notice.kind = ImClientEventFanLX::KindFanLX::Notice;
        notice.detail = "本地聊天库属于已退役的服务端实例（" + oldInstanceId + "），当前实例为 " +
                        newInstanceId + "：已把旧库改名归档为「<文件名>" + suffix +
                        "」（旧历史仍保留在该文件里），并按当前实例新建了本地库，连接继续。";
        pushEvent(std::move(notice));
    }

    void openSqliteFanLX(const std::string &serverInstanceId) {
        instanceId = serverInstanceId;
        const auto identityPath = directory / "client-identity.json";
        // 换库重建身份时沿用原设备号：换掉的是服务端那一侧，本机还是同一台设备、同一个客户端。
        std::string keepDevice;
        if (std::filesystem::exists(identityPath)) {
            identity = nlohmann::json::parse(AtomicFileWriterFanLX::read(identityPath));
            // 第 1 类不一致 —— 目录被误用（不是这个账号 / 不是这台服务器 / 格式不认识）。
            // 这属于真正的配置错误，静默改绑只会把它掩盖掉，因此保持拒绝。
            if (identity.value("account", std::string()) != account ||
                identity.value("host", std::string()) != host ||
                identity.value("port", 0u) != static_cast<unsigned>(port) ||
                identity.value("schema", std::string()) != "fanlx-im-client-identity-1")
                throw std::runtime_error("聊天库归属不符：服务器实例或账号与既有历史不一致");
            // 第 2 类不一致 —— 同一 host:port 上服务端换了库（数据目录被删除重建），实例号必然变。
            // 本地库里的会话游标与待发队列都属于**上一个实例**：直接打开会串历史；
            // 但直接报错会把用户永久挡在登录之外（连不上、进不了主界面，还会不停重连）。
            // 因此改为"归档旧库 + 按当前实例新建"：既不丢证据，也不阻断登录。
            if (identity.value("server_instance_id", std::string()) != serverInstanceId) {
                archiveStaleChatStoreFanLX(identity.value("server_instance_id", std::string()),
                                           serverInstanceId);
                keepDevice = identity.value("device", std::string());
                identity = nlohmann::json::object();
            } else {
                device = identity.at("device").get<std::string>();
            }
        }
        if (device.empty()) {
            device = keepDevice.empty() ? randomDeviceIdFanLX() : keepDevice;
            identity = {{"schema", "fanlx-im-client-identity-1"},
                        {"account", account},
                        {"server_instance_id", serverInstanceId},
                        {"host", host},
                        {"port", port},
                        {"device", device}};
            // 必须用具名发布：AtomicFileWriterFanLX::publish 固定发布为 state.json（平台快照约定），
            // 用它写身份文件会覆盖 IM 的 state.json。
            NamedFilePublisherFanLX::publish(identityPath, identity.dump(2));
        }
        SqliteChatRepositoryFanLX::ConfigFanLX config;
        config.databasePath = directory / "chat.db";
        config.serverInstanceId = serverInstanceId;
        config.ownerAccount = account;
        config.deviceId = device;
        auto repository = std::make_unique<SqliteChatRepositoryFanLX>(config);
        database = std::make_unique<DatabaseWorkerFanLX>(std::move(repository), 512, 16);
        database->registerEventConsumerFanLX([this](const DbEventFanLX &event) { onDatabaseEvent(event); });
        database->registerTaskFanLX("history",
                                    [](DatabaseWorkerFanLX &worker, const DbCommandFanLX &command) {
                                        DatabaseResultFanLX result;
                                        result.history = worker.repository().queryHistory(command.query);
                                        return result;
                                    });
        database->registerTaskFanLX("conversations",
                                    [](DatabaseWorkerFanLX &worker, const DbCommandFanLX &) {
                                        DatabaseResultFanLX result;
                                        result.conversations = worker.repository().conversations();
                                        return result;
                                    });
        database->start();
        sqliteReady = true;
    }

    void openLegacyFanLX() {
        legacyEnabled = true;
        const auto stateFile = directory / "state.json";
        if (std::filesystem::exists(stateFile)) {
            legacyDb = nlohmann::json::parse(AtomicFileWriterFanLX::read(stateFile));
            if (legacyDb.at("schema") != "fanlx-im-client-1" || legacyDb.at("account") != account ||
                legacyDb.at("host") != host || legacyDb.at("port") != port ||
                !legacyDb.at("outbox").is_object() || !legacyDb.at("inbox").is_object() ||
                !legacyDb.at("cursors").is_object() ||
                legacyDb.at("device").get<std::string>().empty())
                throw std::runtime_error("IM 客户端存储身份或格式不符");
            for (const auto &row : legacyDb.at("outbox")) socketIoFanLX::message(row);
            for (const auto &row : legacyDb.at("inbox")) socketIoFanLX::message(row);
            device = legacyDb.at("device").get<std::string>();
        } else {
            device = randomDeviceIdFanLX();
            legacyDb = {{"schema", "fanlx-im-client-1"}, {"account", account},
                        {"host", host},                  {"port", port},
                        {"device", device},              {"next", 1u},
                        {"outbox", nlohmann::json::object()},
                        {"inbox", nlohmann::json::object()},
                        {"cursors", nlohmann::json::object()}};
            socketIoFanLX::commit(directory, legacyDb, legacyDb);
        }
    }

    // ---- 数据库结果 → 事件 / 网络动作（在数据库线程内被调用，必须极短且加锁） ----
    void onDatabaseEvent(const DbEventFanLX &event) {
        ImClientEventFanLX out;
        out.queryId = event.queryId;
        out.requestId = event.requestId;
        if (event.kind == DbEventFanLX::KindFanLX::Failure) {
            std::lock_guard<std::mutex> guard(correlationMutex);
            pendingDeliveries.erase(event.commandId);
            pendingSends.erase(event.commandId);
            out.kind = ImClientEventFanLX::KindFanLX::SendRejected;
            out.detail = event.failureReason;
            pushEvent(std::move(out));
            return;
        }

        std::optional<ChatMessageFanLX> ack;
        {
            std::lock_guard<std::mutex> guard(correlationMutex);
            const auto delivery = pendingDeliveries.find(event.commandId);
            if (delivery != pendingDeliveries.end()) {
                ack = delivery->second;
                pendingDeliveries.erase(delivery);
            } else {
                const auto send = pendingSends.find(event.commandId);
                if (send != pendingSends.end()) {
                    out.kind = send->second.empty() ? ImClientEventFanLX::KindFanLX::Sent
                                                    : ImClientEventFanLX::KindFanLX::Accepted;
                    // 幂等键由数据库在分配本地序号时生成，必须用事件里的值；
                    // 关联表里的值此时可能还是占位空串（提交前无法预知序号）。
                    out.requestId = event.requestId.empty() ? send->second : event.requestId;
                    pendingSends.erase(send);
                }
            }
        }
        if (ack) {
            if (event.value) {
                MessageRowFanLX record;
                record.messageId = static_cast<std::int64_t>(ack->messageId);
                record.conversationKey = conversationKeyFanLX(scopeOfMessageFanLX(*ack), ack->sender,
                                                              ack->recipient);
                record.conversationSequence = static_cast<std::int64_t>(ack->conversationSeq);
                record.sender = ack->sender;
                record.recipient = ack->recipient;
                record.requestId = ack->requestId;
                record.body = ack->text;
                record.acceptedAtUtc = ack->acceptedAtUtc;
                record.state = MessageStateFanLX::Received;
                out.kind = ImClientEventFanLX::KindFanLX::Delivered;
                out.conversationKey = record.conversationKey;
                out.sequence = record.conversationSequence;
                out.messages.push_back(std::move(record));
                pushEvent(std::move(out));
            }
            // 落库成功才回 ACK：否则重连后服务器会重放同一条消息（重复投递要幂等）。
            // ACK 必须带上服务器分配的会话序号：服务端据此推进投递游标（只带 messageId
            // 时游标无法确定，会拒绝 ACK 并让该会话一直停在"在途"，导致往返消息全部卡住）。
            CommandFanLX command;
            command.kind = CommandFanLX::KindFanLX::Send;
            command.message = socketIoFanLX::control("ACK");
            command.message.messageId = ack->messageId;
            command.message.conversationSeq = ack->conversationSeq;
            pushCommand(std::move(command));
            return;
        }
        if (out.kind == ImClientEventFanLX::KindFanLX::Delivered)
            return;
        if (event.customName == "history") {
            out.kind = ImClientEventFanLX::KindFanLX::History;
            out.history = event.history;
        } else if (event.customName == "conversations") {
            out.kind = ImClientEventFanLX::KindFanLX::Conversations;
            out.conversations = event.conversations;
        }
        pushEvent(std::move(out));
    }

    std::int64_t postCommandFanLX(DbCommandFanLX &command) {
        command.commandId = nextCommandId.fetch_add(1);
        return command.commandId;
    }

    void handleLoginOk(const ChatMessageFanLX &message) {
        // 服务器实例标识由 LOGIN_OK 携带；SQLite 后端据此校验本地库归属，
        // 库内身份与服务端不一致时拒绝连接，避免历史错乱。
        if (sqliteEnabled) {
            if (message.requestId.empty()) {
                throw std::runtime_error("服务器未返回实例标识，无法安全绑定聊天数据库");
            }
            if (!instanceId.empty() && instanceId != message.requestId)
                throw std::runtime_error("服务器实例与本地聊天库不一致，已拒绝连接以避免历史错乱");
            instanceId = message.requestId;
        }
        active = true;
        error("");
        // 已通过认证：清掉上一次尝试留下的"登录被拒"标记，界面据此判定为登录成功。
        {
            std::lock_guard<std::mutex> guard(errorMutex);
            loginRejected = false;
            loginRejectReason.clear();
        }
        CommandFanLX command;
        command.kind = CommandFanLX::KindFanLX::Sync;
        command.message = socketIoFanLX::control("SYNC");
        command.message.service = service;
        pushCommand(std::move(command));
    }

    void handleAccepted(const ChatMessageFanLX &message) {
        if (!sqliteReady) {
            error("聊天库不可用，SEND_ACK 未落库");
            return;
        }
        if (!message.messageId || !message.conversationSeq)
            throw std::runtime_error("SEND_ACK 缺少编号或序号");
        DbCommandFanLX command;
        command.kind = DbCommandFanLX::KindFanLX::MarkAccepted;
        command.requestId = message.requestId;
        command.messageId = static_cast<std::int64_t>(message.messageId);
        command.conversationSequence = static_cast<std::int64_t>(message.conversationSeq);
        const auto id = postCommandFanLX(command);
        if (!database->tryPost(command)) {
            error("数据库命令队列已满，SEND_ACK 未落库");
            return;
        }
        {
            std::lock_guard<std::mutex> guard(correlationMutex);
            pendingSends[id] = message.requestId;
        }
    }

    void handleDelivered(const ChatMessageFanLX &message) {
        if (!sqliteReady) {
            error("聊天库不可用，拒绝投递（不回复 ACK）");
            return;
        }
        if (!message.messageId || !message.conversationSeq || message.sender.empty() ||
            (message.scope == "direct" && message.sender != account && message.recipient != account))
            throw std::runtime_error("投递目标/序号非法");
        DbCommandFanLX command;
        command.kind = DbCommandFanLX::KindFanLX::AppendDelivered;
        command.row.messageId = static_cast<std::int64_t>(message.messageId);
        command.row.conversationSequence = static_cast<std::int64_t>(message.conversationSeq);
        command.row.sender = message.sender;
        command.row.recipient = message.recipient;
        command.row.requestId = message.requestId;
        command.row.body = message.text;
        // 服务器时间由服务器提供；未提供时留空，不伪造成本地时间。
        command.row.acceptedAtUtc = message.acceptedAtUtc;
        command.row.conversationKey =
            conversationKeyFanLX(scopeOfMessageFanLX(message), message.sender, message.recipient);
        const auto id = postCommandFanLX(command);
        if (!database->tryPost(command)) {
            // 未落库就不回 ACK：服务器会在重连后重放，不静默丢消息。
            error("数据库命令队列已满，投递未落库（未回 ACK）");
            return;
        }
        {
            std::lock_guard<std::mutex> guard(correlationMutex);
            pendingDeliveries[id] = message;
        }
    }

    // ---- 旧 JSON 后端：保持既有 in-place 语义（读→改→提交），供未启用 SQLite 的调用方使用 ----
    void legacyAcceptedFanLX(const ChatMessageFanLX &message) {
        std::lock_guard<std::mutex> guard(jsonMutex);
        if (!legacyDb.at("outbox").contains(message.requestId))
            return;
        const auto original = socketIoFanLX::message(legacyDb.at("outbox").at(message.requestId));
        if (message.sender != account || original.text != message.text ||
            original.recipient != message.recipient || original.scope != message.scope ||
            !message.messageId || !message.conversationSeq)
            throw std::runtime_error("SEND_ACK 与 outbox 不符");
        auto next = legacyDb;
        next["outbox"].erase(message.requestId);
        socketIoFanLX::commit(directory, legacyDb, std::move(next));
    }

    void legacyDeliveredFanLX(const ChatMessageFanLX &message) {
        if (!active || !message.messageId || !message.conversationSeq || message.sender.empty() ||
            (message.scope == "direct" && message.sender != account && message.recipient != account))
            throw std::runtime_error("投递目标/序号非法");
        {
            std::lock_guard<std::mutex> guard(jsonMutex);
            const auto key = std::to_string(message.messageId);
            if (!legacyDb.at("inbox").contains(key)) {
                auto next = legacyDb;
                next["inbox"][key] = socketIoFanLX::messageJson(message);
                const auto conversation = conversationKeyFanLX(scopeOfMessageFanLX(message), account,
                                                               message.sender == account ? message.recipient
                                                                                         : message.sender);
                const auto cursor = next["cursors"].value(conversation, std::uint64_t{0});
                if (message.conversationSeq > cursor)
                    next["cursors"][conversation] = message.conversationSeq;
                socketIoFanLX::commit(directory, legacyDb, std::move(next));
            }
        }
        ImClientEventFanLX event;
        event.kind = ImClientEventFanLX::KindFanLX::Delivered;
        event.sequence = static_cast<std::int64_t>(message.conversationSeq);
        pushEvent(std::move(event));
        CommandFanLX command;
        command.kind = CommandFanLX::KindFanLX::Send;
        command.message = socketIoFanLX::control("ACK");
        command.message.messageId = message.messageId;
        // 与 SQLite 后端一致：ACK 必须携带会话序号，服务端才能推进投递游标。
        command.message.conversationSeq = message.conversationSeq;
        pushCommand(std::move(command));
    }

    void received(TcpPeerFanLX &peer, const ChatMessageFanLX &message) {
        (void)peer;
        if (message.type == "LOGIN_OK") {
            handleLoginOk(message);
        } else if (message.type == "SEND_ACK") {
            if (sqliteReady)
                handleAccepted(message);
            else
                legacyAcceptedFanLX(message);
        } else if (message.type == "DELIVER") {
            if (sqliteReady)
                handleDelivered(message);
            else
                legacyDeliveredFanLX(message);
        } else if (message.type == "DIRECTORY_LIST") {
            ImClientEventFanLX out;
            out.kind = ImClientEventFanLX::KindFanLX::Directory;
            out.detail = message.text;
            pushEvent(std::move(out));
        } else if (message.type == "ERROR") {
            error(message.text);
            // 尚未通过认证时的 ERROR 就是"登录被拒"：界面必须留在登录窗口重试。
            if (!active.load())
                rejectLogin(message.text);
            ImClientEventFanLX out;
            out.kind = ImClientEventFanLX::KindFanLX::SendRejected;
            out.requestId = message.requestId;
            out.detail = message.text;
            pushEvent(std::move(out));
            // 服务器拒绝：本地标记为失败并保留在库中（不静默丢弃输入），但不再自动重发——
            // 反复重发一条已被拒绝的消息只会制造重连风暴。失败原因在消息列表可见，由用户决定是否重写。
            if (!message.requestId.empty() && sqliteReady) {
                DbCommandFanLX command;
                command.kind = DbCommandFanLX::KindFanLX::MarkFailed;
                command.requestId = message.requestId;
                command.reason = message.text;
                postCommandFanLX(command);
                database->tryPost(command);
            }
        } else if (message.type == "FRIEND_NOTICE") {
            // 好友动态（收到申请 / 申请被同意 / 被删除）：服务端中转推送，界面据此刷新申请列表。
            ImClientEventFanLX out;
            out.kind = ImClientEventFanLX::KindFanLX::FriendNotice;
            out.target = message.sender;
            out.detail = message.text;
            pushEvent(std::move(out));
        } else if (message.type == "PRESENCE" || message.type == "SYNC_END") {
            ImClientEventFanLX out;
            out.kind = message.type == "PRESENCE" ? ImClientEventFanLX::KindFanLX::Presence
                                                  : ImClientEventFanLX::KindFanLX::SyncEnd;
            out.target = message.sender;
            out.detail = message.text;
            pushEvent(std::move(out));
        } else if (message.type != "PONG") {
            throw std::runtime_error("服务端响应类型非法");
        }
    }

    // 单条消息无法组帧（超长 / 含控制字符 / 非法 UTF-8）时的统一出口：
    // 标记失败并上报给界面，但**绝不让它把整条连接带崩**。
    // 旧入口的缺陷正是这里：异常被 run() 的 catch 吞掉后只触发重连，重连又重放同一条，
    // 于是形成"永远重连、永远不到线"的死循环。
    // 旧 JSON 后端没有 failed 状态，只能本次连接跳过（消息仍留在 outbox，不静默丢弃）。
    void failUnsendableFanLX(const ChatMessageFanLX &message, const std::string &reason) {
        ImClientEventFanLX out;
        out.kind = ImClientEventFanLX::KindFanLX::SendRejected;
        out.requestId = message.requestId;
        out.detail = reason;
        pushEvent(std::move(out));
        if (!sqliteReady || message.requestId.empty())
            return;
        DbCommandFanLX command;
        command.kind = DbCommandFanLX::KindFanLX::MarkFailed;
        command.requestId = message.requestId;
        command.reason = reason;
        postCommandFanLX(command);
        database->tryPost(command);
    }

    // 补发 outbox：按本地提交顺序重放，已在本次连接登记过的 requestId 不重复入网。
    // SQLite 与旧 JSON 两种后端都必须支持——"离线期间发送、重连后补发"是核心用例。
    void replayOutboxFanLX(TcpPeerFanLX &peer, std::set<std::string> &sentRequests) {
        std::vector<ChatMessageFanLX> outbox;
        if (sqliteReady) {
            std::vector<PendingMessageFanLX> pending;
            {
                std::lock_guard<std::mutex> guard(databaseMutex);
                if (database)
                    pending = database->repository().pending();
            }
            for (const auto &entry : pending) {
                auto message = socketIoFanLX::control("SEND");
                message.sender = account;
                message.recipient = entry.target; // 已由会话键按方向推导
                message.text = entry.body;
                message.requestId = entry.requestId;
                message.scope = scopeTextFanLX(entry.scope);
                outbox.push_back(std::move(message));
            }
        } else {
            std::lock_guard<std::mutex> guard(jsonMutex);
            std::vector<std::pair<std::uint64_t, ChatMessageFanLX>> ordered;
            for (const auto &row : legacyDb.at("outbox"))
                ordered.emplace_back(row.value("sequence", std::uint64_t{0}), socketIoFanLX::message(row));
            std::sort(ordered.begin(), ordered.end(),
                      [](const auto &left, const auto &right) { return left.first < right.first; });
            for (auto &entry : ordered)
                outbox.push_back(std::move(entry.second));
        }
        for (const auto &message : outbox) {
            if (sentRequests.count(message.requestId))
                continue;
            bool queued = false;
            try {
                queued = peer.trySend(message);
            } catch (const std::exception &encodeError) {
                // 这条消息永远无法组帧：登记为已处理并标记失败，避免死循环重连。
                sentRequests.insert(message.requestId);
                failUnsendableFanLX(message, encodeError.what());
                continue;
            }
            if (!queued)
                return; // 队列满视为慢端，本轮不再补发
            sentRequests.insert(message.requestId);
        }
    }

    void run() noexcept {
        unsigned backoff = 100;
        while (!stop) {
            try {
                reset = false;
                auto peer = TcpPeerFanLX::connect(host, port, std::chrono::seconds(2));                auto login = socketIoFanLX::control("LOGIN");
                login.sender = account;
                login.text = password;
                // 登录帧带上本通道的服务名：服务端按 (服务, 账号) 认证并选择该服务的好友/群分区。
                login.service = service;
                peer->send(login);
                auto last = Clock::now(), ping = last;
                std::set<std::string> sentRequests;
                while (!stop && !reset && !peer->ended()) {
                    for (int budget = 0; budget < 32; ++budget) {
                        auto message = peer->poll();
                        if (!message)
                            break;
                        last = Clock::now();
                        received(*peer, *message);
                    }
                    if (active) {
                        backoff = 100;
                        // 首次收到 LOGIN_OK 后才准备工作存储：需要服务器实例号才能确定库归属。
                        // （初始化可能较慢，因此只发生在工作线程里，界面线程不会被阻塞。）
                        if (!storageAttempted)
                            prepareStorageFanLX();
                        if (storageErrorSet)
                            throw std::runtime_error(storageError);
                        replayOutboxFanLX(*peer, sentRequests);
                        CommandFanLX command;
                        while (popCommand(command)) {
                            if (command.kind == CommandFanLX::KindFanLX::Send &&
                                command.message.type == "SEND") {
                                if (sentRequests.count(command.message.requestId))
                                    continue;
                                sentRequests.insert(command.message.requestId);
                            }
                            bool queued = false;
                            try {
                                // 所有出站帧统一打上本通道的服务名（含从 outbox 重放的旧消息）。
                                command.message.service = service;
                                queued = peer->trySend(command.message);
                            } catch (const std::exception &encodeError) {
                                // 单条不可组帧：标记失败后继续处理后续命令，而不是重置连接。
                                failUnsendableFanLX(command.message, encodeError.what());
                                continue;
                            }
                            if (!queued)
                                throw std::runtime_error("待发送缓冲已满，连接重置");
                        }
                    }
                    if (!peer->flushSend(16)) {
                        throw std::runtime_error(peer->error().empty() ? "发送失败" : peer->error());
                    }
                    if (Clock::now() - ping >= std::chrono::seconds(5)) {
                        if (active) {
                            auto pingMessage = socketIoFanLX::control("PING");
                            peer->trySend(pingMessage);
                        }
                        ping = Clock::now();
                    }
                    if (Clock::now() - last > std::chrono::seconds(15))
                        throw std::runtime_error("心跳/登录超时，正在重连");
                    std::unique_lock<std::mutex> guard(commandMutex);
                    commandReady.wait_for(guard, std::chrono::milliseconds(10), [this] {
                        return stop.load() || reset.load() || !commands.empty();
                    });
                }
                if (!stop) {
                    error(peer->error().empty() ? "连接重置，正在重连" : peer->error());
                    reportConnectionErrorFanLX();
                }
            } catch (const std::exception &exception) {
                error(exception.what());
                reportConnectionErrorFanLX();
            } catch (...) {
                error("IM 工作线程异常");
                reportConnectionErrorFanLX();
            }
            active = false;
            std::unique_lock<std::mutex> guard(commandMutex);
            // 有界指数退避 + 抖动，析构无需等待完整退避周期。
            commandReady.wait_for(guard, std::chrono::milliseconds(backoff + GetTickCount64() % 97),
                                  [this] { return stop.load(); });
            backoff = std::min(backoff * 2, 5000u);
        }
        active = false;
    }

    // ---- 同步便捷接口使用的等待辅助 ----
    std::optional<ImClientEventFanLX> waitForSendResult(std::string &requestIdOut,
                                                       std::chrono::milliseconds timeout) {
        const auto deadline = Clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            std::lock_guard<std::mutex> guard(eventMutex);
            for (auto iterator = events.begin(); iterator != events.end(); ++iterator) {
                const auto kind = iterator->kind;
                if (kind == ImClientEventFanLX::KindFanLX::Sent) {
                    requestIdOut = iterator->requestId;
                    events.erase(iterator);
                    return std::nullopt;
                }
                if (kind == ImClientEventFanLX::KindFanLX::SendRejected && !iterator->detail.empty()) {
                    const auto detail = iterator->detail;
                    events.erase(iterator);
                    throw std::runtime_error(detail);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return std::nullopt;
    }
};

ImClientFanLX::ImClientFanLX(std::string host, unsigned short port, std::string account,
                             std::string password, const std::filesystem::path &directory,
                             std::string service)
    : implFanLX(std::make_unique<Impl>(std::move(host), port, std::move(account), std::move(password),
                                       directory, false, std::move(service))) {}

ImClientFanLX::ImClientFanLX(std::string host, unsigned short port, std::string account,
                             std::string password, const std::filesystem::path &directory,
                             ChatDbConfigFanLX chatDb, std::string service)
    : implFanLX(std::make_unique<Impl>(std::move(host), port, std::move(account), std::move(password),
                                       directory, chatDb.enabled, std::move(service))) {}

ImClientFanLX::~ImClientFanLX() = default;

bool ImClientFanLX::submitSend(const std::string &target, const std::string &text, bool group,
                              std::string &requestIdOut) {
    auto &impl = *implFanLX;
    if (target.empty() || text.empty())
        throw std::invalid_argument("发送目标与内容不能为空");
    // 入口预检必须在**写库之前**完成。否则一条永远无法组帧的消息会被持久化成"待发送"，
    // 之后每次重连都被 replayOutboxFanLX 重放并抛异常，客户端永远无法上线（毒消息）。
    // 校验规则与 MessageCodecFanLX::encode 共用同一处实现，避免两套标准不一致。
    if (const auto issue = MessageCodecFanLX::textIssueFanLX(text); !issue.empty())
        throw std::invalid_argument(issue);
    const auto scope = group ? ScopeFanLX::Group : ScopeFanLX::Direct;

    if (!impl.sqliteReady && impl.sqliteEnabled)
        return false;

    if (impl.sqliteReady) {
        // 先在数据库提交"待发送"：只有提交成功才允许向用户显示已保存、并向服务器发送。
        DbCommandFanLX command;
        command.kind = DbCommandFanLX::KindFanLX::AppendPending;
        command.conversationKey = conversationKeyFanLX(scope, impl.account, target);
        command.scope = scope;
        command.target = target;
        command.body = text;
        // requestId 在数据库侧由"设备 + 本地序号"生成：必须先落库拿到序号，再拿去发网络。
        command.requestId.clear();
        const auto id = impl.postCommandFanLX(command);
        {
            std::lock_guard<std::mutex> guard(impl.correlationMutex);
            impl.pendingSends[id] = std::string();
        }
        if (!impl.database->postAndWait(command, std::chrono::seconds(5))) {
            std::lock_guard<std::mutex> guard(impl.correlationMutex);
            impl.pendingSends.erase(id);
            return false;
        }
        requestIdOut.clear();
        impl.waitForSendResult(requestIdOut, std::chrono::seconds(5));
        if (requestIdOut.empty()) {
            return false;
        }
        Impl::CommandFanLX network;
        network.kind = Impl::CommandFanLX::KindFanLX::Send;
        network.message = socketIoFanLX::control("SEND");
        network.message.sender = impl.account;
        network.message.recipient = target;
        network.message.text = text;
        network.message.requestId = requestIdOut;
        network.message.scope = scopeTextFanLX(scope);
        return impl.pushCommand(std::move(network));
    }

    // 旧 JSON 后端：保持既有语义（提交成功后才入网）。
    std::lock_guard<std::mutex> guard(impl.jsonMutex);
    auto next = impl.legacyDb;
    auto sequence = next.at("next").get<std::uint64_t>();
    if (sequence == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("客户端序号耗尽");
    auto message = socketIoFanLX::control("SEND");
    message.sender = impl.account;
    message.recipient = target;
    message.text = text;
    message.sequence = sequence;
    message.requestId = impl.device + std::to_string(sequence);
    message.scope = group ? "group" : "direct";
    next["next"] = sequence + 1;
    next["outbox"][message.requestId] = socketIoFanLX::messageJson(message);
    socketIoFanLX::commit(impl.directory, impl.legacyDb, std::move(next));
    requestIdOut = message.requestId;
    Impl::CommandFanLX network;
    network.kind = Impl::CommandFanLX::KindFanLX::Send;
    network.message = message;
    return impl.pushCommand(std::move(network));
}

// 会话键 → (范围, 收件人)。单聊键为 d|<账号A>|<账号B>（字典序），对端即非本人那一侧；
// 群聊键为 g|<群号>。这是"发消息给谁"的唯一推导点。
namespace {
std::pair<bool, std::string> parseConversationKeyFanLX(const std::string &key, const std::string &account) {
    if (key.rfind("g|", 0) == 0)
        return {true, key.substr(2)};
    if (key.rfind("d|", 0) != 0)
        throw std::invalid_argument("会话键格式非法：" + key);
    const auto second = key.find('|', 2);
    if (second == std::string::npos)
        throw std::invalid_argument("会话键格式非法：" + key);
    const auto left = key.substr(2, second - 2);
    const auto right = key.substr(second + 1);
    return {false, left == account ? right : left};
}
} // namespace

bool ImClientFanLX::submitSendToConversation(const std::string &conversationKey, const std::string &text,
                                             std::string &requestIdOut) {
    const auto parsed = parseConversationKeyFanLX(conversationKey, implFanLX->account);
    return submitSend(parsed.second, text, parsed.first, requestIdOut);
}

std::string ImClientFanLX::send(const std::string &target, const std::string &text, bool group) {
    std::string requestId;
    if (!submitSend(target, text, group, requestId))
        throw std::runtime_error("发送队列已满或聊天库不可用，请稍后重试");
    return requestId;
}

void ImClientFanLX::queryOnline(const std::string &friendAccount) {
    auto &impl = *implFanLX;
    Impl::CommandFanLX command;
    command.kind = Impl::CommandFanLX::KindFanLX::Send;
    command.message = socketIoFanLX::control("QUERY");
    command.message.recipient = friendAccount;
    if (!impl.pushCommand(std::move(command)))
        impl.error("在线查询队列已满");
}

bool ImClientFanLX::submitHistory(const HistoryQueryFanLX &query, std::int64_t &queryIdOut) {
    auto &impl = *implFanLX;
    if (!impl.sqliteReady)
        return false;
    DbCommandFanLX command;
    command.kind = DbCommandFanLX::KindFanLX::Custom;
    command.customName = "history";
    command.query = query;
    command.queryId = impl.nextQueryId.fetch_add(1);
    impl.postCommandFanLX(command);
    if (!impl.database->tryPost(command))
        return false;
    queryIdOut = command.queryId;
    // 查询窗口与聊天时间线可以并行；由各自控制器显式取消自己的旧查询。
    return true;
}

bool ImClientFanLX::submitConversations() {
    auto &impl = *implFanLX;
    if (!impl.sqliteReady)
        return false;
    DbCommandFanLX command;
    command.kind = DbCommandFanLX::KindFanLX::Custom;
    command.customName = "conversations";
    command.queryId = impl.nextQueryId.fetch_add(1);
    impl.postCommandFanLX(command);
    return impl.database->tryPost(command);
}

bool ImClientFanLX::submitDirectory() {
    auto &impl = *implFanLX;
    Impl::CommandFanLX command;
    command.kind = Impl::CommandFanLX::KindFanLX::Send;
    command.message = socketIoFanLX::control("DIRECTORY");
    return impl.pushCommand(std::move(command));
}

bool ImClientFanLX::submitMarkRead(const std::string &conversationKey, std::int64_t sequence) {
    auto &impl = *implFanLX;
    if (!impl.sqliteReady)
        return false;
    DbCommandFanLX command;
    command.kind = DbCommandFanLX::KindFanLX::MarkRead;
    command.conversationKey = conversationKey;
    command.conversationSequence = sequence;
    impl.postCommandFanLX(command);
    return impl.database->tryPost(command);
}

void ImClientFanLX::cancelQuery(std::int64_t queryId) {
    if (implFanLX->sqliteReady)
        implFanLX->database->markCancelled(queryId);
}

std::optional<ImClientEventFanLX> ImClientFanLX::pollClientEvent() {
    auto &impl = *implFanLX;
    std::lock_guard<std::mutex> guard(impl.eventMutex);
    if (impl.events.empty())
        return std::nullopt;
    auto event = std::move(impl.events.front());
    impl.events.pop_front();
    return event;
}

std::size_t ImClientFanLX::droppedEventCount() const {
    std::lock_guard<std::mutex> guard(implFanLX->eventMutex);
    return implFanLX->droppedEvents;
}

std::optional<ChatMessageFanLX> ImClientFanLX::pollEvent() {
    for (;;) {
        auto event = pollClientEvent();
        if (!event)
            return std::nullopt;
        switch (event->kind) {
        case EventFanLX::KindFanLX::Presence: {
            auto message = socketIoFanLX::control("PRESENCE");
            message.sender = event->target;
            message.text = event->detail;
            return message;
        }
        case EventFanLX::KindFanLX::SyncEnd:
            return socketIoFanLX::control("SYNC_END");
        case EventFanLX::KindFanLX::Error: {
            auto message = socketIoFanLX::control("ERROR");
            message.text = event->detail;
            return message;
        }
        default:
            continue; // 其余事件由 pollClientEvent 消费，不伪装成协议消息。
        }
    }
}

std::vector<ChatMessageFanLX> ImClientFanLX::history() const {
    auto &impl = *implFanLX;
    std::vector<ChatMessageFanLX> result;
    if (impl.sqliteReady) {
        std::lock_guard<std::mutex> guard(impl.databaseMutex);
        HistoryQueryFanLX query;
        query.ascending = true; // 既有调用方期望历史按会话序号递增返回
        for (const auto &conversation : impl.database->repository().conversations()) {
            query.conversationKey = conversation.conversationKey;
            query.beforeSequence = 0;
            for (;;) {
                const auto page = impl.database->repository().queryHistory(query);
                for (const auto &row : page.messages) {
                    ChatMessageFanLX message;
                    message.sender = row.sender;
                    message.recipient = row.recipient;
                    message.text = row.body;
                    message.requestId = row.requestId;
                    message.scope =
                        conversation.scope == ScopeFanLX::Group ? "group" : "direct";
                    message.messageId = row.messageId ? static_cast<std::uint64_t>(*row.messageId) : 0;
                    message.conversationSeq = static_cast<std::uint64_t>(row.conversationSequence);
                    result.push_back(std::move(message));
                }
                if (!page.hasMore)
                    break;
                query.beforeSequence = page.nextBeforeSequence;
            }
        }
        std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
            return left.messageId < right.messageId;
        });
        return result;
    }
    std::lock_guard<std::mutex> guard(impl.jsonMutex);
    // 存储由工作线程异步准备：尚未就绪时返回空历史，而不是对 null 取键抛异常。
    if (impl.legacyDb.is_null() || !impl.legacyDb.contains("inbox"))
        return result;
    for (const auto &row : impl.legacyDb.at("inbox"))
        result.push_back(socketIoFanLX::message(row));
    std::sort(result.begin(), result.end(),
              [](const auto &left, const auto &right) { return left.messageId < right.messageId; });
    return result;
}

std::size_t ImClientFanLX::pending() const {
    auto &impl = *implFanLX;
    if (impl.sqliteReady) {
        std::lock_guard<std::mutex> guard(impl.databaseMutex);
        return impl.database->repository().pending().size();
    }
    std::lock_guard<std::mutex> guard(impl.jsonMutex);
    if (impl.legacyDb.is_null() || !impl.legacyDb.contains("outbox"))
        return 0;
    return impl.legacyDb.at("outbox").size();
}

bool ImClientFanLX::online() const { return implFanLX->active; }
std::string ImClientFanLX::error() const { return implFanLX->errorText(); }
std::string ImClientFanLX::serviceName() const { return implFanLX->service; }
bool ImClientFanLX::authenticationRejected() const {
    return implFanLX->loginWasRejected(nullptr);
}
std::string ImClientFanLX::authenticationRejectionReason() const {
    std::string reason;
    implFanLX->loginWasRejected(&reason);
    return reason;
}
void ImClientFanLX::reconnect() {
    implFanLX->reset = true;
    implFanLX->commandReady.notify_all();
}

std::int64_t ImClientFanLX::nextQueryId() const { return implFanLX->nextQueryId.load(); }
std::string ImClientFanLX::sessionInstanceId() const {
    return implFanLX->instanceId.empty() ? implFanLX->device : implFanLX->instanceId + "|" + implFanLX->account;
}
std::string ImClientFanLX::storageBackend() const { return implFanLX->sqliteReady ? "sqlite" : "json"; }
std::filesystem::path ImClientFanLX::dataDirectory() const { return implFanLX->directory; }
std::size_t ImClientFanLX::queuedSendFrames() const { return 0; }
bool ImClientFanLX::databaseReady() const { return implFanLX->sqliteReady; }

// 账号登记：一次性短连接（连上 → REGISTER → 等 REGISTER_OK/ERROR → 关），
// 不建聊天库、不写任何本地状态——登记的产物完全落在**服务端**的持久化账号库里。
// 之所以做成独立短连接：注册发生在"还没有账号"的时刻，此时客户端连服务账号都还没有，
// 复用长连接会把"先有账号再连接"的顺序倒置。
bool ImClientFanLX::registerAccountFanLX(const std::string &host, unsigned short port,
                                         const std::string &weixId, const std::string &password,
                                         const std::string &nickname, const std::string &birthDate,
                                         const std::string &location, std::string &reason,
                                         const std::vector<ServiceEntryFanLX> &services,
                                         const std::vector<ServiceEntryFanLX> &friends,
                                         int timeoutMs) {
    const auto fail = [&reason](const std::string &text) {
        reason = text;
        return false;
    };
    if (host.empty() || port == 0 || weixId.empty() || password.empty())
        return fail("登记账号需要服务器地址、端口、微X号与密码");
    if (timeoutMs <= 0)
        timeoutMs = 4000;
    try {
        auto peer = TcpPeerFanLX::connect(host, port, std::chrono::milliseconds(timeoutMs));
        if (peer->ended())
            return fail(peer->error().empty() ? std::string("无法连接服务端") : peer->error());
        // 资料放 JSON 里（与 DIRECTORY_LIST 同一套做法），密码单独在 password 字段；
        // services 是本机已开通的下级服务，服务端据此补建服务账号（否则登录会报"未开通"）。
        nlohmann::json profile{{"password", password},
                               {"nickname", nickname},
                               {"birthDate", birthDate},
                               {"location", location}};
        auto serviceList = nlohmann::json::array();
        for (const auto &entry : services)
            serviceList.push_back({{"service", entry.first}, {"accountId", entry.second}});
        profile["services"] = std::move(serviceList);
        // friends：本机已加的好友（服务 + 对端号码），服务端据此补登关系，
        // 否则"本机加好友后发消息"会被服务端判为不是好友。
        auto friendList = nlohmann::json::array();
        for (const auto &entry : friends)
            friendList.push_back({{"service", entry.first}, {"accountId", entry.second}});
        profile["friends"] = std::move(friendList);
        auto request = socketIoFanLX::control("REGISTER");
        request.sender = weixId;
        request.text = profile.dump();
        // REGISTER 是账号层消息：service 取默认值即可（服务端不按通道解释它）。
        if (!peer->trySend(request))
            return fail("发送登记请求失败（缓冲已满）");
        if (!peer->flushSend(16))
            return fail(peer->error().empty() ? std::string("发送登记请求失败") : peer->error());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            auto reply = peer->poll();
            if (!reply) {
                if (peer->ended())
                    return fail(peer->error().empty() ? std::string("服务端关闭了连接") : peer->error());
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            if (reply->type == "REGISTER_OK") {
                reason = reply->text.empty() ? std::string("服务端已登记该账号") : reply->text;
                return true;
            }
            return fail(reply->text.empty() ? std::string("服务端拒绝了登记请求") : reply->text);
        }
        return fail("等待服务端登记结果超时");
    } catch (const std::exception &error) {
        return fail(error.what());
    }
}

// 账号查询：一次性短连接（连接 → LOGIN → LOOKUP → 读回应答 → 关闭）。
// 复用 LOGIN 是刻意的：服务端只让已登录的连接查询账号，否则任何人都能用它枚举号码。
bool ImClientFanLX::lookupAccountFanLX(const std::string &host, unsigned short port,
                                       const std::string &service, const std::string &selfAccount,
                                       const std::string &password, const std::string &peerAccount,
                                       LookupResultFanLX &result, std::string &reason,
                                       int timeoutMs) {
    const auto fail = [&reason](const std::string &text) {
        reason = text;
        return false;
    };
    if (host.empty() || port == 0 || selfAccount.empty() || password.empty() || peerAccount.empty())
        return fail("查询账号需要服务器地址、端口、本人通道凭据与对端号码");
    if (timeoutMs <= 0)
        timeoutMs = 4000;
    try {
        auto peer = TcpPeerFanLX::connect(host, port, std::chrono::milliseconds(timeoutMs));
        if (peer->ended())
            return fail(peer->error().empty() ? std::string("无法连接服务端") : peer->error());
        // 不复用 LOGIN：查询方此刻正连着自己的聊天通道，再登录一次会被单设备策略拒绝。
        // 因此把"本人账号 + 密码"放在查询报文里，由服务端做一次凭据校验后回答。
        auto request = socketIoFanLX::control("LOOKUP");
        request.service = service;
        request.sender = peerAccount;
        request.text = nlohmann::json{{"self", selfAccount}, {"password", password}}.dump();
        if (!peer->trySend(request) || !peer->flushSend(16))
            return fail("发送查询请求失败");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            auto reply = peer->poll();
            if (!reply) {
                if (peer->ended())
                    return fail(peer->error().empty() ? std::string("服务端关闭了连接") : peer->error());
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            if (reply->type != "LOOKUP_RESULT")
                return fail(reply->text.empty() ? std::string("服务端拒绝了查询") : reply->text);
            try {
                const auto parsed = nlohmann::json::parse(reply->text);
                result.exists = parsed.value("exists", false);
                result.service = parsed.value("service", std::string());
                result.accountId = parsed.value("accountId", std::string());
                result.userId = parsed.value("userId", std::string());
                result.nickname = parsed.value("nickname", std::string());
                result.subscribed = parsed.value("subscribed", false);
            } catch (const std::exception &) {
                return fail("服务端查询结果不是合法 JSON");
            }
            reason = result.exists ? std::string() : std::string("服务端没有这个号码");
            return true;
        }
        return fail("等待查询结果超时");
    } catch (const std::exception &error) {
        return fail(error.what());
    }
}

// 好友申请操作：与 LOOKUP 一样是一次性短连接，凭据放在报文里由服务端校验，
// 这样聊天通道保持登录时也能随时发申请 / 同意 / 拒绝。
bool ImClientFanLX::friendOperationFanLX(const std::string &host, unsigned short port,
                                         const std::string &service, const std::string &selfAccount,
                                         const std::string &password, const std::string &op,
                                         const std::string &peerAccount, const std::string &note,
                                         std::string &resultJson, std::string &reason,
                                         const std::string &via, bool *answered, int timeoutMs) {
    const auto fail = [&reason](const std::string &text) {
        reason = text;
        return false;
    };
    if (answered)
        *answered = false;
    if (host.empty() || port == 0 || selfAccount.empty() || password.empty() || op.empty())
        return fail("好友操作需要服务器地址、端口与本人通道凭据");
    if (op != "list" && peerAccount.empty())
        return fail("请填写对方号码");
    if (timeoutMs <= 0)
        timeoutMs = 4000;
    try {
        auto peer = TcpPeerFanLX::connect(host, port, std::chrono::milliseconds(timeoutMs));
        if (peer->ended())
            return fail(peer->error().empty() ? std::string("无法连接服务端") : peer->error());
        auto request = socketIoFanLX::control("FRIEND");
        request.service = service;
        request.sender = peerAccount;
        nlohmann::json body{{"self", selfAccount}, {"password", password}, {"op", op}, {"note", note}};
        if (!via.empty())
            body["via"] = via;
        request.text = body.dump();
        if (!peer->trySend(request) || !peer->flushSend(16))
            return fail("发送好友请求失败");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            auto reply = peer->poll();
            if (!reply) {
                if (peer->ended())
                    return fail(peer->error().empty() ? std::string("服务端关闭了连接") : peer->error());
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            if (answered)
                *answered = true;
            if (reply->type != "FRIEND_RESULT")
                return fail(reply->text.empty() ? std::string("服务端拒绝了好友操作") : reply->text);
            try {
                const auto parsed = nlohmann::json::parse(reply->text);
                if (!parsed.is_object())
                    return fail("服务端好友应答格式错误");
                if (!parsed.value("ok", false))
                    return fail(parsed.value("message", std::string("好友操作失败")));
            } catch (const std::exception &) {
                return fail("服务端好友应答不是合法 JSON");
            }
            resultJson = reply->text;
            reason.clear();
            return true;
        }
        return fail("等待好友操作结果超时");
    } catch (const std::exception &error) {
        return fail(error.what());
    }
}
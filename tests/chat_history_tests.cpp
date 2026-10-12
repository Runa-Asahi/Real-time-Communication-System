// E02 数据库仓储验收：持久化、约束、幂等、回滚、账号隔离、keyset 分页、中文与特殊字符、
// 迁移重跑、异常重启、版本不兼容。所有断言在 Release 下同样生效（首行取消 NDEBUG）。
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "db/ChatMigrationFanLX.h"
#include "db/ChatRepositoryFanLX.h"
#include "db/ChatSchemaStoreFanLX.h"
#include "db/DatabaseWorkerFanLX.h"
#include "db/SqliteChatRepositoryFanLX.h"
#include "db/SqliteHandleFanLX.h"
#include "db/SqliteMessageStoreFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace chatDbFanLX;

namespace {

std::filesystem::path temporaryRootFanLX() {
    static std::atomic<unsigned long long> counter{0};
    const auto root = std::filesystem::temp_directory_path() /
                      ("fanlx-db-tests-" + std::to_string(GetCurrentProcessId()) + "-" +
                       std::to_string(++counter));
    std::filesystem::create_directories(root);
    return root;
}

// 中文路径：数据库文件与中间目录都必须能创建与读写。
std::filesystem::path chineseRootFanLX() {
    static std::atomic<unsigned long long> counter{0};
    const auto root = std::filesystem::temp_directory_path() /
                      ("\xE6\xA8\x8A\xE9\x99\x86\xE6\x97\xAD-\xE8\x81\x8A\xE5\xA4\xA9\xE5\xBA\x93-" +
                       std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++counter));
    std::filesystem::create_directories(root);
    return root;
}

template <class FnFanLX> void failsFanLX(FnFanLX fn) {
    bool thrown = false;
    try {
        fn();
    } catch (const std::exception &) {
        thrown = true;
    }
    assert(thrown);
}

template <class ErrorFanLX, class FnFanLX> void failsAsFanLX(FnFanLX fn) {
    bool matched = false;
    try {
        fn();
    } catch (const ErrorFanLX &) {
        matched = true;
    } catch (const std::exception &) {
    }
    assert(matched);
}

// SQLite 连接持有文件句柄：删除测试目录前必须让仓储对象先离开作用域。
void removeTreeFanLX(const std::filesystem::path &path) {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
    if (std::filesystem::exists(path)) {
        std::cerr << "[cleanup] 无法删除：" << path.u8string() << std::endl;
        std::error_code walkError;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(path, walkError))
            std::cerr << "[cleanup]   残留 " << entry.path().u8string() << std::endl;
    }
    assert(!std::filesystem::exists(path));
}

SqliteChatRepositoryFanLX::ConfigFanLX clientConfigFanLX(const std::filesystem::path &directory,
                                                         const std::string &account) {
    SqliteChatRepositoryFanLX::ConfigFanLX config;
    config.databasePath = directory / "chat.db";
    config.serverInstanceId = "srv-instance-0001";
    config.ownerAccount = account;
    config.deviceId = "device-" + account;
    return config;
}

// 投递行：对端发来的消息（服务器已编号）。
MessageRowFanLX deliveredFanLX(const std::string &sender, const std::string &recipient,
                              std::int64_t messageId, std::int64_t sequence, const std::string &body) {
    MessageRowFanLX row;
    row.messageId = messageId;
    row.conversationSequence = sequence;
    row.sender = sender;
    row.recipient = recipient;
    row.body = body;
    row.acceptedAtUtc = utcNowFanLX();
    row.conversationKey = conversationKeyFanLX(ScopeFanLX::Direct, sender, recipient);
    row.state = MessageStateFanLX::Received;
    return row;
}

// ---- T20 客户端仓储：往返、状态机、幂等与冲突 ------------------------------------------
void t20ClientRoundTripFanLX() {
    const auto directory = temporaryRootFanLX();
    const std::string owner = "2026001", peer = "2026002";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, owner));
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, owner, peer);
        std::string requestId;
        const auto localSequence =
            repository.appendPending(key, ScopeFanLX::Direct, peer, "\xE4\xBD\xA0\xE5\xA5\xBD", "", requestId);
        assert(localSequence > 0);
        assert(requestId == buildRequestIdFanLX("device-" + owner, localSequence));
        assert(repository.messageCount() == 1);
        const auto outbox = repository.pending();
        assert(outbox.size() == 1);
        assert(outbox.front().requestId == requestId);
        assert(outbox.front().target == peer);

        // 本地提交后但未收到 ACK：历史可见且状态为"待发送"（无服务器编号）。
        HistoryQueryFanLX query;
        query.conversationKey = key;
        auto page = repository.queryHistory(query);
        assert(page.messages.size() == 1);
        assert(page.messages.front().state == MessageStateFanLX::Pending);
        assert(!page.messages.front().messageId.has_value());
        assert(page.messages.front().conversationSequence == 0);

        repository.markAccepted(requestId, 11, 1, "2026-09-23T10:00:00Z");
        assert(repository.pending().empty());
        page = repository.queryHistory(query);
        assert(page.messages.size() == 1);
        assert(page.messages.front().state == MessageStateFanLX::SentAck);
        assert(page.messages.front().messageId.has_value() && *page.messages.front().messageId == 11);
        assert(page.messages.front().acceptedAtUtc == "2026-09-23T10:00:00Z");
        assert(repository.lastConversationSequence(key) == 1);

        // 重复 SEND_ACK（网络重放）：内容一致视为成功，不报错、不产生第二行。
        repository.markAccepted(requestId, 11, 1, "2026-09-23T10:00:00Z");
        assert(repository.messageCount() == 1);
        // ACK 与本地记录不一致：必须拒绝。
        failsAsFanLX<ChatConflictFanLX>([&] { repository.markAccepted(requestId, 99, 9, ""); });
    }
    {
        // 重新打开：数据仍在（持久化），本地序号继续递增。
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, owner));
        assert(repository.messageCount() == 1);
        assert(repository.nextLocalSequence() == 2);
        std::string requestId;
        repository.appendPending(conversationKeyFanLX(ScopeFanLX::Direct, owner, peer), ScopeFanLX::Direct,
                                 peer, "second", "", requestId);
        assert(requestId == buildRequestIdFanLX("device-" + owner, 2));
    }
    removeTreeFanLX(directory);
}

void t21IdempotencyAndConflictFanLX() {
    const auto directory = temporaryRootFanLX();
    const std::string owner = "2026001", peer = "2026002";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, owner));
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, owner, peer);
        std::string first;
        repository.appendPending(key, ScopeFanLX::Direct, peer, "same", "req-fixed", first);
        assert(first == "req-fixed");
        assert(repository.messageCount() == 1);
        // 同 requestId 同内容：幂等返回旧序号，不新增行。
        std::string again;
        const auto sequence = repository.appendPending(key, ScopeFanLX::Direct, peer, "same", "req-fixed", again);
        assert(again == "req-fixed");
        assert(sequence == 1);
        assert(repository.messageCount() == 1);
        // 同 requestId 不同内容：必须拒绝，且旧内容不被改写。
        std::string rejected;
        failsAsFanLX<ChatConflictFanLX>(
            [&] { repository.appendPending(key, ScopeFanLX::Direct, peer, "tampered", "req-fixed", rejected); });
        HistoryQueryFanLX query;
        query.conversationKey = key;
        assert(repository.queryHistory(query).messages.front().body == "same");

        // 参数校验失败不得消耗本地序号（回滚检查）。
        const auto before = repository.nextLocalSequence();
        std::string unused;
        failsFanLX([&] { repository.appendPending(key, ScopeFanLX::Direct, "", "no target", "", unused); });
        assert(repository.nextLocalSequence() == before);
    }
    removeTreeFanLX(directory);
}

void t22RollbackAndConstraintsFanLX() {
    const auto directory = temporaryRootFanLX();
    const std::string owner = "2026001", peer = "2026002";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, owner));
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, owner, peer);

        // 参数校验失败必须在事务提交前抛出，磁盘不留下半条记录。
        failsFanLX([&] { repository.markAccepted("missing", 0, 0, ""); });
        failsFanLX([&] { repository.markAccepted("req-not-exist", 5, 5, ""); });
        assert(repository.messageCount() == 0);
        assert(repository.pending().empty());

        // 投递插入与游标推进同事务：重复 messageId 内容一致不新增行，内容冲突必须报错。
        auto row = deliveredFanLX(peer, owner, 100, 1, "received");
        assert(repository.appendDelivered(row) == true);
        assert(repository.appendDelivered(row) == false);
        assert(repository.messageCount() == 1);
        auto conflicting = row;
        conflicting.body = "rewritten";
        failsAsFanLX<ChatConflictFanLX>([&] { repository.appendDelivered(conflicting); });

        // 同一会话序号被另一条消息占用：这是真正的冲突（messageId 与内容都不同），必须拒绝。
        // 注意“晚到的空洞”已不再算冲突——允许补齐是刻意的，见 t56。
        auto duplicateSlot = deliveredFanLX(peer, owner, 101, 1, "same slot, other message");
        failsAsFanLX<ChatConflictFanLX>([&] { repository.appendDelivered(duplicateSlot); });

        // 已读游标只允许单调前进，不回退。
        assert(repository.lastConversationSequence(key) == 1);
        repository.markRead(key, 1);
        repository.markRead(key, 0);
        assert(repository.lastReadSequence(key) == 1);
    }
    removeTreeFanLX(directory);
}

void t23AccountIsolationFanLX() {
    const auto directory = temporaryRootFanLX();
    const std::string first = "2026001", second = "2026002", peer = "2026003";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, first));
        std::string requestId;
        repository.appendPending(conversationKeyFanLX(ScopeFanLX::Direct, first, peer), ScopeFanLX::Direct,
                                 peer, "first account", "", requestId);
        assert(repository.messageCount() == 1);
    }
    // 同一目录换账号打开：身份不符必须拒绝，不得把别人的库当自己的续写。
    failsFanLX([&] { SqliteChatRepositoryFanLX other(clientConfigFanLX(directory, second)); });
    // 换服务器实例身份同样拒绝（避免 messageId 语义冲突）。
    auto config = clientConfigFanLX(directory, first);
    config.serverInstanceId = "srv-instance-9999";
    failsFanLX([&] { SqliteChatRepositoryFanLX other(config); });
    // 独立目录才能各自持有历史。
    const auto otherDirectory = directory / "account-2026002";
    std::filesystem::create_directories(otherDirectory);
    {
        SqliteChatRepositoryFanLX isolated(clientConfigFanLX(otherDirectory, second));
        assert(isolated.messageCount() == 0);
    }
    removeTreeFanLX(directory);
}

// ---- T30 keyset 分页与关键词 -----------------------------------------------------------
void t30KeysetPaginationFanLX() {
    const auto directory = temporaryRootFanLX();
    const std::string owner = "2026001", peer = "2026002";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, owner));
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, owner, peer);
        repository.beginBulkImportFanLX();
        constexpr int total = 5000;
        for (int index = 1; index <= total; ++index)
            repository.appendDelivered(deliveredFanLX(peer, owner, index, index,
                                                     "body " + std::to_string(index)));
        repository.endBulkImportFanLX();

        HistoryQueryFanLX query;
        query.conversationKey = key;
        query.limit = 50;
        auto page = repository.queryHistory(query);
        assert(page.messages.size() == 50);
        assert(page.hasMore);
        assert(page.messages.front().conversationSequence == total);
        assert(page.messages.back().conversationSequence == total - 49);

        // 连续向前翻页：不跳页、不重复，直到最早一条。
        std::set<std::int64_t> seen;
        for (const auto &message : page.messages) seen.insert(message.conversationSequence);
        int pages = 1, collected = 50;
        query.beforeSequence = page.nextBeforeSequence;
        while (page.hasMore) {
            page = repository.queryHistory(query);
            for (const auto &message : page.messages) {
                assert(seen.insert(message.conversationSequence).second);
                ++collected;
            }
            query.beforeSequence = page.nextBeforeSequence;
            ++pages;
            assert(pages < 200);
        }
        assert(collected == total);

        // 关键词命中：逐页累计必须等于逐条统计。
        HistoryQueryFanLX filtered;
        filtered.conversationKey = key;
        filtered.keyword = "111";
        filtered.limit = 200;
        auto hits = repository.queryHistory(filtered);
        int hitCount = static_cast<int>(hits.messages.size());
        while (hits.hasMore) {
            filtered.beforeSequence = hits.nextBeforeSequence;
            hits = repository.queryHistory(filtered);
            hitCount += static_cast<int>(hits.messages.size());
        }
        int expected = 0;
        for (int index = 1; index <= total; ++index)
            if (std::to_string(index).find("111") != std::string::npos) ++expected;
        assert(hitCount == expected);

        // 空结果：不存在的关键词必须返回空页而不是报错。
        filtered.keyword = "no-such-keyword-XYZ";
        filtered.beforeSequence = 0;
        assert(repository.queryHistory(filtered).messages.empty());
    }
    removeTreeFanLX(directory);
}

void t35TimeRangeAndGlobalPagesFanLX() {
    const auto directory = temporaryRootFanLX();
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, "2026001"));
        for (int i = 1; i <= 7; ++i) {
            auto row = deliveredFanLX(i % 2 ? "2026002" : "2026003", "2026001", i, i, "match");
            row.acceptedAtUtc = i == 7 ? "" : "2026-09-26T10:00:00Z";
            repository.appendDelivered(row);
        }
        HistoryQueryFanLX query;
        query.keyword = "match";
        query.limit = 2;
        query.fromAcceptedAtUtc = "2026-09-26T10:00:00Z";
        query.toAcceptedAtUtc = query.fromAcceptedAtUtc;
        std::set<std::int64_t> ids;
        HistoryPageFanLX page;
        do {
            page = repository.queryHistory(query);
            for (const auto &row : page.messages) assert(ids.insert(*row.messageId).second);
            query.beforeAcceptedAtUtc = page.nextBeforeAcceptedAtUtc;
            query.beforeLocalSequence = page.nextBeforeLocalSequence;
        } while (page.hasMore);
        assert(ids.size() == 6); // 同秒不漏页，闭区间包含边界，未知时间排除。
        query.beforeAcceptedAtUtc.clear(); query.beforeLocalSequence = 0;
        query.fromAcceptedAtUtc = "2999-01-01T00:00:00Z";
        query.toAcceptedAtUtc.clear();
        assert(repository.queryHistory(query).messages.empty());
    }
    removeTreeFanLX(directory);
}

void t31EncodingAndUnicodeFanLX() {
    const auto directory = temporaryRootFanLX();
    const auto root = chineseRootFanLX();
    const std::string owner = "2026001", peer = "2026002";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(root, owner));
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, owner, peer);

        // 非 ASCII 用例用 UTF-8 字节显式构造：保证比较的是字节而不是"源码文件里的中文"。
        const std::string cyrillicLower = "\xD0\xBE\xD0\xB1\xD1\x8B\xD1\x87\xD0\xBD\xD1\x8B\xD0\xB9";
        const std::string cyrillicUpper = "\xD0\x9E\xD0\xB1\xD1\x8B\xD1\x87\xD0\xBD\xD1\x8B\xD0\xB9";
        const std::string emoji = "\xF0\x9F\x99\x82";
        const std::string underlined = "\xE4\xB8\x8B\xE5\x88\x92\xE7\xBA\xBF_\xE6\xB5\x8B\xE8\xAF\x95";

        const std::vector<std::string> bodies{"100% done", underlined, "back\\slash",
                                              " " + cyrillicLower, "emoji" + emoji,
                                              "percent%and_underscore"};
        std::int64_t sequence = 0;
        for (const auto &body : bodies) {
            ++sequence;
            repository.appendDelivered(deliveredFanLX(peer, owner, sequence, sequence, body));
        }

        HistoryQueryFanLX query;
        query.conversationKey = key;
        // % 与 _ 必须按字面量匹配：只有真正含有它们的记录命中。
        query.keyword = "%";
        assert(repository.queryHistory(query).messages.size() == 2);
        query.keyword = "_";
        assert(repository.queryHistory(query).messages.size() == 2);
        query.keyword = "\\";
        assert(repository.queryHistory(query).messages.size() == 1);
        query.keyword = "emoji" + emoji;
        assert(repository.queryHistory(query).messages.size() == 1);
        // 大小写折叠只保证 ASCII：非 ASCII 串按字节精确匹配。
        query.keyword = cyrillicUpper;
        assert(repository.queryHistory(query).messages.empty());
        query.keyword = cyrillicLower;
        assert(repository.queryHistory(query).messages.size() == 1);

        // ASCII 大小写不敏感（中文字符不受影响）。
        ++sequence;
        repository.appendDelivered(deliveredFanLX(peer, owner, sequence, sequence, "Hello World"));
        query.keyword = "hello world";
        assert(repository.queryHistory(query).messages.size() == 1);
        query.keyword = "HELLO";
        assert(repository.queryHistory(query).messages.size() == 1);

        // 发送者过滤 + 跨会话检索（界面"全局搜索"）。
        assert(repository.appendDelivered(deliveredFanLX("2026009", owner, 200, 200, "other peer")));
        query.keyword.clear();
        query.sender = "2026009";
        assert(repository.queryHistory(query).messages.empty()); // 该发送者不在本会话
        HistoryQueryFanLX global;
        global.sender = "2026009";
        const auto globalHits = repository.queryHistory(global);
        assert(globalHits.messages.size() == 1);
        assert(globalHits.messages.front().sender == "2026009");
        HistoryQueryFanLX globalKeyword;
        globalKeyword.keyword = "Hello";
        assert(repository.queryHistory(globalKeyword).messages.size() == 1);
        // 跨会话检索不支持会话序号游标：必须显式拒绝而不是返回错页。
        HistoryQueryFanLX invalid;
        invalid.sender = "2026009";
        invalid.beforeSequence = 5;
        failsFanLX([&] { repository.queryHistory(invalid); });
        // 既无会话键也无过滤条件：拒绝。
        HistoryQueryFanLX vacuous;
        failsFanLX([&] { repository.queryHistory(vacuous); });

        // 会话摘要：未读数按对端消息数计，推进已读后必须重新查询才反映变化。
        auto conversations = repository.conversations();
        assert(conversations.size() == 2);
        const auto findConversation = [](const std::vector<ConversationRowFanLX> &rows,
                                         const std::string &wanted) {
            for (const auto &row : rows)
                if (row.conversationKey == wanted) return row;
            throw std::runtime_error("conversation missing: " + wanted);
        };
        const auto peerSummary = findConversation(conversations, key);
        assert(peerSummary.unreadCount == 7);
        assert(peerSummary.lastConversationSequence == 7);
        repository.markRead(key, peerSummary.lastConversationSequence);
        const auto refreshed = repository.conversations();
        assert(findConversation(refreshed, key).unreadCount == 0);
        const auto thirdKey = conversationKeyFanLX(ScopeFanLX::Direct, owner, "2026009");
        assert(findConversation(refreshed, thirdKey).unreadCount == 1);

        // 只保留授权会话：离开群后本地目录同步清理。
        repository.forgetConversations({key});
        const auto pruned = repository.conversations();
        assert(pruned.size() == 1);
        assert(pruned.front().conversationKey == key);
    }
    removeTreeFanLX(root);
    removeTreeFanLX(directory);
}

void t32WorkerQueueAndCancellationFanLX() {
    // 数据库工作线程：异步写入结果事件、取消标记、队列满立即返回 false 不阻塞、停止流程。
    const auto directory = temporaryRootFanLX();
    {
        auto repository = std::make_unique<SqliteChatRepositoryFanLX>(clientConfigFanLX(directory, "2026001"));
        DatabaseWorkerFanLX worker(std::move(repository), 4, 4);
        std::mutex mutex;
        std::vector<DbEventFanLX> events;
        worker.registerEventConsumerFanLX([&](const DbEventFanLX &event) {
            std::lock_guard<std::mutex> guard(mutex);
            events.push_back(event);
        });
        worker.start();
        worker.markCancelled(77);
        assert(worker.isCancelled(77));
        assert(!worker.isCancelled(78));

        DbCommandFanLX command;
        command.kind = DbCommandFanLX::KindFanLX::AppendPending;
        command.commandId = 1;
        command.conversationKey = conversationKeyFanLX(ScopeFanLX::Direct, "2026001", "2026002");
        command.scope = ScopeFanLX::Direct;
        command.target = "2026002";
        command.body = "async write";
        assert(worker.postAndWait(command, std::chrono::seconds(5)));
        {
            std::lock_guard<std::mutex> guard(mutex);
            assert(events.size() >= 1);
            assert(events.front().kind == DbEventFanLX::KindFanLX::Result);
            assert(!events.front().requestId.empty());
        }
        assert(worker.repository().messageCount() == 1);

        // 队列容量为 4：超出必须立刻返回 false，而不是阻塞提交方。
        std::size_t rejected = 0;
        for (int index = 0; index < 64; ++index) {
            DbCommandFanLX extra = command;
            extra.commandId = 100 + index;
            if (!worker.tryPost(extra))
                ++rejected;
        }
        assert(rejected > 0);
        assert(worker.stop(std::chrono::seconds(5)));
        assert(!worker.running());
    }
    removeTreeFanLX(directory);
}

void t33PerformanceFanLX() {
    // 10 万条记录：写入、keyset 分页 p50/p95、关键词扫描分别测量（本机目标 p95 < 200 ms）。
    const auto directory = temporaryRootFanLX();
    const std::string owner = "2026001", peer = "2026002";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, owner));
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, owner, peer);

        constexpr int total = 100000;
        const auto writeStart = std::chrono::steady_clock::now();
        {
            // 批量导入走同一套 appendDelivered 逻辑，只是合并提交点（用于构造测量数据）。
            repository.beginBulkImportFanLX();
            for (int index = 1; index <= total; ++index)
                repository.appendDelivered(deliveredFanLX(peer, owner, index, index,
                                                         "message " + std::to_string(index)));
            repository.endBulkImportFanLX();
        }
        const auto writeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - writeStart)
                                 .count();
        assert(repository.messageCount() == total);

        HistoryQueryFanLX query;
        query.conversationKey = key;
        query.limit = 50;
        std::vector<std::int64_t> samples;
        auto page = repository.queryHistory(query);
        assert(page.messages.size() == 50);
        for (int round = 0; round < 60; ++round) {
            const auto start = std::chrono::steady_clock::now();
            page = repository.queryHistory(query);
            samples.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - start)
                                  .count());
            assert(page.messages.size() == 50);
            query.beforeSequence = page.nextBeforeSequence > 0 ? page.nextBeforeSequence : total - 1;
        }
        std::sort(samples.begin(), samples.end());
        const auto p50 = samples[samples.size() / 2] / 1000.0;
        const auto p95 = samples[static_cast<std::size_t>(samples.size() * 0.95)] / 1000.0;

        // 关键词扫描单独测量（未启用 FTS5，指标不预承诺与分页相同）。
        HistoryQueryFanLX scan;
        scan.conversationKey = key;
        scan.keyword = "message 99999";
        scan.limit = 50;
        const auto scanStart = std::chrono::steady_clock::now();
        const auto scanHits = repository.queryHistory(scan);
        const auto scanMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - scanStart)
                                .count();
        assert(!scanHits.messages.empty());

        std::cout << "[perf] write " << total << " rows=" << writeMs << " ms; keyset page p50=" << p50
                  << " ms p95=" << p95 << " ms; keyword scan=" << scanMs << " ms" << std::endl;
        std::cout << "[perf] plan=" << repository.explainPlanFanLX(query) << std::endl;
        assert(p95 < 200.0);
    }
    removeTreeFanLX(directory);
}

void t34UnsupportedSchemaVersionFanLX() {
    const auto directory = temporaryRootFanLX();
    const auto database = directory / "chat.db";
    {
        sqliteFanLX::ConnectionFanLX connection(database);
        connection.execute("CREATE TABLE schema_meta(key TEXT PRIMARY KEY, value TEXT NOT NULL)");
        connection.execute("INSERT INTO schema_meta VALUES('schema_version','99')");
    }
    // 旧代码打开新结构：必须显式拒绝，不得静默降级写入。
    failsAsFanLX<ChatRepositoryErrorFanLX>(
        [&] { SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, "2026001")); });
    // 损坏的版本元数据同样拒绝。
    {
        sqliteFanLX::ConnectionFanLX connection(database);
        connection.execute("UPDATE schema_meta SET value = 'abc' WHERE key = 'schema_version'");
    }
    failsFanLX([&] { SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, "2026001")); });
    removeTreeFanLX(directory);
}

// ---- T40 服务端消息库 ------------------------------------------------------------------
void t40ServerAcceptAndIdempotencyFanLX() {
    const auto directory = temporaryRootFanLX();
    {
        SqliteMessageStoreFanLX store(directory / "server-chat.db", "srv-instance-0001");

        ServerAcceptRequestFanLX request;
        request.scope = ScopeFanLX::Direct;
        request.sender = "2026001";
        request.target = "2026002";
        request.requestId = "device-2026001-1";
        request.body = "first server message";
        request.recipients = {"2026001", "2026002"};

        const auto first = store.accept(request);
        assert(!first.replayed);
        assert(first.message.messageId.has_value() && *first.message.messageId == 1);
        assert(first.message.conversationSequence == 1);
        assert(!first.message.acceptedAtUtc.empty());
        assert(store.messageCount() == 1);

        // 同 requestId 同内容：返回旧结果，不新增行、不消耗序号。
        const auto replay = store.accept(request);
        assert(replay.replayed);
        assert(*replay.message.messageId == 1);
        assert(replay.message.conversationSequence == 1);
        assert(store.messageCount() == 1);
        assert(store.nextMessageId() == 2);

        // 同 requestId 不同内容：拒绝，不覆盖。
        auto conflicting = request;
        conflicting.body = "tampered";
        failsAsFanLX<ChatConflictFanLX>([&] { store.accept(conflicting); });
        assert(store.messageCount() == 1);

        const auto found = store.findByIdempotencyKey("2026001", "device-2026001-1");
        assert(found.has_value());
        assert(found->message.body == "first server message");
        assert(!store.findByIdempotencyKey("2026001", "no-such-request").has_value());

        // 会话序号按会话独立分配。
        ServerAcceptRequestFanLX group;
        group.scope = ScopeFanLX::Group;
        group.sender = "2026001";
        group.target = "1001";
        group.requestId = "device-2026001-2";
        group.body = "group message";
        group.recipients = {"2026001", "2026002", "2026003"};
        const auto groupResult = store.accept(group);
        assert(groupResult.message.conversationSequence == 1);
        assert(*groupResult.message.messageId == 2);
        assert(store.integrityCheck() == "ok");
    }
    removeTreeFanLX(directory);
}

void t41ServerDeliveryCursorFanLX() {
    const auto directory = temporaryRootFanLX();
    {
        SqliteMessageStoreFanLX store(directory / "server-chat.db", "srv-instance-0001");
        for (int index = 1; index <= 5; ++index) {
            ServerAcceptRequestFanLX request;
            request.scope = ScopeFanLX::Direct;
            request.sender = "2026001";
            request.target = "2026002";
            request.requestId = "req-" + std::to_string(index);
            request.body = "m" + std::to_string(index);
            request.recipients = {"2026001", "2026002"};
            store.accept(request);
        }
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, "2026001", "2026002");

        // 未确认前：5 条都是候选，且按 messageId 递增（保证先投递早消息）。
        DeliveryCursorMapFanLX cursors;
        auto pending = store.pendingFor("2026002", cursors, 256);
        assert(pending.size() == 5);
        for (std::size_t index = 1; index < pending.size(); ++index)
            assert(*pending[index - 1].message.messageId < *pending[index].message.messageId);

        // 推进到 3：只剩后两条；库内游标必须能被重新读出（重连/重启后继续而不是重放）。
        store.advanceDeliveryCursor("2026002", key, 3);
        const auto persisted = store.deliveryCursorsFor("2026002");
        assert(persisted.size() == 1 && persisted.at(key) == 3);
        cursors = persisted;
        pending = store.pendingFor("2026002", cursors, 256);
        assert(pending.size() == 2);
        assert(pending.front().message.conversationSequence == 4);

        // 游标不得回退：库内仍是 3，即使传入更小的会话内游标也不会让服务端倒退。
        store.advanceDeliveryCursor("2026002", key, 1);
        assert(store.deliveryCursorsFor("2026002").at(key) == 3);
        assert(store.pendingFor("2026002", store.deliveryCursorsFor("2026002"), 256).size() == 2);

        // 非收件人看不到任何消息。
        assert(store.pendingFor("2026099", {}, 256).empty());
    }
    removeTreeFanLX(directory);
}

// ---- T50 迁移 --------------------------------------------------------------------------
// 旧存储正文用显式 UTF-8 转义构造并与断言共用同一常量：比较的是字节而不是源码里的中文。
const std::string legacyPendingBodyFanLX() {
    return "\xE6\x97\xA7\xE6\xA0\xBC\xE5\xBC\x8F\xE5\xBE\x85\xE5\x8F\x91\xE9\x80\x81";
}
const std::string legacyReceivedBodyFanLX() {
    return "\xE6\x97\xA7\xE6\xA0\xBC\xE5\xBC\x8F\xE5\xB7\xB2\xE6\x94\xB6\xE5\x88\xB0";
}

nlohmann::json legacyClientJsonFanLX(const std::string &account) {
    nlohmann::json outbox = nlohmann::json::object();
    nlohmann::json inbox = nlohmann::json::object();
    nlohmann::json cursors = nlohmann::json::object();
    const auto directKey = conversationKeyFanLX(ScopeFanLX::Direct, account, "2026002");
    outbox["device-2026001-1"] = {{"sender", account},
                                 {"recipient", "2026002"},
                                 {"sequence", 1},
                                 {"text", legacyPendingBodyFanLX()},
                                 {"type", "SEND"},
                                 {"requestId", "device-2026001-1"},
                                 {"scope", "direct"},
                                 {"messageId", 0},
                                 {"conversationSeq", 0}};
    inbox["7"] = {{"sender", "2026002"},
                  {"recipient", account},
                  {"sequence", 0},
                  {"text", legacyReceivedBodyFanLX()},
                  {"type", "DELIVER"},
                  {"requestId", ""},
                  {"scope", "direct"},
                  {"messageId", 7},
                  {"conversationSeq", 2}};
    cursors[directKey] = 2;
    return {{"schema", "fanlx-im-client-1"},
            {"account", account},
            {"host", "127.0.0.1"},
            {"port", 9500},
            {"device", "device-2026001"},
            {"next", 2},
            {"outbox", outbox},
            {"inbox", inbox},
            {"cursors", cursors}};
}

void t50ClientMigrationFanLX() {
    const auto directory = temporaryRootFanLX();
    const std::string account = "2026001";
    const auto legacy = directory / "state.json";
    std::ofstream(legacy, std::ios::binary) << legacyClientJsonFanLX(account).dump(2);

    ChatMigrationFanLX::ClientSourceFanLX source;
    source.legacyJsonPath = legacy;
    source.databasePath = directory / "chat.db";
    source.serverInstanceId = "srv-instance-0001";
    source.ownerAccount = account;
    source.deviceId = "device-2026001";

    const auto report = ChatMigrationFanLX::migrateClient(source);
    assert(report.migrated);
    assert(!report.alreadyDone);
    assert(report.clientMessages == 2);
    assert(report.clientOutbox == 1);
    assert(!report.warnings.empty()); // 时间语义必须显式声明
    assert(ChatMigrationFanLX::clientMigrationDone(source.databasePath));

    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, account));
        assert(repository.messageCount() == 2);
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, account, "2026002");
        HistoryQueryFanLX query;
        query.conversationKey = key;
        query.ascending = true;
        const auto page = repository.queryHistory(query);
        assert(page.messages.size() == 2);
        // 待发送消息保留"待发送"状态与原始幂等键，不被伪装成已确认。
        assert(page.messages.front().body == legacyPendingBodyFanLX());
        assert(page.messages.front().state == MessageStateFanLX::Pending);
        assert(!page.messages.front().messageId.has_value());
        // 已投递消息保留服务器编号与会话序号；无可信时间则留空。
        assert(page.messages.back().body == legacyReceivedBodyFanLX());
        assert(page.messages.back().conversationSequence == 2);
        assert(page.messages.back().messageId.has_value() && *page.messages.back().messageId == 7);
        assert(page.messages.back().acceptedAtUtc.empty());
        const auto pending = repository.pending();
        assert(pending.size() == 1);
        assert(pending.front().requestId == "device-2026001-1");
        assert(repository.lastConversationSequence(key) == 2);
    }
    // 迁移重跑：已完成即直接返回，不重复导入、不覆盖新库。
    const auto again = ChatMigrationFanLX::migrateClient(source);
    assert(again.alreadyDone);
    assert(!again.migrated);
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, account));
        assert(repository.messageCount() == 2);
    }
    // 原 JSON 保持为只读备份：字节未变。
    assert(nlohmann::json::parse(AtomicFileWriterFanLX::read(legacy)).at("schema") == "fanlx-im-client-1");
    removeTreeFanLX(directory);
}

void t52MigrationFailureLeavesNoPartialDatabaseFanLX() {
    const auto directory = temporaryRootFanLX();
    const auto legacy = directory / "state.json";
    auto payload = legacyClientJsonFanLX("2026001");
    // 制造同一 requestId 内容冲突：迁移必须拒绝，且不留下半成品数据库。
    payload["inbox"]["7"]["sender"] = "2026001";
    payload["inbox"]["7"]["requestId"] = "device-2026001-1";
    payload["inbox"]["7"]["conversationSeq"] = 1;
    std::ofstream(legacy, std::ios::binary) << payload.dump(2);

    ChatMigrationFanLX::ClientSourceFanLX source;
    source.legacyJsonPath = legacy;
    source.databasePath = directory / "chat.db";
    source.serverInstanceId = "srv-instance-0001";
    source.ownerAccount = "2026001";
    source.deviceId = "device-2026001";

    failsFanLX([&] { ChatMigrationFanLX::migrateClient(source); });
    assert(!std::filesystem::exists(source.databasePath));
    assert(!ChatMigrationFanLX::clientMigrationDone(source.databasePath));
    assert(std::filesystem::exists(legacy)); // 旧数据保留，可排查后重试
    // 修复后重试必须成功（失败不写标记，不阻断后续）。
    std::ofstream(legacy, std::ios::binary) << legacyClientJsonFanLX("2026001").dump(2);
    const auto report = ChatMigrationFanLX::migrateClient(source);
    assert(report.migrated);
    assert(report.clientMessages == 2);
    removeTreeFanLX(directory);
}

void t53ServerMigrationFanLX() {
    const auto directory = temporaryRootFanLX();
    const auto legacy = directory / "state.json";
    // 旧 JSON 服务端只服务 QQ 一条通道；新版服务端内部标识是"服务名|号码/群号"，
    // 因此迁移后的账号、会话键与投递游标都带 "QQ|" 前缀（会话键是两端都带）。
    const auto qualifiedFanLX = [](const std::string &id) { return "QQ|" + id; };
    const auto directKey = conversationKeyFanLX(ScopeFanLX::Direct, qualifiedFanLX("2026001"),
                                                qualifiedFanLX("2026002"));
    nlohmann::json messages = nlohmann::json::array();
    for (int index = 1; index <= 3; ++index) {
        messages.push_back({{"message",
                             {{"sender", "2026001"},
                              {"recipient", "2026002"},
                              {"sequence", 0},
                              {"text", "legacy " + std::to_string(index)},
                              {"type", "DELIVER"},
                              {"requestId", "req-" + std::to_string(index)},
                              {"scope", "direct"},
                              {"messageId", index},
                              {"conversationSeq", index}}},
                            {"recipients", {"2026001", "2026002"}}});
    }
    nlohmann::json payload = {{"schema", "fanlx-im-server-1"},
                              {"next", 4},
                              {"messages", messages},
                              {"acks", {{"2026002", {{conversationKeyFanLX(ScopeFanLX::Direct, "2026001", "2026002"), 2}}}}},
                              {"seqs", {{conversationKeyFanLX(ScopeFanLX::Direct, "2026001", "2026002"), 3}}}};
    std::ofstream(legacy, std::ios::binary) << payload.dump(2);

    ChatMigrationFanLX::ServerSourceFanLX source;
    source.legacyJsonPath = legacy;
    source.databasePath = directory / "server-chat.db";
    source.serverInstanceId = "srv-instance-0001";

    const auto report = ChatMigrationFanLX::migrateServer(source);
    assert(report.migrated);
    assert(report.serverMessages == 3);
    assert(report.serverCursors == 1);
    assert(!report.warnings.empty());

    {
        SqliteMessageStoreFanLX store(directory / "server-chat.db", "srv-instance-0001");
        assert(store.messageCount() == 3);
        assert(store.nextMessageId() == 4); // 迁移后编号不得与旧编号重叠
        const auto cursors = store.deliveryCursorsFor("QQ|2026002");
        assert(cursors.size() == 1 && cursors.at(directKey) == 2);
        const auto pending = store.pendingFor("QQ|2026002", cursors, 64);
        assert(pending.size() == 1);
        assert(pending.front().message.conversationSequence == 3);
        // 迁移后的新消息编号从 4 开始，不覆盖旧记录。
        ServerAcceptRequestFanLX request;
        request.scope = ScopeFanLX::Direct;
        request.sender = "QQ|2026001";
        request.target = "QQ|2026002";
        request.requestId = "req-new";
        request.body = "after migration";
        request.recipients = {"QQ|2026001", "QQ|2026002"};
        const auto accepted = store.accept(request);
        assert(*accepted.message.messageId == 4);
        assert(accepted.message.conversationSequence == 4);
    }
    const auto again = ChatMigrationFanLX::migrateServer(source);
    assert(again.alreadyDone);
    removeTreeFanLX(directory);
}

void t54WorkerReportsFailuresFanLX() {
    // 数据库线程内失败必须作为 Failure 事件返回，不得异常逃逸导致 terminate。
    const auto directory = temporaryRootFanLX();
    {
        auto repository = std::make_unique<SqliteChatRepositoryFanLX>(clientConfigFanLX(directory, "2026001"));
        DatabaseWorkerFanLX worker(std::move(repository), 8, 8);
        std::mutex mutex;
        std::vector<DbEventFanLX> failures;
        worker.registerEventConsumerFanLX([&](const DbEventFanLX &event) {
            if (event.kind == DbEventFanLX::KindFanLX::Failure) {
                std::lock_guard<std::mutex> guard(mutex);
                failures.push_back(event);
            }
        });
        worker.start();

        DbCommandFanLX command;
        command.kind = DbCommandFanLX::KindFanLX::MarkAccepted;
        command.commandId = 1;
        command.requestId = "no-such-request";
        command.messageId = 3;
        command.conversationSequence = 3;
        assert(worker.postAndWait(command, std::chrono::seconds(5)));
        {
            std::lock_guard<std::mutex> guard(mutex);
            assert(failures.size() == 1);
            assert(failures.front().commandId == 1);
            assert(!failures.front().failureReason.empty());
        }
        assert(worker.failureCount() >= 1);

        // 未注册的自定义任务也必须报告失败而不是崩溃。
        DbCommandFanLX unknown;
        unknown.kind = DbCommandFanLX::KindFanLX::Custom;
        unknown.commandId = 2;
        unknown.customName = "unregistered-task";
        assert(worker.postAndWait(unknown, std::chrono::seconds(5)));
        assert(worker.failureCount() >= 2);

        // 停止流程：队列为空后停止，线程可回收且在时间预算内。
        const auto start = std::chrono::steady_clock::now();
        assert(worker.stop(std::chrono::seconds(3)));
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        assert(elapsed < 3000);
        assert(worker.queuedCount() == 0);
    }
    removeTreeFanLX(directory);
}

// 回归：failed 是终态，不再被 pending() 当作"待发送"取出重发。分两段验证：
// (a) 被拒/无法组帧的消息标记失败后必须离开"待发送"集合，但仍在库中可查（不静默丢弃输入）；
// (b) 仓储层是"哑存储"、不校验正文长度，所以旧版本留下的超长记录确实以 'pending' 存在——
//     这正是 ImClientFanLX::sweepUnsendablePendingFanLX 在启动时清扫的对象。
// 背景：pending() 曾写成 state IN ('pending','failed')，导致这些消息在每次重连时被重放，
// 形成"永远重连、永远报错"的循环。
void t55FailedNotPendingFanLX() {
    const auto directory = temporaryRootFanLX();
    const std::string owner = "2026001", peer = "2026002";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, owner));
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, owner, peer);
        HistoryQueryFanLX query;
        query.conversationKey = key;

        // (a) 正常提交 → 被判定失败 → 必须离开待发送集合，但消息与原因保留。
        std::string rejected;
        repository.appendPending(key, ScopeFanLX::Direct, peer, "hello", "", rejected);
        assert(repository.pending().size() == 1);
        repository.markFailed(rejected, "消息过长：5000 字节，单条上限 4096 字节");
        assert(repository.pending().empty());
        const auto failedPage = repository.queryHistory(query);
        assert(failedPage.messages.size() == 1);
        assert(failedPage.messages.front().state == MessageStateFanLX::Failed);
        assert(failedPage.messages.front().failureReason.find("消息过长") != std::string::npos);

        // (b) 旧库遗留：超长正文照样能被存成 'pending'（校验在网络层入口，不在仓储层）。
        std::string poisoned;
        repository.appendPending(key, ScopeFanLX::Direct, peer, std::string(5000, 'x'), "", poisoned);
        const auto legacy = repository.pending();
        assert(legacy.size() == 1);
        assert(legacy.front().requestId == poisoned);
        assert(legacy.front().body.size() == 5000);
        // 启动清扫的动作就是下面这句；清完不再被重放，也不影响后续新消息。
        repository.markFailed(poisoned, "历史遗留消息无法发送，已标记失败：消息过长");
        assert(repository.pending().empty());

        std::string afterwards;
        repository.appendPending(key, ScopeFanLX::Direct, peer, "next", "", afterwards);
        const auto pending = repository.pending();
        assert(pending.size() == 1);
        assert(pending.front().requestId == afterwards);
    }
    removeTreeFanLX(directory);
}

// 回归：允许补齐历史空洞，不得再用“序号必须大于水位”拒绝投递。
//
// 背景（2026-09-29 实机报错“会话投递乱序：序号 8 不大于已落库序号 15”）：
// 单聊序号是两端共用的一条单调计数器（conversationKeyFanLX 排序后同键），而客户端
// sync_cursors.last_sequence 会被 markAccepted（自己发出的消息）推到**该消息自身的序号**。
// 于是“自己已到 15、对端更早的 8 还没投到”完全正常；老规则会永久拒给 8，且不落库、不回 ACK，
// 服务端就反复重投、每 10 秒关会话重连——表现为对端发不进来、自己发得出去的单向故障。
// 这里锁定：空洞可补、真冲突仍拒绝。
void t56GapFillAllowedFanLX() {
    const auto directory = temporaryRootFanLX();
    const std::string owner = "2026001", peer = "2026002";
    {
        SqliteChatRepositoryFanLX repository(clientConfigFanLX(directory, owner));
        const auto key = conversationKeyFanLX(ScopeFanLX::Direct, owner, peer);

        // 先落 seq 1 与 seq 3，形成空洞（缺 2）。
        assert(repository.appendDelivered(deliveredFanLX(peer, owner, 200, 1, "first")));
        assert(repository.appendDelivered(deliveredFanLX(peer, owner, 202, 3, "third")));
        assert(repository.lastConversationSequence(key) == 3);

        // 空洞补齐必须被接受（老规则会以“不大于已落库序号”拒绝，这是死锁的根源）。
        assert(repository.appendDelivered(deliveredFanLX(peer, owner, 201, 2, "second")));
        assert(repository.messageCount() == 3);

        // 同一序号被另一条消息占用仍是真冲突。
        failsAsFanLX<ChatConflictFanLX>(
            [&] { repository.appendDelivered(deliveredFanLX(peer, owner, 299, 2, "clash")); });

        // 水位被自己发出的消息推高后，对端更早序号的补投仍必须被接受。
        std::string requestId;
        const auto localSequence =
            repository.appendPending(key, ScopeFanLX::Direct, peer, "mine", "", requestId);
        assert(localSequence > 0);
        repository.markAccepted(requestId, 300, 9, "2026-09-29T10:00:00Z");
        assert(repository.lastConversationSequence(key) == 9);
        assert(repository.appendDelivered(deliveredFanLX(peer, owner, 203, 4, "late but valid")));
        assert(repository.messageCount() == 5);

        // 自己消息的回显（同 messageId）仍应幂等：不新增行，但调用方可以正常回 ACK。
        auto echo = deliveredFanLX(owner, peer, 300, 9, "mine");
        assert(repository.appendDelivered(echo) == false);
        assert(repository.messageCount() == 5);
    }
    removeTreeFanLX(directory);
}

} // namespace

int main(int argc, char **argv) {
    std::string filter;
    bool performance = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--perf")
            performance = true; // 十万条测量单独运行，避免拖慢日常 ctest
        else
            filter = argument;
    }
    const auto run = [&](const char *name, auto body) {
        if (!filter.empty() && filter != name)
            return;
        const auto start = std::chrono::steady_clock::now();
        body();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
        std::cout << "[ok] " << name << " (" << ms << " ms)" << std::endl;
    };
    run("t20", t20ClientRoundTripFanLX);
    run("t21", t21IdempotencyAndConflictFanLX);
    run("t22", t22RollbackAndConstraintsFanLX);
    run("t23", t23AccountIsolationFanLX);
    run("t30", t30KeysetPaginationFanLX);
    run("t31", t31EncodingAndUnicodeFanLX);
    run("t32", t32WorkerQueueAndCancellationFanLX);
    if (performance)
        run("t33", t33PerformanceFanLX);
    run("t34", t34UnsupportedSchemaVersionFanLX);
    run("t35", t35TimeRangeAndGlobalPagesFanLX);
    run("t40", t40ServerAcceptAndIdempotencyFanLX);
    run("t41", t41ServerDeliveryCursorFanLX);
    run("t50", t50ClientMigrationFanLX);
    run("t52", t52MigrationFailureLeavesNoPartialDatabaseFanLX);
    run("t53", t53ServerMigrationFanLX);
    run("t54", t54WorkerReportsFailuresFanLX);
    run("t55", t55FailedNotPendingFanLX);
    run("t56", t56GapFillAllowedFanLX);
    std::cout << "chat_history_behavior: all cases passed" << std::endl;
    return 0;
}

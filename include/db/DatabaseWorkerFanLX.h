#pragma once
#include "db/ChatRepositoryFanLX.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace chatDbFanLX {

// 命令载荷：全部字段都是值类型。投递后命令不再依赖调用方栈上的对象。
struct DbCommandFanLX {
    enum class KindFanLX {
        AppendPending,     // 本地保存待发送消息
        MarkAccepted,      // 收到 SEND_ACK
        MarkFailed,        // 服务器拒绝或存储失败
        AppendDelivered,   // 收到 DELIVER
        MarkRead,          // 推进已读游标
        ForgetConversations,
        Custom             // 注册任务（历史分页、会话列表等）
    };
    KindFanLX kind = KindFanLX::Custom;
    std::int64_t commandId = 0;          // 调用方分配的单调编号，用于把结果与命令对应
    std::int64_t queryId = 0;            // 查询标识：用于丢弃过期结果
    std::int64_t accountSessionId = 0;   // 账号会话标识：切换账号后旧结果必须丢弃
    std::string customName;
    ScopeFanLX scope = ScopeFanLX::Direct;
    std::string target;
    std::string body;
    std::string requestId;
    std::string conversationKey;
    std::int64_t messageId = 0;
    std::int64_t conversationSequence = 0;
    std::string acceptedAtUtc;
    std::string reason;
    MessageRowFanLX row;
    HistoryQueryFanLX query;
    std::vector<std::string> keepKeys;
};

struct DatabaseResultFanLX {
    bool value = false;
    std::int64_t counter = 0;
    HistoryPageFanLX history;
    std::vector<ConversationRowFanLX> conversations;
};

// 数据库线程返回的事件：结果或失败，二者必有其一。
struct DbEventFanLX {
    enum class KindFanLX { Result, Failure };
    KindFanLX kind = KindFanLX::Result;
    std::int64_t commandId = 0;
    std::int64_t queryId = 0;
    std::int64_t accountSessionId = 0;
    std::string customName;
    std::string requestId;
    std::string failureReason;
    bool value = false;      // appendDelivered 是否新增行
    std::int64_t counter = 0; // appendPending 返回的本地序号
    HistoryPageFanLX history;
    std::vector<ConversationRowFanLX> conversations;
};

// 独立数据库工作线程：独占 ChatRepositoryFanLX（连接与语句都不跨线程）。
// 提交方只负责入队；结果通过注册的事件消费者回调返回。
class DatabaseWorkerFanLX {
  public:
    using TaskFanLX = std::function<DatabaseResultFanLX(DatabaseWorkerFanLX &, const DbCommandFanLX &)>;
    using EventConsumerFanLX = std::function<void(const DbEventFanLX &)>;

    DatabaseWorkerFanLX(std::unique_ptr<ChatRepositoryFanLX> repository, std::size_t queueCapacity = 64,
                        std::size_t commandBudget = 8);
    ~DatabaseWorkerFanLX();
    DatabaseWorkerFanLX(const DatabaseWorkerFanLX &) = delete;
    DatabaseWorkerFanLX &operator=(const DatabaseWorkerFanLX &) = delete;

    // 注册自定义任务（历史分页、会话列表、统计）。必须在 start 之前调用。
    void registerTaskFanLX(const std::string &name, TaskFanLX task);
    // 注册事件消费者：在数据库线程内调用，实现必须极短且自行加锁。
    void registerEventConsumerFanLX(EventConsumerFanLX consumer);
    void start();

    // 入队。队列满返回 false（调用方必须显示繁忙并保留输入文本，不得阻塞 UI 线程）。
    bool tryPost(const DbCommandFanLX &command);
    // 入队并等待处理完成；超时返回 false。用于退出前的有界等待。
    bool postAndWait(const DbCommandFanLX &command, std::chrono::milliseconds timeout);

    // 取消标记：已投递但尚未执行的查询据此丢弃结果。
    void markCancelled(std::int64_t queryId);
    bool isCancelled(std::int64_t queryId) const;

    bool stop(std::chrono::milliseconds timeout = std::chrono::seconds(3));
    void requestStop() noexcept;
    void join() noexcept;
    bool running() const noexcept { return runningFanLX.load(); }
    std::size_t queuedCount() const;
    std::size_t processedCount() const noexcept { return processedFanLX.load(); }
    std::size_t failureCount() const noexcept { return failuresFanLX.load(); }

    ChatRepositoryFanLX &repository() { return *repositoryFanLX; }
    const ChatRepositoryFanLX &repository() const { return *repositoryFanLX; }

    // 由任务实现调用：把结果发布给事件消费者。
    void publish(DbEventFanLX event);

  private:
    void runLoopFanLX() noexcept;
    void processFanLX(const DbCommandFanLX &command, DbEventFanLX &event);

    std::unique_ptr<ChatRepositoryFanLX> repositoryFanLX;
    std::size_t capacityFanLX;
    std::size_t budgetFanLX;
    mutable std::mutex mutexFanLX;
    std::condition_variable readyFanLX;
    std::condition_variable idleFanLX;
    std::deque<DbCommandFanLX> queueFanLX;
    std::map<std::string, TaskFanLX> tasksFanLX;
    std::vector<std::int64_t> cancelledFanLX;
    std::map<std::int64_t, bool> completedFanLX;
    EventConsumerFanLX consumeFanLX;
    std::thread workerFanLX;
    std::atomic<bool> stopFanLX{false};
    std::atomic<bool> runningFanLX{false};
    std::atomic<std::size_t> processedFanLX{0};
    std::atomic<std::size_t> failuresFanLX{0};
};

} // namespace chatDbFanLX

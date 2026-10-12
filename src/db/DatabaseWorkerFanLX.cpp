#include "db/DatabaseWorkerFanLX.h"
#include <algorithm>

namespace chatDbFanLX {

DatabaseWorkerFanLX::DatabaseWorkerFanLX(std::unique_ptr<ChatRepositoryFanLX> repository,
                                        std::size_t queueCapacity, std::size_t commandBudget)
    : repositoryFanLX(std::move(repository)), capacityFanLX(std::max<std::size_t>(queueCapacity, 1)),
      budgetFanLX(std::max<std::size_t>(commandBudget, 1)) {
    if (!repositoryFanLX)
        throw std::invalid_argument("数据库工作线程缺少仓储实现");
}

DatabaseWorkerFanLX::~DatabaseWorkerFanLX() {
    // 析构不做无界等待：先请求停止，再在有限预算内 join。
    stop(std::chrono::seconds(3));
}

void DatabaseWorkerFanLX::registerTaskFanLX(const std::string &name, TaskFanLX task) {
    if (name.empty() || !task)
        throw std::invalid_argument("数据库任务注册参数非法");
    std::lock_guard<std::mutex> guard(mutexFanLX);
    if (runningFanLX.load())
        throw std::logic_error("数据库工作线程已启动，不能再注册任务");
    tasksFanLX[name] = std::move(task);
}

void DatabaseWorkerFanLX::registerEventConsumerFanLX(EventConsumerFanLX consumer) {
    std::lock_guard<std::mutex> guard(mutexFanLX);
    consumeFanLX = std::move(consumer);
}

void DatabaseWorkerFanLX::start() {
    std::lock_guard<std::mutex> guard(mutexFanLX);
    if (workerFanLX.joinable())
        throw std::logic_error("数据库工作线程已启动");
    stopFanLX = false;
    workerFanLX = std::thread(&DatabaseWorkerFanLX::runLoopFanLX, this);
}

bool DatabaseWorkerFanLX::tryPost(const DbCommandFanLX &command) {
    {
        std::lock_guard<std::mutex> guard(mutexFanLX);
        if (stopFanLX.load() || queueFanLX.size() >= capacityFanLX)
            return false;
        queueFanLX.push_back(command);
    }
    readyFanLX.notify_all();
    return true;
}

bool DatabaseWorkerFanLX::postAndWait(const DbCommandFanLX &command, std::chrono::milliseconds timeout) {
    {
        std::lock_guard<std::mutex> guard(mutexFanLX);
        if (stopFanLX.load() || queueFanLX.size() >= capacityFanLX)
            return false;
        queueFanLX.push_back(command);
    }
    readyFanLX.notify_all();
    std::unique_lock<std::mutex> guard(mutexFanLX);
    return idleFanLX.wait_for(guard, timeout, [this, &command] {
        return completedFanLX.find(command.commandId) != completedFanLX.end();
    });
}

void DatabaseWorkerFanLX::markCancelled(std::int64_t queryId) {
    if (queryId <= 0)
        return;
    std::lock_guard<std::mutex> guard(mutexFanLX);
    cancelledFanLX.push_back(queryId);
    // 只保留最近 256 条取消标记，避免长时间运行无界增长。
    if (cancelledFanLX.size() > 256)
        cancelledFanLX.erase(cancelledFanLX.begin(), cancelledFanLX.begin() + 128);
}

bool DatabaseWorkerFanLX::isCancelled(std::int64_t queryId) const {
    if (queryId <= 0)
        return false;
    std::lock_guard<std::mutex> guard(mutexFanLX);
    return std::find(cancelledFanLX.begin(), cancelledFanLX.end(), queryId) != cancelledFanLX.end();
}

bool DatabaseWorkerFanLX::stop(std::chrono::milliseconds timeout) {
    requestStop();
    {
        std::unique_lock<std::mutex> guard(mutexFanLX);
        // 已接受的命令必须处理完或明确报告失败，不能静默丢弃。
        (void)idleFanLX.wait_for(guard, timeout, [this] { return queueFanLX.empty(); });
    }
    if (workerFanLX.joinable())
        workerFanLX.join();
    std::lock_guard<std::mutex> guard(mutexFanLX);
    runningFanLX = false;
    return true;
}

void DatabaseWorkerFanLX::requestStop() noexcept {
    {
        std::lock_guard<std::mutex> guard(mutexFanLX);
        stopFanLX = true;
    }
    readyFanLX.notify_all();
}

void DatabaseWorkerFanLX::join() noexcept {
    if (workerFanLX.joinable())
        workerFanLX.join();
}

std::size_t DatabaseWorkerFanLX::queuedCount() const {
    std::lock_guard<std::mutex> guard(mutexFanLX);
    return queueFanLX.size();
}

void DatabaseWorkerFanLX::publish(DbEventFanLX event) {
    EventConsumerFanLX consumer;
    {
        std::lock_guard<std::mutex> guard(mutexFanLX);
        consumer = consumeFanLX;
    }
    if (consumer)
        consumer(event);
}

void DatabaseWorkerFanLX::processFanLX(const DbCommandFanLX &command, DbEventFanLX &event) {
    auto &repository = *repositoryFanLX;
    switch (command.kind) {
    case DbCommandFanLX::KindFanLX::AppendPending: {
        // requestId 必须与"本次分配的本地序号"绑定：先分配序号，再据设备标识拼出幂等键，
        // 并把键随结果回传（调用方据此发 SEND，重连后可用同一键重放）。
        const auto sequence = repository.appendPending(command.conversationKey, command.scope, command.target,
                                                       command.body, command.requestId, event.requestId);
        event.counter = sequence;
        if (event.requestId.empty())
            event.requestId = buildRequestIdFanLX(repository.deviceId(), sequence);
        return;
    }
    case DbCommandFanLX::KindFanLX::MarkAccepted:
        repository.markAccepted(command.requestId, command.messageId, command.conversationSequence,
                               command.acceptedAtUtc);
        return;
    case DbCommandFanLX::KindFanLX::MarkFailed:
        repository.markFailed(command.requestId, command.reason);
        return;
    case DbCommandFanLX::KindFanLX::AppendDelivered:
        event.value = repository.appendDelivered(command.row);
        return;
    case DbCommandFanLX::KindFanLX::MarkRead:
        repository.markRead(command.conversationKey, command.conversationSequence);
        return;
    case DbCommandFanLX::KindFanLX::ForgetConversations:
        repository.forgetConversations(command.keepKeys);
        return;
    case DbCommandFanLX::KindFanLX::Custom: {
        TaskFanLX task;
        {
            std::lock_guard<std::mutex> guard(mutexFanLX);
            const auto found = tasksFanLX.find(command.customName);
            if (found == tasksFanLX.end())
                throw ChatRepositoryErrorFanLX(ChatRepositoryErrorFanLX::KindFanLX::Storage,
                                              "未注册的数据库任务：" + command.customName);
            task = found->second;
        }
        const auto result = task(*this, command);
        event.value = result.value;
        event.counter = result.counter;
        event.history = result.history;
        event.conversations = result.conversations;
        return;
    }
    }
}

void DatabaseWorkerFanLX::runLoopFanLX() noexcept {
    runningFanLX = true;
    for (;;) {
        DbCommandFanLX command;
        {
            std::unique_lock<std::mutex> guard(mutexFanLX);
            readyFanLX.wait(guard, [this] { return stopFanLX.load() || !queueFanLX.empty(); });
            if (queueFanLX.empty()) {
                idleFanLX.notify_all();
                if (stopFanLX.load())
                    break;
                continue;
            }
            command = std::move(queueFanLX.front());
            queueFanLX.pop_front();
        }

        DbEventFanLX event;
        event.commandId = command.commandId;
        event.queryId = command.queryId;
        event.accountSessionId = command.accountSessionId;
        event.customName = command.customName;
        event.requestId = command.requestId;
        try {
            // 最外层捕获全部异常：禁止异常逃逸导致 terminate。
            processFanLX(command, event);
            event.kind = DbEventFanLX::KindFanLX::Result;
            ++processedFanLX;
        } catch (const std::exception &error) {
            event.kind = DbEventFanLX::KindFanLX::Failure;
            event.failureReason = error.what();
            ++failuresFanLX;
        } catch (...) {
            event.kind = DbEventFanLX::KindFanLX::Failure;
            event.failureReason = "数据库线程未知异常";
            ++failuresFanLX;
        }
        // 事件投递失败（消费者抛异常）只影响提示，不影响已提交的数据。
        try {
            publish(event);
        } catch (...) {
            ++failuresFanLX;
        }
        {
            std::lock_guard<std::mutex> guard(mutexFanLX);
            completedFanLX[command.commandId] = true;
            if (completedFanLX.size() > 512)
                completedFanLX.erase(completedFanLX.begin());
        }
        idleFanLX.notify_all();
    }
    runningFanLX = false;
}

} // namespace chatDbFanLX

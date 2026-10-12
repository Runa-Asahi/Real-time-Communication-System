#include "network/MessageMailboxFanLX.h"
#include "network/MessageCodecFanLX.h"
#include <chrono>
#include <thread>

MessageMailboxFanLX::MessageMailboxFanLX(SinkFanLX &sink, std::size_t frameCapacity,
                                         std::size_t byteCapacity)
    : sinkFanLX(sink), frameCapacityFanLX(frameCapacity ? frameCapacity : 1),
      byteCapacityFanLX(byteCapacity ? byteCapacity : MessageCodecFanLX::maxPayload + 4) {}

bool MessageMailboxFanLX::enqueue(const ChatMessageFanLX &message) {
    if (closedFanLX)
        return false;
    // 先组帧再判断容量：帧长度决定是否还有空间，避免"入队成功但永远发不出去"。
    auto bytes = MessageCodecFanLX::encode(message);
    if (framesFanLX.size() >= frameCapacityFanLX || bytesFanLX + bytes.size() > byteCapacityFanLX) {
        ++rejectedFanLX;
        return false;
    }
    bytesFanLX += bytes.size();
    framesFanLX.push_back(FrameFanLX{std::move(bytes), 0});
    return true;
}

int MessageMailboxFanLX::advanceFanLX(FrameFanLX &frame) {
    const auto remaining = frame.bytes.size() - frame.offset;
    if (remaining == 0)
        return 1;
    const int written = sinkFanLX.writeBytes(frame.bytes.data() + frame.offset,
                                             static_cast<int>(remaining));
    if (written < 0) {
        failureFanLX = sinkFanLX.lastError();
        // 半帧已写出时连接不可再用：否则下一帧会被对端解释成上一帧的尾部。
        closedFanLX = true;
        return -1;
    }
    if (written == 0)
        return 0;
    frame.offset += static_cast<std::size_t>(written);
    if (frame.offset >= frame.bytes.size()) {
        midFrameFanLX = false;
        return written;
    }
    midFrameFanLX = true;
    return written;
}

bool MessageMailboxFanLX::flush(unsigned budget) {
    if (closedFanLX)
        return false;
    for (unsigned step = 0; step < budget && !framesFanLX.empty(); ++step) {
        auto &frame = framesFanLX.front();
        const int written = advanceFanLX(frame);
        if (written < 0)
            return false;
        if (written == 0)
            return true; // 当前不可写：保留剩余部分，下次继续。
        if (frame.offset >= frame.bytes.size()) {
            bytesFanLX -= frame.bytes.size();
            framesFanLX.pop_front();
        }
    }
    return true;
}

bool MessageMailboxFanLX::flushBlocking(unsigned budget) {
    // 有界等待：单个连接最多占用 timeoutMsFanLX 量级的推进时间，不无限阻塞。
    constexpr int roundsPerSecond = 20; // 每轮 50 ms，与既有 socketIoFanLX::ready 的等待粒度一致
    constexpr int timeoutRounds = 5 * roundsPerSecond;
    for (int round = 0; round < timeoutRounds; ++round) {
        if (!flush(budget))
            return false;
        if (framesFanLX.empty())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / roundsPerSecond));
    }
    failureFanLX = "发送超时：待发送缓冲未能在有界时间内排空";
    return false;
}

void MessageMailboxFanLX::close() noexcept {
    closedFanLX = true;
    framesFanLX.clear();
    bytesFanLX = 0;
    midFrameFanLX = false;
}

void MessageMailboxFanLX::clear() noexcept {
    framesFanLX.clear();
    bytesFanLX = 0;
    midFrameFanLX = false;
}

#pragma once
#include "network/ChatMessageFanLX.h"
#include <cstddef>
#include <deque>
#include <string>

// 每连接有界待发送缓冲：把"组帧 + 推进发送"从"等待单个客户端"里拆出来。
// 服务端主循环据此不再为慢客户端阻塞（原实现会在 send 内等待最长 5 秒）。
//
// 不变式：
// - 每帧记录自己的已写偏移；半帧失败后连接必须关闭，禁止把下一帧接在半帧之后；
// - 队列满或累计字节超限即判定为慢端，由调用方关闭连接，不无限堆积内存；
// - flush 只在真正写出字节时推进偏移，不可写（WSAEWOULDBLOCK）视为"稍后再试"；
// - 本类不持锁：只允许连接所属线程调用（服务端控制线程 / 客户端控制线程）。
class MessageMailboxFanLX {
  public:
    // 字节写出口。返回已写字节数，0 表示当前不可写（应稍后重试），-1 表示不可恢复失败。
    class SinkFanLX {
      public:
        virtual ~SinkFanLX() = default;
        virtual int writeBytes(const char *data, int size) noexcept = 0;
        virtual std::string lastError() const = 0;
    };

    explicit MessageMailboxFanLX(SinkFanLX &sink, std::size_t frameCapacity = 128,
                                 std::size_t byteCapacity = 1u << 20);

    // 组帧并入队。返回 false 表示已停止或队列已满（慢端）。
    bool enqueue(const ChatMessageFanLX &message);
    // 非阻塞推进：最多处理 budget 帧。返回 false 表示发送失败（调用方应关闭连接）。
    bool flush(unsigned budget = 16);
    // 有界阻塞推进：用于保持既有 TcpPeerFanLX::send 的同步语义。失败或超时返回 false。
    bool flushBlocking(unsigned budget = 16);
    // 半帧是否已经写出（据此决定"失败后必须关闭连接"）。
    bool midFrame() const noexcept { return midFrameFanLX; }

    void close() noexcept;
    void clear() noexcept;
    bool empty() const noexcept { return framesFanLX.empty(); }
    std::size_t frameCount() const noexcept { return framesFanLX.size(); }
    std::size_t byteCount() const noexcept { return bytesFanLX; }
    std::size_t frameCapacity() const noexcept { return frameCapacityFanLX; }
    // 累计因队列满而拒绝的次数，供验收统计慢端行为。
    std::size_t rejectedCount() const noexcept { return rejectedFanLX; }
    std::string failure() const { return failureFanLX; }

  private:
    struct FrameFanLX {
        std::string bytes;
        std::size_t offset = 0;
    };
    // 推进队首一帧：0=稍后重试，>0=已写字节数，<0=失败。
    int advanceFanLX(FrameFanLX &frame);

    SinkFanLX &sinkFanLX;
    std::deque<FrameFanLX> framesFanLX;
    std::size_t frameCapacityFanLX;
    std::size_t byteCapacityFanLX;
    std::size_t bytesFanLX = 0;
    std::size_t rejectedFanLX = 0;
    bool closedFanLX = false;
    bool midFrameFanLX = false;
    std::string failureFanLX;
};

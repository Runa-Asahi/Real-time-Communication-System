#pragma once
#include "network/ChatMessageFanLX.h"
#include <array>
#include <string>

// 4 字节大端长度 + UTF-8 JSON。先验证长度，再为载荷分配空间。
class MessageCodecFanLX {
  public:
    static constexpr std::size_t maxPayload = 16384;
    // 单条消息正文上限。单位是**字节**不是字符数：中文一个字约占 3 字节，即约 1365 个汉字。
    static constexpr std::size_t maxText = 4096;
    static std::string encode(const ChatMessageFanLX &message);
    static ChatMessageFanLX decode(const std::string &payload);
    static std::size_t payloadSize(const std::array<unsigned char, 4> &header);

    // 发送前正文预检：返回空串表示可以发送，否则返回可直接展示给用户的失败原因。
    //
    // 为什么需要它：真正严格的校验在 encode()，但消息在那之前就已经补写入 outbox 并持久化。
    // 一条永远无法组帧的消息（超长 / 含控制字符 / 非法 UTF-8）会变成"毒消息"——
    // 客户端每次重连都用 replayOutboxFanLX 重放它、每次都抛异常、异常被 catch 后再次重连，
    // 于是永远无法上线。所以必须在**写库之前**用同一套规则拦下。
    static std::string textIssueFanLX(const std::string &text);
};

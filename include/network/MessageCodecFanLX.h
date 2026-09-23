#pragma once
#include "network/ChatMessageFanLX.h"
#include <array>
#include <string>

// 4 字节大端长度 + UTF-8 JSON。先验证长度，再为载荷分配空间。
class MessageCodecFanLX {
  public:
    static constexpr std::size_t maxPayload = 16384;
    static std::string encode(const ChatMessageFanLX &message);
    static ChatMessageFanLX decode(const std::string &payload);
    static std::size_t payloadSize(const std::array<unsigned char, 4> &header);
};

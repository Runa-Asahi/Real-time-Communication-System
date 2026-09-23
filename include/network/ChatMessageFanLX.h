#pragma once
#include <cstdint>
#include <string>

// 只携带值数据，网络线程不持有会话、群对象或持久化状态指针。
struct ChatMessageFanLX {
    std::string sender;
    std::string recipient;
    std::uint64_t sequence = 0;
    std::string text;
    // 空 type 保持 v1 测试兼容；实际 IM 入口只使用 v2。
    std::string type = "";
    std::string requestId = "";
    std::string scope = "direct";
    std::uint64_t messageId = 0;
    std::uint64_t conversationSeq = 0;
};

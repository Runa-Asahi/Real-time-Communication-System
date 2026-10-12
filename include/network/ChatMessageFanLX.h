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
    // 本条帧所属的服务（QQ / 微信）：一条连接固定属于一个服务，因此好友、群、在线状态
    // 全部只在该服务的分区里判定与投递——三种服务不共享聊天数据。默认 QQ 兼容旧报文。
    std::string service = "QQ";
    std::uint64_t messageId = 0;
    std::uint64_t conversationSeq = 0;
    // 协议扩展（v3，可选字段，缺省不参与编码）：
    // - protocolVersion：客户端声明可理解的协议版本；0 表示未声明（旧客户端），服务端按 v2 处理。
    // - acceptedAtUtc：服务器时间戳（SEND_ACK / DELIVER 携带）。旧实现不含时间字段，
    //   因此该字段只在服务端确实提供可信时间时填充，绝不本地伪造。
    std::uint32_t protocolVersion = 0;
    std::string acceptedAtUtc = "";
};

// 协议版本常量：v2 = 既有帧协议；v3 = 增加 acceptedAtUtc 与授权目录请求。
inline constexpr std::uint32_t imProtocolVersionV2FanLX = 2;
inline constexpr std::uint32_t imProtocolVersionV3FanLX = 3;
// 客户端可接受的最高协议版本。
inline constexpr std::uint32_t imProtocolVersionCurrentFanLX = imProtocolVersionV3FanLX;

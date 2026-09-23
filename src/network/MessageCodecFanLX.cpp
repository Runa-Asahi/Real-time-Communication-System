#include "network/MessageCodecFanLX.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_set>

namespace {
void validateFanLX(const ChatMessageFanLX &message) {
    if (!message.type.empty()) {
        static const std::unordered_set<std::string> types{
            "LOGIN", "LOGIN_OK", "SEND", "SEND_ACK", "DELIVER", "ACK", "SYNC",
            "SYNC_END", "QUERY", "PRESENCE", "PING", "PONG", "ERROR"};
        if (!types.count(message.type) || (message.scope != "direct" && message.scope != "group"))
            throw std::runtime_error("不支持的 IM 类型/会话");
        for (const auto *value : {&message.sender, &message.recipient, &message.requestId}) {
            if (value->size() > 128) throw std::runtime_error("IM 标识过长");
            for (unsigned char ch : *value)
                if (ch < 32 || ch == 127 || ch == '|') throw std::runtime_error("IM 标识非法");
        }
        if (message.text.size() > 4096) throw std::runtime_error("IM 文本过长");
        for (unsigned char ch : message.text)
            if ((ch < 32 && ch != '\n' && ch != '\t') || ch == 127)
                throw std::runtime_error("IM 文本含控制字符");
        if (message.type == "SEND" && (message.requestId.empty() || message.recipient.empty() || message.text.empty()))
            throw std::runtime_error("SEND 缺少消息标识/目标/内容");
        return;
    }
    for (const auto *id : {&message.sender, &message.recipient}) {
        if (id->empty() || id->size() > 128)
            throw std::runtime_error("聊天账号长度非法");
        for (unsigned char ch : *id)
            if (ch < 32 || ch == 127 || ch == '|')
                throw std::runtime_error("聊天账号含非法字符");
    }
    if (!message.sequence || message.text.empty() || message.text.size() > 4096)
        throw std::runtime_error("聊天序号或文本长度非法");
    // 终端控制码不可直接输出；换行和制表符允许用于中文多行消息。
    for (unsigned char ch : message.text)
        if ((ch < 32 && ch != '\n' && ch != '\t') || ch == 127)
            throw std::runtime_error("聊天文本含终端控制字符");
}
} // namespace
std::string MessageCodecFanLX::encode(const ChatMessageFanLX &message) {
    validateFanLX(message);
    auto value = nlohmann::json{
        {"version", 1},
        {"service", "QQ"},
        {"sender", message.sender},
        {"recipient", message.recipient},
        {"sequence", message.sequence},
        {"text",
         message.text}};
    if (!message.type.empty()) {
        value["version"] = 2;
        value["type"] = message.type;
        value["requestId"] = message.requestId;
        value["scope"] = message.scope;
        value["messageId"] = message.messageId;
        value["conversationSeq"] = message.conversationSeq;
    }
    const auto payload = value.dump(); // dump 严格检查 UTF-8。
    if (payload.size() > maxPayload)
        throw std::runtime_error("消息帧过大");
    const auto size = static_cast<std::uint32_t>(payload.size());
    std::string frame(4, '\0');
    for (int i = 0; i < 4; ++i)
        frame[i] = static_cast<char>((size >> (24 - i * 8)) & 255);
    return frame + payload;
}
ChatMessageFanLX MessageCodecFanLX::decode(const std::string &payload) {
    if (payload.empty() || payload.size() > maxPayload)
        throw std::runtime_error("消息帧长度非法");
    // 聊天协议是平坦对象：拒绝嵌套容器和重复属性，避免歧义解析和深度攻击。
    std::unordered_set<std::string> keys;
    const auto value = nlohmann::json::parse(payload, [&](int depth, auto event, auto &item) {
        using EventFanLX = nlohmann::json::parse_event_t;
        if (event == EventFanLX::array_start || (event == EventFanLX::object_start && depth != 0))
            throw std::runtime_error("聊天协议不允许嵌套容器");
        if (event == EventFanLX::key && !keys.insert(item.template get<std::string>()).second)
            throw std::runtime_error("聊天协议属性重复");
        return true;
    });
    const bool im = value.value("version", 0) == 2;
    if (!value.is_object() || value.size() != (im ? 11u : 6u) || !value.at("version").is_number_integer() ||
        (!im && value.at("version") != 1) || value.at("service") != "QQ" || !value.at("sequence").is_number_unsigned())
        throw std::runtime_error("不支持的聊天协议版本/服务/序号");
    ChatMessageFanLX message{value.at("sender").get<std::string>(), value.at("recipient").get<std::string>(),
                             value.at("sequence").get<std::uint64_t>(), value.at("text").get<std::string>()};
    if (im) {
        message.type = value.at("type").get<std::string>();
        message.requestId = value.at("requestId").get<std::string>();
        message.scope = value.at("scope").get<std::string>();
        if (message.type.empty() || !value.at("messageId").is_number_unsigned() ||
            !value.at("conversationSeq").is_number_unsigned()) throw std::runtime_error("IM 序号非法");
        message.messageId = value.at("messageId").get<std::uint64_t>();
        message.conversationSeq = value.at("conversationSeq").get<std::uint64_t>();
    }
    validateFanLX(message);
    return message;
}
std::size_t MessageCodecFanLX::payloadSize(const std::array<unsigned char, 4> &header) {
    std::uint32_t size = 0;
    for (auto byte : header)
        size = (size << 8) | byte;
    if (!size || size > maxPayload)
        throw std::runtime_error("消息帧长度非法");
    return size;
}

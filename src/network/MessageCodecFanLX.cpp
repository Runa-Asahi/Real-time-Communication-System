#include "network/MessageCodecFanLX.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_set>

namespace {
// 聊天通道支持的服务：QQ / 微信 / 微博 三种下级服务都能单聊（微博没有群）。
// 微X 是上层登录身份，不承载聊天，因此不作为通道服务。
bool chatServiceNameFanLX(const std::string &name) {
    return name == "QQ" || name == "Wechat" || name == "Weibo";
}
void validateFanLX(const ChatMessageFanLX &message) {
    if (!message.type.empty()) {
        static const std::unordered_set<std::string> types{
            "LOGIN", "LOGIN_OK", "SEND", "SEND_ACK", "DELIVER", "ACK", "SYNC",
            "SYNC_END", "QUERY", "PRESENCE", "PING", "PONG", "ERROR",
            // v3 扩展：受认证保护的只读目录请求与应答。
            "DIRECTORY", "DIRECTORY_LIST",
            // 账号登记：未认证也能发（它就是"还没有账号"时用的那条），服务端按内容决定是否建号。
            "REGISTER", "REGISTER_OK",
            // 账号查询（添加好友前确认对端是否存在）：需要已登录会话，服务端只回"存在/不存在"。
            "LOOKUP", "LOOKUP_RESULT",
            // 好友申请（发起 / 同意 / 拒绝 / 撤回 / 删除 / 列表）：与 LOOKUP 一样是一次性凭据请求，
            // 由服务端中转并持久化；FRIEND_NOTICE 是服务端推给对端在线会话的"好友动态"提示。
            "FRIEND", "FRIEND_RESULT", "FRIEND_NOTICE"};
        if (!types.count(message.type) || (message.scope != "direct" && message.scope != "group"))
            throw std::runtime_error("不支持的 IM 类型/会话");
        // REGISTER 是账号层消息：它不进聊天通道，因此不强求 service 是 QQ/微信/微博。
        if (message.type != "REGISTER" && message.type != "REGISTER_OK" &&
            !chatServiceNameFanLX(message.service))
            throw std::runtime_error("不支持的 IM 服务（聊天通道只有 QQ / 微信 / 微博）");
        for (const auto *value : {&message.sender, &message.recipient, &message.requestId}) {
            if (value->size() > 128) throw std::runtime_error("IM 标识过长");
            for (unsigned char ch : *value)
                if (ch < 32 || ch == 127 || ch == '|') throw std::runtime_error("IM 标识非法");
        }
        if (message.text.size() > MessageCodecFanLX::maxText) throw std::runtime_error("IM 文本过长");
        for (unsigned char ch : message.text)
            if ((ch < 32 && ch != '\n' && ch != '\t') || ch == 127)
                throw std::runtime_error("IM 文本含控制字符");
        if (message.type == "SEND" && (message.requestId.empty() || message.recipient.empty() || message.text.empty()))
            throw std::runtime_error("SEND 缺少消息标识/目标/内容");
        if (!message.acceptedAtUtc.empty()) {
            if (message.acceptedAtUtc.size() > 32)
                throw std::runtime_error("服务器时间戳过长");
            for (unsigned char ch : message.acceptedAtUtc)
                if (ch < 32 || ch == 127) throw std::runtime_error("服务器时间戳含控制字符");
        }
        if (message.protocolVersion > imProtocolVersionCurrentFanLX)
            throw std::runtime_error("协议版本超出支持范围");
        return;
    }
    for (const auto *id : {&message.sender, &message.recipient}) {
        if (id->empty() || id->size() > 128)
            throw std::runtime_error("聊天账号长度非法");
        for (unsigned char ch : *id)
            if (ch < 32 || ch == 127 || ch == '|')
                throw std::runtime_error("聊天账号含非法字符");
    }
    if (!message.sequence || message.text.empty() || message.text.size() > MessageCodecFanLX::maxText)
        throw std::runtime_error("聊天序号或文本长度非法");
    // 终端控制码不可直接输出；换行和制表符允许用于中文多行消息。
    for (unsigned char ch : message.text)
        if ((ch < 32 && ch != '\n' && ch != '\t') || ch == 127)
            throw std::runtime_error("聊天文本含终端控制字符");
}
} // namespace

// 与 validateFanLX 中针对正文的三条规则保持一致；两处若不一致，"预检通过但组帧失败"
// 的毒消息就会重新出现，因此这里的判定刻意与那边逐条对应。
std::string MessageCodecFanLX::textIssueFanLX(const std::string &text) {
    if (text.empty())
        return "消息内容不能为空";
    if (text.size() > maxText)
        return "消息过长：当前 " + std::to_string(text.size()) + " 字节，单条上限 " +
               std::to_string(maxText) + " 字节（中文一个字约 3 字节，约 " +
               std::to_string(maxText / 3) + " 个汉字）";
    for (const unsigned char ch : text)
        if ((ch < 32 && ch != '\n' && ch != '\t') || ch == 127)
            return "消息含不可发送的控制字符（只允许换行与制表符）";
    // dump() 严格校验 UTF-8：非法字节序列（例如从 GBK 控制台粘贴得到的内容）必须在这里拦下，
    // 否则它会在组帧时才抛 type_error.316，同样变成毒消息。
    try {
        (void)nlohmann::json(text).dump();
    } catch (const std::exception &) {
        return "消息不是合法的 UTF-8 文本";
    }
    return {};
}

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
        // 服务名随每一条 IM 帧走：服务端按它在本服务的好友/群分区里鉴权与投递。
        value["service"] = message.service;
        value["requestId"] = message.requestId;
        value["scope"] = message.scope;
        value["messageId"] = message.messageId;
        value["conversationSeq"] = message.conversationSeq;
        // v3 可选字段：只在有值时才出现。旧接收端按字段名解析，不认识的字段被忽略，
        // 因此新增字段不改变既有 v2 报文的字节形态（既有测试与旧客户端都不受影响）。
        if (message.protocolVersion != 0) {
            value["protocolVersion"] = message.protocolVersion;
            value["version"] = 3;
        }
        if (!message.acceptedAtUtc.empty()) {
            value["acceptedAtUtc"] = message.acceptedAtUtc;
            value["version"] = 3;
        }
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
    const int version = value.is_object() ? value.value("version", 0) : 0;
    // v2 = 11 个字段；v3 = 在 v2 上增加 protocolVersion / acceptedAtUtc 两个可选字段。
    const bool im = version == 2 || version == 3;
    const bool chatService = value.is_object() && value.contains("service") &&
                             value.at("service").is_string() &&
                             chatServiceNameFanLX(value.at("service").get<std::string>());
    if (!value.is_object() || !value.at("version").is_number_integer() ||
        (!im && version != 1) || !chatService || !value.at("sequence").is_number_unsigned())
        throw std::runtime_error("不支持的聊天协议版本/服务/序号");
    if (im && value.size() != 11u && value.size() != 12u && value.size() != 13u)
        throw std::runtime_error("不支持的聊天协议版本/服务/序号");
    if (!im && value.size() != 6u)
        throw std::runtime_error("不支持的聊天协议版本/服务/序号");
    ChatMessageFanLX message{value.at("sender").get<std::string>(), value.at("recipient").get<std::string>(),
                             value.at("sequence").get<std::uint64_t>(), value.at("text").get<std::string>()};
    message.service = value.at("service").get<std::string>();
    if (im) {
        message.type = value.at("type").get<std::string>();
        message.requestId = value.at("requestId").get<std::string>();
        message.scope = value.at("scope").get<std::string>();
        if (message.type.empty() || !value.at("messageId").is_number_unsigned() ||
            !value.at("conversationSeq").is_number_unsigned()) throw std::runtime_error("IM 序号非法");
        message.messageId = value.at("messageId").get<std::uint64_t>();
        message.conversationSeq = value.at("conversationSeq").get<std::uint64_t>();
        if (value.contains("protocolVersion")) {
            if (!value.at("protocolVersion").is_number_unsigned())
                throw std::runtime_error("协议版本非法");
            message.protocolVersion = value.at("protocolVersion").get<std::uint32_t>();
        }
        if (value.contains("acceptedAtUtc")) {
            if (!value.at("acceptedAtUtc").is_string())
                throw std::runtime_error("服务器时间戳非法");
            message.acceptedAtUtc = value.at("acceptedAtUtc").get<std::string>();
        }
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

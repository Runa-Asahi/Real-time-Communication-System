#pragma once
#include <string>
#include <vector>

// TemporaryConversationFanLX：非好友临时会话的**单方向**额度记录。
// 对应课设新增要求：非好友之间允许一定聊天数目限额的临时对话。
//
// 规则（由服务端 TemporaryChatQuotaFanLX 执行）：
//   - 计数按方向分开：A→B 用满 10 条不影响 B→A 回复的 10 条；
//   - 用满后服务端拒收，提示先发好友申请；
//   - 成为好友后整对记录清除，之后不再受限；
//   - consumedRequestIds 记录已计数的消息 requestId，客户端断线重发同一条消息不会重复扣额度。
struct TemporaryConversationFanLX {
    std::string service; // 通道服务名（QQ / Wechat / Weibo）
    std::string from;    // 发送方号码
    std::string to;      // 接收方号码
    int sent = 0;        // 已发送条数
    std::vector<std::string> consumedRequestIds;
    std::string lastMessageAtUtc;

    int remaining(int limit) const { return sent >= limit ? 0 : limit - sent; }
};

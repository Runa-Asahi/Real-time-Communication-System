#pragma once
#include <string>

// FriendRequestStatusFanLX：好友申请的处理状态。
enum class FriendRequestStatusFanLX {
    Pending,  // 待处理：已发送，等待对方回应
    Accepted, // 已同意：好友关系已建立
    Rejected, // 已拒绝
    Cancelled // 已撤回：发起方在对方回应前撤回
};

// 状态 <-> 存储字符串（friend-requests.json 中使用，禁止随意改动）。
inline std::string friendRequestStatusNameFanLX(FriendRequestStatusFanLX status) {
    switch (status) {
    case FriendRequestStatusFanLX::Accepted:
        return "accepted";
    case FriendRequestStatusFanLX::Rejected:
        return "rejected";
    case FriendRequestStatusFanLX::Cancelled:
        return "cancelled";
    default:
        return "pending";
    }
}
inline FriendRequestStatusFanLX friendRequestStatusFromNameFanLX(const std::string &name) {
    if (name == "accepted")
        return FriendRequestStatusFanLX::Accepted;
    if (name == "rejected")
        return FriendRequestStatusFanLX::Rejected;
    if (name == "cancelled")
        return FriendRequestStatusFanLX::Cancelled;
    return FriendRequestStatusFanLX::Pending;
}

// FriendRequestFanLX：一条好友申请记录。
// 对应课设新增要求：添加好友需要一方发出申请，对方同意后才能成为好友。
//
// 申请只在**同一个服务**里成立（QQ 号向 QQ 号申请）；跨服务"依据另一个服务的好友添加"
// 由客户端换算成本服务号码后，同样走这条申请流程。
// 同一 (服务, 发起方, 接收方) 只保留最新一条记录：重复申请会刷新附言与时间，不会堆积。
struct FriendRequestFanLX {
    std::string service;      // 通道服务名（QQ / Wechat / Weibo）
    std::string from;         // 发起方在该服务的号码
    std::string to;           // 接收方在该服务的号码
    std::string note;         // 申请附言（如"我是你的同学张三"）
    FriendRequestStatusFanLX status = FriendRequestStatusFanLX::Pending;
    std::string requestedAtUtc; // 申请时间（服务器 UTC）
    std::string respondedAtUtc; // 回应时间（未回应为空）

    bool isPending() const { return status == FriendRequestStatusFanLX::Pending; }
};

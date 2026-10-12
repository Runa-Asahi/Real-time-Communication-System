#pragma once
#include <stdexcept>
#include <string>

// ServiceTypeFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
//
// 2026-10 改造（微X 上层）：新增 WeiX 作为 QQ / 微信 / 微博 三种服务的**上层账号**。
// 一个真人在本系统里只有一个 WeiX 账号（微X号 + 密码），它是唯一的登录身份；
// QQ / 微信 / 微博 是登微X之后在内部逐个"开通"出来的下级服务账号。
// 因此 WeiX 是登录入口，QQ/Wechat/Weibo 是业务服务。
enum class ServiceTypeFanLX {
    QQ,     // 腾讯 QQ（微X 下级服务，有群，可聊天）
    Wechat, // 微信（微X 下级服务，有群，可聊天）
    Weibo,  // 新浪微博（微X 下级服务：可单聊、有好友，但没有群）
    WeiX    // 微X 本身：唯一登录身份，是所有下级服务的上层
};

// 存储键 / JSON 中的服务名（禁止改动既有字符串，它们是数据格式的一部分）。
inline std::string serviceNameFanLX(ServiceTypeFanLX type) {
    switch (type) {
    case ServiceTypeFanLX::QQ:
        return "QQ";
    case ServiceTypeFanLX::Wechat:
        return "Wechat";
    case ServiceTypeFanLX::Weibo:
        return "Weibo";
    default:
        return "WeiX";
    }
}

// 界面显示名：控制台与 Qt 界面统一用中文，避免 "Wechat" 这种存储名出现在用户面前。
inline std::string serviceDisplayNameFanLX(ServiceTypeFanLX type) {
    switch (type) {
    case ServiceTypeFanLX::QQ:
        return "QQ";
    case ServiceTypeFanLX::Wechat:
        return "微信";
    case ServiceTypeFanLX::Weibo:
        return "微博";
    default:
        return "微X";
    }
}

// 解析：服务名（存储名或界面中文名）→ 枚举。无法识别时返回 false，不抛异常。
inline bool serviceFromNameFanLX(const std::string &name, ServiceTypeFanLX &out) {
    if (name == "QQ") { out = ServiceTypeFanLX::QQ; return true; }
    if (name == "Wechat" || name == "微信") { out = ServiceTypeFanLX::Wechat; return true; }
    if (name == "Weibo" || name == "微博") { out = ServiceTypeFanLX::Weibo; return true; }
    if (name == "WeiX" || name == "微X") { out = ServiceTypeFanLX::WeiX; return true; }
    return false;
}

// 是否为微X的下级服务（QQ / 微信 / 微博）——只有它们需要"开通"。
inline bool isSubordinateServiceFanLX(ServiceTypeFanLX type) { return type != ServiceTypeFanLX::WeiX; }

// 是否提供聊天通道：QQ / 微信 / 微博 三种下级服务都可以单聊（资料、好友、私聊全部可用）。
// 微X 是上层登录身份，本身不承载聊天。
inline bool chatCapableFanLX(ServiceTypeFanLX type) { return isSubordinateServiceFanLX(type); }

// 是否提供**群**：只有 QQ 与微信（微博除群以外的功能都可用：资料 / 开通 / 好友 / 单聊）。
inline bool groupCapableFanLX(ServiceTypeFanLX type) {
    return type == ServiceTypeFanLX::QQ || type == ServiceTypeFanLX::Wechat;
}

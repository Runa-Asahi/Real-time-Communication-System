#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>

// 客户端数据目录派生（全项目唯一约定）。
//
// 设计依据：客户端存储目录不是用户输入，而是由"服务器身份 + 服务 + 账号"派生。
// 顺序必须是 身份 → 派生路径，而不是 先选路径 → 事后校验归属。
//
//   <数据根>/im-client/<host>_<port>/<账号目录名>/
//       ├─ domain/                客户端领域库（按微X号派生，用户资料/开通/好友/群）
//       └─ <服务名>/              聊天存储按**服务**再分一层（QQ / Wechat / Weibo）
//            ├─ client-identity.json   身份与归属凭据（第二道防线，保留校验）
//            ├─ chat.db                --im-sqlite 时的聊天库
//            └─ state.json             旧 JSON 后端
//
// 为什么目录名里要带服务器身份：accountId 只在单个服务端内唯一
// （见 AccountManagerFanLX::addAccount 的唯一性约束），
// 只按账号派生会让不同服务器上的同名账号共用目录而互相污染。
//
// 为什么聊天目录还要带服务名：QQ 与微博按课程约定共用同一个号码，两个聊天通道的账号
// 字符串完全相同；不带服务名会让"同一个人的 QQ 会话"与"微博会话"落进同一个库、
// 争用同一把目录锁。服务名这一层让三条通道各自一份 chat.db、各自一把锁。
//
// 数据根是唯一允许测试/自动化覆盖的部分；账号子目录始终由程序派生，
// 用户界面不再提供"选择数据目录"入口。
namespace imClientPathFanLX {

// 目录名转义：只保留 ASCII 字母数字与 - _ . ，其余字节按 %XX 转义。
// 按字节处理，不受 locale 影响；转义保证一对一，不会把两个账号映射到同一目录。
inline std::string escapeDirectoryNameFanLX(const std::string &raw) {
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(raw.size());
    for (const unsigned char ch : raw) {
        const bool safe = (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
                          (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_' || ch == '.';
        if (safe) {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('%');
            out.push_back(hex[ch >> 4]);
            out.push_back(hex[ch & 15]);
        }
    }
    // "." 与 ".." 即使全部是安全字符也绝不能直接当目录名，否则会跳出数据根。
    if (out == "." || out == "..")
        out = "%2E" + out.substr(1);
    return out;
}

// 服务器标识目录名：<host>_<port>，例如 127.0.0.1_9500。
inline std::string serverDirectoryNameFanLX(const std::string &host, unsigned short port) {
    std::string safe = host;
    for (char &ch : safe)
        if (ch == ':') // IPv6 字面量防御：冒号在 Windows 路径里非法
            ch = '_';
    return escapeDirectoryNameFanLX(safe) + "_" + std::to_string(port);
}

inline constexpr const char *clientRootNameFanLX = "im-client";

// 领域库子目录名。必须与 IM 客户端目录**分开**：两者各自用 DataDirectoryLockFanLX
// 独占自己那层的 .lock（CreateFileW 共享模式为 0），同目录会互相拒绝启动。
// 放在账号目录下一层既能共享"服务器身份 + 账号"的隔离，又不会争用同一把锁。
inline constexpr const char *domainRootNameFanLX = "domain";

// 派生客户端存储目录。root 为空、端口为 0 或账号为空都是编程错误，直接拒绝而非静默生成默认路径。
inline std::filesystem::path clientDirectoryFanLX(const std::filesystem::path &root,
                                                  const std::string &host, unsigned short port,
                                                  const std::string &accountId) {
    if (root.empty() || host.empty() || port == 0 || accountId.empty())
        throw std::invalid_argument("客户端数据目录需要数据根、服务器地址、端口与账号");
    return root / clientRootNameFanLX / serverDirectoryNameFanLX(host, port) /
           escapeDirectoryNameFanLX(accountId);
}

// 派生客户端领域库目录：<客户端目录>/domain。GUI 的账号/好友/群页面读写这里，
// 与聊天库（chat.db）互不干扰，因此可以同时打开。
inline std::filesystem::path domainDirectoryFanLX(const std::filesystem::path &clientDirectory) {
    if (clientDirectory.empty())
        throw std::invalid_argument("领域库目录需要客户端存储目录");
    return clientDirectory / domainRootNameFanLX;
}

// 派生**聊天**存储目录：<客户端目录>/<服务名>。
// 一个服务一份聊天库与一把目录锁：QQ / 微信 / 微博 互不共享消息与历史。
// service 用存储名（QQ / Wechat / Weibo），与网络协议里的 service 字段同一套取值。
inline std::filesystem::path chatDirectoryFanLX(const std::filesystem::path &root,
                                               const std::string &host, unsigned short port,
                                               const std::string &service,
                                               const std::string &accountId) {
    if (service.empty())
        throw std::invalid_argument("聊天存储目录需要服务名（QQ / Wechat / Weibo）");
    return clientDirectoryFanLX(root, host, port, accountId) / escapeDirectoryNameFanLX(service);
}

} // namespace imClientPathFanLX

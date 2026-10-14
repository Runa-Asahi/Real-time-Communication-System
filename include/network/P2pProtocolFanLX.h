#pragma once
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

// 直连控制帧复用现有平坦 JSON 字段，不新增键，因此字段个数仍落在 v2/v3 的 11/12/13。
// 票据放 requestId，端点或正文哈希放 text。旧客户端不会收到这些类型：
// 只有声明过 P2P_HELLO 的会话才会被服务端选进直连。
namespace p2pFanLX {

inline constexpr const char *helloTypeFanLX = "P2P_HELLO";
inline constexpr const char *offerTypeFanLX = "P2P_OFFER";
inline constexpr const char *expectTypeFanLX = "P2P_EXPECT";
inline constexpr const char *authTypeFanLX = "P2P_AUTH";
inline constexpr const char *authOkTypeFanLX = "P2P_AUTH_OK";
inline constexpr const char *readyTypeFanLX = "P2P_READY";
inline constexpr const char *failTypeFanLX = "P2P_FAIL";
inline constexpr const char *armTypeFanLX = "P2P_ARM";
inline constexpr const char *pushTypeFanLX = "P2P_PUSH";
inline constexpr const char *dataTypeFanLX = "P2P_DATA";

inline bool isDirectTypeFanLX(const std::string &type) {
    return type == helloTypeFanLX || type == offerTypeFanLX || type == expectTypeFanLX ||
           type == authTypeFanLX || type == authOkTypeFanLX || type == readyTypeFanLX ||
           type == failTypeFanLX || type == armTypeFanLX || type == pushTypeFanLX ||
           type == dataTypeFanLX;
}

// 正文哈希。服务端用持久化后的正文计算，接收方据此拒绝被篡改的直连正文。
// 这不是密码学承诺，只绑定“服务器已经接受的那一份正文”。
inline std::string hashBodyFanLX(const std::string &body) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : body) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    std::ostringstream out;
    out << "h:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

// 端点文本：ep:<点分 IPv4>:<端口>。拒绝 0.0.0.0、域名和缺端口。
// 调用方不得把监听通配地址或服务器看到的源地址原样拿来宣告。
inline bool parseEndpointFanLX(const std::string &text, std::string &ip, unsigned short &port) {
    constexpr char prefix[] = "ep:";
    if (text.rfind(prefix, 0) != 0)
        return false;
    const auto body = text.substr(3);
    const auto colon = body.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= body.size())
        return false;
    const auto portText = body.substr(colon + 1);
    if (portText.size() > 5)
        return false;
    int portValue = 0;
    for (char ch : portText) {
        if (ch < '0' || ch > '9')
            return false;
        portValue = portValue * 10 + (ch - '0');
    }
    if (portValue < 1 || portValue > 65535)
        return false;
    int octets[4] = {};
    int index = 0;
    int current = -1;
    int digits = 0;
    const auto host = body.substr(0, colon);
    for (std::size_t i = 0; i <= host.size(); ++i) {
        if (i == host.size() || host[i] == '.') {
            if (digits == 0 || index > 3)
                return false;
            octets[index++] = current;
            current = -1;
            digits = 0;
            continue;
        }
        if (host[i] < '0' || host[i] > '9')
            return false;
        if (current < 0)
            current = 0;
        current = current * 10 + (host[i] - '0');
        ++digits;
        if (current > 255 || digits > 3)
            return false;
    }
    if (index != 4)
        return false;
    if (octets[0] == 0 && octets[1] == 0 && octets[2] == 0 && octets[3] == 0)
        return false;
    ip = std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
         std::to_string(octets[2]) + "." + std::to_string(octets[3]);
    port = static_cast<unsigned short>(portValue);
    return true;
}

inline std::string formatEndpointFanLX(const std::string &ip, unsigned short port) {
    return "ep:" + ip + ":" + std::to_string(port);
}

} // namespace p2pFanLX

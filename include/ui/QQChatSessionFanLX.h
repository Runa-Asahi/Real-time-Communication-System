#pragma once
#include "network/TcpPeerFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"

// v1 兼容回归门面；生产菜单改用 ImClientFanLX + 中心服务端，不再创建点对点会话。
// 只能在主线程调用。传输层解析成功不代表业务授权成功。
class QQChatSessionFanLX {
    PersistenceApplicationFanLX &appFanLX;
    AccountRefFanLX actorFanLX;
    AccountRefFanLX remoteFanLX;
    std::unique_ptr<TcpPeerFanLX> peerFanLX;
    std::uint64_t sentFanLX = 0;
    std::uint64_t receivedFanLX = 0;

  public:
    QQChatSessionFanLX(PersistenceApplicationFanLX &app, AccountRefFanLX actor, AccountRefFanLX remote,
                       std::unique_ptr<TcpPeerFanLX> peer);
    static void authorize(PersistenceApplicationFanLX &app, const AccountRefFanLX &actor,
                          const AccountRefFanLX &remote);
    void send(const std::string &text);
    std::optional<ChatMessageFanLX> poll();
    bool ended() const noexcept { return peerFanLX->ended(); }
    std::string error() const { return peerFanLX->error(); }
};

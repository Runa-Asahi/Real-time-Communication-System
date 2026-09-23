#include "ui/QQChatSessionFanLX.h"
#include <algorithm>
#include <limits>

void QQChatSessionFanLX::authorize(PersistenceApplicationFanLX &app, const AccountRefFanLX &actor,
                                   const AccountRefFanLX &remote) {
    if (actor.service != ServiceTypeFanLX::QQ || remote.service != ServiceTypeFanLX::QQ)
        throw std::runtime_error("TCP 聊天仅支持 QQ");
    if (actor.accountId == remote.accountId)
        throw std::runtime_error("不能连接本人账号");
    // queryFriendAccounts 内部先检查可用状态与会话，才允许读取缓存。
    const auto friends = app.queryFriendAccounts(actor);
    if (!app.state().accounts.isAccountSubscribed(remote.service, remote.accountId) ||
        std::find(friends.begin(), friends.end(), remote.accountId) == friends.end())
        throw std::runtime_error("请选择已开通 QQ 的好友");
}
QQChatSessionFanLX::QQChatSessionFanLX(PersistenceApplicationFanLX &app, AccountRefFanLX actor,
                                       AccountRefFanLX remote, std::unique_ptr<TcpPeerFanLX> peer)
    : appFanLX(app), actorFanLX(std::move(actor)), remoteFanLX(std::move(remote)),
      peerFanLX(std::move(peer)) {
    if (!peerFanLX)
        throw std::invalid_argument("聊天连接不能为空");
    authorize(appFanLX, actorFanLX, remoteFanLX);
}
void QQChatSessionFanLX::send(const std::string &text) {
    try {
        authorize(appFanLX, actorFanLX, remoteFanLX);
        if (sentFanLX == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("发送序号耗尽，请重新连接");
        peerFanLX->send({actorFanLX.accountId, remoteFanLX.accountId, sentFanLX + 1, text});
        ++sentFanLX;
    } catch (...) {
        peerFanLX->close();
        throw;
    }
}
std::optional<ChatMessageFanLX> QQChatSessionFanLX::poll() {
    try {
        authorize(appFanLX, actorFanLX, remoteFanLX);
        auto message = peerFanLX->poll();
        if (message) {
            if (receivedFanLX == std::numeric_limits<std::uint64_t>::max() ||
                message->sender != remoteFanLX.accountId || message->recipient != actorFanLX.accountId ||
                message->sequence != receivedFanLX + 1)
                throw std::runtime_error("收到账号不符或乱序/重复的消息，连接已关闭");
            ++receivedFanLX;
        }
        return message;
    } catch (...) {
        peerFanLX->close();
        throw;
    }
}

#include "network/PeerAccountResolutionFanLX.h"
#include "network/ImClientFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace peerAccountFanLX {

ResolutionFanLX resolvePeerAccountFanLX(PersistenceApplicationFanLX &app, const std::string &host,
                                        unsigned short port, ServiceTypeFanLX service,
                                        const std::string &selfAccount, const std::string &password,
                                        const std::string &peerAccount, std::string &reason) {
    reason.clear();
    if (peerAccount.empty())
        return ResolutionFanLX::Missing;
    const auto &manager = app.state().accounts;
    // 本机已经认识并且已确认（登录态）→ 直接可用，不必打扰服务端。
    if (manager.hasAccount(service, peerAccount) &&
        manager.isAccountSubscribed(service, peerAccount) &&
        manager.isAccountLoggedIn(service, peerAccount))
        return ResolutionFanLX::Ready;
    ImClientFanLX::LookupResultFanLX found;
    const auto answered = ImClientFanLX::lookupAccountFanLX(
        host, port, serviceNameFanLX(service), selfAccount, password, peerAccount, found, reason);
    if (!answered)
        return ResolutionFanLX::Unreachable; // reason 已经是可直接显示的原因
    if (!found.exists)
        return ResolutionFanLX::Missing;
    (void)app.importRemotePeerAccount({service, peerAccount}, found.userId, found.nickname);
    reason = "服务端确认该号码存在，已导入本机副本";
    return ResolutionFanLX::Imported;
}

namespace {

bool reconcileFanLX(PersistenceApplicationFanLX &app, const std::string &host, unsigned short port,
                    const AccountRefFanLX &actor, const std::string &password,
                    const std::vector<std::string> &serverFriends, bool complete) {
    std::vector<std::string> local;
    try {
        local = app.queryFriendAccounts(actor);
    } catch (const std::exception &) {
        return false; // 本人该服务未登录：没有可对账的会话，下次登录后再对账
    }
    bool changed = false;
    for (const auto &peer : serverFriends) {
        if (std::find(local.begin(), local.end(), peer) != local.end())
            continue;
        // 对端可能是本机不认识的账号（别人在别的客户端注册的）：先导入最小账号记录。
        if (!app.state().accounts.hasAccount(actor.service, peer)) {
            std::string why;
            resolvePeerAccountFanLX(app, host, port, actor.service, actor.accountId, password, peer, why);
            if (!app.state().accounts.hasAccount(actor.service, peer))
                continue;
        }
        try {
            changed = app.mirrorFriendship(actor, {actor.service, peer}) || changed;
        } catch (const std::exception &) {
        }
    }
    if (!complete)
        return changed; // 服务端清单被裁剪过：只补不删，避免误删本机关系
    for (const auto &peer : local) {
        if (std::find(serverFriends.begin(), serverFriends.end(), peer) != serverFriends.end())
            continue;
        try {
            changed = app.unmirrorFriendship(actor, {actor.service, peer}) || changed;
        } catch (const std::exception &) {
        }
    }
    return changed;
}

std::vector<std::string> requestRowsFanLX(const nlohmann::json &list) {
    std::vector<std::string> rows;
    if (!list.is_array())
        return rows;
    for (const auto &row : list) {
        const auto note = row.value("note", std::string());
        rows.push_back(row.value("peer", std::string()) + " ｜ " + (note.empty() ? "（无附言）" : note) +
                       " ｜ " + row.value("time", std::string()));
    }
    return rows;
}

} // namespace

FriendOperationResultFanLX friendOperationFanLX(PersistenceApplicationFanLX &app, const std::string &host,
                                                unsigned short port, const AccountRefFanLX &actor,
                                                const std::string &password, const std::string &op,
                                                const std::string &peer, const std::string &note,
                                                const std::string &via) {
    FriendOperationResultFanLX out;
    if (host.empty() || port == 0) {
        out.reason = "未配置服务端地址";
        return out;
    }
    std::string json;
    out.ok = ImClientFanLX::friendOperationFanLX(host, port, serviceNameFanLX(actor.service), actor.accountId,
                                                 password, op, peer, note, json, out.reason, via,
                                                 &out.reachable, 3000);
    if (!out.ok)
        return out;
    try {
        const auto parsed = nlohmann::json::parse(json);
        out.message = parsed.value("message", std::string());
        out.peer = parsed.value("peer", peer);
        out.incoming = requestRowsFanLX(parsed.value("incoming", nlohmann::json::array()));
        out.outgoing = requestRowsFanLX(parsed.value("outgoing", nlohmann::json::array()));
        out.limit = parsed.value("limit", 0);
        for (const auto &row : parsed.value("temporary", nlohmann::json::array()))
            out.temporary.push_back(row.value("peer", std::string()) + " ｜ 我已发 " +
                                    std::to_string(row.value("sent", 0)) + " / 对方已发 " +
                                    std::to_string(row.value("received", 0)) + " ｜ 我还能发 " +
                                    std::to_string(row.value("remaining", 0)) + " 条");
        std::vector<std::string> friends;
        for (const auto &item : parsed.value("friends", nlohmann::json::array()))
            if (item.is_string())
                friends.push_back(item.get<std::string>());
        out.localChanged = reconcileFanLX(app, host, port, actor, password, friends,
                                          parsed.value("friendsComplete", false));
    } catch (const std::exception &) {
        // 对账失败不影响服务端已经完成的操作；下次刷新会再对账一次。
    }
    return out;
}

} // namespace peerAccountFanLX

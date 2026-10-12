#include "network/ServerAccountRegistrarFanLX.h"
#include "account/ServiceTypeFanLX.h"
#include "infra/UtcClockFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include <algorithm>

bool ServerAccountRegistrarFanLX::refreshAccounts() const { return appFanLX.reloadIfChangedFromDisk(); }

std::string ServerAccountRegistrarFanLX::registerAccountFanLX(
    const std::string &weixId, const std::string &password, const std::string &nickname,
    const std::string &birthDate, const std::string &location,
    const std::vector<std::pair<std::string, std::string>> &services,
    const std::vector<std::pair<std::string, std::string>> &friends) const {
    if (weixId.empty() || password.empty())
        return "登记账号需要微X号与密码";
    try {
        const auto &accounts = appFanLX.state().accounts;
        std::string user = "u-" + weixId;
        if (accounts.hasAccount(ServiceTypeFanLX::WeiX, weixId)) {
            // 已存在：密码一致按幂等成功处理（客户端"先登记再登录"会重复调用），
            // 不一致则拒绝——但错误文本不区分"账号不存在"与"密码错"，避免枚举账号。
            if (!accounts.verifyPassword(ServiceTypeFanLX::WeiX, weixId, password))
                return "该微X号已被注册（密码与本次登记不一致）";
            user = accounts.accountByAccountId(ServiceTypeFanLX::WeiX, weixId).userId;
        } else {
            appFanLX.registerWeiXUser({user,
                                       nickname.empty() ? weixId : nickname,
                                       birthDate.empty() ? std::string("2000-01-01") : birthDate,
                                       utcNowFanLX().substr(0, 10), location, password},
                                      weixId);
        }
        // 客户端本机已开通的下级服务、以及已经加好的好友关系，都要一并登记：
        // 不这样做，"注册 → 开通 → 加好友 → 发消息"仍会在服务端被拒
        // （服务端按它自己的账号库判定"这个号码存在吗、是不是好友"）。
        std::vector<std::pair<ServiceTypeFanLX, std::string>> pending;
        for (const auto &entry : services) {
            ServiceTypeFanLX service{};
            if (!serviceFromNameFanLX(entry.first, service) || !isSubordinateServiceFanLX(service) ||
                entry.second.empty())
                continue;
            if (accounts.hasAccount(service, entry.second))
                continue; // 该号码已被占用（含本人已登记）：保留服务端现状
            if (accounts.findAccount(user, service))
                continue; // 本人该服务已有号码：不改写
            pending.emplace_back(service, entry.second);
        }
        // 好友关系：只处理"本人该服务已开通（含本次刚补建的）、对端账号在服务端存在"的组合，
        // 其余静默跳过（对端没在服务端建号时好友关系没有意义，也不该让登记失败）。
        std::vector<std::pair<ServiceTypeFanLX, std::string>> friendships;
        for (const auto &entry : friends) {
            ServiceTypeFanLX service{};
            if (!serviceFromNameFanLX(entry.first, service) || !isSubordinateServiceFanLX(service) ||
                entry.second.empty())
                continue;
            std::string mineId;
            if (const auto *mine = accounts.findAccount(user, service);
                mine && accounts.isAccountSubscribed(service, mine->accountId))
                mineId = mine->accountId;
            if (mineId.empty()) {
                // 本次登记刚补建的服务：状态还没落到 pending 之外，按 pending 里的号码算。
                for (const auto &item : pending)
                    if (item.first == service)
                        mineId = item.second;
            }
            if (mineId.empty())
                continue; // 本人该服务没开通：好友关系无从挂载
            if (!accounts.hasAccount(service, entry.second))
                continue; // 对端在服务端没有这个服务账号
            if (mineId == entry.second)
                continue;
            friendships.emplace_back(service, entry.second);
        }
        // 开通下级服务只允许"以微X身份登录"的会话发起（领域规则），所以这里用刚校验过的
        // 同一份凭据建立微X会话，开通完立即注销：服务端不留下登录态。
        if (!pending.empty()) {
            const AccountRefFanLX weixRef{ServiceTypeFanLX::WeiX, weixId};
            appFanLX.login(weixRef, password, false);
            try {
                for (const auto &item : pending)
                    appFanLX.activateService(weixRef, item.first, item.second);
            } catch (...) {
                appFanLX.logout(weixRef, false);
                throw;
            }
            appFanLX.logout(weixRef, false);
        }
        // 好友关系用"镜像"入口：客户端加好友时已经校验过对端确认登录，服务端不需要也无法
        // 重放那一步（它没有对端密码），只需要让权限判定看到这条关系。幂等，重复登记无副作用。
        for (const auto &item : friendships) {
            const auto *mine = appFanLX.state().accounts.findAccount(user, item.first);
            if (!mine)
                continue; // 上一步开通失败：跳过该服务的好友补登
            appFanLX.mirrorFriendship({item.first, mine->accountId}, {item.first, item.second});
        }
        return {};
    } catch (const std::exception &error) {
        return error.what();
    }
}

std::string ServerAccountRegistrarFanLX::updateFriendshipFanLX(const std::string &service,
                                                               const std::string &self,
                                                               const std::string &peer, bool add) const {
    try {
        ServiceTypeFanLX type{};
        if (!serviceFromNameFanLX(service, type) || !isSubordinateServiceFanLX(type))
            return "好友关系只存在于 QQ / 微信 / 微博";
        if (self.empty() || peer.empty() || self == peer)
            return "好友账号无效";
        const auto &accounts = appFanLX.state().accounts;
        if (!accounts.hasAccount(type, self) || !accounts.hasAccount(type, peer))
            return "好友双方必须都在服务端开通了该服务";
        if (add)
            (void)appFanLX.mirrorFriendship({type, self}, {type, peer});
        else
            (void)appFanLX.unmirrorFriendship({type, self}, {type, peer});
        return {};
    } catch (const std::exception &error) {
        return error.what();
    }
}

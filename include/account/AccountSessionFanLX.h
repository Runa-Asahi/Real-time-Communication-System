#pragma once
#include "account/AccountManagerFanLX.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// AccountSessionFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
class AccountSessionFanLX {
    AccountManagerFanLX *managerFanLX; // 关联的账户管理器（不拥有；生命周期由调用方保证）
    ServiceTypeFanLX serviceFanLX;     // 本会话所在平台
    std::string accountIdFanLX;        // 本会话的平台账号 ID（前端可见身份）
    std::string userIdFanLX;           // 归属用户 ID（后端内部维度；私有，无访问器）

  public:
    // 构造：仅供 AccountManagerFanLX::login 签发。
    AccountSessionFanLX(AccountManagerFanLX &manager, ServiceTypeFanLX service, std::string accountId,
                        std::string user)
        : managerFanLX(&manager), serviceFanLX(service), accountIdFanLX(std::move(accountId)),
          userIdFanLX(std::move(user)) {}

    // 查询：本会话所在平台。
    ServiceTypeFanLX service() const {
        return serviceFanLX;
    }
    // 查询：本会话的平台账号 ID —— 这就是前端所持有的全部身份信息。
    const std::string &accountId() const {
        return accountIdFanLX;
    }
    // 查询：本会话是否仍有效（未被注销）；被注销后使用方应以"会话已过期"处理。
    bool isLoggedIn() const {
        return managerFanLX->isAccountLoggedIn(serviceFanLX, accountIdFanLX);
    }

    // 自主开通：为本人开通某平台服务（该平台须已有本人的服务账号）。
    // 对应工程计划 S02 的"自主开通"；登录本身不会替用户开通未开通的服务。
    void subscribeTo(ServiceTypeFanLX service) {
        managerFanLX->subscribe(userIdFanLX, service);
    }

    // 开通下级服务（微X 上层之下的 QQ / 微信 / 微博）：建号 + 立即开通一步完成。
    // 只有微X会话能调用——"只有登录微X后才能在内部去开通三种服务"就落在这条前置校验上，
    // 因此控制台与 Qt 界面都不需要各自再判一次身份。
    void activateService(ServiceTypeFanLX service, const std::string &serviceAccountId) {
        if (serviceFanLX != ServiceTypeFanLX::WeiX)
            throw std::runtime_error("开通服务需要以微X身份登录（微X 是 QQ/微信/微博 的上层账号）");
        managerFanLX->activateService(userIdFanLX, service, serviceAccountId);
    }
    // 本人当前登录身份是否为微X（上层账号）。界面据此决定是否显示"开通管理"。
    bool isWeiXSession() const { return serviceFanLX == ServiceTypeFanLX::WeiX; }

    // 建立绑定：把"当前登录的这个账号"绑定到另一个平台账号
    // （如"我在微信里绑定我的 QQ 号"），参数是对方平台账号，不涉及 userId。
    void bindTo(ServiceTypeFanLX targetService, const std::string &targetAccountId) {
        managerFanLX->bindAccounts(serviceFanLX, accountIdFanLX, targetService, targetAccountId);
    }
    // 解除绑定：解除当前账号与另一个平台账号之间的绑定关系。
    bool unbindFrom(ServiceTypeFanLX targetService, const std::string &targetAccountId) {
        return managerFanLX->unbindAccounts(serviceFanLX, accountIdFanLX, targetService, targetAccountId);
    }
    // 查询：当前账号的全部绑定对端账号。
    std::vector<ServiceAccountFanLX> boundAccounts() const {
        return managerFanLX->bindingsOf(serviceFanLX, accountIdFanLX);
    }

    // 注销当前平台服务（只退本平台，其它平台不受影响）。
    void logout() {
        managerFanLX->logout(serviceFanLX, accountIdFanLX);
    }
    // 平台注销：退出本人的全部已登录平台（内部按 userId 清理，不暴露 userId）。
    void logoutAll() {
        managerFanLX->logoutAll(serviceFanLX, accountIdFanLX);
    }
};
// 登录入口：用户名（平台账号 ID）+ 密码。密码校验是这里唯一放行的凭据关口；
// resumeSession 之类不校验密码的入口都要求"已经处于登录态"，因此无法绕过这一次校验。
inline AccountSessionFanLX AccountManagerFanLX::login(ServiceTypeFanLX service, const std::string &accountId,
                                                      const std::string &password, bool confirmOthers) {
    // 账号不存在与密码错误回同一句话：不让调用方区分"人不存在"和"密码打错"。
    if (!verifyPassword(service, accountId, password))
        throw CredentialRejectedFanLX("用户名或密码错误");
    const auto key = keyOfAccountIdFanLX(service, accountId); // 此时账号必然存在
    const std::string &user = accountsFanLX.at(key).userId;
    loginCoreFanLX(user, key, confirmOthers);
    return AccountSessionFanLX(*this, service, accountId, user);
}
inline AccountSessionFanLX AccountManagerFanLX::resumeSession(ServiceTypeFanLX service,
                                                              const std::string &accountId) {
    const auto key = keyOfAccountIdFanLX(service, accountId); // 账号不存在时抛"平台账号不存在"
    if (!loggedInFanLX.count(key))
        throw SessionExpiredFanLX("服务未登录");
    return AccountSessionFanLX(*this, service, accountId, accountsFanLX.at(key).userId);
}
inline void AccountManagerFanLX::confirmLoginInSession(ServiceTypeFanLX actorService,
                                                       const std::string &actorAccountId,
                                                       ServiceTypeFanLX targetService,
                                                       const std::string &targetAccountId) {
    // 第 1 步：授权来源是"调用方已持有本人的有效会话"，而不是第二次输入密码。
    // 未登录或账号不存在会在这里抛（SessionExpiredFanLX / 平台账号不存在）。
    (void)resumeSession(actorService, actorAccountId);
    const auto actorKey = keyOfAccountIdFanLX(actorService, actorAccountId);
    const auto targetKey = keyOfAccountIdFanLX(targetService, targetAccountId);
    const auto &from = accountsFanLX.at(actorKey), &to = accountsFanLX.at(targetKey);
    if (from.userId != to.userId)
        throw std::runtime_error("逐项确认需要本人账号");
    // 第 2 步：联动依据是"微X 上层身份"、"同号"或"显式绑定"，"同一个人"本身不是依据
    // （微信与 QQ 是同一人但号码不同，必须先绑定或走微X上层）。规则与 loginCoreFanLX 一致。
    const bool umbrella = from.service == ServiceTypeFanLX::WeiX && to.service != ServiceTypeFanLX::WeiX;
    const bool linked = umbrella || (from.sharedId && to.sharedId && from.accountId == to.accountId) ||
                        boundFanLX(actorKey, targetKey);
    if (!linked)
        throw std::runtime_error("逐项确认需要本人的微X身份、共享号码或已验证绑定");
    loginCoreFanLX(from.userId, targetKey, false);
}

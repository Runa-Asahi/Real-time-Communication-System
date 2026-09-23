#pragma once
// 依赖账户管理器完成「平台 + 账号 ID -> 全局唯一账号键」的换算；
// 只引入真正使用的头文件，不反向依赖域聚合头，避免循环包含。
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

// IdentityResolverFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
class IdentityResolverFanLX {
    const AccountManagerFanLX &accountsFanLX; // 持有账户管理器的引用（只读依赖）
    // 工具函数：由账号记录拼出全局唯一账号键。
    static std::string keyOfFanLX(const ServiceAccountFanLX &account) {
        return account.userId + "|" + serviceNameFanLX(account.service) + "|" + account.accountId;
    }

  public:
    explicit IdentityResolverFanLX(const AccountManagerFanLX &accounts) : accountsFanLX(accounts) {}
    // 解析（前端口径）：由「平台 + 账号 ID」拼出账号键；账号不存在时抛异常。
    std::string accountKey(ServiceTypeFanLX service, const std::string &accountId) const {
        return keyOfFanLX(accountsFanLX.accountByAccountId(service, accountId));
    }
    // 解析（内部口径）：由「归属用户 + 平台」拼出账号键；该用户没有该平台账号时返回空串。
    // 供"同一用户的另一个平台账号"这类内部换算使用，不作为接口参数口径。
    std::string accountKeyOfUser(const std::string &user, ServiceTypeFanLX service) const {
        const auto *found = accountsFanLX.findAccount(user, service);
        return found ? keyOfFanLX(*found) : std::string{};
    }
    // 反解：从账号键中取出最前面的用户 ID 段（第一个 '|' 之前）。
    // 仅供本文件内部的跳平台聚合使用；userId 不会经由它流向调用方。
    std::string userForAccount(const std::string &key) const {
        const auto first = key.find('|');
        return first == std::string::npos ? std::string{} : key.substr(0, first);
    }
    // 跨服务映射只认明示共享号码或已验证绑定；同属 userId 只用于定位，不能作为授权依据。
    std::string verifiedTarget(const std::string &sourceKey, ServiceTypeFanLX source,
                               ServiceTypeFanLX target) const {
        const auto user = userForAccount(sourceKey);
        const auto *from = accountsFanLX.findAccount(user, source);
        const auto *to = accountsFanLX.findAccount(user, target);
        if (!from || !to || !accountsFanLX.isAccountSubscribed(target, to->accountId)) return {};
        if (source == target || (from->sharedId && to->sharedId && from->accountId == to->accountId) ||
            accountsFanLX.isBound(source, from->accountId, target, to->accountId))
            return keyOfFanLX(*to);
        return {};
    }
};

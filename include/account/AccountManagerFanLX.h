#pragma once
#include "account/CredentialRejectedFanLX.h"
#include "account/ServiceAccountFanLX.h"
#include "account/ServiceNotConfirmedFanLX.h"
#include "account/ServiceNotSubscribedFanLX.h"
#include "account/SessionExpiredFanLX.h"
#include "account/UserProfileFanLX.h"
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

// AccountManagerFanLX：从原领域聚合头文件拆出，保留已验证的业务接口。
class AccountSessionFanLX;
class AccountManagerFanLX {
    friend class SnapshotCodecFanLX;
    friend class PreparedCommitFanLX;
    // 存储键 = 用户ID + "|" + 服务类型
    std::unordered_map<std::string, UserProfileFanLX> usersFanLX;
    std::unordered_map<std::string, ServiceAccountFanLX> accountsFanLX;
    // 平台账号索引：键 "服务名|账号ID" -> 账号存储键。
    // accountId 是前端登录身份，必须平台内唯一；该索引同时承担唯一性校验与 O(1) 定位。
    std::unordered_map<std::string, std::string> accountIdIndexFanLX;
    std::unordered_set<std::string> subscriptionsFanLX; // 已"开通"服务的键集合（键格式 "userId|服务名"）
    std::unordered_map<std::string, std::string> bindingsFanLX; // 绑定关系：账号键 -> 与之绑定的另一账号键
    std::unordered_set<std::string> loggedInFanLX;              // 当前处于"登录态"的账号键集合

    // 工具函数：由 用户ID + 服务类型 生成统一存储键，如 "U001|QQ"。
    static std::string keyFanLX(const std::string &user, ServiceTypeFanLX service) {
        return user + "|" + serviceNameFanLX(service);
    }
    // 工具函数：由 平台 + 账号 ID 生成索引键，如 "QQ|20260001"。
    static std::string accountIdKeyFanLX(ServiceTypeFanLX service, const std::string &accountId) {
        return serviceNameFanLX(service) + "|" + accountId;
    }
    // 工具函数：由 平台 + 账号 ID 定位账号存储键；账号不存在时抛异常。
    std::string keyOfAccountIdFanLX(ServiceTypeFanLX service, const std::string &accountId) const {
        auto it = accountIdIndexFanLX.find(accountIdKeyFanLX(service, accountId));
        if (it == accountIdIndexFanLX.end())
            throw std::runtime_error("平台账号不存在");
        return it->second;
    }
    // 工具函数：删除一条指定方向的绑定记录，返回是否真的删除了。
    bool dropBindingFanLX(const std::string &fromKey, const std::string &toKey) {
        auto it = bindingsFanLX.find(fromKey);
        if (it == bindingsFanLX.end() || it->second != toKey)
            return false;
        bindingsFanLX.erase(it);
        return true;
    }

    // 工具函数：判断两个账号键是否"同号"——双方都标记了 sharedId，且账号 ID 完全相同。
    // 语义：同一个账号 ID 在多个平台复用，因此一次登录应当在这些平台同时生效。
    bool sameSharedAccountFanLX(const std::string &leftKey, const std::string &rightKey) const {
        const auto &left = accountsFanLX.at(leftKey), &right = accountsFanLX.at(rightKey);
        return left.sharedId && right.sharedId && left.accountId == right.accountId;
    }

    // 工具函数：判断两个账号键之间是否存在显式绑定关系（双向查 bindingsFanLX）。
    // 语义：平台不同、账号 ID 也完全不同（如 QQ 号与微信号），但用户主动建立过绑定，
    //       这是统一登录的第二个依据，也是 bindingsFanLX 真正的读者。
    bool boundFanLX(const std::string &leftKey, const std::string &rightKey) const {
        auto it = bindingsFanLX.find(leftKey);
        if (it != bindingsFanLX.end() && it->second == rightKey)
            return true;
        it = bindingsFanLX.find(rightKey);
        return it != bindingsFanLX.end() && it->second == leftKey;
    }

    // 工具函数：某用户的微X账号 ID（没有则返回空串）。微X 是上层账号，
    // 下级服务的号码默认与它对齐，因此这里被多处复用。
    std::string weixAccountIdOfFanLX(const std::string &user) const {
        const auto *found = findAccount(user, ServiceTypeFanLX::WeiX);
        return found ? found->accountId : std::string{};
    }

    // 工具函数：按「归属用户 + 平台」取可变账号指针（内部换算用，找不到返回 nullptr）。
    ServiceAccountFanLX *mutableAccountFanLX(const std::string &user, ServiceTypeFanLX service) {
        auto it = accountsFanLX.find(keyFanLX(user, service));
        return it == accountsFanLX.end() ? nullptr : &it->second;
    }

    // 工具函数：重算某用户 QQ / 微博两个账号的"共享号码"标记。
    // 课程约定 QQ 与微博共用同一个号码：两边都在时必须同号（否则抛异常，宁可拒绝也不写出
    // 一份自相矛盾的账号库），此时两边都标 sharedId；只有一边时，号码与微X号相同即视为共享
    // ——微X号是本人的统一号码，QQ/微博沿用它就是"同号"。
    void syncSharedFlagsFanLX(const std::string &user) {
        auto *qq = mutableAccountFanLX(user, ServiceTypeFanLX::QQ);
        auto *weibo = mutableAccountFanLX(user, ServiceTypeFanLX::Weibo);
        const auto weix = weixAccountIdOfFanLX(user);
        if (qq && weibo) {
            if (qq->accountId != weibo->accountId)
                throw std::runtime_error("QQ 与微博必须共用同一个号码（课程约定：两者共享 ID）");
            qq->sharedId = true;
            weibo->sharedId = true;
            return;
        }
        if (qq)
            qq->sharedId = !weix.empty() && qq->accountId == weix;
        if (weibo)
            weibo->sharedId = !weix.empty() && weibo->accountId == weix;
    }

    // 登录核心：把 currentKey 置为登录态；confirmOthers 时联动该用户其它已开通平台。
    // user 仅用于把遍历范围限定在"同一用户的订阅键"上，不构成登录依据。
    void loginCoreFanLX(const std::string &user, const std::string &currentKey, bool confirmOthers) {
        // 未开通的服务不允许登录（工程计划：未开通服务不被自动开通）
        if (!subscriptionsFanLX.count(currentKey))
            throw ServiceNotSubscribedFanLX("服务尚未开通");
        loggedInFanLX.insert(currentKey);
        if (!confirmOthers)
            return;
        // 微X 是 QQ/微信/微博 的上层账号：登微X（并简单确认）就是"本人已开通的下级服务一并登录"，
        // 不再要求下级之间同号或互相绑定——授权来源就是这一次微X登录本身。
        const bool umbrella =
            accountsFanLX.at(currentKey).service == ServiceTypeFanLX::WeiX;
        for (const auto &item : subscriptionsFanLX) // 只遍历已开通的平台
        {
            if (item == currentKey || item.rfind(user + "|", 0) != 0) // 跳过自身与他人的订阅键
                continue;
            if (umbrella)
                loggedInFanLX.insert(item);
            // 下级服务之间：只有"同号"或"显式绑定"才联动；"同一个人"本身不是理由
            else if (sameSharedAccountFanLX(item, currentKey) || boundFanLX(item, currentKey))
                loggedInFanLX.insert(item);
        }
    }

  public:
    // ==================== 后端配给 API（以内部 userId 为键，前端不调用） ====================

    // 注册新用户：用户 ID 必须全局唯一，重复注册抛出异常。
    // 密码由用户在注册时自选，是后续登录的唯一凭据，因此不接受空密码——
    // 账号库里没有"无密码账号"这种状态，否则登录校验就形同虚设。
    void addUser(UserProfileFanLX profile) {
        if (profile.userId.empty())
            throw std::runtime_error("用户ID不能为空");
        if (profile.password.size() > 4096)
            throw std::runtime_error("密码超过 4096 字节");
        if (profile.password.empty())
            throw std::runtime_error("密码不能为空");
        // emplace 失败（键已存在）返回 second=false
        if (!usersFanLX.emplace(profile.userId, std::move(profile)).second)
            throw std::runtime_error("用户ID已存在");
    }

    // 为某用户建号（开通平台账号）：用户必须已注册；
    // 同一用户在同一平台只能有一个账号，且同一平台的账号 ID 不允许被重复注册
    // —— accountId 是前端登录身份，必须平台内唯一，否则登录无法唯一定位。
    void addAccount(ServiceAccountFanLX account) {
        // 校验归属用户是否存在
        if (usersFanLX.count(account.userId) == 0)
            throw std::runtime_error("账号归属用户不存在");
        // 唯一性 1：同一用户在同一平台只能有一个账号
        auto key = keyFanLX(account.userId, account.service);
        if (accountsFanLX.count(key) != 0)
            throw std::runtime_error("服务账号已存在");
        // 唯一性 2：同一平台的账号 ID 不能被两人共用（否则登录身份不唯一）
        auto index = accountIdKeyFanLX(account.service, account.accountId);
        if (accountIdIndexFanLX.count(index) != 0)
            throw std::runtime_error("该平台账号已被占用");
        accountIdIndexFanLX.emplace(index, key); // 先登记索引，再登记账号
        accountsFanLX.emplace(key, std::move(account));
    }

    // 开通服务（后端配给口径）：前提是该用户在该平台已存在服务账号。
    // 前端用户"自主开通"请用 AccountSessionFanLX::subscribeTo。
    void subscribe(const std::string &user, ServiceTypeFanLX service) {
        if (accountsFanLX.count(keyFanLX(user, service)) == 0)
            throw std::runtime_error("该服务账号不存在");
        subscriptionsFanLX.insert(keyFanLX(user, service));
    }

    void updateProfile(const std::string &user, std::string nickname, std::string location) {
        updateNickname(user, std::move(nickname));
        updateLocation(user, std::move(location));
    }

    // 单项修改：空串表示"这一项不改"，因此界面可以只提交昵称或只提交所在地。
    // 以前这里要求两项都非空并且一起覆盖，用户想改昵称就被迫把所在地也重打一遍。
    void updateNickname(const std::string &user, std::string nickname) {
        if (nickname.empty())
            return;
        usersFanLX.at(user).nickname = std::move(nickname);
    }
    void updateLocation(const std::string &user, std::string location) {
        if (location.empty())
            return;
        usersFanLX.at(user).location = std::move(location);
    }

    // ==================== 微X 上层：注册与开通下级服务 ====================

    // 远端账号导入（添加好友时用）：把**服务端已确认存在**的对端账号登记进本机副本。
    //
    // 为什么需要它：本机领域库只是"谁存在、谁开通了什么"的一份局部视图，别人在各自的客户端
    // 注册/开通出来的账号不会自动出现在这里；而添加好友的前置校验要查本机账号表，于是就会
    // 报"对端平台账号不存在"。做法是：先问服务端（IM 的 LOOKUP），再由本方法补一条最小记录。
    //
    // 幂等：缺什么补什么，已存在就只补"已确认登录"的演示标记，并返回是否**新建**了账号记录。
    // 密码用占位值：对端的密码我们无从得知（认证始终发生在服务端），但快照校验要求用户
    // 必须有非空密码，因此写入一个不可能被猜到的占位串，本机不会用它登录任何人。
    // applyDate 由调用方给（领域层不取系统时钟）。
    bool importRemoteAccount(ServiceTypeFanLX service, const std::string &accountId,
                             const std::string &userId, const std::string &nickname,
                             const std::string &applyDate) {
        if (!isSubordinateServiceFanLX(service))
            throw std::runtime_error("只有 QQ / 微信 / 微博 能作为好友所在的服务");
        if (accountId.empty() || userId.empty())
            throw std::runtime_error("导入远端账号需要号码与归属用户");
        if (usersFanLX.count(userId) == 0)
            addUser({userId, nickname.empty() ? accountId : nickname, "2000-01-01",
                     applyDate.empty() ? "2000-01-01" : applyDate, "", "<remote-account>"});
        bool created = false;
        if (accountIdIndexFanLX.count(accountIdKeyFanLX(service, accountId)) == 0) {
            addAccount({userId, service, accountId, false});
            syncSharedFlagsFanLX(userId);
            created = true;
        }
        subscribe(userId, service);
        // "对端已确认登录"的演示语义：对端是否在线由服务端决定，本机副本据此标记为已确认；
        // 否则领域层会以"对端服务尚未确认登录"拒绝加好友——单机演示没有第二个进程替对方登录。
        loggedInFanLX.insert(keyFanLX(userId, service));
        return created;
    }

    // 注册微X账号：一个用户只有一个微X账号（上层身份），注册时**不**捆绑任何下级服务账号
    // ——QQ / 微信 / 微博 必须由本人登录微X后在内部逐个开通（见 activateService）。
    // 微X账号本身立即处于"已开通"状态，否则注册完无法登录。
    void registerWeiXAccount(UserProfileFanLX profile, std::string weixAccountId) {
        if (weixAccountId.empty())
            throw std::runtime_error("微X号不能为空");
        const auto user = profile.userId; // 先取副本：addUser 会移动 profile
        addUser(std::move(profile));
        addAccount({user, ServiceTypeFanLX::WeiX, std::move(weixAccountId), false});
        subscriptionsFanLX.insert(keyFanLX(user, ServiceTypeFanLX::WeiX));
        syncSharedFlagsFanLX(user);
    }

    // 开通下级服务（微X 上层之下）：为该用户建号并立即置为已开通。
    // 前置：本人必须有微X账号（上层）——"只有登录微X后才能在内部去开通三种服务"在
    // AccountSessionFanLX::activateService 里以"必须是微X会话"强制，这里再校验一次账号事实。
    //
    // 号码是**按服务各自唯一**的：QQ / 微信 / 微博 三个聊天通道允许使用相同号码
    // （课程约定 QQ 与微博本来就共号），因为服务端把会话按"服务 + 号码"分开存放与投递，
    // 客户端存储目录也带服务名，两条通道不会互相串消息。
    void activateService(const std::string &user, ServiceTypeFanLX service, std::string accountId) {
        if (!isSubordinateServiceFanLX(service))
            throw std::runtime_error("微X 是上层账号，不能作为下级服务开通");
        if (accountId.empty())
            throw std::runtime_error("服务号码不能为空");
        if (accountsFanLX.count(keyFanLX(user, ServiceTypeFanLX::WeiX)) == 0)
            throw std::runtime_error("本人没有微X账号：请先注册并登录微X");
        if (accountsFanLX.count(keyFanLX(user, service)) != 0)
            throw std::runtime_error("该服务已经开通，无需重复开通");
        if (accountIdIndexFanLX.count(accountIdKeyFanLX(service, accountId)) != 0)
            throw std::runtime_error("该服务号码已被占用");
        addAccount({user, service, std::move(accountId), false});
        subscriptionsFanLX.insert(keyFanLX(user, service));
        syncSharedFlagsFanLX(user); // QQ 与微博的共享号码标记在这里统一维护
    }

    // ==================== 前端入口：登录（用户名 + 密码） ====================

    // 以「平台账号 + 密码」登录并签发会话：调用方持有的是 QQ 号 / 微信号加用户自选密码，
    // 不涉及内部 userId。用户名就是平台账号 ID（accountId），与注册时登记的一致。
    // 密码不符或账号不存在统一抛 CredentialRejectedFanLX("用户名或密码错误")。
    // confirmOthers 为 true 时，按"同号 / 显式绑定"两条依据批量确认登录该用户其它已开通平台
    // （对应工程计划"一次简单确认登录全部"）；为 false 则只登录当前账号。
    // 返回的会话是前端身份载体，只有服务与账号 ID，拿不到 userId。
    AccountSessionFanLX login(ServiceTypeFanLX service, const std::string &accountId,
                              const std::string &password, bool confirmOthers);

    // 密码校验：只回 true/false，不抛异常，供网络认证等"凭据是否成立"的调用方使用。
    // 账号不存在、账号未设密码、密码不符一律 false。
    bool verifyPassword(ServiceTypeFanLX service, const std::string &accountId,
                        const std::string &password) const {
        const auto index = accountIdIndexFanLX.find(accountIdKeyFanLX(service, accountId));
        if (index == accountIdIndexFanLX.end())
            return false;
        const auto &profile = usersFanLX.at(accountsFanLX.at(index->second).userId);
        // 未设密码的账号（例如旧数据加载而来）对任何输入都不通过，包括空串。
        return !profile.password.empty() && profile.password == password;
    }

    // 修改密码（注册之外唯一的凭据写入口）：旧密码必须正确，新密码不允许为空。
    void changePassword(ServiceTypeFanLX service, const std::string &accountId,
                        const std::string &oldPassword, std::string newPassword) {
        if (!verifyPassword(service, accountId, oldPassword))
            throw CredentialRejectedFanLX("用户名或密码错误");
        if (newPassword.empty())
            throw std::runtime_error("新密码不能为空");
        if (newPassword.size() > 4096)
            throw std::runtime_error("新密码超过 4096 字节");
        usersFanLX.at(accountsFanLX.at(keyOfAccountIdFanLX(service, accountId)).userId).password =
            std::move(newPassword);
    }

    // 会话恢复：调用方已凭 isAccountLoggedIn 确认该账号处于登录态，这里只重建前端身份句柄，
    // 不再重复校验密码（密码在 login 那一次已经验证过）。未登录时抛 SessionExpiredFanLX。
    AccountSessionFanLX resumeSession(ServiceTypeFanLX service, const std::string &accountId);

    // 逐项确认登录：在"同一用户的已认证会话"内确认他的另一个平台账号登录
    // （对应控制台"3 逐项确认登录"）。授权来源是已认证的会话，因此不再要求第二次密码；
    // 但联动依据仍然是"同号 / 显式绑定"，与 confirmOthers 完全同一套规则。
    void confirmLoginInSession(ServiceTypeFanLX actorService, const std::string &actorAccountId,
                               ServiceTypeFanLX targetService, const std::string &targetAccountId);

    // ==================== 后端查询 API（配给、统计、持久化与测试用） ====================

    // 查询：用户是否已开通某平台服务。
    bool isSubscribed(const std::string &user, ServiceTypeFanLX service) const {
        return subscriptionsFanLX.count(keyFanLX(user, service)) != 0;
    }
    // 查询：用户在某平台是否处于登录态。
    bool isLoggedIn(const std::string &user, ServiceTypeFanLX service) const {
        return loggedInFanLX.count(keyFanLX(user, service)) != 0;
    }
    // 查询：取出用户资料（键不存在时 at 会抛出异常）。
    const UserProfileFanLX &profile(const std::string &user) const {
        return usersFanLX.at(user);
    }
    // 查询：按「归属用户 + 平台」找服务账号；不存在时返回 nullptr（"查不到就跳过"的内部换算用）。
    const ServiceAccountFanLX *findAccount(const std::string &user, ServiceTypeFanLX service) const {
        auto it = accountsFanLX.find(keyFanLX(user, service));
        return it == accountsFanLX.end() ? nullptr : &it->second;
    }
    // 查询：按「归属用户 + 平台」取服务账号；不存在时抛异常。
    const ServiceAccountFanLX &account(const std::string &user, ServiceTypeFanLX service) const {
        const auto *found = findAccount(user, service);
        if (!found)
            throw std::runtime_error("服务账号不存在");
        return *found;
    }

    // 查询：列出某用户拥有的全部服务账号。
    std::vector<ServiceAccountFanLX> accounts(const std::string &user) const {
        std::vector<ServiceAccountFanLX> result;
        // 遍历所有账号，筛选出归属该用户的（结构化绑定解出 键-值 对）
        for (const auto &[key, a] : accountsFanLX)
            if (a.userId == user)
                result.push_back(a);
        return result;
    }

    // ==================== 平台账号视角的操作（供会话与好友域使用） ====================

    // 查询：某平台账号是否已建号（不抛异常）。
    bool hasAccount(ServiceTypeFanLX service, const std::string &accountId) const {
        return accountIdIndexFanLX.count(accountIdKeyFanLX(service, accountId)) != 0;
    }
    // 查询：某平台账号是否已开通该平台服务（账号不存在时返回 false，不抛异常）。
    bool isAccountSubscribed(ServiceTypeFanLX service, const std::string &accountId) const {
        auto it = accountIdIndexFanLX.find(accountIdKeyFanLX(service, accountId));
        return it != accountIdIndexFanLX.end() && subscriptionsFanLX.count(it->second) != 0;
    }
    // 查询：某平台账号是否处于登录态（账号不存在时返回 false，不抛异常）。
    bool isAccountLoggedIn(ServiceTypeFanLX service, const std::string &accountId) const {
        auto it = accountIdIndexFanLX.find(accountIdKeyFanLX(service, accountId));
        return it != accountIdIndexFanLX.end() && loggedInFanLX.count(it->second) != 0;
    }
    // 查询：按「平台 + 账号 ID」取服务账号（不存在时抛"平台账号不存在"）。
    const ServiceAccountFanLX &accountByAccountId(ServiceTypeFanLX service,
                                                  const std::string &accountId) const {
        return accountsFanLX.at(keyOfAccountIdFanLX(service, accountId));
    }

    // 注销当前平台服务：只清除该账号自身登录态，不联动其它平台。
    void logout(ServiceTypeFanLX service, const std::string &accountId) {
        loggedInFanLX.erase(keyOfAccountIdFanLX(service, accountId));
    }

    // 平台注销：退出该账号所属用户在**全部平台**的登录态（对应工程计划"平台注销"）。
    // userId 只在后端参与匹配，前端通过会话调用，签名里不出现 userId。
    void logoutAll(ServiceTypeFanLX service, const std::string &accountId) {
        const std::string &user = accountsFanLX.at(keyOfAccountIdFanLX(service, accountId)).userId;
        for (auto it = loggedInFanLX.begin(); it != loggedInFanLX.end();) {
            if (it->rfind(user + "|", 0) == 0)
                it = loggedInFanLX.erase(it); // 属于该用户的登录记录：删除并推进迭代器
            else
                ++it;
        }
    }

    // 建立绑定：把两个平台账号关联起来，参数全是平台账号，前端无需 userId。
    // 工程计划 §4.2：绑定双方必须属于同一平台用户、目标账号存在且归属可验证，
    // 不允许"输入任意 QQ 号就把别人的账号绑到自己名下"。
    void bindAccounts(ServiceTypeFanLX fromService, const std::string &fromAccountId,
                      ServiceTypeFanLX toService, const std::string &toAccountId) {
        if (fromService == toService)
            throw std::runtime_error("不支持绑定同一平台的两个账号");
        // 两个账号都必须已建号（否则 keyOfAccountIdFanLX 抛"平台账号不存在"）
        auto fromKey = keyOfAccountIdFanLX(fromService, fromAccountId);
        auto toKey = keyOfAccountIdFanLX(toService, toAccountId);
        // 归属校验：只有同一内部用户的两个账号才能互相绑定
        if (accountsFanLX.at(fromKey).userId != accountsFanLX.at(toKey).userId)
            throw std::runtime_error("绑定双方必须属于同一用户");
        bindingsFanLX[fromKey] = toKey; // 记录 from -> to 的绑定关系
    }

    // 解除绑定：删除成对的绑定记录，返回是否真的删除了一条。
    // 解绑后不再参与统一登录联动，也不再参与任何依赖绑定映射的跨服务匹配。
    bool unbindAccounts(ServiceTypeFanLX fromService, const std::string &fromAccountId,
                        ServiceTypeFanLX toService, const std::string &toAccountId) {
        auto fromKey = keyOfAccountIdFanLX(fromService, fromAccountId);
        auto toKey = keyOfAccountIdFanLX(toService, toAccountId);
        // 两个方向都尝试删除，解绑对调用方的书写方向不敏感
        bool removed = dropBindingFanLX(fromKey, toKey);
        return dropBindingFanLX(toKey, fromKey) || removed;
    }

    // 查询：两个平台账号之间是否存在绑定关系（双向判定）。
    bool isBound(ServiceTypeFanLX fromService, const std::string &fromAccountId, ServiceTypeFanLX toService,
                 const std::string &toAccountId) const {
        return boundFanLX(keyOfAccountIdFanLX(fromService, fromAccountId),
                          keyOfAccountIdFanLX(toService, toAccountId));
    }

    // 查询：某平台账号的全部绑定对端账号（按账号键排序，保证输出稳定可测）。
    std::vector<ServiceAccountFanLX> bindingsOf(ServiceTypeFanLX service,
                                                const std::string &accountId) const {
        const auto self = keyOfAccountIdFanLX(service, accountId);
        std::vector<std::string> peers; // 先收集对端账号键
        auto it = bindingsFanLX.find(self);
        if (it != bindingsFanLX.end())
            peers.push_back(it->second); // 本账号指向别人
        for (const auto &[from, to] : bindingsFanLX)
            if (to == self)
                peers.push_back(from); // 别人指向本账号
        std::sort(peers.begin(), peers.end());
        std::vector<ServiceAccountFanLX> result;
        for (const auto &peer : peers)
            result.push_back(accountsFanLX.at(peer));
        return result;
    }
};
#include "account/AccountSessionFanLX.h"

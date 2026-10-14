#include "network/TcpListenerFanLX.h"
#include "network/ClientDataPathFanLX.h"
#include "network/ImClientFanLX.h"
#include "network/PeerAccountResolutionFanLX.h"
#include "ui/ConsoleAppFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "friend/IdentityResolverFanLX.h"
#include <algorithm>
#include <charconv>
#include <optional>

std::string ConsoleAppFanLX::fieldFanLX(const char *prompt, bool allowEmpty) {
    outFanLX << prompt << "（0 取消）: ";
    std::string value;
    if (!std::getline(inFanLX, value)) {
        eofFanLX = true;
        throw std::runtime_error("输入结束");
    }
    if (!value.empty() && value.back() == '\r') value.pop_back();
    if (value == "0") throw std::runtime_error("已取消");
    if ((!allowEmpty && value.empty()) || value.size() > 4096)
        throw std::runtime_error("输入为空或超过 4096 字节");
    return value;
}
int ConsoleAppFanLX::choiceFanLX(const char *prompt, int max) {
    outFanLX << prompt << '\n' << "> ";
    std::string line;
    if (!std::getline(inFanLX, line)) { eofFanLX = true; return 0; }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    int result = -1;
    const auto parsed = std::from_chars(line.data(), line.data() + line.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != line.data() + line.size() || result < 0 || result > max)
        throw std::runtime_error("请输入菜单范围内的完整数字");
    return result;
}
ServiceTypeFanLX ConsoleAppFanLX::serviceFanLX() {
    // 可供"开通/确认/绑定"选择的下级服务：微X 是上层身份，不出现在这张表里。
    switch (choiceFanLX("下级服务：1 QQ  2 微信  3 微博  0 取消", 3)) {
    case 1: return ServiceTypeFanLX::QQ;
    case 2: return ServiceTypeFanLX::Wechat;
    case 3: return ServiceTypeFanLX::Weibo;
    default: throw std::runtime_error("已取消");
    }
}
AccountRefFanLX ConsoleAppFanLX::accountFanLX() {
    auto service = serviceFanLX();
    return {service, fieldFanLX("服务号码")};
}
AccountRefFanLX ConsoleAppFanLX::currentFanLX() const {
    if (!actorFanLX || !appFanLX.state().accounts.isAccountLoggedIn(actorFanLX->service, actorFanLX->accountId))
        throw std::runtime_error("请先登录当前身份（微X号）");
    return *actorFanLX;
}
// 可直接聊天的服务优先级：QQ > 微信 > 微博（三种下级服务都能单聊）；微X 是上层登录身份，不作为聊天身份。
AccountRefFanLX ConsoleAppFanLX::preferredChatServiceFanLX() const {
    if (!actorFanLX)
        return {};
    const auto &manager = appFanLX.state().accounts;
    const auto &user = manager.accountByAccountId(actorFanLX->service, actorFanLX->accountId).userId;
    for (const auto service :
         {ServiceTypeFanLX::QQ, ServiceTypeFanLX::Wechat, ServiceTypeFanLX::Weibo}) {
        const auto *found = manager.findAccount(user, service);
        if (found && manager.isAccountSubscribed(service, found->accountId) &&
            manager.isAccountLoggedIn(service, found->accountId))
            return {service, found->accountId};
    }
    return {};
}
GroupKeyFanLX ConsoleAppFanLX::groupFanLX() {
    const auto actor = currentFanLX();
    if (!groupCapableFanLX(actor.service))
        throw std::runtime_error("群功能只支持 QQ 与微信：请先切换到已开通的 QQ / 微信身份");
    return {actor.service == ServiceTypeFanLX::QQ ? GroupModeFanLX::QQ : GroupModeFanLX::Wechat,
            fieldFanLX("群号（使用创建平台的群号）")};
}
void ConsoleAppFanLX::showFanLX(const std::vector<std::string> &values) {
    outFanLX << "查询结果 " << values.size() << " 条\n";
    for (const auto &value : values) outFanLX << "  " << value << '\n';
}
// 登录流程（主菜单 1）：登录微X，或注册一个新的微X账号。
// 注册入口刻意放在**登录流程**里，而不是登录后的"账号与服务"菜单里：
// 注册产出的是上层登录身份，只有登录微X之后才谈得上开通 QQ / 微信 / 微博。
void ConsoleAppFanLX::loginFlowFanLX() {
    const auto choice = choiceFanLX("登录：1 用微X号登录 2 注册微X账号 0 取消", 2);
    if (choice == 0)
        return;
    if (choice == 2) {
        const auto weix = fieldFanLX("微X号（登录用的唯一账号，如 2026013）");
        const auto nickname = fieldFanLX("昵称");
        const auto birth = fieldFanLX("出生日期 YYYY-MM-DD");
        const auto location = fieldFanLX("所在地");
        // 密码由注册者自选，是以后登录的唯一凭据；这里要求二次输入以免打错后无法登录。
        const auto password = fieldFanLX("设置登录密码（明文，本人各服务共用）");
        if (fieldFanLX("再次输入密码") != password)
            throw std::runtime_error("两次输入密码不一致");
        // 内部编号由后台生成，界面不要求用户输入 userId。
        // 注册**只**创建微X账号：不捆绑任何服务账号，QQ/微信/微博 由本人在登录后自主开通。
        appFanLX.registerWeiXUser(
            {"u-" + weix, nickname, birth, utcNowFanLX().substr(0, 10), location, password}, weix);
        outFanLX << "注册成功：微X账号 " << weix << " 已创建（未捆绑任何下级服务）。\n"
                 << "请用该微X号登录，然后在「账号与服务 → 开通下级服务」里自主开通 QQ / 微信 / 微博。\n";
        return;
    }
    const auto weix = fieldFanLX("微X号");
    // 用户自选密码：与注册时设置的是同一个（一个用户一个密码，本人各服务共用）。
    // 明文回显是课程范围内的取舍，见 docs/network-validation.md「假设与边界」。
    auto password = fieldFanLX("登录密码（明文输入）");
    const auto confirm = choiceFanLX("确认联动本人已开通的下级服务？1 全部确认 0 仅当前", 1);
    if (eofFanLX)
        return;
    // 登录身份只有微X号：QQ / 微信 / 微博 是登录后开通出来的下级服务，不是登录入口。
    if (!appFanLX.state().accounts.hasAccount(ServiceTypeFanLX::WeiX, weix))
        throw std::runtime_error("账号库里没有微X号 " + weix + "：请先用「2 注册微X账号」创建");
    appFanLX.login({ServiceTypeFanLX::WeiX, weix}, password, confirm == 1);
    actorFanLX = {ServiceTypeFanLX::WeiX, weix};
    imPasswordFanLX = password;
    outFanLX << "登录成功：微X " << weix << "（上层身份）\n";
    // 把账号（以及本机已开通的服务）登记到**服务端**的持久化账号库：
    // 聊天认证查的是服务端那份，不登记就会在聊天登录时被拒；服务端每次认证都读穿存储，
    // 因此登记之后**不需要重启服务端**。
    if (!imHostFanLX.empty()) {
        std::vector<ImClientFanLX::ServiceEntryFanLX> services;
        const auto &manager = appFanLX.state().accounts;
        const auto &user = manager.accountByAccountId(ServiceTypeFanLX::WeiX, weix).userId;
        for (const auto service :
             {ServiceTypeFanLX::QQ, ServiceTypeFanLX::Wechat, ServiceTypeFanLX::Weibo}) {
            const auto *found = manager.findAccount(user, service);
            if (found && manager.isAccountSubscribed(service, found->accountId))
                services.emplace_back(serviceNameFanLX(service), found->accountId);
        }
        std::vector<ImClientFanLX::ServiceEntryFanLX> friends;
        for (const auto &entry : services) {
            ServiceTypeFanLX service{};
            if (!serviceFromNameFanLX(entry.first, service))
                continue;
            for (const auto &peer : appFanLX.queryFriendAccounts({service, entry.second}))
                friends.emplace_back(entry.first, peer);
        }
        std::string reason;
        const auto registered = ImClientFanLX::registerAccountFanLX(
            imHostFanLX, imPortFanLX, weix, password, std::string(), std::string(), std::string(),
            reason, services, friends);
        outFanLX << (registered ? "服务端账号登记：" : "服务端账号登记失败：") << reason << '\n';
    }
    // 已开通且已登录的聊天服务可以直接用：自动把当前服务切过去，省掉每次手工切换。
    // 需要开通/管理服务时，用「账号与服务 → 切换当前服务」切回微X。
    const auto preferred = preferredChatServiceFanLX();
    if (!preferred.accountId.empty()) {
        actorFanLX = preferred;
        outFanLX << "当前服务自动切到 " << serviceDisplayNameFanLX(preferred.service) << " "
                 << preferred.accountId << "（可用「切换当前服务」改回微X）\n";
        openImClientFanLX(preferred);
    } else {
        outFanLX << "本人还没有开通可聊天的服务：请在「账号与服务 → 开通下级服务」里开通 QQ / 微信 / 微博。\n";
    }
}
// 切换当前操作身份：微X（上层，可开通服务）或本人某个已登录的下级服务。
void ConsoleAppFanLX::switchCurrentServiceFanLX() {
    const auto actor = currentFanLX();
    const auto &manager = appFanLX.state().accounts;
    const auto &user = manager.accountByAccountId(actor.service, actor.accountId).userId;
    std::vector<ServiceAccountFanLX> options;
    for (const auto service : {ServiceTypeFanLX::WeiX, ServiceTypeFanLX::QQ, ServiceTypeFanLX::Wechat,
                               ServiceTypeFanLX::Weibo}) {
        const auto *found = manager.findAccount(user, service);
        if (found)
            options.push_back(*found);
    }
    outFanLX << "可切换的身份：\n";
    for (std::size_t index = 0; index < options.size(); ++index)
        outFanLX << "  " << (index + 1) << " " << serviceDisplayNameFanLX(options[index].service) << " "
                 << options[index].accountId
                 << " 开通=" << manager.isAccountSubscribed(options[index].service, options[index].accountId)
                 << " 登录=" << manager.isAccountLoggedIn(options[index].service, options[index].accountId)
                 << '\n';
    const auto choice = choiceFanLX("选择身份编号（0 取消）", static_cast<int>(options.size()));
    if (choice == 0)
        return;
    const auto &picked = options[static_cast<std::size_t>(choice - 1)];
    if (!manager.isAccountLoggedIn(picked.service, picked.accountId))
        throw std::runtime_error("该身份尚未登录：请先用「逐项确认登录」或重新登录微X");
    actorFanLX = {picked.service, picked.accountId};
    // 聊天连接跟随当前服务：切到 QQ / 微信 / 微博 就按该服务的号码建连（微X 是上层身份，没有聊天通道）。
    if (chatCapableFanLX(picked.service))
        openImClientFanLX(*actorFanLX);
    else
        imFanLX.reset();
    outFanLX << "当前服务已切换到 " << serviceDisplayNameFanLX(picked.service) << " " << picked.accountId
             << '\n';
}
void ConsoleAppFanLX::accountsFanLX() {
    while (!eofFanLX) try {
        const auto option = choiceFanLX("账号：1 本人资料与服务 2 开通下级服务 3 逐项确认登录 4 绑定 5 解绑 "
            "6 单项修改资料 7 切换当前服务 8 注销当前服务 9 平台注销 10 修改密码 0 返回", 10);
        if (!option) return;
        const auto actor = currentFanLX();
        const auto &manager = appFanLX.state().accounts;
        if (option == 1) {
            const auto &account = manager.accountByAccountId(actor.service, actor.accountId);
            const auto &profile = manager.profile(account.userId);
            outFanLX << profile.nickname << " 出生 " << profile.birthDate << " 申请 " << profile.applyDate
                     << " 所在地 " << profile.location << '\n';
            const auto today = utcNowFanLX().substr(0, 10);
            const int age = std::stoi(today.substr(0,4)) - std::stoi(profile.applyDate.substr(0,4)) -
                            (today.substr(5) < profile.applyDate.substr(5) ? 1 : 0);
            outFanLX << "T龄 " << std::max(0, age) << " 年\n";
            for (const auto &service : manager.accounts(account.userId))
                outFanLX << serviceNameFanLX(service.service) << " " << service.accountId
                    << " 开通=" << manager.isAccountSubscribed(service.service, service.accountId)
                    << " 登录=" << manager.isAccountLoggedIn(service.service, service.accountId) << '\n';
            for (const auto &bound : manager.bindingsOf(actor.service, actor.accountId))
                outFanLX << "绑定 " << serviceNameFanLX(bound.service) << " " << bound.accountId << '\n';
        } else if (option == 2) {
            // 开通下级服务 = 建号 + 立即开通；以微X身份执行（领域层会拒绝非微X会话）。
            const auto service = serviceFanLX();
            const auto suggested = std::string("默认 ") +
                (service == ServiceTypeFanLX::Wechat ? "wx_" + actor.accountId : actor.accountId);
            outFanLX << "服务 " << serviceDisplayNameFanLX(service) << " 的号码（" << suggested
                     << "，直接回车沿用）\n";
            auto number = fieldFanLX("服务号码", true);
            if (number.empty())
                number = service == ServiceTypeFanLX::Wechat ? "wx_" + actor.accountId : actor.accountId;
            appFanLX.activateService(actor, service, number);
            outFanLX << "已开通 " << serviceDisplayNameFanLX(service) << " " << number << '\n';
        } else if (option == 3) appFanLX.confirmLogin(actor, accountFanLX());
        else if (option == 4 || option == 5) appFanLX.bind(actor, accountFanLX(), option == 5);
        else if (option == 6) {
            // 单项修改：两项都允许留空，留空表示"这一项不改"。
            outFanLX << "留空表示不修改该项\n";
            auto nickname = fieldFanLX("新昵称（可留空）", true), location = fieldFanLX("新所在地（可留空）", true);
            if (nickname.empty() && location.empty())
                throw std::runtime_error("昵称与所在地都为空：没有要修改的内容");
            appFanLX.updateProfile(actor, nickname, location);
            outFanLX << "资料已更新（只改了填写的那一项）\n";
        } else if (option == 7) {
            switchCurrentServiceFanLX();
            continue;
        } else if (option == 10) {
            const auto oldPassword = fieldFanLX("当前密码");
            const auto newPassword = fieldFanLX("新密码");
            if (fieldFanLX("再次输入新密码") != newPassword) throw std::runtime_error("两次输入密码不一致");
            appFanLX.changePassword(actor, oldPassword, newPassword);
            // 改的是"本客户端数据目录里的账号库"，服务端那份不会跟着变——领域管理不通过 IM
            // 协议同步（注册同理，见 HELP.md 6.10）。所以这里只更新"下次登录要用的凭据"，
            // **不**去重建长连接：现有连接仍以服务端认识的旧密码维持，断了也能自动重连成功。
            // 若在这里按新密码重建，反而会把一条正常连接立刻打断，而服务端还没改，
            // 用户只会看到满屏"用户名或密码错误"。等服务端那份也改成新密码后，用主菜单 1
            // 重新登录即可（那时才会用新密码建连）。
            imPasswordFanLX = newPassword;
        } else {
            appFanLX.logout(actor, option == 9);
            imFanLX.reset();
            imPasswordFanLX.clear();
            actorFanLX.reset();
        }
        outFanLX << "操作完成\n";
    } catch (const std::exception &e) { outFanLX << "操作失败或取消：" << e.what() << '\n'; }
}
void ConsoleAppFanLX::friendsFanLX() {
    while (!eofFanLX) try {
        const auto option = choiceFanLX("好友：1 列表和备注 2 发送好友申请 3 删除 4 备注 5 同平台共同好友 "
            "6 跨平台推荐 7 跨平台共同好友 8 好友申请与临时会话 9 同意申请 10 拒绝申请 11 撤回申请 "
            "12 依据另一个服务的好友申请 0 返回", 12);
        if (!option) return;
        const auto actor = currentFanLX();
        // 好友关系的权威副本在服务端（申请须对方同意）；执行后按服务端清单对账本机副本。
        // 返回 false = 服务端不可达；被服务端拒绝则抛出原因。
        const auto serverOp = [&](const std::string &op, const std::string &peer, const std::string &note,
                                  const std::string &via = {}) {
            const auto result = peerAccountFanLX::friendOperationFanLX(appFanLX, imHostFanLX, imPortFanLX, actor,
                                                                       imPasswordFanLX, op, peer, note, via);
            if (!result.ok && result.reachable) throw std::runtime_error(result.reason);
            if (!result.ok) return std::optional<peerAccountFanLX::FriendOperationResultFanLX>{};
            return std::optional<peerAccountFanLX::FriendOperationResultFanLX>{result};
        };
        if (option == 1) {
            const auto peers = appFanLX.queryFriendAccounts(actor);
            IdentityResolverFanLX resolver(appFanLX.state().accounts);
            const auto own = resolver.accountKey(actor.service, actor.accountId);
            // 只读"当前服务"那一份好友分区：QQ 的身份看不到微信/微博的好友。
            outFanLX << serviceDisplayNameFanLX(actor.service) << " 好友 " << peers.size() << " 人\n";
            for (const auto &peer : peers) {
                const auto detail = appFanLX.state().friends.directory(actor.service)
                                        .detail(own, resolver.accountKey(actor.service, peer));
                outFanLX << peer << " 备注=" << detail.remark << " 标签=" << detail.tag << '\n';
            }
        } else if (option == 6 || option == 7) {
            outFanLX << "选择本人另一个已确认登录的下级服务账号\n";
            showFanLX(appFanLX.queryCrossService(actor, accountFanLX(), option == 6));
            outFanLX << "需要添加时用 12（依据另一个服务的好友申请），或切换到目标平台后用 2 发送申请。\n";
        } else if (option == 8) {
            const auto result = serverOp("list", {}, {});
            if (!result) throw std::runtime_error("服务端不可达：好友申请由服务端中转，请先启动服务端");
            outFanLX << "收到的申请（对方 ｜ 附言 ｜ 时间）：\n";
            showFanLX(result->incoming);
            outFanLX << "我发出的申请：\n";
            showFanLX(result->outgoing);
            outFanLX << "临时会话（非好友，每方向最多 " << result->limit << " 条）：\n";
            showFanLX(result->temporary);
        } else if (option >= 9 && option <= 11) {
            const auto peer = fieldFanLX("对方号码");
            const auto result = serverOp(option == 9 ? "accept" : option == 10 ? "reject" : "cancel", peer, {});
            if (!result) throw std::runtime_error("服务端不可达：好友申请由服务端中转，请先启动服务端");
            outFanLX << result->message << '\n';
        } else if (option == 12) {
            const auto source = serviceFanLX();
            if (source == actor.service || !chatCapableFanLX(source))
                throw std::runtime_error("来源服务必须是本人另一个已开通的 QQ / 微信 / 微博");
            const auto peer = fieldFanLX("对方在来源服务里的号码");
            const auto note = fieldFanLX("附言", true);
            const auto result = serverOp("request", peer, note, serviceNameFanLX(source));
            if (!result) throw std::runtime_error("服务端不可达：跨服务好友申请需要服务端换算号码");
            outFanLX << "已换算为" << serviceDisplayNameFanLX(actor.service) << "号码 " << result->peer << "："
                     << result->message << '\n';
        } else {
            AccountRefFanLX peer{actor.service, fieldFanLX("对方平台账号")};
            if (option == 2) {
                const auto note = fieldFanLX("附言", true);
                const auto result = serverOp("request", peer.accountId, note);
                if (result) {
                    outFanLX << result->message << "（对方同意前可临时会话，每方向最多 " << result->limit
                             << " 条）\n";
                } else {
                    // 没有服务端可中转申请：单机演示直接在本机建立关系（领域规则照旧校验）。
                    outFanLX << "服务端不可达，单机演示模式：直接在本机建立好友关系。\n";
                    appFanLX.addFriend(actor, peer);
                }
            } else if (option == 3) {
                (void)serverOp("remove", peer.accountId, {});
                const auto still = appFanLX.queryFriendAccounts(actor);
                if (std::find(still.begin(), still.end(), peer.accountId) != still.end())
                    appFanLX.removeFriend(actor, peer);
            } else if (option == 4) {
                auto remark = fieldFanLX("备注", true), tag = fieldFanLX("标签", true);
                appFanLX.updateRemark(actor, peer, remark, tag);
            } else showFanLX(appFanLX.queryCommonFriends(actor, peer));
        }
        outFanLX << "操作完成\n";
    } catch (const std::exception &e) { outFanLX << "操作失败或取消：" << e.what() << '\n'; }
}
void ConsoleAppFanLX::groupsFanLX() {
    while (!eofFanLX) try {
        const auto option = choiceFanLX("群：1 我的群 2 成员 3 QQ申请 4 批准 5 拒绝申请 6 微信邀请 "
            "7 确认邀请 8 拒绝邀请 9 退群 10 踢人 11 任命管理员 12 撤销管理员 13 转让群主 "
            "14 切换模式 15 创建讨论组 16 子群加人 17 解散 18 创建群 19 改名 20 子群与待处理记录 0 返回", 20);
        if (!option) return;
        const auto actor = currentFanLX();
        const auto user = appFanLX.state().accounts.accountByAccountId(actor.service, actor.accountId).userId;
        if (option == 1) {
            for (const auto &g : appFanLX.snapshot().groups) {
                const auto service = g.key.mode == GroupModeFanLX::QQ ? ServiceTypeFanLX::QQ : ServiceTypeFanLX::Wechat;
                if (service == actor.service && g.state.members.count(user))
                    outFanLX << g.key.id << " " << g.name << " 当前模式="
                             << (g.currentMode == GroupModeFanLX::QQ ? "QQ" : "Wechat") << '\n';
            }
            continue;
        }
        const auto key = groupFanLX();
        if (option == 2) showFanLX(appFanLX.queryGroupMembers(actor, key));
        else if (option == 3) appFanLX.applyToJoin(actor, key);
        else if (option == 7 || option == 8) appFanLX.confirmInvitation(actor, key, option == 8);
        else if (option == 9) appFanLX.leave(actor, key);
        else if (option == 14) {
            const auto mode = choiceFanLX("目标模式：1 QQ 2 微信 0 取消", 2);
            if (!mode) continue;
            appFanLX.switchPolicy(actor, key, mode == 1 ? GroupModeFanLX::QQ : GroupModeFanLX::Wechat);
        } else if (option == 15) appFanLX.createDiscussion(actor, key, fieldFanLX("讨论组编号"));
        else if (option == 16) {
            auto id = fieldFanLX("讨论组编号");
            AccountRefFanLX peer{actor.service, fieldFanLX("父群成员平台账号")};
            appFanLX.joinDiscussion(actor, key, id, peer);
        } else if (option == 17) {
            if (choiceFanLX("确认解散并归档？1 确认 0 取消", 1) == 1) appFanLX.dissolve(actor, key);
            else continue;
        } else if (option == 18) appFanLX.createGroup(actor, key);
        else if (option == 19) appFanLX.renameGroup(actor, key, fieldFanLX("新群名"));
        else if (option == 20) {
            appFanLX.queryGroupMembers(actor, key); // 鉴权后才展示记录。
            const auto &group = appFanLX.state().groups.get(key);
            const bool manager = group.roleOf(user) == GroupRoleFanLX::Owner ||
                (group.roleOf(user) == GroupRoleFanLX::Admin && group.policy().canAssignAdmin());
            for (const auto &g : appFanLX.snapshot().groups) if (g.key == key) {
                for (const auto &discussion : g.discussions)
                    outFanLX << "子群 " << discussion.id << " 成员=" << discussion.members.size()
                             << " 已归档=" << discussion.archived << '\n';
                if (manager) for (const auto &a : g.applications) if (a.pending)
                    outFanLX << "待审批 " << appFanLX.state().accounts.account(a.applicant, actor.service).accountId << '\n';
                for (const auto &a : g.invitations) if (a.pending && (manager || a.inviter == user || a.invitee == user))
                    outFanLX << "待确认 " << appFanLX.state().accounts.account(a.invitee, actor.service).accountId << '\n';
            }
        } else {
            AccountRefFanLX peer{actor.service, fieldFanLX("目标平台账号")};
            if (option == 4 || option == 5) appFanLX.approveApplication(actor, key, peer, option == 5);
            else if (option == 6) appFanLX.inviteMember(actor, key, peer);
            else if (option == 10) appFanLX.kick(actor, key, peer);
            else if (option == 11 || option == 12) appFanLX.assignAdmin(actor, key, peer, option == 12);
            else if (option == 13) appFanLX.transferOwnership(actor, key, peer);
        }
        outFanLX << "操作完成\n";
    } catch (const std::exception &e) { outFanLX << "操作失败或取消：" << e.what() << '\n'; }
}
void ConsoleAppFanLX::openImClientFanLX(const AccountRefFanLX &actor) {
    // 先释放旧客户端：同一账号目录被第二个进程/对象打开会被目录独占锁拒绝，
    // 而且只有析构了旧客户端，它的网络线程才会停下来。
    imFanLX.reset();
    // 聊天通道：QQ / 微信 / 微博 都能单聊；微X 是上层登录身份，没有聊天通道。
    if (imHostFanLX.empty() || imPasswordFanLX.empty() || !chatCapableFanLX(actor.service))
        return;
    ImClientFanLX::ChatDbConfigFanLX chatDb;
    chatDb.enabled = imUseChatDbFanLX;
    // 先确保服务端账号库里有"这个微X号 + 这个服务"：认证查的是服务端的持久化存储，
    // 而开通服务发生在本机（领域库），两者不会自动同步。登记是幂等的，重复调用没有副作用；
    // 服务端每次认证都读穿存储，因此这里登记完**不需要重启服务端**就能连上。
    {
        const auto &manager = appFanLX.state().accounts;
        const auto &user = manager.accountByAccountId(actor.service, actor.accountId).userId;
        std::vector<ImClientFanLX::ServiceEntryFanLX> services;
        std::string weixId;
        if (const auto *weix = manager.findAccount(user, ServiceTypeFanLX::WeiX)) {
            weixId = weix->accountId;
            for (const auto service :
                 {ServiceTypeFanLX::QQ, ServiceTypeFanLX::Wechat, ServiceTypeFanLX::Weibo}) {
                const auto *found = manager.findAccount(user, service);
                if (found && manager.isAccountSubscribed(service, found->accountId))
                    services.emplace_back(serviceNameFanLX(service), found->accountId);
            }
        }
        if (!weixId.empty()) {
            // 本机已加的好友一并登记：服务端按它自己的账号库判定"是不是好友"，
            // 只在本机加过的好友会被它拒掉。actor 就是当前服务，直接查该服务的好友即可。
            std::vector<ImClientFanLX::ServiceEntryFanLX> friends;
            for (const auto &peer : appFanLX.queryFriendAccounts(actor))
                friends.emplace_back(serviceNameFanLX(actor.service), peer);
            std::string reason;
            const auto pushed = ImClientFanLX::registerAccountFanLX(
                imHostFanLX, imPortFanLX, weixId, imPasswordFanLX, std::string(), std::string(),
                std::string(), reason, services, friends);
            outFanLX << (pushed ? "服务端账号登记：" : "服务端账号登记失败：") << reason << '\n';
        }
    }
    // 存储目录由服务器身份 + 服务 + 登录账号派生，与控制台/GUI/脚本共用同一条规则，
    // 不再各自拼接目录名（旧实现在这里手写过账号的十六进制转义）。
    // 因此 QQ / 微信 / 微博 各自一份聊天库（即使号码相同也不会互相覆盖），消息与历史按服务分开。
    // IM 一侧复用菜单里输入的同一份凭据：没有令牌文件可读。
    imFanLX = std::make_unique<ImClientFanLX>(
        imHostFanLX, imPortFanLX, actor.accountId, imPasswordFanLX,
        imClientPathFanLX::chatDirectoryFanLX(imDataRootFanLX, imHostFanLX, imPortFanLX,
                                              serviceNameFanLX(actor.service), actor.accountId),
        chatDb, serviceNameFanLX(actor.service));
}
void ConsoleAppFanLX::chatFanLX() {
    const auto actor = currentFanLX();
    const auto label = serviceDisplayNameFanLX(actor.service);
    outFanLX << label << " 聊天：中心服务器转发，返回菜单保持长连接。\n";
    // 两种缺失分开报：一个是"当前身份不对"，另一个是"启动时没配服务端地址"，
    // 合并成一句话时用户往往照着错的那半句去改命令。
    if (!chatCapableFanLX(actor.service))
        throw std::runtime_error("微X 是上层登录身份，没有聊天通道：请先切到已开通的 QQ / 微信 / 微博");
    if (!imFanLX)
        throw std::runtime_error("请使用 --im-host / --im-port 配置服务端后登录微X（或切换当前服务）");
    // 微博可以单聊，但没有群：菜单里不显示"群聊发送"，误选时也明确拒绝。
    const bool hasGroup = groupCapableFanLX(actor.service);
    while (!eofFanLX) {
        outFanLX << (imFanLX->online() ? "在线" : "连接中") << " 待确认=" << imFanLX->pending()
                 << " " << imFanLX->error() << '\n';
        const auto option = choiceFanLX(
            hasGroup ? "聊天：1 单聊发送 2 历史记录 3 群聊发送 4 好友在线查询 5 状态通知 0 返回"
                     : "聊天：1 单聊发送 2 历史记录 4 好友在线查询 5 状态通知 0 返回（本服务没有群）",
            5);
        if (!option) return;
        if (option == 3 && !hasGroup) {
            outFanLX << "本服务（" << label << "）没有群功能：只有 QQ 与微信有群。\n";
            continue;
        }
        if (option == 1 || option == 3) {
            const auto target = fieldFanLX(option == 1 ? "好友号码" : "群号");
            const auto text = fieldFanLX("消息");
            outFanLX << "已保存待发送：" << imFanLX->send(target, text, option == 3) << '\n';
        } else if (option == 2) {
            for (const auto &m : imFanLX->history())
                outFanLX << (m.scope == "group" ? "群 " : "单聊 ") << m.recipient << " #" << m.messageId
                         << " " << m.sender << ": " << m.text << '\n';
        } else if (option == 4) imFanLX->queryOnline(fieldFanLX("好友号码"));
        else while (auto m = imFanLX->pollEvent())
            outFanLX << m->type << " " << m->sender << " " << m->text << '\n';
    }
}
int ConsoleAppFanLX::run() {
    outFanLX << "Ciallo 即时通讯系统\n输入 0 返回；输入结束时执行保存检查。\n";
    if (appFanLX.loadResult().status != LoadStatusFanLX::Loaded) {
        try {
            auto status = appFanLX.loadResult().status;
            if (status == LoadStatusFanLX::Empty || status == LoadStatusFanLX::UncommittedOnly) {
                if (choiceFanLX("新目录：1 初始化演示数据 0 退出", 1) != 1) return 0;
                appFanLX.initialize(); appFanLX.importInitial(demoSeedFanLX());
            } else {
                outFanLX << appFanLX.loadResult().message << '\n';
                if (choiceFanLX("恢复会选择上一有效备份，可能丢失最近提交。1 确认恢复 0 退出", 1) != 1) return 0;
                appFanLX.confirmRecovery();
            }
        } catch (const std::exception &e) { outFanLX << "启动未完成：" << e.what() << '\n'; return 1; }
    }
    while (!eofFanLX) try {
        if (actorFanLX)
            outFanLX << "当前 " << serviceDisplayNameFanLX(actorFanLX->service) << " "
                     << actorFanLX->accountId << '\n';
        const auto option =
            choiceFanLX("主菜单：1 登录微X或注册 2 账号与服务 3 好友 4 群 5 缓存统计 6 聊天 0 保存退出", 6);
        if (!option) break;
        if (option == 1) loginFlowFanLX();
        else if (option == 2) accountsFanLX();
        else if (option == 3) friendsFanLX();
        else if (option == 4) groupsFanLX();
        else if (option == 6) chatFanLX();
        else {
            const auto m = appFanLX.queryCacheMetrics();
            outFanLX << "命中=" << m.hits << " 未命中=" << m.misses << " 有效条目=" << m.entries
                     << " 逻辑载荷字节=" << m.bytes << " 池保留字节=" << m.poolReservedBytes << '\n';
        }
    } catch (const std::exception &e) { outFanLX << "操作失败或取消：" << e.what() << '\n'; }
    imFanLX.reset();
    try { appFanLX.saveAndClose(); outFanLX << "保存检查完成，已关闭。\n"; return 0; }
    catch (const std::exception &e) { outFanLX << "保存失败：" << e.what() << '\n'; return 1; }
}

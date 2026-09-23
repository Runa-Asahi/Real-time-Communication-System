#include "network/TcpListenerFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include "ui/ConsoleAppFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "friend/IdentityResolverFanLX.h"
#include <charconv>

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
    switch (choiceFanLX("平台：1 QQ  2 微信  3 微博  0 取消", 3)) {
    case 1: return ServiceTypeFanLX::QQ;
    case 2: return ServiceTypeFanLX::Wechat;
    case 3: return ServiceTypeFanLX::Weibo;
    default: throw std::runtime_error("已取消");
    }
}
AccountRefFanLX ConsoleAppFanLX::accountFanLX() {
    auto service = serviceFanLX();
    return {service, fieldFanLX("平台账号")};
}
AccountRefFanLX ConsoleAppFanLX::currentFanLX() const {
    if (!actorFanLX || !appFanLX.state().accounts.isAccountLoggedIn(actorFanLX->service, actorFanLX->accountId))
        throw std::runtime_error("请先登录当前平台");
    return *actorFanLX;
}
GroupKeyFanLX ConsoleAppFanLX::groupFanLX() {
    const auto actor = currentFanLX();
    if (actor.service == ServiceTypeFanLX::Weibo)
        throw std::runtime_error("当前群策略支持 QQ 和微信，请切换平台");
    return {actor.service == ServiceTypeFanLX::QQ ? GroupModeFanLX::QQ : GroupModeFanLX::Wechat,
            fieldFanLX("群号（使用创建平台的群号）")};
}
void ConsoleAppFanLX::showFanLX(const std::vector<std::string> &values) {
    outFanLX << "查询结果 " << values.size() << " 条\n";
    for (const auto &value : values) outFanLX << "  " << value << '\n';
}
void ConsoleAppFanLX::accountsFanLX() {
    while (!eofFanLX) try {
        const auto option = choiceFanLX("账号：1 本人资料与服务 2 开通服务 3 逐项确认登录 4 绑定 5 解绑 "
            "6 修改资料 7 当前服务注销 8 全平台注销 9 注册 0 返回", 9);
        if (!option) return;
        if (option == 9) {
            auto first = serviceFanLX();
            auto qq = fieldFanLX("QQ/微博共享号码"), wx = fieldFanLX("独立微信号");
            auto nickname = fieldFanLX("昵称"), birth = fieldFanLX("出生日期 YYYY-MM-DD");
            auto location = fieldFanLX("所在地");
            // 内部编号由后台生成，界面不要求用户输入 userId。
            const auto user = "registered-" + qq;
            std::vector<ServiceAccountFanLX> accounts{
                {user, ServiceTypeFanLX::QQ, qq, true}, {user, ServiceTypeFanLX::Weibo, qq, true},
                {user, ServiceTypeFanLX::Wechat, wx, false}};
            std::stable_sort(accounts.begin(), accounts.end(), [first](const auto &a, const auto &b) {
                return a.service == first && b.service != first;
            });
            appFanLX.registerUser({user, nickname, birth, utcNowFanLX().substr(0, 10), location}, accounts);
            outFanLX << "注册已保存；首选服务已开通，其余服务可自主开通。\n";
            continue;
        }
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
        } else if (option == 2) appFanLX.subscribe(actor, serviceFanLX());
        else if (option == 3) appFanLX.confirmLogin(actor, accountFanLX());
        else if (option == 4 || option == 5) appFanLX.bind(actor, accountFanLX(), option == 5);
        else if (option == 6) {
            auto nickname = fieldFanLX("新昵称"), location = fieldFanLX("新所在地");
            appFanLX.updateProfile(actor, nickname, location);
        } else {
            appFanLX.logout(actor, option == 8);
            imFanLX.reset();
            actorFanLX.reset();
        }
        outFanLX << "操作完成\n";
    } catch (const std::exception &e) { outFanLX << "操作失败或取消：" << e.what() << '\n'; }
}
void ConsoleAppFanLX::friendsFanLX() {
    while (!eofFanLX) try {
        const auto option = choiceFanLX("好友：1 列表和备注 2 添加 3 删除 4 备注 5 同平台共同好友 "
            "6 跨平台推荐 7 跨平台共同好友 0 返回", 7);
        if (!option) return;
        const auto actor = currentFanLX();
        if (option == 1) {
            const auto peers = appFanLX.queryFriendAccounts(actor);
            IdentityResolverFanLX resolver(appFanLX.state().accounts);
            const auto own = resolver.accountKey(actor.service, actor.accountId);
            outFanLX << "好友 " << peers.size() << " 人\n";
            for (const auto &peer : peers) {
                const auto detail = appFanLX.state().friends.detail(own, resolver.accountKey(actor.service, peer));
                outFanLX << peer << " 备注=" << detail.remark << " 标签=" << detail.tag << '\n';
            }
        } else if (option >= 6) {
            outFanLX << "选择本人另一个已确认登录的平台账号\n";
            showFanLX(appFanLX.queryCrossService(actor, accountFanLX(), option == 6));
            outFanLX << "需要添加时切换到目标平台，再使用添加好友；不会自动添加。\n";
        } else {
            AccountRefFanLX peer{actor.service, fieldFanLX("对方平台账号")};
            if (option == 2) appFanLX.addFriend(actor, peer);
            else if (option == 3) appFanLX.removeFriend(actor, peer);
            else if (option == 4) {
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
void ConsoleAppFanLX::chatFanLX() {
    const auto actor = currentFanLX();
    outFanLX << "QQ 聊天：中心服务器转发，返回菜单保持长连接。\n";
    if (actor.service != ServiceTypeFanLX::QQ || !imFanLX)
        throw std::runtime_error("请使用 --im-host / --im-port / --im-tokens 配置服务端后登录 QQ");
    while (!eofFanLX) {
        outFanLX << (imFanLX->online() ? "在线" : "连接中") << " 待确认=" << imFanLX->pending()
                 << " " << imFanLX->error() << '\n';
        const auto option = choiceFanLX("聊天：1 单聊发送 2 历史记录 3 群聊发送 4 好友在线查询 5 状态通知 0 返回", 5);
        if (!option) return;
        if (option == 1 || option == 3) {
            const auto target = fieldFanLX(option == 1 ? "好友 QQ 号" : "QQ 群号");
            const auto text = fieldFanLX("消息");
            outFanLX << "已保存待发送：" << imFanLX->send(target, text, option == 3) << '\n';
        } else if (option == 2) {
            for (const auto &m : imFanLX->history())
                outFanLX << (m.scope == "group" ? "群 " : "单聊 ") << m.recipient << " #" << m.messageId
                         << " " << m.sender << ": " << m.text << '\n';
        } else if (option == 4) imFanLX->queryOnline(fieldFanLX("好友 QQ 号"));
        else while (auto m = imFanLX->pollEvent())
            outFanLX << m->type << " " << m->sender << " " << m->text << '\n';
    }
}
int ConsoleAppFanLX::run() {
    outFanLX << "樊陆旭模拟即时通讯系统\n输入 0 返回；输入结束时执行保存检查。\n";
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
        if (actorFanLX) outFanLX << "当前 " << serviceNameFanLX(actorFanLX->service) << " " << actorFanLX->accountId << '\n';
        const auto option = choiceFanLX("主菜单：1 登录或切换账号 2 账号与服务 3 好友 4 群 5 缓存统计 6 QQ聊天 0 保存退出", 6);
        if (!option) break;
        if (option == 1) {
            auto actor = accountFanLX();
            auto confirm = choiceFanLX("确认其它同号或已绑定服务？1 全部确认 0 仅当前", 1);
            if (eofFanLX) break;
            appFanLX.login(actor, confirm == 1); actorFanLX = actor;
            imFanLX.reset();
            if (!imHostFanLX.empty() && actor.service == ServiceTypeFanLX::QQ) {
                const auto tokens = nlohmann::json::parse(AtomicFileWriterFanLX::read(imTokensFanLX));
                std::string safe;
                for (unsigned char ch : actor.accountId) {
                    safe += "0123456789abcdef"[ch >> 4]; safe += "0123456789abcdef"[ch & 15];
                }
                imFanLX = std::make_unique<ImClientFanLX>(imHostFanLX, imPortFanLX, actor.accountId,
                    tokens.at(actor.accountId).get<std::string>(), imDirectoryFanLX / safe);
            }
            outFanLX << "登录成功\n";
        } else if (option == 2) accountsFanLX();
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

#include "friend/IdentityResolverFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/SnapshotCodecFanLX.h"

// 演示数据的登录密码规则：**统一为 123**。
//
// 历史：原规则是 pw-<微X号>（微X号 2026001 的密码是 pw-2026001）。2026-10-10 起改为固定 "123"，
// 原因是演示时要反复手输密码，pw-20260012 这类长口令既难记又容易敲错；而本项目是纯本机演示，
// 口令强度并无实际意义。注意：账号库里口令以 PBKDF2 加盐哈希存储（见 account/PasswordHashFanLX.h），
// 但登录报文仍以明文经 TCP 传输（见 docs/network-validation.md「假设与边界」）。
//
// 这里刻意**保留函数签名并接收 number**（不再使用）：全项目 60 余处调用点、测试与截图工具
// 都通过它取口令，改成常量参数才能让调用点一行都不用动。若将来要恢复"一人一口令"，
// 只需把函数体改回 "pw-" + number，调用点无需修改。
std::string demoPasswordFanLX(const std::string &number) {
    (void)number; // 规则与号码无关；保留形参以兼容既有调用点。
    return "123";
}

PlatformSnapshotFanLX demoSeedFanLX() {
    PlatformStateFanLX state;
    for (int n = 1; n <= 12; ++n) {
        const auto id = "U" + std::to_string(n);
        const auto number = "202600" + std::to_string(n);
        // 注册只创建微X账号（上层身份）：QQ / 微信 / 微博 是随后"在微X里面开通"的下级服务。
        state.accounts.registerWeiXAccount({id, n == 1 ? "樊陆旭" : "同学" + std::to_string(n), "2005-01-01",
                                            "2026-09-01", "吉林", demoPasswordFanLX(number)},
                                           number);
        // QQ 与微博沿用微X号（课程约定：两者共享同一个号码），微信使用独立号并绑定到 QQ。
        state.accounts.activateService(id, ServiceTypeFanLX::QQ, number);
        state.accounts.activateService(id, ServiceTypeFanLX::Weibo, number);
        state.accounts.activateService(id, ServiceTypeFanLX::Wechat, "wx_" + number);
        state.accounts.bindAccounts(ServiceTypeFanLX::Wechat, "wx_" + number, ServiceTypeFanLX::QQ, number);
    }
    for (auto mode : {GroupModeFanLX::QQ, GroupModeFanLX::Wechat})
        for (int n = 1001; n <= 1006; ++n) {
            auto &group = state.groups.create({mode, std::to_string(n)});
            group.addMember("U1", GroupRoleFanLX::Owner);
            group.addMember("U2");
            group.addMember("U3");
        }
    const GroupKeyFanLX qq{GroupModeFanLX::QQ, "1001"};
    state.groups.assignAdmin(qq, "U1", "U2");
    state.groups.createDiscussion(qq, "D1", "U1");
    state.groups.joinDiscussion(qq, "D1", "U1", "U1");
    state.groups.joinDiscussion(qq, "D1", "U1", "U3");
    state.groups.applyToJoin(qq, "U4");
    state.groups.inviteMember({GroupModeFanLX::Wechat, "1001"}, "U1", "U4");
    // 好友只建在"各自的"服务分区里：QQ 一份、微博一份；微信保持为空，
    // 这样既能直接演示"微博除群以外都可用"（有好友、能单聊），又能演示
    // "QQ 的好友不会出现在微信列表里"（分别存储）。
    IdentityResolverFanLX resolver(state.accounts);
    auto left = resolver.accountKey(ServiceTypeFanLX::QQ, "2026001");
    auto right = resolver.accountKey(ServiceTypeFanLX::QQ, "2026002");
    auto &qqFriends = state.friends.directory(ServiceTypeFanLX::QQ);
    qqFriends.add(left, right);
    qqFriends.update(left, right, "课程搭档", "同学");
    qqFriends.update(right, left, "樊同学", "好友");
    auto weiboLeft = resolver.accountKey(ServiceTypeFanLX::Weibo, "2026001");
    auto weiboRight = resolver.accountKey(ServiceTypeFanLX::Weibo, "2026002");
    auto &weiboFriends = state.friends.directory(ServiceTypeFanLX::Weibo);
    weiboFriends.add(weiboLeft, weiboRight);
    weiboFriends.update(weiboLeft, weiboRight, "微博好友", "同学");
    weiboFriends.update(weiboRight, weiboLeft, "樊同学", "好友");
    return SnapshotCodecFanLX::capture(state);
}

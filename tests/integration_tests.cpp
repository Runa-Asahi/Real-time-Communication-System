#ifdef NDEBUG
#undef NDEBUG
#endif
#include "persistence/AtomicFileWriterFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include <cassert>
#include <functional>
#include <iostream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
const AccountRefFanLX owner{ServiceTypeFanLX::QQ, "2026001"};
const AccountRefFanLX admin{ServiceTypeFanLX::QQ, "2026002"};
const AccountRefFanLX member{ServiceTypeFanLX::QQ, "2026003"};
const AccountRefFanLX outsider{ServiceTypeFanLX::QQ, "2026004"};
const GroupKeyFanLX group{GroupModeFanLX::QQ, "1001"};
const AccountRefFanLX wxOwner{ServiceTypeFanLX::Wechat, "wx_2026001"};
template <class FnFanLX> void failsFanLX(FnFanLX fn) {
    bool thrown = false;
    try {
        fn();
    } catch (const std::exception &) {
        thrown = true;
    }
    assert(thrown);
}
// 固定时钟，对同一业务轨迹开/关缓存，比较所有查询和重启全字段快照。
nlohmann::json scenarioFanLX(const std::filesystem::path &directory, bool enabled) {
    nlohmann::json result = nlohmann::json::array(), expected;
    {
        bool failWrite = false;
        PersistenceApplicationFanLX app(
            directory, [] { return std::string("2026-09-23T00:00:00Z"); },
            [&](auto stage) {
                if (failWrite && stage == CommitStageFanLX::BeforePublish)
                    throw std::runtime_error("S10 提交前故障");
            });
        app.initialize();
        app.importInitial(demoSeedFanLX());
        app.setQueryCacheEnabled(enabled);
        for (const auto &actor : {owner, admin, member, outsider})
            app.login(actor, true);
        const auto *address = &app.state().groups.get(group);
        const auto *discussion = &app.state().groups.getDiscussion(group, "D1");
        const auto check = [&] {
            result.push_back(app.queryGroupMembers(owner, group));
            result.push_back(app.queryFriendAccounts(owner));
            result.push_back(app.queryCommonFriends(owner, admin));
            result.push_back(app.queryCrossService(owner, wxOwner));
            assert(&app.state().groups.get(group) == address);
            assert(&app.state().groups.getDiscussion(group, "D1") == discussion);
        };
        check();
        check(); // 先预热，检查命中前鉴权以及拒绝后的缓存正确性。
        const auto rejectUnchanged = [&](const std::function<void()> &operation) {
            const auto snapshot = SnapshotCodecFanLX::encode(app.snapshot());
            const auto disk = AtomicFileWriterFanLX::read(directory / "state.json");
            failsFanLX(operation);
            assert(SnapshotCodecFanLX::encode(app.snapshot()) == snapshot); // 包含代数和局部版本。
            assert(AtomicFileWriterFanLX::read(directory / "state.json") == disk);
            check();
        };
        // S10a：应用事务层的群主/管理员/成员/非成员/跨服务权限矩阵。
        for (const auto &actor : {admin, member, outsider}) {
            rejectUnchanged([&] { app.assignAdmin(actor, group, member); });
            rejectUnchanged([&] { app.transferOwnership(actor, group, member); });
            rejectUnchanged([&] { app.switchPolicy(actor, group, GroupModeFanLX::Wechat); });
            rejectUnchanged([&] { app.dissolve(actor, group); });
        }
        for (const auto &target : {owner, admin, member})
            rejectUnchanged([&] { app.kick(member, group, target); });
        rejectUnchanged([&] { app.kick(admin, group, owner); });
        rejectUnchanged([&] { app.kick(admin, group, admin); });
        rejectUnchanged([&] { app.kick(owner, group, owner); });
        rejectUnchanged([&] { app.leave(owner, group); });
        rejectUnchanged([&] { app.createDiscussion(member, group, "blocked"); });
        rejectUnchanged([&] { app.approveApplication(member, group, outsider); });
        rejectUnchanged([&] { app.queryGroupMembers(outsider, group); });
        rejectUnchanged([&] { app.switchPolicy(wxOwner, group, GroupModeFanLX::Wechat); });
        rejectUnchanged([&] { app.transferOwnership(owner, group, wxOwner); });
        rejectUnchanged([&] { app.bind(owner, {ServiceTypeFanLX::Wechat, "wx_2026002"}); });
        // S10b：写失败不发布候选；成功发布后失效查询且保持群/子群外壳。
        failWrite = true;
        rejectUnchanged([&] { app.approveApplication(owner, group, outsider); });
        failWrite = false;
        app.approveApplication(admin, group, outsider);
        check();
        app.joinDiscussion(admin, group, "D1", outsider);
        app.assignAdmin(owner, group, member);
        rejectUnchanged([&] { app.kick(admin, group, member); });
        app.assignAdmin(owner, group, member, true);
        app.kick(admin, group, outsider);
        assert(!app.state().groups.getDiscussion(group, "D1").hasMember("U4"));
        check();
        const auto beforeSwitch = app.state().groups.get(group).snapshot();
        app.switchPolicy(owner, group, GroupModeFanLX::Wechat);
        assert(app.state().groups.get(group).snapshot() == beforeSwitch);
        rejectUnchanged([&] { app.kick(admin, group, member); });
        rejectUnchanged([&] { app.createDiscussion(owner, group, "blocked"); });
        rejectUnchanged([&] { app.joinDiscussion(owner, group, "D1", admin); });
        rejectUnchanged([&] { app.applyToJoin(outsider, group); });
        app.inviteMember(member, group, outsider);
        app.confirmInvitation(outsider, group);
        check();
        app.leave(outsider, group);
        app.switchPolicy(owner, group, GroupModeFanLX::QQ);
        check();
        app.removeFriend(owner, admin);
        check();
        app.addFriend(owner, admin);
        app.updateRemark(owner, admin, "集成中文备注", "验收");
        check();
        app.bind(admin, {ServiceTypeFanLX::Wechat, "wx_2026002"}, true);
        check();
        app.bind(admin, {ServiceTypeFanLX::Wechat, "wx_2026002"});
        check();
        app.logout(owner, true);
        failsFanLX([&] { app.queryGroupMembers(owner, group); });
        failsFanLX([&] { app.queryFriendAccounts(owner); });
        app.login(owner, true);
        app.transferOwnership(owner, group, admin);
        app.leave(owner, group);
        failsFanLX([&] { app.queryGroupMembers(owner, group); });
        app.dissolve(admin, group);
        failsFanLX([&] { app.state().groups.getDiscussion(group, "D1"); });
        app.createGroup(owner, group); // 同键重建不能返回旧群缓存。
        assert(app.queryGroupMembers(owner, group) == std::vector<std::string>{"2026001"});
        result.push_back(app.queryGroupMembers(owner, group));
        expected = SnapshotCodecFanLX::encode(app.snapshot());
        app.saveAndClose();
    }
    {
        PersistenceApplicationFanLX app(directory);
        assert(app.loadResult().status == LoadStatusFanLX::Loaded);
        assert(SnapshotCodecFanLX::encode(app.snapshot()) == expected);
        assert(!app.state().accounts.isAccountLoggedIn(owner.service, owner.accountId));
        assert(app.queryCacheMetrics().entries == 0);
        result.push_back(expected);
        app.saveAndClose();
    }
    return result;
}
} // namespace
int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("fanlx-integration-" + std::to_string(GetCurrentProcessId()) + "-" +
                       std::to_string(GetTickCount64()));
    try {
        const auto withoutCache = scenarioFanLX(root / "off", false);
        const auto withCache = scenarioFanLX(root / "on", true);
        assert(withoutCache == withCache);
        std::filesystem::remove_all(root);
        std::cout << "S10: permissions, failure isolation, cache equivalence and restart passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << " evidence=" << root.u8string() << '\n';
        return 1;
    }
}

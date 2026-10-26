#ifdef NDEBUG
#undef NDEBUG
#endif
#include "persistence/AtomicFileWriterFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include "persistence/PersistenceErrorFanLX.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
const AccountRefFanLX owner{ServiceTypeFanLX::QQ, "2026001"};
const AccountRefFanLX admin{ServiceTypeFanLX::QQ, "2026002"};
const AccountRefFanLX third{ServiceTypeFanLX::QQ, "2026003"};
const AccountRefFanLX fourth{ServiceTypeFanLX::QQ, "2026004"};
const GroupKeyFanLX qq{GroupModeFanLX::QQ, "1001"};
std::string fixedTimeFanLX() { return "2026-09-14T12:00:00Z"; }
template <class FnFanLX> void failsFanLX(FnFanLX fn) {
    bool thrown = false;
    try {
        fn();
    } catch (const std::exception &) {
        thrown = true;
    }
    assert(thrown);
}
void overwriteFanLX(const std::filesystem::path &p, const std::string &text) {
    std::ofstream out(p, std::ios::binary);
    out << text;
    out.close();
    assert(out.good());
}
std::wstring executableFanLX() {
    wchar_t path[32768];
    DWORD n = GetModuleFileNameW(nullptr, path, 32768);
    assert(n && n < 32768);
    return std::wstring(path, n);
}
DWORD childFanLX(const std::wstring &args) {
    auto executable = executableFanLX();
    std::wstring command = L"\"" + executable + L"\" " + args;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    assert(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                          nullptr, nullptr, &startup, &process));
    DWORD wait = WaitForSingleObject(process.hProcess, 15000);
    if (wait != WAIT_OBJECT_0)
        TerminateProcess(process.hProcess, 98);
    assert(wait == WAIT_OBJECT_0);
    DWORD result = 0;
    assert(GetExitCodeProcess(process.hProcess, &result));
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}
void initializeFanLX(const std::filesystem::path &dir) {
    PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
    app.initialize();
    app.importInitial(demoSeedFanLX());
    app.saveAndClose();
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#if defined(_MSC_VER) || defined(_UCRT)
    // 旧 msvcrt 版 MinGW（如 CI 的 mingw1310）不导出该函数，只在 UCRT/MSVC 下调用。
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    // 辅助进程只访问父测试传入的专用临时目录；故障退出不运行析构。
    if (argc >= 3) {
        const std::filesystem::path dir = std::filesystem::path(argv[2]);
        if (std::wstring(argv[1]) == L"lock") {
            try {
                JsonFileRepositoryFanLX repo(dir);
                return 2;
            } catch (const PersistenceErrorFanLX &e) {
                return e.code == PersistenceErrorCodeFanLX::Locked ? 0 : 3;
            }
        }
        if (std::wstring(argv[1]) == L"crash") {
            int wanted = std::stoi(argv[3]);
            CommitHookFanLX hook = [&](CommitStageFanLX stage) {
                if (static_cast<int>(stage) == wanted)
                    TerminateProcess(GetCurrentProcess(), 77);
            };
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX, hook);
            app.login(owner, demoPasswordFanLX(owner.accountId));
            app.switchPolicy(owner, qq, GroupModeFanLX::Wechat);
            return 4;
        }
        if (std::wstring(argv[1]) == L"restart") {
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
            assert(app.loadResult().status == LoadStatusFanLX::Loaded);
            assert(!app.state().accounts.isAccountLoggedIn(owner.service, owner.accountId));
            assert(app.state().groups.get(qq).currentMode() == GroupModeFanLX::Wechat);
            app.saveAndClose();
            return 0;
        }
    }
    auto root = std::filesystem::temp_directory_path() /
                ("fanlx-persistence-" + std::to_string(GetCurrentProcessId()) + "-" +
                 std::to_string(GetTickCount64()));
    std::filesystem::create_directories(root);
    try {
        const auto fixtures = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "persistence";
        assert(SnapshotCodecFanLX::parse(AtomicFileWriterFanLX::read(fixtures / "valid-state-v2.json"))
                   .users.size() == 12);
        // 账号库改造的迁移边界：本次之前的 v1 文件没有用户密码字段。这种文件必须被明确拒绝，
        // 并且错误文本要指出重新 --init-demo，而不是加载出一个"谁也用密码登不进去"的账号库。
        {
            auto legacy = nlohmann::json::parse(
                AtomicFileWriterFanLX::read(fixtures / "valid-state-v2.json"));
            for (auto &user : legacy["users"])
                user.erase("password");
            assert(legacy["users"][0].find("password") == legacy["users"][0].end());
            bool rejected = false;
            try {
                SnapshotCodecFanLX::parse(legacy.dump());
            } catch (const PersistenceErrorFanLX &error) {
                rejected = std::string(error.what()).find("缺少登录密码") != std::string::npos;
            }
            assert(rejected);
        }
        failsFanLX([&] {
            SnapshotCodecFanLX::parse(AtomicFileWriterFanLX::read(fixtures / "duplicate-property.json"));
        });
        failsFanLX([&] {
            SnapshotCodecFanLX::parse(AtomicFileWriterFanLX::read(fixtures / "unsupported-schema.json"));
        });
        // 本次改造的第二个迁移边界：v1（好友与群是一个扁平数组）已不再支持，
        // 必须明确报 schemaVersion 并要求重新初始化，而不是把旧结构读成"没有好友"。
        {
            bool rejected = false;
            try {
                SnapshotCodecFanLX::parse(AtomicFileWriterFanLX::read(fixtures / "valid-state-v1.json"));
            } catch (const PersistenceErrorFanLX &error) {
                rejected = std::string(error.what()).find("schemaVersion") != std::string::npos;
            }
            assert(rejected);
        }
        // T11：完整字段、幂等、事务门面、稳定地址、S05 保存后切换回原策略。
        auto basic = root / L"中文往返";
        initializeFanLX(basic);
        nlohmann::json expected;
        {
            PersistenceApplicationFanLX app(basic, fixedTimeFanLX);
            app.login(owner, demoPasswordFanLX(owner.accountId), true);
            app.login(admin, demoPasswordFanLX(admin.accountId));
            app.login(third, demoPasswordFanLX(third.accountId));
            app.login(fourth, demoPasswordFanLX(fourth.accountId));
            const auto *address = &app.state().groups.get(qq);
            const auto *childAddress = &app.state().groups.getDiscussion(qq, "D1");
            const auto before = app.snapshot();
            assert(before.groups.size() == 12 && before.users.size() == 12);
            // 演示数据有两对好友关系：QQ 一份、微博一份（微信保持为空，用于演示"分别存储"）。
            assert(before.bindings.size() == 12 && before.friendships.size() == 2);
            const auto generation = app.state().generation;
            app.applyToJoin(fourth, qq);
            assert(app.state().generation ==
                   generation); // pending already exists, timestamp supplied by import
            app.approveApplication(owner, qq, fourth);
            assert(app.state().groups.get(qq).hasMember("U4"));
            app.createDiscussion(owner, qq, "D2");
            app.joinDiscussion(owner, qq, "D2", fourth);
            app.leave(fourth, qq);
            app.updateRemark(owner, admin, "新的中文备注", "课程");
            app.addFriend(owner, third);
            app.removeFriend(owner, third);
            app.updateProfile(owner, "同学1", "长春");
            app.bind(owner, {ServiceTypeFanLX::Wechat, "wx_2026001"});
            app.bind(owner, {ServiceTypeFanLX::Wechat, "wx_2026001"}, true);
            app.bind(owner, {ServiceTypeFanLX::Wechat, "wx_2026001"});
            app.subscribe(owner, ServiceTypeFanLX::Weibo);
            app.renameGroup(owner, qq, "课程群");
            app.assignAdmin(owner, qq, third);
            app.assignAdmin(owner, qq, third, true);
            app.switchPolicy(owner, qq, GroupModeFanLX::Wechat);
            assert(&app.state().groups.get(qq) == address);
            assert(&app.state().groups.getDiscussion(qq, "D1") == childAddress);
            failsFanLX([&] { app.kick(admin, qq, third); });
            failsFanLX([&] { app.createDiscussion(owner, qq, "forbidden"); });
            app.inviteMember(owner, qq, fourth);
            app.confirmInvitation(fourth, qq);
            assert(app.state().groups.get(qq).roleOf("U2") == GroupRoleFanLX::Admin);
            expected = SnapshotCodecFanLX::encode(app.snapshot());
            app.saveAndClose();
        }
        assert(childFanLX(L"restart \"" + basic.wstring() + L"\"") == 0);
        {
            PersistenceApplicationFanLX app(basic, fixedTimeFanLX);
            assert(SnapshotCodecFanLX::encode(app.snapshot()) == expected);
            app.login(owner, demoPasswordFanLX(owner.accountId));
            app.login(admin, demoPasswordFanLX(admin.accountId));
            app.login(fourth, demoPasswordFanLX(fourth.accountId));
            app.switchPolicy(owner, qq, GroupModeFanLX::QQ);
            app.kick(admin, qq, fourth);
            app.transferOwnership(owner, qq, admin);
            app.leave(owner, qq);
            app.dissolve(admin, qq);
            assert(app.state().groups.archivedGroupCount() == 1);
            app.saveAndClose();
        }
        {
            PersistenceApplicationFanLX app(basic, fixedTimeFanLX);
            assert(app.state().groups.archivedGroupCount() == 1);
        }

        // T12：各提交点失败都检查在线内容、群地址、版本以及磁盘代数。
        for (auto stage :
             {CommitStageFanLX::BeforeWrite, CommitStageFanLX::DuringWrite, CommitStageFanLX::BeforeFlush,
              CommitStageFanLX::AfterTemporary, CommitStageFanLX::BeforeBackup, CommitStageFanLX::AfterBackup,
              CommitStageFanLX::BeforePublish}) {
            auto dir = root / ("failure-" + std::to_string(static_cast<int>(stage)));
            initializeFanLX(dir);
            bool enabled = true;
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX, [&](auto at) {
                if (enabled && at == stage)
                    throw std::runtime_error("injected I/O failure");
            });
            app.login(owner, demoPasswordFanLX(owner.accountId));
            auto before = SnapshotCodecFanLX::encode(app.snapshot());
            const auto *policy = &app.state().groups.get(qq).policy();
            const auto disk = AtomicFileWriterFanLX::read(dir / L"state.json");
            failsFanLX([&] { app.switchPolicy(owner, qq, GroupModeFanLX::Wechat); });
            assert(before == SnapshotCodecFanLX::encode(app.snapshot()));
            assert(policy == &app.state().groups.get(qq).policy());
            assert(disk == AtomicFileWriterFanLX::read(dir / L"state.json"));
            enabled = false;
            app.saveAndClose();
        }
        // 发布后抛异常不报告回滚：重新核验实际主文件并完成内存发布。
        {
            auto dir = root / L"after-publish";
            initializeFanLX(dir);
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX, [](auto stage) {
                if (stage == CommitStageFanLX::AfterPublish)
                    throw std::runtime_error("after commit");
            });
            app.login(owner, demoPasswordFanLX(owner.accountId));
            app.switchPolicy(owner, qq, GroupModeFanLX::Wechat);
            assert(app.state().groups.get(qq).currentMode() == GroupModeFanLX::Wechat);
        }
        // T12f/h：真实子进程中断，OS 释放锁；按提交点选择旧/新代。
        for (auto stage : {CommitStageFanLX::AfterTemporary, CommitStageFanLX::AfterBackup,
                           CommitStageFanLX::BeforePublish, CommitStageFanLX::AfterPublish,
                           CommitStageFanLX::BeforeMemoryPublish, CommitStageFanLX::BeforeReply}) {
            auto dir = root / ("crash-" + std::to_string(static_cast<int>(stage)));
            initializeFanLX(dir);
            assert(childFanLX(L"crash \"" + dir.wstring() + L"\" " +
                              std::to_wstring(static_cast<int>(stage))) == 77);
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
            const bool committed =
                static_cast<int>(stage) >= static_cast<int>(CommitStageFanLX::AfterPublish);
            assert(app.state().groups.get(qq).currentMode() ==
                   (committed ? GroupModeFanLX::Wechat : GroupModeFanLX::QQ));
            assert(app.state().generation == (committed ? 3u : 2u));
            assert(childFanLX(L"lock \"" + dir.wstring() + L"\"") == 0);
        }
        // T12b/i：损坏保护、确认前只读、恢复不能用坏主文件覆盖有效备份。
        {
            auto dir = root / L"recover";
            initializeFanLX(dir);
            const auto prev = AtomicFileWriterFanLX::read(dir / L"state.json.prev");
            overwriteFanLX(dir / L"state.json", "broken");
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
            assert(app.loadResult().status == LoadStatusFanLX::RecoveredReadOnly);
            failsFanLX([&] { app.initialize(); });
            failsFanLX([&] { app.registerUser({"new", "n", "2000-01-01", "2026-01-01", "x", "pw-new"}, {}); });
            assert(AtomicFileWriterFanLX::read(dir / L"state.json") == "broken");
            app.confirmRecovery();
            assert(AtomicFileWriterFanLX::read(dir / L"state.json.prev") == prev);
            bool preserved = false, recorded = false;
            for (const auto &file : std::filesystem::directory_iterator(dir)) {
                const auto name = file.path().filename().u8string();
                if (name.find("state.damaged.") == 0) {
                    assert(AtomicFileWriterFanLX::read(file.path()) == "broken");
                    preserved = true;
                }
                if (name.find("recovery.") == 0) {
                    const auto record = nlohmann::json::parse(AtomicFileWriterFanLX::read(file.path()));
                    assert(record.at("sourceGeneration") == 1u);
                    assert(record.at("oldGeneration").is_null());
                    assert(!record.at("time").get<std::string>().empty());
                    recorded = true;
                }
            }
            assert(preserved && recorded);
            app.importInitial(demoSeedFanLX());
            app.saveAndClose();
        }
        {
            auto dir = root / L"double-bad";
            initializeFanLX(dir);
            overwriteFanLX(dir / L"state.json", "bad");
            overwriteFanLX(dir / L"state.json.prev", "bad-prev");
            PersistenceApplicationFanLX app(dir);
            assert(app.loadResult().status == LoadStatusFanLX::Failed);
            failsFanLX([&] { app.initialize(); });
            assert(AtomicFileWriterFanLX::read(dir / L"state.json.prev") == "bad-prev");
        }
        {
            auto dir = root / L"unknown";
            initializeFanLX(dir);
            auto j = nlohmann::json::parse(AtomicFileWriterFanLX::read(dir / L"state.json"));
            j["schemaVersion"] = 999u;
            overwriteFanLX(dir / L"state.json", j.dump());
            PersistenceApplicationFanLX app(dir);
            assert(app.loadResult().status == LoadStatusFanLX::UnsupportedSchema);
            failsFanLX([&] { app.initialize(); });
        }
        {
            auto dir = root / L"tmp-only";
            std::filesystem::create_directories(dir);
            overwriteFanLX(dir / L".tmp.orphan", "not committed");
            PersistenceApplicationFanLX app(dir);
            assert(app.loadResult().status == LoadStatusFanLX::UncommittedOnly);
            app.initialize();
            app.saveAndClose();
        }
        // T12c：非法结构/引用/版本不得构建在线对象。
        auto valid = SnapshotCodecFanLX::encode(demoSeedFanLX());
        for (int i = 0; i < 12; ++i) {
            auto bad = valid;
            if (i == 0)
                bad["users"][0]["userId"] = "";
            if (i == 1)
                bad["accounts"][0]["userId"] = "absent";
            if (i == 2)
                bad["groups"]["QQ"][0]["members"][0]["role"] = "Member";
            if (i == 3)
                bad["groups"]["QQ"][0]["members"][1]["role"] = "Owner";
            if (i == 4)
                bad["groups"]["QQ"][0]["nextJoinOrder"] = 0u;
            if (i == 5)
                bad["groups"]["QQ"][0]["members"][1]["joinOrder"] = 0u;
            if (i == 6)
                bad["users"][0]["birthDate"] = "2001-02-29";
            if (i == 7)
                bad["subscriptions"].push_back(bad["subscriptions"][0]);
            if (i == 8)
                bad["generation"] = -1;
            if (i == 9)
                bad["groups"]["QQ"][0]["currentMode"] = "Unknown";
            if (i == 10)
                bad["friends"]["QQ"][0]["right"]["accountId"] = "missing";
            if (i == 11)
                bad["groups"]["QQ"][0]["discussions"][0]["members"].push_back("U12");
            failsFanLX([&] { SnapshotCodecFanLX::parse(bad.dump()); });
        }
        failsFanLX([&] { SnapshotCodecFanLX::parse("{\"schemaVersion\":1,\"schemaVersion\":1}"); });
        failsFanLX([&] { SnapshotCodecFanLX::parse(std::string(40, '[') + std::string(40, ']')); });
        failsFanLX([&] { SnapshotCodecFanLX::parse("{\"x\":\"\xff\"}"); });
        // T11e：补齐注册、服务开通、拒绝流程与未知时间的持久化边界。
        {
            auto dir = root / L"commands";
            initializeFanLX(dir);
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
            app.login(owner, demoPasswordFanLX(owner.accountId));
            app.login(fourth, demoPasswordFanLX(fourth.accountId));
            app.approveApplication(owner, qq, fourth, true);
            assert(!app.state().groups.get(qq).hasMember("U4"));
            app.applyToJoin(fourth, qq);
            app.approveApplication(owner, qq, fourth);
            app.leave(fourth, qq);
            app.switchPolicy(owner, qq, GroupModeFanLX::Wechat);
            app.inviteMember(owner, qq, fourth);
            app.confirmInvitation(fourth, qq, true);
            assert(!app.state().groups.get(qq).hasMember("U4"));
            const AccountRefFanLX fresh{ServiceTypeFanLX::QQ, "0099"};
            // 注册时用户自选密码，随后用「用户名 + 密码」登录；不再是"写完就能登"。
            app.registerUser({"U99", "新注册", "2000-01-01", "2026-09-14", "吉林", "pw-new"},
                             {{"U99", ServiceTypeFanLX::QQ, "0099", true},
                              {"U99", ServiceTypeFanLX::Wechat, "wx0099", false}});
            failsFanLX([&] { app.login(fresh, "wrong-pw"); });   // 密码不符必须登录不上
            app.login(fresh, "pw-new");
            assert(!app.state().accounts.isSubscribed("U99", ServiceTypeFanLX::Wechat));
            app.subscribe(fresh, ServiceTypeFanLX::Wechat);
            app.createGroup(fresh, {GroupModeFanLX::QQ, "new"});
            // 改密后旧密码立即失效，新密码可用
            app.changePassword(fresh, "pw-new", "pw-next");
            app.logout(fresh, true);
            failsFanLX([&] { app.login(fresh, "pw-new"); });
            app.login(fresh, "pw-next");
            app.logout(fresh, true);
            failsFanLX([&] { app.updateProfile(fresh, "bad", "bad"); });
            app.saveAndClose();
        }
        // 本次改造：微X 是 QQ/微信/微博 的上层账号。
        //   · 注册只创建微X（不捆绑任何下级服务）；
        //   · 只有"已登录微X"的会话才能开通下级服务；
        //   · 号码按服务各自唯一：QQ 与微博必须共用同一个号码，不同服务之间允许同号
        //     （服务端按"服务 + 号码"分通道，客户端聊天库也按服务分目录）。
        {
            auto dir = root / L"weix";
            initializeFanLX(dir);
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
            const AccountRefFanLX weix{ServiceTypeFanLX::WeiX, "500001"};
            app.registerWeiXUser({"U500", "微X用户", "2000-01-01", "2026-10-01", "吉林", "pw-500"},
                                 "500001");
            {
                const auto snapshot = app.snapshot();
                std::size_t owned = 0;
                for (const auto &account : snapshot.accounts)
                    if (account.userId == "U500") {
                        ++owned;
                        assert(account.service == ServiceTypeFanLX::WeiX); // 注册不捆绑任何服务账号
                    }
                assert(owned == 1);
                // 微X 自身注册即开通（否则注册完无法登录），下级服务一个都没开通。
                assert(app.state().accounts.isSubscribed("U500", ServiceTypeFanLX::WeiX));
                assert(!app.state().accounts.isSubscribed("U500", ServiceTypeFanLX::QQ));
                assert(!app.state().accounts.isSubscribed("U500", ServiceTypeFanLX::Wechat));
                assert(!app.state().accounts.isSubscribed("U500", ServiceTypeFanLX::Weibo));
            }
            // 没有登录微X：开通请求必须被拒（"只有登录微X后才能开通三种服务"）。
            failsFanLX([&] { app.activateService(weix, ServiceTypeFanLX::QQ, "500001"); });
            // 用微X号登录后逐个开通。
            app.login(weix, "pw-500", true);
            app.activateService(weix, ServiceTypeFanLX::QQ, "500001");
            // QQ 与微博必须同号：先给微博一个不同的号码必须被拒。
            failsFanLX([&] { app.activateService(weix, ServiceTypeFanLX::Weibo, "999999"); });
            app.activateService(weix, ServiceTypeFanLX::Weibo, "500001");
            app.activateService(weix, ServiceTypeFanLX::Wechat, "wx_500001");
            assert(app.state().accounts.isAccountSubscribed(ServiceTypeFanLX::QQ, "500001"));
            assert(app.state().accounts.isAccountSubscribed(ServiceTypeFanLX::Weibo, "500001"));
            assert(app.state().accounts.isAccountSubscribed(ServiceTypeFanLX::Wechat, "wx_500001"));
            assert(app.state().accounts.accountByAccountId(ServiceTypeFanLX::Weibo, "500001").sharedId);
            assert(!app.state().accounts.accountByAccountId(ServiceTypeFanLX::Wechat, "wx_500001").sharedId);
            // 重复开通同一个服务被拒；微X 自己不能被"开通"。
            failsFanLX([&] { app.activateService(weix, ServiceTypeFanLX::QQ, "500001"); });
            failsFanLX([&] { app.activateService(weix, ServiceTypeFanLX::WeiX, "500002"); });
            // 另一个人的微信可以使用与 QQ 相同的号码：号码只在**各自服务内**唯一，
            // 服务端按"服务 + 号码"分通道，客户端存储目录也带服务名，因此不会串消息。
            app.registerWeiXUser({"U501", "另一位", "2000-01-01", "2026-10-01", "吉林", "pw-501"},
                                 "500002");
            const AccountRefFanLX otherWeix{ServiceTypeFanLX::WeiX, "500002"};
            app.login(otherWeix, "pw-501", true);
            app.activateService(otherWeix, ServiceTypeFanLX::QQ, "500002");
            app.activateService(otherWeix, ServiceTypeFanLX::Wechat, "500002");
            assert(app.state().accounts.hasAccount(ServiceTypeFanLX::QQ, "500002"));
            assert(app.state().accounts.hasAccount(ServiceTypeFanLX::Wechat, "500002"));
            // 微博也能开通并直接单聊（没有群）；同一用户允许 QQ 与微博共号（课程约定）。
            app.activateService(otherWeix, ServiceTypeFanLX::Weibo, "500002");
            assert(app.state().accounts.accountByAccountId(ServiceTypeFanLX::Weibo, "500002").sharedId);
            // 好友只能在同一服务内建立：QQ 身份不能把微信账号加成好友（跨分区关系写不进去）。
            failsFanLX([&] { app.addFriend({ServiceTypeFanLX::QQ, "500001"},
                                          {ServiceTypeFanLX::Wechat, "wx_500001"}); });
            app.saveAndClose();
            // 重新打开：微X 与三个下级服务都还在（分区格式能落盘也能读回）。
            PersistenceApplicationFanLX reopened(dir);
            assert(reopened.state().accounts.hasAccount(ServiceTypeFanLX::WeiX, "500001"));
            assert(reopened.state().accounts.isAccountSubscribed(ServiceTypeFanLX::QQ, "500001"));
            reopened.saveAndClose();
        }
        // 候选中已经修改，再抛异常，在线状态与文件依然保持原样。
        {
            auto dir = root / L"candidate";
            JsonFileRepositoryFanLX repo(dir);
            repo.load();
            repo.initialize();
            PlatformStateFanLX state;
            TransactionCoordinatorFanLX tx(repo, state, fixedTimeFanLX);
            tx.execute([](auto &s) { s = SnapshotCodecFanLX::build(demoSeedFanLX()); }, true);
            const auto before = SnapshotCodecFanLX::encode(SnapshotCodecFanLX::capture(state));
            const auto disk = AtomicFileWriterFanLX::read(dir / L"state.json");
            auto invalidSnapshot = SnapshotCodecFanLX::capture(state);
            invalidSnapshot.parentGeneration = invalidSnapshot.generation;
            ++invalidSnapshot.generation;
            invalidSnapshot.groups.front().currentMode = static_cast<GroupModeFanLX>(99);
            failsFanLX([&] { repo.commit(invalidSnapshot); });
            assert(AtomicFileWriterFanLX::read(dir / L"state.json") == disk);
            failsFanLX([&] {
                tx.execute([](auto &s) {
                    s.groups.switchToWechat(qq, "U1");
                    throw std::bad_alloc();
                });
            });
            assert(SnapshotCodecFanLX::encode(SnapshotCodecFanLX::capture(state)) == before);
            assert(AtomicFileWriterFanLX::read(dir / L"state.json") == disk);
            failsFanLX([&] {
                tx.execute([](auto &s) { s.accounts.updateProfile("U1", std::string("\xff"), "x"); });
            });
            assert(AtomicFileWriterFanLX::read(dir / L"state.json") == disk);
        }
        // 有效主文件被外部改写时阻断，不能用当前内存静默覆盖。
        {
            auto dir = root / L"external";
            initializeFanLX(dir);
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
            app.login(owner, demoPasswordFanLX(owner.accountId));
            overwriteFanLX(dir / L"state.json", "external corruption");
            failsFanLX([&] { app.switchPolicy(owner, qq, GroupModeFanLX::Wechat); });
            failsFanLX([&] { app.updateProfile(owner, "bad", "bad"); });
            assert(AtomicFileWriterFanLX::read(dir / L"state.json") == "external corruption");
        }
        // 恢复发布失败不动有效备份，再次显式确认仍能恢复。
        {
            auto dir = root / L"recovery-failure";
            initializeFanLX(dir);
            overwriteFanLX(dir / L"state.json", "damaged");
            bool enabled = true;
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX, [&](auto stage) {
                if (enabled && stage == CommitStageFanLX::BeforePublish)
                    throw std::runtime_error("recovery failure");
            });
            const auto backup = AtomicFileWriterFanLX::read(dir / L"state.json.prev");
            failsFanLX([&] { app.confirmRecovery(); });
            assert(AtomicFileWriterFanLX::read(dir / L"state.json.prev") == backup);
            assert(AtomicFileWriterFanLX::read(dir / L"state.json") == "damaged");
            enabled = false;
            app.confirmRecovery();
            app.saveAndClose();
        }
        // 恢复发布后若再次校验失败，关闭应用写入口，不能以旧内存覆盖恢复结果。
        {
            auto dir = root / L"recovery-reload-failure";
            initializeFanLX(dir);
            overwriteFanLX(dir / L"state.json", "damaged");
            const auto backup = AtomicFileWriterFanLX::read(dir / L"state.json.prev");
            {
                PersistenceApplicationFanLX app(dir, fixedTimeFanLX, [&](auto stage) {
                    if (stage == CommitStageFanLX::AfterPublish)
                        overwriteFanLX(dir / L"state.json", "changed-after-recovery");
                });
                failsFanLX([&] { app.confirmRecovery(); });
                failsFanLX([&] { app.saveAndClose(); });
                failsFanLX([&] { app.importInitial(demoSeedFanLX()); });
                assert(AtomicFileWriterFanLX::read(dir / L"state.json.prev") == backup);
            }
            PersistenceApplicationFanLX restarted(dir, fixedTimeFanLX);
            restarted.confirmRecovery();
            restarted.saveAndClose();
        }
        // OS 实际拒绝写入：只读目标使主文件替换失败；不是“磁盘写满”模拟。
        {
            auto dir = root / L"read-only";
            initializeFanLX(dir);
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
            app.login(owner, demoPasswordFanLX(owner.accountId));
            const auto file = dir / L"state.json";
            const auto before = AtomicFileWriterFanLX::read(file);
            assert(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY));
            failsFanLX([&] { app.switchPolicy(owner, qq, GroupModeFanLX::Wechat); });
            assert(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL));
            assert(AtomicFileWriterFanLX::read(file) == before);
            app.saveAndClose();
        }
        // 主文件缺失与较新备份不是空目录；代数矛盾不能自动选择最大代。
        {
            auto dir = root / L"missing-main";
            initializeFanLX(dir);
            std::filesystem::remove(dir / L"state.json");
            PersistenceApplicationFanLX app(dir);
            assert(app.loadResult().status == LoadStatusFanLX::RecoveredReadOnly);
            app.confirmRecovery();
            app.saveAndClose();
        }
        {
            auto dir = root / L"generation";
            initializeFanLX(dir);
            auto newer = nlohmann::json::parse(AtomicFileWriterFanLX::read(dir / L"state.json"));
            newer["generation"] = 9u;
            newer["parentGeneration"] = 8u;
            overwriteFanLX(dir / L"state.json.prev", newer.dump());
            PersistenceApplicationFanLX app(dir);
            assert(app.loadResult().status == LoadStatusFanLX::Failed);
        }
        // 恢复后的过期申请不自动批准，关闭过期记录作为新的候选事务提交。
        {
            auto dir = root / L"expired";
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX);
            app.initialize();
            auto seed = demoSeedFanLX();
            seed.groups.front().applications.front().createdAtUtc = "2026-09-01T00:00:00Z";
            seed.groups.front().applications.front().expiresAtUtc = "2026-09-02T00:00:00Z";
            app.importInitial(seed);
            app.login(owner, demoPasswordFanLX(owner.accountId));
            app.approveApplication(owner, qq, fourth);
            assert(!app.state().groups.get(qq).hasMember("U4"));
            assert(app.snapshot().groups.front().applications.front().status == "Expired");
        }
        // 提交途中目标被改变且未等于候选：不能把“不确定”当作已回滚或继续写。
        {
            auto dir = root / L"indeterminate";
            initializeFanLX(dir);
            PersistenceApplicationFanLX app(dir, fixedTimeFanLX, [&](auto stage) {
                if (stage == CommitStageFanLX::BeforePublish) {
                    overwriteFanLX(dir / L"state.json", "tampered-during-commit");
                    throw std::runtime_error("interrupted");
                }
            });
            app.login(owner, demoPasswordFanLX(owner.accountId));
            const auto before = SnapshotCodecFanLX::encode(app.snapshot());
            bool indeterminate = false;
            try {
                app.switchPolicy(owner, qq, GroupModeFanLX::Wechat);
            } catch (const PersistenceErrorFanLX &e) {
                indeterminate = e.code == PersistenceErrorCodeFanLX::CommitIndeterminate;
            }
            assert(indeterminate && SnapshotCodecFanLX::encode(app.snapshot()) == before);
            failsFanLX([&] { app.updateProfile(owner, "blocked", "blocked"); });
        }
        std::cout << "T11/T12: round-trip, transactions, recovery, faults and "
                     "subprocess crash checks passed\n";
        // 删除范围仅限本测试创建的唯一临时目录。
        std::filesystem::remove_all(root);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << " | evidence: " << root.u8string() << '\n';
        return 1;
    }
}

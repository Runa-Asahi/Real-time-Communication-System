#include "infra/ArcCacheFanLX.h"
#include "network/TcpListenerFanLX.h"
#include "persistence/AtomicFileWriterFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include "ui/ConsoleAppFanLX.h"
#include <chrono>
#include <iostream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
std::atomic<bool> stopImFanLX{false};
BOOL WINAPI stopImHandlerFanLX(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) { stopImFanLX = true; return TRUE; }
    return FALSE;
}
void demoFanLX() {
    // 每次演示使用新目录，保留实测快照用于复查，不触碰默认 data。
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("fanlx-demo-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    const AccountRefFanLX owner{ServiceTypeFanLX::QQ, "2026001"};
    const AccountRefFanLX applicant{ServiceTypeFanLX::QQ, "2026004"};
    const GroupKeyFanLX key{GroupModeFanLX::QQ, "1001"};
    nlohmann::json saved;
    {
        PersistenceApplicationFanLX app(directory);
        app.initialize();
        app.importInitial(demoSeedFanLX());
        app.login(owner, true);
        const AccountRefFanLX wxOwner{ServiceTypeFanLX::Wechat, "wx_2026001"};
        const AccountRefFanLX wxPeer{ServiceTypeFanLX::Wechat, "wx_2026002"};
        std::cout << "D01 loaded users=" << app.snapshot().users.size() << " groups=" << app.snapshot().groups.size() << '\n';
        std::cout << "D02 linked Wechat login=" << app.state().accounts.isAccountLoggedIn(wxOwner.service, wxOwner.accountId) << '\n';
        const auto recommended = app.queryCrossService(owner, wxOwner);
        if (recommended != std::vector<std::string>{"wx_2026002"}) throw std::runtime_error("推荐不匹配");
        app.login(wxPeer);
        app.addFriend(wxOwner, wxPeer);
        std::cout << "D03 recommendations=" << recommended.size() << " target friends=" << app.queryFriendAccounts(wxOwner).size() << '\n';
        app.login(applicant);
        app.approveApplication(owner, key, applicant);
        std::cout << "D04 QQ approval members=" << app.queryGroupMembers(owner, key).size() << '\n';
        app.createDiscussion(owner, key, "D-demo");
        app.joinDiscussion(owner, key, "D-demo", applicant);
        app.leave(applicant, key);
        if (app.state().groups.getDiscussion(key, "D-demo").hasMember("U4"))
            throw std::runtime_error("父群退出未同步子群");
        std::cout << "D05 child cleanup=true\n";
        app.updateRemark(owner, {ServiceTypeFanLX::QQ, "2026002"}, "已保存的课程搭档", "同学");
        const auto before = app.state().groups.get(key).snapshot();
        const auto *address = &app.state().groups.get(key);
        app.switchPolicy(owner, key, GroupModeFanLX::Wechat);
        if (before != app.state().groups.get(key).snapshot() || address != &app.state().groups.get(key))
            throw std::runtime_error("切换无损检查失败");
        std::cout << "Group switch: members preserved, stable address=true\n";
        app.inviteMember(owner, key, applicant);
        app.confirmInvitation(applicant, key);
        std::cout << "D06 Wechat invitation members=" << app.queryGroupMembers(owner, key).size() << '\n';
        app.queryGroupMembers(owner, key);
        std::cout << "D07 query cache hits=" << app.queryCacheMetrics().hits << '\n';
        saved = SnapshotCodecFanLX::encode(app.snapshot());
        app.saveAndClose();
    }
    {
        PersistenceApplicationFanLX restarted(directory);
        if (saved != SnapshotCodecFanLX::encode(restarted.snapshot()))
            throw std::runtime_error("重启快照不一致");
        std::cout << "Restart: generation=" << restarted.state().generation
                  << " mode=" << restarted.state().groups.get(key).policy().name()
                  << " logged-in=" << std::boolalpha
                  << restarted.state().accounts.isAccountLoggedIn(owner.service, owner.accountId) << '\n';
        std::cout << "D08 full snapshot equal=true cache entries=" << restarted.queryCacheMetrics().entries << '\n';
        restarted.saveAndClose();
    }
    ArcCacheFanLX<std::string, int> cache(8, 4096);
    cache.put("demo", 42);
    std::cout << "ARC query=" << *cache.get("demo") << "\nEvidence directory: " << directory.u8string()
              << '\n';
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    try {
        std::filesystem::path data = L"data";
        bool initialize = false, recover = false, check = false;
        bool imServer = false;
        std::string imHost = "127.0.0.1";
        bool imClient = false;
        unsigned short imPort = 9500;
        std::filesystem::path imTokens;
        for (int i = 1; i < argc; ++i) {
            const std::wstring arg = argv[i];
            if (arg == L"--demo") {
                demoFanLX();
                return 0;
            }
            if (arg == L"--im-server") imServer = true;
            else if (arg == L"--im-host" && i + 1 < argc) {
                imHost = std::filesystem::path(argv[++i]).u8string(); imClient = true;
            } else if (arg == L"--im-port" && i + 1 < argc) {
                const std::wstring number = argv[++i];
                std::size_t used = 0; const auto port = std::stoul(number, &used);
                if (used != number.size() || port == 0 || port > 65535) throw std::invalid_argument("IM 端口非法");
                imPort = static_cast<unsigned short>(port);
            } else if (arg == L"--im-tokens" && i + 1 < argc) imTokens = argv[++i];
            else if (arg == L"--data-dir" && i + 1 < argc)
                data = argv[++i];
            else if (arg == L"--init-demo")
                initialize = true;
            else if (arg == L"--recover")
                recover = true;
            else if (arg == L"--check") check = true;
            else if (arg == L"--menu") check = false;
            else
                throw std::invalid_argument("用法: --demo 或 [--data-dir 路径] [--menu | --check | --init-demo | --recover]，IM: [--im-server] --im-host IPv4 --im-port 端口 --im-tokens 令牌JSON路径");
        }
        if (initialize && recover)
            throw std::invalid_argument("初始化与恢复不能同时执行");
        PersistenceApplicationFanLX app(data);
        std::cout << app.loadResult().message << '\n';
        if (imServer) {
            if (app.loadResult().status != LoadStatusFanLX::Loaded || initialize || recover || check)
                throw std::runtime_error("服务端需先用 --init-demo 初始化，不能与维护参数同时使用");
            auto tokens = nlohmann::json::parse(AtomicFileWriterFanLX::read(imTokens)).get<std::map<std::string, std::string>>();
            ImServerFanLX server(app.state(), data / "im-server", std::move(tokens), imPort, imHost);
            SetConsoleCtrlHandler(stopImHandlerFanLX, TRUE);
            std::cout << "IM server " << imHost << ":" << server.port() << "，Ctrl+C 停止。\n" << std::flush;
            while (!stopImFanLX) server.tick();
            SetConsoleCtrlHandler(stopImHandlerFanLX, FALSE);
            app.saveAndClose(); return 0;
        }
        if (!initialize && !recover && !check) {
            ConsoleAppFanLX console(app, std::cin, std::cout);
            if (imClient) {
                if (imTokens.empty()) throw std::invalid_argument("IM 客户端需要 --im-tokens 令牌文件");
                console.configureIm(imHost, imPort, imTokens, data / "im-client");
            }
            return console.run();
        }
        if (recover)
            app.confirmRecovery();
        else if (initialize) {
            app.initialize();
            app.importInitial(demoSeedFanLX());
        } else if (app.loadResult().status != LoadStatusFanLX::Loaded) {
            std::cout << "未进入可写状态。空目录使用 --init-demo；有效备份恢复使用 --recover。\n";
            return 2;
        }
        std::cout << "Loaded users=" << app.snapshot().users.size()
                  << " generation=" << app.state().generation << '\n';
        // 显式批处理参数执行一次后关闭；默认入口和 --menu 进入数字菜单。
        app.saveAndClose();
        std::cout << "保存检查完成，已关闭。\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "操作失败: " << e.what() << '\n';
        return 1;
    }
}

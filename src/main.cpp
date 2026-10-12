#include "db/ChatMigrationFanLX.h"
#include "infra/ArcCacheFanLX.h"
#include "network/ImClientFanLX.h"
#include "network/ImServerFanLX.h"
#include "network/ServerAccountRegistrarFanLX.h"
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
        // 演示数据的密码规则是 pw-<QQ号>（见 demoPasswordFanLX）；下面按业务语义分账号登录。
        app.login(owner, demoPasswordFanLX(owner.accountId), true);
        const AccountRefFanLX wxOwner{ServiceTypeFanLX::Wechat, "wx_2026001"};
        const AccountRefFanLX wxPeer{ServiceTypeFanLX::Wechat, "wx_2026002"};
        std::cout << "D01 loaded users=" << app.snapshot().users.size() << " groups=" << app.snapshot().groups.size() << '\n';
        std::cout << "D02 linked Wechat login=" << app.state().accounts.isAccountLoggedIn(wxOwner.service, wxOwner.accountId) << '\n';
        const auto recommended = app.queryCrossService(owner, wxOwner);
        if (recommended != std::vector<std::string>{"wx_2026002"}) throw std::runtime_error("推荐不匹配");
        app.login(wxPeer, demoPasswordFanLX("2026002"));
        app.addFriend(wxOwner, wxPeer);
        std::cout << "D03 recommendations=" << recommended.size() << " target friends=" << app.queryFriendAccounts(wxOwner).size() << '\n';
        app.login(applicant, demoPasswordFanLX(applicant.accountId));
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
        // 扩展项：--im-sqlite 使用 SQLite 聊天库；--im-migrate 把旧 JSON 离线迁移到 SQLite。
        bool imSqlite = false;
        bool imMigrate = false;
        std::filesystem::path imMigrateDir;
        for (int i = 1; i < argc; ++i) {
            const std::wstring arg = argv[i];
            if (arg == L"--demo") {
                demoFanLX();
                return 0;
            }
            if (arg == L"--im-server") imServer = true;
            else if (arg == L"--im-sqlite") imSqlite = true;
            else if (arg == L"--im-migrate") imMigrate = true;
            else if (arg == L"--im-migrate-dir" && i + 1 < argc) imMigrateDir = argv[++i];
            else if (arg == L"--im-host" && i + 1 < argc) {
                imHost = std::filesystem::path(argv[++i]).u8string(); imClient = true;
            } else if (arg == L"--im-port" && i + 1 < argc) {
                const std::wstring number = argv[++i];
                std::size_t used = 0; const auto port = std::stoul(number, &used);
                if (used != number.size() || port == 0 || port > 65535) throw std::invalid_argument("IM 端口非法");
                imPort = static_cast<unsigned short>(port);
            } else if (arg == L"--data-dir" && i + 1 < argc)
                data = argv[++i];
            else if (arg == L"--init-demo")
                initialize = true;
            else if (arg == L"--recover")
                recover = true;
            else if (arg == L"--check") check = true;
            else if (arg == L"--menu") check = false;
            else
                throw std::invalid_argument("用法: --demo 或 [--data-dir 数据根目录] [--menu | --check | --init-demo | --recover]，IM: [--im-server] [--im-sqlite] --im-host IPv4 --im-port 端口，迁移: --im-migrate [--im-migrate-dir 目录]。登录用「微X号 + 密码」，账号库即领域快照里的账号（无令牌文件）。客户端聊天存储目录按服务派生：<数据根>/im-client/<host>_<端口>/<服务账号>/<服务名>");
        }
        if (initialize && recover)
            throw std::invalid_argument("初始化与恢复不能同时执行");
        PersistenceApplicationFanLX app(data);
        std::cout << app.loadResult().message << '\n';
        if (imMigrate) {
            // 离线迁移：把旧 IM JSON 存储转成 SQLite。原 JSON 保留为只读备份，不回写。
            const auto root = imMigrateDir.empty() ? data : imMigrateDir;
            if (app.loadResult().status != LoadStatusFanLX::Loaded)
                throw std::runtime_error("迁移需要已加载的领域状态以确认账号与群归属");
            nlohmann::json report = nlohmann::json::array();            const auto clientLegacy = root / "im-client" / "state.json";
            if (std::filesystem::exists(clientLegacy)) {
                chatDbFanLX::ChatMigrationFanLX::ClientSourceFanLX source;
                source.legacyJsonPath = clientLegacy;
                source.databasePath = root / "im-client" / "chat.db";
                const auto snapshot = app.snapshot();
                if (snapshot.accounts.empty())
                    throw std::runtime_error("领域状态没有账号，无法确定迁移归属");
                const auto &account = snapshot.accounts.front();
                source.ownerAccount = account.accountId;
                source.deviceId = "migrated-" + std::to_string(GetTickCount64());
                source.serverInstanceId = "legacy-migrated";
                const auto result = chatDbFanLX::ChatMigrationFanLX::migrateClient(source);
                report.push_back({{"kind", "client"}, {"message", result.message},
                                  {"warnings", result.warnings}});
            }
            const auto serverLegacy = root / "im-server" / "state.json";
            if (std::filesystem::exists(serverLegacy)) {
                chatDbFanLX::ChatMigrationFanLX::ServerSourceFanLX source;
                source.legacyJsonPath = serverLegacy;
                source.databasePath = root / "im-server" / "server-chat.db";
                source.serverInstanceId = "legacy-migrated";
                const auto result = chatDbFanLX::ChatMigrationFanLX::migrateServer(source);
                report.push_back({{"kind", "server"}, {"message", result.message},
                                  {"warnings", result.warnings}});
            }
            if (report.empty())
                throw std::runtime_error("未找到可迁移的旧 IM 存储（im-client/state.json 或 im-server/state.json）");
            std::cout << report.dump(2) << '\n';
            app.saveAndClose();
            return 0;
        }
        if (imServer) {
            if (app.loadResult().status != LoadStatusFanLX::Loaded || initialize || recover || check)
                throw std::runtime_error("服务端需先用 --init-demo 初始化，不能与维护参数同时使用");
            // 认证直接用已加载的领域账号库，服务端不再读取任何令牌 / 口令副本文件。
            ImServerFanLX::OptionsFanLX options;
            if (imSqlite) options.chatDatabasePath = data / "im-server" / "server-chat.db";
            // 服务端无状态的两条接线（实现见 ServerAccountRegistrarFanLX）：
            //   1) 每次认证前"读穿"持久化快照（只有文件真的变了才重新解析），
            //      因此账号数据的权威副本始终是数据目录里的 state.json；
            //   2) REGISTER 直接把新微X账号（含客户端已开通的服务）写进同一份持久化存储
            //      （候选事务 + 原子发布），所以注册之后**不需要重启服务端**就能登录。
            const ServerAccountRegistrarFanLX registrar(app);
            options.refreshAccounts = [&registrar] { (void)registrar.refreshAccounts(); };
            options.registerAccount = [&registrar](
                                          const std::string &weixId, const std::string &password,
                                          const std::string &nickname, const std::string &birthDate,
                                          const std::string &location,
                                          const std::vector<std::pair<std::string, std::string>> &services,
                                          const std::vector<std::pair<std::string, std::string>> &friends) {
                return registrar.registerAccountFanLX(weixId, password, nickname, birthDate, location,
                                                      services, friends);
            };
            //   3) 好友申请被同意 / 删除好友时，把好友关系落到同一份持久化存储（服务端中转）。
            options.updateFriendship = [&registrar](const std::string &service, const std::string &self,
                                                    const std::string &peer, bool add) {
                return registrar.updateFriendshipFanLX(service, self, peer, add);
            };
            ImServerFanLX server(app.state(), data / "im-server", imPort, imHost, options);
            SetConsoleCtrlHandler(stopImHandlerFanLX, TRUE);
            std::cout << "IM server " << imHost << ":" << server.port() << " 存储=" << server.storageBackend()
                      << " 实例=" << server.instanceId()
                      << "，认证=微X号+密码（聊天通道 QQ/微信/微博）；"
                         "账号库=持久化快照（认证前读穿，REGISTER 可直接登记，无需重启），Ctrl+C 停止。\n"
                      << std::flush;
            while (!stopImFanLX) server.tick();
            SetConsoleCtrlHandler(stopImHandlerFanLX, FALSE);
            app.saveAndClose(); return 0;
        }
        if (!initialize && !recover && !check) {
            ConsoleAppFanLX console(app, std::cin, std::cout);
            if (imClient) {
                // 传"数据根"：客户端存储目录由服务器身份 + 登录账号派生
                // （<根>/im-client/<host>_<port>/<账号>），登录后在 ConsoleAppFanLX 内确定。
                console.configureIm(imHost, imPort, data, imSqlite);
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

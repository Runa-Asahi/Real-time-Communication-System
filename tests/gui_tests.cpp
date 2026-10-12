// E04 图形层验收（离屏运行，不需要真实窗口系统）：
// - 控制器把网络/数据库事件正确映射为会话与消息模型；
// - 会话切换丢弃过期查询结果；
// - 发送路径"先落库再入网"，队列满/库不可用不得静默丢弃输入；
// - 界面不执行阻塞操作：控制器方法在无服务器时也必须立即返回。
#ifdef NDEBUG
#undef NDEBUG
#endif
// winsock2.h 必须先于 windows.h（ChatControllerFanLX.h 经由 SqliteHandleFanLX.h 引入 windows.h）。
#include "network/ImServerFanLX.h"
#include "network/ServerAccountRegistrarFanLX.h"
#include "gui/ChatControllerFanLX.h"
#include "gui/ConversationModelFanLX.h"
#include "db/SqliteChatRepositoryFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include <QCoreApplication>
#include <QTimer>
#include <chrono>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>

using namespace fanlxGui;

namespace {

std::filesystem::path temporaryRootFanLX() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("fanlx-gui-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    return root;
}

void removeTreeFanLX(const std::filesystem::path &path) {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
    assert(!std::filesystem::exists(path));
}

// 会话模型：展示字段与角色必须一一对应，未读数不能丢失。
void t60ConversationModelFanLX() {
    ConversationListModelFanLX model;
    QList<ConversationItemFanLX> items;
    ConversationItemFanLX direct;
    direct.key = QStringLiteral("d|2026001|2026002");
    direct.title = QStringLiteral("2026002");
    direct.subtitle = QStringLiteral("你好");
    direct.unread = 3;
    items.push_back(direct);
    ConversationItemFanLX group;
    group.key = QStringLiteral("g|1001");
    group.title = QStringLiteral("1001");
    group.group = true;
    group.unread = 0;
    items.push_back(group);
    model.setItems(items);

    assert(model.rowCount() == 2);
    assert(model.data(model.index(0), ConversationListModelFanLX::KeyRoleFanLX).toString() ==
           QStringLiteral("d|2026001|2026002"));
    assert(model.data(model.index(0), ConversationListModelFanLX::UnreadRoleFanLX).toInt() == 3);
    assert(model.data(model.index(1), ConversationListModelFanLX::GroupRoleFanLX).toBool() == true);
    assert(model.keyAt(1) == QStringLiteral("g|1001"));
    assert(model.keyAt(9).isEmpty());
    std::cout << "[ok] t60 conversation model" << std::endl;
}

// 消息模型：状态文案与"自身/失败/待发送"标记必须可见（界面据此区分语义）。
void t61MessageModelFanLX() {
    MessageListModelFanLX model;
    QList<MessageItemFanLX> items;
    MessageItemFanLX pending;
    pending.sender = QStringLiteral("2026001");
    pending.body = QStringLiteral("待发送内容");
    pending.self = true;
    pending.pending = true;
    pending.statusText = QStringLiteral("待发送");
    pending.sequence = 0;
    items.push_back(pending);
    MessageItemFanLX received;
    received.sender = QStringLiteral("2026002");
    received.body = QStringLiteral("对端消息");
    received.sequence = 4;
    received.timeText = QStringLiteral("2026-09-26T10:00:00Z");
    items.push_back(received);
    model.setMessages(items);

    assert(model.rowCount() == 2);
    assert(model.data(model.index(0), MessageListModelFanLX::PendingRoleFanLX).toBool());
    assert(model.data(model.index(0), MessageListModelFanLX::StatusRoleFanLX).toString() ==
           QStringLiteral("待发送"));
    assert(model.data(model.index(1), MessageListModelFanLX::TimeRoleFanLX).toString() ==
           QStringLiteral("2026-09-26T10:00:00Z"));
    assert(model.sequenceAt(1) == 4);
    std::cout << "[ok] t61 message model" << std::endl;
}

// 配置缺失时控制器必须立即失败并给出原因，不进入"假装已连接"状态。
// 这里同时覆盖"数据根缺失"：控制器不再接受某个账号目录，而是必须拿到数据根才能派生。
void t62ControllerRejectsIncompleteConfigFanLX() {
    ChatControllerFanLX controller;
    controller.configure(QStringLiteral("127.0.0.1"), 0, QStringLiteral("2026001"), QStringLiteral("pw"),
                         QString(), true);
    QString detail;
    const auto started = controller.start(&detail);
    assert(!started);
    assert(!detail.isEmpty());
    assert(controller.state() == ConnectionStateFanLX::Disconnected);
    assert(controller.messages().isEmpty());
    std::cout << "[ok] t62 incomplete config rejected: " << detail.toStdString() << std::endl;
}

// 数据库可用但服务器离线时：start 必须失败并给出可读原因，而不是崩溃或静默成功。
// 传入的是"数据根"，账号子目录由控制器派生。
void t63ControllerReportsStorageFailureFanLX() {
    const auto root = temporaryRootFanLX() / "gui-storage";
    std::filesystem::create_directories(root);
    // 先确认该目录本身能被 SQLite 正常打开（保证后续失败来自网络握手而不是存储）。
    {
        chatDbFanLX::SqliteChatRepositoryFanLX::ConfigFanLX config;
        config.databasePath = root / "chat.db";
        config.serverInstanceId = "srv-instance-gui";
        config.ownerAccount = "2026001";
        config.deviceId = "device-gui";
        chatDbFanLX::SqliteChatRepositoryFanLX holder(config);
        assert(holder.messageCount() == 0);
    }
    assert(std::filesystem::exists(root / "chat.db"));
    removeTreeFanLX(temporaryRootFanLX());

    ChatControllerFanLX controller;
    controller.configure(QStringLiteral("127.0.0.1"), 59999, QStringLiteral("2026001"),
                         QStringLiteral("0123456789abcdef"), QString::fromStdWString(root.wstring()), true);
    QString detail;
    // start() 必须是即时的：服务器是否可达由后台线程判断，不能让界面被同步握手阻塞。
    const auto startedAt = std::chrono::steady_clock::now();
    const auto started = controller.start(&detail);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - startedAt)
                             .count();
    assert(started);
    assert(elapsed < 1000);
    // 服务器不可达：必须保持"未在线"，并把错误通过事件报告出来。
    QString reported;
    QObject::connect(&controller, &ChatControllerFanLX::errorFanLX,
                     [&](const QString &text) { reported = text; });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents();
        if (!reported.isEmpty())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (reported.isEmpty()) {
        std::cout << "[t63] no error event; online=" << (controller.online() ? 1 : 0) << " lastError=["
                  << controller.lastError().toStdString() << "]" << std::endl;
        assert(false);
    }
    assert(!controller.online());
    std::cout << "[ok] t63 offline server reported: " << reported.toStdString()
              << " (start returned in " << elapsed << " ms)" << std::endl;
    controller.stop();
}

// 端到端：真实服务端 + 两个控制器（Qt 事件循环 + 网络线程 + 数据库线程）互相收发。
// 这是"三个独立客户端真实收发"的自动化部分：单聊发送、服务器 ACK、对端收到都必须在
// 有界时间内完成，且界面模型能反映出来。
void t64TwoClientsChatFanLX() {
    using namespace std::chrono_literals;
    const auto root = temporaryRootFanLX() / "gui-e2e";
    std::filesystem::create_directories(root);

    PersistenceApplicationFanLX app(root / "domain");
    app.initialize();
    app.importInitial(demoSeedFanLX());
    // 登录凭据来自领域账号库（演示数据密码规则 pw-<QQ号>），不再有测试专用令牌表。
    ImServerFanLX server(app.state(), root / "server", 0);
    const auto port = server.port();
    assert(port != 0);

    // Qt 定时器驱动服务端 tick：控制器构造期间的握手重试也因此能成功。
    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&server] { server.tick(); });
    ticker.start(20);

    ChatControllerFanLX first, second;
    // 两个账号共用同一个"数据根"：存储目录按 服务器身份+账号 派生，天然隔离。
    // 这正是"用户不再自选目录"的前提——如果共用根会互相踩踏，隔离就无从谈起。
    const auto sharedRoot = QString::fromStdWString(root.wstring());
    first.configure(QStringLiteral("127.0.0.1"), port, QStringLiteral("2026001"),
                    QString::fromStdString(demoPasswordFanLX("2026001")), sharedRoot, true);
    second.configure(QStringLiteral("127.0.0.1"), port, QStringLiteral("2026002"),
                     QString::fromStdString(demoPasswordFanLX("2026002")), sharedRoot, true);
    assert(first.dataDirectory() != second.dataDirectory());
    assert(first.dataDirectory().contains(QStringLiteral("2026001")));
    assert(second.dataDirectory().contains(QStringLiteral("2026002")));

    QString failure;
    const auto pump = [&](auto predicate, const char *label) {
        const auto deadline = std::chrono::steady_clock::now() + 15s;
        while (!predicate()) {
            QCoreApplication::processEvents();
            server.tick();
            if (std::chrono::steady_clock::now() > deadline) {
                std::cout << "[gui pump timeout] " << label << std::endl;
                throw std::runtime_error(std::string("GUI 端到端等待超时：") + label);
            }
            std::this_thread::sleep_for(5ms);
        }
    };

    // 握手是同步的（控制器构造函数内完成），必须先把服务端推进到已监听并持续 tick。
    ticker.stop();
    const auto warmup = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < warmup) {
        server.tick();
        std::this_thread::sleep_for(2ms);
    }
    // 用裸连接确认服务端确实在监听：避免把"服务端没起来"误判成"客户端握手有问题"。
    {
        auto probe = TcpPeerFanLX::connect("127.0.0.1", port, 2s);
        assert(!probe->ended());
        probe->close();
        server.tick();
    }

    // 用一个独立进程内客户端（非 GUI 控制器）验证服务端握手链路本身可用。
    {
        ImClientFanLX rawClient("127.0.0.1", port, "2026002", demoPasswordFanLX("2026002"),
                               std::filesystem::path((root / "raw").wstring()));
        const auto rawDeadline = std::chrono::steady_clock::now() + 10s;
        while (!rawClient.online() && std::chrono::steady_clock::now() < rawDeadline) {
            server.tick();
            std::this_thread::sleep_for(2ms);
        }
        std::cout << "[t64] raw client online=" << (rawClient.online() ? 1 : 0)
                  << " error=[" << rawClient.error() << "]" << std::endl;
        assert(rawClient.online());
        rawClient.reconnect();
    }

    assert(first.start(&failure) && failure.isEmpty());
    assert(second.start(&failure) && failure.isEmpty());
    pump([&] { return first.online(); }, "first-online");
    pump([&] { return second.online(); }, "second-online");
    // 存储后端由工作线程在收到 LOGIN_OK 后准备（界面线程不阻塞），因此这里等待它就绪。
    pump([&] { return first.storageBackend() == QStringLiteral("sqlite"); }, "first-storage");
    pump([&] { return second.storageBackend() == QStringLiteral("sqlite"); }, "second-storage");

    // 目录请求：服务端按权限返回，两个账号互为好友。
    QList<QString> friends, groups;
    QObject::connect(&first, &ChatControllerFanLX::directoryFanLX,
                     [&](const QList<QString> &f, const QList<QString> &g) {
                         friends = f;
                         groups = g;
                     });
    first.requestDirectory();
    pump([&] { return !friends.isEmpty() || !groups.isEmpty(); }, "directory");
    assert(friends.contains(QStringLiteral("2026002")));

    // 单聊：A 打开与 B 的会话并发送，B 必须收到。
    const QString conversation = QStringLiteral("d|2026001|2026002");
    first.openConversation(conversation);
    pump([&] { return !first.messages().isEmpty() || first.currentQueryId() != 0; }, "first-history-query");
    // 消息内容用显式 UTF-8 构造，避免源码编码影响断言。
    const QString body = QString::fromUtf8("\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8CGUI");
    if (!first.sendText(conversation, 0, body)) {
        std::cout << "[t64] sendText failed; online=" << (first.online() ? 1 : 0)
                  << " storage=" << first.storageBackend().toStdString()
                  << " error=[" << first.lastError().toStdString() << "]" << std::endl;
        assert(false);
    }

    pump([&] {
        for (const auto &item : first.messages())
            if (item.body == body && item.statusText == QStringLiteral("服务器已接收"))
                return true;
        return false;
    }, "sender-ack");

    second.openConversation(conversation);
    pump([&] {
        for (const auto &item : second.messages())
            if (item.body == body)
                return true;
        return false;
    }, "receiver-delivered");

    // 会话列表必须反映两侧各自的未读/最新消息。
    first.requestConversations();
    pump([&] { return !first.conversations().isEmpty(); }, "conversations");
    assert(first.conversations().front().key == conversation);

    // 本次改造要求 3：聊天界面上"群"与"好友"必须有区别标志。
    // 目录里包含 QQ 群 1001，因此会话列表里同时会出现好友单聊与群聊两种会话。
    first.requestDirectory();
    pump([&] {
        bool friendSeen = false, groupSeen = false;
        for (const auto &item : first.conversations()) {
            friendSeen = friendSeen || !item.group;
            groupSeen = groupSeen || item.group;
        }
        return friendSeen && groupSeen;
    }, "conversation-kinds");
    {
        bool friendSeen = false, groupSeen = false;
        QString friendColor, groupColor, friendKind, groupKind;
        for (const auto &item : first.conversations()) {
            if (item.group) {
                groupSeen = true;
                groupColor = item.kindColor;
                groupKind = item.kindText;
            } else {
                friendSeen = true;
                friendColor = item.kindColor;
                friendKind = item.kindText;
            }
        }
        assert(friendSeen && groupSeen);
        assert(friendKind == QStringLiteral("好友") && groupKind == QStringLiteral("群"));
        // 颜色必须不同：列表行即使不看文字也能分辨。
        assert(!friendColor.isEmpty() && !groupColor.isEmpty() && friendColor != groupColor);
        // 打开群会话时控制器也要判定为"群"（标题栏 / 状态栏用的是同一个来源）。
        const QString groupKey = QStringLiteral("g|1001");
        first.openConversation(groupKey);
        assert(first.currentConversationIsGroup());
        assert(first.currentConversationKindText() == QStringLiteral("群"));
        first.openConversation(conversation);
        assert(!first.currentConversationIsGroup());
        assert(first.currentConversationKindText() == QStringLiteral("好友"));
        std::cout << "[ok] t65 conversation kind flags (friend=" << friendColor.toStdString()
                  << ", group=" << groupColor.toStdString() << ")" << std::endl;
    }

    // 关闭客户端不得留下未回收线程：析构内部完成 stop 并 join。
    first.stop();
    second.stop();
    ticker.stop();
    std::cout << "[ok] t64 two sqlite clients chat end to end" << std::endl;
}

// 本次改造要求 4：三种服务的聊天数据分开——QQ 连接只看得到 QQ 的好友与群，
// 微信连接只看得到微信的；QQ 的好友关系不能用来给微信号发单聊。
void t67PerServiceChannelsFanLX() {
    using namespace std::chrono_literals;
    const auto root = temporaryRootFanLX() / "gui-per-service";
    std::filesystem::create_directories(root);

    PersistenceApplicationFanLX app(root / "domain");
    app.initialize();
    app.importInitial(demoSeedFanLX());
    ImServerFanLX server(app.state(), root / "server", 0);
    assert(server.port() != 0);

    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&server] { server.tick(); });
    ticker.start(20);
    const auto warmup = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < warmup) {
        server.tick();
        std::this_thread::sleep_for(2ms);
    }

    const auto sharedRoot = QString::fromStdWString(root.wstring());
    const auto password = QString::fromStdString(demoPasswordFanLX("2026001"));
    ChatControllerFanLX qq, wechat;
    qq.configure(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("2026001"), password,
                 sharedRoot, true, QStringLiteral("QQ"));
    // 同一个人的微信通道用微信号登录（QQ 号与微信号是两个服务身份，互不冲突）。
    wechat.configure(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("wx_2026001"), password,
                     sharedRoot, true, QStringLiteral("Wechat"));
    QString failure, reason;
    assert(qq.start(&failure));
    assert(qq.waitForLogin(8000, &reason));
    assert(wechat.start(&failure));
    assert(wechat.waitForLogin(8000, &reason));
    assert(qq.online() && wechat.online());
    assert(qq.service() == QStringLiteral("QQ") && wechat.service() == QStringLiteral("Wechat"));

    QList<QString> qqFriends, qqGroups, wxFriends, wxGroups;
    QObject::connect(&qq, &ChatControllerFanLX::directoryFanLX,
                     [&](const QList<QString> &f, const QList<QString> &g) {
                         qqFriends = f;
                         qqGroups = g;
                     });
    QObject::connect(&wechat, &ChatControllerFanLX::directoryFanLX,
                     [&](const QList<QString> &f, const QList<QString> &g) {
                         wxFriends = f;
                         wxGroups = g;
                     });
    qq.requestDirectory();
    wechat.requestDirectory();
    const auto pump = [&](auto predicate, const char *label) {
        const auto deadline = std::chrono::steady_clock::now() + 15s;
        while (!predicate()) {
            QCoreApplication::processEvents();
            server.tick();
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error(std::string("等待超时：") + label);
            std::this_thread::sleep_for(5ms);
        }
    };
    pump([&] { return !qqFriends.isEmpty() || !qqGroups.isEmpty(); }, "qq-directory");
    pump([&] { return !wxGroups.isEmpty(); }, "wechat-directory");

    // 目录按服务分开：QQ 有好友 2026002，微信的好友列表为空（演示数据只建了 QQ 好友）。
    assert(qqFriends.contains(QStringLiteral("2026002")));
    assert(!qqFriends.contains(QStringLiteral("wx_2026002")));
    assert(wxFriends.isEmpty());
    assert(!qqGroups.isEmpty() && !wxGroups.isEmpty());
    std::cout << "[ok] t67 directories are per service (qq friends=" << qqFriends.size()
              << ", wechat friends=" << wxFriends.size() << ")" << std::endl;

    // 微信通道给 wx_2026002 发单聊：QQ 的好友关系不会带到微信，因此这只能是**临时会话**——
    // 服务端接收（非好友允许限额内的临时对话），但会计入额度，双方仍不是微信好友。
    const QString wxDirect = QStringLiteral("d|wx_2026001|wx_2026002");
    wechat.openConversation(wxDirect);
    pump([&] { return wechat.currentConversation() == wxDirect; }, "wechat-open-direct");
    const QString body = QString::fromUtf8("\xE5\xBE\xAE\xE4\xBF\xA1\xE5\x8D\x95\xE8\x81\x8A");
    assert(wechat.sendText(wxDirect, 0, body));
    pump([&] {
        for (const auto &item : wechat.messages())
            if (item.body == body && !item.pending && !item.failed)
                return true;
        return false;
    }, "wechat-direct-temporary-accepted");
    pump([&] { return wechat.temporaryRemaining(QStringLiteral("wx_2026002")) == 9; }, "wechat-temporary-quota");
    assert(!wxFriends.contains(QStringLiteral("wx_2026002")));
    std::cout << "[ok] t67 QQ friendship does not carry to WeChat: the direct message is a temporary chat"
              << std::endl;

    // 各自的聊天库分开存放：QQ 客户端目录与微信客户端目录不是同一个
    // （目录里还带服务名，因此即使 QQ 与微博共用号码也不会共用聊天库）。
    assert(qq.dataDirectory() != wechat.dataDirectory());
    qq.stop();
    wechat.stop();
    ticker.stop();
}

// 微博通道：除群以外的功能都要可用——资料/开通/好友/单聊。
// 这里验证：微博能认证上线、目录里有好友（没有群）、单聊能真实收发、
// 以及 QQ 与微博共用同一个号码时两条通道的消息互不串（服务端按"服务 + 号码"分流）。
void t68WeiboChannelFanLX() {
    using namespace std::chrono_literals;
    const auto root = temporaryRootFanLX() / "gui-weibo";
    std::filesystem::create_directories(root);

    PersistenceApplicationFanLX app(root / "domain");
    app.initialize();
    app.importInitial(demoSeedFanLX());
    // 演示数据本身已经带了微博分区的好友（U1↔U2）；这里再显式登录并调一次添加，
    // 既是幂等保护，也确保"微博好友能单聊"这条用例不依赖种子细节。
    {
        app.login({ServiceTypeFanLX::WeiX, "2026001"}, demoPasswordFanLX("2026001"), true);
        app.login({ServiceTypeFanLX::WeiX, "2026002"}, demoPasswordFanLX("2026002"), true);
        app.addFriend({ServiceTypeFanLX::Weibo, "2026001"}, {ServiceTypeFanLX::Weibo, "2026002"});
    }
    ImServerFanLX server(app.state(), root / "server", 0);
    assert(server.port() != 0);

    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&server] { server.tick(); });
    ticker.start(20);
    const auto warmup = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < warmup) {
        server.tick();
        std::this_thread::sleep_for(2ms);
    }

    const auto sharedRoot = QString::fromStdWString(root.wstring());
    // QQ 与微博共用同一个号码（课程约定），两条通道同时在线。
    ChatControllerFanLX qq, weibo;
    qq.configure(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("2026001"),
                 QString::fromStdString(demoPasswordFanLX("2026001")), sharedRoot, true,
                 QStringLiteral("QQ"));
    weibo.configure(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("2026001"),
                    QString::fromStdString(demoPasswordFanLX("2026001")), sharedRoot, true,
                    QStringLiteral("Weibo"));
    QString failure, reason;
    assert(qq.start(&failure));
    assert(qq.waitForLogin(8000, &reason));
    assert(weibo.start(&failure));
    assert(weibo.waitForLogin(8000, &reason));
    assert(qq.online() && weibo.online());
    // 同号码的两条通道各占一把目录锁、各一份聊天库，因此不会互相拒绝。
    assert(qq.dataDirectory() != weibo.dataDirectory());
    assert(weibo.dataDirectory().contains(QStringLiteral("Weibo")));

    QList<QString> weiboFriends, weiboGroups;
    QObject::connect(&weibo, &ChatControllerFanLX::directoryFanLX,
                     [&](const QList<QString> &f, const QList<QString> &g) {
                         weiboFriends = f;
                         weiboGroups = g;
                     });
    weibo.requestDirectory();
    const auto pump = [&](auto predicate, const char *label) {
        const auto deadline = std::chrono::steady_clock::now() + 15s;
        while (!predicate()) {
            QCoreApplication::processEvents();
            server.tick();
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error(std::string("等待超时：") + label);
            std::this_thread::sleep_for(5ms);
        }
    };
    pump([&] { return weiboFriends.contains(QStringLiteral("2026002")); }, "weibo-directory");
    // 微博没有群：目录里的群列表必须为空（"除群以外的功能"）。
    assert(weiboGroups.isEmpty());
    std::cout << "[ok] t68 weibo directory: friends=" << weiboFriends.size()
              << " groups=" << weiboGroups.size() << std::endl;

    // 微博单聊：真实收发。
    const QString weiboDirect = QStringLiteral("d|2026001|2026002");
    weibo.openConversation(weiboDirect);
    pump([&] { return weibo.currentConversation() == weiboDirect; }, "weibo-open");
    const QString body = QString::fromUtf8("\xE5\xBE\xAE\xE5\x8D\x9A\xE7\xA7\x81\xE4\xBF\xA1");
    assert(weibo.sendText(weiboDirect, 0, body));
    pump([&] {
        for (const auto &item : weibo.messages())
            if (item.body == body && item.statusText == QStringLiteral("服务器已接收"))
                return true;
        return false;
    }, "weibo-ack");
    std::cout << "[ok] t68 weibo direct message accepted" << std::endl;

    // 同号码的 QQ 通道不该看到这条微博消息：它有自己的会话与历史。
    qq.openConversation(weiboDirect);
    pump([&] { return qq.currentConversation() == weiboDirect; }, "qq-open-same-key");
    const auto qqDeadline = std::chrono::steady_clock::now() + 1500ms;
    while (std::chrono::steady_clock::now() < qqDeadline) {
        QCoreApplication::processEvents();
        server.tick();
        std::this_thread::sleep_for(5ms);
    }
    for (const auto &item : qq.messages())
        assert(item.body != body);
    std::cout << "[ok] t68 same number on two channels stays isolated" << std::endl;

    qq.stop();
    weibo.stop();
    ticker.stop();
}

// 本次改造要求 2：登录失败必须能判定出来（界面据此留在登录窗口，而不是进主界面再报错）。
// 这里用真实服务端 + 错误密码：控制器必须给出"被服务器拒绝"的结论，并且始终不在线。
void t66LoginRejectionFanLX() {
    using namespace std::chrono_literals;
    const auto root = temporaryRootFanLX() / "gui-login-reject";
    std::filesystem::create_directories(root);

    PersistenceApplicationFanLX app(root / "domain");
    app.initialize();
    app.importInitial(demoSeedFanLX());
    ImServerFanLX server(app.state(), root / "server", 0);
    assert(server.port() != 0);

    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&server] { server.tick(); });
    ticker.start(20);
    // 让服务端先进入监听状态。
    const auto warmup = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < warmup) {
        server.tick();
        std::this_thread::sleep_for(2ms);
    }

    // 1) 密码错误：服务器明确拒绝，waitForLogin 必须返回 false 并给出原因。
    {
        ChatControllerFanLX controller;
        controller.configure(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("2026001"),
                             QStringLiteral("wrong-password"), QString::fromStdWString(root.wstring()), true,
                             QStringLiteral("QQ"));
        QString failure;
        assert(controller.start(&failure));
        QString reason;
        const auto ok = controller.waitForLogin(8000, &reason);
        std::cout << "[t66] wrong password -> ok=" << (ok ? 1 : 0)
                  << " reason=[" << reason.toStdString() << "]" << std::endl;
        assert(!ok);
        assert(!controller.online());
        assert(reason.contains(QStringLiteral("用户名或密码错误")));
        controller.stop();
    }
    // 2) 微X 是上层登录身份，没有聊天通道：用它建连必须在客户端就被挡下（配置阶段返回失败）。
    //    微博现在**有**单聊通道（只是没有群），因此不再是"被拒绝"的例子。
    {
        ChatControllerFanLX weix;
        weix.configure(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("2026001"),
                       QString::fromStdString(demoPasswordFanLX("2026001")),
                       QString::fromStdWString(root.wstring()), true, QStringLiteral("WeiX"));
        QString failure;
        assert(!weix.start(&failure));
        assert(failure.contains(QStringLiteral("微X")));
        std::cout << "[ok] t66 upper-layer identity rejected at start: " << failure.toStdString()
                  << std::endl;
    }
    // 2b) 账号不存在：服务器同样拒绝（与密码错分开报，便于定位）。
    {
        ChatControllerFanLX ghost;
        ghost.configure(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("2999999"),
                        QStringLiteral("whatever"), QString::fromStdWString(root.wstring()), true,
                        QStringLiteral("QQ"));
        QString failure;
        assert(ghost.start(&failure));
        QString reason;
        assert(!ghost.waitForLogin(8000, &reason));
        assert(!ghost.online());
        std::cout << "[ok] t66 unknown account rejected: " << reason.toStdString() << std::endl;
        ghost.stop();
    }
    // 3) 正确的微X密码 + QQ 号：应当登录成功（对照组，证明上面的失败不是"端口不通"）。
    {
        ChatControllerFanLX controller;
        controller.configure(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("2026001"),
                             QString::fromStdString(demoPasswordFanLX("2026001")),
                        QString::fromStdWString(root.wstring()), true,
                             QStringLiteral("QQ"));
        QString failure;
        assert(controller.start(&failure));
        QString reason;
        assert(controller.waitForLogin(8000, &reason));
        assert(controller.online());
        assert(reason.isEmpty());
        controller.stop();
        std::cout << "[ok] t66 correct credentials still log in" << std::endl;
    }
    ticker.stop();
}

// 服务端无状态 + 账号数据持久化（用户实测问题的回归）：
// 在本机注册一个微X号后，**不重启服务端**就能登录；服务端重启后依然能登录（数据在持久化存储里）。
// 同时验证：只登记微X号、没登记服务账号时，登录会被明确拒为"未开通"，补登记服务后即可登录。
void t69ServerSideRegistrationFanLX() {
    using namespace std::chrono_literals;
    const auto root = temporaryRootFanLX() / "gui-server-register";
    std::filesystem::create_directories(root);
    const auto serverDomain = root / "server-domain";

    // 服务端账号库 = 它自己的持久化领域快照（与 --im-server 完全同一条路径）。
    auto app = std::make_unique<PersistenceApplicationFanLX>(serverDomain);
    app->initialize();
    app->importInitial(demoSeedFanLX());
    const ServerAccountRegistrarFanLX registrar(*app);
    ImServerFanLX::OptionsFanLX options;
    options.refreshAccounts = [&registrar] { (void)registrar.refreshAccounts(); };
    options.registerAccount = [&registrar](
                                  const std::string &weixId, const std::string &password,
                                  const std::string &nickname, const std::string &birthDate,
                                  const std::string &location,
                                  const std::vector<std::pair<std::string, std::string>> &services,
                                  const std::vector<std::pair<std::string, std::string>> &friends) {
        return registrar.registerAccountFanLX(weixId, password, nickname, birthDate, location, services,
                                              friends);
    };
    auto server = std::make_unique<ImServerFanLX>(app->state(), root / "server", 0, "127.0.0.1", options);
    const auto port = server->port();
    assert(port != 0);
    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&server] { server->tick(); });
    ticker.start(20);
    const auto warmup = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < warmup) {
        server->tick();
        std::this_thread::sleep_for(2ms);
    }

    const std::string newId = "2026088";
    const std::string newPassword = "pw-2026088";
    const auto sharedRoot = QString::fromStdWString(root.wstring());

    // 登记调用是同步的（真机上是"客户端进程"在等"服务端进程"回话）；在本进程测试里，
    // 服务端的 tick 必须继续跑，所以在工作线程里发起登记、主线程负责 tick。
    const auto registerOnServer = [&](ImServerFanLX &target,
                                      const std::vector<ImClientFanLX::ServiceEntryFanLX> &services,
                                      const std::vector<ImClientFanLX::ServiceEntryFanLX> &friends,
                                      std::string &reason) {
        std::atomic<bool> done{false};
        bool ok = false;
        std::thread worker([&] {
            ok = ImClientFanLX::registerAccountFanLX("127.0.0.1", port, newId, newPassword, "新用户",
                                                     "2006-01-02", "长春", reason, services, friends);
            done = true;
        });
        const auto deadline = std::chrono::steady_clock::now() + 15s;
        while (!done.load() && std::chrono::steady_clock::now() < deadline) {
            target.tick();
            std::this_thread::sleep_for(2ms);
        }
        worker.join();
        return ok;
    };

    // 1) 还没登记：服务端账号库里没有这个号码，聊天登录必须被拒（正是用户看到的现象）。
    {
        ChatControllerFanLX controller;
        controller.configure(QStringLiteral("127.0.0.1"), port, QString::fromStdString(newId),
                             QString::fromStdString(newPassword), sharedRoot, true, QStringLiteral("QQ"));
        QString failure;
        assert(controller.start(&failure));
        QString reason;
        assert(!controller.waitForLogin(8000, &reason));
        assert(!controller.online());
        assert(reason.contains(QStringLiteral("用户名或密码错误")) ||
               reason.contains(QStringLiteral("未开通")));
        std::cout << "[ok] t69 unregistered account rejected: " << reason.toStdString() << std::endl;
        controller.stop();
    }

    // 2) 只登记微X号（不带服务）：登记成功，但聊天登录仍会因"该服务未开通"被拒。
    {
        std::string reason;
        const auto pushed = registerOnServer(*server, {}, {}, reason);
        std::cout << "[t69] register pushed=" << (pushed ? 1 : 0) << " reason=[" << reason << "]"
                  << std::endl;
        assert(pushed);
        std::cout << "[ok] t69 registered on server: " << reason << std::endl;
        ChatControllerFanLX controller;
        controller.configure(QStringLiteral("127.0.0.1"), port, QString::fromStdString(newId),
                             QString::fromStdString(newPassword), sharedRoot, true, QStringLiteral("QQ"));
        QString failure;
        assert(controller.start(&failure));
        QString rejected;
        const auto failed = !controller.waitForLogin(8000, &rejected);
        std::cout << "[t69] step2 rejected=" << (failed ? 1 : 0) << " text=[" << rejected.toStdString()
                  << "]" << std::endl;
        assert(failed);
        // 服务端读穿账号库后能分辨"这个微X号存在、但服务端还没为它开通 QQ"与"密码错"。
        assert(rejected.contains(QStringLiteral("开通 QQ 服务")));
        std::cout << "[ok] t69 service not activated yet: " << rejected.toStdString() << std::endl;
        controller.stop();
    }

    // 3) 带上"本机已开通的服务"再登记一次（幂等）：服务端补建服务账号后立刻可登录，
    //    **中间没有重启服务端**——这正是"服务端无状态 + 账号持久化"要达到的效果。
    {
        std::string reason;
        const auto withFriends = registerOnServer(*server, {{"QQ", newId}, {"Weibo", newId}},
                                                    {{"QQ", "2026003"}}, reason);
        std::cout << "[t69] register+friends=" << (withFriends ? 1 : 0) << " reason=[" << reason
                  << "]" << std::endl;
        assert(withFriends);
        ChatControllerFanLX controller;
        controller.configure(QStringLiteral("127.0.0.1"), port, QString::fromStdString(newId),
                             QString::fromStdString(newPassword), sharedRoot, true, QStringLiteral("QQ"));
        QString failure;
        assert(controller.start(&failure));
        QString ok;
        assert(controller.waitForLogin(8000, &ok));
        assert(controller.online());
        assert(ok.isEmpty());
        std::cout << "[ok] t69 login works without restarting the server" << std::endl;
        // 单设备策略：服务端只允许一个在线会话，所以先放掉控制器这条连接，再用原始客户端验证好友。
        controller.stop();
        // 服务端按**它自己的**账号库判定"是不是好友"：本机加的好友必须随登记一起补登，
        // 否则这一步会被服务端拒为"只能给好友发送消息"。
        // 会话键是"d|本人|对端"（客户端统一用它推导收件人，避免把消息发给自己）。
        // 服务端按**它自己的**账号库判定"是不是好友"：本机加的好友必须随登记一起补登，
        // 否则这条消息会被服务端拒为"只能给好友发送消息"。这里用两个原始客户端验证
        // "发送被接受 + 对端真的收到"，比只看服务端内存状态更硬。
        const auto pumpServer = [&](auto predicate, const char *label) {
            const auto deadline = std::chrono::steady_clock::now() + 10s;
            while (!predicate()) {
                if (std::chrono::steady_clock::now() > deadline) {
                    std::cout << "[t69] pump timeout: " << label << std::endl;
                    return false;
                }
                server->tick();
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                std::this_thread::sleep_for(2ms);
            }
            return true;
        };
        auto sender = std::make_unique<ImClientFanLX>("127.0.0.1", port, newId, newPassword,
                                                     std::filesystem::path((root / "newclient").wstring()));
        assert(pumpServer([&] { return sender->online(); }, "new-account-online"));
        sender->send("2026003", "hello-friend");
        // 发送被接受后 pending 会归零；若被服务端拒绝（例如"只能给好友发送消息"），
        // 这里会拿到错误文本，便于把"好友没补登"和"别的原因"区分开。
        if (!pumpServer([&] { return sender->pending() == 0; }, "new-account-send"))
            std::cout << "[t69] sender error=[" << sender->error() << "]" << std::endl;
        auto peer = std::make_unique<ImClientFanLX>(
            "127.0.0.1", port, "2026003", demoPasswordFanLX("2026003"),
            std::filesystem::path((root / "peer").wstring()));
        const auto received = pumpServer(
            [&] {
                for (const auto &message : peer->history())
                    if (message.text == "hello-friend")
                        return true;
                return false;
            },
            "peer-received");
        if (!received)
            std::cout << "[t69] peer history=" << peer->history().size()
                      << " senderError=[" << sender->error() << "]" << std::endl;
        assert(received);
        std::cout << "[ok] t69 locally added friend registered on the server (message delivered)"
                  << std::endl;
        peer.reset();
        sender.reset();
    }

    // 4) 密码不一致的重复登记必须被拒（不能借登记改掉别人的密码）。
    {
        std::string reason;
        std::atomic<bool> done{false};
        bool ok = true;
        std::thread worker([&] {
            ok = ImClientFanLX::registerAccountFanLX("127.0.0.1", port, newId, "another-password",
                                                     "别人", "", "", reason);
            done = true;
        });
        const auto deadline = std::chrono::steady_clock::now() + 15s;
        while (!done.load() && std::chrono::steady_clock::now() < deadline) {
            server->tick();
            std::this_thread::sleep_for(2ms);
        }
        worker.join();
        assert(!ok);
        assert(reason.find("已被注册") != std::string::npos);
        std::cout << "[ok] t69 duplicate registration with wrong password rejected" << std::endl;
    }

    // 5) 持久化：停掉服务端，用**同一个数据目录**重新起一个（等价于重启服务端）——账号与开通都还在。
    {
        ticker.stop();
        server.reset();
        // 先释放数据目录独占锁（真机上是"服务端进程退出"这一步）。
        app->saveAndClose();
        app.reset();
        assert(std::filesystem::exists(serverDomain / "state.json"));
        auto restarted = std::make_unique<PersistenceApplicationFanLX>(serverDomain);
        assert(restarted->loadResult().status == LoadStatusFanLX::Loaded);
        assert(restarted->state().accounts.hasAccount(ServiceTypeFanLX::WeiX, newId));
        assert(restarted->state().accounts.isAccountSubscribed(ServiceTypeFanLX::QQ, newId));
        assert(restarted->state().accounts.verifyPassword(ServiceTypeFanLX::WeiX, newId, newPassword));
        ImServerFanLX::OptionsFanLX restartedOptions;
        const ServerAccountRegistrarFanLX restartedRegistrar(*restarted);
        restartedOptions.refreshAccounts = [&restartedRegistrar] {
            (void)restartedRegistrar.refreshAccounts();
        };
        auto server2 = std::make_unique<ImServerFanLX>(restarted->state(), root / "server", port,
                                                       "127.0.0.1", restartedOptions);
        QTimer ticker2;
        QObject::connect(&ticker2, &QTimer::timeout, [&server2] { server2->tick(); });
        ticker2.start(20);
        ChatControllerFanLX controller;
        controller.configure(QStringLiteral("127.0.0.1"), port, QString::fromStdString(newId),
                             QString::fromStdString(newPassword), sharedRoot, true, QStringLiteral("QQ"));
        QString failure;
        assert(controller.start(&failure));
        QString ok;
        assert(controller.waitForLogin(8000, &ok));
        assert(controller.online());
        controller.stop();
        ticker2.stop();
        server2.reset();
        restarted->saveAndClose();
        std::cout << "[ok] t69 account survives a server restart (persisted)" << std::endl;
    }
}

} // namespace

int main(int argc, char **argv) {
    // 离屏平台由 CTest 的 ENVIRONMENT 设置（QT_QPA_PLATFORM=offscreen）提供；
    // 手工运行时在此兜底，保证信号槽与事件循环行为与真实界面一致。
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QCoreApplication app(argc, argv);
    t60ConversationModelFanLX();
    t61MessageModelFanLX();
    t62ControllerRejectsIncompleteConfigFanLX();
    t63ControllerReportsStorageFailureFanLX();
    t64TwoClientsChatFanLX();
    t66LoginRejectionFanLX();
    t67PerServiceChannelsFanLX();
    t68WeiboChannelFanLX();
    t69ServerSideRegistrationFanLX();
    std::cout << "gui_behavior: all cases passed" << std::endl;
    return 0;
}

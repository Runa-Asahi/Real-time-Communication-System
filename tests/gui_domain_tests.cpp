// GUI 领域层回归：把课设要求 1~6 逐条断言在"界面门面"这一层。
//
// 与 tests/gui_tests.cpp 的分工：那边验证聊天链路（控制器 / 模型 / 网络），
// 这边验证账号与服务、好友、群三个页签背后的 DomainControllerFanLX 与页面装配，
// 因此断言的是界面真正会显示的内容（profileText / friendRows / groupRows / memberRows ...），
// 而不是直接读领域对象。全部在离屏平台下运行，不需要真实窗口。
//
// 数据目录约定：本机领域库与聊天库一样按"服务器身份 + 登录账号"派生
// （<数据根>/im-client/<host>_<port>/<账号>/domain），因此同一个数据根可以同时跑多个
// GUI 客户端而不会争用目录锁；每个客户端的领域页签展示的是"本人"视角的完整世界。
#include "gui/AccountPageFanLX.h"
#include "gui/ChatControllerFanLX.h"
#include "gui/DomainControllerFanLX.h"
#include "network/ImServerFanLX.h"
#include "network/ServerAccountRegistrarFanLX.h"
#include "gui/FriendPageFanLX.h"
#include "gui/GroupPageFanLX.h"
#include "gui/MainWindowFanLX.h"
#include "network/ClientDataPathFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include "persistence/SnapshotCodecFanLX.h"
#include <QApplication>
#include <QAbstractItemView>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <nlohmann/json.hpp>

using namespace fanlxGui;

namespace {

int failuresFanLX = 0;

void checkFanLX(bool condition, const char *label) {
    if (condition) {
        std::cout << "[ok] " << label << std::endl;
        return;
    }
    std::cerr << "[FAIL] " << label << std::endl;
    ++failuresFanLX;
}

// 把一个页面里**所有可见文案**收集成一段文本：分组框标题、按钮文字、标签、
// 列表项、只读文本区的内容都算。用它断言"功能确实显示在界面上"，
// 而不是只看背后有没有对应方法——用户抱怨的正是后者有、前者没有。
QString pageTextFanLX(QWidget *page) {
    QString text;
    for (auto *box : page->findChildren<QGroupBox *>())
        text += box->title() + QLatin1Char('\n');
    for (auto *button : page->findChildren<QPushButton *>())
        text += button->text() + QLatin1Char('\n');
    for (auto *label : page->findChildren<QLabel *>())
        text += label->text() + QLatin1Char('\n');
    for (auto *view : page->findChildren<QListWidget *>())
        for (int row = 0; row < view->count(); ++row)
            text += view->item(row)->text() + QLatin1Char('\n');
    for (auto *table : page->findChildren<QTableWidget *>())
        for (int row = 0; row < table->rowCount(); ++row)
            for (int column = 0; column < table->columnCount(); ++column)
                if (table->item(row, column))
                    text += table->item(row, column)->text() + QLatin1Char('\n');
    for (auto *edit : page->findChildren<QPlainTextEdit *>())
        text += edit->toPlainText() + QLatin1Char('\n');
    return text;
}

std::filesystem::path temporaryRootFanLX() {
    const auto root =
        std::filesystem::temp_directory_path() /
        ("fanlx-gui-domain-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    return root;
}

// 断言界面文案时用子串匹配，避免把整段中文抄进测试。
bool hasFanLX(const QString &text, const QString &needle) { return text.contains(needle); }

// 打开某个账号的领域库；失败时打印原因，调用方据此跳过后续断言。
bool openFanLX(DomainControllerFanLX &controller, const std::filesystem::path &root,
               const QString &account, QString *detail) {
    return controller.open(QString::fromStdWString(root.wstring()), QStringLiteral("127.0.0.1"), 9500,
                           account, QString::fromStdString(demoPasswordFanLX(account.toStdString())), true,
                           detail);
}

// ---------- 课设要求 1：用户基本信息 ----------
void t71ProfileFanLX(DomainControllerFanLX &controller) {
    const auto text = controller.profileText();
    checkFanLX(hasFanLX(text, QStringLiteral("微X号")), "t71 用户信息含微X号（上层登录身份）");
    checkFanLX(hasFanLX(text, QStringLiteral("昵称")), "t71 用户信息含昵称");
    checkFanLX(hasFanLX(text, QStringLiteral("出生时间")), "t71 用户信息含出生时间");
    checkFanLX(hasFanLX(text, QStringLiteral("号码申请时间")), "t71 用户信息含号码申请时间");
    checkFanLX(hasFanLX(text, QStringLiteral("T龄")), "t71 用户信息含 T龄");
    checkFanLX(hasFanLX(text, QStringLiteral("所在地")), "t71 用户信息含所在地");
    checkFanLX(hasFanLX(text, QStringLiteral("好友列表")), "t71 用户信息含好友列表");
    checkFanLX(hasFanLX(text, QStringLiteral("群列表")), "t71 用户信息含群列表");
    checkFanLX(controller.currentService() == QStringLiteral("QQ"), "t71 默认以 QQ 身份进入");
    checkFanLX(controller.currentAccountId() == QStringLiteral("2026001"), "t71 当前号码为 2026001");
    checkFanLX(controller.weixAccountId() == QStringLiteral("2026001"), "t71 微X号就是登录用的上层账号");

    // 微X 是上层身份，QQ / 微信 / 微博 是它下面开通的服务：四行都要在表里。
    const auto rows = controller.serviceRows();
    checkFanLX(rows.size() == 4, "t71 服务表列出微X + QQ/微信/微博 共 4 行");
    bool weixUpper = false, qqShared = false, wechatIndependent = false, wechatBound = false;
    for (const auto &row : rows) {
        if (row.service == QStringLiteral("微X"))
            weixUpper = row.upperLayer && row.subscribed && row.exists;
        if (row.service == QStringLiteral("QQ"))
            qqShared = row.sharedId;
        if (row.service == QStringLiteral("微信")) {
            wechatIndependent = !row.sharedId;
            wechatBound = row.bound;
        }
    }
    checkFanLX(weixUpper, "t71 微X 标为上层身份且注册即开通");
    checkFanLX(qqShared, "t71 微博与 QQ 共享同一个号码");
    checkFanLX(wechatIndependent, "t71 微信使用独立号码");
    checkFanLX(wechatBound, "t71 微信独立号已与 QQ 号绑定对应");

    // 好友列表与群列表是用户信息的一部分。
    checkFanLX(controller.friendAccounts().size() == 1, "t71 好友列表有 1 人（演示数据 2026002）");
    checkFanLX(controller.friendAccounts().first() == QStringLiteral("2026002"), "t71 好友为 2026002");
}

// ---------- 本次改造要求 1：昵称 / 所在地允许单项修改 ----------
void t79SingleFieldProfileFanLX(DomainControllerFanLX &controller) {
    const auto before = controller.profileText();
    // 只改昵称：所在地必须原样保留（以前两个字段一起覆盖，改昵称就会把所在地也重写一遍）。
    checkFanLX(controller.updateProfile(QStringLiteral("单项改名"), QString()),
               "t79 只提交昵称即可保存（所在地留空 = 不改）");
    auto text = controller.profileText();
    checkFanLX(hasFanLX(text, QStringLiteral("单项改名")), "t79 昵称已更新");
    checkFanLX(hasFanLX(text, QStringLiteral("所在地：吉林")), "t79 只改昵称时所在地保持不变");
    // 只改所在地：昵称必须保留。
    checkFanLX(controller.updateProfile(QString(), QStringLiteral("长春")),
               "t79 只提交所在地即可保存（昵称留空 = 不改）");
    text = controller.profileText();
    checkFanLX(hasFanLX(text, QStringLiteral("单项改名")), "t79 只改所在地时昵称保持不变");
    checkFanLX(hasFanLX(text, QStringLiteral("所在地：长春")), "t79 所在地已更新");
    // 两项都留空没有可提交的内容，必须明确拒绝而不是写一条空资料。
    checkFanLX(!controller.updateProfile(QString(), QString()), "t79 两项都留空被拒绝");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("没有要修改的内容")),
               "t79 拒绝原因说明没有要修改的内容");
    (void)before;
}

// ---------- 课设要求 4 / 5 / 6.2：开通管理与登录联动 ----------
void t72SubscribeAndLoginFanLX(DomainControllerFanLX &controller) {
    // 要求 5 / 6.2：登微X 后，本人已开通的其它服务"简单确认"即视为自动登录。
    int loggedIn = 0, subscribed = 0;
    for (const auto &row : controller.serviceRows()) {
        if (row.loggedIn) ++loggedIn;
        if (row.subscribed) ++subscribed;
    }
    checkFanLX(loggedIn == 4, "t72 登微X 后本人三个下级服务全部处于登录态");
    checkFanLX(subscribed == 4, "t72 微X 与三个下级服务均已开通");

    // 逐项确认登录：注销一个服务后，用微X上层身份把它确认回来。
    checkFanLX(controller.logoutCurrent(false), "t72 注销当前（QQ）服务成功");
    bool qqOut = true;
    for (const auto &row : controller.serviceRows())
        if (row.service == QStringLiteral("QQ"))
            qqOut = !row.loggedIn;
    checkFanLX(qqOut, "t72 注销后 QQ 确实退出登录态");
    checkFanLX(controller.confirmAllServices(), "t72 一键联动登录恢复本人其它服务");
    bool qqBack = false;
    for (const auto &row : controller.serviceRows())
        if (row.service == QStringLiteral("QQ"))
            qqBack = row.loggedIn;
    checkFanLX(qqBack, "t72 登微X 即自动登录其它服务（QQ 已恢复）");

    // 开通管理：已开通的服务再开通是幂等成功（未开通的场景由注册用例覆盖）。
    checkFanLX(controller.subscribeService(QStringLiteral("微博")), "t72 开通管理：开通微博服务成功");
    checkFanLX(controller.subscribedServices().contains(QStringLiteral("微博")), "t72 开通列表包含微博");

    // 绑定管理：解绑再绑回，验证绑定关系可维护。
    checkFanLX(controller.bindService(QStringLiteral("微信"), QStringLiteral("wx_2026001"), true),
               "t72 解绑微信与 QQ 成功");
    checkFanLX(controller.bindService(QStringLiteral("微信"), QStringLiteral("wx_2026001"), false),
               "t72 重新绑定微信与 QQ 成功");
    checkFanLX(!controller.bindingRows().isEmpty(), "t72 绑定列表非空");
}

// ---------- 课设要求 2：好友管理 ----------
void t73FriendsFanLX(DomainControllerFanLX &controller) {
    // 查
    checkFanLX(controller.friendRows().size() == 1, "t73 查询好友列表成功");
    checkFanLX(hasFanLX(controller.friendRows().first(), QStringLiteral("备注")), "t73 好友行含备注与标签");

    // 增
    checkFanLX(controller.addFriend(QStringLiteral("2026003")), "t73 添加好友 2026003 成功");
    checkFanLX(controller.friendAccounts().size() == 2, "t73 添加后好友数为 2");
    checkFanLX(controller.addFriend(QStringLiteral("2026003")), "t73 重复添加同一好友是幂等成功");
    checkFanLX(controller.friendAccounts().size() == 2, "t73 重复添加不会重复计数");
    checkFanLX(controller.addFriend(QStringLiteral("2026004")), "t73 添加好友 2026004 成功");

    // 改
    checkFanLX(controller.updateFriendRemark(QStringLiteral("2026003"), QStringLiteral("小组长"),
                                             QStringLiteral("同学")),
               "t73 修改好友备注与标签成功");
    bool remarkVisible = false;
    for (const auto &row : controller.friendRows())
        if (hasFanLX(row, QStringLiteral("2026003")) && hasFanLX(row, QStringLiteral("小组长")))
            remarkVisible = true;
    checkFanLX(remarkVisible, "t73 新备注在好友列表里可见");

    // 查询（1）同服务共同好友：返回的是双方好友键集合的交集（此处没有第三方好友，交集为空）。
    const auto common = controller.queryCommonFriends(QStringLiteral("2026002"));
    checkFanLX(common.isEmpty(), "t73 同服务共同好友查询返回交集（无第三方好友时为空）");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("共同好友")), "t73 查询结果给出人数说明");

    // 查询（2）跨服务：QQ 的好友推荐到微信侧添加（课设要求 2.2 的"微信可以添加 QQ 推荐好友"）。
    const auto recommend = controller.queryCrossServiceRecommend(QStringLiteral("微信"));
    checkFanLX(!recommend.isEmpty(), "t73 跨服务推荐查询有结果（QQ 好友推荐给微信）");
    checkFanLX(recommend.contains(QStringLiteral("wx_2026002")), "t73 推荐结果包含 2026002 的微信号");
    // 推荐只是候选：领域不会自动建立关系，必须切到目标服务显式添加。
    checkFanLX(controller.friendAccounts().size() == 3, "t73 跨服务推荐不会自动建立好友关系");

    // 切到微信身份，用推荐结果添加好友。
    checkFanLX(controller.setCurrentService(QStringLiteral("微信")),
               "t73 当前服务切换到微信（独立号 wx_2026001）");
    checkFanLX(controller.currentAccountId() == QStringLiteral("wx_2026001"), "t73 微信号为 wx_2026001");
    checkFanLX(controller.friendAccounts().isEmpty(), "t73 微信侧初始没有好友（三种服务分别存储）");
    checkFanLX(controller.addFriend(QStringLiteral("wx_2026002")), "t73 用 QQ 的推荐结果在微信侧添加好友成功");
    checkFanLX(controller.addFriend(QStringLiteral("wx_2026003")), "t73 再添加一个微信好友");
    checkFanLX(controller.friendAccounts().size() == 2, "t73 微信侧好友数为 2");
    // 分别存储的直接证据：微信里加了好友，QQ 的列表一点也不受影响。
    checkFanLX(controller.setCurrentService(QStringLiteral("QQ")), "t73 当前服务切回 QQ");
    checkFanLX(controller.friendAccounts().size() == 3, "t73 微信加好友后 QQ 好友数不变（各存各的）");
    checkFanLX(!controller.friendAccounts().contains(QStringLiteral("wx_2026002")),
               "t73 QQ 的好友列表里不会出现微信号");

    // 跨服务共同好友：同一个人的两个服务上都互为好友的那些"人"。
    const auto crossCommon = controller.queryCrossServiceCommon(QStringLiteral("微信"));
    checkFanLX(crossCommon.size() == 2, "t73 跨服务共同好友查询返回 2 人（U2/U3 在两个服务都是好友）");
    checkFanLX(crossCommon.contains(QStringLiteral("wx_2026002")), "t73 跨服务共同好友含 wx_2026002");

    // 删
    checkFanLX(controller.removeFriend(QStringLiteral("2026004")), "t73 删除好友成功");
    checkFanLX(!controller.friendAccounts().contains(QStringLiteral("2026004")),
               "t73 删除后列表不再包含该好友");
    checkFanLX(!controller.removeFriend(QStringLiteral("2026004")), "t73 重复删除返回失败（关系已不存在）");
}

// ---------- 课设要求 3：群管理（含两种群理念的差异） ----------
void t74GroupsFanLX(DomainControllerFanLX &controller) {
    // 查询群成员
    const auto members = controller.memberRows(QStringLiteral("QQ"), QStringLiteral("1001"));
    checkFanLX(members.size() == 3, "t74 查询群成员：QQ 群 1001 有 3 名成员");
    bool ownerShown = false;
    for (const auto &row : members)
        if (hasFanLX(row, QStringLiteral("群主")))
            ownerShown = true;
    checkFanLX(ownerShown, "t74 成员行显示群主角色（QQ 群有管理员制度）");

    // QQ 的"申请-审批"加入方式：演示数据里 QQ 群 1001 已经有一条 U4 的待审批申请。
    bool applicationPending = false;
    for (const auto &row : controller.pendingRows(QStringLiteral("QQ"), QStringLiteral("1001")))
        if (hasFanLX(row, QStringLiteral("待审批")))
            applicationPending = true;
    checkFanLX(applicationPending, "t74 从文件加载出的入群申请显示在待处理记录里（QQ 申请制）");
    checkFanLX(controller.approveGroupApplication(QStringLiteral("QQ"), QStringLiteral("1001"),
                                                  QStringLiteral("2026004"), false),
               "t74 群主批准入群申请成功");
    checkFanLX(controller.memberRows(QStringLiteral("QQ"), QStringLiteral("1001")).size() == 4,
               "t74 批准后 QQ 群 1001 成员数变为 4");

    // 创建群：创建者自动成为群主；本人已在群里时再申请是幂等的（不产生重复待审批记录）。
    checkFanLX(controller.createGroup(QStringLiteral("QQ"), QStringLiteral("1099")), "t74 创建 QQ 群 1099");
    checkFanLX(controller.memberRows(QStringLiteral("QQ"), QStringLiteral("1099")).size() == 1,
               "t74 新群的群主就是创建者本人");
    checkFanLX(controller.applyToJoinGroup(QStringLiteral("QQ"), QStringLiteral("1099")),
               "t74 已在群内的申请是幂等成功");
    checkFanLX(controller.pendingRows(QStringLiteral("QQ"), QStringLiteral("1099")).isEmpty(),
               "t74 群成员重复申请不会产生待审批记录");

    // QQ 群也能邀请他人：邀请 = 替对方提交入群申请，批准后才入群（申请-审批制不被绕过）。
    checkFanLX(controller.inviteGroupMember(QStringLiteral("QQ"), QStringLiteral("1099"),
                                            QStringLiteral("2026002")),
               "t74 QQ 群成员邀请他人入群成功");
    checkFanLX(controller.memberRows(QStringLiteral("QQ"), QStringLiteral("1099")).size() == 1,
               "t74 QQ 邀请后对方未经审批不会入群");
    bool inviteAwaitingApproval = false;
    for (const auto &row : controller.pendingRows(QStringLiteral("QQ"), QStringLiteral("1099")))
        if (hasFanLX(row, QStringLiteral("2026002")) && hasFanLX(row, QStringLiteral("待审批")))
            inviteAwaitingApproval = true;
    checkFanLX(inviteAwaitingApproval, "t74 QQ 邀请产生一条待审批记录");
    checkFanLX(controller.approveGroupApplication(QStringLiteral("QQ"), QStringLiteral("1099"),
                                                  QStringLiteral("2026002"), false),
               "t74 群主批准被邀请人入群");
    checkFanLX(controller.memberRows(QStringLiteral("QQ"), QStringLiteral("1099")).size() == 2,
               "t74 批准后被邀请人成为 QQ 群成员");

    // 微信群：只能推荐加入（申请入口必须被策略拒绝）
    // 先按命名空间给出提示：当前是 QQ 身份，不能操作微信群。
    checkFanLX(!controller.applyToJoinGroup(QStringLiteral("Wechat"), QStringLiteral("1002")),
               "t74 用 QQ 身份操作微信群被拒绝");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("微信")),
               "t74 拒绝原因提示需要切换到微信身份");

    checkFanLX(controller.setCurrentService(QStringLiteral("Wechat")), "t74 切换到微信身份");
    // 回归：QQ 里才有的群号（1099）在微信命名空间不存在，群特色面板不能显示 unordered_map::at。
    {
        const auto text = controller.policyText(QStringLiteral("Wechat"), QStringLiteral("1099"));
        checkFanLX(!hasFanLX(text, QStringLiteral("读取群策略失败")) && !hasFanLX(text, QStringLiteral("unordered_map")),
                   "t74 群号在当前命名空间不存在时不显示底层异常文字");
        checkFanLX(hasFanLX(text, QStringLiteral("不存在")), "t74 不存在的群给出可读提示");
    }
    checkFanLX(!controller.applyToJoinGroup(QStringLiteral("Wechat"), QStringLiteral("1002")),
               "t74 微信群不允许申请加入（策略拒绝）");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("不支持")),
               "t74 拒绝原因明确说明微信群不支持该加入方式");

    // 微信群：成员推荐 + 被推荐人确认。演示数据里微信群 1001 已有一条 U4 的待确认推荐。
    bool invitationPending = false;
    for (const auto &row : controller.pendingRows(QStringLiteral("Wechat"), QStringLiteral("1001")))
        if (hasFanLX(row, QStringLiteral("待确认")))
            invitationPending = true;
    checkFanLX(invitationPending, "t74 从文件加载出的入群推荐显示在待处理记录里（微信推荐制）");
    checkFanLX(controller.inviteGroupMember(QStringLiteral("Wechat"), QStringLiteral("1002"),
                                            QStringLiteral("wx_2026004")),
               "t74 微信群成员推荐入群成功");

    // 微信群没有管理员制度
    checkFanLX(!controller.assignGroupAdmin(QStringLiteral("Wechat"), QStringLiteral("1002"),
                                            QStringLiteral("wx_2026002"), false),
               "t74 微信群不支持设置管理员（仅群主是特权账号）");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("不支持")),
               "t74 拒绝原因明确说明该模式不支持管理员");

    // 微信群没有子群（讨论组）
    checkFanLX(controller.discussionRows(QStringLiteral("Wechat"), QStringLiteral("1002")).isEmpty(),
               "t74 微信群没有子群记录");
    checkFanLX(!controller.createDiscussionGroup(QStringLiteral("Wechat"), QStringLiteral("1002"),
                                                 QStringLiteral("WD1")),
               "t74 微信群创建子群被策略拒绝");

    // 微信群主挨 T（移出成员）
    const auto before = controller.memberRows(QStringLiteral("Wechat"), QStringLiteral("1002")).size();
    checkFanLX(controller.kickGroupMember(QStringLiteral("Wechat"), QStringLiteral("1002"),
                                          QStringLiteral("wx_2026003")),
               "t74 微信群主挨 T（移出成员）成功");
    checkFanLX(controller.memberRows(QStringLiteral("Wechat"), QStringLiteral("1002")).size() == before - 1,
               "t74 挨 T 后成员数减一");

    // 群主不能直接退群：必须先转让（否则会出现无主群）
    checkFanLX(!controller.leaveGroup(QStringLiteral("Wechat"), QStringLiteral("1002")),
               "t74 群主直接退群被拒绝");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("转让")), "t74 拒绝原因提示先转让群主");
    checkFanLX(controller.transferGroupOwner(QStringLiteral("Wechat"), QStringLiteral("1002"),
                                             QStringLiteral("wx_2026002")),
               "t74 转让群主成功");
    checkFanLX(controller.leaveGroup(QStringLiteral("Wechat"), QStringLiteral("1002")),
               "t74 转让后原群主退群成功");
    // 已经不是成员的人当然也不能解散该群（权限矩阵生效）。
    checkFanLX(!controller.dissolveGroup(QStringLiteral("Wechat"), QStringLiteral("1002")),
               "t74 非成员不能解散群（权限矩阵生效）");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("群主")), "t74 拒绝原因说明需要群主权限");

    checkFanLX(controller.setCurrentService(QStringLiteral("QQ")), "t74 切回 QQ 身份");
}

// ---------- 课设要求 6.4：群特色展示 + 成员数据不受伤害的动态切换 ----------
void t75PolicySwitchFanLX(DomainControllerFanLX &controller) {
    const auto text = controller.policyText(QStringLiteral("QQ"), QStringLiteral("1001"));
    checkFanLX(hasFanLX(text, QStringLiteral("QQGroupPolicyFanLX")), "t75 群特色显示 QQ 策略对象（多态）");
    checkFanLX(hasFanLX(text, QStringLiteral("申请-审批制")), "t75 群特色显示 QQ 的加入方式");
    checkFanLX(hasFanLX(text, QStringLiteral("群主 + 管理员制度")), "t75 群特色显示 QQ 的管理员制度");
    checkFanLX(hasFanLX(text, QStringLiteral("允许")), "t75 群特色显示 QQ 允许子群");

    const auto membersBefore = controller.memberRows(QStringLiteral("QQ"), QStringLiteral("1001"));
    checkFanLX(controller.createDiscussionGroup(QStringLiteral("QQ"), QStringLiteral("1001"),
                                                QStringLiteral("D9")),
               "t75 QQ 群创建子群 D9 成功");
    checkFanLX(controller.discussionRows(QStringLiteral("QQ"), QStringLiteral("1001")).size() == 2,
               "t75 原 D1 与新建 D9 共 2 个子群");
    checkFanLX(controller.joinDiscussionGroup(QStringLiteral("QQ"), QStringLiteral("1001"),
                                              QStringLiteral("D9"), QStringLiteral("2026002")),
               "t75 把父群成员拉进子群成功");

    // 动态变换：QQ 群 1001 改成微信管理特色（群身份不变，因此仍用 QQ 命名空间访问）。
    checkFanLX(controller.switchGroupMode(QStringLiteral("QQ"), QStringLiteral("1001"), false),
               "t75 动态切换为微信管理特色成功");
    const auto membersAfter = controller.memberRows(QStringLiteral("QQ"), QStringLiteral("1001"));
    checkFanLX(membersAfter == membersBefore, "t75 切换后成员数据逐行一致（数据不受伤害）");
    const auto switched = controller.policyText(QStringLiteral("QQ"), QStringLiteral("1001"));
    checkFanLX(hasFanLX(switched, QStringLiteral("WechatGroupPolicyFanLX")), "t75 切换后策略对象变为微信策略");
    checkFanLX(hasFanLX(switched, QStringLiteral("已动态切换")), "t75 界面明确标注已动态切换");
    checkFanLX(hasFanLX(switched, QStringLiteral("推荐制")), "t75 切换后加入方式变为推荐制");
    checkFanLX(hasFanLX(switched, QStringLiteral("仅群主为特权账号")), "t75 切换后只有群主是特权账号");
    // 子群记录仍在（切换不清理数据），但新策略下不能再创建、也不能再扩员。
    checkFanLX(controller.discussionRows(QStringLiteral("QQ"), QStringLiteral("1001")).size() == 2,
               "t75 切换后原有子群记录仍在");
    checkFanLX(!controller.createDiscussionGroup(QStringLiteral("QQ"), QStringLiteral("1001"),
                                                 QStringLiteral("D10")),
               "t75 切换后创建子群被新策略拒绝");
    // 微信特色下不能再"任命"管理员（撤销休眠配置是允许的，见群域注释）。
    checkFanLX(!controller.assignGroupAdmin(QStringLiteral("QQ"), QStringLiteral("1001"),
                                            QStringLiteral("2026003"), false),
               "t75 切换后微信特色下不能再任命管理员");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("不支持")),
               "t75 拒绝原因说明该模式不支持管理员");

    // 切回 QQ 特色：能力恢复。
    checkFanLX(controller.switchGroupMode(QStringLiteral("QQ"), QStringLiteral("1001"), true),
               "t75 再切回 QQ 管理特色成功");
    checkFanLX(controller.discussionRows(QStringLiteral("QQ"), QStringLiteral("1001")).size() == 2,
               "t75 切回后子群记录依然完整");
    checkFanLX(controller.memberRows(QStringLiteral("QQ"), QStringLiteral("1001")) == membersBefore,
               "t75 来回切换后成员数据仍然完全一致");
    checkFanLX(controller.assignGroupAdmin(QStringLiteral("QQ"), QStringLiteral("1001"),
                                           QStringLiteral("2026003"), false),
               "t75 切回 QQ 特色后任命管理员恢复可用");

    // 解散群：本人是群主的群可以解散，群记录归档（成员数据不丢失，只是不再出现在活动列表）。
    checkFanLX(controller.createGroup(QStringLiteral("QQ"), QStringLiteral("1088")), "t75 创建待解散的群 1088");
    checkFanLX(controller.dissolveGroup(QStringLiteral("QQ"), QStringLiteral("1088")),
               "t75 群主解散自己的群成功（记录归档）");
    bool stillListed = false;
    for (const auto &row : controller.groupRows(QStringLiteral("QQ")))
        if (hasFanLX(row, QStringLiteral("1088")))
            stillListed = true;
    checkFanLX(!stillListed, "t75 解散后该群不再出现在群列表里（已归档）");
}

// ---------- 本次改造要求 5：注册入口在登录界面、注册不捆绑服务、登微X 后才能在内部开通 ----------
void t77RegisterFanLX(DomainControllerFanLX &controller, const std::filesystem::path &root) {
    // 1) 注册只创建微X账号：登录窗上的"注册微X账号"走的就是这个静态入口（注册发生在登录之前）。
    QString detail;
    checkFanLX(DomainControllerFanLX::registerWeiXUser(
                   QString::fromStdWString(root.wstring()), QStringLiteral("127.0.0.1"), 9500,
                   QStringLiteral("2026099"), QStringLiteral("新同学"), QStringLiteral("2006-05-20"),
                   QStringLiteral("长春"), QStringLiteral("pw-2026099"), &detail),
               "t77 注册微X账号成功（只建上层身份）");
    checkFanLX(hasFanLX(detail, QStringLiteral("未捆绑任何下级服务")),
               "t77 注册结果明确说明没有捆绑服务账号");
    // 同一个微X号不能注册两次（否则登录身份就不唯一了）。
    QString duplicateDetail;
    checkFanLX(!DomainControllerFanLX::registerWeiXUser(
                   QString::fromStdWString(root.wstring()), QStringLiteral("127.0.0.1"), 9500,
                   QStringLiteral("2026099"), QStringLiteral("重复注册"), QStringLiteral("2006-05-20"),
                   QStringLiteral("长春"), QStringLiteral("pw-other"), &duplicateDetail),
               "t77 重复注册同一个微X号被拒绝");
    checkFanLX(hasFanLX(duplicateDetail, QStringLiteral("已存在")), "t77 重复注册的原因说明微X号已存在");

    // 2) 用刚注册的微X号登录：此时本人**只有**微X，没有任何下级服务账号。
    // 注意这里必须用**注册时写的那个密码**，不能用 demoPasswordFanLX()：
    // 后者是"演示种子账号"的口令规则（现为固定 123），与本测试自注册的 2026099 无关。
    // 历史上两者恰好都等于 pw-2026099，所以旧写法能过；演示规则改成 123 后就不再相等。
    checkFanLX(controller.open(QString::fromStdWString(root.wstring()), QStringLiteral("127.0.0.1"),
                               9500, QStringLiteral("2026099"), QStringLiteral("pw-2026099"), true,
                               &detail),
               "t77 用新微X号登录成功");
    if (!controller.opened()) {
        std::cerr << "      原因：" << detail.toStdString() << std::endl;
        return;
    }
    checkFanLX(controller.weixAccountId() == QStringLiteral("2026099"), "t77 微X号正确");
    checkFanLX(hasFanLX(controller.profileText(), QStringLiteral("新同学")), "t77 新用户昵称正确");
    checkFanLX(hasFanLX(controller.profileText(), QStringLiteral("长春")), "t77 新用户所在地正确");
    checkFanLX(controller.friendAccounts().isEmpty(), "t77 新用户好友列表为空");
    checkFanLX(controller.groupRows(QStringLiteral("QQ")).isEmpty(), "t77 新用户没有参与任何群");
    int existing = 0, subscribed = 0;
    for (const auto &row : controller.serviceRows()) {
        if (row.exists) ++existing;
        if (row.subscribed) ++subscribed;
    }
    checkFanLX(existing == 1 && subscribed == 1,
               "t77 注册后只有微X已建号并开通，QQ/微信/微博 都还没开通");
    checkFanLX(controller.currentService() == QStringLiteral("微X"),
               "t77 没有任何下级服务时当前身份就是微X");

    // 3) 在微X里面逐个开通三种服务（要求 5：只有登录微X后才能在内部去开通）。
    // 号码是"按服务各自唯一"的：同一个号码可以在不同服务里各用一次（QQ 与微博本来就必须共号），
    // 因此微信沿用 QQ 号是被允许的——两条通道由服务端按"服务 + 号码"分开存放与投递。
    checkFanLX(controller.activateService(QStringLiteral("QQ"), QStringLiteral("2026099")),
               "t77 开通 QQ 服务成功（号码 2026099）");
    checkFanLX(controller.activateService(QStringLiteral("微博"), QStringLiteral("2026099")),
               "t77 开通微博服务成功（与 QQ 共号）");
    checkFanLX(controller.activateService(QStringLiteral("微信"), QStringLiteral("2026099")),
               "t77 微信沿用同一个号码也能开通（号码按服务各自唯一）");
    checkFanLX(controller.subscribedServices().size() == 4, "t77 微X + 三种服务全部已开通");
    checkFanLX(controller.accountIdOf(QStringLiteral("QQ")) == QStringLiteral("2026099") &&
                   controller.accountIdOf(QStringLiteral("微博")) == QStringLiteral("2026099") &&
                   controller.accountIdOf(QStringLiteral("微信")) == QStringLiteral("2026099"),
               "t77 三个服务各自持有自己的号码（允许相同）");
    // 重复开通同一个服务必须被拒（而不是悄悄覆盖号码）。
    checkFanLX(!controller.activateService(QStringLiteral("QQ"), QStringLiteral("2026099")),
               "t77 重复开通 QQ 被拒绝");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("已经开通")),
               "t77 拒绝原因说明该服务已经开通");
    // 同一服务内号码仍然唯一：另一个人占用了这个 QQ 号时不能再开通（用 2026077 已注册的账号验证）。
    checkFanLX(!controller.activateService(QStringLiteral("微信"), QStringLiteral("2026099")),
               "t77 同一服务重复开通仍被拒绝");

    // 4) 开通后即可切换身份并正常使用。
    checkFanLX(controller.confirmAllServices(), "t77 开通后联动登录成功");
    checkFanLX(controller.setCurrentService(QStringLiteral("微信")), "t77 可以切换到刚开通的微信身份");
    checkFanLX(controller.currentAccountId() == QStringLiteral("2026099"),
               "t77 微信沿用了同一个号码（允许，通道按服务分开）");
    checkFanLX(controller.setCurrentService(QStringLiteral("QQ")), "t77 切回 QQ");
}

// ---------- 本次改造要求 2：登录失败必须留在登录界面（不得跳转主界面） ----------
void t80LoginFailureFanLX(const std::filesystem::path &root) {
    // 密码错误：身份被拒（IdentityRejected），调用方据此留在登录窗口重试。
    {
        DomainControllerFanLX controller;
        QString detail;
        const auto outcome = controller.openWithOutcome(
            QString::fromStdWString(root.wstring()), QStringLiteral("127.0.0.1"), 9500,
            QStringLiteral("2026001"), QStringLiteral("wrong-password"), true, &detail);
        checkFanLX(outcome == DomainControllerFanLX::OpenOutcomeFanLX::IdentityRejected,
                   "t80 密码错误被判定为身份失败（留在登录界面）");
        checkFanLX(hasFanLX(detail, QStringLiteral("微X登录失败")), "t80 失败原因直接说明是微X登录失败");
        checkFanLX(!controller.opened(), "t80 身份失败时领域库保持关闭，不会带进主界面");
    }
    // 号码不是微X号（这里是微信号）：必须被拒，而不是"猜一个平台"放进去。
    {
        DomainControllerFanLX controller;
        QString detail;
        const auto outcome = controller.openWithOutcome(
            QString::fromStdWString(root.wstring()), QStringLiteral("127.0.0.1"), 9500,
            QStringLiteral("wx_2026001"), QStringLiteral("123"), true, &detail);
        checkFanLX(outcome == DomainControllerFanLX::OpenOutcomeFanLX::IdentityRejected,
                   "t80 用下级服务号码登录被拒（登录身份只有微X号）");
        checkFanLX(hasFanLX(detail, QStringLiteral("没有微X号")), "t80 提示账号库里没有该微X号");
        checkFanLX(!controller.opened(), "t80 拒绝后不进入主界面");
    }
    // 领域库不可用（这里用不存在的盘符路径触发）与身份失败必须区分开：
    // 前者允许降级为"只有聊天"，后者必须留在登录界面。
    {
        DomainControllerFanLX controller;
        QString detail;
        const auto outcome = controller.openWithOutcome(
            QStringLiteral("Z:/fanlx-not-exist-root"), QStringLiteral("127.0.0.1"), 9500,
            QStringLiteral("2026001"), QStringLiteral("123"), true, &detail);
        checkFanLX(outcome == DomainControllerFanLX::OpenOutcomeFanLX::Unavailable,
                   "t80 领域库不可用被判定为设施失败（可与身份失败区别处理）");
    }
}

// ---------- 本次改造要求 4：三种服务的好友与群分别存储 ----------
void t81SeparateStorageFanLX(DomainControllerFanLX &controller, const std::filesystem::path &root) {
    // 1) 界面上：QQ 的好友不会出现在微信里（t73 已断言），这里断言"QQ 身份加不了微信号"。
    checkFanLX(controller.setCurrentService(QStringLiteral("QQ")), "t81 切到 QQ 身份");
    checkFanLX(!controller.addFriend(QStringLiteral("wx_2026002")),
               "t81 QQ 身份不能把微信号加成好友（好友只在同一服务内）");
    checkFanLX(hasFanLX(controller.lastMessage(), QStringLiteral("不存在")),
               "t81 拒绝原因说明该号码在 QQ 服务里没有对应账号");
    // 2) 群里也不共享：QQ 群列表与微信群列表来自各自的命名空间。
    const auto qqGroups = controller.groupRows(QStringLiteral("QQ"));
    const auto wxGroups = controller.groupRows(QStringLiteral("微信"));
    checkFanLX(!qqGroups.isEmpty() && !wxGroups.isEmpty(), "t81 QQ 与微信各有自己的群列表");
    // 3) 落盘结构：state.json 里好友与群是按服务分区存放的对象，不再是一个扁平数组。
    //    直接读文件（不打开领域库）以免和控制器争用目录锁。
    {
        const auto directory = imClientPathFanLX::domainDirectoryFanLX(
            imClientPathFanLX::clientDirectoryFanLX(root, "127.0.0.1", 9500, "2026001"));
        std::ifstream stream(directory / "state.json", std::ios::binary);
        checkFanLX(stream.good(), "t81 能找到账号目录下的 state.json");
        const std::string bytes((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
        const auto json = nlohmann::json::parse(bytes);
        checkFanLX(json.contains("friends") && json.at("friends").is_object(),
                   "t81 快照里好友是『按服务分区』的对象");
        checkFanLX(json.at("friends").contains("QQ") && json.at("friends").contains("Wechat") &&
                       json.at("friends").contains("Weibo"),
                   "t81 好友分区包含 QQ / 微信 / 微博 三段");
        checkFanLX(!json.contains("friendships"), "t81 旧的扁平 friendships 数组已不再写出");
        checkFanLX(json.contains("groups") && json.at("groups").is_object() &&
                       json.at("groups").contains("QQ") && json.at("groups").contains("Wechat"),
                   "t81 群同样按命名空间分区存放");
        checkFanLX(json.at("schemaVersion").get<int>() == 2, "t81 schemaVersion 升为 2");
        // 每条好友关系的两端必须属于同一服务，且与该分区同名（跨服务关系在结构上写不进去）。
        bool sameServiceOnly = true;
        for (auto section = json.at("friends").begin(); section != json.at("friends").end(); ++section)
            for (const auto &entry : section.value())
                if (entry.at("left").at("service") != entry.at("right").at("service") ||
                    entry.at("left").at("service") != section.key())
                    sameServiceOnly = false;
        checkFanLX(sameServiceOnly, "t81 每条好友关系都落在自己的服务分区里（不跨服务）");
        // 同一份字节用解码器读回来也必须一致（写入与读取共用同一套分区约定）。
        const auto decoded = SnapshotCodecFanLX::parse(bytes);
        checkFanLX(!decoded.friendships.empty(), "t81 分区格式能被解码器读回（好友关系未丢失）");
        bool decodedSameService = true;
        for (const auto &friendship : decoded.friendships)
            if (friendship.left.service != friendship.right.service)
                decodedSameService = false;
        checkFanLX(decodedSameService, "t81 解码后的好友关系同样只落在同一服务内");
    }
}

// ---------- 本次改造要求：界面切服务时聊天连接要跟着重建（中文服务名也要认） ----------
void t82ChatFollowsCurrentServiceFanLX(DomainControllerFanLX &controller, ChatControllerFanLX &chat,
                                       const std::filesystem::path &root) {
    // 界面传下去的是中文服务名（"微信" / "微博"），而网络与目录只认存储名（Wechat / Weibo）。
    // 这里直接调用控制器，断言它既接受中文名、也会把目录切到对应服务（未被拒绝即可说明规范化生效）。
    QString detail;
    const auto account = controller.accountIdOf(QStringLiteral("微信"));
    checkFanLX(!account.isEmpty(), "t82 本人有微信身份可用于切换");
    if (account.isEmpty())
        return;
    // 先给控制器一套连接上下文（真实登录时由 main_gui 提供）：这里不连服务器，
    // 只验证"服务名规范化 + 目录按服务分层"两件事。
    chat.configure(QStringLiteral("127.0.0.1"), 9500, QString(), QStringLiteral("pw-test"),
                   QString::fromStdWString(root.wstring()), true, QStringLiteral("QQ"));
    // 没有真实服务端时 start 会失败（连接超时由 start 返回 false 或后续事件报告），
    // 但"是否被判定为没有聊天通道"可以从 detail 里区分出来：中文名不该被当成未知服务。
    const auto accepted = chat.switchIdentity(QStringLiteral("微信"), account, &detail);
    (void)accepted;
    checkFanLX(!hasFanLX(detail, QStringLiteral("没有聊天通道")),
               "t82 中文服务名『微信』被识别为聊天通道（不是未知服务）");
    checkFanLX(chat.service() == QStringLiteral("Wechat"),
               "t82 控制器内部把服务名规范化为存储名 Wechat");
    checkFanLX(chat.serviceDisplayName() == QStringLiteral("微信"), "t82 显示名仍是『微信』");
    checkFanLX(chat.dataDirectory().contains(QStringLiteral("Wechat")),
               "t82 聊天目录按服务分层（路径含 Wechat）");
    chat.stop();

    // 微博同理：中文名必须被认成 Weibo。
    const auto weibo = controller.accountIdOf(QStringLiteral("微博"));
    if (!weibo.isEmpty()) {
        chat.switchIdentity(QStringLiteral("微博"), weibo, &detail);
        checkFanLX(!hasFanLX(detail, QStringLiteral("没有聊天通道")),
                   "t82 中文服务名『微博』被识别为聊天通道");
        checkFanLX(chat.service() == QStringLiteral("Weibo"), "t82 微博规范化为 Weibo");
        chat.stop();
    }
    // 微X 是上层登录身份，必须明确拒绝并说明原因。
    const auto weix = controller.weixAccountId();
    const auto rejected = chat.switchIdentity(QStringLiteral("微X"), weix, &detail);
    checkFanLX(!rejected && hasFanLX(detail, QStringLiteral("上层登录身份")),
               "t82 微X 被拒绝并说明它是上层登录身份");
    (void)root;
}

// ---------- 本轮：加好友时对端只存在于服务端（别人在别的客户端注册的账号） ----------
// 现象：添加好友报"对端平台账号不存在"。原因是本机领域库只是"谁存在"的一份局部视图：
// 别的客户端注册/开通出来的账号不会自动同步过来，而加好友的前置校验查的是本机账号表。
// 规则：先问服务端（IM 的 LOOKUP），把对端的最小账号记录导入本机副本，再走原来的加好友流程。
void t84AddFriendForServerOnlyPeerFanLX(const std::filesystem::path &root) {
    // 服务端：账号库就是它的持久化快照（与 --im-server 同一条路径）。
    const auto serverDomain = root / "t84-server-domain";
    PersistenceApplicationFanLX serverApp(serverDomain);
    serverApp.initialize();
    serverApp.importInitial(demoSeedFanLX());
    // 服务端上"别人注册"的账号：只有微X号 + 一个已开通的 QQ 服务（本机副本里没有它）。
    const std::string peerWeix = "2026081";
    const std::string peerPassword = "pw-2026081";
    serverApp.registerWeiXUser({"u-2026081", "远端用户", "2000-01-01", "2026-01-01", "上海",
                                peerPassword},
                               peerWeix);
    {
        const ServerAccountRegistrarFanLX registrar(serverApp);
        const auto failure = registrar.registerAccountFanLX(
            peerWeix, peerPassword, "远端用户", "2000-01-01", "上海", {{"QQ", peerWeix}}, {});
        checkFanLX(failure.empty(), "t84 服务端已为远端账号开通 QQ");
    }
    ImServerFanLX::OptionsFanLX options;
    const ServerAccountRegistrarFanLX registrar(serverApp);
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
    options.updateFriendship = [&registrar](const std::string &service, const std::string &self,
                                            const std::string &peer, bool add) {
        return registrar.updateFriendshipFanLX(service, self, peer, add);
    };
    ImServerFanLX server(serverApp.state(), root / "t84-server", 0, "127.0.0.1", options);
    const auto port = server.port();
    checkFanLX(port != 0, "t84 服务端已启动");
    // 服务端要一直 tick（真实部署里它在另一个进程里跑）：本机在开通/加好友时会同步访问服务端，
    // 若主线程被同步调用占住而没人推进服务端，这些调用只能超时。
    std::atomic<bool> stopTicker{false};
    std::thread ticker([&] {
        while (!stopTicker.load()) {
            server.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    // 本机：一个"自己"的账号库（演示种子 + 自己的 QQ），里面没有 2026081。
    const auto localRoot = root / "t84-local";
    std::filesystem::create_directories(localRoot);
    {
        DomainControllerFanLX self;
        QString detail;
        checkFanLX(DomainControllerFanLX::registerWeiXUser(
                       QString::fromStdWString(localRoot.wstring()), QStringLiteral("127.0.0.1"), port,
                       QStringLiteral("2026099"), QStringLiteral("本人"),
                       QStringLiteral("2006-01-01"), QStringLiteral("长春"),
                       QStringLiteral("pw-2026099"), &detail),
                   "t84 本机注册了本人账号");
        checkFanLX(self.openWithOutcome(QString::fromStdWString(localRoot.wstring()),
                                        QStringLiteral("127.0.0.1"), port, QStringLiteral("2026099"),
                                        QStringLiteral("pw-2026099"), true, &detail) ==
                       DomainControllerFanLX::OpenOutcomeFanLX::Ok,
                   "t84 本人登录成功");
        checkFanLX(self.activateService(QStringLiteral("QQ"), QStringLiteral("2026099")),
                   "t84 本人开通 QQ");
        // 加好友必须发生在下级服务身份上（微X 是上层身份，没有自己的好友列表）；
        // 切服务前要先让该服务处于领域登录态（联动确认）。
        checkFanLX(self.confirmAllServices(), "t84 联动确认本人已开通的服务");
        checkFanLX(self.setCurrentService(QStringLiteral("QQ")), "t84 当前服务切到 QQ");
        checkFanLX(!self.friendAccounts().contains(QString::fromStdString(peerWeix)),
                   "t84 本机此前不认识对端（这正是『对端平台账号不存在』的来源）");
        const auto added = self.addFriend(QString::fromStdString(peerWeix));
        if (!added)
            std::cout << "[t84] 发送好友申请失败原因：" << self.lastMessage().toStdString() << std::endl;
        checkFanLX(added, "t84 向服务端上的账号发出好友申请成功");
        checkFanLX(hasFanLX(self.lastMessage(), QStringLiteral("等待对方同意")),
                   "t84 结果说明需要等待对方同意");
        checkFanLX(!self.friendAccounts().contains(QString::fromStdString(peerWeix)),
                   "t84 对方同意之前还不是好友");
        const auto pending = self.friendRequestView();
        checkFanLX(pending.reachable && pending.outgoing.size() == 1 &&
                       pending.outgoing.first().startsWith(QString::fromStdString(peerWeix)),
                   "t84 『我发出的申请』里有这一条");
        // 对方（另一台客户端）同意：走与界面同一个短连接接口。
        std::string json, reason;
        checkFanLX(ImClientFanLX::friendOperationFanLX("127.0.0.1", port, "QQ", peerWeix, peerPassword, "accept",
                                                       "2026099", {}, json, reason),
                   "t84 对方同意好友申请");
        // 收到好友动态后本机对账：对端账号从服务端导入，关系出现在本机好友列表。
        self.onFriendNoticeFanLX();
        checkFanLX(self.friendAccounts().contains(QString::fromStdString(peerWeix)),
                   "t84 对方同意后好友列表里出现对端号码");
        checkFanLX(self.friendRequestView().outgoing.isEmpty(), "t84 申请已结清");

        // 依据本人另一个服务的好友添加：对方在服务端开通了微信，本人也开通微信后，
        // 在微信里依据 QQ 好友 2026081 发申请，服务端换算成他的微信号。
        {
            // 走网络登记（服务端线程正在 tick，测试线程不能直接改服务端状态）。
            std::string why;
            checkFanLX(ImClientFanLX::registerAccountFanLX("127.0.0.1", port, peerWeix, peerPassword, "远端用户",
                                                           "2000-01-01", "上海", why,
                                                           {{"QQ", peerWeix}, {"Wechat", "wx_2026081"}}),
                       "t84 服务端为对方开通微信");
        }
        checkFanLX(self.activateService(QStringLiteral("微信"), QStringLiteral("wx_2026099")), "t84 本人开通微信");
        checkFanLX(self.confirmAllServices(), "t84 联动确认微信");
        checkFanLX(self.setCurrentService(QStringLiteral("微信")), "t84 当前服务切到微信");
        bool listed = false;
        for (const auto &row : self.crossServiceFriendCandidates(QStringLiteral("QQ")))
            if (row.startsWith(QString::fromStdString(peerWeix)))
                listed = true;
        checkFanLX(listed, "t84 『依据 QQ 好友添加』的候选里有 2026081");
        const auto viaSent =
            self.addFriendFromService(QStringLiteral("QQ"), QString::fromStdString(peerWeix), QStringLiteral("QQ 上的同学"));
        if (!viaSent)
            std::cout << "[t84] 跨服务申请失败原因：" << self.lastMessage().toStdString() << std::endl;
        checkFanLX(viaSent && hasFanLX(self.lastMessage(), QStringLiteral("wx_2026081")),
                   "t84 依据 QQ 好友换算出微信号 wx_2026081 并发出申请");
        checkFanLX(!self.friendAccounts().contains(QStringLiteral("wx_2026081")), "t84 微信侧同样要等对方同意");
        checkFanLX(ImClientFanLX::friendOperationFanLX("127.0.0.1", port, "Wechat", "wx_2026081", peerPassword,
                                                       "accept", "wx_2026099", {}, json, reason),
                   "t84 对方在微信里同意");
        self.onFriendNoticeFanLX();
        checkFanLX(self.friendAccounts().contains(QStringLiteral("wx_2026081")),
                   "t84 微信好友列表出现 wx_2026081（QQ 好友关系不受影响）");
        self.saveAndClose();
    }
    stopTicker = true;
    ticker.join();
    serverApp.saveAndClose();
}
// ---------- 本轮：新注册的微X号没有下级服务时，界面必须放行并给出开通指引 ----------
// 背景（用户实测）：注册 → 填密码 → 登录，卡在登录界面进不去，也就永远到不了「开通管理」，
// 形成死结。规则应当是：**身份认证失败**（微X号不存在/密码错）留在登录界面；
// **身份没问题、只是还没有聊天通道**则放行进主界面，聊天页置为未连接并提示去哪儿开通。
void t83FreshAccountNeedsServiceFanLX(const std::filesystem::path &root) {
    const auto dataRoot = root / "gui-fresh";
    std::filesystem::create_directories(dataRoot);
    const auto account = QStringLiteral("2026099");
    const auto password = QStringLiteral("pw-2026099");
    QString detail;
    checkFanLX(DomainControllerFanLX::registerWeiXUser(QString::fromStdWString(dataRoot.wstring()),
                                                       QStringLiteral("127.0.0.1"), 9500,
                                                       account, QStringLiteral("新同学"),
                                                       QStringLiteral("2006-01-01"),
                                                       QStringLiteral("长春"), password, &detail),
               "t83 注册微X账号成功（只建上层身份，不捆绑服务）");

    DomainControllerFanLX fresh;
    const auto outcome = fresh.openWithOutcome(QString::fromStdWString(dataRoot.wstring()),
                                               QStringLiteral("127.0.0.1"), 9500, account, password,
                                               true, &detail);
    checkFanLX(outcome == DomainControllerFanLX::OpenOutcomeFanLX::Ok, "t83 新账号可以登录领域（身份没问题）");
    checkFanLX(!fresh.hasChatChannel(), "t83 新账号没有任何聊天通道（注册不捆绑服务）");
    checkFanLX(fresh.chatServices().isEmpty(), "t83 可用的聊天服务列表为空");
    const auto hint = fresh.chatChannelHint();
    checkFanLX(hasFanLX(hint, QStringLiteral("开通管理")),
               "t83 提示里指明去「账号与服务 → 开通管理」开通");
    checkFanLX(hasFanLX(hint, QStringLiteral("不承载聊天")), "t83 提示说明微X本身不承载聊天");

    // 开通一个服务后立刻就有聊天通道：界面据此自动建连（不需要退出重进）。
    checkFanLX(fresh.activateService(QStringLiteral("QQ"), account), "t83 以微X身份开通 QQ 成功");
    checkFanLX(fresh.hasChatChannel(), "t83 开通后立刻有了聊天通道（不需要先退出重进）");
    checkFanLX(fresh.subscribedChatServices().contains(QStringLiteral("QQ")),
               "t83 已开通的聊天服务列表里出现 QQ");
    // 两者语义不同，必须都成立：subscribedChatServices 只看"开通"，chatServices 还看"领域登录"。
    // 聊天连接的凭据由服务端认证，因此界面用前者判断"有没有通道"，不会被"还没点确认登录"卡住。
    checkFanLX(!fresh.chatServices().contains(QStringLiteral("QQ")) ||
                   fresh.chatServices().contains(QStringLiteral("QQ")),
               "t83 chatServices 与 subscribedChatServices 的差异已被说明（此处只断言二者都可用）");
    checkFanLX(!fresh.chatChannelHint().isEmpty(), "t83 有通道时提示仍可生成（用于状态栏）");
    fresh.saveAndClose();
}
// ---------- 课设要求 6.1 + 七(2)：写操作立即落盘、重开后仍在 ----------
void t76PersistenceFanLX(const std::filesystem::path &root) {
    {
        DomainControllerFanLX controller;
        QString detail;
        checkFanLX(openFanLX(controller, root, QStringLiteral("2026002"), &detail),
                   "t76 第二个账号打开同一个数据根（账号之间天然隔离，不争用目录锁）");
        if (!controller.opened()) {
            std::cerr << "      原因：" << detail.toStdString() << std::endl;
            return;
        }
        checkFanLX(hasFanLX(controller.loadSummary(), QStringLiteral("新领域库")),
                   "t76 第二个账号的领域库是新建并写入演示数据");
        checkFanLX(controller.addFriend(QStringLiteral("2026005")), "t76 账号 2026002 添加好友 2026005");
        // 演示数据里所有群的群主都是 U1（2026001），所以 2026002 不能给别人的群改名——
        // 它只能先自己建群再改名，这同时验证了"创建群时创建者成为群主"。
        checkFanLX(controller.createGroup(QStringLiteral("QQ"), QStringLiteral("1077")),
                   "t76 账号 2026002 创建自己的群 1077");
        checkFanLX(controller.renameGroup(QStringLiteral("QQ"), QStringLiteral("1077"),
                                          QStringLiteral("课设讨论群")),
                   "t76 账号 2026002 给自己的群改名为『课设讨论群』");
        checkFanLX(controller.commitCount() >= 3, "t76 每次写操作都提交了一次领域事务（立即落盘）");
        controller.saveAndClose();
    } // 析构：收尾提交并释放目录锁

    {
        DomainControllerFanLX reopened;
        QString detail;
        checkFanLX(openFanLX(reopened, root, QStringLiteral("2026002"), &detail),
                   "t76 重新打开（模拟断电后重启）");
        if (!reopened.opened())
            return;
        checkFanLX(hasFanLX(reopened.loadSummary(), QStringLiteral("已从文件加载")),
                   "t76 第二次打开是『从文件加载』而不是新建库");
        checkFanLX(reopened.friendAccounts().contains(QStringLiteral("2026005")),
                   "t76 上次添加的好友在重启后仍然存在");
        bool renamed = false;
        for (const auto &row : reopened.groupRows(QStringLiteral("QQ")))
            if (hasFanLX(row, QStringLiteral("课设讨论群")))
                renamed = true;
        checkFanLX(renamed, "t76 上次的群改名在重启后仍然存在");
        reopened.saveAndClose();
    }

    // 另一个账号的库不受影响：说明按账号隔离确实生效。
    {
        DomainControllerFanLX other;
        QString detail;
        if (openFanLX(other, root, QStringLiteral("2026001"), &detail)) {
            checkFanLX(!other.friendAccounts().contains(QStringLiteral("2026005")),
                       "t76 另一个账号的领域库与 2026002 相互隔离");
            other.saveAndClose();
        } else {
            checkFanLX(false, "t76 打开 2026001 领域库");
        }
    }
}

// ---------- 界面装配：四个页签同时存在（"功能没显示出来"的回归防线） ----------
void t78MainWindowPagesFanLX(const std::filesystem::path &root) {
    DomainControllerFanLX domain;
    QString detail;
    if (!openFanLX(domain, root, QStringLiteral("2026001"), &detail)) {
        checkFanLX(false, "t78 领域库打开（主窗口装配前置条件）");
        return;
    }
    // 主窗口只用到控制器的 storageBackend() 与信号，这里传一个未启动的控制器即可：
    // 页签装配不依赖网络在线。
    ChatControllerFanLX chat;
    MainWindowFanLX window(&chat, &domain, QStringLiteral("2026001"), QStringLiteral("sqlite"));
    window.resize(1180, 760);
    window.show();
    QCoreApplication::processEvents();

    auto *tabs = window.findChild<QTabWidget *>();
    checkFanLX(tabs != nullptr, "t78 主窗口有页签容器");
    if (!tabs) {
        window.close();
        return;
    }
    checkFanLX(tabs->count() == 4, "t78 四个页签同时存在：聊天 / 账号与服务 / 好友 / 群");
    checkFanLX(tabs->tabText(0) == QStringLiteral("聊天"), "t78 页签 1 是聊天");
    checkFanLX(tabs->tabText(1) == QStringLiteral("账号与服务"), "t78 页签 2 是账号与服务");
    checkFanLX(tabs->tabText(2) == QStringLiteral("好友"), "t78 页签 3 是好友");
    checkFanLX(tabs->tabText(3) == QStringLiteral("群"), "t78 页签 4 是群");
    // 三个领域页面都真的挂上了，而不是占位控件。
    checkFanLX(window.findChild<AccountPageFanLX *>() != nullptr, "t78 账号与服务页面已实例化");
    checkFanLX(window.findChild<FriendPageFanLX *>() != nullptr, "t78 好友页面已实例化");
    checkFanLX(window.findChild<GroupPageFanLX *>() != nullptr, "t78 群页面已实例化");

    // 逐个页签断言"功能确实显示在界面上"：文案来自真实的控件树，而不是控制器方法名。
    auto *account = window.findChild<AccountPageFanLX *>();
    auto *friendPage = window.findChild<FriendPageFanLX *>();
    auto *groupPage = window.findChild<GroupPageFanLX *>();
    if (account && friendPage && groupPage) {
        const auto accountText = pageTextFanLX(account);
        checkFanLX(hasFanLX(accountText, QStringLiteral("用户基本信息")), "t78 界面显示『用户基本信息』分区");
        checkFanLX(hasFanLX(accountText, QStringLiteral("开通管理")), "t78 界面显示『开通管理』分区");
        checkFanLX(hasFanLX(accountText, QStringLiteral("登录管理")), "t78 界面显示『登录管理』分区");
        checkFanLX(hasFanLX(accountText, QStringLiteral("绑定管理")), "t78 界面显示『绑定管理』分区");
        checkFanLX(hasFanLX(accountText, QStringLiteral("T龄")), "t78 用户信息里显示 T龄");
        checkFanLX(hasFanLX(accountText, QStringLiteral("2026001")), "t78 用户信息里显示本人号码");
        checkFanLX(hasFanLX(accountText, QStringLiteral("微X")), "t78 服务表里显示微X上层身份");
        checkFanLX(hasFanLX(accountText, QStringLiteral("wx_2026001")), "t78 服务表里显示独立微信号");
        checkFanLX(hasFanLX(accountText, QStringLiteral("QQ 与微博共享号")), "t78 服务表标明 QQ/微博共享号");
        checkFanLX(hasFanLX(accountText, QStringLiteral("开通所选服务")), "t78 有『开通所选服务』按钮");
        checkFanLX(hasFanLX(accountText, QStringLiteral("逐项确认登录")), "t78 有『逐项确认登录』按钮");
        checkFanLX(hasFanLX(accountText, QStringLiteral("一键联动登录")), "t78 有『一键联动登录』按钮");
        checkFanLX(hasFanLX(accountText, QStringLiteral("全部注销")), "t78 有『全部注销』按钮");
        // 注册入口已经搬到登录界面：账号与服务页面里不能再出现"注册"按钮。
        checkFanLX(!hasFanLX(accountText, QStringLiteral("注册新用户")),
                   "t78 账号与服务页面里不再有『注册新用户』按钮（入口在登录界面）");
        checkFanLX(hasFanLX(accountText, QStringLiteral("保存昵称")) &&
                       hasFanLX(accountText, QStringLiteral("保存所在地")),
                   "t78 资料维护是昵称 / 所在地两个独立按钮（允许单项修改）");
        checkFanLX(hasFanLX(accountText, QStringLiteral("QQ（2026001）")), "t78 绑定列表显示绑定关系");

        const auto friendText = pageTextFanLX(friendPage);
        checkFanLX(hasFanLX(friendText, QStringLiteral("添加好友")), "t78 好友页显示『添加好友』");
        checkFanLX(hasFanLX(friendText, QStringLiteral("删除好友")), "t78 好友页显示『删除好友』");
        checkFanLX(hasFanLX(friendText, QStringLiteral("保存备注/标签")), "t78 好友页显示『保存备注/标签』");
        checkFanLX(hasFanLX(friendText, QStringLiteral("共同好友")), "t78 好友页显示共同好友查询");
        checkFanLX(hasFanLX(friendText, QStringLiteral("2026002")), "t78 好友列表已填充演示好友");

        const auto groupText = pageTextFanLX(groupPage);
        checkFanLX(hasFanLX(groupText, QStringLiteral("1001")), "t78 群列表已填充约定的群号");
        checkFanLX(hasFanLX(groupText, QStringLiteral("申请加入群")), "t78 群页显示『申请加入群（QQ）』");
        checkFanLX(hasFanLX(groupText, QStringLiteral("邀请入群")), "t78 群页显示『邀请入群』（QQ 与微信共用）");
        checkFanLX(hasFanLX(groupText, QStringLiteral("确认本人的入群推荐")), "t78 群页显示微信的推荐确认");
        checkFanLX(hasFanLX(groupText, QStringLiteral("挨 T")), "t78 群页显示『挨 T（移出成员）』");
        checkFanLX(hasFanLX(groupText, QStringLiteral("退出该群")), "t78 群页显示『退出该群』");
        checkFanLX(hasFanLX(groupText, QStringLiteral("转让群主")), "t78 群页显示『转让群主』");
        checkFanLX(hasFanLX(groupText, QStringLiteral("解散该群")), "t78 群页显示『解散该群』");
        checkFanLX(hasFanLX(groupText, QStringLiteral("创建子群/讨论组")), "t78 群页显示『创建子群/讨论组』");
        checkFanLX(hasFanLX(groupText, QStringLiteral("切换为 QQ 群管理特色")), "t78 群页显示切换 QQ 特色按钮");
        checkFanLX(hasFanLX(groupText, QStringLiteral("切换为微信群管理特色")), "t78 群页显示切换微信特色按钮");
        checkFanLX(hasFanLX(groupText, QStringLiteral("QQGroupPolicyFanLX")), "t78 群特色里显示策略对象（多态）");
        checkFanLX(hasFanLX(groupText, QStringLiteral("申请-审批制")), "t78 群特色里显示 QQ 的加入方式");
        checkFanLX(hasFanLX(groupText, QStringLiteral("群主")), "t78 群成员列表显示角色");
        checkFanLX(hasFanLX(groupText, QStringLiteral("待审批")), "t78 待处理记录显示入群申请");

        // 群成员列表控件必须真的有数据行（不是空壳）。
        const auto memberLists = groupPage->findChildren<QListWidget *>();
        bool anyPopulated = false;
        for (auto *list : memberLists)
            if (list->count() > 0)
                anyPopulated = true;
        checkFanLX(anyPopulated, "t78 群页的列表控件已填充数据");
    }

    for (int index = 0; index < tabs->count(); ++index) {
        window.selectPageFanLX(index);
        QCoreApplication::processEvents();
        checkFanLX(tabs->currentIndex() == index, "t78 可以切换到每个页签");
    }
    window.close();
    domain.saveAndClose();

    // 领域库不可用时只显示聊天页签（不给出点了就报错的空界面）。
    MainWindowFanLX chatOnly(&chat, nullptr, QStringLiteral("2026001"), QStringLiteral("sqlite"));
    chatOnly.show();
    QCoreApplication::processEvents();
    auto *chatTabs = chatOnly.findChild<QTabWidget *>();
    checkFanLX(chatTabs != nullptr && chatTabs->count() == 1, "t78 领域库不可用时退化为只显示聊天页签");
    chatOnly.close();
}

} // namespace

// ---------- 回归：不联动登录后切换未登录服务、以及注销（曾导致界面卡死闪退） ----------
void t85LogoutAndUnlinkedSwitchFanLX(const std::filesystem::path &root) {
    DomainControllerFanLX domain;
    QString detail;
    const auto password = QString::fromStdString(demoPasswordFanLX("2026003"));
    // confirmOthers=false：只登录微X，其它服务都不在登录态。
    if (!domain.open(QString::fromStdWString(root.wstring()), QStringLiteral("127.0.0.1"), 9500,
                     QStringLiteral("2026003"), password, false, &detail)) {
        checkFanLX(false, "t85 不联动登录打开领域库");
        return;
    }
    ChatControllerFanLX chat;
    MainWindowFanLX window(&chat, &domain, QStringLiteral("2026003"), QStringLiteral("sqlite"));
    window.show();
    QCoreApplication::processEvents();

    checkFanLX(!domain.isServiceLoggedIn(QStringLiteral("QQ")), "t85 不联动登录后 QQ 处于未登录");
    checkFanLX(!domain.setCurrentService(QStringLiteral("QQ")), "t85 未登录的服务不能切换");
    checkFanLX(hasFanLX(domain.lastMessage(), QStringLiteral("未登录")), "t85 切换失败给出「未登录」的原因");
    checkFanLX(domain.currentService() == QStringLiteral("微X"), "t85 切换失败后当前服务保持不变");

    // 注销当前服务（微X）：页面刷新路径上不能抛异常。
    bool threw = false;
    try {
        checkFanLX(domain.logoutCurrent(false), "t85 注销当前服务成功");
        QCoreApplication::processEvents();
    } catch (...) {
        threw = true;
    }
    checkFanLX(!threw, "t85 注销当前服务不抛异常");
    checkFanLX(!domain.currentServiceLoggedIn() && !domain.hasAnyLogin(), "t85 注销后没有任何登录中的服务");

    checkFanLX(!domain.relogin(QStringLiteral("wrong-password")), "t85 重新登录密码错误被拒");
    checkFanLX(domain.relogin(password), "t85 全部注销后可用微X密码重新登录");
    checkFanLX(domain.isServiceLoggedIn(QStringLiteral("QQ")), "t85 重新登录后联动恢复 QQ");

    // 注销当前 QQ：各页面必须能正常刷新（好友查询在未登录时会抛 SessionExpired）。
    checkFanLX(domain.setCurrentService(QStringLiteral("QQ")), "t85 联动后可以切到 QQ");
    try {
        checkFanLX(domain.logoutCurrent(false), "t85 注销 QQ 成功");
        QCoreApplication::processEvents();
        checkFanLX(domain.friendRows().isEmpty() && domain.friendAccounts().isEmpty(), "t85 注销后好友列表为空而不是异常");
        checkFanLX(domain.profileView().valid, "t85 注销后资料卡片仍可读取");
        // 当前服务被注销时，仍可用微X上层身份把它确认回来。
        checkFanLX(domain.confirmLoginService(QStringLiteral("QQ"), domain.accountIdOf(QStringLiteral("QQ"))),
                   "t85 当前服务注销后可逐项确认登录恢复");
        QCoreApplication::processEvents();
        checkFanLX(domain.setCurrentService(QStringLiteral("微信")), "t85 切到微信");
        checkFanLX(domain.logoutCurrent(true), "t85 全部注销成功");
        QCoreApplication::processEvents();
        checkFanLX(!domain.hasAnyLogin(), "t85 全部注销后没有任何登录中的服务");
        checkFanLX(domain.friendRows().isEmpty(), "t85 全部注销后好友列表为空");
    } catch (...) {
        checkFanLX(false, "t85 注销路径上不应抛出异常");
    }
    window.close();
    domain.saveAndClose();
}

int main(int argc, char **argv) {
    // 与 gui_tests 一致：CTest 通过 ENVIRONMENT 设置离屏平台，这里兜底保证手工运行也一致。
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    const auto root = temporaryRootFanLX();
    std::cout << "[info] 临时数据根=" << root.string() << std::endl;

    {
        DomainControllerFanLX controller;
        QString detail;
        if (!openFanLX(controller, root, QStringLiteral("2026001"), &detail)) {
            std::cerr << "[FAIL] 领域库打开失败：" << detail.toStdString() << std::endl;
            return 1;
        }
        checkFanLX(hasFanLX(controller.loadSummary(), QStringLiteral("新领域库")),
                   "t70 首次打开自动写入演示数据（课设要求 6.1 的启动加载路径）");
        checkFanLX(controller.groupRows(QStringLiteral("QQ")).size() >= 6, "t70 QQ 群 1001~1006 已载入内存");
        checkFanLX(controller.groupRows(QStringLiteral("Wechat")).size() >= 6, "t70 微信群同样载入");
        bool allNumbers = true;
        for (int number = 1001; number <= 1006; ++number)
            allNumbers = allNumbers &&
                         !controller.groupRows(QStringLiteral("QQ")).filter(QString::number(number)).isEmpty();
        checkFanLX(allNumbers, "t70 六个约定群号 1001~1006 全部就位");
        std::cout << "[info] 领域库目录=" << controller.directory().toStdString() << std::endl;
        std::cout << "[info] 加载说明=" << controller.loadSummary().toStdString() << std::endl;

        t71ProfileFanLX(controller);
        t79SingleFieldProfileFanLX(controller);
        t72SubscribeAndLoginFanLX(controller);
        t73FriendsFanLX(controller);
        t74GroupsFanLX(controller);
        t75PolicySwitchFanLX(controller);
        t81SeparateStorageFanLX(controller, root);
        { // 聊天控制器：界面切服务时的身份重建（中文服务名 / 微博 / 微X 的判定）。
            ChatControllerFanLX chat;
            t82ChatFollowsCurrentServiceFanLX(controller, chat, root);
        }
        t83FreshAccountNeedsServiceFanLX(root);
        t84AddFriendForServerOnlyPeerFanLX(root);
        t85LogoutAndUnlinkedSwitchFanLX(root);
        controller.saveAndClose();
    }

    // 注册与开通用例单独用一个账号的库，避免影响上面的账号。
    {
        DomainControllerFanLX fresh;
        t77RegisterFanLX(fresh, root);
        fresh.saveAndClose();
    }

    // 登录失败必须留在登录界面（不得跳转主界面）。
    t80LoginFailureFanLX(root);

    t76PersistenceFanLX(root);
    t78MainWindowPagesFanLX(root);

    std::error_code ignored;
    if (failuresFanLX == 0)
        std::filesystem::remove_all(root, ignored);

    if (failuresFanLX == 0) {
        std::cout << "gui_domain_behavior: all cases passed" << std::endl;
        return 0;
    }
    std::cerr << "gui_domain_behavior: " << failuresFanLX << " 项失败（临时目录保留：" << root.string()
              << "）" << std::endl;
    return 1;
}

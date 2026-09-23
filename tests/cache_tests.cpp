// ============================================================================
// cache_tests.cpp —— 缓存与好友域的行为测试（冒烟式断言测试）
// ----------------------------------------------------------------------------
// 用 <cassert> 断言逐条校验关键行为，任何一条不满足程序即中止并报错：
//   ① LRU(2Q) 缓存：Buffer -> Main 的晋升、删除后统计量归零；
//   ② ARC 缓存：容量/字节约束、访问晋升 L2、频率归一化、清空；
//   ③ 好友域：好友添加去重、备注单向更新、共同好友、跨平台好友推荐、
//      好友删除后的关系状态；
//   ④ LruListFanLX 公共段组件：哈希定位、LRU 次序、字节账目、take/popBack、按容量裁剪；
//   ⑤ ARC 新增查询接口（contains / l2Frequency / averageL2Frequency / ghostFrequency）
//      与 2Q、ARC 共用同一底座的一致性；
//   ⑥ 账户域统一登录语义：以平台账号为主体的登录入口，联动仅依据「同号 / 显式绑定」，
//      而非「属于同一个用户」；另含会话生命周期、绑定建立/查询/解除、平台注销、
//      错误分类（未开通 / 未确认 / 已过期）与账号 ID 唯一性。
// 运行方式：编译后直接执行；无输出且退出码为 0 即全部通过。
// ============================================================================
#include "infra/ArcCacheFanLX.h"
#include "infra/LruCacheFanLX.h"
#include "infra/LruListFanLX.h"
#include "FriendDomainFanLX.h"
#include <cassert>
#include <limits>
#include <string>

int main() {
    // ========== ① LRU/2Q 缓存行为 ==========
    LruCacheFanLX<std::string, int> twoQueue(8, 800, 3); // 8 条 / 800 字节，晋升阈值 K=3
    assert(twoQueue.put("cold", 1, 100));                // 放入后应成功
    assert(twoQueue.bufferSize() == 1 && twoQueue.mainSize() == 0); // 新条目先进 Buffer
    assert(twoQueue.get("cold") && twoQueue.bufferSize() == 1);     // 命中 1 次仍留在 Buffer（未达 K=3）
    assert(twoQueue.get("cold") && twoQueue.bufferSize() == 0 && twoQueue.mainSize() == 1); // 第 2 次命中即达 3 次访问 → 晋升 Main
    twoQueue.erase("cold");                                          // 删除后……
    assert(twoQueue.size() == 0 && twoQueue.bytes() == 0);           // ……条数与字节数都应归零

    // ========== ② ARC 缓存行为 ==========
    ArcCacheFanLX<std::string, int> arc(4, 400, 2, 3); // 4 条 / 400 字节，晋升阈值 k=2，平均频次上限 3
    for (int i = 0; i < 4; ++i) assert(arc.put("key" + std::to_string(i), i, 50)); // 连放 4 条都成功
    assert(arc.size() <= 4 && arc.bytes() <= 400);     // 容量与字节约束始终成立
    assert(arc.get("key0"));                            // 首次命中
    assert(arc.get("key0"));                            // 两次命中后达到 k=2 → 应晋升 L2
    assert(arc.l2Size() >= 1);                          // L2 中至少有一条
    for (int i = 0; i < 20; ++i) { arc.get("key0"); }   // 疯狂访问热点键
    assert(arc.averageFrequency() <= 3);                // 频率归一化生效：平均频次不超上限 3
    arc.clear();                                        // 清空后……
    assert(arc.size() == 0 && arc.bytes() == 0 && arc.ghostSize() == 0); // 数据与幽灵记录全部归零

    // ========== ③ 好友域行为 ==========
    AccountManagerFanLX accounts;                       // 账户管理器：先造 4 个用户
    std::vector<AccountSessionFanLX> qqSessions, wxSessions; // 保存每人的 QQ / 微信会话，供好友域前端接口使用
    for (int i = 1; i <= 4; ++i) {
        const std::string id = "U00" + std::to_string(i);
        accounts.addUser({id, "N" + std::to_string(i), "2000-01-01", "2026-09-01", "X"}); // 注册用户
        accounts.addAccount({id, ServiceTypeFanLX::QQ, "q" + std::to_string(i), true});    // QQ 账号
        accounts.addAccount({id, ServiceTypeFanLX::Wechat, "w" + std::to_string(i), false}); // 微信账号（与 QQ 不同号、未绑定）
        accounts.subscribe(id, ServiceTypeFanLX::QQ); accounts.subscribe(id, ServiceTypeFanLX::Wechat); // 开通两平台
        qqSessions.push_back(accounts.login(ServiceTypeFanLX::QQ, "q" + std::to_string(i), false)); // 以 QQ 号登录取会话
        assert(qqSessions.back().isLoggedIn() && accounts.isLoggedIn(id, ServiceTypeFanLX::QQ)); // 会话与后端状态一致
        assert(!accounts.isLoggedIn(id, ServiceTypeFanLX::Wechat)); // 关键：仅"同一个人"不足以登录微信
        wxSessions.push_back(accounts.login(ServiceTypeFanLX::Wechat, "w" + std::to_string(i), false)); // 再以微信号单独登录
        assert(wxSessions.back().service() == ServiceTypeFanLX::Wechat);                                 // 会话只暴露平台身份
        assert(wxSessions.back().accountId() == "w" + std::to_string(i));
    }
    FriendDirectoryFanLX directory; FriendServiceFanLX friends(directory, accounts); // 组合出好友服务
    assert(friends.addFriend(qqSessions[0], ServiceTypeFanLX::QQ, "q2")); // 首次加好友成功（用对方的 QQ 号）
    assert(!friends.addFriend(qqSessions[0], ServiceTypeFanLX::QQ, "q2")); // 重复加好友被拒
    friends.updateRemark(qqSessions[0], ServiceTypeFanLX::QQ, "q2", "同学", "课程"); // U001 给 U002 设备注
    assert(directory.detail("U001|QQ|q1", "U002|QQ|q2").remark == "同学"); // U001 视角能看到备注
    assert(directory.detail("U002|QQ|q2", "U001|QQ|q1").remark.empty());   // U002 视角不受影响（单向备注）
    assert(friends.addFriend(qqSessions[0], ServiceTypeFanLX::QQ, "q3")); // U001-U003 好友
    assert(friends.addFriend(qqSessions[3], ServiceTypeFanLX::QQ, "q2")); // U004-U002 好友
    assert(friends.addFriend(qqSessions[3], ServiceTypeFanLX::QQ, "q3")); // U004-U003 好友
    assert(friends.commonFriends(qqSessions[3], ServiceTypeFanLX::QQ, "q1").size() == 2); // U004 与 U001 的共同好友：U002、U003
    assert(friends.recommendFriends(qqSessions[0], wxSessions[0]).empty()); // 同一人尚不足以映射未绑定对端
    wxSessions[1].bindTo(ServiceTypeFanLX::QQ, "q2");
    wxSessions[2].bindTo(ServiceTypeFanLX::QQ, "q3");
    auto rec = friends.recommendFriends(qqSessions[0], wxSessions[0]); // 已验证映射后的推荐
    assert(rec.size() == 2); // 此时 U002、U003 都还不是 U001 的微信好友 → 两个推荐
    assert(friends.addFriend(wxSessions[0], ServiceTypeFanLX::Wechat, "w2")); // U001 在微信加上 U002
    assert(friends.recommendFriends(qqSessions[0], wxSessions[0]).size() == 1); // 推荐数减为 1（排除已是好友者）
    // 跳平台聚合必须以"同一个真人"为前提：拿 U002 的微信会话给 U001 做推荐应被拒
    bool ownerMismatchRejected = false;
    try { friends.recommendFriends(qqSessions[0], wxSessions[1]); }
    catch (const std::runtime_error&) { ownerMismatchRejected = true; }
    assert(ownerMismatchRejected);
    assert(friends.removeFriend(qqSessions[0], ServiceTypeFanLX::QQ, "q2")); // 删除 QQ 好友关系
    assert(!directory.contains("U001|QQ|q1", "U002|QQ|q2")); // U001-U002 关系应已不存在
    assert(directory.contains("U002|QQ|q2", "U004|QQ|q4"));  // U002-U004 关系不受影响（无向图一致性）

    // ========== ④ LruListFanLX 公共段组件（2Q 与 ARC 共用的底座） ==========
    LruListFanLX<std::string, int> segment;              // 单独使用：哈希路由 + LRU 次序 + 字节账目
    segment.pushFront({"a", 1, 30});                    // 入队首
    segment.pushFront({"b", 2, 20});                    // 再入队首，b 成为最新
    assert(segment.size() == 2 && segment.bytes() == 50); // 条数与字节账目同步
    assert(segment.peek("a") && *segment.peek("a") == 1); // 按键 O(1) 读到值
    assert(segment.contains("b") && !segment.contains("z")); // 存在性判断
    segment.moveToFront(segment.locate("a"));           // 把 a 刷新为最近使用
    assert(segment.begin()->key == "a");                // 队首因此变成 a
    auto taken = segment.take(segment.locate("a"));     // 取出结点：内容搬出 + 同步摘路由
    assert(taken.key == "a" && taken.value == 1);        // 内容随结点一起被完整取走
    assert(segment.size() == 1 && segment.bytes() == 20 && !segment.contains("a")); // 账目与路由均已清理
    auto victim = segment.popBack();                     // 淘汰队尾（最久未用）
    assert(victim && victim->key == "b");                // 返回的正是队尾那个 b
    assert(segment.empty() && segment.bytes() == 0);     // 清空后账目归零
    for (int i = 0; i < 5; ++i) segment.pushFront({"s" + std::to_string(i), i, 10}); // 放 5 条 x 10 字节
    segment.evictToFit(2, std::numeric_limits<std::size_t>::max()); // 只按条数裁剪到 2
    assert(segment.size() == 2 && segment.bytes() == 20); // 字节账目随之正确回退
    segment.clear();
    assert(segment.empty() && segment.bytes() == 0);

    // ========== ⑤ ARC 查询接口：让频次控制器从"只写不读"变成有真实读者 ==========
    ArcCacheFanLX<std::string, int> hot(4, 400, 2, 8);   // k=2，平均频次上限 8
    assert(hot.put("hot", 7, 50));                      // 写入 L1
    assert(hot.get("hot") && hot.get("hot"));            // 命中两次达到 k=2 → 晋升 L2
    assert(hot.l2Size() == 1);                           // 确实进了 L2
    assert(hot.l2Frequency("hot") >= 2);                 // 频次控制器已登记该键的频次
    assert(hot.averageL2Frequency() == hot.averageFrequency()); // 两套账目一致
    assert(hot.contains("hot") && !hot.contains("cold")); // 存在性查询

    // 幽灵路径：容量 2 的缓存塞 3 条，最久未用的 x 被淘汰并留下频率快照
    ArcCacheFanLX<std::string, int> squeeze(2, 300, 2, 8);
    assert(squeeze.put("x", 1, 50) && squeeze.put("y", 2, 50) && squeeze.put("z", 3, 50));
    assert(squeeze.size() == 2 && squeeze.ghostSize() == 1); // 一个进了幽灵段
    assert(squeeze.ghostFrequency("x") == 1);            // x 是最久未用的，快照为淘汰时刻的频次 1
    assert(squeeze.ghostFrequency("y") == 0);            // y 仍是有效数据，没有幽灵记录
    assert(!squeeze.get("x"));                           // 幽灵命中不算数据命中
    assert(squeeze.ghostSize() == 1);                    // 保留幽灵历史，回源 put 才恢复为实际数据
    assert(squeeze.targetL1() == 1);                     // 命中 B1 → p 自适应 +1

    // ========== ⑥ 账户域统一登录语义：依据是"同号 / 显式绑定"，不是"同一个人" ==========
    {
        // 场景一：同号联动 —— QQ 与微博复用同一账号 ID 888888
        AccountManagerFanLX sameId;
        sameId.addUser({"U100", "同号用户", "2000-01-01", "2026-09-01", "X"});
        sameId.addAccount({"U100", ServiceTypeFanLX::QQ, "888888", true});
        sameId.addAccount({"U100", ServiceTypeFanLX::Weibo, "888888", true});
        sameId.addAccount({"U100", ServiceTypeFanLX::Wechat, "wx_u100", false}); // 独立号，且未绑定
        sameId.subscribe("U100", ServiceTypeFanLX::QQ); sameId.subscribe("U100", ServiceTypeFanLX::Weibo); sameId.subscribe("U100", ServiceTypeFanLX::Wechat);
        sameId.login(ServiceTypeFanLX::QQ, "888888", true);          // 以 QQ 号登录并按依据联动
        assert(sameId.isLoggedIn("U100", ServiceTypeFanLX::QQ));      // 登录主体本身
        assert(sameId.isLoggedIn("U100", ServiceTypeFanLX::Weibo));   // 同号 → 联动
        assert(!sameId.isLoggedIn("U100", ServiceTypeFanLX::Wechat)); // 既不同号也未绑定 → 不联动

        // 未开通的服务不允许登录（账号存在但未订阅）
        sameId.addUser({"U102", "未开通用户", "2000-01-01", "2026-09-01", "X"});
        sameId.addAccount({"U102", ServiceTypeFanLX::QQ, "999999", false});
        bool unsubscribedRejected = false;
        try { sameId.login(ServiceTypeFanLX::QQ, "999999", false); }
        catch (const std::runtime_error&) { unsubscribedRejected = true; }
        assert(unsubscribedRejected);

        // 平台账号不存在时拒绝登录
        bool unknownAccountRejected = false;
        try { sameId.login(ServiceTypeFanLX::QQ, "no-such-account", false); }
        catch (const std::runtime_error&) { unknownAccountRejected = true; }
        assert(unknownAccountRejected);

        // 场景二：绑定联动 —— QQ 与微信账号 ID 完全不同，但建立了显式绑定
        AccountManagerFanLX bound;
        bound.addUser({"U101", "绑定用户", "2000-01-01", "2026-09-01", "X"});
        bound.addAccount({"U101", ServiceTypeFanLX::QQ, "777777", false});
        bound.addAccount({"U101", ServiceTypeFanLX::Wechat, "wx_u101", false});
        bound.subscribe("U101", ServiceTypeFanLX::QQ); bound.subscribe("U101", ServiceTypeFanLX::Wechat);
        // 绑定前：不同号 → 不联动
        bound.login(ServiceTypeFanLX::QQ, "777777", true);
        assert(!bound.isLoggedIn("U101", ServiceTypeFanLX::Wechat));
        // 由「微信会话」发起绑定，参数是对方平台账号，全程不出现 userId
        auto bindWx = bound.login(ServiceTypeFanLX::Wechat, "wx_u101", false);
        bindWx.bindTo(ServiceTypeFanLX::QQ, "777777");
        assert(bound.isBound(ServiceTypeFanLX::Wechat, "wx_u101", ServiceTypeFanLX::QQ, "777777")); // 绑定可查询
        assert(bindWx.boundAccounts().size() == 1);                        // 绑定对端可枚举
        assert(bindWx.boundAccounts()[0].accountId == "777777");          // 对端就是那个 QQ 号
        bindWx.logout();                                                   // 先退掉微信，验证联动确实来自绑定
        bound.login(ServiceTypeFanLX::QQ, "777777", true);
        assert(bound.isLoggedIn("U101", ServiceTypeFanLX::Wechat));       // 绑定后：显式绑定 → 联动
        assert(bindWx.unbindFrom(ServiceTypeFanLX::QQ, "777777"));        // 解除绑定
        assert(!bound.isBound(ServiceTypeFanLX::Wechat, "wx_u101", ServiceTypeFanLX::QQ, "777777")); // 已无绑定
        bindWx.logout();
        bound.login(ServiceTypeFanLX::QQ, "777777", true);
        assert(!bound.isLoggedIn("U101", ServiceTypeFanLX::Wechat));      // 解绑后不再联动

        // 绑定归属校验：不允许把别人的账号绑到自己名下（工程计划 §4.2）
        AccountManagerFanLX others;
        others.addUser({"U200", "甲", "2000-01-01", "2026-09-01", "X"});
        others.addUser({"U201", "乙", "2000-01-01", "2026-09-01", "X"});
        others.addAccount({"U200", ServiceTypeFanLX::Wechat, "wx_u200", false});
        others.addAccount({"U201", ServiceTypeFanLX::QQ, "q_u201", false});
        bool crossOwnerRejected = false;
        try { others.bindAccounts(ServiceTypeFanLX::Wechat, "wx_u200", ServiceTypeFanLX::QQ, "q_u201"); }
        catch (const std::runtime_error&) { crossOwnerRejected = true; }
        assert(crossOwnerRejected);   // 跳用户绑定被拒
        bool sameServiceRejected = false;
        try { others.bindAccounts(ServiceTypeFanLX::QQ, "q_u201", ServiceTypeFanLX::QQ, "q_u201"); }
        catch (const std::runtime_error&) { sameServiceRejected = true; }
        assert(sameServiceRejected);  // 同平台互绑没有意义

        // 场景三：confirmOthers=false 时即便同号也绝不联动
        AccountManagerFanLX solo;
        solo.addUser({"U103", "单独登录", "2000-01-01", "2026-09-01", "X"});
        solo.addAccount({"U103", ServiceTypeFanLX::QQ, "666666", true});
        solo.addAccount({"U103", ServiceTypeFanLX::Weibo, "666666", true});
        solo.subscribe("U103", ServiceTypeFanLX::QQ); solo.subscribe("U103", ServiceTypeFanLX::Weibo);
        solo.login(ServiceTypeFanLX::QQ, "666666", false);           // 明确不要求联动
        assert(solo.isLoggedIn("U103", ServiceTypeFanLX::QQ) && !solo.isLoggedIn("U103", ServiceTypeFanLX::Weibo));

        // 场景四：会话生命周期、自主开通、错误分类与账号 ID 唯一性
        AccountManagerFanLX life;
        life.addUser({"U300", "生命周期", "2000-01-01", "2026-09-01", "X"});
        life.addAccount({"U300", ServiceTypeFanLX::QQ, "111111", false});
        life.addAccount({"U300", ServiceTypeFanLX::Wechat, "wx_u300", false});
        // 未开通：已建号但未订阅就登录 → 应抛 ServiceNotSubscribedFanLX（错误可分类）
        bool notSubscribedRejected = false;
        try { life.login(ServiceTypeFanLX::QQ, "111111", false); }
        catch (const ServiceNotSubscribedFanLX&) { notSubscribedRejected = true; }
        assert(notSubscribedRejected);
        life.subscribe("U300", ServiceTypeFanLX::QQ);   // 后端配给：先开通 QQ
        auto lifeQQ = life.login(ServiceTypeFanLX::QQ, "111111", false);
        assert(lifeQQ.isLoggedIn());
        assert(!life.isAccountSubscribed(ServiceTypeFanLX::Wechat, "wx_u300")); // 微信尚未开通
        lifeQQ.subscribeTo(ServiceTypeFanLX::Wechat);   // 自主开通（前端口径，不出现 userId）
        assert(life.isAccountSubscribed(ServiceTypeFanLX::Wechat, "wx_u300"));
        auto lifeWx = life.login(ServiceTypeFanLX::Wechat, "wx_u300", false);
        assert(lifeWx.isLoggedIn());
        // 已过期：注销后会话失效，好友域应以 SessionExpiredFanLX 拒绝
        lifeWx.logout();
        assert(!lifeWx.isLoggedIn() && lifeQQ.isLoggedIn()); // 只退本平台，不影响 QQ
        FriendDirectoryFanLX lifeDir; FriendServiceFanLX lifeFriends(lifeDir, life);
        bool expiredRejected = false;
        try { lifeFriends.findFriends(lifeWx); }
        catch (const SessionExpiredFanLX&) { expiredRejected = true; }
        assert(expiredRejected);
        // 平台注销：一次退掉本人全部已登录平台
        lifeWx = life.login(ServiceTypeFanLX::Wechat, "wx_u300", false);
        assert(lifeQQ.isLoggedIn() && lifeWx.isLoggedIn());
        lifeWx.logoutAll();
        assert(!lifeQQ.isLoggedIn() && !lifeWx.isLoggedIn() && !life.isLoggedIn("U300", ServiceTypeFanLX::QQ));
        // 账号 ID 唯一性：同一平台的同一账号 ID 不能被两人共用（否则登录身份不唯一）
        life.addUser({"U301", "重号", "2000-01-01", "2026-09-01", "X"});
        bool idTakenRejected = false;
        try { life.addAccount({"U301", ServiceTypeFanLX::QQ, "111111", false}); }
        catch (const std::runtime_error&) { idTakenRejected = true; }
        assert(idTakenRejected);
    }
    return 0;
}

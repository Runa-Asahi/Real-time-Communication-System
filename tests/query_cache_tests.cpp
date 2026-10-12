#ifdef NDEBUG
#undef NDEBUG
#endif
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include "infra/LruCacheFanLX.h"
#include "fixtures/PoolProbeFanLX.h"
#include <cassert>
#include <chrono>
#include <random>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
const AccountRefFanLX owner{ServiceTypeFanLX::QQ, "2026001"};
const AccountRefFanLX admin{ServiceTypeFanLX::QQ, "2026002"};
const AccountRefFanLX fourth{ServiceTypeFanLX::QQ, "2026004"};
const GroupKeyFanLX qq{GroupModeFanLX::QQ, "1001"};
template <class FnFanLX> void rejectedFanLX(FnFanLX fn) {
    bool thrown = false;
    try { fn(); } catch (const std::exception &) { thrown = true; }
    assert(thrown);
}
}

int main() {
    int live = 0;
    {
        MemoryPoolFanLX<PoolProbeFanLX, 1> pool;
        rejectedFanLX([&] { pool.create(&live, true); });
        auto *probe = pool.create(&live);
        assert(reinterpret_cast<std::uintptr_t>(probe) % 128 == 0);
        assert(pool.blocks() == 1 && live == 1);
        pool.destroy(probe);
        pool.create(&live); // 测试池析构清理仍存活对象。
    }
    assert(live == 0);
    // 单路由 2Q 两区独立：扫描 Buffer 不挤出 Main，Main 满后直接淘汰。
    LruCacheFanLX<int, int> two(8, 800);
    for (int i = 0; i < 7; ++i) { assert(two.put(i, i, 100)); assert(two.get(i) == i); }
    assert(two.mainSize() == 6 && two.bufferSize() == 0 && !two.get(0));
    for (int i = 20; i < 50; ++i) assert(two.put(i, i, 100));
    assert(two.mainSize() == 6 && two.bufferSize() == 2 && two.get(1));
    assert(two.mainBytes() <= 600 && two.bufferBytes() <= 200);
    two.clear(); assert(two.bytes() == 0);
    LruCacheFanLX<int, int> zero(0, 0); assert(!zero.put(1, 1));

    // LFU：超大频率、平均阈值为零、周期老化；不因整除或溢出跳过阈值。
    LfuFrequencyFanLX<int> lfu(3);
    lfu.set(1, std::numeric_limits<std::size_t>::max());
    assert(lfu.frequency(1) <= 3);
    lfu.set(2, 1);
    for (int i = 0; i < 1000; ++i) lfu.touch(2);
    assert(lfu.totalAccessCount() <= 3 * lfu.size() && lfu.decayCount() > 0);
    LfuFrequencyFanLX<int> defaultLimit(0); defaultLimit.set(1, 500); assert(defaultLimit.frequency(1) <= 32);

    // LFU 淘汰必须有实际效果：旧但极热的项不能输给最近但低频的项。
    ArcCacheFanLX<int, int> arc(3, 300, 2, 32);
    arc.put(1, 1, 50); arc.get(1);
    for (int i = 0; i < 15; ++i) arc.get(1);
    for (int i = 2; i <= 3; ++i) { arc.put(i, i, 50); arc.get(i); }
    arc.put(4, 4, 50); // 新项被 L1 淘汰，留下 B1 历史。
    assert(!arc.get(4));
    assert(arc.put(4, 4, 50)); // 幽灵历史达到 K，进入 L2，引发 LFU 淘汰。
    assert(arc.contains(1) && !arc.contains(2));
    assert(arc.averageFrequency() == arc.averageL2Frequency());
    assert(arc.l2TotalFrequency() <= arc.l2Size() * 32);
    ArcCacheFanLX<int, int> history(2, 200, 3);
    history.put(1, 1, 50); history.put(2, 2, 50); history.put(3, 3, 50);
    assert(!history.get(1)); assert(history.put(1, 1, 50));
    assert(history.l2Size() == 0); // 写入和首次回源共两次访问，未达到 K=3。
    assert(history.get(1)); assert(history.l2Size() == 1);

    std::mt19937 rng(2026);
    ArcCacheFanLX<int, int> randomCache(16, 400, 3, 4);
    for (int i = 0; i < 20000; ++i) {
        const int key = static_cast<int>(rng() % 64);
        if (rng() % 3 == 0) randomCache.erase(key);
        else if (auto value = randomCache.get(key)) assert(*value == key);
        else randomCache.put(key, key, 1 + rng() % 100);
        assert(randomCache.size() <= 16 && randomCache.bytes() <= 400);
        assert(randomCache.ghostSize() <= 16 && randomCache.targetL1() <= 16);
        assert(randomCache.l2TotalFrequency() <= randomCache.l2Size() * 4);
    }

    // 缓存结果池真正被使用，超容量扫描后槽位复用，clear 归还对象。
    QueryCacheFanLX poolCache(8, 8000);
    for (int i = 0; i < 1000; ++i) poolCache.put(std::to_string(i), {"中文结果"});
    assert(poolCache.metrics().poolCreates == 1000);
    assert(poolCache.metrics().poolBlocks == 1);
    poolCache.clear(); assert(poolCache.metrics().entries == 0);

    const auto directory = std::filesystem::temp_directory_path() /
        ("fanlx-query-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    try {
        {
            bool failCommit = false;
            PersistenceApplicationFanLX app(directory, [] { return std::string("2026-09-22T12:00:00Z"); },
                [&](auto stage) { if (failCommit && stage == CommitStageFanLX::BeforePublish)
                                     throw std::runtime_error("模拟写盘失败"); });
            app.initialize(); app.importInitial(demoSeedFanLX());
            app.login(owner, demoPasswordFanLX(owner.accountId)); app.login(admin, demoPasswordFanLX(admin.accountId)); app.login(fourth, demoPasswordFanLX(fourth.accountId));
            const AccountRefFanLX wxOwner{ServiceTypeFanLX::Wechat, "wx_2026001"};
            app.confirmLogin(owner, wxOwner);
            assert(app.queryCrossService(owner, wxOwner) == std::vector<std::string>{"wx_2026002"});
            app.bind(admin, {ServiceTypeFanLX::Wechat, "wx_2026002"}, true);
            assert(app.queryCrossService(owner, wxOwner).empty()); // 解绑发布版本后旧推荐立即失效。
            app.bind(admin, {ServiceTypeFanLX::Wechat, "wx_2026002"});
            assert(app.queryCrossService(owner, wxOwner).size() == 1);
            app.logout(wxOwner);
            rejectedFanLX([&] { app.queryCrossService(owner, wxOwner); });
            app.confirmLogin(owner, wxOwner);
            assert(app.queryCommonFriends(owner, admin).empty());
            const auto first = app.queryGroupMembers(owner, qq);
            assert(first == std::vector<std::string>({"2026001", "2026002", "2026003"}));
            assert(app.queryGroupMembers(owner, qq) == first);
            const auto hit = app.queryCacheMetrics().hits;
            rejectedFanLX([&] { app.queryGroupMembers(fourth, qq); }); // 已预热也不能越权。
            assert(app.queryCacheMetrics().hits == hit);
            app.renameGroup(owner, {GroupModeFanLX::QQ, "1002"}, "无关写入");
            assert(app.queryGroupMembers(owner, qq) == first);
            assert(app.queryCacheMetrics().hits == hit + 1);
            failCommit = true;
            rejectedFanLX([&] { app.approveApplication(owner, qq, fourth); });
            failCommit = false;
            assert(app.queryGroupMembers(owner, qq) == first);
            assert(app.queryCacheMetrics().hits == hit + 2); // 写失败不发布版本。
            app.approveApplication(owner, qq, fourth);
            assert(app.queryGroupMembers(owner, qq).size() == 4);
            app.leave(fourth, qq);
            rejectedFanLX([&] { app.queryGroupMembers(fourth, qq); });
            const auto friends = app.queryFriendAccounts(owner);
            assert(friends == std::vector<std::string>{"2026002"});
            app.removeFriend(owner, admin);
            assert(app.queryFriendAccounts(owner).empty());
            app.logout(owner);
            rejectedFanLX([&] { app.queryFriendAccounts(owner); });
            app.login(owner, demoPasswordFanLX(owner.accountId));
            const auto cached = app.queryGroupMembers(owner, qq);
            app.setQueryCacheEnabled(false);
            assert(app.queryGroupMembers(owner, qq) == cached);
            app.setQueryCacheEnabled(true);
            assert(app.queryGroupMembers(owner, qq) == cached);
            app.dissolve(owner, qq);
            rejectedFanLX([&] { app.queryGroupMembers(owner, qq); });
            app.saveAndClose();
        }
        {
            PersistenceApplicationFanLX app(directory);
            assert(app.queryCacheMetrics().entries == 0);
            rejectedFanLX([&] { app.queryFriendAccounts(owner); });
        }
        std::filesystem::remove_all(directory); // 仅删除本次创建的唯一测试目录。
    } catch (...) { throw; } // 失败保留现场。
}

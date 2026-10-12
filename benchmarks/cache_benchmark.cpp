// 缓存 / 回源对照基准。
//
// 设计要点（改动时请一并保持）：
//  1. 走真实应用查询入口（含权限检查、键构造、领域遍历与返回值复制），不是缓存数据结构的微基准；
//  2. 缓存开与关共用同一个查询计划，唯一自变量就是开关；关缓存的结果冻结为 expected，
//     每一轮每种模式逐项比对，缓存算错时基准直接失败而不是输出一份漂亮的错表；
//  3. 固定种子，数据规模与请求序列完全可复现；
//  4. 显式预热轮 + 交替测量顺序，用于对抗实测到的跨轮漂移；
//  5. 反向指标（全量快照写放大）必须报告，不能只展示缓存命中收益。
//
// 输出：一张长表 CSV，列定义见 writeRowFanLX()。可用 argv[1] 指定输出路径。
#include "friend/IdentityResolverFanLX.h"
#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using ClockFanLX = std::chrono::steady_clock;
static double elapsedFanLX(ClockFanLX::time_point start) {
    return std::chrono::duration<double, std::micro>(ClockFanLX::now() - start).count();
}

namespace {

// ---------------------------------------------------------------------------
// 配置：所有规模魔数集中在这里，控制台总结行也从同一份配置生成，
// 避免"改了循环忘了改输出"导致基准自己撒谎。
// ---------------------------------------------------------------------------
struct BenchConfigFanLX {
    std::uint32_t seed = 2026;
    int users = 1000;                      // 在演示种子之上叠加的基准用户数
    int groups = 100;                      // bench<i> 群的数量
    int membersPerGroup = 128;             // 每群成员数
    int requests = 1000;                   // 每个工作负载每轮的查询次数
    int warmupRounds = 1;                  // 不计入 CSV 的预热轮
    int measuredRounds = 9;                // 计入 CSV 的测量轮
    int distinctCommonPeers = 4;           // 共同好友查询里不同的对端数量（>1 才是真混合）
    int writeRepeats = 5;                  // 同一库上重复几次写事务
};
const BenchConfigFanLX kConfigFanLX{};

// ---------------------------------------------------------------------------
// 临时目录：异常路径同样要清理。用 RAII 而不是在 try 尾部手写 remove_all，
// 否则任何一次 throw（例如"缓存开关结果不同"）都会在 %TEMP% 留下整个数据目录。
// ---------------------------------------------------------------------------
struct TempDirGuardFanLX {
    std::filesystem::path path;
    ~TempDirGuardFanLX() {
        std::error_code error;
        std::filesystem::remove_all(path, error); // 清理失败不掩盖原始异常
    }
};

// 可移植的临时目录名：不依赖 Windows API，基准因此能在非 Windows 上编译。
std::filesystem::path makeTempDirFanLX() {
    std::random_device device;
    const auto tag = (static_cast<std::uint64_t>(device()) << 32) ^
                     static_cast<std::uint64_t>(device());
    return std::filesystem::temp_directory_path() /
           ("fanlx-benchmark-" + std::to_string(tag));
}

// 百分位从实际样本数推导，不写死下标——请求数一改就会越界或静默偏移。
double percentileFanLX(const std::vector<double> &sorted, double quantile) {
    if (sorted.empty()) return 0.0;
    const auto last = sorted.size() - 1;
    auto index = static_cast<std::size_t>(quantile * static_cast<double>(sorted.size()));
    return sorted[std::min(index, last)];
}

// ---------------------------------------------------------------------------
// 查询计划：把影响缓存行为的三个自变量显式建模。
// ---------------------------------------------------------------------------
enum class OpFanLX { GroupMembers, CommonFriends };
enum class WorkloadFanLX { Hot, Mixed, Scan };

std::string_view workloadNameFanLX(WorkloadFanLX workload) {
    switch (workload) {
    case WorkloadFanLX::Hot:
        return "hot";
    case WorkloadFanLX::Mixed:
        return "mixed";
    default:
        return "scan";
    }
}

struct RequestFanLX {
    OpFanLX op = OpFanLX::GroupMembers;
    int argument = 0; // GroupMembers: 群序号；CommonFriends: 对端序号
};

// hot  ：8 个群的高频复用，工作集远小于缓存容量，命中率应接近上限；
// scan ：100 个群顺序扫，覆盖全部群，容量不足时命中率应接近 0 且不污染长期段；
// mixed：80% 群成员查询跨 100 个群（有容量压力）+ 20% 共同好友查询。
//        对端在 distinctCommonPeers 个里随机取，冷启动后这些条目稳定命中——
//        即"廉价高频查询"与"昂贵稀有查询"争抢同一份缓存预算。
std::vector<RequestFanLX> buildPlanFanLX(WorkloadFanLX workload, const BenchConfigFanLX &config,
                                         std::mt19937 &rng) {
    std::vector<RequestFanLX> plan;
    plan.reserve(static_cast<std::size_t>(config.requests));
    std::uniform_int_distribution<int> coin(0, 99);
    for (int i = 0; i < config.requests; ++i) {
        switch (workload) {
        case WorkloadFanLX::Hot:
            plan.push_back({OpFanLX::GroupMembers, static_cast<int>(rng() % 8)});
            break;
        case WorkloadFanLX::Scan:
            plan.push_back({OpFanLX::GroupMembers, i % config.groups});
            break;
        default:
            if (coin(rng) < 20)
                plan.push_back({OpFanLX::CommonFriends,
                                static_cast<int>(rng() % config.distinctCommonPeers)});
            else
                plan.push_back({OpFanLX::GroupMembers,
                                static_cast<int>(rng() % config.groups)});
            break;
        }
    }
    return plan;
}

// ---------------------------------------------------------------------------
// 数据构造。规模全部来自 config；返回的计数由调用方从 state 实取，不手抄。
// ---------------------------------------------------------------------------
struct FixtureFanLX {
    PlatformSnapshotFanLX snapshot;
    std::size_t users = 0;
    std::size_t groups = 0;
    std::size_t friendships = 0;
};

FixtureFanLX buildFixtureFanLX(const BenchConfigFanLX &config) {
    auto state = SnapshotCodecFanLX::build(demoSeedFanLX());
    for (int i = 0; i < config.users; ++i) {
        const auto user = "B" + std::to_string(i), number = "900" + std::to_string(i);
        state.accounts.addUser(
            {user, "基准用户", "2000-01-01", "2026-09-01", "吉林", "pw-b" + std::to_string(i)});
        state.accounts.addAccount({user, ServiceTypeFanLX::QQ, number, false});
        state.accounts.subscribe(user, ServiceTypeFanLX::QQ);
    }
    IdentityResolverFanLX resolver(state.accounts);
    const auto ownerKey = resolver.accountKey(ServiceTypeFanLX::QQ, "2026001");
    // 两个"查询者"：owner 是无缓存/有缓存两侧共用的操作者，peer 只用于共同好友查询。
    const auto peerKey = resolver.accountKey(ServiceTypeFanLX::QQ, "2026002");
    for (int i = 0; i < config.groups; ++i) {
        auto &group = state.groups.create({GroupModeFanLX::QQ, "bench" + std::to_string(i)});
        group.addMember("U1", GroupRoleFanLX::Owner);
        for (int j = 0; j < config.membersPerGroup; ++j)
            group.addMember("B" + std::to_string((i * 7 + j) % config.users));
    }
    // 好友写进 QQ 分区（目录按服务分开存放，基准只压 QQ 这一份）。
    auto &qqFriends = state.friends.directory(ServiceTypeFanLX::QQ);
    for (int i = 0; i < config.users; ++i) {
        auto peer = resolver.accountKey(ServiceTypeFanLX::QQ, "900" + std::to_string(i));
        qqFriends.add(ownerKey, peer);
        // 偶数号同时是 2026002 的好友：这部分就是"共同好友"的非空结果集。
        if (i % 2 == 0) qqFriends.add(peerKey, peer);
    }
    FixtureFanLX fixture;
    fixture.snapshot = SnapshotCodecFanLX::capture(state);
    // 计数从真正落盘的快照实取：输出行因此不可能与构造出的数据脱节。
    fixture.users = fixture.snapshot.users.size();
    fixture.groups = fixture.snapshot.groups.size();
    fixture.friendships = fixture.snapshot.friendships.size();
    return fixture;
}

// ---------------------------------------------------------------------------
// 落盘：列定义只写一次，增删字段不会让某个调用点错位。
//
// 列语义（delta 与 gauge 必须分清，否则读者会把累积量当成每轮量）：
//   hits / misses / pool_creates_delta   —— 本轮增量（after - before）
//   payload_bytes / pool_reserved_bytes / pool_blocks —— 本行结束时的瞬时值（gauge）
// cache 列：-1 = 未启用缓存（写放大扫描与缓存无关），0/1 = 开关状态。
// p95_spread_us 只出现在汇总行（round=0，queries=0），是该负载各轮 p95 的极差。
// ---------------------------------------------------------------------------
void writeHeaderFanLX(std::ostream &out) {
    out << "workload,cache,round,queries,total_us,p50_us,p95_us,p95_spread_us,hits,misses,"
           "payload_bytes,pool_reserved_bytes,pool_blocks,pool_creates_delta\n";
}

void writeRowFanLX(std::ostream &out, std::string_view workload, int cache, int round,
                   std::size_t queries, double total, double p50, double p95, double p95Spread,
                   std::size_t hits, std::size_t misses, const QueryCacheMetricsFanLX &metrics,
                   std::size_t poolCreatesDelta, std::size_t poolBlocks) {
    out << workload << ',' << cache << ',' << round << ',' << queries << ',' << total << ',' << p50
        << ',' << p95 << ',' << p95Spread << ',' << hits << ',' << misses << ',' << metrics.bytes
        << ',' << metrics.poolReservedBytes << ',' << poolBlocks << ',' << poolCreatesDelta << '\n';
}

} // namespace

int main(int argc, char **argv) {
    try {
        const BenchConfigFanLX &config = kConfigFanLX;
        TempDirGuardFanLX temporary{makeTempDirFanLX()};
        std::ofstream csv(argc > 1 ? argv[1] : "cache-benchmark.csv");
        if (!csv) throw std::runtime_error("不能创建基准 CSV");
        writeHeaderFanLX(csv);

        std::size_t fixtureUsers = 0, fixtureGroups = 0, fixtureFriendships = 0;
        {
            PersistenceApplicationFanLX app(temporary.path);
            app.initialize();
            auto fixture = buildFixtureFanLX(config);
            fixtureUsers = fixture.users;
            fixtureGroups = fixture.groups;
            fixtureFriendships = fixture.friendships;
            app.importInitial(fixture.snapshot);
            const AccountRefFanLX owner{ServiceTypeFanLX::QQ, "2026001"};
            const AccountRefFanLX peer{ServiceTypeFanLX::QQ, "2026002"};
            app.login(owner, demoPasswordFanLX(owner.accountId), true);

            std::mt19937 rng(config.seed);
            for (const auto workload : {WorkloadFanLX::Hot, WorkloadFanLX::Mixed, WorkloadFanLX::Scan}) {
                const auto name = workloadNameFanLX(workload);
                const auto plan = buildPlanFanLX(workload, config, rng);
                const auto runQuery = [&](const RequestFanLX &request) {
                    if (request.op == OpFanLX::CommonFriends)
                        return app.queryCommonFriends(owner, peer);
                    return app.queryGroupMembers(
                        owner, {GroupModeFanLX::QQ, "bench" + std::to_string(request.argument)});
                };

                // 基准答案用关缓存路径预计算：既冻结 expected，又替冷路径热身。
                app.setQueryCacheEnabled(false);
                std::vector<std::vector<std::string>> expected;
                expected.reserve(plan.size());
                for (const auto &request : plan) expected.push_back(runQuery(request));

                // 开缓存方向单独热身一轮：ARC 的四个区与内存池都从零状态起步，
                // 不热身就会把首轮冷启动算进"缓存模式"的成本里。
                for (int warmup = 0; warmup < config.warmupRounds; ++warmup) {
                    for (int order = 0; order < 2; ++order) {
                        const bool cached = order == 1;
                        app.setQueryCacheEnabled(cached);
                        for (std::size_t i = 0; i < plan.size(); ++i)
                            if (runQuery(plan[i]) != expected[i])
                                throw std::runtime_error("预热轮缓存开关结果不同");
                    }
                }

                std::vector<double> roundP95;
                for (int round = 0; round < config.measuredRounds; ++round) {
                    // 交替测量顺序，让两种模式在整个轮次时间轴上对称分布。
                    for (int order = 0; order < 2; ++order) {
                        const bool cached = (round + order) % 2 == 1;
                        app.setQueryCacheEnabled(cached);
                        const auto before = app.queryCacheMetrics();
                        std::vector<double> latencies;
                        latencies.reserve(plan.size());
                        const auto begin = ClockFanLX::now();
                        for (std::size_t i = 0; i < plan.size(); ++i) {
                            const auto start = ClockFanLX::now();
                            const auto result = runQuery(plan[i]);
                            latencies.push_back(elapsedFanLX(start));
                            if (result != expected[i]) throw std::runtime_error("缓存开关结果不同");
                        }
                        const auto total = elapsedFanLX(begin);
                        std::sort(latencies.begin(), latencies.end());
                        const auto after = app.queryCacheMetrics();
                        const auto p95 = percentileFanLX(latencies, 0.95);
                        if (cached) roundP95.push_back(p95);
                        writeRowFanLX(csv, name, cached ? 1 : 0, round + 1, latencies.size(), total,
                                      percentileFanLX(latencies, 0.50), p95, 0.0,
                                      after.hits - before.hits, after.misses - before.misses, after,
                                      after.poolCreates - before.poolCreates, after.poolBlocks);
                    }
                }
                // 轮间 p95 极差：把跨轮漂移直接暴露在数据里，而不是让它污染平均值。
                // 用 round=0 且 queries=0 标记这是一条负载级汇总行，不是某一次测量。
                if (!roundP95.empty()) {
                    const auto spread = *std::max_element(roundP95.begin(), roundP95.end()) -
                                        *std::min_element(roundP95.begin(), roundP95.end());
                    const auto summary = app.queryCacheMetrics();
                    writeRowFanLX(csv, name, 1, 0, 0, 0, 0, 0, spread, 0, 0, summary, 0,
                                  summary.poolBlocks);
                }
            }

            // ---- 全量快照写放大扫描 ----
            // 领域查询很小，提交才是大头：TransactionCoordinatorFanLX::execute 每次都要
            // capture + build + 两次全量 JSON encode 比较 + validate。只报缓存收益不报这个
            // 数字，等于把最大的性能问题藏起来。
            //
            // 说明：受 importInitial 的"仅允许空库导入"约束，一次进程内只能有一个规模的
            // 数据库；这里重复同一个库上的多次写事务，给出的是该规模下写延迟的离散度，
            // 不是规模梯度曲线。要做梯度需要在子进程里按规模重建库。
            // 起始规模即演示种子自带的 12 个用户；fixture 里叠加的 1000 个 B 号用户
            // 并没有被 importInitial 写入，所以这里的绝对延迟不能与上面查询侧的规模混谈。
            const auto writeKey = GroupKeyFanLX{GroupModeFanLX::QQ, "1001"};
            for (int repeat = 0; repeat < config.writeRepeats; ++repeat) {
                const auto start = ClockFanLX::now();
                app.renameGroup(owner, writeKey, "基准写入" + std::to_string(repeat));
                writeRowFanLX(csv, "write", -1, repeat + 1, 1, elapsedFanLX(start), 0, 0, 0, 0, 0,
                              app.queryCacheMetrics(), 0, 0);
            }
            app.saveAndClose();
        }
        csv.close();
        if (!csv) throw std::runtime_error("写 CSV 失败");
        std::cout << "seed=" << config.seed << " users=" << fixtureUsers << " groups=" << fixtureGroups
                  << " friendships=" << fixtureFriendships << "; "
                  << "3 workloads x " << config.requests << " queries x " << config.measuredRounds
                  << " rounds (" << config.warmupRounds << " warmup) x 2 modes; "
                  << config.writeRepeats << " write transactions; all equal\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

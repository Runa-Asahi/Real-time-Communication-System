#include "persistence/DemoSeedFanLX.h"
#include "persistence/PersistenceApplicationFanLX.h"
#include "friend/IdentityResolverFanLX.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using ClockFanLX = std::chrono::steady_clock;
double elapsedFanLX(ClockFanLX::time_point start) {
    return std::chrono::duration<double, std::micro>(ClockFanLX::now() - start).count();
}

// 基准走真实应用查询入口，包含权限检查、键构造与返回值复制；无缓存分支同样计算结果。
int main(int argc, char **argv) {
    try {
        const auto directory = std::filesystem::temp_directory_path() /
            ("fanlx-benchmark-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        std::ofstream csv(argc > 1 ? argv[1] : "cache-benchmark.csv");
        if (!csv) throw std::runtime_error("不能创建基准 CSV");
        csv << "workload,cache,round,queries,total_us,p50_us,p95_us,hits,misses,payload_bytes,pool_reserved_bytes,pool_creates\n";
        {
            PersistenceApplicationFanLX app(directory);
            app.initialize();
            auto state = SnapshotCodecFanLX::build(demoSeedFanLX());
            for (int i = 0; i < 1000; ++i) {
                const auto user = "B" + std::to_string(i), number = "900" + std::to_string(i);
                state.accounts.addUser({user, "基准用户", "2000-01-01", "2026-09-01", "吉林"});
                state.accounts.addAccount({user, ServiceTypeFanLX::QQ, number, false});
                state.accounts.subscribe(user, ServiceTypeFanLX::QQ);
            }
            IdentityResolverFanLX resolver(state.accounts);
            const auto ownerKey = resolver.accountKey(ServiceTypeFanLX::QQ, "2026001");
            for (int i = 0; i < 100; ++i) {
                auto &group = state.groups.create({GroupModeFanLX::QQ, "bench" + std::to_string(i)});
                group.addMember("U1", GroupRoleFanLX::Owner);
                for (int j = 0; j < 128; ++j) group.addMember("B" + std::to_string((i * 7 + j) % 1000));
            }
            for (int i = 0; i < 1000; ++i) {
                auto peer = resolver.accountKey(ServiceTypeFanLX::QQ, "900" + std::to_string(i));
                state.friends.add(ownerKey, peer);
                if (i % 2 == 0) state.friends.add(resolver.accountKey(ServiceTypeFanLX::QQ, "2026002"), peer);
            }
            app.importInitial(SnapshotCodecFanLX::capture(state));
            const AccountRefFanLX owner{ServiceTypeFanLX::QQ, "2026001"};
            app.login(owner, true);
            std::mt19937 rng(2026);
            for (const auto &workload : {"hot", "mixed", "scan"}) {
                std::vector<int> requests;
                for (int i = 0; i < 1000; ++i)
                    requests.push_back(std::string(workload) == "scan" ? i % 100 :
                        static_cast<int>(rng() % (std::string(workload) == "hot" ? 8 : 100)));
                auto query = [&](int number) {
                    if (std::string(workload) == "mixed" && number % 5 == 0)
                        return app.queryCommonFriends(owner, {ServiceTypeFanLX::QQ, "2026002"});
                    return app.queryGroupMembers(owner, {GroupModeFanLX::QQ, "bench" + std::to_string(number)});
                };
                app.setQueryCacheEnabled(false);
                std::vector<std::vector<std::string>> expected;
                for (const auto request : requests) expected.push_back(query(request));
                for (int round = 0; round < 5; ++round) {
                    // 交替测量顺序，降低始终先跑无缓存带来的热身偏差。
                    for (int order = 0; order < 2; ++order) {
                        const bool cached = (round + order) % 2 == 1;
                        app.setQueryCacheEnabled(cached);
                        const auto before = app.queryCacheMetrics();
                        std::vector<double> latencies;
                        const auto begin = ClockFanLX::now();
                        for (std::size_t i = 0; i < requests.size(); ++i) {
                            const auto start = ClockFanLX::now();
                            const auto result = query(requests[i]);
                            latencies.push_back(elapsedFanLX(start));
                            if (result != expected[i]) throw std::runtime_error("缓存开关结果不同");
                        }
                        const auto total = elapsedFanLX(begin);
                        std::sort(latencies.begin(), latencies.end());
                        const auto after = app.queryCacheMetrics();
                        csv << workload << ',' << cached << ',' << round + 1 << ",1000," << total << ','
                            << latencies[500] << ',' << latencies[950] << ',' << after.hits - before.hits << ','
                            << after.misses - before.misses << ',' << after.bytes << ',' << after.poolReservedBytes
                            << ',' << after.poolCreates - before.poolCreates << '\n';
                    }
                }
            }
            // 全量快照写放大必须报告，不能只展示缓存命中收益。
            for (int round = 1; round <= 5; ++round) {
                const auto start = ClockFanLX::now();
                app.renameGroup(owner, {GroupModeFanLX::QQ, "1001"}, "基准写入" + std::to_string(round));
                csv << "write,0," << round << ",1," << elapsedFanLX(start) << ",0,0,0,0,0,0,0\n";
            }
            app.saveAndClose();
        }
        csv.close();
        if (!csv) throw std::runtime_error("写 CSV 失败");
        std::filesystem::remove_all(directory);
        std::cout << "seed=2026 users=1012 groups=112; 3 workloads x 1000 queries x 5 rounds x 2 modes; all equal\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}

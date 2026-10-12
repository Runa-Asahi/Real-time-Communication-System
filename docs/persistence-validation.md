# S06 持久化验收记录

验收日期：2026-09-14。对应 [progress.md](progress.md) 的 P07.1—P07.7、T11/T12 和 S05 延迟存储验收；格式契约见 [data-format.md](data-format.md)。本记录只说明 S06，不能作为整个课设或性能目标全部完成的证明。

> **证据目录说明（2026-09-23 清理）**：本文中的 `.artifacts/...` 是当时存放证据的临时目录，现已连同其它临时产物一并删除。下文的命令与结论仍然有效——把 `.artifacts/xxx` 换成任意构建目录即可复现；需要重新生成发布级证据请运行 `scripts/verify-release.ps1`，它会在独立目录下构建、跑全量 CTest 并保存日志与 SHA-256 清单。

> **2026-09-30 凭据改造对本记录的补充**：users[] 新增明文登录密码字段 `password`（约束见 [data-format.md](data-format.md)），
> 因此下文 T11a 的「全字段比较」现在也覆盖密码，夹具 [valid-state-v1.json](../tests/fixtures/persistence/valid-state-v1.json)
> 已按新字段重新生成（users[] 中可见各演示账号的密码（现统一为 `123`；该 fixture 生成于改动之前，仍为 `pw-...`））。T11b 的结论不受影响：
> 登录态从不写盘，与新增的持久密码字段是两件不同的事。改造前初始化的旧快照因缺 password 被校验器拒绝，
> 报 `用户缺少登录密码（旧数据请重新 --init-demo 初始化）`；迁移＝删除该数据目录后重新 `--init-demo`，
> 程序不就地补字段。

## 环境与构建

| 项目 | 实测值 |
| --- | --- |
| 系统 | Windows NT 10.0.26200.0，64 位 |
| 文件系统 | C: NTFS，本地同卷目录 |
| 编译器 | D:/mingw64，MinGW GCC 15.1.0，C++17 |
| CMake | 4.4.3，MinGW Makefiles（工程最低要求 3.20） |
| 依赖 | nlohmann/json 3.11.3，源码内固定头文件，无配置期下载 |
| 默认构建 | build；smoke、cache_behavior、group_behavior、persistence_behavior 全通过，2.78 s |
| Debug | .artifacts/build-debug；同上 4/4 通过，3.81 s |
| Release | .artifacts/build-release；同上 4/4 通过，1.63 s |

本轮已从独立目录配置 Debug、Release，并在最后修改后增量重建及运行全量 CTest。关键断言在 Release 下仍执行：group_tests / persistence_tests 在包含 cassert 前取消 NDEBUG，cache_tests 使用 -UNDEBUG。持久化库、主入口及新增测试以 -Wall -Wextra -Wpedantic -Werror 编译。

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -- -j4
ctest --test-dir build --output-on-failure

cmake -S . -B .artifacts/build-debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build .artifacts/build-debug -- -j4
ctest --test-dir .artifacts/build-debug --output-on-failure

cmake -S . -B .artifacts/build-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .artifacts/build-release -- -j4
ctest --test-dir .artifacts/build-release --output-on-failure
```

## 数据、输入与判定

测试源为 [persistence_tests.cpp](../tests/persistence_tests.cpp)。每次创建独立的系统临时目录 fanlx-persistence-进程号-时钟值；正常通过时清理该目录，异常时输出并保留证据。断言导致的进程终止可能来不及打印目录，此时可按测试进程号定位。不会修改默认 data。

通用前置状态由 initialize 加 importInitial 生成：第 1 代为空库，第 2 代包含 12 名用户（各带演示登录密码，2026-10-10 起统一为 `123`）、36 个服务账号、12 组绑定、QQ/微信各 6 个群、双方私有中文备注、子群和待处理流程。测试时钟固定为 2026-09-14T12:00:00Z；真实演示使用系统 UTC。完整可解析样例为 [valid-state-v1.json](../tests/fixtures/persistence/valid-state-v1.json)。

| 对应条款 | 输入和检查内容 | 实际结果 |
| --- | --- | --- |
| T11a、R12/R21 | 编码→写盘→关闭→重建，比较完整 JSON，包括账号/开通/绑定/私有备注/同号群/角色/时间/序号/流程/子群/用户资料与密码 | 全字段相等，中文内容保留 |
| T11b | 在中文目录中用 CreateProcessW 启动独立进程；此前已登录、切换并保存 | 新进程加载成功，登录态为空，当前模式保持 |
| T11c、S05 §7.4 | QQ→微信保存重启→QQ；保留休眠 Admin，退群后不复活 | 模式及成员符合规则，原 T06—T10 无回归 |
| T11d | 多次业务事务后对比群与子群指针；审批新成员后退出；转让和切换 | 存续外壳地址相同，序号空洞与 nextJoinOrder 保留 |
| T11e | 注册/开通/绑定解除/资料、好友增删备注、群申请邀请审批拒绝、加退踢、任免转让、子群、改名、切换、解散、退出 | 均通过应用事务入口；幂等申请不增代；归档重启保留 |
| T12a | 空目录、仅孤立 tmp、删除主文件但保留备份 | 分别 Empty、UncommittedOnly、RecoveredReadOnly；tmp 不自动提交 |
| T12b | 主文件写入 broken、主备双损坏、schemaVersion=999 | 只读恢复/失败/未知版本分别处理；禁止自动初始化覆盖 |
| T12c | 非法用户/账号/好友/子群引用、缺失或重复 Owner、非法序号/日期/代数/模式、重复属性、深度超限、非法 UTF-8 | 拒绝加载；直接仓储提交非法 C++ 枚举也被拒绝，原文件不变 |
| T12d | 候选修改后抛 bad_alloc，或资料带非法 UTF-8 导致编码失败 | 不发布候选；在线快照和旧存档保持不变 |
| T12e/g | 七个提交前阶段注入异常，执行 QQ→微信切换 | 全字段、策略对象地址、局部版本及旧主文件字节不变 |
| T12f | 六个提交位置真实 TerminateProcess，随后重新加载 | 提交前为旧完整第 2 代；提交后为新完整第 3 代 |
| T12h | 首进程持锁时启动第二进程；首进程被终止后重新打开；不同测试目录 | 同目录第二写进程被拒；OS 释放异常进程锁，其他目录可独立使用 |
| T12i | 恢复前写入、恢复发布前失败、恢复后重新校验失败、备份代数较新、提交中外部改写 | 阻止不安全写入；有效备份保留；可重试的恢复成功后继续提交 |
| T12j | 中文路径、真实 FILE_ATTRIBUTE_READONLY 目标、写入中/刷新前注入错误 | 路径往返正确，访问拒绝不损坏旧主文件，注入错误不发布内存 |
| 工作流 | 恢复含已过期申请的快照后尝试审批 | 不自动入群，候选事务将记录关闭为 Expired |

恢复成功测试还核对 state.damaged.* 的原始损坏字节，以及 recovery.*.json 的来源代数、时间和 oldGeneration。旧主文件不能可信解析时 oldGeneration 为 null，不从损坏内容猜测代数。提交途中无法确认主文件属于旧版还是候选版时报告 CommitIndeterminate，后续写入被阻断。

## 故障位置与提交边界

| CommitStageFanLX | 注入异常 | 真实终止子进程 | 期望重启状态 |
| --- | --- | --- | --- |
| BeforeWrite | 是 | 否 | 旧主文件 |
| DuringWrite | 是，在写完一个块后 | 否 | 旧主文件，临时文件可能不完整 |
| BeforeFlush | 是 | 否 | 旧主文件 |
| AfterTemporary | 是 | 是 | 旧第 2 代 |
| BeforeBackup | 是 | 否 | 旧主文件 |
| AfterBackup | 是 | 是 | 旧第 2 代，备份可能与主同代 |
| BeforePublish | 是 | 是 | 旧第 2 代 |
| AfterPublish | 是，仓储核验候选后返回成功 | 是 | 新第 3 代 |
| BeforeMemoryPublish | 无异常注入断言 | 是 | 新第 3 代 |
| BeforeReply | 无异常注入断言 | 是 | 新第 3 代 |

主文件发布是提交点。发布后未收到业务应答，不能推断操作未提交。恢复发布前失败保持只读，允许重新确认；恢复已发布后若重新校验或内存重建失败，应用关闭仓储并要求重新启动，禁止旧内存继续写入。

## 证据与复查

- .artifacts/S06/ctest-default.log、ctest-debug.log、ctest-release.log：最终 CTest 原始 LastTest.log 副本，含命令、时间、输出和结果。
- .artifacts/S06/*.clangd.log：核心翻译单元检查记录；clangd 22.1.6，使用 build/compile_commands.json 和 MinGW query-driver；--tweaks=none 仅关闭 assert 宏重构动作自测，保留诊断检查。
- .artifacts/S06/sample-data：真实命令行 --init-demo 创建后再次启动的样例目录，users=12、generation=2；不可重复使用 --init-demo 覆盖。
- .artifacts/S06/final-manifest.txt：本次工具链、源码/夹具/文档及第三方头文件 SHA-256，可用于识别测试所对应版本。

```powershell
.\build\fanlx_app.exe --demo
.\build\fanlx_app.exe --data-dir .artifacts/S06/sample-data
```

--demo 输出另一独立证据目录，完成实际存档、群切换无损、关闭重开与全字段比较；不是写死成功文本。原始课程任务书未修改，最终正式 DOCX 报告按 S11 更新。

## 尚未实测的边界

没有进行真实硬件断电、真实磁盘耗尽、网络文件系统或跨卷发布试验。DuringWrite/BeforeFlush 是控制流异常注入，并未强制真实 WriteFile 返回短写或 FlushFileBuffers 返回失败；生产代码检查了这些返回值，测试证明的是这些阶段中止后的事务行为。真实 OS 测试包括文件替换拒绝、独占锁、中文路径和进程强制终止。不能将本记录表述为任意硬件断电绝对零丢失。

64 MiB、字段/集合限额及单写线程约束已落实代码，但本轮未进行最大规模压力、实际业务缓存性能或峰值内存测量；这些属于 S07/S10。全量快照事务存在复制、校验及写盘成本，不能从 CTest 耗时推断生产吞吐量。

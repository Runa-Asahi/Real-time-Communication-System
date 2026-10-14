# 混合 TCP：服务器控制面 + 好友单聊直连

2026-10-14。在已有的 Winsock2 中心转发上增加在线好友单聊的点对点 TCP。群、离线、登录、好友与群权限、持久化、历史和离线补发仍走服务器。本文是这条路径的技术说明。协议字段的旧表仍在 [network-validation.md](network-validation.md)。对象池数字在 [cache-validation.md](cache-validation.md) 有同一份记录。

## 两条平面

| | 控制面 | 数据面 |
| --- | --- | --- |
| 谁和谁连 | 每个客户端一条到 `ImServerFanLX` 的长连接 | 两个已登录好友之间一条 `DirectLinkFanLX` TCP |
| 承载 | 登录、好友与群权限、SEND 落盘、SEND_ACK、投递游标、ACK、在线状态、群消息、离线补发 | 仅在线好友单聊的正文 |
| 实现 | 原有 `TcpPeerFanLX` + `ImClientFanLX` 工作线程 | `DirectLinkFanLX` 复用 `TcpListenerFanLX`、`TcpPeerFanLX`、`MessageCodecFanLX` |

服务器连接不断开。直连失败时，同一条已落盘的消息改由服务器把正文发给接收方。界面不假装直连：监听失败或握手失败时走中转，并给出说明。

`ImClientFanLX` 默认 `preferDirect=true`。不发 `P2P_HELLO` 的旧客户端保持纯中转。现有 `network_tests` 用原始 `TcpPeer`，不会握手，因此仍断言 `DELIVER`。

## 协商与认证

1. 客户端在**已经登录**的服务器连接上发送 `P2P_HELLO`。`text` 是自己的监听端点，形如 `ep:127.0.0.1:端口`。服务器不把观察到的源地址拿来替换这个声明。
2. 双方都是好友、都在线、都宣告了合法端点时，服务器发 `P2P_OFFER` 给连接方、`P2P_EXPECT` 给接受方。账号字符串较小的一方拨号，双方都监听。
3. 票据放在 `requestId`，约 30 秒，绑定连接方、接受方和服务。只在已认证的服务器会话上签发。直连路径不传密码。
4. 连接方发 `P2P_AUTH`。接受方只接受本地 `EXPECT` 里那一张未过期、未用过、账号匹配的票据。缺失、过期、复用或不匹配都拒绝。未通过认证的 TCP 不能发聊天正文。
5. 双方都向服务器回 `P2P_READY` 后，路由变为 `DirectReady`。`P2P_FAIL` 把该票据或该好友对降为 `DirectDegraded`。

路由：`RelayOnly` → `DirectConnecting`（报价，5 秒未就绪则超时）→ `DirectReady`。失败后 `DirectDegraded`，8 秒内不再报价。日志只写 `fanlx-p2p`、路由和 `messageId`，不写票据、密码或正文。

## 一条单聊怎么送出

服务器先按原路径把消息写入 JSON 日志或 SQLite，再发 `SEND_ACK`。这一步与是否直连无关。

随后对**接收方**（不是发送方自己的历史回显）：

- 可以直连时，服务器发 `P2P_ARM`。`text` 是持久化正文的 FNV-1a 64 位十六进制，前缀 `h:`，**没有正文**。`messageId`、`requestId`、`conversationSeq` 与中转帧相同。同时给发送方发 `P2P_PUSH`。
- 发送方在工作线程上按 `requestId` 找到刚才提交的正文，经直连套接字发 `P2P_DATA`。
- 接收方核对哈希，再走原来的幂等落库，然后在**服务器套接字**上发 `ACK`。`ACK` 必须等于该连接当前的 `inFlight`，且 `conversationSeq != 0`。游标只在这条 ACK 上前进。
- 哈希不符就丢掉直连正文，等待服务器补发带正文的 `DELIVER`。
- 3 秒内没有 ACK：服务器用**同一个** `messageId` 发 `DELIVER`（带正文），记一次 `RelayFallback`，并把路由降级。不等待原来的 10 秒空闲关闭。不循环重发 `P2P_ARM`。
- 会话在 ACK 前断开：`directTried` 保留，重连后的 `deliver()` 走中转，避免同一条消息再等一轮 3 秒。

发送方自己的历史回显始终是带正文的 `DELIVER`，这样原有收件箱和历史不用改格式。`peerRelayBodies` 只计算经服务器发给对端的正文，成功的直连路径上它为 0。群消息、离线、非好友和临时会话不进入直连。好友关系仍由服务器判定。

同一连接里重复的 ACK 由客户端 `ackQueued` 压住。新的服务器连接会清空这个集合，因此断线后对已入库消息的再次 ACK 仍然会发出（崩溃恢复测试依赖这一点）。

## 同机、局域网、NAT

| 范围 | 行为 |
| --- | --- |
| 默认 | 绑定并宣告 `127.0.0.1` 上的临时端口。同一台机器上的两个客户端可以直连。自动化测试只覆盖这一档。 |
| 局域网 | 把环境变量 `FANLX_P2P_BIND` 设成一块网卡的具体 IPv4，例如 `192.168.1.20`。`0.0.0.0` 和主机名会被拒绝，退回 `127.0.0.1`。 |
| NAT / 公网 | 不支持。没有打洞，也没有中继地址替换。对端拨不到时，3 秒后正文走服务器。 |

监听失败时客户端提示：「本机直连端口监听失败，单聊将通过服务器中转。若防火墙拦截入站连接，允许本程序或继续使用中转即可。」握手成功时提示「与 <账号> 的单聊已改为点对点直连」。这些是 `Kind::Notice`，不是连接错误。

## 帧与兼容

帧仍是 4 字节大端长度 + UTF-8 JSON，最大载荷 16384，正文最大 4096。对象是平坦的。空的可选字段省略，因此旧的 v2 字节不变。v2 为 11 个字段，v3 为 12 或 13（`protocolVersion`、`acceptedAtUtc`）。多出来的 JSON 键会让解码抛异常。新类型只进入编解码白名单，不新增键。

| type | 方向 | 用到的已有字段 |
| --- | --- | --- |
| `P2P_HELLO` | 客户端 → 服务器 | `text` = 端点 |
| `P2P_OFFER` | 服务器 → 连接方 | `requestId` = 票据，`text` = 接受方端点，`recipient` = 接受方 |
| `P2P_EXPECT` | 服务器 → 接受方 | `requestId` = 票据，`recipient` = 连接方 |
| `P2P_AUTH` / `P2P_AUTH_OK` | 直连套接字 | `requestId` = 票据，`sender` / `recipient` = 双方账号 |
| `P2P_READY` / `P2P_FAIL` | 客户端 → 服务器 | `requestId` = 票据；失败原因只允许 `timeout` / `connect` / `rejected` / `closed`，其余记为 `fail` |
| `P2P_ARM` | 服务器 → 接收方 | 与 `DELIVER` 相同的编号和序号，`text` = 正文哈希 |
| `P2P_PUSH` | 服务器 → 发送方 | 同上，发送方据此把正文推到直连 |
| `P2P_DATA` | 直连套接字 | `messageId`、`requestId`、`conversationSeq`、`text` = 正文 |

未知类型在编解码层抛出。客户端 `received()` 必须识别上表，否则会把合法帧当成「服务端响应类型非法」。

## 对象池

`MemoryPoolFanLX<T, BlockSize>` 是头文件。`QueryCacheFanLX` 使用 `MemoryPoolFanLX<QueryResultFanLX, 64>`，给查询结果用，不是聊天消息池。缓存仍非权威：先鉴权，再查缓存。ARC / LFU / LRU 的淘汰策略没有改。

| 约定 | 说明 |
| --- | --- |
| 线程 | 不加锁。查询缓存把池放在自己的对象里，由持有缓存的那一个线程调用。 |
| 寿命 | 块留到池析构。空闲槽可以复用，整块不还给操作系统。基准里 `peak_live=1`，若空块回收，每次操作都会重新分配。 |
| `allocations()` / `poolCreates` | 成功构造次数。`query_cache_tests` 要求 1000 次 put、容量 8 时 `poolCreates==1000` 且 `poolBlocks==1`。 |
| 新增计数 | `reuses`、`live`、`peakLive`、`rejectedDestroys`。`destroy` 返回 `bool`；不看返回值的旧调用仍能编译。 |
| `reservedBytes()` | 只算池块数组。不是进程 RSS，也不含 `std::string`、`vector` 或哈希表自己的堆分配。 |
| 安全 | 用块地址的整数范围判断槽位。无关指针做 `<` 比较是未定义行为。地址必须正好落在槽位存储上，且 `live==true`，否则 `rejected` 加一，不调用析构，也不放回空闲链表。构造抛异常时槽位归还，且不计入成功构造。析构会销毁仍然存活的对象。 |

基准 `benchmarks/pool_benchmark.cpp`：`PayloadFanLX{int, 32 字符 string}`，块大小 64，200000 次 create/destroy，与同样次数的 `new`/`delete` 对照。校验和两边都是 `20021700000`。每次还打印 `constructions=200000 reuses=199999 blocks=1 reserved_bytes=3584 peak_live=1`。

同日较早三次（对象池改完、P2P 编译之前）：

| 次序 | pool_us | plain_us |
| --- | ---: | ---: |
| 1 | 24503 | 23776 |
| 2 | 23974 | 23810 |
| 3 | 27109 | 23626 |

2026-10-14 本轮三次（`build\pool_benchmark.exe`，退出码 0）：

| 次序 | pool_us | plain_us |
| --- | ---: | ---: |
| 1 | 23786 | 24926 |
| 2 | 23837 | 24513 |
| 3 | 23833 | 24476 |

六次里谁更快会换边，差距大约几个百分点。正文里的 `std::string` 每次仍要堆分配，池只省下了外层对象那一块。**不能写成「对象池更快」。** 改池之前的全量 CTest 基线没有跑，不能把后来的通过说成「相对改池前零回退」的对照。

## 构建、运行、排错

工具链与 [HELP.md](../HELP.md) 相同：MinGW 在 `D:\mingw64\bin`，Qt 6.11.1 在 `E:/Qt/6.11.1/mingw_64`。配置：

```powershell
$env:PATH = "D:\mingw64\bin;" + $env:PATH
cmake -S . -B build -G "MinGW Makefiles" -DFANLX_BUILD_GUI=ON -DCMAKE_PREFIX_PATH="E:/Qt/6.11.1/mingw_64"
cmake --build build --target fanlx_network network_tests p2p_tests -- -j4
```

```powershell
ctest --test-dir build -R "p2p_behavior|network_behavior|im_behavior" --output-on-failure
.\build\pool_benchmark.exe
```

`p2p_tests` 自己拉起本机服务器和两个客户端，端口用 0，不访问公网，也不需要指定网卡。它用 `GetExtendedTcpTable` 确认存在一条指向客户端监听端口的 `ESTABLISHED` 连接，且两端属于同一进程。

| 现象 | 含义 |
| --- | --- |
| 日志 `route=RelayOnly reason=listen` | 直连端口没听上，单聊走中转。检查端口占用或入站防火墙。 |
| `route=RelayFallback` | 直连在 3 秒内没有完成 ACK，正文已改由服务器送出。消息编号不变。 |
| `hello-rejected` | 端点不是 `ep:` + 点分 IPv4 + 端口，或用了 `0.0.0.0` / 域名。 |
| 两台机器互相只能中转 | 默认绑的是 `127.0.0.1`。局域网需设置 `FANLX_P2P_BIND`。NAT 不会变成直连。 |
| 登录报「网络操作超时」 | 服务器没在听。本机拒绝连接大约要 2 秒，聊天登录超时也是 2 秒。不要把 `Desktop\OOP\build\fanlx_app.exe` 占在 `127.0.0.1:9500` 上。 |

## 课程要求对照

类别按任务书：必做、选做 6.(5)、课后不考核、本仓库的工程优化。类名后缀 `FanLX`，不使用 MFC。下表「差距」指本次开工前；已经对齐的必做项本次没有改行为。

| 类别 | 原文 | 现有实现 | 差距 | 方案 | 验收 |
| --- | --- | --- | --- | --- | --- |
| 必做 一、1 | 号码、昵称、出生时间、T 龄、所在地、好友列表、群列表；微博与 QQ 同号，微信独立号可绑定 | `UserProfileFanLX`、`ServiceAccountFanLX`、微X 上层、按服务分区 | 无 | 不改账户模型 | `cache_behavior`、`persistence_behavior`、`console_behavior` |
| 必做 一、2 | 好友增删改查；查询共同好友，微信可添加 QQ 推荐 | `FriendServiceFanLX`、查询缓存、申请/同意 | 无 | 直连不绕过好友判定 | `im_behavior`；`p2p_behavior` 确认非好友群发送被拒 |
| 必做 一、3 | 群 1001–1006；加入、退出、踢人、查成员；QQ 可申请、可子群、有管理员，微信只邀请、无子群、仅群主特权 | `GroupPolicyFanLX` 策略族与 `GroupServiceFanLX` | 无 | 群消息保持 `DELIVER` | `group_behavior`；`p2p_behavior` 断言群计数增加且 `p2pMessages` 不增加 |
| 必做 一、4 | 用户自行开通 N 个微X 服务 | `activateService`，注册只建微X | 无 | 不改开通 | `persistence_behavior`、`console_behavior` |
| 必做 一、5 | 一个服务登录后，其它服务简单确认即登录 | 微X 登录后已开通服务一并进入登录态 | 无 | 不改登录 | `cache_behavior`、`query_cache_behavior` |
| 必做 一、6（1） | 开通、群成员、好友可预存文件，启动时载入内存 | `JsonFileRepositoryFanLX`、`state.json` | 无 | 不改快照格式 | `persistence_behavior` |
| 必做 一、6（2）（3） | 登录一个服务后其余已开通服务进入开通状态；可用另一服务的好友添加好友 | 微X 会话开通下级服务；`via` 换算后仍须对方同意 | 无 | 不改 | `console_behavior`、`im_behavior` |
| 必做 一、6（4） | 展示当前群特色，成员数据不受损即可换成另一类管理模式 | `switchPolicy` | 无 | 不改 | `group_behavior` |
| 选做 一、6（5） | QQ 点对点 TCP：`ws2_32`，`bind` / `listen` / `accept` / `connect` / `send` / `recv` | 原先只有客户端到服务器的 TCP | 没有客户端之间的收发 | `DirectLinkFanLX` 在临时端口上监听和拨号；服务器只协商；失败回退中转 | `p2p_behavior`：JSON 与 SQLite 各一条成功路径，TCP 表证明连的是客户端端口；`peerRelayBodies==0` 且 `p2pMessages>=3` |
| 必做 二、2–4 | 类切割、继承或组合、多态 | 账户 / 好友 / 群策略 / 仓储已分层 | 无 | 直连是独立类，不把监听写进界面 | 头文件独立，类名带 `FanLX` |
| 优化 二、5（1）（2） | 数字菜单；启动读文件，退出写回 | `ConsoleAppFanLX`、原子提交 | 无 | 不改菜单结构 | `console_behavior`、`smoke` |
| 不考核 八 | 可视化、多人聊天、聊天记录查询、多线程、数据库 | Qt 客户端、SQLite、历史查询、工作线程已经存在 | 这些不是本课计分项 | 直连复用已有落库和 ACK，不改历史界面，不改库表 | `chat_history_behavior`、`im_behavior` 仍通过。图形用例见 HELP，本次未把界面当成验收对象 |
| 工程优化 | 对象池（任务书未单列，本仓库 S07 已有） | `MemoryPoolFanLX` 被查询缓存使用 | 外来指针比较有未定义行为；成功构造和复用分不清 | 整数地址校验，`destroy` 返回是否接受，增加复用/存活/拒绝计数，不回收空块 | `query_cache_behavior`；上表六次基准 |

## 本次改动的文件

| 文件 | 原因 |
| --- | --- |
| `include/infra/MemoryPoolFanLX.h` | 归属检查、异常归还、析构清理、复用与拒绝计数 |
| `include/infra/QueryCacheMetricsFanLX.h`、`include/infra/QueryCacheFanLX.h` | 把新计数交到查询缓存的统计里。`poolCreates` 仍是成功构造 |
| `tests/query_cache_tests.cpp` | 抛异常构造、对齐、非法 `destroy`、容量 8 上 1000 次 put |
| `benchmarks/pool_benchmark.cpp`、`CMakeLists.txt` | 可重复的对照基准，不预设谁更快 |
| `include/network/P2pProtocolFanLX.h` | 类型名、端点解析、正文哈希。不新增 JSON 键 |
| `include/network/DirectLinkFanLX.h`、`src/network/DirectLinkFanLX.cpp` | 监听、拨号、票据握手、正文收发。线程不放在 Qt 主线程 |
| `src/network/MessageCodecFanLX.cpp` | 白名单加入 10 个直连类型 |
| `include/network/ImServerFanLX.h`、`src/network/ImServerFanLX.cpp` | 报价、就绪、ARM/PUSH、3 秒回退、统计 |
| `include/network/ImClientFanLX.h`、`src/network/ImClientFanLX.cpp` | 登录后 HELLO、处理报价、把 PUSH 变成直连 DATA、ACK 仍走服务器 |
| `tests/p2p_tests.cpp` | 本机双客户端矩阵：成功、关闭直连、握手失败、忽略 DATA 后回退、退出重登、群与跨服务、伪造票据 |
| `docs/hybrid-tcp.md` 及 README、HELP、验收文档中的索引 | 把控制面/数据面、回退和实测数字写下来 |

## 边界

- 不做 NAT 穿越。跨机器默认回退，除非 `FANLX_P2P_BIND` 是本机网卡地址且对端拨得到。
- `reservedBytes()` 不是内存占用。
- 发送方回显仍是服务器 `DELIVER`。成功直连时 `relayDirectMessages` 仍会增加，`peerRelayBodies` 不会。
- 同一时刻一条连接只有一个在途投递。
- 对象池在这六次运行里没有稳定的速度优势。
- 改池之前没有留下一份 CTest 基线。

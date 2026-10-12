# 交接提示词：可视化即时通信扩展（E01–E06）

> 用途：把本仓库的**当前真实状态、全部已知问题、验证方法、约束**交给另一个 AI 继续工作。
> 本文件是提示词，不是宣传材料：**未通过验证的条目一律写在"未解决"里**。
> 生成时间：2026-09-26。生成时的实测状态见第 2 节。

---

## 0 给接手者的任务指令（可直接复制给 AI）

```
你要在 C:\Users\25686\Desktop\OOP（Windows，MinGW GCC 15.1.0 + CMake，C++17）继续完成
"可视化 多线程与数据库扩展"。计划原文是 docs/extension-plan.md（E01–E06），
项目结构与构建命令见 docs/project-structure.md，进度与证据见 docs/progress.md 末尾。

先读这三份文档，再按第 5 节的优先级表工作。硬性约束见第 2 节，违反会直接污染仓库。

工作方式要求（重要，前一位接手者在这里浪费了大量时间）：
1. 修改源码只用"精确编辑"（read + edit/write 工具，或带确切 old_string 的替换）。
   禁止用 PowerShell 的 Get-Content/Set-Content/-replace 做批量改写：本项目源码含大量中文注释，
   该做法已两次破坏 UTF-8 编码、并误删过控制流语句，导致回归失败。
2. 不要用"全局删除所有调试打印行"这类按行脚本清理插桩：跨行语句会被切断。
3. 每改一处就跑对应测试；不要连续插桩调试超过 2 轮而不收敛。
4. 必须区分"计划/实现/实际验证"三态写入 docs/progress.md，未实测不得写作完成。
```

---

## 1 环境与构建（已实测可用）

| 项 | 值 |
| --- | --- |
| 编译器 | MinGW GCC 15.1.0（`D:/mingw64`，已 `-static` 链接运行库） |
| CMake | 4.4.3；`cmake_minimum_required(3.20)` |
| Qt | `E:/Qt/6.11.1/mingw_64`（6.11.1 预编译包，**实测与本机 GCC 15.1 可混用**） |
| SQLite | 系统 `C:/Windows/System32/winsqlite3.dll` = 3.51.1；头文件 `third_party/sqlite/sqlite3.h`（3.51.1） |
| 构建目录 | `build/`（已配置 `-DFANLX_BUILD_GUI=ON -DCMAKE_PREFIX_PATH=E:/Qt/6.11.1/mingw_64`） |

```powershell
# 基础版（不需要 Qt）
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -- -j4

# 带 GUI
cmake -S . -B build -G "MinGW Makefiles" -DFANLX_BUILD_GUI=ON -DCMAKE_PREFIX_PATH="E:/Qt/6.11.1/mingw_64"
cmake --build build -- -j4

# 关键：把 Qt 运行依赖部署到 build/，否则 GUI 程序与 gui_behavior 测试会因找不到 DLL 失败
powershell -File scripts/deploy-gui.ps1 -TargetDir build -QtBin "E:/Qt/6.11.1/mingw_64/bin" -MinGwBin "D:/mingw64/bin"

# 跑 GUI 相关命令前建议把 PATH 收敛到系统目录，验证"干净环境可运行"
$env:PATH = "C:\Windows\system32;C:\Windows"; $env:QT_QPA_PLATFORM = 'offscreen'
```

---

## 2 硬性约束（违反即污染仓库，务必遵守）

1. **不得修改原始任务书** `2026-面向对象程序课程设计.doc`。
2. 基础课设交付（数字菜单控制台）必须保持可用：`fanlx_app.exe`、`console_behavior`、`smoke` 不得回归。
3. 业务语义不得改：领域层（`include/account|friend|group`）不依赖 JSON/Win32/SQLite/Qt；
   `db/` 不依赖 Qt；`gui/` 不直接触碰 SQL 与 socket。
4. 每个类独立头文件 + 实现文件；自定义类统一 `FanLX` 后缀；中文注释保留。
5. 警告即错误：所有目标以 `-Wall -Wextra -Wpedantic -Werror` 编译（GUI 目标同样）。
6. **不得使用 `cmake --build` 之外的即席编译产物进仓库**；临时诊断程序放在 `build/`（已被 .gitignore 忽略），跑完删除。
7. **不得把未实测的条目写成完成**；`docs/progress.md` 必须区分"计划/实现/实际验证"。
8. 数据目录隔离：一个账号一个数据目录；服务端与各客户端目录必须不同。
9. 提交失败不得回成功 ACK；重复投递必须幂等；未提交结果不得发布给客户端。
10. 时区/时间语义：服务器时间由服务端提供，客户端不得伪造；旧 JSON 无可信时间时留空并标注"原始时间未知"。

---

## 3 当前真实状态（2026-09-26 实测）

### 3.1 通过项

| 验证 | 结果 | 命令 |
| --- | --- | --- |
| 全量 CTest（不含 perf） | **11/11 通过**（20.55 秒） | `ctest --test-dir build -E performance` |
| 十万条性能 | 写入 3732 ms；keyset 分页 p50 51.8 ms、**p95 73.6 ms**（目标 <200 ms）；关键词扫描 42 ms | `ctest --test-dir build -L perf` |
| GUI 干净目录启动 | 通过（PATH 仅系统目录时可运行，依赖 12 个文件） | `scripts/deploy-gui.ps1` 后运行 `fanlx_gui.exe` |
| 两客户端 SQLite 往返 A→B→A | 通过 | 见 4.3 复现步骤 |
| 图形层离屏测试 | `gui_behavior` 4 用例通过（含两客户端真实收发） | `ctest --test-dir build -R gui_behavior` |
| 界面截图渲染 | 3 张 PNG 已生成并通过"非白像素"自检 | `build\gui_screenshot.exe --out .artifacts\gui-screenshots` |

### 3.2 历史问题（已解决）

**三客户端 GUI 演示中，客户端"A 收到对端消息"这一步曾超时。**

复现（`tools/gui_screenshot.cpp`，隔离目录、自建数据、真起 SQLite 服务端）：

```powershell
$env:PATH = "C:\Windows\system32;C:\Windows"; $env:QT_QPA_PLATFORM = 'offscreen'
build\gui_screenshot.exe --out .artifacts\gui-screenshots
```

早期观察到的输出（现已修复）：

```
[ok]   三个客户端全部在线
[ok]   A 发送单聊消息
[ok]   B 回复单聊消息
[FAIL] 等待超时：A 收到回复
[ok]   A 发送群消息
[FAIL] 等待超时：群成员收到群消息
[ok]   C 在群里回复
[FAIL] 等待超时：A 收到群回复
[info] A 会话数=2 未读=2          ← 消息确实进了数据库
```

**修复结果**：控制器现在在 `Delivered`/`Accepted` 后自动请求当前会话最新历史；查看旧页时保留滚动位置并提示新消息。该场景已固化为 `gui_three_client_behavior` CTest。

已知强线索：
- 直接用 `ImClientFanLX`（不经过控制器）做两客户端往返**是通的**（4.3），所以问题在控制器侧。
- `gui_behavior` 的 t64 用例同样走控制器且**单向**收发通过；差异在于
  ①三客户端同时在线 ②发生了"回复"（即对同一会话的第二轮消息） ③断言要求 `messages()` 里出现该正文。
- 控制器用 30 ms 的 `QTimer` 调 `drainEventsFanLX()`；`gui_screenshot` 的 `pump` 只调
  `QCoreApplication::processEvents()`。**首要怀疑**：断言谓词读取的
  `controllers[i]->messages()` 依赖 `History` 事件回填，而该用例只调用了
  `sendText` / `openConversation`，没有在收到 `Delivered` 后重新发起历史查询或就地追加到模型。
  请先核对 `ChatControllerFanLX::drainEventsFanLX()` 的 `Delivered` 分支是否真的更新了
  `messagesFanLX`（当前实现是"标记 hasNewMessage + 发信号 + requestConversations()"，
  **并不把新消息插入 `messagesFanLX`**，这很可能就是断言不成立的原因）。

### 3.3 本轮已修复的缺陷（供接手者理解代码为何长这样，勿回退）

| # | 缺陷 | 后果 | 修复位置 |
| --- | --- | --- | --- |
| 1 | 客户端回 `ACK` 不带服务器分配的会话序号 | 服务端推不动投递游标，会话永久停在"在途"，**该会话后续消息全部发不出** | `src/network/ImClientFanLX.cpp`（两处 ACK 构造：SQLite 分支与旧 JSON 分支） |
| 2 | 服务端"投递记录缺失"直接失败 | 会话交接后 ACK 无处校验，消息被反复重发（曾观察到 SEND 重放到错误目标） | `src/network/ImServerFanLX.cpp` `handleAck`：登记缺失时按 `messageId` 从库回查会话键（`SqliteMessageStoreFanLX::findById`） |
| 3 | 待发送消息的收件人取 `conversations.target` | 该字段是建会话时写下的，**由对端先建会话时它等于对端** → **消息发给自己** | `SqliteChatRepositoryFanLX::pending()` 改为由 `conversationKey` 按方向推导；新增 `ImClientFanLX::submitSendToConversation`，GUI 控制器改用它与网络层共用同一推导点 |
| 4 | 服务端对 `conversationSeq == 0` 的 ACK 静默卡住 | 同一会话后续消息无法投递 | `ImServerFanLX::handleAck` 显式拒绝 |
| 5 | `AtomicFileWriterFanLX::publish` 固定发布为 `state.json` | 用它写 `client-identity.json` 会覆盖 IM 快照 → 重启后 `key 'schema' not found` | 新增 `persistence/NamedFilePublisherFanLX`（具名原子发布） |
| 6 | SQLite `INTEGER PRIMARY KEY` 上的显式 NULL 被换成 rowid | "待发送"消息伪装成已有服务器编号 | 客户端表改为 `device_sequence` 主键 + `message_id` 可空唯一部分索引 |
| 7 | 查询条件按需拼接导致 `?2..?7` 参数错位 | "按发送者过滤查不到历史" | `queryHistory` 改恒真占位写法（未启用时写 `?n = ''` / `?n = 0`） |
| 8 | 存储初始化放进工作线程后 `history()/pending()` 对 null 取键 | `json.type_error: cannot use at() with null` | 旧 JSON 后端恢复"构造即就绪"；SQLite 后端异步初始化 + 查询侧空值保护 |
| 9 | 连接层错误不上报 | 界面静默停在"重连中" | 新增去重的 `Connection` 事件，控制器转成 `errorFanLX` |
| 10 | `Sent` 事件携带空幂等键 | 发送路径拿不到 requestId 而返回失败 | 改用数据库事件回传的值 |
| 11 | `im_behavior` 手工构造 ACK 未设会话序号 | 该回归用例在新增校验后超时 | `tests/network_tests.cpp` 补齐 `ack.conversationSeq` |
| 12 | 截图工具窗口未 `show()` | `render()` 得到空白 PNG | `tools/gui_screenshot.cpp` 显式 `show()` + 多轮 `processEvents()` 后再渲染 |
| 13 | Qt `CMAKE_AUTOMOC` 在受限环境失败（libuv 子进程） | GUI 无法构建 | 改用 `qt6_wrap_cpp` |
| 14 | 混用 Qt 自带 MinGW 13 运行库与 GCC 15 编译代码 | 运行期堆损坏 `0xc0000374` | `scripts/deploy-gui.ps1` 只部署 Qt6 DLL + 平台插件 + 本项目工具链的运行库 |

---

## 4 复现与验证方法

### 4.1 全量测试

```powershell
ctest --test-dir build -E performance            # 期望 11/11
ctest --test-dir build -L perf                   # 十万条测量
```

### 4.2 三客户端 GUI 演示（当前已通过）

```powershell
$env:PATH = "C:\Windows\system32;C:\Windows"; $env:QT_QPA_PLATFORM = 'offscreen'
build\gui_screenshot.exe --out .artifacts\gui-screenshots
```

### 4.3 两客户端往返诊断（已通过；临时程序已删除，需要时重建）

用一个程序同时：后台线程 tick 服务端 + 前台构造两个 `ImClientFanLX`（SQLite 配置，构造函数第 4 个参数是登录密码，演示值现为 `123`（2026-10-10 前为 `pw-<QQ号>`）），
依次 `submitSend("2026002","first")` → 等 `a->pending()==0` 与 `b->history().size()==1`
→ `b->submitSend("2026001","reply")` → 期望 `b->pending()==0` 且 `a->history().size()==2`。
注意：客户端构造函数是异步的（存储在工作线程准备），必须等 `online()` 与 `databaseReady()`。
编译方式参考 `tools/gui_screenshot.cpp` 的链接参数。

### 4.4 真实手工三端联调（给人看的演示）

```powershell
.\build\fanlx_app.exe --data-dir data-im-server --init-demo
.\build\fanlx_app.exe --data-dir data-im-server --im-server --im-sqlite --im-host 127.0.0.1 --im-port 9500
.\build\fanlx_gui.exe --host 127.0.0.1 --port 9500 --account 2026001 --password 123
.\build\fanlx_gui.exe --host 127.0.0.1 --port 9500 --account 2026002 --password 123
.\build\fanlx_gui.exe --host 127.0.0.1 --port 9500 --account 2026003 --password 123
```

登录凭据是「用户名（QQ 号）+ 用户自选密码」，账号库即服务端 `state.json` 的 `users[]`，
演示数据密码现统一为 `123`（2026-10-10 前为 `pw-<QQ号>`）；`--im-tokens` / `--tokens` 与令牌文件已随 2026-09-30 改造删除。

群 1001 成员为 2026001/2026002/2026003，可直接演示三人群聊；好友关系为 2026001↔2026002、
2026001↔2026003（后一条由截图工具在隔离目录内动态补建，若要手工演示需先建好友）。
三个 GUI 客户端共用一个数据根（默认 `data-im-gui`）：存储目录按
`<根>/im-client/<host>_<端口>/<账号>` 派生，账号之间天然隔离，无需手工指定目录。
同一账号被第二个客户端打开仍会被目录独占锁拒绝（刻意隔离）。

---

## 5 剩余工作与优先级

| 优先级 | 任务 | 完成判据 |
| --- | --- | --- |
| P0 | 修复 3.2 的"数据库事件 → 界面模型"映射，并固化三客户端回归 | 已完成；`gui_screenshot.exe` 退出码 0，CTest 已纳入 |
| P1 | E05：时间范围过滤、跨会话分页、异步取消、关闭流程验收 | 已接入并通过数据库/GUI 回归，故障演示仍待补充 |
| P1 | E05：故障恢复演示（数据库提交失败、只读目录、空间不足注入） | 逐项有实测证据，未测项明确标注 |
| P2 | E06：UML 类图、扩展报告、演示数据、三客户端截图/录像记录 | 计划 §7 E06 验收出口 |
| P2 | 把 `docs/progress.md` 的"当前收尾状态"改为正式完成记录（仅在 P0/P1 全部通过后） | 文档与测试结果一致 |

### 明确不要做的事

- 不要为了通过测试而放宽断言（例如把"必须收到正文"改成"未读数增加"）。当前失败是真实缺陷。
- 不要回退 3.3 里的任何一项修复；如需改协议字段，必须同时更新客户端、服务端、测试与文档。
- 不要在未实测的情况下宣布 E04/E05/E06 完成——计划第 137 行明确：只启动线程、生成空数据库或导出静态网页不算完成。

---

## 6 关键文件索引

**计划与文档**：`docs/extension-plan.md`（计划）、`docs/project-structure.md`（结构与命令）、
`docs/progress.md`（进度/证据/未解决项）、`docs/network-validation.md`、`docs/data-format.md`、`docs/dependencies.md`

**扩展实现**：
- 数据库层：`include/db/`、`src/db/`（8+7 个文件）
- 网络层：`include/network/MessageMailboxFanLX.h`、`include/network/ImClientFanLX.h`、`include/network/ImServerFanLX.h` 及对应 `src/network/*.cpp`
- 图形层：`include/gui/`、`src/gui/`（5+6 个文件）
- 具名发布：`include/persistence/NamedFilePublisherFanLX.h`、`src/persistence/NamedFilePublisherFanLX.cpp`

**测试与工具**：`tests/chat_history_tests.cpp`（T20–T54，`--perf` 十万条）、
`tests/gui_tests.cpp`（图形层 4 用例）、`tests/network_tests.cpp --im`、
`tools/gui_screenshot.cpp`（三客户端 + 截图）、`scripts/deploy-gui.ps1`（Qt 部署）

**构建入口**：`CMakeLists.txt`（`FANLX_BUILD_GUI` 开关、`FANLX_SQLITE_DLL` 变量、`fanlx_chat` / `fanlx_gui_core` 目标）

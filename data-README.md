# 数据目录说明（data-*）

本文件回答一个问题：**哪个目录能删、哪个不能删。**

一句话原则：**看目录里有没有你「手动产生」的内容。**
`--init-demo` 造出来的演示数据都能重建；你自己聊出来的记录不能。

| 目录 | 内容 | 能删吗 | 怎么恢复 |
| --- | --- | --- | --- |
| `data/` | 仅 `.gitkeep`（空占位） | ✅ 能 | 无需恢复。它是控制台默认根（`src/main.cpp:92`），被 git 跟踪 |
| `data-im-a/` | 控制台客户端 A 的演示数据 | ⚠️ 能重建 | `build\fanlx_app.exe --data-dir data-im-a --init-demo` |
| `data-im-b/` | 控制台客户端 B 的演示数据 | ⚠️ 能重建 | 同上，换目录名 |
| `data-im-server/` | 服务端数据根（含消息库 `im-server/`） | ⚠️ 能重建 | `build\fanlx_app.exe --data-dir data-im-server --init-demo` |
| `data-im-gui/` | **GUI 客户端数据根（真实聊天记录）** | 🔴 **不能删** | **无法恢复**（`--init-demo` 只能造演示数据，造不回你的历史） |

## 重建演示数据

`--init-demo` **只能用于空目录**；若目录里已有快照会拒绝覆盖，需先删掉整个目录。

```powershell
# 在项目根目录执行
Remove-Item -Recurse -Force data-im-a, data-im-b, data-im-server
.\build\fanlx_app.exe --data-dir data-im-server --init-demo
.\build\fanlx_app.exe --data-dir data-im-a     --init-demo
.\build\fanlx_app.exe --data-dir data-im-b     --init-demo
```

期望输出：`Loaded users=12 generation=2`、退出码 0。

> `data-im-server` 重建后只有 3 个文件是正常的：SQLite 消息库
> （`im-server/server-chat.db`）在**服务端首次启动**时才创建。

## 删目录时的两个注意点

1. **整目录删，不要只删里面的单个文件。**
   SQLite 的 `-wal` / `-shm` 与主库是一体的；只删 `chat.db` 而留下 `-wal`
   会得到不一致的状态。（曾经 `server-chat.db` 只有 57 KB，而 `-wal` 有 737 KB。）
2. **`.gitignore` 的 `/data-*/` 只是「不让 git 跟踪」，不等于有备份。**
   这些目录**没有任何版本控制保护**。若在意 `data-im-gui` 里的记录，请自行定期复制到项目外：

   ```powershell
   Copy-Item -Recurse data-im-gui "D:\备份\data-im-gui-$(Get-Date -Format yyyyMMdd)"
   ```

## 目录结构与密码

客户端真实存储路径是**派生**的，不由你选：

```
<数据根>/im-client/<地址>_<端口>/<登录账号>/
    ├── domain/   领域库：state.json（账号/好友/群，含明文密码）
    └── <服务名>/  聊天库：QQ / Wechat / Weibo 各一套 chat.db
```

- **密码是明文的**（`include/account/UserProfileFanLX.h:27`），校验就是字符串相等
  （`AccountManagerFanLX.hpp:290`）。课程范围内不做哈希/加盐。
- **服务端与客户端各有一份账号库**：服务端看 `<服务端根>/state.json`，
  客户端看 `.../<账号>/domain/state.json`。**改密码要两边都改**，
  否则报「用户名或密码错误」（账号不存在与密码错误刻意用同一句话）。
- **演示账号密码**：**统一为 `123`**，由 `demoPasswordFanLX()`（`src/persistence/DemoSeedFanLX.cpp`）
  唯一生成。服务端与客户端用同一份规则，因此天然一致。详见 `HELP.md` 6.1 节。
  **自己注册的账号不受影响。**

## 相关文档

- `HELP.md` 4.2 节 —— 启动脚本与"默认数据根跟着当前工作目录"的坑
- `HELP.md` 4.5 节 —— `--data-root` 语义与 GUI 命令行参数
- `HELP.md` 6.1 节 —— 登录与密码规则
- `docs/storage-layout.md` —— 快照字段与服务商分层的完整结构

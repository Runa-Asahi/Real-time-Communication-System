# Ciallo 使用说明

面向对象课程设计 · 即时通讯客户端（原「樊陆旭模拟即时通讯系统」，现产品名 **Ciallo**）。

---

## 一、同设备使用（最简单）

1. 解压发布包（或本机构建后的 `dist/Ciallo`）。
2. **双击 `Ciallo.bat`**。
3. 脚本会：
   - 首次自动初始化演示数据；
   - 后台启动本机服务端（`127.0.0.1:9500`）；
   - 打开登录窗口。
4. 用演示账号登录：

| 微X号 | 密码 |
| --- | --- |
| `2026001` | `123` |
| `2026002` | `123` |
| `2026003` | `123` |

5. 想同时开第二个账号：再双击一次 `Ciallo.bat`，用另一个微X号登录即可（本机多开）。
6. 结束：运行 `Ciallo-Stop.bat`，或关掉所有 Ciallo 窗口后在任务管理器结束 `fanlx_app.exe`。

> 不要直接双击 `bin\Ciallo.exe`：数据目录会落在 `bin\` 下，容易和发布包根目录的数据搞混。请始终用 `Ciallo.bat`。

### 本机能做什么

- 聊天（好友单聊优先尝试本机直连，失败则经服务器转发）
- 账号与服务（开通 QQ / 微信 / 微博、登录、绑定、改密）
- 好友、群

---

## 二、同局域网 · 异端设备

**可行性：已支持。** 服务端可绑定 `0.0.0.0`，客户端填写对方局域网 IP 即可联机。好友单聊的点对点直连在局域网下需设置本机网卡 IP（启动器会自动处理）；直连失败时仍经服务器中转，聊天可用。

### 电脑 A（服务器）

1. 两台电脑连同一 Wi-Fi / 有线局域网。
2. 在 A 上双击 **`Ciallo-LAN-Server.bat`**。
3. 窗口会打印本机局域网地址，例如 `192.168.1.20`。记下它。
4. 若 Windows 防火墙弹窗，勾选「专用网络」并允许。

### 电脑 B（客户端）

1. 把同一份发布包拷到 B（或只拷贝解压后的文件夹）。
2. 双击 **`Ciallo-LAN-Join.bat`**，输入 A 显示的 IP。
3. 用不同的演示账号登录（例如 A 用 `2026001`，B 用 `2026002`）。

### 注意

| 情况 | 说明 |
| --- | --- |
| 连不上 | 确认同一网段、A 已启动、防火墙放行 `fanlx_app.exe` / `Ciallo.exe`、端口 `9500` |
| 能登录但直连提示失败 | 正常：会自动走服务器中转；聊天仍可用 |
| 跨公网 / 手机流量 | **不支持**（无 NAT 穿透）。仅限同一局域网 |
| 账号数据 | 服务端账号库在 A 的 `data\server`；两边注册的新账号都会写进 A |

---

## 三、从源码构建（Windows）

需要：CMake ≥ 3.20、MinGW、Qt 6、系统 `winsqlite3.dll`。

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DFANLX_BUILD_GUI=ON -DFANLX_BUILD_CLI=ON `
  -DCMAKE_PREFIX_PATH="E:/Qt/6.11.1/mingw_64"
cmake --build build -- -j4
powershell -File scripts/deploy-gui.ps1 -TargetDir build `
  -QtBin "E:/Qt/6.11.1/mingw_64/bin" -MinGwBin "D:/mingw64/bin"
powershell -File scripts/package-release.ps1 -BuildDir build `
  -QtBin "E:/Qt/6.11.1/mingw_64/bin" -MinGwBin "D:/mingw64/bin"
```

也可打 tag `v1.0.0` 推送，由 GitHub Actions（`.github/workflows/windows-release.yml`）自动构建并上传正式 Release。

本仓库网络与文件锁依赖 Winsock / Win32，**不能在 Linux 上直接产出 Windows 可执行文件**；请用上述本机构建或 CI。

---

## 四、界面与品牌

- 产品名：**Ciallo**（登录页、窗口标题、发布包）。
- 登录页使用应用图标，不再用「樊」字或个人姓名作品牌。
- 演示用户昵称仍可能是课设种子数据里的名字，与产品名无关。

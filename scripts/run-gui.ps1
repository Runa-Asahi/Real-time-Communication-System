param(
    # 位置参数顺序：先端口、再账号。
    # .bat 用 %* 原样转发，所以第一个位置参数必须是端口；否则
    # `run-gui.bat 9500` 会把 9500 绑到主机名上，服务端报「IM 端口非法」。
    [Parameter(Position = 0)][string]$Port = '9500',
    [Parameter(Position = 1)][string]$Account = '2026001',
    [string]$BindHost = '127.0.0.1',
    [string]$Password = '123',
    [string]$DataRoot = 'data-im-gui',
    [switch]$NoServer,
    [switch]$AutoLogin,
    [switch]$NoPause
)

# 启动 GUI 客户端（可选同时拉起本地服务端）。
#
# 为什么要这个脚本：GUI 的数据根默认是「当前工作目录」下的 data-im-gui
# （src/gui/main_gui.cpp）。在资源管理器里双击 build\fanlx_gui.exe 时当前目录是
# build\，于是会去读 build\data-im-gui —— 那是**另一套**数据，密码与项目根的
# data-im-gui 不一致，看起来就像"密码错了"。本脚本先切到项目根再启动，
# 并在需要时显式传 --data-root，消除这个歧义。

$ErrorActionPreference = 'Stop'

try { [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false) } catch { }

# 切到项目根（本脚本所在目录的上一级），与 run-im-server.ps1 保持一致。
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $guiExe = Join-Path $root 'build\fanlx_gui.exe'
    $srvExe = Join-Path $root 'build\fanlx_app.exe'
    $logDir = Join-Path $root 'build\tmp'
    if (-not (Test-Path -LiteralPath $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }

    function Wait-BeforeClose {
        if ($NoPause) { return }
        Write-Host ''
        Write-Host '按任意键关闭本窗口...'
        try { $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown') }
        catch { Start-Sleep -Seconds 2 }
    }

    # ---- 前置检查：GUI 可执行文件 ----
    if (-not (Test-Path -LiteralPath $guiExe -PathType Leaf)) {
        Write-Host "[错误] 找不到 build\fanlx_gui.exe"
        Write-Host ''
        Write-Host '        请先构建可视化版（需要 Qt 6）：'
        Write-Host '            cmake -S . -B build -G "MinGW Makefiles" -DFANLX_BUILD_GUI=ON -DCMAKE_PREFIX_PATH="E:/Qt/6.11.1/mingw_64"'
        Write-Host '            cmake --build build -- -j4'
        Write-Host '            powershell -File scripts/deploy-gui.ps1 -TargetDir build -QtBin "E:/Qt/6.11.1/mingw_64/bin" -MinGwBin "D:/mingw64/bin"'
        Wait-BeforeClose
        exit 1
    }

    # ---- 数据根：默认用项目根下的 data-im-gui ----
    $dataRootAbs = if ([System.IO.Path]::IsPathRooted($DataRoot)) { $DataRoot }
                   else { Join-Path $root $DataRoot }
    if (-not (Test-Path -LiteralPath $dataRootAbs)) {
        Write-Host "[提示] 数据根不存在，将在首次登录时创建：$DataRoot"
    }

    # 提醒：项目根与 build\ 下各有一份 data-im-gui，很容易搞混。
    $shadow = Join-Path $root 'build\data-im-gui'
    if ((Test-Path -LiteralPath $shadow) -and -not [System.IO.Path]::IsPathRooted($DataRoot)) {
        Write-Host "[提示] 检测到另有 build\data-im-gui 存在——那是双击 exe 时才会用到的另一套数据。"
        Write-Host "       本脚本使用：$DataRoot（密码可能不同，别混用）。"
        Write-Host ''
    }

    $portNum = 0
    if (-not [int]::TryParse($Port, [ref]$portNum)) {
        Write-Host "[错误] 端口不是数字：$Port"
        Wait-BeforeClose
        exit 1
    }

    # ---- 可选：先把服务端拉起来（GUI 登录需要它在线） ----
    $serverProc = $null
    if (-not $NoServer) {
        $listening = $false
        try {
            $client = New-Object System.Net.Sockets.TcpClient
            $listening = $client.ConnectAsync($BindHost, $portNum).Wait(800)
            $client.Close()
        } catch { $listening = $false }

        if ($listening) {
            # 端口上已有进程时不能盲用：另一份工程的服务端会占用同一个端口和同一个实例号，
            # 但 messageId 从分叉点起对不上，客户端就会报「messageId N 内容冲突」并且再也发不出去。
            $listenerCommand = $null
            $listenMatch = netstat -ano | Select-String -Pattern ":$portNum\s+\S+\s+LISTENING\s+(\d+)" |
                Select-Object -First 1
            if ($listenMatch -and $listenMatch.Matches.Count -gt 0) {
                $listenPid = [int]$listenMatch.Matches[0].Groups[1].Value
                $listener = Get-CimInstance Win32_Process -Filter "ProcessId=$listenPid" -ErrorAction SilentlyContinue
                if ($listener) { $listenerCommand = $listener.CommandLine }
            }
            $ownDataDir = Join-Path $root 'data-im-server'
            $usesThisServer = $false
            if ($listenerCommand) {
                $normalized = ($listenerCommand -replace '/', '\').ToLowerInvariant()
                $usesThisServer = $normalized.Contains($ownDataDir.ToLowerInvariant())
            }
            if ($listenerCommand -and -not $usesThisServer) {
                Write-Host "[错误] $BindHost`:$Port 上的服务端不是本工程的数据目录。"
                Write-Host "       它的命令行是："
                Write-Host "       $listenerCommand"
                Write-Host "       本工程客户端的聊天库与那份服务端记录已经分叉，连上去会报 messageId 内容冲突，消息发不出去。"
                Write-Host "       请先关掉那个进程，再重新运行本脚本。本脚本会启动："
                Write-Host "       $srvExe --data-dir $ownDataDir"
                Wait-BeforeClose
                exit 1
            }
            Write-Host "[信息] $BindHost`:$Port 已有服务端在监听，直接使用。"
        } elseif (-not (Test-Path -LiteralPath $srvExe -PathType Leaf)) {
            Write-Host "[警告] 找不到 build\fanlx_app.exe，无法自动启动服务端。"
            Write-Host "       若没有服务端，登录会失败（用户名或密码错误）。"
            Write-Host "       请用 -DFANLX_BUILD_CLI=ON 重新构建，或加 -NoServer 跳过本提示。"
            Write-Host ''
        } else {
            Write-Host "[信息] 正在启动本地服务端 $BindHost`:$Port ..."
            $srvArgs = @('--data-dir', (Join-Path $root 'data-im-server'),
                         '--im-server', '--im-host', $BindHost, '--im-port', $Port, '--im-sqlite')
            $serverProc = Start-Process -FilePath $srvExe -ArgumentList $srvArgs -PassThru `
                -WindowStyle Hidden `
                -RedirectStandardOutput (Join-Path $logDir 'gui-launcher-server.out') `
                -RedirectStandardError  (Join-Path $logDir 'gui-launcher-server.err')
            # 等服务端真正开始监听（最多 15 秒）。
            $ready = $false
            for ($i = 0; $i -lt 30; $i++) {
                Start-Sleep -Milliseconds 500
                try {
                    $c = New-Object System.Net.Sockets.TcpClient
                    $ready = $c.ConnectAsync($BindHost, $portNum).Wait(500)
                    $c.Close()
                } catch { $ready = $false }
                if ($ready) { break }
            }
            if ($ready) {
                Write-Host "[信息] 服务端已就绪（pid $($serverProc.Id)）。"
            } else {
                Write-Host "[警告] 服务端未在 15 秒内就绪，日志见 build\tmp\gui-launcher-server.err"
            }
        }
    }

    # ---- 启动 GUI：显式 --data-root，避免依赖当前工作目录 ----
    $guiArgs = @('--host', $BindHost, '--port', $Port, '--account', $Account, '--data-root', $dataRootAbs)
    if ($Password) { $guiArgs += @('--password', $Password) }
    if ($AutoLogin) { $guiArgs += '--auto-login' }

    Write-Host "[信息] 启动 GUI：账号 $Account（密码由登录窗口输入）"
    if ($AutoLogin) { Write-Host "[信息] 已请求自动登录（--auto-login）" }
    Write-Host "[信息] 数据根：$dataRootAbs"
    Write-Host ''

    # GUI 是控制台子系统程序。不指定 -WindowStyle 时它会继承本脚本的控制台，
    # 父窗口关掉或被藏起来，GUI 会一起被销毁。
    # -WindowStyle Normal 会按 SW_SHOWNORMAL 再弹出一个空的命令行（每登录一个账号一个），
    # 程序里的 ShowWindow(SW_HIDE) 抢不过这次显示。Hidden 仍然是独立控制台，只是从一开始就隐藏；
    # fanlx_gui 随后 FreeConsole，空窗口不会留下来，Qt 窗口不受影响。
    $gui = Start-Process -FilePath $guiExe -ArgumentList $guiArgs -PassThru -WindowStyle Hidden
    Write-Host "[信息] GUI 已启动（pid $($gui.Id)）。关闭它的窗口即退出。"

    # 等 GUI 退出后收尾服务端（仅当我们自己启动的那个）。
    if ($null -ne $serverProc) {
        Write-Host "[信息] 本窗口会等待 GUI 退出，然后关闭它拉起的服务端。"
        try { $gui.WaitForExit() } catch { }
        if (-not $serverProc.HasExited) {
            Write-Host "[信息] 正在关闭服务端（pid $($serverProc.Id)）..."
            try { Stop-Process -Id $serverProc.Id -Force } catch { }
        }
        Write-Host "[信息] 已退出。"
    }
} finally {
    Pop-Location
}

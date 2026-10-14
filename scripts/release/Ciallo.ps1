param(
    [ValidateSet('local', 'lan-server', 'lan-client')][string]$Mode = 'local',
    [string]$ServerHost = '',
    [string]$Port = '9500'
)
# Ciallo 发布包启动器。由同目录的 Ciallo*.bat 调用（bat 只做 chcp 与转发，保持纯 ASCII）。
#   local       本机：自动初始化并后台启动服务端（127.0.0.1），再打开客户端
#   lan-server  局域网主机：服务端监听 0.0.0.0，本机同时打开一个客户端，并显示本机局域网 IP
#   lan-client  局域网其它电脑：只打开客户端，连接输入的服务器 IP
$ErrorActionPreference = 'Stop'
try { [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false) } catch { }

function Pause-Exit([int]$code) {
    Write-Host ''
    Write-Host '按任意键关闭本窗口...'
    try { $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown') } catch { Start-Sleep -Seconds 3 }
    exit $code
}

$root = $PSScriptRoot
$bin = Join-Path $root 'bin'
$guiExe = Join-Path $bin 'Ciallo.exe'
$srvExe = Join-Path $bin 'fanlx_app.exe'
# 发布包里可执行文件在同级 bin\。源码树里的 scripts\release 没有这份 bin，
# 此时改用仓库 build\ 下的 fanlx_gui.exe / fanlx_app.exe（部署 Qt 运行库之后即可启动）。
if (-not (Test-Path -LiteralPath $guiExe)) {
    $repoRoot = Split-Path -Parent (Split-Path -Parent $root)
    $devGui = Join-Path $repoRoot 'build\fanlx_gui.exe'
    $devSrv = Join-Path $repoRoot 'build\fanlx_app.exe'
    $inRepo = Test-Path -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt')
    if ($inRepo -and (Test-Path -LiteralPath $devGui) -and (Test-Path -LiteralPath $devSrv)) {
        $guiExe = $devGui
        $srvExe = $devSrv
        Write-Host '[信息] 当前是源码目录，使用 build\fanlx_gui.exe 与 build\fanlx_app.exe。'
    } elseif ($inRepo) {
        Write-Host '[错误] 找不到发布包里的 bin\Ciallo.exe，也找不到 build\fanlx_gui.exe / build\fanlx_app.exe。'
        Write-Host '        请先在仓库根目录构建并部署 Qt 运行库：'
        Write-Host '            cmake -S . -B build -G "MinGW Makefiles" -DFANLX_BUILD_GUI=ON -DFANLX_BUILD_CLI=ON -DCMAKE_PREFIX_PATH="E:/Qt/6.11.1/mingw_64"'
        Write-Host '            cmake --build build --target fanlx_gui --target fanlx_app --parallel 4'
        Write-Host '            powershell -File scripts/deploy-gui.ps1 -TargetDir build -QtBin "E:/Qt/6.11.1/mingw_64/bin" -MinGwBin "D:/mingw64/bin"'
        Pause-Exit 1
    }
}
$serverData = Join-Path $root 'data\server'
$clientData = Join-Path $root 'data\client'
$logDir = Join-Path $root 'data\logs'
$savedHost = Join-Path $root 'data\server-host.txt'
foreach ($d in @($serverData, $clientData, $logDir)) {
    if (-not (Test-Path -LiteralPath $d)) { New-Item -ItemType Directory -Force -Path $d | Out-Null }
}

function Test-Port([string]$h, [int]$p) {
    try {
        $c = New-Object System.Net.Sockets.TcpClient
        $ok = $c.ConnectAsync($h, $p).Wait(800)
        $c.Close()
        return $ok
    } catch { return $false }
}

# 本机可用的局域网 IPv4（排除回环、169.254 自动地址）。
function Get-LanAddresses {
    $list = @()
    try {
        $list = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop |
            Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' } |
            Sort-Object -Property InterfaceMetric | Select-Object -ExpandProperty IPAddress
    } catch {
        $list = [System.Net.Dns]::GetHostAddresses([System.Net.Dns]::GetHostName()) |
            Where-Object { $_.AddressFamily -eq 'InterNetwork' -and -not $_.ToString().StartsWith('127.') } |
            ForEach-Object { $_.ToString() }
    }
    return @($list)
}

# 与服务器处在同一网段的本机地址，用作点对点直连的宣告地址（FANLX_P2P_BIND）。
function Select-P2pAddress([string]$serverIp) {
    $mine = Get-LanAddresses
    if ($mine.Count -eq 0) { return '' }
    $prefix = ($serverIp -split '\.')[0..2] -join '.'
    $same = $mine | Where-Object { $_.StartsWith($prefix + '.') } | Select-Object -First 1
    if ($same) { return $same }
    return $mine[0]
}

function Start-Server([string]$bindHost) {
    if (-not (Test-Path -LiteralPath (Join-Path $serverData 'state.json'))) {
        Write-Host '[信息] 首次运行：初始化演示数据（账号 2026001 / 2026002 / 2026003，密码 123）'
        & $srvExe --data-dir $serverData --init-demo | Out-Null
        if ($LASTEXITCODE -ne 0) { Write-Host '[错误] 初始化服务端数据失败'; Pause-Exit 1 }
    }
    $probe = if ($bindHost -eq '0.0.0.0') { '127.0.0.1' } else { $bindHost }
    if (Test-Port $probe ([int]$Port)) {
        Write-Host "[信息] 端口 $Port 已有服务端在运行，直接使用。"
        return
    }
    Write-Host "[信息] 启动服务端 $bindHost`:$Port ..."
    $srvArgs = @('--data-dir', $serverData, '--im-server', '--im-host', $bindHost, '--im-port', $Port, '--im-sqlite')
    $proc = Start-Process -FilePath $srvExe -ArgumentList $srvArgs -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $logDir 'server.out.log') `
        -RedirectStandardError (Join-Path $logDir 'server.err.log')
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 500
        if (Test-Port $probe ([int]$Port)) {
            Write-Host "[信息] 服务端已就绪（pid $($proc.Id)）。关闭方法：任务管理器结束 fanlx_app.exe，或运行 Ciallo-Stop.bat。"
            return
        }
    }
    Write-Host '[错误] 服务端 15 秒内未就绪，日志：data\logs\server.err.log'
    Pause-Exit 1
}

function Start-Client([string]$serverIp) {
    $guiArgs = @('--host', $serverIp, '--port', $Port, '--data-root', $clientData)
    Start-Process -FilePath $guiExe -ArgumentList $guiArgs -WorkingDirectory $root | Out-Null
}

if (-not (Test-Path -LiteralPath $guiExe)) { Write-Host "[错误] 找不到 $guiExe"; Pause-Exit 1 }

switch ($Mode) {
    'local' {
        Start-Server '127.0.0.1'
        Start-Client '127.0.0.1'
        Write-Host '[完成] 已打开 Ciallo。想同时登录第二个账号，再双击一次 Ciallo.bat 即可。'
        Start-Sleep -Seconds 2
    }
    'lan-server' {
        $ips = Get-LanAddresses
        if ($ips.Count -eq 0) { Write-Host '[错误] 没有找到局域网 IPv4 地址，请先连上路由器 / 热点。'; Pause-Exit 1 }
        Start-Server '0.0.0.0'
        $hostIp = $ips[0]
        $env:FANLX_P2P_BIND = $hostIp
        Start-Client $hostIp
        Write-Host ''
        Write-Host '================ 局域网服务器已启动 ================'
        Write-Host ' 在同一局域网的其它电脑上双击 Ciallo-LAN-Join.bat，输入下面任一地址：'
        foreach ($ip in $ips) { Write-Host "     $ip" }
        Write-Host " 端口：$Port"
        Write-Host ' 若对方连不上：Windows 防火墙弹窗时请勾选「专用网络」并允许 fanlx_app.exe / Ciallo.exe。'
        Write-Host '=================================================='
        Pause-Exit 0
    }
    'lan-client' {
        $default = ''
        if (Test-Path -LiteralPath $savedHost) { $default = (Get-Content -LiteralPath $savedHost -Raw).Trim() }
        $serverIp = $ServerHost
        if (-not $serverIp) {
            $prompt = if ($default) { "请输入服务器 IP（回车使用上次的 $default）" } else { '请输入服务器 IP（服务器窗口里显示的地址，如 192.168.1.20）' }
            $serverIp = (Read-Host $prompt).Trim()
            if (-not $serverIp) { $serverIp = $default }
        }
        if ($serverIp -notmatch '^\d{1,3}(\.\d{1,3}){3}$') { Write-Host "[错误] 不是合法的 IPv4 地址：$serverIp"; Pause-Exit 1 }
        Set-Content -LiteralPath $savedHost -Value $serverIp -Encoding UTF8
        if (-not (Test-Port $serverIp ([int]$Port))) {
            Write-Host "[错误] 连不上 $serverIp`:$Port。请确认：1) 服务器电脑已运行 Ciallo-LAN-Server.bat；2) 两台电脑在同一局域网；3) 服务器防火墙已放行。"
            Pause-Exit 1
        }
        $p2p = Select-P2pAddress $serverIp
        if ($p2p) { $env:FANLX_P2P_BIND = $p2p }
        Start-Client $serverIp
        Write-Host "[完成] 已连接 $serverIp`:$Port 并打开 Ciallo。"
        Start-Sleep -Seconds 2
    }
}

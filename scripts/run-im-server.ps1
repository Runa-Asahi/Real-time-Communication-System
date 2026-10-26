<#
    run-im-server.ps1 -- 启动 Ciallo 即时通讯系统的中心转发服务端（前台运行，Ctrl+C 停止）

    为什么启动逻辑放在 .ps1 而不是 .bat（重要，别改回去）：
      cmd.exe 解析批处理文件时用的是「控制台当前代码页」，而且它记录读取位置的方式会在
      代码页切换 / 多字节汉字被切开时错位。于是「带中文的 .bat + 脚本里 chcp」会把中文行的
      后半截当成命令执行，报出 'rver.bat' / '置' / '系统找不到指定的路径' 之类莫名其妙的错误
      （在 936 控制台下必现，65001 控制台下不出现，所以看上去时好时坏）。
      本机 cmd 默认代码页是 936，所以：
        - run-im-server.bat 只保留 ASCII 字符，只负责 chcp + 转发到本文件；
        - 中文提示、前置检查、启动参数全部放在本文件（UTF-8 with BOM，PowerShell 5.1 需要 BOM 才能正确读中文）。
      注意：改动本文件请保持 UTF-8 with BOM，否则 PowerShell 5.1 会按 GBK 读，中文提示会变乱码。

    认证：用户用「微X号 + 自选密码」登录，服务端直接查领域账号库
      （<DataDir>\state.json 里的 users[].password），因此启动时不需要任何令牌 / 口令文件。
      微X 是 QQ / 微信 / 微博 的上层登录身份；聊天通道按"当前服务"认证
      （QQ / 微信 / 微博 都能单聊，只有 QQ 与微信有群）。

    旧数据目录：脚本启动前会检查快照格式。空目录直接初始化；schemaVersion=1 或缺少
      users[].password 的历史库会被**改名保留**（<数据目录>.legacy-<时间戳>）后重新初始化，
      因此升级后直接双击本脚本即可，不需要手工删目录。

    用法：
        scripts\run-im-server.bat                  用下面的默认 IP 与端口
        scripts\run-im-server.bat 9600             只改端口
        scripts\run-im-server.bat 9600 0.0.0.0     同时改端口与 IP
        powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run-im-server.ps1 9600

    可选开关：-Port -BindHost -DataDir -Exe -UseSqlite 0|1 -DryRun -NoPause
    环境变量：FANLX_DRY_RUN=1 只打印最终命令行不真的启动；FANLX_NO_PAUSE=1 失败时不等待按键。
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)][string]$Port,
    [Parameter(Position = 1)][string]$BindHost,
    [string]$DataDir,
    [string]$Exe,
    [string]$UseSqlite,
    [switch]$DryRun,
    [switch]$NoPause
)

$ErrorActionPreference = 'Stop'

# 输出统一成 UTF-8：控制台直接显示中文；重定向到文件/管道时也是 UTF-8，
# 与 fanlx_app.exe 自己的输出编码保持一致，日志里不会出现两种编码混排。
try { [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false) } catch { }

# ===== 默认配置：要改就改这里 ==============================================
$defaultBindHost = '127.0.0.1'
$defaultPort     = '9500'
$defaultDataDir  = 'data-im-server'
$defaultExe      = 'build\fanlx_app.exe'
# 1 = 用 SQLite 消息库(im-server\server-chat.db)；0 = 用旧 JSON 日志
$defaultUseSqlite = '1'
# ==========================================================================

$separator = '=' * 76

function Get-Setting {
    param([string]$Value, [string]$EnvName, [string]$Fallback)
    if ($Value) { return $Value }
    $fromEnv = [Environment]::GetEnvironmentVariable($EnvName)
    if ($fromEnv) { return $fromEnv }
    return $Fallback
}

function Wait-BeforeClose {
    if ($NoPause) { return }
    Write-Host ''
    Write-Host '按任意键关闭本窗口...'
    try { $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown') }
    catch { Start-Sleep -Seconds 2 }   # 输入被重定向时无法读键，退化成短暂等待
}

$bindHost   = Get-Setting $BindHost 'FANLX_HOST'     $defaultBindHost
$port       = Get-Setting $Port     'FANLX_PORT'     $defaultPort
$dataDir    = Get-Setting $DataDir  'FANLX_DATA_DIR' $defaultDataDir
$exeRel     = Get-Setting $Exe      'FANLX_EXE'      $defaultExe
$useSqlite  = Get-Setting $UseSqlite 'FANLX_USE_SQLITE' $defaultUseSqlite
if (-not $DryRun)   { $DryRun   = ($env:FANLX_DRY_RUN  -eq '1') }
if (-not $NoPause)  { $NoPause  = ($env:FANLX_NO_PAUSE -eq '1') }

# 切到项目根目录（本脚本所在目录的上一级），这样 build\ 与 data-* 的相对路径都成立
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $exePath = Join-Path $root $exeRel

    Write-Host $separator
    Write-Host ' Ciallo 即时通讯系统 - 中心转发服务端'
    Write-Host $separator
    Write-Host ''

    # ---- 前置检查 1：可执行文件 ----
    if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
        Write-Host "[错误] 找不到 $exeRel"
        Write-Host ''
        Write-Host '        请先在项目根目录完成构建：'
        Write-Host '            cmake -S . -B build -G "MinGW Makefiles"'
        Write-Host '            cmake --build build -- -j4'
        Write-Host ''
        Write-Host '        如果构建目录不是 build，请改本文件顶部的 $defaultExe（或传 -Exe）。'
        Wait-BeforeClose
        exit 1
    }

    # ---- 前置检查 2：数据目录必须已初始化、且快照版本受支持 ----
    # 服务端只在「已加载」的领域状态上工作：好友/群授权与登录密码都来自它。
    # 两代历史快照必须先重建，否则服务端会以明确的错误退出：
    #   · 缺 users[].password（凭据改造之前）；
    #   · schemaVersion=1（好友与群还是扁平数组，本版只读写 schemaVersion=2）。
    $statePath = Join-Path $root (Join-Path $dataDir 'state.json')
    $stateText = ''
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $stateText = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8
        } catch {
            $stateText = ''
        }
    }
    $needsRebuild = $false
    $rebuildReason = ''
    if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) {
        Write-Host "[提示] $dataDir 还没有领域快照，正在初始化演示数据..."
        $needsRebuild = $true
        $rebuildReason = '空目录'
    } elseif ($stateText -match '"schemaVersion"\s*:\s*1\b') {
        $needsRebuild = $true
        $rebuildReason = '快照是 schemaVersion=1（好友与群还是扁平数组，本版只读写 schemaVersion=2）'
    } elseif ($stateText -notmatch '"password"') {
        $needsRebuild = $true
        $rebuildReason = '快照里的用户没有登录密码字段（凭据改造之前的旧库）'
    }
    if ($needsRebuild) {
        if ($DryRun) {
            # 干跑只报告计划，不改动任何目录（-DryRun 的语义就是"没有副作用"）。
            Write-Host "[干跑] 需要重建数据目录：$rebuildReason"
            Write-Host "[干跑] 本应改名并初始化：$dataDir"
            Write-Host ''
        } else {
            if ($rebuildReason -ne '空目录') {
                # 旧库必须先让位：程序拒绝就地升级，这里把旧目录改名保留（不删除证据），
                # 再按当前格式重新初始化。改名后缀带时间戳，重复运行不会互相覆盖。
                $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
                $backup = "$dataDir.legacy-$stamp"
                Write-Host "[提示] $rebuildReason"
                Write-Host "       把旧目录改名为 $backup（保留证据，不删除），再按当前格式重新初始化..."
                try {
                    Move-Item -LiteralPath (Join-Path $root $dataDir) -Destination (Join-Path $root $backup) -Force
                } catch {
                    Write-Host "[错误] 旧目录改名失败：$($_.Exception.Message)"
                    Write-Host '        请手工把它移走（或删除）后重新运行本脚本。'
                    Wait-BeforeClose
                    exit 1
                }
            }
            & $exePath --data-dir $dataDir --init-demo
            if ($LASTEXITCODE -ne 0) {
                Write-Host '[错误] 初始化失败，请查看上面的输出。'
                Wait-BeforeClose
                exit 1
            }
            Write-Host '[提示] 初始化完成。'
            Write-Host ''
        }
    }

    # ---- 组装可选参数与可读说明 ----
    $sqliteArg = @()
    $storageDesc = "旧 JSON 日志  ($dataDir\im-server\state.json)"
    if ($useSqlite -eq '1') {
        $sqliteArg = @('--im-sqlite')
        $storageDesc = "SQLite 消息库  ($dataDir\im-server\server-chat.db)"
    }

    Write-Host "   监听地址    ${bindHost}:${port}"
    Write-Host "   数据目录    $dataDir"
    Write-Host "   账号库      $dataDir\state.json（users[].password，明文；服务端每次认证前读穿它）"
    Write-Host "   消息存储    $storageDesc"
    Write-Host "   实例标识    $dataDir\im-server\server-identity.json"
    Write-Host ''
    Write-Host '   停止：在本窗口按 Ctrl+C'
    Write-Host '   说明：停止后同一数据目录可直接重启续用，无需重新初始化。'
    Write-Host '         客户端新注册的微X号会通过 IM 的 REGISTER 直接写进上面这份账号库，'
    Write-Host '         因此**注册之后不需要重启本服务端**（停掉再开也照样能登录）。'
    Write-Host '         客户端登录用「微X号 + 密码」：微X 是 QQ / 微信 / 微博 的上层登录身份，密码注册时自选。'
    Write-Host '         演示数据的密码规则是 pw-<微X号>，例如微X号 2026001 的密码是 pw-2026001。'
    Write-Host '         聊天通道按"当前服务"建连：QQ / 微信 / 微博 都能单聊，只有 QQ 与微信有群。'
    Write-Host "          客户端按 <数据根>/im-client/${bindHost}_${port}/<服务账号>/<服务名>/ 派生聊天目录，"
    Write-Host '          领域库在同层的 domain 子目录。'
    Write-Host $separator
    Write-Host ''

    $launchArgs = @('--data-dir', $dataDir, '--im-server', '--im-host', $bindHost,
                    '--im-port', $port) + $sqliteArg
    $commandText = "`"$exeRel`" --data-dir `"$dataDir`" --im-server --im-host $bindHost " +
                   "--im-port $port " + ($sqliteArg -join ' ')

    # FANLX_DRY_RUN=1：只打印最终命令行，不真的启动服务端（排错用，不会阻塞）
    if ($DryRun) {
        Write-Host '[干跑] 本应执行：'
        Write-Host "       $commandText"
        Write-Host '[干跑] 未启动服务端。'
        exit 0
    }

    & $exePath @launchArgs
    $exitCode = $LASTEXITCODE
    Write-Host ''

    # Ctrl+C 终止时 Windows 给出的退出码，按「正常停止」处理
    if (@(-1073741510, 3221225786, 130) -contains $exitCode) {
        Write-Host $separator
        Write-Host ' 服务端已停止（用户按了 Ctrl+C）。数据已落盘，可直接重新运行本脚本继续。'
        Write-Host $separator
        exit 0
    }

    if ($exitCode -ne 0) {
        Write-Host $separator
        Write-Host "[错误] 服务端异常退出，退出代码 $exitCode"
        Write-Host $separator
        Write-Host '   常见原因：'
        Write-Host '     1) 领域快照是旧格式，本脚本未能自动重建（例如目录被占用、只读）：'
        Write-Host '        · 缺 password：用户缺少登录密码（旧数据请重新 --init-demo 初始化）'
        Write-Host '        · schemaVersion=1：未知 schemaVersion：本版本读写 schemaVersion=2'
        Write-Host '        处理：把该数据目录改名后重新运行本脚本，或手工执行'
        Write-Host "              $exeRel --data-dir $dataDir --init-demo"
        Write-Host "     2) 端口 $port 已被其它进程占用"
        Write-Host "     3) 数据目录被另一个进程占用（$dataDir\.lock）"
        Write-Host '     4) 领域快照不是合法 UTF-8 JSON（可先用 --check 看加载结论）'
        Write-Host '   对照排查见 HELP.md 第 6.10 节「出错对照表」。'
        Wait-BeforeClose
        exit 1
    }

    Write-Host $separator
    Write-Host ' 服务端已正常停止。数据已落盘，可直接重新运行本脚本继续。'
    Write-Host $separator
    exit 0
}
finally {
    Pop-Location
}

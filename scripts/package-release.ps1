# package-release.ps1 -- 把已构建的 fanlx_gui / fanlx_app 打成可分发的 Ciallo 文件夹。
# 用法（在项目根）：
#   powershell -File scripts/package-release.ps1 -BuildDir build -QtBin "E:/Qt/6.11.1/mingw_64/bin" -MinGwBin "D:/mingw64/bin"
param(
    [string]$BuildDir = 'build',
    [string]$OutDir = 'dist/Ciallo',
    [string]$QtBin = '',
    [string]$MinGwBin = '',
    [string]$Version = '1.0.0'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $root $BuildDir }
$out = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $root $OutDir }
$bin = Join-Path $out 'bin'

if (-not (Test-Path (Join-Path $build 'fanlx_gui.exe'))) {
    throw "找不到 $build\fanlx_gui.exe，请先用 -DFANLX_BUILD_GUI=ON 构建"
}
if (-not (Test-Path (Join-Path $build 'fanlx_app.exe'))) {
    throw "找不到 $build\fanlx_app.exe，请先用 -DFANLX_BUILD_CLI=ON 构建（服务端在这个可执行文件里）"
}

if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force -Path $bin | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $out 'data') | Out-Null

Copy-Item (Join-Path $build 'fanlx_gui.exe') (Join-Path $bin 'Ciallo.exe') -Force
Copy-Item (Join-Path $build 'fanlx_app.exe') (Join-Path $bin 'fanlx_app.exe') -Force

# 拷贝 Qt / MinGW 运行时到 bin（与 deploy-gui.ps1 同一套规则）
& (Join-Path $PSScriptRoot 'deploy-gui.ps1') -TargetDir $bin -QtBin $QtBin -MinGwBin $MinGwBin

# 启动器（同机 / 局域网）。只拷贝脚本文件，避免把源码树里试运行生成的 data\ 打进发布包。
Get-ChildItem -LiteralPath (Join-Path $root 'scripts\release') -File |
    Copy-Item -Destination $out -Force
Copy-Item (Join-Path $root 'docs\usage\CIALLO使用说明.md') (Join-Path $out '使用说明.md') -Force
Copy-Item (Join-Path $root 'resources\icons\ciallo.ico') (Join-Path $out 'Ciallo.ico') -Force -ErrorAction SilentlyContinue

# 简短 README
@"
# Ciallo $Version

双击 **Ciallo.bat** 即可在本机使用。
局域网请看 **使用说明.md**，或直接用 **Ciallo-LAN-Server.bat** / **Ciallo-LAN-Join.bat**。

演示账号：2026001 / 2026002 / 2026003，密码均为 123。
"@ | Set-Content -LiteralPath (Join-Path $out 'README.txt') -Encoding UTF8

Write-Host "已生成发布目录：$out"
Write-Host "可压缩为 Ciallo-$Version-windows-x64.zip 后上传到 GitHub Release。"

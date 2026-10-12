# Deploy Qt runtime dependencies for fanlx_gui into a target directory so that a clean
# directory can be started without a development environment on PATH.
#
# Why not windeployqt directly: windeployqt also copies Qt's bundled MinGW runtime
# (libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll). This project is built
# with MinGW GCC 15.1.0 and links the runtime statically; mixing in Qt's GCC 13 runtime
# causes heap corruption at run time (observed as 0xc0000374). So this script copies only
# Qt6*.dll and the platform plugin - the C++ runtime comes from the static link.
#
# Purpose: copy Qt6*.dll and the platform plugin into the target directory.
# The Chinese explanation lives in docs/gui-validation.md so that this script stays
# pure ASCII and is safe under any console code page.
param(
    [Parameter(Mandatory = $true)][string]$TargetDir,
    [string]$QtBin = "",
    [string]$QtPlugins = "",
    [string]$MinGwBin = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $TargetDir)) {
    New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
}

function Find-QtRoot {
    if ($QtBin -ne "") {
        return (Split-Path -Parent $QtBin)
    }
    $qmake = Get-Command qmake6 -ErrorAction SilentlyContinue
    if (-not $qmake) { $qmake = Get-Command qmake -ErrorAction SilentlyContinue }
    if ($qmake) {
        return (Split-Path -Parent (Split-Path -Parent $qmake.Source))
    }
    foreach ($candidate in @("E:\Qt\6.11.1\mingw_64", "C:\Qt\6.11.1\mingw_64", "D:\Qt\6.11.1\mingw_64")) {
        if (Test-Path (Join-Path $candidate "bin\Qt6Core.dll")) { return $candidate }
    }
    throw "Qt installation not found; pass -QtBin (for example E:\Qt\6.11.1\mingw_64\bin)"
}

# Locate the MinGW bin used to build the project: the Qt prebuilt DLLs import
# libstdc++-6/libgcc_s_seh-1/libwinpthread-1, so those three must ship alongside.
function Find-MinGwBin {
    if ($MinGwBin -ne "") { return $MinGwBin }
    $gxx = Get-Command g++ -ErrorAction SilentlyContinue
    if ($gxx) { return (Split-Path -Parent $gxx.Source) }
    foreach ($candidate in @("D:\mingw64\bin", "C:\mingw64\bin", "E:\Qt\Tools\mingw1310_64\bin")) {
        if (Test-Path (Join-Path $candidate "g++.exe")) { return $candidate }
    }
    throw "MinGW bin not found; pass -MinGwBin (for example D:\mingw64\bin)"
}

$qtRoot = Find-QtRoot
$qtBinDir = Join-Path $qtRoot "bin"
if ($QtPlugins -ne "") { $qtPluginDir = $QtPlugins } else { $qtPluginDir = Join-Path $qtRoot "plugins" }

# 1) Qt6 runtime DLLs actually imported by fanlx_gui (no MinGW runtime; see header comment).
#    Only these three are needed: Qt6Core, Qt6Gui, Qt6Widgets. Copying all 76 Qt DLLs
#    would bloat the delivery directory for no benefit.
$required = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll")
foreach ($dll in $required) {
    $source = Join-Path $qtBinDir $dll
    if (-not (Test-Path $source)) {
        throw ("required Qt runtime DLL missing: {0}" -f $source)
    }
    Copy-Item $source -Destination $TargetDir -Force
    Write-Output ("copied {0}" -f $dll)
}

# 2) Platform plugin: without platforms\qwindows.dll Qt aborts with
#    "could not find or load the Qt platform plugin windows".
$platformTarget = Join-Path $TargetDir "platforms"
New-Item -ItemType Directory -Force -Path $platformTarget | Out-Null
foreach ($plugin in @("qwindows.dll", "qminimal.dll", "qoffscreen.dll")) {
    $source = Join-Path $qtPluginDir ("platforms\" + $plugin)
    if (Test-Path $source) {
        Copy-Item $source -Destination $platformTarget -Force
        Write-Output ("copied platform plugin {0}" -f $plugin)
    } elseif ($plugin -eq "qwindows.dll") {
        throw ("required platform plugin missing: {0}" -f $source)
    }
}

# 3) MinGW C++ runtime DLLs imported by the Qt prebuilt DLLs.
$mingwBinDir = Find-MinGwBin
foreach ($dll in @("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll")) {
    $source = Join-Path $mingwBinDir $dll
    if (-not (Test-Path $source)) {
        throw ("required MinGW runtime DLL missing: {0}" -f $source)
    }
    Copy-Item $source -Destination $TargetDir -Force
    Write-Output ("copied {0}" -f $dll)
}

# 4) Optional style plugin: affects appearance only.
$styleSource = Join-Path $qtPluginDir "styles\qmodernwindowsstyle.dll"
if (Test-Path $styleSource) {
    $styleTarget = Join-Path $TargetDir "styles"
    New-Item -ItemType Directory -Force -Path $styleTarget | Out-Null
    Copy-Item $styleSource -Destination $styleTarget -Force
    Write-Output "copied style plugin qmodernwindowsstyle.dll"
}

Write-Output ("deployment complete: {0}" -f $TargetDir)
Write-Output "Run fanlx_gui.exe from this directory; no PATH changes required."

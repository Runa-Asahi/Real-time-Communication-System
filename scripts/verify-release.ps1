param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
# 每次使用唯一目录，不删除旧证据，也不复用已有目标文件。
$runRoot = Join-Path $projectRoot ('.artifacts/verify-' + [guid]::NewGuid().ToString('N'))
$buildRoot = Join-Path $runRoot 'build'
New-Item -ItemType Directory -Path $runRoot | Out-Null
function Invoke-CheckedFanLX {
    param([string]$Program, [string[]]$Arguments, [string]$LogName)
    & $Program @Arguments 2>&1 | Tee-Object -FilePath (Join-Path $runRoot $LogName)
    # PowerShell 默认不把原生命令的非零退出码变成异常，必须显式检查。
    if ($LASTEXITCODE -ne 0) { throw "$Program failed: $LASTEXITCODE; evidence: $runRoot" }
}
Invoke-CheckedFanLX 'cmake' @('-S', $projectRoot, '-B', $buildRoot, '-G', 'MinGW Makefiles', "-DCMAKE_BUILD_TYPE=$Configuration") 'configure.log'
Invoke-CheckedFanLX 'cmake' @('--build', $buildRoot, '--parallel', '4') 'build.log'
Invoke-CheckedFanLX 'ctest' @('--test-dir', $buildRoot, '--output-on-failure') 'ctest.log'
# smoke 已执行隔离 demo；无需再创建一份演示数据库。
$inputs = @('include', 'src', 'tests', 'benchmarks', 'docs', 'scripts')
$files = foreach ($inputDir in $inputs) { Get-ChildItem -LiteralPath (Join-Path $projectRoot $inputDir) -File -Recurse }
foreach ($entry in @('CMakeLists.txt', 'README.md', '2026-面向对象程序课程设计.doc')) {
    $files += Get-Item -LiteralPath (Join-Path $projectRoot $entry)
}
$files += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'deliverables') -File
$files += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'third_party') -File -Recurse
$files | Sort-Object FullName | ForEach-Object {
    $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
    $hash.Hash + '  ' + $_.FullName.Substring($projectRoot.Length + 1)
} | Set-Content -LiteralPath (Join-Path $runRoot 'manifest.sha256') -Encoding utf8
Write-Output "Verification passed. Evidence: $runRoot"

# im-tokens.ps1 - generate / maintain / distribute IM login tokens.
#
# Why: the server validates LOGIN with a static account->token table loaded once at
# startup, and each client reads its own token file at every login. The same account
# MUST hold the same token on both sides, so hand-editing several JSON files is
# error prone. This script keeps ONE registry (source of truth) and derives every
# file from it, so the two sides can never disagree.
#
# Usage:
#   # 1) add accounts (random 32-char token per new account, existing kept)
#   .\scripts\im-tokens.ps1 -Add 2026001,2026002
#
#   # 2) distribute: server file gets all accounts, each client file gets one account
#   .\scripts\im-tokens.ps1 -Sync -ServerFile data-im-server/tokens.json `
#                           -Client 'data-im-a=2026001','data-im-b=2026002'
#
#   # 3) show current registry (tokens masked)
#   .\scripts\im-tokens.ps1
#
# Reminder: the server reads tokens only at startup, so restart it after -Sync.
# Only ASCII is printed; the registry file itself is UTF-8 (no BOM).

[CmdletBinding()]
param(
    [string]$Registry = 'im-tokens.json',
    [string[]]$Add = @(),
    [switch]$Sync,
    [string]$ServerFile = 'data-im-server/tokens.json',
    [string[]]$Client = @()
)

$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function New-TokenFanLX {
    # 32 hex chars, comfortably above the server's 16-char minimum.
    return [guid]::NewGuid().ToString('N')
}

function Read-RegistryFanLX([string]$path) {
    $map = @{}
    if (-not (Test-Path -LiteralPath $path)) { return $map }
    $text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    if ([string]::IsNullOrWhiteSpace($text)) { return $map }
    foreach ($prop in ($text | ConvertFrom-Json).PSObject.Properties) {
        $map[$prop.Name] = [string]$prop.Value
    }
    return $map
}

function Write-MapFanLX([string]$path, $map) {
    $parent = Split-Path -Parent $path
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    $ordered = [ordered]@{}
    foreach ($key in ($map.Keys | Sort-Object)) { $ordered[$key] = $map[$key] }
    [System.IO.File]::WriteAllText($path, ($ordered | ConvertTo-Json -Compress), $utf8NoBom)
}

function Show-BadTokenFanLX($map, [string]$path) {
    foreach ($key in $map.Keys) {
        if ($map[$key].Length -lt 16 -or $map[$key].Length -gt 4096) {
            Write-Warning "$path : account $key has a token of $($map[$key].Length) chars; the server requires 16..4096."
        }
    }
}

$registryMap = Read-RegistryFanLX $Registry
$generated = @()

foreach ($account in $Add) {
    if ([string]::IsNullOrWhiteSpace($account)) { continue }
    if (-not $registryMap.ContainsKey($account)) {
        $registryMap[$account] = New-TokenFanLX
        $generated += $account
    }
}

if ($generated.Count -gt 0) {
    Write-MapFanLX $Registry $registryMap
    Write-Output "registry  $Registry  <= generated token for $($generated -join ', ')"
}

if ($Sync) {
    if ($registryMap.Count -eq 0) { throw "Registry $Registry is empty; run with -Add first." }

    Write-MapFanLX $ServerFile $registryMap
    Show-BadTokenFanLX $registryMap $ServerFile
    Write-Output "server    $ServerFile  <= all $($registryMap.Count) account(s): $(($registryMap.Keys | Sort-Object) -join ',')"

    foreach ($pair in $Client) {
        $parts = $pair -split '=', 2
        if ($parts.Count -ne 2) { throw "Client entries look like 'dir=account'; got: $pair" }
        $dir = $parts[0].Trim()
        $account = $parts[1].Trim()
        if (-not $registryMap.ContainsKey($account)) {
            throw "account $account is not in $Registry ; add it first: -Add $account"
        }
        $one = @{}
        $one[$account] = $registryMap[$account]
        $file = Join-Path $dir 'token.json'
        Write-MapFanLX $file $one
        Write-Output "client    $file  <= $account"
    }
    Write-Output 'Done. Restart the IM server so it reloads the token table.'
}
elseif ($generated.Count -eq 0) {
    if ($registryMap.Count -eq 0) {
        Write-Output "Registry $Registry does not exist yet. Use -Add <accounts> to create it."
    }
    else {
        Write-Output "Registry $Registry holds $($registryMap.Count) account(s) (tokens masked):"
        foreach ($key in ($registryMap.Keys | Sort-Object)) {
            $token = $registryMap[$key]
            Write-Output ("  {0}  {1}...({2} chars)" -f $key, $token.Substring(0, 4), $token.Length)
        }
    }
}

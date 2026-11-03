@echo off
rem Ciallo - stop the server and clients started from THIS folder only.
rem Other Ciallo copies (for example an older version on port 9500) keep running.
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -Command "$r=(Resolve-Path '%~dp0').Path; Get-Process fanlx_app,Ciallo -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($r, [StringComparison]::OrdinalIgnoreCase) } | ForEach-Object { Write-Host ('stop ' + $_.Name + ' pid ' + $_.Id); Stop-Process -Id $_.Id -Force }"
echo Ciallo (this folder) stopped.
timeout /t 2 >nul

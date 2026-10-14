@echo off
rem Ciallo - host a LAN server on this computer and open a client.
rem ASCII only: all Chinese messages live in Ciallo.ps1 (UTF-8 with BOM).
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Ciallo.ps1" -Mode lan-server %*

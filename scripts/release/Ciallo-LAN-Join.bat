@echo off
rem Ciallo - join a LAN server running on another computer.
rem ASCII only: all Chinese messages live in Ciallo.ps1 (UTF-8 with BOM).
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Ciallo.ps1" -Mode lan-client %*

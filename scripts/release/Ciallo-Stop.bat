@echo off
rem Ciallo - stop the background server and all clients.
taskkill /IM fanlx_app.exe /F >nul 2>&1
taskkill /IM Ciallo.exe /F >nul 2>&1
echo Ciallo stopped.
timeout /t 2 >nul

@echo off
rem ===========================================================================
rem  run-gui.bat -- launcher for the Qt GUI client.
rem
rem  Usage:
rem      scripts\run-gui.bat                       start server if needed, then GUI
rem      scripts\run-gui.bat 9600                  use a different port
rem      scripts\run-gui.bat 9600 2026002          different port and account
rem      Double-clicking also works: the script switches to the project root
rem      itself, so the current directory does not have to be correct.
rem
rem  WHY THIS FILE EXISTS:
rem      The GUI derives its data root from the CURRENT WORKING DIRECTORY.
rem      Double-clicking build\fanlx_gui.exe therefore reads build\data-im-gui,
rem      which is a DIFFERENT data set from the project-root data-im-gui.
rem      The two have different passwords, so login appears to fail with
rem      "wrong user name or password". This launcher always runs from the
rem      project root and passes an explicit --data-root, and it starts the
rem      local server first because login also requires the server to be up.
rem
rem  WHY THIS FILE IS ASCII ONLY -- do NOT add Chinese text back into it:
rem      cmd.exe parses a .bat file with the console's CURRENT code page and its
rem      read position desyncs as soon as that code page changes or multi-byte
rem      characters get sliced. Chinese text inside a .bat therefore makes cmd
rem      execute fragments of its own lines. All Chinese messages live in
rem      run-gui.ps1 (UTF-8 with BOM); this file only sets the code page and
rem      forwards every argument to it.
rem
rem  Environment variables:
rem      FANLX_NO_PAUSE=1   do not wait for a key press when a check fails
rem ===========================================================================
chcp 65001 >nul
setlocal
set "FANLX_EXIT=0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-gui.ps1" %*
set "FANLX_EXIT=%ERRORLEVEL%"
endlocal & exit /b %FANLX_EXIT%

@echo off
rem ===========================================================================
rem  run-im-server.bat -- launcher for the central-relay IM server.
rem
rem  Usage (unchanged):
rem      scripts\run-im-server.bat                   use the default IP and port
rem      scripts\run-im-server.bat 9600              change the port only
rem      scripts\run-im-server.bat 9600 0.0.0.0      change the port and the bind IP
rem      Double-clicking also works: the script switches to the project root
rem      itself, so the current directory does not have to be correct.
rem
rem  WHY THIS FILE IS ASCII ONLY -- do NOT add Chinese text back into it:
rem      cmd.exe parses a .bat file with the console's CURRENT code page and its
rem      read position desyncs as soon as that code page changes or multi-byte
rem      characters get sliced. Chinese text inside a .bat therefore makes cmd
rem      execute fragments of its own lines, which is the classic
rem      'rver.bat' / 'is not recognized as an internal or external command'
rem      noise seen on a 936 (GBK) console. It depends on the code page the
rem      console happened to start with, so it looks random and can even damage
rem      the control flow. All Chinese messages and all pre-flight checks live in
rem      run-im-server.ps1 (UTF-8 with BOM); this file only sets the code page
rem      and forwards every argument to it.
rem
rem      Keep run-im-server.ps1 saved as UTF-8 WITH BOM, otherwise PowerShell 5.1
rem      decodes its Chinese text as GBK and prints mojibake.
rem
rem  Environment variables (unchanged):
rem      FANLX_DRY_RUN=1    print the final command line, do not start the server
rem      FANLX_NO_PAUSE=1   do not wait for a key press when a check fails
rem      FANLX_HOST / FANLX_PORT / FANLX_DATA_DIR / FANLX_EXE /
rem      FANLX_USE_SQLITE   override the defaults without editing any file
rem ===========================================================================
chcp 65001 >nul
setlocal
set "FANLX_EXIT=0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-im-server.ps1" %*
set "FANLX_EXIT=%ERRORLEVEL%"
endlocal & exit /b %FANLX_EXIT%

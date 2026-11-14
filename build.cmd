@echo off
rem MicMix one-click build. All arguments are forwarded to scripts\build.ps1,
rem e.g.  build.cmd -Clean -Install
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" %*
set "MICMIX_EXIT=%ERRORLEVEL%"
rem Keep the window open when started by double-click from Explorer.
echo %CMDCMDLINE% | findstr /I /C:"/c" >nul && if not defined MICMIX_NO_PAUSE pause
exit /b %MICMIX_EXIT%

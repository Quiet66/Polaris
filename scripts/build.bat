@echo off
rem Build on a Windows dev machine. Prepare C:\Qt first (see README).
rem ASCII only on purpose: cmd.exe reads .bat in the system code page.
setlocal
if "%QTROOT%"=="" set QTROOT=C:\Qt
set PATH=%QTROOT%\5.15.2\mingw81_32\bin;%QTROOT%\Tools\mingw810_32\bin;%PATH%
cd /d "%~dp0.."
qmake polaris.pro -spec win32-g++ CONFIG+=release || exit /b 1
mingw32-make -j4 || exit /b 1
echo Build OK: build\bin\polaris.exe

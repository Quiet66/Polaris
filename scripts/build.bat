@echo off
rem Windows 开发机上的构建脚本。先按 README 准备 C:\Qt。
setlocal
if "%QTROOT%"=="" set QTROOT=C:\Qt
set PATH=%QTROOT%\5.15.2\mingw81_32\bin;%QTROOT%\Tools\mingw810_32\bin;%PATH%
cd /d "%~dp0.."
qmake trailcanvas.pro -spec win32-g++ CONFIG+=release || exit /b 1
mingw32-make -j4 || exit /b 1
echo 构建完成: build\bin\trailcanvas.exe

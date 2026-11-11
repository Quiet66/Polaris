@echo off
rem 打包便携版目录 dist\TrailCanvas（SPEC 第 11 节）。先运行 build.bat。
rem Qt 5.15.2 MinGW 版的 windeployqt 会把未 strip 的插件误判为 debug，
rem 报 "Unable to find the platform plugin"，所以这里按固定清单手工拷贝。
setlocal
if "%QTROOT%"=="" set QTROOT=C:\Qt
set QT=%QTROOT%\5.15.2\mingw81_32
cd /d "%~dp0.."
set OUT=dist\TrailCanvas
if exist %OUT% rmdir /s /q %OUT%
mkdir %OUT%\platforms %OUT%\styles || exit /b 1
copy /y build\bin\trailcanvas.exe %OUT%\ >nul || exit /b 1
for %%f in (Qt5Core Qt5Gui Qt5Widgets) do (
    copy /y "%QT%\bin\%%f.dll" %OUT%\ >nul || exit /b 1
)
for %%f in (libgcc_s_dw2-1 libstdc++-6 libwinpthread-1) do (
    copy /y "%QTROOT%\Tools\mingw810_32\bin\%%f.dll" %OUT%\ >nul || exit /b 1
)
copy /y "%QT%\plugins\platforms\qwindows.dll" %OUT%\platforms\ >nul || exit /b 1
copy /y "%QT%\plugins\styles\qwindowsvistastyle.dll" %OUT%\styles\ >nul || exit /b 1
echo 打包完成: %OUT%

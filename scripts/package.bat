@echo off
rem Assemble the portable folder dist\TrailCanvas (SPEC section 11). Run build.bat first.
rem ASCII only on purpose: cmd.exe reads .bat in the system code page.
rem
rem windeployqt is not used: the one shipped with Qt 5.15.2 MinGW mistakes the
rem unstripped plugins for debug builds and fails with
rem "Unable to find the platform plugin". Files are copied from a fixed list instead.
setlocal
if "%QTROOT%"=="" set QTROOT=C:\Qt
set QT=%QTROOT%\5.15.2\mingw81_32
set MINGW=%QTROOT%\Tools\mingw810_32\bin
cd /d "%~dp0.."
set OUT=dist\TrailCanvas
if exist %OUT% rmdir /s /q %OUT%
mkdir %OUT%\platforms %OUT%\styles || exit /b 1
copy /y build\bin\trailcanvas.exe %OUT%\ >nul || exit /b 1
for %%f in (Qt5Core Qt5Gui Qt5Widgets) do (
    copy /y "%QT%\bin\%%f.dll" %OUT%\ >nul || exit /b 1
)
for %%f in (libgcc_s_dw2-1 libstdc++-6 libwinpthread-1) do (
    copy /y "%MINGW%\%%f.dll" %OUT%\ >nul || exit /b 1
)
copy /y "%QT%\plugins\platforms\qwindows.dll" %OUT%\platforms\ >nul || exit /b 1
copy /y "%QT%\plugins\styles\qwindowsvistastyle.dll" %OUT%\styles\ >nul || exit /b 1

rem Slimming: drop unneeded symbol tables from the Qt DLLs and plugins (about 32 MB to 25 MB).
for %%f in (%OUT%\Qt5*.dll %OUT%\platforms\*.dll %OUT%\styles\*.dll) do (
    "%MINGW%\strip.exe" --strip-unneeded "%%f" || exit /b 1
)
echo Package OK: %OUT%

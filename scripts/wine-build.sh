#!/bin/bash
# Linux + Wine 下构建并打包，与 build.bat + package.bat 等价。
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/wine-env.sh

wine qmake.exe polaris.pro -spec win32-g++ CONFIG+=release
wine mingw32-make.exe -j4

# 部署：Qt 5.15.2 MinGW 版的 windeployqt 会把未 strip 的插件误判为 debug，
# 报 "Unable to find the platform plugin"，所以这里按固定清单手工拷贝。
QT="$QTROOT/5.15.2/mingw81_32"
OUT=dist/Polaris
rm -rf "$OUT" && mkdir -p "$OUT/platforms" "$OUT/styles"
cp build/bin/polaris.exe "$OUT/"
cp "$QT"/bin/{Qt5Core,Qt5Gui,Qt5Widgets}.dll "$OUT/"
cp "$QTROOT"/Tools/mingw810_32/bin/{libgcc_s_dw2-1,libstdc++-6,libwinpthread-1}.dll "$OUT/"
cp "$QT/plugins/platforms/qwindows.dll" "$OUT/platforms/"
cp "$QT/plugins/styles/qwindowsvistastyle.dll" "$OUT/styles/"
# 使用说明与许可文件。
cp packaging/README.txt packaging/THIRD-PARTY.txt "$OUT/"
cp LICENSE "$OUT/LICENSE.txt"
mkdir -p "$OUT/licenses" && cp packaging/licenses/*.txt "$OUT/licenses/"

# 瘦身：去掉 Qt DLL 和插件里用不到的符号表（约 32 MB -> 25 MB）。
for f in "$OUT"/Qt5*.dll "$OUT"/platforms/*.dll "$OUT"/styles/*.dll; do
    wine strip.exe --strip-unneeded "$(winepath -w "$f")"
done
(cd dist && rm -f Polaris.zip && zip -q -r -9 Polaris.zip Polaris)
du -sh dist/Polaris dist/Polaris.zip

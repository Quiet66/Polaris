# 在 Linux 上通过 Wine 运行原版 Windows 工具链（Qt 5.15.2 win32_mingw81 + MinGW 8.1）。
# 用法：source scripts/wine-env.sh
export QTROOT="${QTROOT:-/opt/Qt}"
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-trailcanvas}"
export WINEARCH=win32
export WINEDEBUG=-all
_w() { winepath -w "$1" 2>/dev/null; }
export WINEPATH="$(_w "$QTROOT/5.15.2/mingw81_32/bin");$(_w "$QTROOT/Tools/mingw810_32/bin")"

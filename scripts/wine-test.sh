#!/bin/bash
# Linux + Wine 下构建并运行 tests/ 里的程序。
#   scripts/wine-test.sh          MdIo 的控制台测试
#   scripts/wine-test.sh bench    画布绘制性能基准（需要 X，例如 xvfb-run -a）
set -euo pipefail
cd "$(dirname "$0")/../tests"
source ../scripts/wine-env.sh
target="tst_mdio"
[ "${1:-}" = "bench" ] && target="bench_canvas"
wine qmake.exe "$target.pro" -spec win32-g++ CONFIG+=release
wine mingw32-make.exe -j4
wine "build/$target.exe" "${@:2}"

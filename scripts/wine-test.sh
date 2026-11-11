#!/bin/bash
# Linux + Wine 下构建并运行 tests/ 里的控制台测试。
set -euo pipefail
cd "$(dirname "$0")/../tests"
source ../scripts/wine-env.sh
wine qmake.exe tst_mdio.pro -spec win32-g++ CONFIG+=release
wine mingw32-make.exe -j4
wine build/tst_mdio.exe

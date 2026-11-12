# TrailCanvas

始终置顶、无边框的查询路径画布。规格见 [SPEC.md](SPEC.md)。

- Qt 5.15.2（仅 QtCore / QtGui / QtWidgets），32 位 MinGW 8.1，qmake。
- 无第三方库；Win32 调用全部在 `src/WinNative.*`。
- 颜色、圆角、间距集中在 `src/Theme.*`（SPEC 目录清单之外新增的唯一文件）。

## Windows 上构建

准备环境（一次性，需联网，不需要 Qt 账号）：

```
pip install aqtinstall
aqt install-qt   -O C:\Qt windows desktop 5.15.2 win32_mingw81
aqt install-tool -O C:\Qt windows desktop tools_mingw qt.tools.win32_mingw810
```

下载中途报连接错误时，在 `aqt` 后加 `-b https://master.qt.io` 重试。

构建与打包：

```
scripts\build.bat      生成 build\bin\trailcanvas.exe
scripts\package.bat    生成 dist\TrailCanvas\（便携版目录）
```

Qt 不在 `C:\Qt` 时先 `set QTROOT=你的路径`。

## Linux 上构建（Wine）

用 Wine 直接运行同一套 Windows 版 Qt 和 MinGW，产物与 Windows 上构建的一致。

```
apt-get install wine wine32:i386 xvfb zip      # 需先 dpkg --add-architecture i386
pip install aqtinstall
aqt install-qt   -b https://master.qt.io -O /opt/Qt windows desktop 5.15.2 win32_mingw81 --archives qtbase qttools
aqt install-tool -b https://master.qt.io -O /opt/Qt windows desktop tools_mingw qt.tools.win32_mingw810
xvfb-run -a scripts/wine-build.sh               # 生成 dist/TrailCanvas 和 dist/TrailCanvas.zip
```

## 测试与基准

`tests/` 里有两个程序：

- `tst_mdio`：MdIo 的控制台测试（大纲生成规则、保存读入往返）。
- `bench_canvas`：画布绘制性能基准，默认 200 个节点，输出平移、缩放、拖动时的每帧耗时。
  每一帧都同步重绘整个视口，是最坏情况。可带参数：节点数、背景（plain / dots / lines）。

```
xvfb-run -a scripts/wine-test.sh                    # Linux + Wine：MdIo 测试
xvfb-run -a scripts/wine-test.sh bench 200 dots     # Linux + Wine：性能基准
```

Windows 上：进入 `tests\`，`qmake tst_mdio.pro -spec win32-g++ CONFIG+=release`
（或 `bench_canvas.pro`），`mingw32-make`，运行 `build\` 下的 exe。
`bench_canvas.exe` 需要和 Qt 的 DLL、`platforms\qwindows.dll` 放在一起，或把 Qt 的 `bin` 加进 PATH。

## 打包说明

没有使用 `windeployqt`：Qt 5.15.2 MinGW 版的 `windeployqt` 会把未 strip 的插件误判为
debug 版，报 "Unable to find the platform plugin"。打包脚本按固定清单拷贝：

```
trailcanvas.exe
Qt5Core.dll  Qt5Gui.dll  Qt5Widgets.dll
libgcc_s_dw2-1.dll  libstdc++-6.dll  libwinpthread-1.dll
platforms\qwindows.dll
styles\qwindowsvistastyle.dll
```

拷贝后用 MinGW 的 `strip --strip-unneeded` 去掉 Qt DLL 和插件里的符号表，
程序目录从约 32 MB 降到约 25 MB，zip 约 10 MB。

`.bat` 脚本只用 ASCII 字符：cmd.exe 按系统代码页读批处理文件，UTF-8 的中文在中文 Windows 上会乱码。

# 开发说明

面向想自己编译、修改 Polaris 的读者。使用方法见仓库首页的 [README](../README.md)。

## 技术概况

- Qt 5.15.2（仅 QtCore / QtGui / QtWidgets），C++14，32 位 MinGW 8.1，qmake。
- 不依赖任何第三方库；不使用 QML、OpenGL、QGraphicsEffect。
- 画布基于 `QGraphicsScene` + `QGraphicsView`，视口是普通 `QWidget`（软件渲染）。
- Win32 专用调用全部集中在 `src/WinNative.*`，其他文件不包含 `windows.h`。
- 颜色、圆角、间距集中在 `src/Theme.*`。
- 32 位构建同时支持 32 位和 64 位 Windows，只发一个包。

## 目录结构

```
polaris.pro            qmake 工程
resources.qrc          内嵌资源（图标）
assets/                图标源文件
src/
  main.cpp             启动、单实例、字体、卡死与崩溃记录
  MainWindow.*         无边框窗口、菜单、托盘、全局快捷键、文件操作
  CanvasView.*         视图：平移缩放、背景、鼠标手势、键盘分发
  CanvasScene.*        场景：节点和连线的增删、选择、撤销、动画
  NodeItem.*           节点：绘制、文字编辑、宽度手柄、连接点
  EdgeItem.*           连线：曲线、箭头、命中范围
  Document.*           数据模型与 JSON
  Commands.*           撤销命令（QUndoStack）
  MdIo.*               Markdown 写出与读入
  Exporter.*           导出 PNG
  Prefs.*              偏好设置（ini）
  Theme.*              浅色 / 深色主题
  WinNative.*          置顶、全局快捷键、无边框缩放、单实例等 Win32 调用
packaging/             随程序包分发的使用说明与许可文件
scripts/               构建、打包、测试脚本（Windows .bat 与 Linux + Wine .sh）
tests/                 MdIo 测试与画布性能基准
docs/                  文档与图片
SPEC.md                最初的需求规格（暂定名 TrailCanvas）
```

## 文件格式

文件保存为 UTF-8（无 BOM）的 `.md`。正文是按连线生成的大纲，最后一行
`<!-- polaris:{...} -->` 是完整布局的紧凑 JSON，打开时完全按它还原。
JSON 里的 `>` 写作 `\u003e`，避免提前结束注释。
改名前保存的文件（`<!-- trailcanvas:`）仍然能打开。大纲生成规则见 [SPEC.md](../SPEC.md) 第 7 节。

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
scripts\build.bat      生成 build\bin\polaris.exe
scripts\package.bat    生成 dist\Polaris\（便携版目录，含使用说明和许可文件）
```

Qt 不在 `C:\Qt` 时先 `set QTROOT=你的路径`。

## Linux 上构建（Wine）

用 Wine 直接运行同一套 Windows 版 Qt 和 MinGW，产物与 Windows 上构建的一致。

```
apt-get install wine wine32:i386 xvfb zip      # 需先 dpkg --add-architecture i386
pip install aqtinstall
aqt install-qt   -b https://master.qt.io -O /opt/Qt windows desktop 5.15.2 win32_mingw81 --archives qtbase qttools
aqt install-tool -b https://master.qt.io -O /opt/Qt windows desktop tools_mingw qt.tools.win32_mingw810
xvfb-run -a scripts/wine-build.sh               # 生成 dist/Polaris 和 dist/Polaris.zip
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
polaris.exe
Qt5Core.dll  Qt5Gui.dll  Qt5Widgets.dll
libgcc_s_dw2-1.dll  libstdc++-6.dll  libwinpthread-1.dll
platforms\qwindows.dll
styles\qwindowsvistastyle.dll
```

拷贝后用 MinGW 的 `strip --strip-unneeded` 去掉 Qt DLL 和插件里的符号表，
程序目录从约 32 MB 降到约 25 MB，zip 约 10 MB。

`.bat` 脚本只用 ASCII 字符：cmd.exe 按系统代码页读批处理文件，UTF-8 的中文在中文 Windows 上会乱码。

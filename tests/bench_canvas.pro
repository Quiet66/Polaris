# 画布绘制性能基准：200 个节点时平移、缩放、拖动的每帧耗时。
# 构建后运行 build/bench_canvas.exe [节点数]。
QT       = core gui widgets
TEMPLATE = app
TARGET   = bench_canvas
CONFIG  += c++14 console warn_on
CONFIG  -= debug_and_release app_bundle
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
INCLUDEPATH += ../src
SOURCES += bench_canvas.cpp \
    ../src/CanvasScene.cpp ../src/CanvasView.cpp ../src/NodeItem.cpp ../src/EdgeItem.cpp \
    ../src/Commands.cpp ../src/Document.cpp ../src/Theme.cpp ../src/Prefs.cpp
HEADERS += \
    ../src/CanvasScene.h ../src/CanvasView.h ../src/NodeItem.h ../src/EdgeItem.h \
    ../src/Commands.h ../src/Document.h ../src/Theme.h ../src/Prefs.h
DESTDIR     = build
OBJECTS_DIR = build/obj_bench
MOC_DIR     = build/moc_bench

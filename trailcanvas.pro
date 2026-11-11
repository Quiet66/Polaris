# TrailCanvas — Qt 5.15.2 / MinGW 8.1 (32-bit)
# 只使用 QtCore / QtGui / QtWidgets，无第三方库。
QT       = core gui widgets
TEMPLATE = app
TARGET   = trailcanvas
CONFIG  += c++14 warn_on
CONFIG  -= debug_and_release

DEFINES += QT_DEPRECATED_WARNINGS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII \
           UNICODE _UNICODE WINVER=0x0601 _WIN32_WINNT=0x0601

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/MainWindow.cpp \
    src/CanvasView.cpp \
    src/CanvasScene.cpp \
    src/NodeItem.cpp \
    src/EdgeItem.cpp \
    src/Commands.cpp \
    src/Document.cpp \
    src/MdIo.cpp \
    src/Exporter.cpp \
    src/Theme.cpp \
    src/Prefs.cpp \
    src/WinNative.cpp

HEADERS += \
    src/MainWindow.h \
    src/CanvasView.h \
    src/CanvasScene.h \
    src/NodeItem.h \
    src/EdgeItem.h \
    src/Commands.h \
    src/Document.h \
    src/MdIo.h \
    src/Exporter.h \
    src/Theme.h \
    src/Prefs.h \
    src/WinNative.h

RESOURCES += resources.qrc
RC_ICONS   = assets/app.ico

LIBS += -luser32

DESTDIR     = build/bin
OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc

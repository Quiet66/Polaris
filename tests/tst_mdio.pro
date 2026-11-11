# MdIo / Document 的控制台测试。构建后运行 build/tst_mdio.exe，全部通过时退出码为 0。
QT       = core
TEMPLATE = app
TARGET   = tst_mdio
CONFIG  += c++14 console warn_on
CONFIG  -= debug_and_release app_bundle
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
INCLUDEPATH += ../src
SOURCES += tst_mdio.cpp ../src/MdIo.cpp ../src/Document.cpp
HEADERS += ../src/MdIo.h ../src/Document.h
DESTDIR     = build
OBJECTS_DIR = build/obj
MOC_DIR     = build/moc

#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("TrailCanvas"));

    // 优先微软雅黑（Win7 自带）；不存在时保留系统默认字体。
    const QString yahei = QStringLiteral("Microsoft YaHei");
    if (QFontDatabase().families().contains(yahei)) {
        QFont f(yahei);
        f.setPointSize(10);
        QApplication::setFont(f);
    }

    MainWindow w;
    w.show();
    return app.exec();
}

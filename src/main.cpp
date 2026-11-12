#include "MainWindow.h"
#include "WinNative.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Polaris"));

    // 单实例：重复启动时唤出已有窗口，然后退出。
    if (!WinNative::acquireSingleInstance()) {
        WinNative::notifyExistingInstance();
        return 0;
    }
    // 窗口隐藏到托盘时程序继续运行。
    QApplication::setQuitOnLastWindowClosed(false);

    // 优先微软雅黑（Win7 自带）；不存在时保留系统默认字体。
    const QString yahei = QStringLiteral("Microsoft YaHei");
    if (QFontDatabase().families().contains(yahei)) {
        QFont f(yahei);
        f.setPointSize(10);
        QApplication::setFont(f);
    }

    MainWindow w;
    w.startup();

    // 先看有没有需要恢复的草稿；没有恢复草稿时，命令行带了文件就打开它。
    const QStringList args = QCoreApplication::arguments();
    if (!w.checkDraftOnStartup() && args.size() > 1)
        w.openFile(args.at(1));

    return app.exec();
}

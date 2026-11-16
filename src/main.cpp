#include "MainWindow.h"
#include "Prefs.h"
#include "WinNative.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QThread>
#include <QTimer>

#include <atomic>

// ---------------------------------------------------------------- 卡死记录
//
// 界面线程每 500 ms 跳一下"心跳"；另一个线程盯着它，心跳停了 5 秒以上就往数据目录的
// hang.log 里记一行：当时界面线程正在把哪种事件交给哪个类处理。恢复后再记一行持续时间。
// 平时的开销只是每个事件多写两个原子变量。

namespace {

std::atomic<int>          g_heartbeat{0};
std::atomic<int>          g_eventType{0};
std::atomic<const char *> g_receiverClass{""};

class App : public QApplication
{
public:
    using QApplication::QApplication;

    bool notify(QObject *receiver, QEvent *event) override
    {
        // className() 返回的是静态字符串，跨线程读指针是安全的。
        g_receiverClass.store(receiver->metaObject()->className(), std::memory_order_relaxed);
        g_eventType.store(int(event->type()), std::memory_order_relaxed);
        return QApplication::notify(receiver, event);
    }
};

class Watchdog : public QThread
{
public:
    explicit Watchdog(const QString &logPath) : m_logPath(logPath) {}

protected:
    void run() override
    {
        const int limitTicks = 10;      // 10 x 500 ms = 5 秒
        int last = g_heartbeat.load();
        int stalled = 0;
        bool reported = false;
        while (!isInterruptionRequested()) {
            msleep(500);
            const int now = g_heartbeat.load();
            if (now != last) {
                if (reported)
                    append(QStringLiteral("recovered after about %1 s").arg((stalled + 1) / 2));
                last = now;
                stalled = 0;
                reported = false;
                continue;
            }
            if (++stalled == limitTicks && !reported) {
                reported = true;
                append(QStringLiteral("UI thread not responding for 5 s; last event type=%1 receiver=%2")
                           .arg(g_eventType.load())
                           .arg(QString::fromLatin1(g_receiverClass.load())));
            }
        }
    }

private:
    void append(const QString &what)
    {
        QFile f(m_logPath);
        if (QFileInfo(f).size() > 256 * 1024)
            return;             // 不让日志无限增长
        if (!f.open(QIODevice::Append | QIODevice::Text))
            return;
        const QString line = QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ')
                           + what + QLatin1Char('\n');
        f.write(line.toUtf8());
    }

    QString m_logPath;
};

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    App app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Polaris"));

    // 单实例：重复启动时唤出已有窗口，然后退出。
    if (!WinNative::acquireSingleInstance()) {
        WinNative::notifyExistingInstance();
        return 0;
    }
    // 窗口隐藏到托盘时程序继续运行。
    QApplication::setQuitOnLastWindowClosed(false);

    // 崩溃和卡死都在数据目录里留一条记录，方便事后排查。
    const QString dataDir = Prefs::instance().dataDir();
    WinNative::installCrashLogger(QDir::toNativeSeparators(dataDir + QStringLiteral("/crash.log")));
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, []() { g_heartbeat.fetch_add(1); });
    heartbeat.start(500);
    Watchdog watchdog(dataDir + QStringLiteral("/hang.log"));
    watchdog.start(QThread::LowPriority);

    // 优先微软雅黑（Win7 自带）；不存在时保留系统默认字体。
    const QString yahei = QStringLiteral("Microsoft YaHei");
    if (QFontDatabase().families().contains(yahei)) {
        QFont f(yahei);
        f.setPointSize(10);
        QApplication::setFont(f);
    }

    int rc = 0;
    {
        MainWindow w;
        w.startup();

        // 先看有没有需要恢复的草稿；没有恢复草稿时，命令行带了文件就打开它。
        const QStringList args = QCoreApplication::arguments();
        if (!w.checkDraftOnStartup() && args.size() > 1)
            w.openFile(args.at(1));

        rc = app.exec();
    }

    watchdog.requestInterruption();
    watchdog.wait(2000);
    return rc;
}

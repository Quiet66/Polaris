#include "MainWindow.h"
#include "WinNative.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>

namespace {
const int kResizeBorder = 6;    // 边缘缩放热区（设备无关像素）
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("TrailCanvas"));
    setMinimumSize(240, 160);
    resize(720, 480);
    WinNative::setupFramelessWindow(this);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    if (WinNative::handleNativeEvent(this, eventType, message, result, kResizeBorder))
        return true;
    return QWidget::nativeEvent(eventType, message, result);
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0xFA, 0xFA, 0xF8));
    p.setPen(QColor(0xD8, 0xD8, 0xD4));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    p.setPen(QColor(0x9A, 0x9A, 0x96));
    p.drawText(rect(), Qt::AlignCenter,
               QStringLiteral("TrailCanvas · M0\n"
                              "拖动窗口内任意位置移动，拖动边缘调整大小\n"
                              "双击切换最大化，Esc 或 Alt+F4 退出"));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // M0 临时：还没有菜单，用 Esc 退出。
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    // M0 临时：还没有菜单按钮，拖动窗口内任意位置即可移动窗口。
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

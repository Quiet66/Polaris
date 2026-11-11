#include "MainWindow.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Theme.h"
#include "WinNative.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWindow>

namespace {
const int kResizeBorder = 6;    // 边缘缩放热区（设备无关像素）
const int kButtonSize   = 22;
const int kButtonMargin = 10;
}

// ------------------------------------------------------------------ MenuButton

MenuButton::MenuButton(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(kButtonSize, kButtonSize);
    setCursor(Qt::ArrowCursor);
    setToolTip(QStringLiteral("拖动移动窗口，双击最大化"));
}

void MenuButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor c = Theme::current().buttonFill;
    c.setAlpha(m_pressed ? 220 : (m_hover ? 170 : 60));
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QRectF(rect()).adjusted(3, 3, -3, -3));
}

void MenuButton::enterEvent(QEvent *)
{
    m_hover = true;
    update();
}

void MenuButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void MenuButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_pressPos = event->globalPos();
    update();
}

void MenuButton::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed)
        return;
    if ((event->globalPos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        // 交给系统拖动窗口；之后的鼠标释放由系统消化。
        m_pressed = false;
        update();
        emit dragStarted();
    }
}

void MenuButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    update();
    if (m_suppressClick) {
        m_suppressClick = false;    // 双击的第二次释放不算单击
        return;
    }
    if (rect().contains(event->pos()))
        emit clicked();
}

void MenuButton::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_suppressClick = true;
    m_pressPos = event->globalPos();
    emit doubleClicked();
}

// ------------------------------------------------------------------ MainWindow

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_scene(new CanvasScene(this))
    , m_view(new CanvasView(m_scene, this))
    , m_menuButton(new MenuButton(this))
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("TrailCanvas"));
    setMinimumSize(240, 160);
    resize(900, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);     // 留 1px 画窗口描边
    layout->addWidget(m_view);

    m_menuButton->move(kButtonMargin, kButtonMargin);
    m_menuButton->raise();
    connect(m_menuButton, &MenuButton::dragStarted, this, [this]() {
        if (windowHandle())
            windowHandle()->startSystemMove();
    });
    connect(m_menuButton, &MenuButton::doubleClicked, this, &MainWindow::toggleMaximized);
    // clicked() 弹出菜单：M3 实现。

    m_view->setFocus();
    WinNative::setupFramelessWindow(this);
}

void MainWindow::toggleMaximized()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
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
    p.setPen(Theme::current().windowBorder);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

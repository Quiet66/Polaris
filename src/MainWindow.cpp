#include "MainWindow.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Prefs.h"
#include "Theme.h"
#include "WinNative.h"

#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWindow>

namespace {
// 菜单项左侧的勾。所有菜单项都带一个同尺寸的图标（未勾选时是透明的），
// 这样勾选项和普通项的文字能对齐；样式表下原生的勾选标记会把文字挤偏。
QIcon checkIcon(bool on)
{
    const int px = 12, scale = 2;
    QPixmap pm(px * scale, px * scale);
    pm.setDevicePixelRatio(scale);
    pm.fill(Qt::transparent);
    if (on) {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(Theme::current().menuText, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF pts[] = { QPointF(2.5, 6.5), QPointF(5, 9), QPointF(9.5, 3.5) };
        p.drawPolyline(pts, 3);
    }
    return QIcon(pm);
}

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
    setToolTip(QStringLiteral("单击打开菜单，拖动移动窗口，双击最大化"));
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
    connect(m_menuButton, &MenuButton::clicked, this, &MainWindow::showMenu);

    m_scene->setSettings(Prefs::instance().docDefaults());

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

void MainWindow::applySettings(const DocSettings &s)
{
    m_scene->setSettings(s);
    Prefs::instance().setDocDefaults(s);
    m_view->viewport()->update();
    m_menuButton->update();
    update();
}

void MainWindow::showMenu()
{
    const Theme &t = Theme::current();
    Prefs &prefs = Prefs::instance();
    const DocSettings cur = m_scene->settings();

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // 圆角菜单：弹出窗口本身要透明、无系统阴影，否则四角会露出方形底。
    auto polish = [&t](QMenu *m) {
        m->setWindowFlags(m->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        m->setAttribute(Qt::WA_TranslucentBackground);
        m->setStyleSheet(t.menuStyleSheet());
        m->setFont(QApplication::font());
    };
    polish(menu);

    auto later = [menu](const QString &text, const QString &shortcut = QString()) {
        // 后续里程碑才实现的功能：先占位并置灰。
        QAction *a = menu->addAction(shortcut.isEmpty() ? text
                                                        : text + QLatin1Char('\t') + shortcut);
        a->setEnabled(false);
        a->setIcon(checkIcon(false));
        return a;
    };
    auto toggle = [](QMenu *m, const QString &text, bool on) {
        return m->addAction(checkIcon(on), text);
    };

    later(QStringLiteral("新建"), QStringLiteral("Ctrl+N"));
    later(QStringLiteral("打开…"), QStringLiteral("Ctrl+O"));
    later(QStringLiteral("保存"), QStringLiteral("Ctrl+S"));
    later(QStringLiteral("另存为…"), QStringLiteral("Ctrl+Shift+S"));
    later(QStringLiteral("导出图片…"));
    menu->addSeparator();

    QMenu *bg = menu->addMenu(checkIcon(false), QStringLiteral("背景"));
    polish(bg);
    const struct { const char *key; QString label; } bgs[] = {
        { "plain", QStringLiteral("纯色") },
        { "dots",  QStringLiteral("点阵") },
        { "lines", QStringLiteral("线阵") },
    };
    for (const auto &b : bgs) {
        const QString key = QString::fromLatin1(b.key);
        QAction *a = toggle(bg, b.label, cur.background == key);
        connect(a, &QAction::triggered, this, [this, key]() {
            DocSettings s = m_scene->settings();
            s.background = key;
            applySettings(s);
        });
    }

    QMenu *th = menu->addMenu(checkIcon(false), QStringLiteral("主题"));
    polish(th);
    const struct { const char *key; QString label; } ths[] = {
        { "light", QStringLiteral("浅色") },
        { "dark",  QStringLiteral("深色") },
    };
    for (const auto &x : ths) {
        const QString key = QString::fromLatin1(x.key);
        QAction *a = toggle(th, x.label, cur.theme == key);
        connect(a, &QAction::triggered, this, [this, key]() {
            DocSettings s = m_scene->settings();
            s.theme = key;
            applySettings(s);
        });
    }

    QAction *directed = toggle(menu, QStringLiteral("连线方向"), cur.directed);
    connect(directed, &QAction::triggered, this, [this]() {
        DocSettings s = m_scene->settings();
        s.directed = !s.directed;
        applySettings(s);
    });

    QAction *snap = toggle(menu, QStringLiteral("对齐吸附"), prefs.snapEnabled());
    connect(snap, &QAction::triggered, this, []() {
        Prefs::instance().setSnapEnabled(!Prefs::instance().snapEnabled());
    });

    QAction *motion = toggle(menu, QStringLiteral("精简动效"), prefs.reduceMotion());
    connect(motion, &QAction::triggered, this, []() {
        Prefs::instance().setReduceMotion(!Prefs::instance().reduceMotion());
    });

    menu->addSeparator();
    later(QStringLiteral("窗口置顶"));
    later(QStringLiteral("透明度"));
    later(QStringLiteral("快捷键设置…"));
    menu->addSeparator();

    QAction *max = menu->addAction(checkIcon(false), isMaximized() ? QStringLiteral("还原")
                                                                   : QStringLiteral("最大化"));
    connect(max, &QAction::triggered, this, &MainWindow::toggleMaximized);
    QAction *quit = menu->addAction(checkIcon(false), QStringLiteral("退出"));
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    menu->popup(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height() + 2)));
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

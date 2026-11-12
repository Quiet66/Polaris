#include "MainWindow.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Exporter.h"
#include "MdIo.h"
#include "Prefs.h"
#include "Theme.h"
#include "WinNative.h"

#include <functional>

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVariantAnimation>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
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
const int kDraftDelayMs = 2000; // 内容变化后多久写一次草稿

// 全局快捷键。id 即数组下标 + 1。
struct HotkeyDef
{
    const char *name;           // 偏好里的键名
    const char *fallback;       // 默认值
    QString     label;
    bool        allowRepeat;    // 按住时是否连续触发
};

const QVector<HotkeyDef> &hotkeyDefs()
{
    static const QVector<HotkeyDef> defs = {
        { "toggle",      "Ctrl+Alt+Space", QStringLiteral("隐藏 / 唤出窗口"), false },
        { "topmost",     "Ctrl+Alt+T",     QStringLiteral("切换置顶"),        false },
        { "opacityUp",   "Ctrl+Alt+Up",    QStringLiteral("提高不透明度"),    true  },
        { "opacityDown", "Ctrl+Alt+Down",  QStringLiteral("降低不透明度"),    true  },
    };
    return defs;
}

// 鼠标手势的默认值与可选项。
const MouseGesture kDefaultPan    = MouseGesture::SpaceLeft;
const MouseGesture kDefaultWindow = MouseGesture::AltLeft;

MouseGesture storedGesture(const char *name, MouseGesture fallback)
{
    const QString key = Prefs::instance().mouseGesture(QString::fromLatin1(name),
                                                       CanvasView::gestureKey(fallback));
    return CanvasView::gestureFromKey(key, fallback);
}

QString hotkeyValue(const HotkeyDef &d)
{
    return Prefs::instance().hotkey(QString::fromLatin1(d.name), QString::fromLatin1(d.fallback));
}
}

// ------------------------------------------------------------------ MenuButton

MenuButton::MenuButton(QWidget *parent)
    : QWidget(parent)
    , m_fade(new QVariantAnimation(this))
{
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_reveal = v.toReal();
        update();
    });
    setFixedSize(kButtonSize, kButtonSize);
    setCursor(Qt::ArrowCursor);
    setToolTip(QStringLiteral("单击打开菜单，拖动移动窗口，双击最大化"));
}

void MenuButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor c = Theme::current().buttonFill;
    c.setAlpha(m_pressed ? 220 : qRound(60 + 110 * m_reveal));
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(QRectF(rect()).adjusted(3, 3, -3, -3));
}

void MenuButton::fadeTo(qreal target)
{
    m_fade->stop();
    if (Prefs::instance().reduceMotion()) {
        m_reveal = target;      // 精简动效：直接到位
        update();
        return;
    }
    m_fade->setDuration(Theme::current().buttonFadeMs);
    m_fade->setStartValue(m_reveal);
    m_fade->setEndValue(target);
    m_fade->start();
}

void MenuButton::enterEvent(QEvent *)
{
    m_hover = true;
    fadeTo(1);
}

void MenuButton::leaveEvent(QEvent *)
{
    m_hover = false;
    fadeTo(0);
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
    , m_draftTimer(new QTimer(this))
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("Polaris"));
    setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
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

    // 内容变化：标记未保存，并在 2 秒后写一次草稿。
    m_draftTimer->setSingleShot(true);
    m_draftTimer->setInterval(kDraftDelayMs);
    connect(m_draftTimer, &QTimer::timeout, this, &MainWindow::writeDraft);
    connect(m_scene, &CanvasScene::modified, this, [this]() { setDirty(true); });

    auto shortcut = [this](const QKeySequence &key, void (MainWindow::*slot)()) {
        QShortcut *sc = new QShortcut(key, this);
        sc->setContext(Qt::WindowShortcut);
        connect(sc, &QShortcut::activated, this, slot);
    };
    shortcut(QKeySequence(Qt::CTRL + Qt::Key_N), &MainWindow::newDocument);
    shortcut(QKeySequence(Qt::CTRL + Qt::Key_O), &MainWindow::openDocument);
    QShortcut *save = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S), this);
    connect(save, &QShortcut::activated, this, [this]() { saveDocument(); });
    QShortcut *saveAs = new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_S), this);
    connect(saveAs, &QShortcut::activated, this, [this]() { saveDocumentAs(); });

    m_view->setFocus();
    WinNative::setupFramelessWindow(this);

    restoreWindowState();
    setupTray();
    applyMouseGestures();
}

void MainWindow::applyMouseGestures()
{
    m_view->setGestures(storedGesture("pan", kDefaultPan), storedGesture("window", kDefaultWindow));
}

// -------------------------------------------------------------- 窗口与系统集成

void MainWindow::restoreWindowState()
{
    Prefs &prefs = Prefs::instance();

    // 恢复位置和大小前，确认它仍然落在某块屏幕上（显示器可能已经拔掉或换了分辨率）。
    const QRect saved = prefs.windowGeometry();
    if (saved.isValid()) {
        bool visible = false;
        const QList<QScreen *> screens = QGuiApplication::screens();
        for (QScreen *sc : screens) {
            const QRect hit = sc->availableGeometry().intersected(saved);
            if (hit.width() >= 100 && hit.height() >= 60) {
                visible = true;
                break;
            }
        }
        if (visible) {
            setGeometry(saved);
        } else if (QScreen *primary = QGuiApplication::primaryScreen()) {
            // 位置已经不在任何屏幕内：保留大小（不超过屏幕），居中显示。
            const QRect avail = primary->availableGeometry();
            const QSize size = saved.size().boundedTo(avail.size());
            setGeometry(QRect(avail.center() - QPoint(size.width() / 2, size.height() / 2), size));
        }
    }
    setWindowOpacity(prefs.opacity());
    WinNative::setTopmost(this, prefs.topmost());
}

void MainWindow::saveWindowState()
{
    Prefs &prefs = Prefs::instance();
    // 最大化时记录还原后的大小，下次取消最大化能回到原来的位置。
    prefs.setWindowGeometry(isMaximized() ? normalGeometry() : geometry());
    prefs.setWindowMaximized(isMaximized());
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;         // 没有托盘时，关闭窗口就是退出

    m_tray = new QSystemTrayIcon(windowIcon(), this);
    m_tray->setToolTip(QStringLiteral("Polaris"));

    // 右键菜单与左上角按钮的菜单内容相同；每次弹出前重建，保证勾选状态是最新的。
    m_trayMenu = new QMenu(this);
    polishMenu(m_trayMenu);
    connect(m_trayMenu, &QMenu::aboutToShow, this, [this]() {
        m_trayMenu->clear();
        qDeleteAll(m_trayMenu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
        populateMenu(m_trayMenu);
    });
    populateMenu(m_trayMenu);
    m_tray->setContextMenu(m_trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)     // 左键单击切换显示
            toggleVisible();
    });
    m_tray->show();
}

void MainWindow::startup()
{
    if (Prefs::instance().windowMaximized())
        showMaximized();
    else
        show();
    WinNative::setTopmost(this, Prefs::instance().topmost());
    reportHotkeyFailures(registerHotkeys());
}

void MainWindow::toggleVisible()
{
    if (isVisible() && !isMinimized())
        hideToTray();
    else
        showFromTray();
}

void MainWindow::hideToTray()
{
    m_scene->commitEditing();
    saveWindowState();
    hide();
}

void MainWindow::showFromTray()
{
    if (isMinimized())
        setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    WinNative::bringToFront(this);
    m_view->setFocus();
}

void MainWindow::setTopmost(bool on)
{
    WinNative::setTopmost(this, on);
    Prefs::instance().setTopmost(on);
}

void MainWindow::setOpacityLevel(qreal v)
{
    v = qBound(0.3, qRound(v * 10) / 10.0, 1.0);    // 30% ~ 100%，按 10% 取整
    setWindowOpacity(v);
    Prefs::instance().setOpacity(v);
}

QStringList MainWindow::registerHotkeys()
{
    QStringList failed;
    const QVector<HotkeyDef> &defs = hotkeyDefs();
    for (int i = 0; i < defs.size(); ++i) {
        const int id = i + 1;
        WinNative::unregisterHotkey(this, id);

        const QString text = hotkeyValue(defs[i]);
        const QKeySequence seq(text, QKeySequence::PortableText);
        if (seq.isEmpty())
            continue;               // 留空 = 不启用
        const int combined = seq[0];
        const Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(combined & Qt::KeyboardModifierMask);
        const int key = combined & ~Qt::KeyboardModifierMask;
        if (!WinNative::registerHotkey(this, id, key, mods, defs[i].allowRepeat)) {
            failed << QStringLiteral("%1（%2）").arg(defs[i].label,
                                                    seq.toString(QKeySequence::NativeText));
        }
    }
    return failed;
}

void MainWindow::reportHotkeyFailures(const QStringList &failed)
{
    if (failed.isEmpty())
        return;
    const QString msg = QStringLiteral("以下全局快捷键注册失败，可能已被其他程序占用：\n%1\n"
                                       "可以在菜单的“快捷键与鼠标设置”里改用别的组合。")
                            .arg(failed.join(QLatin1Char('\n')));
    if (m_tray)
        m_tray->showMessage(QStringLiteral("Polaris"), msg, QSystemTrayIcon::Warning, 8000);
    else
        QMessageBox::warning(this, QStringLiteral("Polaris"), msg);
}

void MainWindow::onHotkey(int id)
{
    switch (id) {
    case 1: toggleVisible(); break;
    case 2: setTopmost(!Prefs::instance().topmost()); break;
    case 3: setOpacityLevel(windowOpacity() + 0.1); break;
    case 4: setOpacityLevel(windowOpacity() - 0.1); break;
    default: break;
    }
}

void MainWindow::showHotkeyDialog()
{
    const QVector<HotkeyDef> &defs = hotkeyDefs();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("快捷键与鼠标设置"));
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QFormLayout *form = new QFormLayout;
    QVector<QKeySequenceEdit *> edits;
    for (const HotkeyDef &d : defs) {
        QKeySequenceEdit *e = new QKeySequenceEdit(
            QKeySequence(hotkeyValue(d), QKeySequence::PortableText), &dlg);
        form->addRow(d.label, e);
        edits.append(e);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(&dlg);
    buttons->addButton(QStringLiteral("确定"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
    QPushButton *reset = buttons->addButton(QStringLiteral("恢复默认"), QDialogButtonBox::ResetRole);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(reset, &QPushButton::clicked, &dlg, [&]() {
        for (int i = 0; i < defs.size(); ++i)
            edits[i]->setKeySequence(QKeySequence(QString::fromLatin1(defs[i].fallback),
                                                  QKeySequence::PortableText));
    });

    // 鼠标手势：拖动画布、拖动窗口。
    auto gestureCombo = [&dlg](const QVector<MouseGesture> &choices, MouseGesture current) {
        QComboBox *box = new QComboBox(&dlg);
        for (MouseGesture g : choices)
            box->addItem(CanvasView::gestureLabel(g), CanvasView::gestureKey(g));
        box->setCurrentIndex(qMax(0, box->findData(CanvasView::gestureKey(current))));
        return box;
    };
    const QVector<MouseGesture> panChoices = { MouseGesture::SpaceLeft, MouseGesture::Right,
                                               MouseGesture::Middle, MouseGesture::CtrlLeft,
                                               MouseGesture::AltLeft };
    const QVector<MouseGesture> windowChoices = { MouseGesture::AltLeft, MouseGesture::CtrlLeft,
                                                  MouseGesture::Right, MouseGesture::Middle,
                                                  MouseGesture::SpaceLeft, MouseGesture::None };
    QComboBox *panBox = gestureCombo(panChoices, storedGesture("pan", kDefaultPan));
    QComboBox *windowBox = gestureCombo(windowChoices, storedGesture("window", kDefaultWindow));
    QFormLayout *mouseForm = new QFormLayout;
    mouseForm->addRow(QStringLiteral("拖动画布"), panBox);
    mouseForm->addRow(QStringLiteral("拖动窗口"), windowBox);

    connect(reset, &QPushButton::clicked, &dlg, [&]() {
        panBox->setCurrentIndex(panBox->findData(CanvasView::gestureKey(kDefaultPan)));
        windowBox->setCurrentIndex(windowBox->findData(CanvasView::gestureKey(kDefaultWindow)));
    });
    // 两个手势相同就分不清要拖哪个：不让确定。
    disconnect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (panBox->currentData() == windowBox->currentData()) {
            QMessageBox::warning(&dlg, QStringLiteral("Polaris"),
                                 QStringLiteral("“拖动画布”和“拖动窗口”不能设成同一种操作。"));
            return;
        }
        dlg.accept();
    });

    QLabel *keyTitle = new QLabel(QStringLiteral("全局快捷键（在任何程序里都有效）"), &dlg);
    QLabel *hint = new QLabel(QStringLiteral("点一下输入框再按下组合键；清空表示不启用。"), &dlg);
    QLabel *mouseTitle = new QLabel(QStringLiteral("鼠标操作"), &dlg);
    QLabel *mouseHint = new QLabel(
        QStringLiteral("中键拖动始终可以平移画布（除非把它设成拖动窗口）。\n"
                       "左上角的圆点始终可以拖动窗口。"), &dlg);
    QFont bold = keyTitle->font();
    bold.setBold(true);
    keyTitle->setFont(bold);
    mouseTitle->setFont(bold);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->addWidget(mouseTitle);
    layout->addLayout(mouseForm);
    layout->addWidget(mouseHint);
    layout->addSpacing(10);
    layout->addWidget(keyTitle);
    layout->addWidget(hint);
    layout->addLayout(form);
    layout->addSpacing(6);
    layout->addWidget(buttons);

    // 设置期间先注销，否则按下当前的组合键会直接触发功能而录不进去。
    for (int i = 0; i < defs.size(); ++i)
        WinNative::unregisterHotkey(this, i + 1);

    if (dlg.exec() == QDialog::Accepted) {
        for (int i = 0; i < defs.size(); ++i) {
            // 全局快捷键只取一个组合键。
            const QKeySequence seq = edits[i]->keySequence();
            const QString text = seq.isEmpty() ? QString()
                                               : QKeySequence(seq[0]).toString(QKeySequence::PortableText);
            Prefs::instance().setHotkey(QString::fromLatin1(defs[i].name), text);
        }
        Prefs::instance().setMouseGesture(QStringLiteral("pan"), panBox->currentData().toString());
        Prefs::instance().setMouseGesture(QStringLiteral("window"),
                                          windowBox->currentData().toString());
        applyMouseGestures();
    }
    const QStringList failed = registerHotkeys();
    if (!failed.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Polaris"),
                             QStringLiteral("以下全局快捷键注册失败，可能已被其他程序占用：\n%1")
                                 .arg(failed.join(QLatin1Char('\n'))));
    }
}

void MainWindow::quitApp()
{
    if (m_dirty && !isVisible())
        showFromTray();         // 让"要保存吗"的提示有个着落
    if (!maybeSave())
        return;
    if (isVisible())
        saveWindowState();
    m_quitting = true;

    // 收尾顺序：先隐藏主窗口，再注销快捷键、撤掉托盘图标，最后退出事件循环。
    hide();
    for (int i = 0; i < hotkeyDefs().size(); ++i)
        WinNative::unregisterHotkey(this, i + 1);
    if (m_tray)
        m_tray->hide();
    QApplication::quit();
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
    setDirty(true);             // 背景、主题、方向都存在文件里
    m_view->viewport()->update();
    m_menuButton->update();
    update();
}

void MainWindow::polishMenu(QMenu *menu) const
{
    // 圆角菜单：弹出窗口本身要透明、无系统阴影，否则四角会露出方形底。
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    menu->setAttribute(Qt::WA_TranslucentBackground);
    menu->setStyleSheet(Theme::current().menuStyleSheet());
    menu->setFont(QApplication::font());
}

void MainWindow::showMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    populateMenu(menu);
    menu->popup(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height() + 2)));
}

void MainWindow::populateMenu(QMenu *menu)
{
    Prefs &prefs = Prefs::instance();
    const DocSettings cur = m_scene->settings();
    polishMenu(menu);       // 主题可能变了，每次都重设样式

    auto item = [](QMenu *m, const QString &text, bool checked = false,
                   const QString &shortcut = QString()) {
        return m->addAction(checkIcon(checked),
                            shortcut.isEmpty() ? text : text + QLatin1Char('\t') + shortcut);
    };
    auto submenu = [this](QMenu *parent, const QString &text) {
        QMenu *m = parent->addMenu(checkIcon(false), text);
        polishMenu(m);
        return m;
    };
    // 菜单关闭后再执行（QueuedConnection）：对话框不要从正在关闭的弹出菜单里打开。
    auto run = [this](QAction *a, std::function<void()> fn) {
        connect(a, &QAction::triggered, this, fn, Qt::QueuedConnection);
    };

    run(item(menu, QStringLiteral("新建"), false, QStringLiteral("Ctrl+N")), [this]() { newDocument(); });
    run(item(menu, QStringLiteral("打开…"), false, QStringLiteral("Ctrl+O")), [this]() { openDocument(); });
    run(item(menu, QStringLiteral("保存"), false, QStringLiteral("Ctrl+S")), [this]() { saveDocument(); });
    run(item(menu, QStringLiteral("另存为…"), false, QStringLiteral("Ctrl+Shift+S")),
        [this]() { saveDocumentAs(); });
    run(item(menu, QStringLiteral("导出图片…")), [this]() { exportImage(); });
    menu->addSeparator();

    QMenu *bg = submenu(menu, QStringLiteral("背景"));
    const struct { const char *key; QString label; } bgs[] = {
        { "plain", QStringLiteral("纯色") },
        { "dots",  QStringLiteral("点阵") },
        { "lines", QStringLiteral("线阵") },
    };
    for (const auto &b : bgs) {
        const QString key = QString::fromLatin1(b.key);
        run(item(bg, b.label, cur.background == key), [this, key]() {
            DocSettings s = m_scene->settings();
            s.background = key;
            applySettings(s);
        });
    }

    QMenu *th = submenu(menu, QStringLiteral("主题"));
    const struct { const char *key; QString label; } ths[] = {
        { "light", QStringLiteral("浅色") },
        { "dark",  QStringLiteral("深色") },
    };
    for (const auto &x : ths) {
        const QString key = QString::fromLatin1(x.key);
        run(item(th, x.label, cur.theme == key), [this, key]() {
            DocSettings s = m_scene->settings();
            s.theme = key;
            applySettings(s);
        });
    }

    run(item(menu, QStringLiteral("连线方向"), cur.directed), [this]() {
        DocSettings s = m_scene->settings();
        s.directed = !s.directed;
        applySettings(s);
    });
    run(item(menu, QStringLiteral("对齐吸附"), prefs.snapEnabled()), []() {
        Prefs::instance().setSnapEnabled(!Prefs::instance().snapEnabled());
    });
    run(item(menu, QStringLiteral("精简动效"), prefs.reduceMotion()), []() {
        Prefs::instance().setReduceMotion(!Prefs::instance().reduceMotion());
    });
    menu->addSeparator();

    run(item(menu, QStringLiteral("窗口置顶"), prefs.topmost()),
        [this]() { setTopmost(!Prefs::instance().topmost()); });

    QMenu *op = submenu(menu, QStringLiteral("透明度"));
    const int current = qRound(windowOpacity() * 10);
    for (int level = 10; level >= 3; --level) {
        run(item(op, QStringLiteral("%1%").arg(level * 10), level == current),
            [this, level]() { setOpacityLevel(level / 10.0); });
    }

    run(item(menu, QStringLiteral("快捷键与鼠标设置…")), [this]() { showHotkeyDialog(); });
    menu->addSeparator();

    if (m_tray) {
        run(item(menu, isVisible() ? QStringLiteral("隐藏窗口") : QStringLiteral("显示窗口")),
            [this]() { toggleVisible(); });
    }
    run(item(menu, isMaximized() ? QStringLiteral("还原") : QStringLiteral("最大化")),
        [this]() {
            if (!isVisible())
                showFromTray();
            toggleMaximized();
        });
    run(item(menu, QStringLiteral("退出")), [this]() { quitApp(); });
}

// -------------------------------------------------------------------- 文件操作

QString MainWindow::docTitle() const
{
    return m_filePath.isEmpty() ? QStringLiteral("未命名") : QFileInfo(m_filePath).completeBaseName();
}

QString MainWindow::dialogDir() const
{
    if (!m_filePath.isEmpty())
        return QFileInfo(m_filePath).absolutePath();
    const QString last = Prefs::instance().lastDir();
    if (!last.isEmpty() && QDir(last).exists())
        return last;
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

Document MainWindow::currentDocument() const
{
    Document d = m_scene->toDocument();
    d.view = m_view->viewState();
    return d;
}

void MainWindow::loadIntoCanvas(const Document &doc)
{
    m_scene->loadDocument(doc);
    m_view->setViewState(doc.view);
    m_view->viewport()->update();
    m_menuButton->update();
    update();
}

void MainWindow::setDirty(bool dirty)
{
    m_dirty = dirty;
    if (dirty)
        m_draftTimer->start();
    else
        m_draftTimer->stop();
}

void MainWindow::writeDraft()
{
    if (!m_dirty)
        return;
    if (m_scene->editingNode()) {
        m_draftTimer->start();      // 正在打字：等这次编辑结束再写
        return;
    }
    Prefs &prefs = Prefs::instance();
    if (MdIo::saveFile(prefs.draftPath(), currentDocument(), docTitle()))
        prefs.setDraftSource(m_filePath);
}

void MainWindow::discardDraft()
{
    m_draftTimer->stop();
    QFile::remove(Prefs::instance().draftPath());
}

bool MainWindow::maybeSave()
{
    m_scene->commitEditing();
    if (!m_dirty)
        return true;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Polaris"));
    box.setText(QStringLiteral("“%1”有未保存的修改，要保存吗？").arg(docTitle()));
    QPushButton *save = box.addButton(QStringLiteral("保存"), QMessageBox::AcceptRole);
    QPushButton *discard = box.addButton(QStringLiteral("不保存"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(save);
    box.exec();

    if (box.clickedButton() == save)
        return saveDocument();
    if (box.clickedButton() == discard) {
        discardDraft();
        return true;
    }
    return false;
}

void MainWindow::newDocument()
{
    if (!maybeSave())
        return;
    Document d;
    d.settings = Prefs::instance().docDefaults();
    loadIntoCanvas(d);
    m_view->resetTransform();
    m_view->centerOn(0, 0);
    m_filePath.clear();
    setDirty(false);
    discardDraft();
}

void MainWindow::openDocument()
{
    if (!maybeSave())
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开"), dialogDir(), QStringLiteral("Markdown 文件 (*.md);;所有文件 (*)"));
    if (!path.isEmpty())
        openFile(path);
}

bool MainWindow::openFile(const QString &path)
{
    Document d;
    QString err;
    if (!MdIo::loadFile(path, &d, &err)) {
        QMessageBox::warning(this, QStringLiteral("Polaris"),
                             QStringLiteral("无法打开文件：\n%1\n\n%2").arg(QDir::toNativeSeparators(path), err));
        return false;
    }
    loadIntoCanvas(d);
    m_filePath = QFileInfo(path).absoluteFilePath();
    Prefs::instance().setLastFile(m_filePath);
    Prefs::instance().setLastDir(QFileInfo(m_filePath).absolutePath());
    setDirty(false);
    discardDraft();
    return true;
}

bool MainWindow::saveDocument()
{
    m_scene->commitEditing();
    if (m_filePath.isEmpty())
        return saveDocumentAs();
    return saveTo(m_filePath);
}

bool MainWindow::saveDocumentAs()
{
    m_scene->commitEditing();
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("另存为"), QDir(dialogDir()).filePath(docTitle() + QStringLiteral(".md")),
        QStringLiteral("Markdown 文件 (*.md)"));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".md");
    return saveTo(path);
}

bool MainWindow::saveTo(const QString &path)
{
    QString err;
    const QString abs = QFileInfo(path).absoluteFilePath();
    // 标题默认取文件名。
    if (!MdIo::saveFile(abs, currentDocument(), QFileInfo(abs).completeBaseName(), &err)) {
        QMessageBox::warning(this, QStringLiteral("Polaris"),
                             QStringLiteral("保存失败：\n%1\n\n%2").arg(QDir::toNativeSeparators(abs), err));
        return false;
    }
    m_filePath = abs;
    Prefs::instance().setLastFile(abs);
    Prefs::instance().setLastDir(QFileInfo(abs).absolutePath());
    setDirty(false);
    discardDraft();
    return true;
}

void MainWindow::exportImage()
{
    m_scene->commitEditing();
    if (m_scene->isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Polaris"),
                                 QStringLiteral("画布是空的，没有可导出的内容。"));
        return;
    }
    // 带不带背景用文件类型来选。
    const QString withBg = QStringLiteral("PNG 图片，带背景 (*.png)");
    const QString noBg = QStringLiteral("PNG 图片，透明背景 (*.png)");
    QString chosen = withBg;
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出图片"), QDir(dialogDir()).filePath(docTitle() + QStringLiteral(".png")),
        withBg + QStringLiteral(";;") + noBg, &chosen);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".png");

    QString err;
    if (!Exporter::exportPng(m_scene, path, chosen != noBg, &err)) {
        QMessageBox::warning(this, QStringLiteral("Polaris"),
                             QStringLiteral("导出失败：\n%1").arg(err));
        return;
    }
    Prefs::instance().setLastDir(QFileInfo(path).absolutePath());
}

bool MainWindow::checkDraftOnStartup()
{
    Prefs &prefs = Prefs::instance();
    const QFileInfo draft(prefs.draftPath());
    if (!draft.exists())
        return false;

    // 草稿比上次保存的文件新（或根本没有保存过）才询问。
    const QFileInfo last(prefs.lastFile());
    const bool newer = prefs.lastFile().isEmpty() || !last.exists()
                    || draft.lastModified() > last.lastModified();
    Document d;
    if (!newer || !MdIo::loadFile(draft.absoluteFilePath(), &d)) {
        QFile::remove(draft.absoluteFilePath());
        return false;
    }

    const QString source = prefs.draftSource();
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Polaris"));
    box.setText(QStringLiteral("上次有未保存的内容（%1，%2 个节点），要恢复吗？")
                    .arg(source.isEmpty() ? QStringLiteral("未命名")
                                          : QFileInfo(source).completeBaseName())
                    .arg(d.nodes.size()));
    QPushButton *restore = box.addButton(QStringLiteral("恢复"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("丢弃"), QMessageBox::DestructiveRole);
    box.setDefaultButton(restore);
    box.exec();

    if (box.clickedButton() == restore) {
        loadIntoCanvas(d);
        m_filePath = source;
        setDirty(true);         // 恢复出来的内容还没有保存；草稿保留到保存为止
        return true;
    }
    QFile::remove(draft.absoluteFilePath());
    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 关闭窗口（Alt+F4）只是隐藏到托盘；真正退出走菜单里的"退出"。
    if (m_tray && !m_quitting) {
        hideToTray();
        event->ignore();
        return;
    }
    if (m_quitting) {
        event->accept();
        return;
    }
    // 没有托盘可用：关闭就是退出。
    event->ignore();
    quitApp();
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    WinNative::Events ev;
    if (WinNative::handleNativeEvent(this, eventType, message, result, kResizeBorder, &ev)) {
        if (ev.hotkeyId >= 0)
            onHotkey(ev.hotkeyId);
        if (ev.activateRequest)
            showFromTray();
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setPen(Theme::current().windowBorder);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

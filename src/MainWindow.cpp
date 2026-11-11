#include "MainWindow.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "Exporter.h"
#include "MdIo.h"
#include "Prefs.h"
#include "Theme.h"
#include "WinNative.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
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
    , m_draftTimer(new QTimer(this))
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

    auto item = [menu](const QString &text, const QString &shortcut = QString()) {
        return menu->addAction(checkIcon(false),
                               shortcut.isEmpty() ? text : text + QLatin1Char('\t') + shortcut);
    };
    // 菜单关闭后再执行：文件对话框不要从正在关闭的弹出菜单里打开。
    connect(item(QStringLiteral("新建"), QStringLiteral("Ctrl+N")), &QAction::triggered, this,
            &MainWindow::newDocument, Qt::QueuedConnection);
    connect(item(QStringLiteral("打开…"), QStringLiteral("Ctrl+O")), &QAction::triggered, this,
            &MainWindow::openDocument, Qt::QueuedConnection);
    connect(item(QStringLiteral("保存"), QStringLiteral("Ctrl+S")), &QAction::triggered, this,
            [this]() { saveDocument(); }, Qt::QueuedConnection);
    connect(item(QStringLiteral("另存为…"), QStringLiteral("Ctrl+Shift+S")), &QAction::triggered,
            this, [this]() { saveDocumentAs(); }, Qt::QueuedConnection);
    connect(item(QStringLiteral("导出图片…")), &QAction::triggered, this,
            &MainWindow::exportImage, Qt::QueuedConnection);
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
    connect(quit, &QAction::triggered, this, &QWidget::close, Qt::QueuedConnection);

    menu->popup(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height() + 2)));
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
    box.setWindowTitle(QStringLiteral("TrailCanvas"));
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
        QMessageBox::warning(this, QStringLiteral("TrailCanvas"),
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
        QMessageBox::warning(this, QStringLiteral("TrailCanvas"),
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
        QMessageBox::information(this, QStringLiteral("TrailCanvas"),
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
        QMessageBox::warning(this, QStringLiteral("TrailCanvas"),
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
    box.setWindowTitle(QStringLiteral("TrailCanvas"));
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
    if (maybeSave())
        event->accept();
    else
        event->ignore();
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

#pragma once

#include "Document.h"

#include <QWidget>

class CanvasScene;
class CanvasView;
class QMenu;
class QSystemTrayIcon;
class QTimer;

// 画布左上角的小圆形按钮：平时半透明，悬停时显现。
// 按住拖动超过几个像素 = 拖窗口；单击 = clicked()；双击 = doubleClicked()。
class MenuButton : public QWidget
{
    Q_OBJECT
public:
    explicit MenuButton(QWidget *parent = nullptr);

signals:
    void clicked();
    void doubleClicked();
    void dragStarted();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    bool   m_hover = false;
    bool   m_pressed = false;
    bool   m_suppressClick = false;
    QPoint m_pressPos;
};

// 无边框主窗口：整个窗口就是画布。
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool openFile(const QString &path);     // 失败时弹出提示并返回 false
    void startup();                         // 显示窗口后调用：注册快捷键并提示冲突
    bool checkDraftOnStartup();             // 有比上次保存更新的草稿时询问；返回是否恢复了草稿

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;


private:
    // 菜单（左上角按钮和托盘右键共用同一套内容）
    void populateMenu(QMenu *menu);
    void polishMenu(QMenu *menu) const;

    // 窗口与系统集成
    void setupTray();
    void restoreWindowState();
    void saveWindowState();
    void toggleVisible();
    void hideToTray();
    void showFromTray();
    void setTopmost(bool on);
    void setOpacityLevel(qreal v);
    QStringList registerHotkeys();          // 返回注册失败的快捷键说明
    void reportHotkeyFailures(const QStringList &failed);
    void onHotkey(int id);
    void showHotkeyDialog();
    void quitApp();                         // 真正退出（关闭窗口只是隐藏到托盘）

    // 文件操作
    void newDocument();
    void openDocument();
    bool saveDocument();                    // 未命名时转到另存为
    bool saveDocumentAs();
    bool saveTo(const QString &path);
    void exportImage();
    bool maybeSave();                       // 有未保存修改时询问；返回 false 表示用户取消
    Document currentDocument() const;
    void loadIntoCanvas(const Document &doc);
    void setDirty(bool dirty);
    void writeDraft();
    void discardDraft();
    QString dialogDir() const;
    QString docTitle() const;

    void toggleMaximized();
    void showMenu();
    void applySettings(const DocSettings &s);   // 应用到画布并记入偏好

    CanvasScene *m_scene;
    CanvasView  *m_view;
    MenuButton  *m_menuButton;
    QTimer      *m_draftTimer;
    QString      m_filePath;                // 空 = 未命名
    bool         m_dirty = false;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu       *m_trayMenu = nullptr;
    bool         m_quitting = false;
};

#pragma once

#include <QWidget>

class CanvasScene;
class CanvasView;

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

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void toggleMaximized();

    CanvasScene *m_scene;
    CanvasView  *m_view;
    MenuButton  *m_menuButton;
};

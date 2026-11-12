#pragma once

#include "Document.h"

#include <QGraphicsView>

class CanvasScene;

// 可自定义的鼠标手势：用来拖动画布、拖动整个窗口。
enum class MouseGesture {
    None,           // 不启用
    SpaceLeft,      // 按住空格 + 左键拖动
    Right,          // 右键拖动
    Middle,         // 中键拖动
    CtrlLeft,       // Ctrl + 左键拖动
    AltLeft,        // Alt + 左键拖动
};

// 视图：平移缩放、背景绘制、键鼠分发。
class CanvasView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CanvasView(CanvasScene *scene, QWidget *parent = nullptr);

    static constexpr qreal kMinZoom = 0.2;
    static constexpr qreal kMaxZoom = 3.0;

    qreal zoom() const;
    void  setZoomAt(qreal zoom, const QPoint &viewportPos);   // 以视口内一点为中心缩放

    void  fitAll();                                           // Ctrl+1

    // 手势设置。拖动窗口优先于拖动画布；两者不应设成同一个。
    void setGestures(MouseGesture pan, MouseGesture windowDrag);
    static QString      gestureKey(MouseGesture g);             // 存进偏好的字符串
    static MouseGesture gestureFromKey(const QString &key, MouseGesture fallback);
    static QString      gestureLabel(MouseGesture g);           // 界面上显示的名称

    DocView viewState() const;
    void    setViewState(const DocView &v);

protected:
    bool event(QEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void updateCursor();
    bool matches(MouseGesture g, const QMouseEvent *event) const;
    static QMouseEvent shiftAsCtrl(const QMouseEvent *event);

    CanvasScene *m_scene;
    bool   m_spaceDown = false;
    bool   m_panning = false;
    bool   m_movingWindow = false;
    Qt::MouseButton m_dragButton = Qt::NoButton;    // 开始平移或拖窗口的那个键
    QPoint m_windowDragOffset;                      // 光标相对窗口左上角的偏移
    MouseGesture m_panGesture = MouseGesture::SpaceLeft;
    MouseGesture m_windowGesture = MouseGesture::AltLeft;
    bool   m_rubberBanding = false;
    bool   m_filteringSelection = false;
    QPoint m_lastPanPos;
};

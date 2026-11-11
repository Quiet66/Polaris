#pragma once

#include "Document.h"

#include <QGraphicsView>

class CanvasScene;

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
    static QMouseEvent shiftAsCtrl(const QMouseEvent *event);

    CanvasScene *m_scene;
    bool   m_spaceDown = false;
    bool   m_panning = false;
    bool   m_rubberBanding = false;
    bool   m_filteringSelection = false;
    QPoint m_lastPanPos;
};

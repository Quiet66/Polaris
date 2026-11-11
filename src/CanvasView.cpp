#include "CanvasView.h"
#include "CanvasScene.h"
#include "EdgeItem.h"
#include "Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QUndoStack>
#include <QVarLengthArray>
#include <QWheelEvent>
#include <QtMath>

constexpr qreal CanvasView::kMinZoom;
constexpr qreal CanvasView::kMaxZoom;

CanvasView::CanvasView(CanvasScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
    , m_scene(scene)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 普通 QWidget 视口（不用 OpenGL），按 SPEC 第 3 节的性能要求配置。
    setViewportUpdateMode(SmartViewportUpdate);
    setOptimizationFlags(DontSavePainterState);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    setTransformationAnchor(NoAnchor);      // 缩放锚点自行处理
    setResizeAnchor(NoAnchor);
    setDragMode(RubberBandDrag);            // 在空白处拖动 = 框选
    setFocusPolicy(Qt::StrongFocus);

    // 框选只选节点：橡皮筋扫过的连线不参与选择。
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        if (!m_rubberBanding || m_filteringSelection)
            return;
        m_filteringSelection = true;
        const QList<QGraphicsItem *> sel = m_scene->selectedItems();
        for (QGraphicsItem *it : sel) {
            if (it->type() == EdgeItem::Type)
                it->setSelected(false);
        }
        m_filteringSelection = false;
    });

    centerOn(0, 0);
}

qreal CanvasView::zoom() const
{
    return transform().m11();
}

void CanvasView::setZoomAt(qreal z, const QPoint &viewportPos)
{
    z = qBound(kMinZoom, z, kMaxZoom);
    if (qFuzzyCompare(z, zoom()))
        return;
    const QPointF anchor = mapToScene(viewportPos);
    setTransform(QTransform::fromScale(z, z));
    // 把锚点移回光标下。
    const QPoint moved = mapFromScene(anchor) - viewportPos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + moved.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + moved.y());
}

void CanvasView::fitAll()
{
    const QRectF r = m_scene->nodesBoundingRect();
    if (r.isEmpty())
        return;
    const qreal margin = 40;    // 屏幕像素
    const QSizeF vp = viewport()->size();
    const qreal z = qMin((vp.width() - 2 * margin) / r.width(),
                         (vp.height() - 2 * margin) / r.height());
    // 只为看全而缩小，不因为节点少而放大超过 100%。
    setTransform(QTransform::fromScale(qBound(kMinZoom, z, 1.0), qBound(kMinZoom, z, 1.0)));
    centerOn(r.center());
}

DocView CanvasView::viewState() const
{
    DocView v;
    const QPointF tl = mapToScene(QPoint(0, 0));
    v.x = tl.x();
    v.y = tl.y();
    v.zoom = zoom();
    return v;
}

void CanvasView::setViewState(const DocView &v)
{
    const qreal z = qBound(kMinZoom, v.zoom, kMaxZoom);
    setTransform(QTransform::fromScale(z, z));
    const QPoint moved = mapFromScene(QPointF(v.x, v.y));
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + moved.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + moved.y());
}

bool CanvasView::event(QEvent *event)
{
    // QGraphicsView / QWidget 会先把 Tab 拿去做焦点切换；这里直接当普通按键处理。
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *k = static_cast<QKeyEvent *>(event);
        if (k->key() == Qt::Key_Tab || k->key() == Qt::Key_Backtab) {
            keyPressEvent(k);
            return true;
        }
    }
    return QGraphicsView::event(event);
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    const Theme &t = Theme::current();
    painter->fillRect(rect, t.canvas);

    const QString mode = m_scene->settings().background;
    if (mode == QLatin1String("plain"))
        return;

    // 缩得很小时把密度逐级减半，保持屏幕上的间距不小于 gridMinPixelStep。
    qreal step = t.gridStep;
    while (step * zoom() < t.gridMinPixelStep)
        step *= 2;

    const qreal left = qFloor(rect.left() / step) * step;
    const qreal top  = qFloor(rect.top() / step) * step;

    if (mode == QLatin1String("lines")) {
        QVarLengthArray<QLineF, 256> lines;
        for (qreal x = left; x <= rect.right(); x += step)
            lines.append(QLineF(x, rect.top(), x, rect.bottom()));
        for (qreal y = top; y <= rect.bottom(); y += step)
            lines.append(QLineF(rect.left(), y, rect.right(), y));
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(QPen(t.grid, 0));       // 0 = 恒为 1 像素
        painter->drawLines(lines.data(), lines.size());
        painter->setRenderHint(QPainter::Antialiasing, true);
    } else {
        QVarLengthArray<QPointF, 1024> pts;
        for (qreal x = left; x <= rect.right(); x += step)
            for (qreal y = top; y <= rect.bottom(); y += step)
                pts.append(QPointF(x, y));
        QPen pen(t.grid, 2, Qt::SolidLine, Qt::RoundCap);
        pen.setCosmetic(true);                  // 点的大小不随缩放变化
        painter->setPen(pen);
        painter->drawPoints(pts.data(), pts.size());
    }
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta != 0) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QPoint pos = event->position().toPoint();
#else
        const QPoint pos = event->pos();
#endif
        setZoomAt(zoom() * qPow(1.0015, delta), pos);
    }
    event->accept();
}

void CanvasView::updateCursor()
{
    if (m_panning)
        viewport()->setCursor(Qt::ClosedHandCursor);
    else if (m_spaceDown)
        viewport()->setCursor(Qt::OpenHandCursor);
    else
        viewport()->unsetCursor();
}

// Shift+单击 = 加选或减选，Shift+框选 = 追加。QGraphicsScene 内建用的是 Ctrl，
// 而且按下、移动、释放都要看修饰键，所以整个鼠标序列都把 Shift 换成 Ctrl 再转发。
QMouseEvent CanvasView::shiftAsCtrl(const QMouseEvent *event)
{
    Qt::KeyboardModifiers mods = event->modifiers();
    if (mods & Qt::ShiftModifier)
        mods = (mods & ~Qt::ShiftModifier) | Qt::ControlModifier;
    return QMouseEvent(event->type(), event->localPos(), event->windowPos(), event->screenPos(),
                       event->button(), event->buttons(), mods, event->source());
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    const bool panButton = event->button() == Qt::MiddleButton
                        || (event->button() == Qt::LeftButton && m_spaceDown);
    if (panButton) {
        m_panning = true;
        m_lastPanPos = event->pos();
        updateCursor();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_rubberBanding = !itemAt(event->pos());
    }
    QMouseEvent e = shiftAsCtrl(event);
    QGraphicsView::mousePressEvent(&e);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPoint d = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        event->accept();
        return;
    }
    QMouseEvent e = shiftAsCtrl(event);
    QGraphicsView::mouseMoveEvent(&e);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_panning = false;
        updateCursor();
        event->accept();
        return;
    }
    QMouseEvent e = shiftAsCtrl(event);
    QGraphicsView::mouseReleaseEvent(&e);
    if (event->button() == Qt::LeftButton)
        m_rubberBanding = false;
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_spaceDown && !itemAt(event->pos())) {
        m_scene->createNodeAt(mapToScene(event->pos()));
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    // 正在编辑文字时，所有按键都交给文字图元。
    if (m_scene->editingNode()) {
        QGraphicsView::keyPressEvent(event);
        return;
    }

    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    if (ctrl) {
        switch (event->key()) {
        case Qt::Key_Z:
            if (shift)
                m_scene->undoStack()->redo();
            else
                m_scene->undoStack()->undo();
            event->accept();
            return;
        case Qt::Key_Y:
            m_scene->undoStack()->redo();
            event->accept();
            return;
        case Qt::Key_A:
            m_scene->selectAllNodes();
            event->accept();
            return;
        case Qt::Key_1:
            fitAll();
            event->accept();
            return;
        case Qt::Key_Equal:
        case Qt::Key_Plus:
            m_scene->changeFontSize(+1);
            event->accept();
            return;
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            m_scene->changeFontSize(-1);
            event->accept();
            return;
        case Qt::Key_0:
            m_scene->changeFontSize(0);
            event->accept();
            return;
        default:
            break;
        }
    }

    switch (event->key()) {
    case Qt::Key_Tab: {
        // 只在恰好选中一个节点时有效。
        const QList<NodeItem *> sel = m_scene->selectedNodes();
        if (sel.size() == 1 && m_scene->selectedItems().size() == 1)
            m_scene->createLinkedNode(sel.first());
        event->accept();
        return;
    }
    case Qt::Key_Backtab:
        event->accept();
        return;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        m_scene->deleteSelection();
        event->accept();
        return;
    case Qt::Key_Space:
        if (!event->isAutoRepeat()) {
            m_spaceDown = true;
            updateCursor();
        }
        event->accept();
        return;
    case Qt::Key_F2: {
        const QList<NodeItem *> sel = m_scene->selectedNodes();
        if (sel.size() == 1)
            sel.first()->beginEdit();
        event->accept();
        return;
    }
    default:
        break;
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat() && m_spaceDown) {
        m_spaceDown = false;
        updateCursor();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::focusOutEvent(QFocusEvent *event)
{
    // 失去焦点后收不到按键释放，这里复位空格和平移状态。
    m_spaceDown = false;
    m_panning = false;
    updateCursor();
    QGraphicsView::focusOutEvent(event);
}

bool CanvasView::focusNextPrevChild(bool)
{
    return false;       // Tab 不用于焦点切换，由画布自行处理
}

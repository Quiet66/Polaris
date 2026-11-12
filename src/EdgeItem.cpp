#include "EdgeItem.h"
#include "CanvasScene.h"
#include "NodeItem.h"
#include "Theme.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

EdgeGeometry EdgeGeometry::build(const QRectF &a, const QRectF &b, bool directed)
{
    const Theme &t = Theme::current();
    const QPointF ca = a.center();
    const QPointF cb = b.center();
    const qreal dx = cb.x() - ca.x();
    const qreal dy = cb.y() - ca.y();

    // 两个矩形之间的水平、垂直间隙（<= 0 表示在该方向上的投影有重叠）。
    const qreal gapX = qMax(b.left() - a.right(), a.left() - b.right());
    const qreal gapY = qMax(b.top() - a.bottom(), a.top() - b.bottom());

    // 左右分开时优先走左右两边（节点通常宽而扁，路径也多是从左往右展开）；
    // 只有上下分开时走上下两边；互相重叠时按中心距离较大的方向。
    bool horizontal;
    if (gapX > 0 && gapY > 0)
        horizontal = gapX >= 24 || gapX >= gapY * 0.5;
    else if (gapX > 0 || gapY > 0)
        horizontal = gapX > 0;
    else
        horizontal = qAbs(dx) >= qAbs(dy);

    QPointF p1, p2, d1, d2;     // 端点，及端点处朝外的方向
    if (horizontal) {
        const qreal s = dx >= 0 ? 1 : -1;
        p1 = QPointF(s > 0 ? a.right() : a.left(), ca.y());
        p2 = QPointF(s > 0 ? b.left() : b.right(), cb.y());
        d1 = QPointF(s, 0);
        d2 = QPointF(-s, 0);
    } else {
        const qreal s = dy >= 0 ? 1 : -1;
        p1 = QPointF(ca.x(), s > 0 ? a.bottom() : a.top());
        p2 = QPointF(cb.x(), s > 0 ? b.top() : b.bottom());
        d1 = QPointF(0, s);
        d2 = QPointF(0, -s);
    }

    EdgeGeometry g;
    QPointF end = p2;
    if (directed) {
        const QPointF back = p2 + d2 * t.arrowLength;
        const QPointF side(d2.y() * t.arrowHalfWidth, -d2.x() * t.arrowHalfWidth);
        g.arrow << p2 << back + side << back - side;
        end = p2 + d2 * (t.arrowLength * 0.7);      // 曲线止于箭头内部，线头不会从箭尖穿出
    }

    const qreal span = qAbs(horizontal ? end.x() - p1.x() : end.y() - p1.y());
    const qreal k = qBound<qreal>(18, span * 0.5, 140);
    g.curve.moveTo(p1);
    g.curve.cubicTo(p1 + d1 * k, end + d2 * k, end);
    return g;
}

EdgeItem::EdgeItem(const EdgeData &data, NodeItem *from, NodeItem *to)
    : m_data(data)
    , m_from(from)
    , m_to(to)
{
    setFlag(ItemIsSelectable, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setZValue(-1);      // 画在节点下面
}

void EdgeItem::updateGeometry()
{
    const CanvasScene *s = qobject_cast<CanvasScene *>(scene());
    const bool directed = s ? s->settings().directed : true;
    const Theme &t = Theme::current();

    prepareGeometryChange();
    m_geo = EdgeGeometry::build(m_from->sceneBodyRect(), m_to->sceneBodyRect(), directed);

    // 可点击范围比可见线宽大得多。
    QPainterPathStroker stroker;
    stroker.setWidth(t.edgeHitWidth);
    stroker.setCapStyle(Qt::RoundCap);
    m_shape = stroker.createStroke(m_geo.curve);
    if (!m_geo.arrow.isEmpty())
        m_shape.addPolygon(m_geo.arrow);

    const qreal m = t.edgeActiveWidth + 1;
    m_bounds = m_shape.boundingRect().adjusted(-m, -m, m, m);
    update();
}

QRectF EdgeItem::boundingRect() const
{
    return m_bounds;
}

QPainterPath EdgeItem::shape() const
{
    return m_shape;
}

void EdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    const Theme &t = Theme::current();
    const bool selected = option->state & QStyle::State_Selected;
    const CanvasScene *cs = qobject_cast<CanvasScene *>(scene());
    const bool active = (selected || m_hover) && !(cs && cs->isExporting());
    const QColor color = active ? t.accent : t.edge;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(color, active ? t.edgeActiveWidth : t.edgeWidth,
                         Qt::SolidLine, Qt::RoundCap));
    painter->setBrush(Qt::NoBrush);
    if (m_progress < 1) {
        // 描线动画：只画曲线的前一段，箭头等画完再出现。
        const int steps = 24;
        QPainterPath part(m_geo.curve.pointAtPercent(0));
        for (int i = 1; i <= steps; ++i)
            part.lineTo(m_geo.curve.pointAtPercent(m_progress * i / steps));
        painter->drawPath(part);
        return;
    }
    painter->drawPath(m_geo.curve);

    if (!m_geo.arrow.isEmpty()) {
        painter->setPen(QPen(color, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(color);
        painter->drawPolygon(m_geo.arrow);
    }
}

void EdgeItem::setDrawProgress(qreal p)
{
    m_progress = qBound<qreal>(0, p, 1);
    update();
}

void EdgeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    m_hover = true;
    update();
}

void EdgeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    m_hover = false;
    update();
}

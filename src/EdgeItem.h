#pragma once

#include "Document.h"

#include <QGraphicsItem>
#include <QPainterPath>
#include <QPolygonF>

class NodeItem;

// 连线的几何：一条三次曲线，加一个可选的箭头。
struct EdgeGeometry
{
    QPainterPath curve;
    QPolygonF    arrow;

    // 端点取两个矩形相对的两条边的中点；directed 时在终点画箭头。
    static EdgeGeometry build(const QRectF &from, const QRectF &to, bool directed);
};

// 连线图元：平滑曲线、箭头、放大的命中范围。坐标即场景坐标（自身 pos 恒为 0,0）。
class EdgeItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 2 };

    EdgeItem(const EdgeData &data, NodeItem *from, NodeItem *to);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    const EdgeData &data() const { return m_data; }
    QString   id() const { return m_data.id; }
    NodeItem *fromNode() const { return m_from; }
    NodeItem *toNode() const { return m_to; }

    void updateGeometry();      // 端点节点移动、改变大小，或方向开关变化后调用

    // 描线动画进度：0 = 还没画，1 = 完整。只影响绘制，不影响命中范围。
    void setDrawProgress(qreal p);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    EdgeData     m_data;
    NodeItem    *m_from;
    NodeItem    *m_to;
    EdgeGeometry m_geo;
    QPainterPath m_shape;
    QRectF       m_bounds;
    bool         m_hover = false;
    qreal        m_progress = 1;
};

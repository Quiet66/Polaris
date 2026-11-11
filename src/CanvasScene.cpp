#include "CanvasScene.h"
#include "Commands.h"
#include "EdgeItem.h"
#include "Theme.h"

#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <QPointer>
#include <QTimer>
#include <QUndoStack>

#include <algorithm>

CanvasScene::CanvasScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_undo(new QUndoStack(this))
{
    // 画布逻辑上无限大；固定一个足够大的范围，避免场景范围随节点变化而抖动。
    setSceneRect(-100000, -100000, 200000, 200000);
    // 节点经常移动，BSP 索引的维护成本高于收益。
    setItemIndexMethod(NoIndex);

    m_undo->setUndoLimit(200);
    connect(m_undo, &QUndoStack::indexChanged, this, &CanvasScene::modified);
}

void CanvasScene::setSettings(const DocSettings &s)
{
    m_settings = s;
    Theme::setCurrent(s.theme);
    applyTheme();
}

// ------------------------------------------------------------------------ 查询

QList<NodeItem *> CanvasScene::nodes() const
{
    return m_nodes.values();
}

QList<EdgeItem *> CanvasScene::edges() const
{
    return m_edges.values();
}

QList<NodeItem *> CanvasScene::selectedNodes() const
{
    QList<NodeItem *> out;
    const QList<QGraphicsItem *> sel = selectedItems();
    for (QGraphicsItem *it : sel) {
        if (NodeItem *n = qgraphicsitem_cast<NodeItem *>(it))
            out.append(n);
    }
    return out;
}

EdgeItem *CanvasScene::edgeBetween(const NodeItem *a, const NodeItem *b) const
{
    for (EdgeItem *e : a->edges()) {
        if ((e->fromNode() == a && e->toNode() == b) || (e->fromNode() == b && e->toNode() == a))
            return e;
    }
    return nullptr;
}

NodeItem *CanvasScene::nodeAt(const QPointF &scenePos) const
{
    const QList<QGraphicsItem *> hit = items(scenePos);
    for (QGraphicsItem *it : hit) {
        if (NodeItem *n = qgraphicsitem_cast<NodeItem *>(it))
            return n;
        if (NodeItem *n = qgraphicsitem_cast<NodeItem *>(it->parentItem()))
            return n;
    }
    return nullptr;
}

NodeItem *CanvasScene::editingNode() const
{
    QGraphicsItem *f = focusItem();
    if (!f)
        return nullptr;
    NodeItem *n = qgraphicsitem_cast<NodeItem *>(f->parentItem());
    return (n && n->isEditing()) ? n : nullptr;
}

QRectF CanvasScene::nodesBoundingRect() const
{
    QRectF r;
    for (NodeItem *n : m_nodes)
        r |= n->sceneBodyRect();
    return r;
}

// -------------------------------------------------------------------- 底层操作

NodeItem *CanvasScene::addNode(const NodeData &data)
{
    if (data.id.isEmpty() || m_nodes.contains(data.id))
        return nullptr;
    NodeItem *n = new NodeItem(data);
    addItem(n);
    m_nodes.insert(data.id, n);
    return n;
}

EdgeItem *CanvasScene::addEdge(const EdgeData &data)
{
    NodeItem *from = m_nodes.value(data.from);
    NodeItem *to = m_nodes.value(data.to);
    if (data.id.isEmpty() || m_edges.contains(data.id) || !from || !to || from == to
        || edgeBetween(from, to))
        return nullptr;
    EdgeItem *e = new EdgeItem(data, from, to);
    addItem(e);
    m_edges.insert(data.id, e);
    from->attachEdge(e);
    to->attachEdge(e);
    e->updateGeometry();
    return e;
}

void CanvasScene::removeEdge(const QString &id)
{
    EdgeItem *e = m_edges.take(id);
    if (!e)
        return;
    e->fromNode()->detachEdge(e);
    e->toNode()->detachEdge(e);
    removeItem(e);
    delete e;
}

void CanvasScene::removeNode(const QString &id)
{
    NodeItem *n = m_nodes.value(id);
    if (!n)
        return;
    const QList<EdgeItem *> attached = n->edges();
    for (EdgeItem *e : attached)
        removeEdge(e->id());
    if (n == m_linkFrom)
        m_linkFrom = nullptr;
    m_nodes.remove(id);
    removeItem(n);
    delete n;
}

void CanvasScene::setNodePos(const QString &id, const QPointF &pos)
{
    if (NodeItem *n = m_nodes.value(id))
        n->setPos(pos);
}

void CanvasScene::setNodeText(const QString &id, const QString &text)
{
    if (NodeItem *n = m_nodes.value(id))
        n->setText(text);
}

void CanvasScene::setNodeStyle(const NodeData &d)
{
    if (NodeItem *n = m_nodes.value(d.id)) {
        n->setPos(d.x, d.y);
        n->setStyle(d.w, d.fontSize);
    }
}

// -------------------------------------------------------------------- 用户操作

void CanvasScene::changeFontSize(int steps)
{
    const Theme &t = Theme::current();
    QVector<NodeChange> changes;
    const QList<NodeItem *> sel = selectedNodes();
    for (NodeItem *n : sel) {
        NodeChange c;
        c.before = c.after = n->data();
        if (steps == 0) {
            c.after.fontSize = 0;
        } else {
            const qreal cur = c.before.fontSize > 0 ? c.before.fontSize : t.defaultFontSize;
            c.after.fontSize = qBound(t.minFontSize, cur + steps, t.maxFontSize);
        }
        if (!qFuzzyCompare(c.after.fontSize + 1, c.before.fontSize + 1))
            changes.append(c);
    }
    if (!changes.isEmpty())
        m_undo->push(new ChangeNodesCommand(this, changes, false, QStringLiteral("字号")));
}

void CanvasScene::nodeResized(const NodeData &before, const NodeData &after)
{
    if (qFuzzyCompare(before.w + 1, after.w + 1) && qFuzzyCompare(before.x + 1, after.x + 1))
        return;
    QVector<NodeChange> changes;
    changes.append(NodeChange{before, after});
    m_undo->push(new ChangeNodesCommand(this, changes, true, QStringLiteral("调整宽度")));
}

void CanvasScene::startPendingNode(const QPointF &topLeft, NodeItem *from)
{
    NodeData d;
    d.id = Document::newId();
    d.createdAt = Document::now();
    d.x = topLeft.x();
    d.y = topLeft.y();
    NodeItem *n = addNode(d);

    m_pendingNodeId = d.id;
    m_pendingEdgeId.clear();
    if (from) {
        EdgeData e;
        e.id = Document::newId();
        e.from = from->id();
        e.to = d.id;
        e.createdAt = d.createdAt;
        if (addEdge(e))
            m_pendingEdgeId = e.id;
    }

    clearSelection();
    n->setSelected(true);
    n->beginEdit();
}

void CanvasScene::createNodeAt(const QPointF &scenePos)
{
    // 让该点落在文字起始处、垂直居中。
    const Theme &t = Theme::current();
    const qreal h = NodeItem(NodeData()).bodyRect().height();
    startPendingNode(QPointF(scenePos.x() - t.nodePadX, scenePos.y() - h / 2), nullptr);
}

QPointF CanvasScene::linkedNodePos(const NodeItem *from) const
{
    // 放在来源节点右侧固定间距处；与已有节点重叠时依次向下错开。
    const Theme &t = Theme::current();
    const QRectF src = from->sceneBodyRect();
    const qreal h = NodeItem(NodeData()).bodyRect().height();
    QRectF r(src.right() + t.linkedNodeGap, src.center().y() - h / 2, 120, h);

    for (int guard = 0; guard < 500; ++guard) {
        bool overlap = false;
        qreal nextTop = r.top();
        for (NodeItem *n : m_nodes) {
            const QRectF o = n->sceneBodyRect().adjusted(-4, -t.linkedNodeStepGap + 1,
                                                         4, t.linkedNodeStepGap - 1);
            if (o.intersects(r)) {
                overlap = true;
                nextTop = qMax(nextTop, n->sceneBodyRect().bottom() + t.linkedNodeStepGap);
            }
        }
        if (!overlap)
            break;
        r.moveTop(nextTop);
    }
    return r.topLeft();
}

void CanvasScene::createLinkedNode(NodeItem *from)
{
    if (!from)
        return;
    startPendingNode(linkedNodePos(from), from);
}

void CanvasScene::deleteSelection()
{
    ItemsSnapshot snap;
    QSet<QString> edgeIds;
    auto takeEdge = [&](EdgeItem *e) {
        if (!edgeIds.contains(e->id())) {
            edgeIds.insert(e->id());
            snap.edges.append(e->data());
        }
    };

    const QList<QGraphicsItem *> sel = selectedItems();
    for (QGraphicsItem *it : sel) {
        if (NodeItem *n = qgraphicsitem_cast<NodeItem *>(it)) {
            snap.nodes.append(n->data());
            for (EdgeItem *e : n->edges())
                takeEdge(e);
        } else if (EdgeItem *e = qgraphicsitem_cast<EdgeItem *>(it)) {
            takeEdge(e);        // 只选中线时：只删线，两端节点不动
        }
    }
    if (snap.isEmpty())
        return;
    m_undo->push(new RemoveItemsCommand(this, snap, QStringLiteral("删除")));
}

void CanvasScene::selectAllNodes()
{
    clearSelection();
    for (NodeItem *n : qAsConst(m_nodes))
        n->setSelected(true);
}

// ------------------------------------------------------------------ 拖出连线

void CanvasScene::beginLinkDrag(NodeItem *from, const QPointF &scenePos)
{
    m_linkFrom = from;
    if (!m_linkPreview) {
        m_linkPreview = new QGraphicsPathItem;
        m_linkPreview->setZValue(10);
        m_linkPreview->setAcceptedMouseButtons(Qt::NoButton);
        addItem(m_linkPreview);
    }
    QPen pen(Theme::current().accent, Theme::current().edgeWidth, Qt::DashLine, Qt::RoundCap);
    m_linkPreview->setPen(pen);
    m_linkPreview->setVisible(true);
    updateLinkDrag(scenePos);
}

void CanvasScene::updateLinkDrag(const QPointF &scenePos)
{
    if (!m_linkFrom || !m_linkPreview)
        return;
    const EdgeGeometry g = EdgeGeometry::build(m_linkFrom->sceneBodyRect(),
                                               QRectF(scenePos, QSizeF(0, 0)), false);
    m_linkPreview->setPath(g.curve);
}

void CanvasScene::endLinkDrag(const QPointF &scenePos)
{
    NodeItem *from = m_linkFrom;
    m_linkFrom = nullptr;
    if (m_linkPreview)
        m_linkPreview->setVisible(false);
    if (!from)
        return;

    NodeItem *target = nodeAt(scenePos);
    if (target == from)
        return;

    if (target) {
        if (edgeBetween(from, target))
            return;             // 两个节点之间至多一条线
        ItemsSnapshot snap;
        EdgeData e;
        e.id = Document::newId();
        e.from = from->id();
        e.to = target->id();
        e.createdAt = Document::now();
        snap.edges.append(e);
        m_undo->push(new AddItemsCommand(this, snap, false, QStringLiteral("连线")));
    } else {
        // 拖到空白处：在该处新建节点并连线，进入编辑。
        const Theme &t = Theme::current();
        const qreal h = NodeItem(NodeData()).bodyRect().height();
        startPendingNode(QPointF(scenePos.x() - t.nodePadX, scenePos.y() - h / 2), from);
    }
}

// ------------------------------------------------------------------ 编辑结束

void CanvasScene::nodeEditFinished(NodeItem *node, NodeItem::EditEnd how, const QString &oldText)
{
    const QString id = node->id();
    const bool pending = (id == m_pendingNodeId);
    const QString pendingEdge = pending ? m_pendingEdgeId : QString();
    if (pending) {
        m_pendingNodeId.clear();
        m_pendingEdgeId.clear();
    }

    if (node->text().isEmpty()) {
        // 确认时文字为空：删除该节点。此刻仍在节点自己的事件处理函数里，推迟到事件结束后再删。
        node->setVisible(false);
        if (pending) {
            // 刚新建还没写入撤销栈，直接丢弃（连同自动建的线）。
            QTimer::singleShot(0, this, [this, id]() { removeNode(id); });
        } else {
            ItemsSnapshot snap;
            NodeData d = node->data();
            d.text = oldText;           // 撤销时恢复编辑前的文字
            snap.nodes.append(d);
            for (EdgeItem *e : node->edges())
                snap.edges.append(e->data());
            QTimer::singleShot(0, this, [this, snap]() {
                m_undo->push(new RemoveItemsCommand(this, snap, QStringLiteral("删除")));
            });
        }
        return;
    }

    if (pending) {
        ItemsSnapshot snap;
        snap.nodes.append(node->data());
        if (EdgeItem *e = m_edges.value(pendingEdge))
            snap.edges.append(e->data());
        m_undo->push(new AddItemsCommand(this, snap, true, QStringLiteral("新建节点")));
    } else if (node->text() != oldText) {
        m_undo->push(new EditTextCommand(this, id, oldText, node->text()));
    }

    if (how == NodeItem::CommitAndChain) {
        // 编辑中按 Tab：确认，并以该节点为起点再建一个关联节点。
        QTimer::singleShot(0, this, [this, id]() { createLinkedNode(m_nodes.value(id)); });
    }
}

// ------------------------------------------------------------ 拖动 = 一步撤销

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mousePressEvent(event);
    m_dragStart.clear();
    // 正在拖连线或拖宽度手柄时，不当作移动节点。
    const NodeItem *grabber = qgraphicsitem_cast<NodeItem *>(mouseGrabberItem());
    if (event->button() == Qt::LeftButton && !(grabber && grabber->isBusy())) {
        const QList<NodeItem *> sel = selectedNodes();
        for (NodeItem *n : sel)
            m_dragStart.insert(n->id(), n->pos());
    }
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton || m_dragStart.isEmpty())
        return;

    QVector<NodeMove> moves;
    for (auto it = m_dragStart.constBegin(); it != m_dragStart.constEnd(); ++it) {
        NodeItem *n = m_nodes.value(it.key());
        if (n && n->pos() != it.value())
            moves.append(NodeMove{it.key(), it.value(), n->pos()});
    }
    m_dragStart.clear();
    if (!moves.isEmpty())
        m_undo->push(new MoveNodesCommand(this, moves, true));
}

// ------------------------------------------------------------------ 文档互转

Document CanvasScene::toDocument() const
{
    Document d;
    d.settings = m_settings;
    for (NodeItem *n : m_nodes) {
        if (n->id() != m_pendingNodeId)
            d.nodes.append(n->data());
    }
    for (EdgeItem *e : m_edges) {
        if (e->id() != m_pendingEdgeId)
            d.edges.append(e->data());
    }
    std::sort(d.nodes.begin(), d.nodes.end(), [](const NodeData &a, const NodeData &b) {
        return a.createdAt != b.createdAt ? a.createdAt < b.createdAt : a.id < b.id;
    });
    std::sort(d.edges.begin(), d.edges.end(), [](const EdgeData &a, const EdgeData &b) {
        return a.createdAt != b.createdAt ? a.createdAt < b.createdAt : a.id < b.id;
    });
    return d;
}

void CanvasScene::clearAll()
{
    m_undo->clear();
    m_pendingNodeId.clear();
    m_pendingEdgeId.clear();
    m_dragStart.clear();
    m_linkFrom = nullptr;
    m_linkPreview = nullptr;    // clear() 会连它一起删除
    m_nodes.clear();
    m_edges.clear();
    clear();
}

void CanvasScene::loadDocument(const Document &doc)
{
    clearAll();
    m_settings = doc.settings;
    Theme::setCurrent(m_settings.theme);
    for (const NodeData &n : doc.nodes)
        addNode(n);
    for (const EdgeData &e : doc.edges)
        addEdge(e);
    update();
}

void CanvasScene::applyTheme()
{
    for (NodeItem *n : qAsConst(m_nodes))
        n->relayout();
    for (EdgeItem *e : qAsConst(m_edges))
        e->updateGeometry();
    update();
}

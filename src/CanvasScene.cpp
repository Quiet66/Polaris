#include "CanvasScene.h"
#include "Theme.h"

#include <QTimer>

CanvasScene::CanvasScene(QObject *parent)
    : QGraphicsScene(parent)
{
    // 画布逻辑上无限大；固定一个足够大的范围，避免场景范围随节点变化而抖动。
    setSceneRect(-100000, -100000, 200000, 200000);
    // 节点经常移动，BSP 索引的维护成本高于收益。
    setItemIndexMethod(NoIndex);
}

void CanvasScene::setSettings(const DocSettings &s)
{
    m_settings = s;
    Theme::setCurrent(s.theme);
    applyTheme();
}

QList<NodeItem *> CanvasScene::nodes() const
{
    QList<NodeItem *> out;
    const QList<QGraphicsItem *> all = items();
    for (QGraphicsItem *it : all) {
        if (NodeItem *n = qgraphicsitem_cast<NodeItem *>(it))
            out.append(n);
    }
    return out;
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

NodeItem *CanvasScene::editingNode() const
{
    QGraphicsItem *f = focusItem();
    if (!f)
        return nullptr;
    NodeItem *n = qgraphicsitem_cast<NodeItem *>(f->parentItem());
    return (n && n->isEditing()) ? n : nullptr;
}

NodeItem *CanvasScene::addNode(const NodeData &data)
{
    NodeItem *n = new NodeItem(data);
    addItem(n);
    return n;
}

void CanvasScene::removeNode(NodeItem *node)
{
    if (!node || node->scene() != this)
        return;
    removeItem(node);
    delete node;
    emit modified();
}

NodeItem *CanvasScene::createNodeAt(const QPointF &scenePos)
{
    NodeData d;
    d.id = Document::newId();
    d.createdAt = Document::now();
    NodeItem *n = addNode(d);
    // 让双击点落在文字起始处、垂直居中。
    const QRectF r = n->bodyRect();
    n->setPos(scenePos.x() - Theme::current().nodePadX, scenePos.y() - r.height() / 2);

    clearSelection();
    n->setSelected(true);
    n->beginEdit();
    return n;
}

void CanvasScene::nodeEditFinished(NodeItem *node, NodeItem::EditEnd how, const QString &oldText)
{
    Q_UNUSED(how)   // CommitAndChain（编辑中按 Tab 再建关联节点）在 M2 实现

    if (node->text().isEmpty()) {
        // 确认时文字为空则删除该节点。此刻仍在节点自己的事件处理函数里，推迟到事件结束后再删。
        node->setVisible(false);
        QTimer::singleShot(0, this, [this, node]() { removeNode(node); });
        return;
    }
    if (node->text() != oldText)
        emit modified();
}

Document CanvasScene::toDocument() const
{
    Document d;
    d.settings = m_settings;
    const QList<NodeItem *> ns = nodes();
    for (NodeItem *n : ns)
        d.nodes.append(n->data());
    std::sort(d.nodes.begin(), d.nodes.end(), [](const NodeData &a, const NodeData &b) {
        return a.createdAt != b.createdAt ? a.createdAt < b.createdAt : a.id < b.id;
    });
    return d;
}

void CanvasScene::clearAll()
{
    clear();
}

void CanvasScene::loadDocument(const Document &doc)
{
    clearAll();
    m_settings = doc.settings;
    Theme::setCurrent(m_settings.theme);
    for (const NodeData &n : doc.nodes)
        addNode(n);
    update();
}

void CanvasScene::applyTheme()
{
    const QList<NodeItem *> ns = nodes();
    for (NodeItem *n : ns)
        n->relayout();
    update();
}

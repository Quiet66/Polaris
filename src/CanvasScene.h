#pragma once

#include "Document.h"
#include "NodeItem.h"

#include <QGraphicsScene>
#include <QHash>
#include <QList>
#include <QPointF>

class EdgeItem;
class QGraphicsPathItem;
class QUndoStack;

// 场景：节点和连线的增删、选择。场景中的图元是编辑时的唯一数据来源，
// toDocument() / loadDocument() 负责与数据模型互转。
class CanvasScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit CanvasScene(QObject *parent = nullptr);

    const DocSettings &settings() const { return m_settings; }
    void setSettings(const DocSettings &s);

    QUndoStack *undoStack() const { return m_undo; }

    // ---- 查询
    QList<NodeItem *> nodes() const;
    QList<EdgeItem *> edges() const;
    QList<NodeItem *> selectedNodes() const;
    NodeItem *nodeById(const QString &id) const { return m_nodes.value(id); }
    EdgeItem *edgeById(const QString &id) const { return m_edges.value(id); }
    EdgeItem *edgeBetween(const NodeItem *a, const NodeItem *b) const;   // 不分方向
    NodeItem *nodeAt(const QPointF &scenePos) const;
    NodeItem *editingNode() const;
    QRectF    nodesBoundingRect() const;

    // ---- 底层操作：直接改场景，不进撤销栈（供 Commands 和载入使用）
    NodeItem *addNode(const NodeData &data);
    EdgeItem *addEdge(const EdgeData &data);    // 自环、重复、端点不存在时返回 nullptr
    void      removeNode(const QString &id);    // 连同与它相连的线
    void      removeEdge(const QString &id);
    void      setNodePos(const QString &id, const QPointF &pos);
    void      setNodeText(const QString &id, const QString &text);
    void      setNodeStyle(const NodeData &d);  // 按 d.id 应用 w、fontSize、x、y

    // ---- 用户操作：进撤销栈
    void createNodeAt(const QPointF &scenePos);     // 双击空白处
    void createLinkedNode(NodeItem *from);          // Tab
    void deleteSelection();                         // Delete / Backspace
    void selectAllNodes();                          // Ctrl+A
    void changeFontSize(int steps);                 // Ctrl+= / Ctrl+-；0 = 恢复默认（Ctrl+0）
    void nodeResized(const NodeData &before, const NodeData &after);   // 拖完宽度手柄

    // 从连接点拖线（由 NodeItem 调用）
    void beginLinkDrag(NodeItem *from, const QPointF &scenePos);
    void updateLinkDrag(const QPointF &scenePos);
    void endLinkDrag(const QPointF &scenePos);

    // 由 NodeItem 在编辑结束时调用。
    void nodeEditFinished(NodeItem *node, NodeItem::EditEnd how, const QString &oldText);

    Document toDocument() const;
    void     loadDocument(const Document &doc);
    void     clearAll();

    void applyTheme();      // 主题或方向开关变化后刷新所有图元

signals:
    void modified();        // 内容发生变化（供之后的草稿、未保存标记使用）

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    // 新建一个空节点（可选地自动连上 from）并进入编辑。它在编辑确认前是"待定"的：
    // 确认时文字非空才作为一步写入撤销栈，否则连同自动建的线一起丢弃。
    void startPendingNode(const QPointF &topLeft, NodeItem *from);
    QPointF linkedNodePos(const NodeItem *from) const;

    DocSettings m_settings;
    QUndoStack *m_undo;
    QHash<QString, NodeItem *> m_nodes;
    QHash<QString, EdgeItem *> m_edges;

    QString m_pendingNodeId;
    QString m_pendingEdgeId;

    QHash<QString, QPointF> m_dragStart;        // 拖动开始时被选中节点的位置

    NodeItem          *m_linkFrom = nullptr;
    QGraphicsPathItem *m_linkPreview = nullptr;
};

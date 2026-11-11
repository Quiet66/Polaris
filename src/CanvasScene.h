#pragma once

#include "Document.h"
#include "NodeItem.h"

#include <QGraphicsScene>
#include <QList>

// 场景：节点（以及之后的连线）的增删、选择。场景中的图元是编辑时的唯一数据来源，
// toDocument() / loadDocument() 负责与数据模型互转。
class CanvasScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit CanvasScene(QObject *parent = nullptr);

    const DocSettings &settings() const { return m_settings; }
    void setSettings(const DocSettings &s);

    QList<NodeItem *> nodes() const;
    QList<NodeItem *> selectedNodes() const;
    NodeItem *editingNode() const;

    NodeItem *addNode(const NodeData &data);
    void      removeNode(NodeItem *node);

    // 在 scenePos 处新建空节点并进入编辑（双击空白处）。
    NodeItem *createNodeAt(const QPointF &scenePos);

    // 由 NodeItem 在编辑结束时调用。
    void nodeEditFinished(NodeItem *node, NodeItem::EditEnd how, const QString &oldText);

    Document toDocument() const;
    void     loadDocument(const Document &doc);
    void     clearAll();

    void applyTheme();      // 主题变化后刷新所有图元

signals:
    void modified();        // 内容发生变化（供之后的草稿、未保存标记使用）

private:
    DocSettings m_settings;
};

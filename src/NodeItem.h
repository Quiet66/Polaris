#pragma once

#include "Document.h"

#include <QGraphicsItem>
#include <QGraphicsTextItem>

class CanvasScene;
class EdgeItem;
class NodeItem;

// 节点内的文字编辑器。平时隐藏（文字由 NodeItem 绘制），编辑时显示并切换为 TextEditorInteraction。
class NodeTextItem : public QGraphicsTextItem
{
public:
    explicit NodeTextItem(NodeItem *owner);

protected:
    bool sceneEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    NodeItem *m_owner;
    bool      m_composing = false;    // 输入法正在组词
};

// 节点图元：圆角矩形 + 文字。pos() 是矩形左上角（即 doc 中的 x, y）。
class NodeItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 1 };
    enum EditEnd { Commit, Cancel, CommitAndChain };   // CommitAndChain：编辑中按 Tab

    explicit NodeItem(const NodeData &data);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    NodeData data() const;
    QString  id() const { return m_data.id; }
    QString  text() const { return m_data.text; }
    void     setText(const QString &text);
    void     setStyle(qreal w, qreal fontSize);     // <= 0 表示 null
    bool     isBusy() const { return m_linking || m_resizeSide != 0; }
    QRectF   bodyRect() const { return QRectF(QPointF(0, 0), m_size); }
    QRectF   sceneBodyRect() const { return QRectF(pos(), m_size); }

    // 与本节点相连的线，由 CanvasScene 在增删连线时维护。
    const QList<EdgeItem *> &edges() const { return m_edges; }
    void attachEdge(EdgeItem *e) { m_edges.append(e); }
    void detachEdge(EdgeItem *e) { m_edges.removeAll(e); }

    bool isEditing() const { return m_editing; }
    void beginEdit();
    void endEdit(EditEnd how);

    void relayout();        // 文字、字号、宽度或主题变化后重新计算尺寸

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    CanvasScene *canvasScene() const;
    QPointF portCenter(int index) const;        // 0 右 1 下 2 左 3 上
    int     portAt(const QPointF &localPos) const;
    int     gripAt(const QPointF &localPos) const;      // -1 左手柄，+1 右手柄，0 无
    void    updateEdges();

    QList<EdgeItem *> m_edges;
    bool          m_hover = false;
    bool          m_linking = false;            // 正从连接点拖出连线
    int           m_resizeSide = 0;             // 正在拖宽度手柄：-1 左，+1 右
    NodeData      m_resizeStart;
    qreal         m_resizeStartWidth = 0;       // 开始拖动时的实际宽度（w 为 null 时也有值）

    NodeData      m_data;
    NodeTextItem *m_text;
    QSizeF        m_size;
    bool          m_editing = false;
    QString       m_textBeforeEdit;
};

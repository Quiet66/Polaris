#pragma once

#include "Document.h"

#include <QGraphicsItem>
#include <QGraphicsTextItem>

class CanvasScene;
class NodeItem;

// 节点内的文字。平时只显示，不接收鼠标；编辑时切换为 TextEditorInteraction。
class NodeTextItem : public QGraphicsTextItem
{
public:
    explicit NodeTextItem(NodeItem *owner);

protected:
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
    QRectF   bodyRect() const { return QRectF(QPointF(0, 0), m_size); }

    bool isEditing() const { return m_editing; }
    void beginEdit();
    void endEdit(EditEnd how);

    void relayout();        // 文字、字号、宽度或主题变化后重新计算尺寸

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    CanvasScene *canvasScene() const;

    NodeData      m_data;
    NodeTextItem *m_text;
    QSizeF        m_size;
    bool          m_editing = false;
    QString       m_textBeforeEdit;
};

#pragma once

#include "Document.h"

#include <QPointF>
#include <QUndoCommand>
#include <QVector>

class CanvasScene;

// 撤销命令。命令只保存数据和 id，不保存图元指针，执行时通过 CanvasScene 按 id 查找。
// alreadyApplied 为 true 表示操作已经由界面完成，入栈时的第一次 redo() 不再重复执行。

struct ItemsSnapshot
{
    QVector<NodeData> nodes;
    QVector<EdgeData> edges;
    bool isEmpty() const { return nodes.isEmpty() && edges.isEmpty(); }
};

class AddItemsCommand : public QUndoCommand
{
public:
    AddItemsCommand(CanvasScene *scene, const ItemsSnapshot &items, bool alreadyApplied,
                    const QString &text);
    void undo() override;
    void redo() override;

private:
    CanvasScene  *m_scene;
    ItemsSnapshot m_items;
    bool          m_skipRedo;
};

class RemoveItemsCommand : public QUndoCommand
{
public:
    RemoveItemsCommand(CanvasScene *scene, const ItemsSnapshot &items, const QString &text);
    void undo() override;
    void redo() override;

private:
    CanvasScene  *m_scene;
    ItemsSnapshot m_items;
};

struct NodeMove
{
    QString id;
    QPointF from;
    QPointF to;
};

class MoveNodesCommand : public QUndoCommand
{
public:
    MoveNodesCommand(CanvasScene *scene, const QVector<NodeMove> &moves, bool alreadyApplied);
    void undo() override;
    void redo() override;

private:
    CanvasScene      *m_scene;
    QVector<NodeMove> m_moves;
    bool              m_skipRedo;
};

class EditTextCommand : public QUndoCommand
{
public:
    EditTextCommand(CanvasScene *scene, const QString &id,
                    const QString &oldText, const QString &newText);
    void undo() override;
    void redo() override;

private:
    CanvasScene *m_scene;
    QString      m_id;
    QString      m_old;
    QString      m_new;
};

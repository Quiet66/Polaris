#include "Commands.h"
#include "CanvasScene.h"

namespace {

void addAll(CanvasScene *scene, const ItemsSnapshot &items)
{
    for (const NodeData &n : items.nodes)
        scene->addNode(n);
    for (const EdgeData &e : items.edges)
        scene->addEdge(e);
}

void removeAll(CanvasScene *scene, const ItemsSnapshot &items)
{
    for (const EdgeData &e : items.edges)
        scene->removeEdge(e.id);
    for (const NodeData &n : items.nodes)
        scene->removeNode(n.id);
}

} // namespace

AddItemsCommand::AddItemsCommand(CanvasScene *scene, const ItemsSnapshot &items,
                                 bool alreadyApplied, const QString &text)
    : QUndoCommand(text)
    , m_scene(scene)
    , m_items(items)
    , m_skipRedo(alreadyApplied)
{
}

void AddItemsCommand::undo()
{
    removeAll(m_scene, m_items);
}

void AddItemsCommand::redo()
{
    if (m_skipRedo) {
        m_skipRedo = false;
        return;
    }
    addAll(m_scene, m_items);
}

RemoveItemsCommand::RemoveItemsCommand(CanvasScene *scene, const ItemsSnapshot &items,
                                       const QString &text)
    : QUndoCommand(text)
    , m_scene(scene)
    , m_items(items)
{
}

void RemoveItemsCommand::undo()
{
    addAll(m_scene, m_items);
}

void RemoveItemsCommand::redo()
{
    removeAll(m_scene, m_items);
}

MoveNodesCommand::MoveNodesCommand(CanvasScene *scene, const QVector<NodeMove> &moves,
                                   bool alreadyApplied)
    : QUndoCommand(QStringLiteral("移动"))
    , m_scene(scene)
    , m_moves(moves)
    , m_skipRedo(alreadyApplied)
{
}

void MoveNodesCommand::undo()
{
    for (const NodeMove &m : qAsConst(m_moves))
        m_scene->setNodePos(m.id, m.from);
}

void MoveNodesCommand::redo()
{
    if (m_skipRedo) {
        m_skipRedo = false;
        return;
    }
    for (const NodeMove &m : qAsConst(m_moves))
        m_scene->setNodePos(m.id, m.to);
}

EditTextCommand::EditTextCommand(CanvasScene *scene, const QString &id,
                                 const QString &oldText, const QString &newText)
    : QUndoCommand(QStringLiteral("编辑文字"))
    , m_scene(scene)
    , m_id(id)
    , m_old(oldText)
    , m_new(newText)
{
}

void EditTextCommand::undo()
{
    m_scene->setNodeText(m_id, m_old);
}

void EditTextCommand::redo()
{
    m_scene->setNodeText(m_id, m_new);
}

ChangeNodesCommand::ChangeNodesCommand(CanvasScene *scene, const QVector<NodeChange> &changes,
                                       bool alreadyApplied, const QString &text)
    : QUndoCommand(text)
    , m_scene(scene)
    , m_changes(changes)
    , m_skipRedo(alreadyApplied)
{
}

void ChangeNodesCommand::undo()
{
    for (const NodeChange &c : qAsConst(m_changes))
        m_scene->setNodeStyle(c.before);
}

void ChangeNodesCommand::redo()
{
    if (m_skipRedo) {
        m_skipRedo = false;
        return;
    }
    for (const NodeChange &c : qAsConst(m_changes))
        m_scene->setNodeStyle(c.after);
}

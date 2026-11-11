#include "NodeItem.h"
#include "CanvasScene.h"
#include "EdgeItem.h"
#include "Theme.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>

// ---------------------------------------------------------------- NodeTextItem

NodeTextItem::NodeTextItem(NodeItem *owner)
    : QGraphicsTextItem(owner)
    , m_owner(owner)
{
    document()->setDocumentMargin(0);
    QTextOption opt = document()->defaultTextOption();
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document()->setDefaultTextOption(opt);

    setTextInteractionFlags(Qt::NoTextInteraction);
    setAcceptedMouseButtons(Qt::NoButton);      // 平时让点击和拖动落到节点上
    setCacheMode(DeviceCoordinateCache);

    QObject::connect(document(), &QTextDocument::contentsChanged, this, [this]() {
        if (m_owner->isEditing())
            m_owner->relayout();
    });
}

void NodeTextItem::inputMethodEvent(QInputMethodEvent *event)
{
    m_composing = !event->preeditString().isEmpty();
    QGraphicsTextItem::inputMethodEvent(event);
}

bool NodeTextItem::sceneEvent(QEvent *event)
{
    // QGraphicsTextItem::sceneEvent 会绕过 keyPressEvent 直接把 Tab 插入文本，这里拦下来。
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *k = static_cast<QKeyEvent *>(event);
        if (k->key() == Qt::Key_Tab || k->key() == Qt::Key_Backtab) {
            keyPressEvent(k);
            return true;
        }
    }
    return QGraphicsTextItem::sceneEvent(event);
}

void NodeTextItem::keyPressEvent(QKeyEvent *event)
{
    // 输入法组词期间，Enter / Tab / Esc 属于输入法，不触发确认或取消。
    if (!m_composing && m_owner->isEditing()) {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (event->modifiers() & Qt::ShiftModifier) {
                textCursor().insertText(QStringLiteral("\n"));
            } else {
                m_owner->endEdit(NodeItem::Commit);
            }
            event->accept();
            return;
        case Qt::Key_Escape:
            m_owner->endEdit(NodeItem::Cancel);
            event->accept();
            return;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            m_owner->endEdit(NodeItem::CommitAndChain);
            event->accept();
            return;
        default:
            break;
        }
    }
    QGraphicsTextItem::keyPressEvent(event);
}

void NodeTextItem::focusOutEvent(QFocusEvent *event)
{
    QGraphicsTextItem::focusOutEvent(event);
    m_composing = false;
    // 弹出输入法候选窗、右键菜单等不算离开编辑。
    if (event->reason() == Qt::PopupFocusReason || event->reason() == Qt::ActiveWindowFocusReason)
        return;
    if (m_owner->isEditing())
        m_owner->endEdit(NodeItem::Commit);
}

// -------------------------------------------------------------------- NodeItem

NodeItem::NodeItem(const NodeData &data)
    : m_data(data)
    , m_text(new NodeTextItem(this))
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCacheMode(DeviceCoordinateCache);
    setPos(data.x, data.y);
    m_text->setPlainText(data.text);
    relayout();
}

CanvasScene *NodeItem::canvasScene() const
{
    return qobject_cast<CanvasScene *>(scene());
}

NodeData NodeItem::data() const
{
    NodeData d = m_data;
    d.x = pos().x();
    d.y = pos().y();
    return d;
}

void NodeItem::setText(const QString &text)
{
    m_data.text = text;
    if (m_text->toPlainText() != text)
        m_text->setPlainText(text);
    relayout();
}

void NodeItem::relayout()
{
    const Theme &t = Theme::current();

    QFont f = QApplication::font();
    f.setPointSizeF(m_data.fontSize > 0 ? m_data.fontSize : t.defaultFontSize);
    if (m_text->font() != f)
        m_text->setFont(f);
    m_text->setDefaultTextColor(t.text);

    if (m_data.w > 0) {
        m_text->setTextWidth(qMax<qreal>(m_data.w - 2 * t.nodePadX, 8));
    } else {
        const qreal maxText = t.nodeMaxWidth - 2 * t.nodePadX;
        m_text->setTextWidth(-1);
        if (m_text->document()->idealWidth() > maxText)
            m_text->setTextWidth(maxText);
    }

    const QSizeF ts = m_text->boundingRect().size();
    QSizeF size(m_data.w > 0 ? m_data.w : qMax(t.nodeMinWidth, ts.width() + 2 * t.nodePadX),
                ts.height() + 2 * t.nodePadY);
    if (size != m_size) {
        prepareGeometryChange();
        m_size = size;
        updateEdges();
    }
    m_text->setPos(t.nodePadX, t.nodePadY);
    update();
}

QRectF NodeItem::boundingRect() const
{
    const Theme &t = Theme::current();
    const qreal m = qMax<qreal>(2, t.portRadius + 1.5);     // 连接点有一半露在矩形外
    return bodyRect().adjusted(-m, -m, m, qMax(m, t.nodeShadowOffset + 2));
}

QPainterPath NodeItem::shape() const
{
    // 形状始终包含四个连接点，这样光标移到连接点露在外面的那一半时仍算悬停在节点上。
    const Theme &t = Theme::current();
    QPainterPath p;
    p.setFillRule(Qt::WindingFill);
    p.addRoundedRect(bodyRect(), t.nodeRadius, t.nodeRadius);
    for (int i = 0; i < 4; ++i)
        p.addEllipse(portCenter(i), t.portRadius + 1, t.portRadius + 1);
    return p;
}

QPointF NodeItem::portCenter(int index) const
{
    const QRectF b = bodyRect();
    switch (index) {
    case 0:  return QPointF(b.right(), b.center().y());
    case 1:  return QPointF(b.center().x(), b.bottom());
    case 2:  return QPointF(b.left(), b.center().y());
    default: return QPointF(b.center().x(), b.top());
    }
}

int NodeItem::portAt(const QPointF &p) const
{
    const qreal r = Theme::current().portRadius + 2.5;      // 命中范围比可见圆点略大
    for (int i = 0; i < 4; ++i) {
        const QPointF d = p - portCenter(i);
        if (d.x() * d.x() + d.y() * d.y() <= r * r)
            return i;
    }
    return -1;
}

void NodeItem::updateEdges()
{
    for (EdgeItem *e : qAsConst(m_edges))
        e->updateGeometry();
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged)
        updateEdges();
    return QGraphicsItem::itemChange(change, value);
}

void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    m_hover = true;
    update();
}

void NodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    m_hover = false;
    update();
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_editing && portAt(event->pos()) >= 0) {
        if (CanvasScene *s = canvasScene()) {
            m_linking = true;
            s->beginLinkDrag(this, event->scenePos());
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_linking) {
        if (CanvasScene *s = canvasScene())
            s->updateLinkDrag(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_linking) {
        if (event->button() == Qt::LeftButton) {
            m_linking = false;
            if (CanvasScene *s = canvasScene())
                s->endLinkDrag(event->scenePos());
        }
        event->accept();
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

void NodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    const Theme &t = Theme::current();
    const QRectF body = bodyRect();
    const bool selected = option->state & QStyle::State_Selected;

    painter->setRenderHint(QPainter::Antialiasing, true);

    // 手绘阴影：一个极淡的偏移圆角矩形（不用 QGraphicsEffect）。
    painter->setPen(Qt::NoPen);
    painter->setBrush(t.nodeShadow);
    painter->drawRoundedRect(body.translated(0, t.nodeShadowOffset), t.nodeRadius, t.nodeRadius);

    const qreal bw = selected ? t.nodeSelectedWidth : t.nodeBorderWidth;
    painter->setPen(QPen(selected ? t.accent : t.nodeBorder, bw));
    painter->setBrush(t.nodeFill);
    const qreal h = bw / 2;
    painter->drawRoundedRect(body.adjusted(h, h, -h, -h), t.nodeRadius, t.nodeRadius);

    // 悬停时在四边中点显示连接点。
    if (m_hover && !m_editing) {
        painter->setPen(QPen(t.accent, 1.2));
        painter->setBrush(t.nodeFill);
        for (int i = 0; i < 4; ++i)
            painter->drawEllipse(portCenter(i), t.portRadius, t.portRadius);
    }
}

void NodeItem::beginEdit()
{
    if (m_editing || !scene())
        return;
    m_editing = true;
    m_textBeforeEdit = m_data.text;
    update();

    m_text->setCacheMode(NoCache);
    m_text->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    m_text->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_text->setFocus(Qt::OtherFocusReason);
    QTextCursor c = m_text->textCursor();
    c.movePosition(QTextCursor::End);
    m_text->setTextCursor(c);
}

void NodeItem::endEdit(EditEnd how)
{
    if (!m_editing)
        return;
    m_editing = false;      // 先置位：下面 clearFocus 会再次触发 focusOut

    const QString newText = (how == Cancel) ? m_textBeforeEdit
                                            : m_text->toPlainText().trimmed();

    QTextCursor c = m_text->textCursor();
    c.clearSelection();
    m_text->setTextCursor(c);
    m_text->setTextInteractionFlags(Qt::NoTextInteraction);
    m_text->setAcceptedMouseButtons(Qt::NoButton);
    m_text->clearFocus();
    m_text->setCacheMode(DeviceCoordinateCache);

    m_data.text = newText;
    if (m_text->toPlainText() != newText)
        m_text->setPlainText(newText);
    relayout();

    if (CanvasScene *s = canvasScene())
        s->nodeEditFinished(this, how, m_textBeforeEdit);
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        beginEdit();
        event->accept();
        return;
    }
    QGraphicsItem::mouseDoubleClickEvent(event);
}

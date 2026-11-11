#include "NodeItem.h"
#include "CanvasScene.h"
#include "EdgeItem.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
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
    setAcceptedMouseButtons(Qt::NoButton);
    // 平时隐藏：文字由 NodeItem 画进它自己的缓存（少一层图元，拖动缩放更省）；
    // 只有编辑时才显示出来接收输入。
    setVisible(false);

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

void NodeItem::setStyle(qreal w, qreal fontSize)
{
    m_data.w = w;
    m_data.fontSize = fontSize;
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

int NodeItem::gripAt(const QPointF &p) const
{
    // 宽度手柄 = 选中节点的左右两条边；边的中点让给连接点。
    if (!isSelected() || m_editing || portAt(p) >= 0)
        return 0;
    const QRectF b = bodyRect();
    const qreal g = Theme::current().resizeGrip;
    if (p.y() < b.top() || p.y() > b.bottom())
        return 0;
    if (qAbs(p.x() - b.left()) <= g)
        return -1;
    if (qAbs(p.x() - b.right()) <= g)
        return 1;
    return 0;
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

void NodeItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (gripAt(event->pos()) != 0)
        setCursor(Qt::SizeHorCursor);
    else
        unsetCursor();
}

void NodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    m_hover = false;
    unsetCursor();
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
    if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ControlModifier)) {
        const int side = gripAt(event->pos());
        if (side != 0) {
            m_resizeSide = side;
            m_resizeStart = data();
            m_resizeStartWidth = m_size.width();
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizeSide != 0) {
        const qreal minW = Theme::current().nodeMinResizeWidth;
        if (m_resizeSide > 0) {
            m_data.w = qMax(minW, event->scenePos().x() - pos().x());
        } else {
            // 拖左边：右边缘保持不动。
            const qreal right = m_resizeStart.x + m_resizeStartWidth;
            const qreal left = qMin(event->scenePos().x(), right - minW);
            m_data.w = right - left;
            setPos(left, pos().y());
        }
        relayout();
        event->accept();
        return;
    }
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
    if (m_resizeSide != 0) {
        if (event->button() == Qt::LeftButton) {
            m_resizeSide = 0;
            if (CanvasScene *s = canvasScene())
                s->nodeResized(m_resizeStart, data());
        }
        event->accept();
        return;
    }
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

    if (!m_editing) {
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.palette.setColor(QPalette::Text, t.text);
        painter->save();
        painter->translate(m_text->pos());
        m_text->document()->documentLayout()->draw(painter, ctx);
        painter->restore();
    }

    // 选中时左右两条边加粗，提示可以拖动调整宽度。
    if (selected && !m_editing) {
        const qreal r = t.nodeRadius;
        painter->setPen(QPen(t.accent, 3, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(body.left() + 1, body.top() + r + 1),
                          QPointF(body.left() + 1, body.bottom() - r - 1));
        painter->drawLine(QPointF(body.right() - 1, body.top() + r + 1),
                          QPointF(body.right() - 1, body.bottom() - r - 1));
    }

    // 悬停时在四边中点显示连接点。
    const CanvasScene *cs = canvasScene();
    if (m_hover && !m_editing && !(cs && cs->isExporting())) {
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

    m_text->setVisible(true);
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
    m_text->setVisible(false);

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

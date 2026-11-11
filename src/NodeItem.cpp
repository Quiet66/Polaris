#include "NodeItem.h"
#include "CanvasScene.h"
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
    }
    m_text->setPos(t.nodePadX, t.nodePadY);
    update();
}

QRectF NodeItem::boundingRect() const
{
    const qreal so = Theme::current().nodeShadowOffset;
    return bodyRect().adjusted(-2, -2, so + 2, so + 2);
}

QPainterPath NodeItem::shape() const
{
    QPainterPath p;
    const qreal r = Theme::current().nodeRadius;
    p.addRoundedRect(bodyRect(), r, r);
    return p;
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
}

void NodeItem::beginEdit()
{
    if (m_editing || !scene())
        return;
    m_editing = true;
    m_textBeforeEdit = m_data.text;

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

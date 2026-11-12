#include "MdIo.h"

#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {

const QLatin1String kMarker("<!-- polaris:");
// 改名前（TrailCanvas）保存的文件用的是这个标记，读入时仍然认。
const QLatin1String kLegacyMarker("<!-- trailcanvas:");

// 布局注释必须独占一行的开头：节点文字里即使出现同样的标记，也在 JSON 内部，不会被误认。
// 返回 JSON 起始位置，找不到返回 -1。
int findLayoutJson(const QString &text)
{
    int best = -1;
    for (const QLatin1String &marker : { kMarker, kLegacyMarker }) {
        int start = text.startsWith(marker) ? 0 : -1;
        const int nl = text.lastIndexOf(QLatin1Char('\n') + QString(marker));
        if (nl >= 0)
            start = nl + 1;
        if (start >= 0)
            best = qMax(best, start + marker.size());
    }
    return best;
}

struct Graph
{
    QHash<QString, const NodeData *> node;
    QHash<QString, QVector<const EdgeData *>> out;      // 有向：出边；无向：所有相邻边
    QHash<QString, int> inDegree;
};

bool nodeEarlier(const NodeData *a, const NodeData *b)
{
    return a->createdAt != b->createdAt ? a->createdAt < b->createdAt : a->id < b->id;
}

bool edgeEarlier(const EdgeData *a, const EdgeData *b)
{
    return a->createdAt != b->createdAt ? a->createdAt < b->createdAt : a->id < b->id;
}

// 大纲里一个节点占一行：换行替换为空格。
QString oneLine(const QString &text)
{
    QString s = text;
    s.replace(QLatin1String("\r\n"), QLatin1String(" "));
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    s.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return s.trimmed();
}

} // namespace

QString MdIo::outline(const Document &doc, const QString &title)
{
    const bool directed = doc.settings.directed;

    Graph g;
    QVector<const NodeData *> order;        // 按创建时间排序的全部节点
    for (const NodeData &n : doc.nodes) {
        g.node.insert(n.id, &n);
        g.inDegree.insert(n.id, 0);
        order.append(&n);
    }
    std::sort(order.begin(), order.end(), nodeEarlier);

    for (const EdgeData &e : doc.edges) {
        if (!g.node.contains(e.from) || !g.node.contains(e.to))
            continue;
        g.out[e.from].append(&e);
        if (directed)
            g.inDegree[e.to] += 1;
        else
            g.out[e.to].append(&e);
    }
    for (auto it = g.out.begin(); it != g.out.end(); ++it)
        std::sort(it->begin(), it->end(), edgeEarlier);     // 子节点按连线创建时间排序

    QStringList lines;
    QStringList cross;                      // "其他关联"
    QSet<QString> written;
    QSet<QString> usedEdges;
    const QString arrow = directed ? QStringLiteral(" → ") : QStringLiteral(" — ");

    // 深度优先。用显式栈，避免很长的链把调用栈撑爆。
    struct Frame { QString id; int depth; int next; };
    auto walk = [&](const NodeData *root) {
        QVector<Frame> stack;
        written.insert(root->id);
        lines.append(QStringLiteral("- ") + oneLine(root->text));
        stack.append(Frame{root->id, 0, 0});

        while (!stack.isEmpty()) {
            Frame &f = stack.last();
            const QVector<const EdgeData *> &edges = g.out[f.id];
            if (f.next >= edges.size()) {
                stack.removeLast();
                continue;
            }
            const EdgeData *e = edges.at(f.next++);
            if (usedEdges.contains(e->id))
                continue;                   // 无向时：来时走过的那条边
            usedEdges.insert(e->id);

            const QString other = (e->from == f.id) ? e->to : e->from;
            if (written.contains(other)) {
                // 节点只在第一次到达时写入大纲；指向已写入节点的线记到"其他关联"。
                cross.append(QStringLiteral("- ") + oneLine(g.node[f.id]->text) + arrow
                             + oneLine(g.node[other]->text));
                continue;
            }
            const int depth = f.depth + 1;  // 先取出来：下面 append 可能使 f 失效
            written.insert(other);
            lines.append(QString(depth * 2, QLatin1Char(' ')) + QStringLiteral("- ")
                         + oneLine(g.node[other]->text));
            stack.append(Frame{other, depth, 0});
        }
    };

    if (directed) {
        // 没有入边的节点为根，按创建时间排序。
        for (const NodeData *n : qAsConst(order)) {
            if (g.inDegree.value(n->id) == 0 && !written.contains(n->id))
                walk(n);
        }
    }
    // 有向：剩下的节点都在环里，取创建时间最早的作为新根，重复直到全部写入。
    // 无向：每个连通块以创建时间最早的节点为根。
    for (const NodeData *n : qAsConst(order)) {
        if (!written.contains(n->id))
            walk(n);
    }

    QString md = QStringLiteral("# ") + title + QStringLiteral("\n\n");
    if (!lines.isEmpty())
        md += lines.join(QLatin1Char('\n')) + QStringLiteral("\n\n");
    if (!cross.isEmpty()) {
        md += QStringLiteral("## 其他关联\n\n");
        md += cross.join(QLatin1Char('\n')) + QStringLiteral("\n\n");
    }
    return md;
}

QString MdIo::toMarkdown(const Document &doc, const QString &title)
{
    QString json = QString::fromUtf8(QJsonDocument(doc.toJson()).toJson(QJsonDocument::Compact));
    // 避免 JSON 文本里的 '>' 提前结束注释。
    json.replace(QLatin1Char('>'), QLatin1String("\\u003e"));
    return outline(doc, title) + kMarker + json + QStringLiteral(" -->\n");
}

bool MdIo::fromMarkdown(const QString &text, Document *out, QString *error)
{
    const int jsonStart = findLayoutJson(text);
    if (jsonStart < 0) {
        if (error)
            *error = QStringLiteral("这个文件里没有 Polaris 的布局信息，暂时无法打开。");
        return false;
    }
    const int end = text.indexOf(QLatin1String("-->"), jsonStart);
    if (end < 0) {
        if (error)
            *error = QStringLiteral("布局信息不完整（注释没有结束）。");
        return false;
    }

    QJsonParseError pe;
    const QJsonDocument jd =
        QJsonDocument::fromJson(text.mid(jsonStart, end - jsonStart).trimmed().toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !jd.isObject()) {
        if (error)
            *error = QStringLiteral("布局信息损坏：%1").arg(pe.errorString());
        return false;
    }
    return Document::fromJson(jd.object(), out, error);
}

bool MdIo::saveFile(const QString &path, const Document &doc, const QString &title, QString *error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    const QByteArray bytes = toMarkdown(doc, title).toUtf8();   // UTF-8，无 BOM
    if (f.write(bytes) != bytes.size() || !f.commit()) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

bool MdIo::loadFile(const QString &path, Document *out, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return fromMarkdown(QString::fromUtf8(f.readAll()), out, error);
}

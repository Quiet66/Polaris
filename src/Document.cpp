#include "Document.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <QUuid>

namespace {
inline QLatin1String L(const char *s) { return QLatin1String(s); }

QJsonValue numberOrNull(qreal v)
{
    return v > 0 ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
}
}

QString Document::newId()
{
    return QUuid::createUuid().toString(QUuid::Id128).left(12);
}

qint64 Document::now()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QJsonObject Document::toJson() const
{
    QJsonObject s;
    s[L("directed")]   = settings.directed;
    s[L("background")] = settings.background;
    s[L("theme")]      = settings.theme;

    QJsonObject v;
    v[L("x")]    = view.x;
    v[L("y")]    = view.y;
    v[L("zoom")] = view.zoom;

    QJsonArray ns;
    for (const NodeData &n : nodes) {
        QJsonObject o;
        o[L("id")]        = n.id;
        o[L("x")]         = n.x;
        o[L("y")]         = n.y;
        o[L("text")]      = n.text;
        o[L("w")]         = numberOrNull(n.w);
        o[L("fontSize")]  = numberOrNull(n.fontSize);
        o[L("createdAt")] = double(n.createdAt);
        ns.append(o);
    }

    QJsonArray es;
    for (const EdgeData &e : edges) {
        QJsonObject o;
        o[L("id")]        = e.id;
        o[L("from")]      = e.from;
        o[L("to")]        = e.to;
        o[L("createdAt")] = double(e.createdAt);
        es.append(o);
    }

    QJsonObject root;
    root[L("version")]  = version;
    root[L("settings")] = s;
    root[L("view")]     = v;
    root[L("nodes")]    = ns;
    root[L("edges")]    = es;
    return root;
}

bool Document::fromJson(const QJsonObject &obj, Document *out, QString *error)
{
    Document d;
    d.version = obj.value(L("version")).toInt(1);
    if (d.version != 1) {
        if (error)
            *error = QStringLiteral("不支持的文件版本：%1").arg(d.version);
        return false;
    }

    const QJsonObject s = obj.value(L("settings")).toObject();
    d.settings.directed = s.value(L("directed")).toBool(true);
    const QString bg = s.value(L("background")).toString();
    if (bg == L("plain") || bg == L("dots") || bg == L("lines"))
        d.settings.background = bg;
    const QString th = s.value(L("theme")).toString();
    if (th == L("light") || th == L("dark"))
        d.settings.theme = th;

    const QJsonObject v = obj.value(L("view")).toObject();
    d.view.x = v.value(L("x")).toDouble(0);
    d.view.y = v.value(L("y")).toDouble(0);
    d.view.zoom = qBound(0.2, v.value(L("zoom")).toDouble(1), 3.0);

    QSet<QString> ids;
    const QJsonArray ns = obj.value(L("nodes")).toArray();
    for (const QJsonValue &val : ns) {
        const QJsonObject o = val.toObject();
        NodeData n;
        n.id = o.value(L("id")).toString();
        if (n.id.isEmpty() || ids.contains(n.id))
            continue;
        n.x         = o.value(L("x")).toDouble(0);
        n.y         = o.value(L("y")).toDouble(0);
        n.text      = o.value(L("text")).toString();
        n.w         = o.value(L("w")).toDouble(0);
        n.fontSize  = o.value(L("fontSize")).toDouble(0);
        n.createdAt = qint64(o.value(L("createdAt")).toDouble(0));
        ids.insert(n.id);
        d.nodes.append(n);
    }

    QSet<QString> pairs;    // 两个节点之间至多一条线（不分方向）
    const QJsonArray es = obj.value(L("edges")).toArray();
    for (const QJsonValue &val : es) {
        const QJsonObject o = val.toObject();
        EdgeData e;
        e.id   = o.value(L("id")).toString();
        e.from = o.value(L("from")).toString();
        e.to   = o.value(L("to")).toString();
        e.createdAt = qint64(o.value(L("createdAt")).toDouble(0));
        if (e.id.isEmpty() || e.from == e.to || !ids.contains(e.from) || !ids.contains(e.to))
            continue;
        const QString key = e.from < e.to ? e.from + QLatin1Char('|') + e.to
                                          : e.to + QLatin1Char('|') + e.from;
        if (pairs.contains(key))
            continue;
        pairs.insert(key);
        d.edges.append(e);
    }

    *out = d;
    return true;
}

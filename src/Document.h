#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

// 数据模型（SPEC 第 5 节）。纯数据，不依赖界面。

struct NodeData
{
    QString id;
    qreal   x = 0;
    qreal   y = 0;
    QString text;
    qreal   w = 0;          // <= 0 表示 null：宽度随文字自适应
    qreal   fontSize = 0;   // <= 0 表示 null：用默认字号
    qint64  createdAt = 0;  // 毫秒时间戳
};

struct EdgeData
{
    QString id;
    QString from;
    QString to;
    qint64  createdAt = 0;
};

struct DocSettings
{
    bool    directed = true;
    QString background = QStringLiteral("dots");    // "plain" / "dots" / "lines"
    QString theme = QStringLiteral("light");        // "light" / "dark"
};

struct DocView
{
    qreal x = 0;        // 视口左上角对应的场景坐标
    qreal y = 0;
    qreal zoom = 1;
};

struct Document
{
    int               version = 1;
    DocSettings       settings;
    DocView           view;
    QVector<NodeData> nodes;
    QVector<EdgeData> edges;

    QJsonObject toJson() const;
    // 读入并校验：丢弃自环、重复连线、端点不存在的连线。
    static bool fromJson(const QJsonObject &obj, Document *out, QString *error = nullptr);

    static QString newId();
    static qint64  now();
};

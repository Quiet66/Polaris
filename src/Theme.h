#pragma once

#include <QColor>
#include <QString>

// 所有颜色、圆角、间距集中在这里（SPEC 第 10 节）。
struct Theme
{
    QString name;

    QColor canvas;          // 画布底色
    QColor grid;            // 点阵 / 线阵
    QColor windowBorder;    // 窗口 1px 描边
    QColor nodeFill;
    QColor nodeBorder;
    QColor nodeShadow;      // 手绘偏移阴影
    QColor text;
    QColor accent;          // 选中等强调色
    QColor buttonFill;      // 菜单按钮
    QColor edge;            // 连线

    qreal nodeRadius          = 6;
    qreal nodePadX            = 12;
    qreal nodePadY            = 7;
    qreal nodeBorderWidth     = 1;
    qreal nodeSelectedWidth   = 1.6;
    qreal nodeShadowOffset    = 2;
    qreal nodeMinWidth        = 40;
    qreal nodeMaxWidth        = 300;    // w 为 null 时的自动换行宽度
    qreal defaultFontSize     = 11;     // pt
    qreal portRadius          = 4.5;    // 连接点
    qreal edgeWidth           = 1.5;
    qreal edgeActiveWidth     = 2.6;    // 悬停、选中
    qreal edgeHitWidth        = 12;     // 可点击范围
    qreal arrowLength         = 9;
    qreal arrowHalfWidth      = 4;
    qreal linkedNodeGap       = 60;     // Tab 新建节点与来源节点的水平间距
    qreal linkedNodeStepGap   = 14;     // 重叠时向下错开的间隙
    qreal gridStep            = 24;     // 场景坐标
    qreal gridMinPixelStep    = 14;     // 缩小后网格的最小屏幕间距，低于它就把密度减半

    static Theme light();
    static Theme dark();
    static const Theme &current();
    static void setCurrent(const QString &name);   // "light" / "dark"
};

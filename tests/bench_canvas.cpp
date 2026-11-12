// 画布绘制性能基准（SPEC 第 3 节：200 个节点时拖动和缩放不卡顿）。
// 每一帧都同步重绘整个视口（没有滚动复用），所以是最坏情况下的每帧耗时。
#include "CanvasScene.h"
#include "CanvasView.h"
#include "EdgeItem.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QScrollBar>

#include <algorithm>
#include <cstdio>
#include <functional>

static void report(const char *what, QVector<qint64> &ns)
{
    std::sort(ns.begin(), ns.end());
    qint64 sum = 0;
    for (qint64 v : ns)
        sum += v;
    const double avg = sum / double(ns.size()) / 1e6;
    const double p95 = ns[int(ns.size() * 0.95)] / 1e6;
    const double worst = ns.last() / 1e6;
    std::printf("%-28s avg %6.2f ms   p95 %6.2f ms   max %6.2f ms   (%.0f fps avg)\n",
                what, avg, p95, worst, 1000.0 / avg);
}

static QVector<qint64> run(int frames, QWidget *viewport, const std::function<void(int)> &step)
{
    QVector<qint64> ns;
    QElapsedTimer t;
    for (int i = 0; i < frames; ++i) {
        t.start();
        step(i);
        viewport->repaint();        // 同步重绘整个视口
        ns.append(t.nsecsElapsed());
    }
    return ns;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFont f(QStringLiteral("Microsoft YaHei"));
    f.setPointSize(10);
    QApplication::setFont(f);

    const int count = argc > 1 ? qMax(1, atoi(argv[1])) : 200;
    const int cols = 20;

    CanvasScene scene;
    Document d;
    for (int i = 0; i < count; ++i) {
        NodeData n;
        n.id = QStringLiteral("n%1").arg(i);
        n.text = (i % 7 == 0) ? QStringLiteral("节点 %1\n第二行说明文字").arg(i)
                              : QStringLiteral("查询路径节点 %1").arg(i);
        n.x = (i % cols) * 190.0;
        n.y = (i / cols) * 90.0;
        n.createdAt = i;
        d.nodes.append(n);
    }
    int e = 0;
    auto link = [&](int a, int b) {
        if (a < 0 || b < 0 || a >= count || b >= count || a == b)
            return;
        EdgeData ed;
        ed.id = QStringLiteral("e%1").arg(e++);
        ed.from = QStringLiteral("n%1").arg(a);
        ed.to = QStringLiteral("n%1").arg(b);
        ed.createdAt = e;
        d.edges.append(ed);
    };
    for (int i = 0; i < count; ++i) {
        if ((i + 1) % cols != 0)
            link(i, i + 1);             // 横向链
        if (i % 3 == 0)
            link(i, i + cols);          // 纵向
        if (i % 11 == 0)
            link(i, i + cols + 3);      // 斜向交叉
    }
    if (argc > 2)
        d.settings.background = QString::fromLatin1(argv[2]);   // plain / dots / lines
    scene.loadDocument(d);

    CanvasView view(&scene);
    view.resize(1280, 800);
    view.show();
    view.fitAll();
    QApplication::processEvents();
    QWidget *vp = view.viewport();

    std::printf("nodes %d, edges %d, background %s, viewport %dx%d, fit zoom %.2f\n",
                scene.nodes().size(), scene.edges().size(),
                scene.settings().background.toLatin1().constData(), vp->width(), vp->height(),
                view.zoom());

    for (int warm = 0; warm < 5; ++warm)
        vp->repaint();

    // 1. 平移：全部节点可见，每帧移动 6 像素。
    {
        QVector<qint64> ns = run(120, vp, [&](int i) {
            QScrollBar *h = view.horizontalScrollBar();
            h->setValue(h->value() + (i < 60 ? 6 : -6));
        });
        report("pan (all nodes visible)", ns);
    }
    // 2. 缩放：每帧改变缩放比例，所有节点缓存都要重建。
    {
        const QPoint c = vp->rect().center();
        QVector<qint64> ns = run(80, vp, [&](int i) {
            view.setZoomAt(view.zoom() * (i < 40 ? 1.03 : 1 / 1.03), c);
        });
        report("zoom (cache rebuilt)", ns);
    }
    // 3. 拖动：选中 10 个节点一起移动，连线实时跟随。
    {
        const QList<NodeItem *> all = scene.nodes();
        QList<NodeItem *> moving;
        for (int i = 0; i < all.size() && moving.size() < 10; i += qMax(1, all.size() / 10))
            moving.append(all[i]);
        QVector<qint64> ns = run(120, vp, [&](int i) {
            for (NodeItem *n : qAsConst(moving))
                n->moveBy(i < 60 ? 3 : -3, i < 60 ? 2 : -2);
        });
        report("drag 10 nodes", ns);
    }
    // 4. 100% 缩放下平移：只有一部分节点可见。
    {
        view.setZoomAt(1.0, vp->rect().center());
        QVector<qint64> ns = run(120, vp, [&](int i) {
            QScrollBar *h = view.horizontalScrollBar();
            h->setValue(h->value() + (i < 60 ? 6 : -6));
        });
        report("pan at 100% zoom", ns);
    }
    return 0;
}

// MdIo 的测试：大纲生成规则（SPEC 第 7 节）和保存/读入往返。
#include "MdIo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>

#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failed;
}

static void expectOutline(const Document &d, const QString &expected, const char *what)
{
    const QString got = MdIo::outline(d, QStringLiteral("T"));
    const bool ok = (got == expected);
    check(ok, what);
    if (!ok) {
        std::printf("---- expected ----\n%s\n---- got ----\n%s\n", expected.toUtf8().constData(),
                    got.toUtf8().constData());
    }
}

static NodeData N(const char *id, const QString &text, qint64 t)
{
    NodeData n;
    n.id = QString::fromLatin1(id);
    n.text = text;
    n.createdAt = t;
    n.x = t * 10;
    n.y = t * 3.5;
    return n;
}

static EdgeData E(const char *id, const char *from, const char *to, qint64 t)
{
    EdgeData e;
    e.id = QString::fromLatin1(id);
    e.from = QString::fromLatin1(from);
    e.to = QString::fromLatin1(to);
    e.createdAt = t;
    return e;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // 1. SPEC 里的例子：A→B→C，A→D，D→C（交叉），E 独立。
    {
        Document d;
        d.nodes = { N("a", QStringLiteral("欧洲历史人物 A"), 1), N("b", QStringLiteral("人物 B"), 2),
                    N("c", QStringLiteral("某公司 C"), 3), N("d", QStringLiteral("事件 D"), 4),
                    N("e", QStringLiteral("另一条独立线索 E"), 5) };
        d.edges = { E("e1", "a", "b", 10), E("e2", "b", "c", 11), E("e3", "a", "d", 12),
                    E("e4", "d", "c", 13) };
        expectOutline(d, QStringLiteral("# T\n\n"
                                        "- 欧洲历史人物 A\n"
                                        "  - 人物 B\n"
                                        "    - 某公司 C\n"
                                        "  - 事件 D\n"
                                        "- 另一条独立线索 E\n\n"
                                        "## 其他关联\n\n"
                                        "- 事件 D → 某公司 C\n\n"),
                      "directed: spec example with one cross edge");
    }

    // 2. 根按节点创建时间排序；子节点按连线创建时间排序（不是节点创建时间）。
    {
        Document d;
        d.nodes = { N("r2", QStringLiteral("R2"), 5), N("r1", QStringLiteral("R1"), 1),
                    N("x", QStringLiteral("X"), 2), N("y", QStringLiteral("Y"), 3) };
        d.edges = { E("e1", "r1", "y", 10), E("e2", "r1", "x", 20) };
        expectOutline(d, QStringLiteral("# T\n\n- R1\n  - Y\n  - X\n- R2\n\n"),
                      "directed: root order by node time, child order by edge time; no cross section");
    }

    // 3. 纯环 a→b→c→a：没有根，取最早的节点；回边记到其他关联。
    {
        Document d;
        d.nodes = { N("b", QStringLiteral("B"), 2), N("a", QStringLiteral("A"), 1),
                    N("c", QStringLiteral("C"), 3) };
        d.edges = { E("e1", "a", "b", 1), E("e2", "b", "c", 2), E("e3", "c", "a", 3) };
        expectOutline(d, QStringLiteral("# T\n\n- A\n  - B\n    - C\n\n"
                                        "## 其他关联\n\n- C → A\n\n"),
                      "directed: pure cycle starts at earliest node");
    }

    // 4. 根 + 远处的环：r→a，另有独立的环 p→q→p。
    {
        Document d;
        d.nodes = { N("r", QStringLiteral("R"), 1), N("a", QStringLiteral("A"), 2),
                    N("q", QStringLiteral("Q"), 3), N("p", QStringLiteral("P"), 4) };
        d.edges = { E("e1", "r", "a", 1), E("e2", "p", "q", 2) , E("e3", "q", "p", 3) };
        // 注意：p-q 之间按"两个节点之间至多一条线"实际不会有两条；这里只测大纲算法本身。
        expectOutline(d, QStringLiteral("# T\n\n- R\n  - A\n- Q\n  - P\n\n"
                                        "## 其他关联\n\n- P → Q\n\n"),
                      "directed: leftover cycle rooted at its earliest node");
    }

    // 5. 无向：连通块以最早的节点为根；交叉线用 "—"；来时的边不算交叉。
    {
        Document d;
        d.settings.directed = false;
        d.nodes = { N("a", QStringLiteral("A"), 1), N("b", QStringLiteral("B"), 2),
                    N("c", QStringLiteral("C"), 3), N("z", QStringLiteral("Z"), 0) };
        // 方向故意写成"指向 a"，无向模式下应被忽略。
        d.edges = { E("e1", "b", "a", 1), E("e2", "c", "b", 2), E("e3", "c", "a", 3) };
        expectOutline(d, QStringLiteral("# T\n\n- Z\n- A\n  - B\n    - C\n\n"
                                        "## 其他关联\n\n- C — A\n\n"),
                      "undirected: components, em-dash, tree edge not repeated");
    }

    // 6. 多行文本在大纲里把换行替换为空格。
    {
        Document d;
        d.nodes = { N("a", QStringLiteral("第一行\n第二行\r\n第三行"), 1) };
        expectOutline(d, QStringLiteral("# T\n\n- 第一行 第二行 第三行\n\n"),
                      "multi-line text flattened");
    }

    // 7. 空文档。
    {
        Document d;
        expectOutline(d, QStringLiteral("# T\n\n"), "empty document");
    }

    // 8. 往返：含 '>'、"-->"、引号、换行、null 宽度与数值宽度。
    {
        Document d;
        d.settings.directed = false;
        d.settings.background = QStringLiteral("lines");
        d.settings.theme = QStringLiteral("dark");
        d.view.x = -123.5;
        d.view.y = 42;
        d.view.zoom = 1.75;
        NodeData a = N("a", QStringLiteral("a --> b > c \"引号\" \\ <!-- x -->\n第二行"), 1);
        a.w = 222.5;
        a.fontSize = 14;
        NodeData b = N("b", QStringLiteral("普通"), 2);
        d.nodes = { a, b };
        d.edges = { E("e1", "a", "b", 3) };

        const QString md = MdIo::toMarkdown(d, QStringLiteral("标题"));
        const int marker = md.lastIndexOf(QLatin1String("<!-- trailcanvas:"));
        const QString comment = md.mid(marker);
        check(marker > 0, "round trip: marker present");
        check(comment.count(QLatin1Char('>')) == 1 && comment.trimmed().endsWith(QLatin1String("-->")),
              "round trip: the only '>' in the comment line is its terminator");
        check(!comment.trimmed().contains(QLatin1Char('\n')), "round trip: comment is a single line");

        Document r;
        QString err;
        check(MdIo::fromMarkdown(md, &r, &err), "round trip: parses");
        check(QJsonDocument(r.toJson()) == QJsonDocument(d.toJson()), "round trip: document identical");
        check(r.nodes.size() == 2 && r.nodes[0].text == a.text && r.nodes[1].w <= 0
                  && qFuzzyCompare(r.nodes[0].w, 222.5),
              "round trip: text, null width and numeric width preserved");
    }

    // 8b. 节点文字里出现标记本身，也不会被误认成布局注释。
    {
        Document d;
        d.nodes = { N("a", QStringLiteral("<!-- trailcanvas:{\"version\":1} -->"), 1) };
        Document r;
        QString err;
        check(MdIo::fromMarkdown(MdIo::toMarkdown(d, QStringLiteral("T")), &r, &err)
                  && r.nodes.size() == 1 && r.nodes[0].text == d.nodes[0].text,
              "round trip: marker text inside a node");
    }

    // 9. 文件往返：UTF-8 无 BOM，原子写入后可读回。
    {
        Document d;
        d.nodes = { N("a", QStringLiteral("中文节点"), 1) };
        const QString path = QDir::temp().filePath(QStringLiteral("tst_mdio_roundtrip.md"));
        QString err;
        check(MdIo::saveFile(path, d, QStringLiteral("标题"), &err), "file: save");
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        const QByteArray raw = f.readAll();
        f.close();
        check(!raw.startsWith("\xEF\xBB\xBF") && raw.startsWith("# \xE6\xA0\x87\xE9\xA2\x98\n"),
              "file: UTF-8 without BOM, starts with the title");
        Document r;
        check(MdIo::loadFile(path, &r, &err) && r.nodes.size() == 1
                  && r.nodes[0].text == QStringLiteral("中文节点"),
              "file: load");
        QFile::remove(path);
    }

    // 10. 没有注释的文件：明确报错，不崩溃。
    {
        Document r;
        QString err;
        check(!MdIo::fromMarkdown(QStringLiteral("# 手写\n\n- a\n  - b\n"), &r, &err) && !err.isEmpty(),
              "no comment: rejected with a message");
        check(!MdIo::fromMarkdown(QStringLiteral("<!-- trailcanvas:{\"version\":1"), &r, &err),
              "unterminated comment: rejected");
        check(!MdIo::fromMarkdown(QStringLiteral("<!-- trailcanvas:{oops} -->"), &r, &err),
              "broken json: rejected");
    }

    std::printf("\n%s (%d failed)\n", g_failed ? "FAILED" : "ALL PASSED", g_failed);
    return g_failed ? 1 : 0;
}

#include "Exporter.h"
#include "CanvasScene.h"
#include "Theme.h"

#include <QImage>
#include <QPainter>

namespace {
const qreal kMargin  = 40;      // 场景单位
const qreal kScale   = 2;       // 默认 2 倍分辨率
const int   kMaxSide = 16000;   // 任一边超过它就降低倍率
}

bool Exporter::exportPng(CanvasScene *scene, const QString &path, bool withBackground,
                         QString *error)
{
    const QRectF content = scene->contentBoundingRect();
    if (content.isEmpty()) {
        if (error)
            *error = QStringLiteral("画布是空的，没有可导出的内容。");
        return false;
    }
    const QRectF source = content.adjusted(-kMargin, -kMargin, kMargin, kMargin);

    qreal scale = kScale;
    const qreal longest = qMax(source.width(), source.height());
    if (longest * scale > kMaxSide)
        scale = kMaxSide / longest;

    const QSize size(qMax(1, qRound(source.width() * scale)), qMax(1, qRound(source.height() * scale)));
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        if (error)
            *error = QStringLiteral("内存不足，无法生成 %1×%2 的图片。").arg(size.width()).arg(size.height());
        return false;
    }
    image.fill(Qt::transparent);

    // 导出前临时清除选中状态和连接点，导出后恢复。
    scene->beginExport();
    {
        QPainter p(&image);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                         | QPainter::SmoothPixmapTransform);
        if (withBackground) {
            p.save();
            p.scale(scale, scale);
            p.translate(-source.topLeft());
            scene->paintBackground(&p, source, scale);
            p.restore();
        }
        scene->render(&p, QRectF(QPointF(0, 0), QSizeF(size)), source, Qt::IgnoreAspectRatio);
    }
    scene->endExport();

    if (!image.save(path, "PNG")) {
        if (error)
            *error = QStringLiteral("无法写入文件：%1").arg(path);
        return false;
    }
    return true;
}

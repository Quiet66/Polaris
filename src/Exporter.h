#pragma once

#include <QString>

class CanvasScene;

// 导出 PNG（SPEC 第 9 节）：范围是全部节点和连线的外框加边距，不是当前窗口。
namespace Exporter {

bool exportPng(CanvasScene *scene, const QString &path, bool withBackground,
               QString *error = nullptr);

} // namespace Exporter

#pragma once

#include "Document.h"

#include <QString>

// Markdown 写出与读入（SPEC 第 7 节）。
// 文件 = 标题 + 大纲 + 可选的"其他关联" + 末尾一行 HTML 注释（完整 doc 的紧凑 JSON）。
// 注释写作 <!-- polaris:{...} -->；改名前的 <!-- trailcanvas:{...} --> 读入时仍然认。
namespace MdIo {

// 生成整个 .md 文件的文本。
QString toMarkdown(const Document &doc, const QString &title);

// 只生成大纲部分（标题、树、其他关联），不含末尾注释。
QString outline(const Document &doc, const QString &title);

// 从 .md 文本还原。有注释就完全按注释还原，忽略大纲正文。
// 没有注释的文件目前不支持（按大纲导入并自动排版属于 M7）。
bool fromMarkdown(const QString &text, Document *out, QString *error = nullptr);

// 原子写入（QSaveFile），UTF-8 无 BOM。
bool saveFile(const QString &path, const Document &doc, const QString &title,
              QString *error = nullptr);
bool loadFile(const QString &path, Document *out, QString *error = nullptr);

} // namespace MdIo

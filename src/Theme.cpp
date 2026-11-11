#include "Theme.h"

namespace {
Theme &currentRef()
{
    static Theme t = Theme::light();
    return t;
}
}

Theme Theme::light()
{
    Theme t;
    t.name         = QStringLiteral("light");
    t.canvas       = QColor(0xFA, 0xFA, 0xF8);
    t.grid         = QColor(0xD9, 0xD9, 0xD4);
    t.windowBorder = QColor(0xCF, 0xCF, 0xCA);
    t.nodeFill     = QColor(0xFF, 0xFF, 0xFF);
    t.nodeBorder   = QColor(0xC9, 0xC9, 0xC4);
    t.nodeShadow   = QColor(0, 0, 0, 16);
    t.text         = QColor(0x2B, 0x2B, 0x2B);
    t.accent       = QColor(0x3B, 0x82, 0xF6);
    t.buttonFill   = QColor(0x55, 0x55, 0x55);
    t.edge         = QColor(0x8C, 0x8C, 0x86);
    return t;
}

Theme Theme::dark()
{
    Theme t;
    t.name         = QStringLiteral("dark");
    t.canvas       = QColor(0x1E, 0x1F, 0x22);
    t.grid         = QColor(0x3A, 0x3C, 0x41);
    t.windowBorder = QColor(0x45, 0x47, 0x4C);
    t.nodeFill     = QColor(0x2B, 0x2D, 0x31);
    t.nodeBorder   = QColor(0x52, 0x55, 0x5B);
    t.nodeShadow   = QColor(0, 0, 0, 60);
    t.text         = QColor(0xE6, 0xE6, 0xE3);
    t.accent       = QColor(0x60, 0xA5, 0xFA);
    t.buttonFill   = QColor(0xC8, 0xC8, 0xC8);
    t.edge         = QColor(0x8A, 0x8D, 0x93);
    return t;
}

const Theme &Theme::current()
{
    return currentRef();
}

void Theme::setCurrent(const QString &name)
{
    currentRef() = (name == QLatin1String("dark")) ? dark() : light();
}

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
    t.menuFill     = QColor(0xFF, 0xFF, 0xFF);
    t.menuBorder   = QColor(0xD4, 0xD4, 0xCF);
    t.menuHover    = QColor(0xEE, 0xF1, 0xF6);
    t.menuText     = QColor(0x2B, 0x2B, 0x2B);
    t.menuTextDisabled = QColor(0xB4, 0xB4, 0xAF);
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
    t.menuFill     = QColor(0x2B, 0x2D, 0x31);
    t.menuBorder   = QColor(0x4A, 0x4D, 0x53);
    t.menuHover    = QColor(0x3A, 0x3D, 0x44);
    t.menuText     = QColor(0xE6, 0xE6, 0xE3);
    t.menuTextDisabled = QColor(0x6E, 0x71, 0x77);
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

QString Theme::menuStyleSheet() const
{
    return QStringLiteral(
        "QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 5px; }"
        "QMenu::item { color: %4; padding: 5px 24px 5px 8px; border-radius: 4px; margin: 0px; }"
        "QMenu::icon { padding-left: 8px; }"
        "QMenu::item:selected { background: %3; }"
        "QMenu::item:disabled { color: %5; background: transparent; }"
        "QMenu::separator { height: 1px; background: %2; margin: 5px 8px; }"
        "QMenu::right-arrow { width: 8px; height: 8px; right: 8px; }")
        .arg(menuFill.name(), menuBorder.name(), menuHover.name(),
             menuText.name(), menuTextDisabled.name());
}

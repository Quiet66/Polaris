#pragma once

#include "Document.h"

#include <QRect>
#include <QString>

class QSettings;

// 偏好设置：ini 文件，放在数据目录里（SPEC 第 7 节）。
// 数据目录 = 程序目录下的 data/；不可写时改用 %APPDATA%\Polaris。
class Prefs
{
public:
    static Prefs &instance();

    QString dataDir() const { return m_dataDir; }

    // 新画布的默认设置：背景、主题、连线方向。
    DocSettings docDefaults() const;
    void setDocDefaults(const DocSettings &s);

    bool snapEnabled() const;           // 对齐吸附（默认关，M7 生效）
    void setSnapEnabled(bool on);

    bool reduceMotion() const;          // 精简动效（M6 生效）
    void setReduceMotion(bool on);

    // 窗口状态
    QRect windowGeometry() const;       // 无记录时为空矩形
    void setWindowGeometry(const QRect &r);
    bool windowMaximized() const;
    void setWindowMaximized(bool on);
    bool topmost() const;               // 默认置顶
    void setTopmost(bool on);
    qreal opacity() const;              // 0.3 ~ 1.0
    void setOpacity(qreal v);

    // 全局快捷键，按名字存取（QKeySequence 的可移植文本，空 = 不启用）。
    QString hotkey(const QString &name, const QString &fallback) const;
    void setHotkey(const QString &name, const QString &sequence);

    // 鼠标手势（拖动画布、拖动窗口），存的是 CanvasView::gestureKey() 的字符串。
    QString mouseGesture(const QString &name, const QString &fallback) const;
    void setMouseGesture(const QString &name, const QString &key);

    QString lastFile() const;           // 上次保存或打开的文件
    void setLastFile(const QString &path);

    QString lastDir() const;            // 文件对话框的起始目录
    void setLastDir(const QString &dir);

    // 草稿：内容变化后定时写到数据目录；draftSource 记录草稿对应的文件（未命名时为空）。
    QString draftPath() const;
    QString draftSource() const;
    void setDraftSource(const QString &path);

private:
    Prefs();
    ~Prefs();
    Prefs(const Prefs &) = delete;
    Prefs &operator=(const Prefs &) = delete;

    QString    m_dataDir;
    QSettings *m_settings;
};

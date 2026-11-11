#pragma once

#include "Document.h"

#include <QString>

class QSettings;

// 偏好设置：ini 文件，放在数据目录里（SPEC 第 7 节）。
// 数据目录 = 程序目录下的 data/；不可写时改用 %APPDATA%\TrailCanvas。
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

private:
    Prefs();
    ~Prefs();
    Prefs(const Prefs &) = delete;
    Prefs &operator=(const Prefs &) = delete;

    QString    m_dataDir;
    QSettings *m_settings;
};

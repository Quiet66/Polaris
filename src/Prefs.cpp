#include "Prefs.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

bool ensureWritable(const QString &dir)
{
    if (!QDir().mkpath(dir))
        return false;
    QFile probe(dir + QStringLiteral("/.write_test"));
    if (!probe.open(QIODevice::WriteOnly))
        return false;
    probe.close();
    probe.remove();
    return true;
}

QString chooseDataDir()
{
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
    if (ensureWritable(local))
        return local;
    // Windows 上是 %APPDATA%\TrailCanvas。
    const QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    ensureWritable(roaming);
    return roaming;
}

inline QString K(const char *s) { return QString::fromLatin1(s); }

} // namespace

Prefs &Prefs::instance()
{
    static Prefs p;
    return p;
}

Prefs::Prefs()
    : m_dataDir(chooseDataDir())
    , m_settings(new QSettings(m_dataDir + QStringLiteral("/trailcanvas.ini"), QSettings::IniFormat))
{
}

Prefs::~Prefs()
{
    delete m_settings;
}

DocSettings Prefs::docDefaults() const
{
    DocSettings s;
    s.directed = m_settings->value(K("canvas/directed"), true).toBool();
    const QString bg = m_settings->value(K("canvas/background")).toString();
    if (bg == QLatin1String("plain") || bg == QLatin1String("dots") || bg == QLatin1String("lines"))
        s.background = bg;
    const QString th = m_settings->value(K("canvas/theme")).toString();
    if (th == QLatin1String("light") || th == QLatin1String("dark"))
        s.theme = th;
    return s;
}

void Prefs::setDocDefaults(const DocSettings &s)
{
    m_settings->setValue(K("canvas/directed"), s.directed);
    m_settings->setValue(K("canvas/background"), s.background);
    m_settings->setValue(K("canvas/theme"), s.theme);
    m_settings->sync();
}

bool Prefs::snapEnabled() const
{
    return m_settings->value(K("canvas/snap"), false).toBool();
}

void Prefs::setSnapEnabled(bool on)
{
    m_settings->setValue(K("canvas/snap"), on);
    m_settings->sync();
}

bool Prefs::reduceMotion() const
{
    return m_settings->value(K("ui/reduceMotion"), false).toBool();
}

void Prefs::setReduceMotion(bool on)
{
    m_settings->setValue(K("ui/reduceMotion"), on);
    m_settings->sync();
}

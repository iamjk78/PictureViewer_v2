#include "app/ProfileManager.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace {

constexpr auto kActiveKey       = "General/active";
constexpr auto kNamesKey        = "Profiles/names";

constexpr int  kMaxNameLength   = 50;
const QString  kInvalidChars    = QStringLiteral("/\\:*?\"<>|");

} // namespace

namespace pictureviewer {

ProfileManager::ProfileManager(const QString &appConfigDir)
    : m_appConfigDir(appConfigDir)
{
    m_profilesIniPath = m_appConfigDir + QStringLiteral("/profiles.ini");
    m_profilesDir     = m_appConfigDir + QStringLiteral("/profiles");
    load();
}

// ── Přístup ────────────────────────────────────────────────────────────────

QStringList ProfileManager::profiles() const
{
    return m_profiles;
}

QString ProfileManager::activeProfile() const
{
    return m_activeProfile;
}

QString ProfileManager::profileDir(const QString &profileName) const
{
    return m_profilesDir + QStringLiteral("/") + profileName + QStringLiteral("/");
}

QString ProfileManager::configPath(const QString &profileName) const
{
    return profileDir(profileName) + QStringLiteral("config.ini");
}

QString ProfileManager::dbPath(const QString &profileName) const
{
    return profileDir(profileName) + QStringLiteral("categories.db");
}

// ── Načtení / uložení ────────────────────────────────────────────────────────

void ProfileManager::load()
{
    if (!QFile::exists(m_profilesIniPath)) {
        // První spuštění — inicializovat s výchozím profilem.
        m_profiles = {QString::fromUtf8(kDefaultProfileName)};
        m_activeProfile = QString::fromUtf8(kDefaultProfileName);
        QDir().mkpath(profileDir(m_activeProfile));
        save();
        return;
    }

    QSettings ini(m_profilesIniPath, QSettings::IniFormat);
    m_activeProfile = ini.value(kActiveKey).toString();
    m_profiles      = ini.value(kNamesKey).toStringList();

    // Sanity — nikdy nezůstat bez profilu.
    if (m_profiles.isEmpty()) {
        m_profiles = {QString::fromUtf8(kDefaultProfileName)};
    }
    if (m_activeProfile.isEmpty() || !m_profiles.contains(m_activeProfile)) {
        m_activeProfile = m_profiles.first();
    }

    // Zajistit, že adresář aktivního profilu existuje.
    QDir().mkpath(profileDir(m_activeProfile));
}

void ProfileManager::save()
{
    QDir().mkpath(QFileInfo(m_profilesIniPath).absolutePath());
    QSettings ini(m_profilesIniPath, QSettings::IniFormat);
    ini.setValue(kActiveKey, m_activeProfile);
    ini.setValue(kNamesKey, m_profiles);
    ini.sync();
}

// ── Migrace ──────────────────────────────────────────────────────────────────

void ProfileManager::migrateIfNeeded()
{
    // Migrujeme jen pokud profiles.ini ještě neexistuje (tj. úplně první start
    // s touto verzí) a zároveň existuje stará plochá struktura.
    if (QFile::exists(m_profilesIniPath)) {
        // Profiles už existují — pokud i tak leží stará plochá data vedle,
        // nemigrujeme (mohlo by přepsat profil). Konec.
        // Nicméně load() už proběhl v konstruktoru; nic neděláme.
    }

    const QString oldConfig = m_appConfigDir + QStringLiteral("/config.ini");
    const QString oldDb     = m_appConfigDir + QStringLiteral("/categories.db");

    const bool hasOldConfig = QFile::exists(oldConfig);
    const bool hasOldDb     = QFile::exists(oldDb);

    if (!hasOldConfig && !hasOldDb) {
        return;   // není co migrovat
    }

    const QString target = QString::fromUtf8(kDefaultProfileName);
    QDir().mkpath(profileDir(target));

    if (hasOldConfig) {
        const QString dst = configPath(target);
        if (!QFile::exists(dst)) {
            QFile::rename(oldConfig, dst);
        }
    }
    if (hasOldDb) {
        const QString dst = dbPath(target);
        if (!QFile::exists(dst)) {
            QFile::rename(oldDb, dst);
        }
    }

    // Ujistit se, že výchozí profil je v seznamu a uložit stav.
    if (!m_profiles.contains(target)) {
        m_profiles.prepend(target);
    }
    if (m_activeProfile.isEmpty()) {
        m_activeProfile = target;
    }
    save();
}

// ── Validace ──────────────────────────────────────────────────────────────────

bool ProfileManager::nameIsValid(const QString &name) const
{
    if (name.isEmpty()) {
        return false;
    }
    if (name.length() > kMaxNameLength) {
        return false;
    }
    for (const QChar &c : name) {
        if (kInvalidChars.contains(c)) {
            return false;
        }
    }
    return true;
}

// ── Kopírování adresáře ────────────────────────────────────────────────────────

bool ProfileManager::copyDir(const QString &src, const QString &dst) const
{
    QDir srcDir(src);
    if (!srcDir.exists()) {
        return false;
    }
    QDir().mkpath(dst);

    const QFileInfoList entries =
        srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &fi : entries) {
        const QString target = dst + QStringLiteral("/") + fi.fileName();
        if (fi.isDir()) {
            if (!copyDir(fi.absoluteFilePath(), target)) {
                return false;
            }
        } else {
            QFile::remove(target);   // přepsat, pokud existuje
            if (!QFile::copy(fi.absoluteFilePath(), target)) {
                return false;
            }
        }
    }
    return true;
}

// ── CRUD ──────────────────────────────────────────────────────────────────────

bool ProfileManager::createProfile(const QString &name, bool copyFromActive)
{
    m_lastError.clear();

    if (!nameIsValid(name)) {
        m_lastError = QStringLiteral("Neplatný název profilu.");
        return false;
    }
    if (m_profiles.contains(name)) {
        m_lastError = QStringLiteral("Profil s tímto názvem již existuje.");
        return false;
    }

    const QString dir = profileDir(name);
    if (!QDir().mkpath(dir)) {
        m_lastError = QStringLiteral("Nelze vytvořit adresář profilu.");
        return false;
    }

    if (copyFromActive && !m_activeProfile.isEmpty()) {
        if (!copyDir(profileDir(m_activeProfile), dir)) {
            m_lastError = QStringLiteral("Nelze zkopírovat data z aktivního profilu.");
            return false;
        }
    }

    m_profiles.append(name);
    save();
    return true;
}

bool ProfileManager::renameProfile(const QString &oldName, const QString &newName)
{
    m_lastError.clear();

    if (!m_profiles.contains(oldName)) {
        m_lastError = QStringLiteral("Profil neexistuje.");
        return false;
    }
    if (oldName == newName) {
        return true;
    }
    if (!nameIsValid(newName)) {
        m_lastError = QStringLiteral("Neplatný název profilu.");
        return false;
    }
    if (m_profiles.contains(newName)) {
        m_lastError = QStringLiteral("Profil s tímto názvem již existuje.");
        return false;
    }

    const QString oldDir = m_profilesDir + QStringLiteral("/") + oldName;
    const QString newDir = m_profilesDir + QStringLiteral("/") + newName;
    if (QDir(oldDir).exists()) {
        if (!QDir().rename(oldDir, newDir)) {
            m_lastError = QStringLiteral("Nelze přejmenovat adresář profilu.");
            return false;
        }
    } else {
        QDir().mkpath(newDir);
    }

    const int idx = m_profiles.indexOf(oldName);
    m_profiles[idx] = newName;
    if (m_activeProfile == oldName) {
        m_activeProfile = newName;
    }
    save();
    return true;
}

bool ProfileManager::deleteProfile(const QString &name)
{
    m_lastError.clear();

    if (m_profiles.size() == 1) {
        m_lastError = QStringLiteral("Nelze smazat poslední profil.");
        return false;
    }
    if (!m_profiles.contains(name)) {
        m_lastError = QStringLiteral("Profil neexistuje.");
        return false;
    }

    const QString dir = m_profilesDir + QStringLiteral("/") + name;
    QDir(dir).removeRecursively();

    m_profiles.removeAll(name);

    if (m_activeProfile == name) {
        m_activeProfile = m_profiles.first();
        QDir().mkpath(profileDir(m_activeProfile));
    }
    save();
    return true;
}

bool ProfileManager::duplicateProfile(const QString &srcName, const QString &newName)
{
    m_lastError.clear();

    if (!m_profiles.contains(srcName)) {
        m_lastError = QStringLiteral("Zdrojový profil neexistuje.");
        return false;
    }
    if (!nameIsValid(newName)) {
        m_lastError = QStringLiteral("Neplatný název profilu.");
        return false;
    }
    if (m_profiles.contains(newName)) {
        m_lastError = QStringLiteral("Profil s tímto názvem již existuje.");
        return false;
    }

    const QString dst = profileDir(newName);
    QDir().mkpath(dst);
    if (!copyDir(profileDir(srcName), dst)) {
        m_lastError = QStringLiteral("Nelze zkopírovat data profilu.");
        return false;
    }

    m_profiles.append(newName);
    save();
    return true;
}

void ProfileManager::setActiveProfile(const QString &name)
{
    if (!m_profiles.contains(name)) {
        return;
    }
    m_activeProfile = name;
    QDir().mkpath(profileDir(m_activeProfile));
    save();
}

} // namespace pictureviewer

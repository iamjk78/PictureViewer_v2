#pragma once
#include <QString>
#include <QStringList>

namespace pictureviewer {

class ProfileManager {
public:
    explicit ProfileManager(const QString &appConfigDir);

    // Přístup k profilům
    QStringList profiles() const;
    QString activeProfile() const;

    // Cesty pro aktivní nebo konkrétní profil
    QString configPath(const QString &profileName) const;
    QString dbPath(const QString &profileName) const;
    QString profileDir(const QString &profileName) const;

    // CRUD
    bool createProfile(const QString &name, bool copyFromActive = false);
    bool renameProfile(const QString &oldName, const QString &newName);
    bool deleteProfile(const QString &name);   // selže pokud je jediný
    bool duplicateProfile(const QString &srcName, const QString &newName);
    void setActiveProfile(const QString &name);

    QString lastError() const { return m_lastError; }

    // Migrace ze staré ploché struktury (config.ini + categories.db → profiles/Výchozí/)
    void migrateIfNeeded();

    static constexpr const char *kDefaultProfileName = "Výchozí";

private:
    void load();
    void save();
    bool nameIsValid(const QString &name) const;
    bool copyDir(const QString &src, const QString &dst) const;

    QString m_appConfigDir;
    QString m_profilesIniPath;
    QString m_profilesDir;
    QString m_activeProfile;
    QStringList m_profiles;
    QString m_lastError;
};

} // namespace pictureviewer

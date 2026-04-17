#pragma once

#include <QDateTime>
#include <QString>

class QSettings;

namespace pictureviewer {

class SettingsManager
{
public:
    SettingsManager();
    ~SettingsManager();

    // Returns the absolute path to config.ini (useful for diagnostics).
    static QString configFilePath();

    // ── General ──────────────────────────────────────────────────────────────
    QString lastFolder() const;
    bool rememberLastFolder() const;
    void clearLastFolder();
    void setLastFolder(const QString &folderPath);
    void setRememberLastFolder(bool enabled);

    // ── Updates ──────────────────────────────────────────────────────────────
    int updateCheckDelayMinutes() const;
    int updateCheckIntervalDays() const;
    QDateTime lastUpdateCheck() const;
    void setUpdateCheckDelayMinutes(int minutes);
    void setUpdateCheckIntervalDays(int days);
    void setLastUpdateCheck(const QDateTime &dt);

    // ── File Handling ────────────────────────────────────────────────────────
    bool enableDeleteImage() const;
    void setEnableDeleteImage(bool enabled);
    bool enableMoveToDelete() const;
    void setEnableMoveToDelete(bool enabled);
    bool askConfirmationDelete() const;
    void setAskConfirmationDelete(bool enabled);

    // ── VLC Settings ─────────────────────────────────────────────────────────
    QString vlcPath() const;
    void setVlcPath(const QString &path);
    int vlcTimeoutMs() const;
    void setVlcTimeoutMs(int ms);

private:
    QSettings *m_settings;
};

} // namespace pictureviewer

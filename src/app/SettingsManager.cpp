#include "app/SettingsManager.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

// ── Key names ────────────────────────────────────────────────────────────────
// Group prefix ("General/", "Updates/") produces named INI sections, making
// the file human-readable and easy to edit by hand:
//
//   [General]
//   remember_last_folder=false
//   last_folder=
//
//   [Updates]
//   update_check_delay_minutes=5
//   update_check_interval_days=1
//   last_update_check=

constexpr auto kRememberLastFolderKey    = "General/remember_last_folder";
constexpr auto kLastFolderKey            = "General/last_folder";

constexpr auto kUpdateDelayMinutesKey    = "Updates/update_check_delay_minutes";
constexpr auto kUpdateIntervalDaysKey    = "Updates/update_check_interval_days";
constexpr auto kLastUpdateCheckKey       = "Updates/last_update_check";

constexpr auto kEnableDeleteImageKey     = "FileHandling/enable_delete_image";
constexpr auto kEnableMoveToDeleteKey    = "FileHandling/enable_move_to_delete";
constexpr auto kAskConfirmationDeleteKey = "FileHandling/ask_confirmation_delete";

constexpr auto kVlcPathKey               = "VLC/vlc_path";
constexpr auto kVlcTimeoutMsKey          = "VLC/vlc_timeout_ms";

constexpr int kDefaultUpdateDelayMinutes = 5;
constexpr int kDefaultUpdateIntervalDays = 1;
constexpr int kDefaultVlcTimeoutMs       = 5000;

} // namespace

namespace pictureviewer {

// static
QString SettingsManager::configFilePath()
{
    // QStandardPaths uses the org/app names set on QCoreApplication:
    //   macOS   → ~/Library/Preferences/JiriKrejci/PictureViewer/config.ini
    //   Windows → %APPDATA%\JiriKrejci\PictureViewer\config.ini
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + "/config.ini";
}

SettingsManager::SettingsManager()
{
    const QString path = configFilePath();
    // Ensure the directory exists before handing the path to QSettings.
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_settings = new QSettings(path, QSettings::IniFormat);
}

SettingsManager::~SettingsManager()
{
    delete m_settings;
}

// ── General ──────────────────────────────────────────────────────────────────

QString SettingsManager::lastFolder() const
{
    return m_settings->value(kLastFolderKey, QString()).toString();
}

bool SettingsManager::rememberLastFolder() const
{
    return m_settings->value(kRememberLastFolderKey, false).toBool();
}

void SettingsManager::clearLastFolder()
{
    m_settings->remove(kLastFolderKey);
}

void SettingsManager::setLastFolder(const QString &folderPath)
{
    m_settings->setValue(kLastFolderKey, folderPath);
}

void SettingsManager::setRememberLastFolder(bool enabled)
{
    m_settings->setValue(kRememberLastFolderKey, enabled);
}

// ── Updates ──────────────────────────────────────────────────────────────────

int SettingsManager::updateCheckDelayMinutes() const
{
    return m_settings->value(kUpdateDelayMinutesKey, kDefaultUpdateDelayMinutes).toInt();
}

int SettingsManager::updateCheckIntervalDays() const
{
    return m_settings->value(kUpdateIntervalDaysKey, kDefaultUpdateIntervalDays).toInt();
}

QDateTime SettingsManager::lastUpdateCheck() const
{
    const QString s = m_settings->value(kLastUpdateCheckKey, QString()).toString();
    if (s.isEmpty()) {
        return QDateTime{};
    }
    return QDateTime::fromString(s, Qt::ISODate);
}

void SettingsManager::setUpdateCheckDelayMinutes(int minutes)
{
    m_settings->setValue(kUpdateDelayMinutesKey, minutes);
}

void SettingsManager::setUpdateCheckIntervalDays(int days)
{
    m_settings->setValue(kUpdateIntervalDaysKey, days);
}

void SettingsManager::setLastUpdateCheck(const QDateTime &dt)
{
    m_settings->setValue(kLastUpdateCheckKey, dt.toString(Qt::ISODate));
}

// ── File Handling ────────────────────────────────────────────────────────────

bool SettingsManager::enableDeleteImage() const
{
    return m_settings->value(kEnableDeleteImageKey, false).toBool();
}

void SettingsManager::setEnableDeleteImage(bool enabled)
{
    m_settings->setValue(kEnableDeleteImageKey, enabled);
}

bool SettingsManager::enableMoveToDelete() const
{
    return m_settings->value(kEnableMoveToDeleteKey, false).toBool();
}

void SettingsManager::setEnableMoveToDelete(bool enabled)
{
    m_settings->setValue(kEnableMoveToDeleteKey, enabled);
}

bool SettingsManager::askConfirmationDelete() const
{
    return m_settings->value(kAskConfirmationDeleteKey, false).toBool();
}

void SettingsManager::setAskConfirmationDelete(bool enabled)
{
    m_settings->setValue(kAskConfirmationDeleteKey, enabled);
}

// ── VLC Settings ─────────────────────────────────────────────────────────────

QString SettingsManager::vlcPath() const
{
    return m_settings->value(kVlcPathKey, QString()).toString();
}

void SettingsManager::setVlcPath(const QString &path)
{
    m_settings->setValue(kVlcPathKey, path);
}

int SettingsManager::vlcTimeoutMs() const
{
    return m_settings->value(kVlcTimeoutMsKey, kDefaultVlcTimeoutMs).toInt();
}

void SettingsManager::setVlcTimeoutMs(int ms)
{
    m_settings->setValue(kVlcTimeoutMsKey, ms);
}

} // namespace pictureviewer

#include "app/SettingsManager.hpp"

#include <QSettings>

namespace {

constexpr auto kRememberLastFolderKey = "remember_last_folder";
constexpr auto kLastFolderKey = "last_folder";

} // namespace

namespace pictureviewer {

SettingsManager::SettingsManager()
    : m_settings(new QSettings())
{
}

SettingsManager::~SettingsManager()
{
    delete m_settings;
}

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

} // namespace pictureviewer

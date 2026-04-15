#pragma once

#include <QString>

class QSettings;

namespace pictureviewer {

class SettingsManager
{
public:
    SettingsManager();
    ~SettingsManager();

    QString lastFolder() const;
    bool rememberLastFolder() const;

    void clearLastFolder();
    void setLastFolder(const QString &folderPath);
    void setRememberLastFolder(bool enabled);

private:
    QSettings *m_settings;
};

} // namespace pictureviewer

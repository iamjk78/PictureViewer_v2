#pragma once

#include <QStringList>

class QFileInfo;

namespace pictureviewer {

class ImageCatalog
{
public:
    QStringList loadFolder(const QString &folderPath) const;
    bool isSupported(const QFileInfo &fileInfo) const;
};

} // namespace pictureviewer

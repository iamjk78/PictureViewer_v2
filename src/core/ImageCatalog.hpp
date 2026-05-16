#pragma once

#include <QStringList>

class QFileInfo;

namespace pictureviewer {

class ImageCatalog
{
public:
    // loadFolder: if includePdf=true, both images and PDFs are included
    QStringList loadFolder(const QString &folderPath, bool includePdf = true) const;
    bool isSupported(const QFileInfo &fileInfo, bool includePdf = true) const;
};

} // namespace pictureviewer

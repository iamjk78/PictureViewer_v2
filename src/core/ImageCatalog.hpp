#pragma once

#include <QStringList>

class QFileInfo;

namespace pictureviewer {

// Kritérium řazení souborů ve složce.
enum class SortKey {
    Name,   // přirozené řazení podle názvu (img2 < img10)
    Date,   // podle času poslední změny
    Size,   // podle velikosti souboru
};

class ImageCatalog
{
public:
    // loadFolder: if includePdf=true, both images and PDFs are included.
    // sortKey + ascending určují pořadí výsledného seznamu.
    QStringList loadFolder(const QString &folderPath,
                           bool includePdf = true,
                           SortKey sortKey = SortKey::Name,
                           bool ascending = true) const;
    bool isSupported(const QFileInfo &fileInfo, bool includePdf = true) const;
};

} // namespace pictureviewer

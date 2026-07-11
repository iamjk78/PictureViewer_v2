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
    // loadFolder: řídí co se načítá pomocí bool přepínačů.
    // sortKey + ascending určují pořadí výsledného seznamu.
    // Filtrování podle štítků se dělá až nad výsledkem v app vrstvě
    // (MainWindow::onScanComplete) — core nesmí záviset na CategoryManager.
    QStringList loadFolder(const QString &folderPath,
                           bool includePdf = true,
                           SortKey sortKey = SortKey::Name,
                           bool ascending = true,
                           bool includeImages = true,
                           bool includeVideos = false) const;
    bool isSupported(const QFileInfo &fileInfo,
                     bool includePdf = true,
                     bool includeImages = true,
                     bool includeVideos = false) const;
};

} // namespace pictureviewer

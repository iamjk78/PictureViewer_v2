#pragma once

#include <QStringList>

#include <functional>

class QFileInfo;

namespace pictureviewer {

class FileStampIndex;

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
                           bool includeVideos = false,
                           FileStampIndex *stamps = nullptr) const;
    // Postupné načítání pro pomalá (síťová) úložiště: výpis složky se čte po
    // dávkách a každá se hned předá onBatch() (v pořadí, v jakém ji úložiště
    // vrací — nesetříděné), takže volající může první soubory ukázat dřív,
    // než se dočte celá složka. Na konci vrátí kompletní seznam seřazený
    // podle jména (bez dalšího čtení z disku). isCancelled se kontroluje při
    // KAŽDÉM souboru; po zrušení vrací prázdný seznam a už nic nevydá.
    // Dávka se vydá po maxBatch souborech nebo po flushMs od poslední.
    // Řazení podle data/velikosti potřebuje stat() všech souborů — pro ně
    // použij loadFolder(); tahle varianta řadí jen podle jména.
    // Výpis (na macOS getattrlistbulk, viz BulkDirectoryLister) dodá u každého
    // souboru i čas změny a velikost; pokud je zadán stamps, uloží se tam,
    // aby generátory miniatur nemusely dělat stat() na každý soubor.
    QStringList loadFolderStreaming(const QString &folderPath,
                                    bool includePdf,
                                    bool ascending,
                                    bool includeImages,
                                    bool includeVideos,
                                    const std::function<bool()> &isCancelled,
                                    const std::function<void(const QStringList &)> &onBatch,
                                    int maxBatch = 500,
                                    int flushMs = 300,
                                    FileStampIndex *stamps = nullptr) const;
    // Rozhodnutí podle přípony (bez dotazu na úložiště).
    bool isSupportedSuffix(const QString &suffix,
                           bool includePdf = true,
                           bool includeImages = true,
                           bool includeVideos = false) const;
    bool isSupported(const QFileInfo &fileInfo,
                     bool includePdf = true,
                     bool includeImages = true,
                     bool includeVideos = false) const;
};

} // namespace pictureviewer

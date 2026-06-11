#include "core/ImageCatalog.hpp"

#include "app/CategoryManager.hpp"
#include "core/ImageFormats.hpp"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QString>
#include <algorithm>

#include <stdexcept>

namespace pictureviewer {

QStringList ImageCatalog::loadFolder(const QString &folderPath, bool includePdf,
                                     SortKey sortKey, bool ascending,
                                     const QList<int> &categoryIds,
                                     CategoryManager *categoryManager) const
{
    const QDir directory(folderPath);
    if (!directory.exists()) {
        throw std::runtime_error(QString("Cesta není složka: %1").arg(folderPath).toStdString());
    }

    // Bez řazení přes QDir — třídíme níže sami podle zvoleného kritéria.
    const QFileInfoList entries = directory.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot
    );

    // Pokud máme filtrování podle kategorií, nejdřív získat seznam cest
    QSet<QString> categoryFilteredPaths;
    if (!categoryIds.isEmpty() && categoryManager) {
        categoryFilteredPaths = QSet<QString>(
            categoryManager->imagePathsWithAllCategories(categoryIds).begin(),
            categoryManager->imagePathsWithAllCategories(categoryIds).end()
        );
    }

    // Pracujeme s QFileInfo, ať pro řazení podle data/velikosti nečteme stat
    // v komparátoru opakovaně.
    QFileInfoList supported;
    supported.reserve(entries.size());
    for (const QFileInfo &entry : entries) {
        if (!isSupported(entry, includePdf)) {
            continue;
        }

        // Pokud máme kategoriální filtr, ověřit že cesta je v seznamu
        if (!categoryIds.isEmpty() && !categoryFilteredPaths.contains(entry.absoluteFilePath())) {
            continue;
        }

        supported.append(entry);
    }

    // QCollator s numericMode řadí číselné úseky podle hodnoty (img2 < img10),
    // ne lexikograficky. Použije se pro řazení podle názvu i jako rozhodčí při
    // shodě data/velikosti, aby bylo pořadí stabilní a předvídatelné.
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    auto byName = [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    };

    // Strict-weak-ordering pro vzestupné řazení. Sestupné se získá prohozením
    // argumentů (lessThan(b, a)) — negace (!lessThan) by u shodných klíčů
    // porušila ostré uspořádání a vedla k UB ve std::sort.
    auto lessThan = [&](const QFileInfo &a, const QFileInfo &b) {
        switch (sortKey) {
        case SortKey::Date:
            if (a.lastModified() != b.lastModified()) {
                return a.lastModified() < b.lastModified();
            }
            return byName(a, b);   // shodný čas → podle názvu
        case SortKey::Size:
            if (a.size() != b.size()) {
                return a.size() < b.size();
            }
            return byName(a, b);   // shodná velikost → podle názvu
        case SortKey::Name:
            break;
        }
        return byName(a, b);
    };

    std::sort(supported.begin(), supported.end(),
              [&](const QFileInfo &a, const QFileInfo &b) {
        return ascending ? lessThan(a, b) : lessThan(b, a);
    });

    QStringList files;
    files.reserve(supported.size());
    for (const QFileInfo &info : supported) {
        files.append(info.absoluteFilePath());
    }
    return files;
}

bool ImageCatalog::isSupported(const QFileInfo &fileInfo, bool includePdf) const
{
    if (!fileInfo.isFile()) {
        return false;
    }

    const QString suffix = QStringLiteral(".") + fileInfo.suffix();

    if (isSupportedImageExtension(suffix)) {
        return true;
    }

    if (includePdf && isSupportedDocumentExtension(suffix)) {
        return true;
    }

    return false;
}

} // namespace pictureviewer

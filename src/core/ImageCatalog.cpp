#include "core/ImageCatalog.hpp"

#include "core/Collation.hpp"
#include "core/ImageFormats.hpp"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFileInfoList>
#include <QString>
#include <algorithm>

#include <stdexcept>

namespace pictureviewer {

QStringList ImageCatalog::loadFolder(const QString &folderPath, bool includePdf,
                                     SortKey sortKey, bool ascending,
                                     bool includeImages, bool includeVideos) const
{
    const QDir directory(folderPath);
    if (!directory.exists()) {
        throw std::runtime_error(QString("Cesta není složka: %1").arg(folderPath).toStdString());
    }

    // Bez řazení přes QDir — třídíme níže sami podle zvoleného kritéria.
    const QFileInfoList entries = directory.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot
    );

    // Pracujeme s QFileInfo, ať pro řazení podle data/velikosti nečteme stat
    // v komparátoru opakovaně.
    QFileInfoList supported;
    supported.reserve(entries.size());
    for (const QFileInfo &entry : entries) {
        if (!isSupported(entry, includePdf, includeImages, includeVideos)) {
            continue;
        }
        supported.append(entry);
    }

    // Přirozené locale-aware řazení (sdílený collator — viz core/Collation.hpp).
    const QCollator collator = makeNaturalCollator();

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

bool ImageCatalog::isSupported(const QFileInfo &fileInfo,
                               bool includePdf,
                               bool includeImages,
                               bool includeVideos) const
{
    if (!fileInfo.isFile()) {
        return false;
    }
    return isSupportedSuffix(fileInfo.suffix(), includePdf, includeImages, includeVideos);
}

bool ImageCatalog::isSupportedSuffix(const QString &rawSuffix,
                                     bool includePdf,
                                     bool includeImages,
                                     bool includeVideos) const
{
    const QString suffix = QStringLiteral(".") + rawSuffix;

    if (includeImages && isSupportedImageExtension(suffix)) {
        return true;
    }

    if (includePdf && isSupportedDocumentExtension(suffix)) {
        return true;
    }

    if (includeVideos && isVideoFile(suffix)) {
        return true;
    }

    return false;
}

QStringList ImageCatalog::loadFolderStreaming(const QString &folderPath,
                                              bool includePdf,
                                              bool ascending,
                                              bool includeImages,
                                              bool includeVideos,
                                              const std::function<bool()> &isCancelled,
                                              const std::function<void(const QStringList &)> &onBatch,
                                              int maxBatch,
                                              int flushMs) const
{
    const QDir directory(folderPath);
    if (!directory.exists()) {
        throw std::runtime_error(QString("Cesta není složka: %1").arg(folderPath).toStdString());
    }

    QStringList all;
    QStringList batch;
    QElapsedTimer sinceFlush;
    sinceFlush.start();

    // Jen názvy — žádné QFileInfo/stat na soubor (typ dodá samotný výpis).
    QDirIterator it(directory.absolutePath(), QDir::Files | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        if (isCancelled && isCancelled()) {
            return {};
        }
        it.next();
        const QString name = it.fileName();
        if (!isSupportedSuffix(QFileInfo(name).suffix(), includePdf, includeImages, includeVideos)) {
            continue;
        }
        const QString path = directory.absoluteFilePath(name);
        all.append(path);
        batch.append(path);
        if (batch.size() >= maxBatch || sinceFlush.elapsed() >= flushMs) {
            if (onBatch) {
                onBatch(batch);
            }
            batch.clear();
            sinceFlush.restart();
        }
    }
    if (!batch.isEmpty() && onBatch && !(isCancelled && isCancelled())) {
        onBatch(batch);
    }
    if (isCancelled && isCancelled()) {
        return {};
    }

    const QCollator collator = makeNaturalCollator();
    std::sort(all.begin(), all.end(), [&](const QString &a, const QString &b) {
        const int cmp = collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName());
        return ascending ? cmp < 0 : cmp > 0;
    });
    return all;
}

} // namespace pictureviewer

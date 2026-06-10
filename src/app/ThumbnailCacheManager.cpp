#include "app/ThumbnailCacheManager.hpp"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <algorithm>

namespace pictureviewer {

qint64 ThumbnailCacheManager::calculateCacheSize(const QString &cacheDir)
{
    QDir dir(cacheDir);
    if (!dir.exists()) {
        return 0;
    }

    qint64 totalSize = 0;
    QDirIterator it(cacheDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        totalSize += it.fileInfo().size();
    }

    return totalSize;
}

void ThumbnailCacheManager::cleanupIfNeeded(const QString &cacheDir)
{
    QDir dir(cacheDir);
    if (!dir.exists()) {
        return;
    }

    // Spočítat aktuální velikost cache
    qint64 totalSize = calculateCacheSize(cacheDir);

    // Jenom mazat pokud cache >= 500 MB
    if (totalSize < CacheLimitBytes) {
        return;
    }

    // Sbírat všechny cache soubory s jejich metadata
    struct CacheFile {
        QString path;
        qint64 size;
        qint64 lastModifiedTime;
    };

    QList<CacheFile> files;
    const qint64 thirtyDaysAgo = QDateTime::currentSecsSinceEpoch() - (DeleteOlderThanDays * 86400);

    QDirIterator it(cacheDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo fileInfo(it.fileInfo());
        qint64 lastModified = fileInfo.lastModified().toSecsSinceEpoch();

        // Jenom zahrnout soubory starší než 30 dní
        if (lastModified < thirtyDaysAgo) {
            CacheFile f;
            f.path = fileInfo.absoluteFilePath();
            f.size = fileInfo.size();
            f.lastModifiedTime = lastModified;
            files.append(f);
        }
    }

    // Pokud nemáme žádné soubory ke smazání, skončit
    if (files.isEmpty()) {
        return;
    }

    // Setřídit podle času přístupu (nejstarší na začátku)
    std::sort(files.begin(), files.end(), [](const CacheFile &a, const CacheFile &b) {
        return a.lastModifiedTime < b.lastModifiedTime;
    });

    // Smazat nejstarší soubory, dokud se nedostaneme pod limit
    qint64 targetSize = CacheLimitBytes * 80 / 100;  // Cíl: 80% limitu (400 MB)
    for (const CacheFile &f : files) {
        if (totalSize <= targetSize) {
            break;
        }
        if (QFile::remove(f.path)) {
            totalSize -= f.size;
        }
    }
}

} // namespace pictureviewer

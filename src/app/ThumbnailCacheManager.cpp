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

    // Pod limitem se nemaže nic — cache se nechá růst až do 500 MB.
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

    QDirIterator it(cacheDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fileInfo(it.fileInfo());
        CacheFile f;
        f.path = fileInfo.absoluteFilePath();
        f.size = fileInfo.size();
        f.lastModifiedTime = fileInfo.lastModified().toSecsSinceEpoch();
        files.append(f);
    }

    // Setřídit podle času změny (nejstarší na začátku)
    std::sort(files.begin(), files.end(), [](const CacheFile &a, const CacheFile &b) {
        return a.lastModifiedTime < b.lastModifiedTime;
    });

    // Mazat nejstarší soubory, dokud cache neklesne pod cílových 80 % limitu
    // (400 MB). Mažeme bez ohledu na stáří — strop musí platit i při intenzivním
    // používání během krátké doby, kdy jsou všechny soubory čerstvé.
    const qint64 targetSize = CacheLimitBytes * 80 / 100;
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

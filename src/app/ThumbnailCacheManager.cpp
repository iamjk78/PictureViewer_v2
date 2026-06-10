#include "app/ThumbnailCacheManager.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <algorithm>

namespace pictureviewer {

void ThumbnailCacheManager::cleanupIfNeeded(const QString &cacheDir)
{
    QDir dir(cacheDir);
    if (!dir.exists()) {
        return;
    }

    // Sbírat všechny cache soubory s jejich metadata
    struct CacheFile {
        QString path;
        qint64 size;
        qint64 lastAccessTime;
    };

    QList<CacheFile> files;
    qint64 totalSize = 0;

    QDirIterator it(cacheDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo fileInfo(it.fileInfo());
        CacheFile f;
        f.path = fileInfo.absoluteFilePath();
        f.size = fileInfo.size();
        f.lastAccessTime = fileInfo.lastModified().toSecsSinceEpoch();
        files.append(f);
        totalSize += f.size;
    }

    // Pokud jsme pod limitem, nic nedělej
    if (totalSize <= CacheLimitBytes) {
        return;
    }

    // Setřídit podle času přístupu (nejstarší na začátku)
    std::sort(files.begin(), files.end(), [](const CacheFile &a, const CacheFile &b) {
        return a.lastAccessTime < b.lastAccessTime;
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

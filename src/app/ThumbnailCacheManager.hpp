#pragma once

#include <QString>

namespace pictureviewer {

class ThumbnailCacheManager
{
public:
    // Vyčistit diskovou cache náhledů pokud překročila limit (500 MB)
    // Smaže nejstarší soubory dokud se nedostane pod limit
    static void cleanupIfNeeded(const QString &cacheDir);

private:
    static constexpr qint64 CacheLimitBytes = 500 * 1024 * 1024;  // 500 MB
};

} // namespace pictureviewer

#pragma once

#include <QString>

namespace pictureviewer {

class ThumbnailCacheManager
{
public:
    // Spočítat celkovou velikost cache v bytes
    static qint64 calculateCacheSize(const QString &cacheDir);

    // Vyčistit diskovou cache náhledů pokud překročila limit (500 MB)
    // Smaže nejstarší soubory starší než 30 dní dokud se nedostane pod limit
    static void cleanupIfNeeded(const QString &cacheDir);

private:
    static constexpr qint64 CacheLimitBytes = 500 * 1024 * 1024;  // 500 MB
    static constexpr int DeleteOlderThanDays = 30;
};

} // namespace pictureviewer

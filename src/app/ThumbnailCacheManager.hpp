#pragma once

#include <QString>

namespace pictureviewer {

class ThumbnailCacheManager
{
public:
    // Spočítat celkovou velikost cache v bytes
    static qint64 calculateCacheSize(const QString &cacheDir);

    // Vyčistit diskovou cache náhledů, pokud dosáhla limitu (500 MB).
    // Pod limitem se nemaže nic; při dosažení limitu se mažou nejstarší soubory
    // (bez ohledu na stáří), dokud cache neklesne pod cílových 80 % limitu.
    static void cleanupIfNeeded(const QString &cacheDir);

private:
    static constexpr qint64 CacheLimitBytes = 500 * 1024 * 1024;  // 500 MB
};

} // namespace pictureviewer

#pragma once

#include <QCache>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>

namespace pictureviewer {

// Asynchronní dekodér obrázků s RAM cache (LRU) a prefetchem.
//
// Cache klíč je (cesta + mtime), takže externí změna souboru přirozeně
// zneplatní starý záznam. Cost je v kB dekódovaných dat; výchozí limit
// 256 MB ≈ 5 fotek z mobilu v plném rozlišení.
//
// Použití: zavolat cachedImage() — při hitu zobrazit hned; při missu
// zavolat request() a výsledek přijde signálem imageReady().
class ImageLoader : public QObject
{
    Q_OBJECT

public:
    static constexpr int DefaultCacheLimitKb = 256 * 1024;

    explicit ImageLoader(QObject *parent = nullptr);

    // Vrátí dekódovaný obrázek z cache, nebo null QImage při missu.
    QImage cachedImage(const QString &path) const;

    // Asynchronně dekóduje soubor; po dokončení emituje imageReady().
    // No-op, pokud už dekódování stejné cesty běží.
    void request(const QString &path);

    // Tiché přednačtení do cache (bez imageReady pro UI — signál se emituje,
    // ale volající ho ignoruje díky kontrole aktuální cesty).
    void prefetch(const QStringList &paths);

    // Zastaví doručování výsledků; běžící dekódování doběhne naprázdno.
    void shutdown();

signals:
    void imageReady(const QString &path, const QImage &image);

private:
    static QString cacheKey(const QString &path);
    void startDecode(const QString &path);

    mutable QCache<QString, QImage> m_cache;
    QSet<QString> m_inFlight;
    bool m_shuttingDown = false;
};

} // namespace pictureviewer

#pragma once

#include <QCache>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

template <typename T>
class QFutureWatcher;

namespace pictureviewer {

// Asynchronní dekodér obrázků s RAM cache (LRU) a prefetchem.
//
// Cache je klíčovaná CESTOU — dotaz z UI vlákna tak nesahá na úložiště (stat()
// přes síť stojí desetiny sekundy). U záznamu je uložený otisk souboru
// (mtime + velikost) z okamžiku dekódování; při každém zásahu se na pozadí
// porovná s aktuálním stavem souboru a změněný soubor se zahodí a ohlásí
// signálem imageChangedOnDisk(). Cost je v kB dekódovaných dat; výchozí limit
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
    ~ImageLoader();

    // Vrátí dekódovaný obrázek z cache, nebo null QImage při missu.
    // Zásah spustí na pozadí kontrolu, zda se soubor mezitím nezměnil.
    QImage cachedImage(const QString &path);

    // Asynchronně dekóduje soubor; po dokončení emituje imageReady().
    // No-op, pokud už dekódování stejné cesty běží.
    void request(const QString &path);

    // Tiché přednačtení do cache (bez imageReady pro UI — signál se emituje,
    // ale volající ho ignoruje díky kontrole aktuální cesty). Dokud běží jiné
    // dekódování (typicky právě zobrazovaný obrázek), přednačítání počká —
    // na pomalém úložišti by jinak soupeřilo o stejné spojení.
    void prefetch(const QStringList &paths);

    // Během čtení výpisu složky se nepřednačítá (soupeřilo by o stejné síťové
    // spojení); čekající přednačtení naváže po obnovení.
    void setPrefetchPaused(bool paused);

    // Zastaví doručování výsledků; běžící dekódování doběhne naprázdno.
    void shutdown();

    // Otisk souboru: cesta + mtime + velikost (stat). Veřejné kvůli testům.
    static QString cacheKey(const QString &path);

signals:
    void imageReady(const QString &path, const QImage &image);
    // true = právě běží (nebo přibylo) dekódování; false = žádné neběží.
    // Slouží k tomu, aby práce na pozadí (zahřívání cache miniatur) ustoupila,
    // dokud se prohlížený obrázek načítá — na pomalém úložišti by jinak
    // soupeřila o stejné spojení.
    void busyChanged(bool busy);
    // Soubor, který byl v cache, se na disku změnil; záznam je zahozen.
    void imageChangedOnDisk(const QString &path);

private:
    struct Entry {
        QImage image;
        QString stamp;   // cacheKey() v okamžiku dekódování
    };
    struct Decoded {
        QImage image;
        QString stamp;
    };

    void startDecode(const QString &path);
    void validateAsync(const QString &path, const QString &stamp);
    void startPendingPrefetch();

    QCache<QString, Entry> m_cache;
    QSet<QString> m_inFlight;
    QSet<QString> m_validating;
    QStringList m_pendingPrefetch;
    bool m_shuttingDown = false;
    bool m_prefetchPaused = false;
    QList<QFutureWatcher<Decoded>*> m_watchers;   // sledují běžící futures
    QList<QFutureWatcher<bool>*> m_validationWatchers;
};

} // namespace pictureviewer

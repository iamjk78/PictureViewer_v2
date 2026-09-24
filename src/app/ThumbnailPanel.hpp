#pragma once

#include <QHash>
#include <QListWidget>
#include <QPersistentModelIndex>
#include <QSet>
#include <QSharedPointer>
#include <QStringList>
#include <QThreadPool>

class QImage;
class QListWidgetItem;
class QShowEvent;
class QTimer;

namespace pictureviewer {

class ThumbnailClaims;
class ThumbnailWorker;

class ThumbnailPanel : public QListWidget
{
    Q_OBJECT

public:
    // Vertical   – sloupec v levém docku (klasické rozložení)
    // Horizontal – filmový pás ve spodním docku
    // Grid       – mřížka přes celé okno (režim Galerie)
    enum class DisplayMode { Vertical, Horizontal, Grid };

    explicit ThumbnailPanel(QWidget *parent = nullptr);
    ~ThumbnailPanel() override;

    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return m_displayMode; }

    // Konfigurace diskové cache miniatur; projeví se při dalším loadImages().
    void setDiskCache(bool enabled, const QString &cacheDir);

    // Nahradí seznam položek. Miniatury se NEgenerují hned pro všechny —
    // panel si sám hlídá, které položky jsou vidět (plus okolí ve směru
    // posouvání), a generuje je jen pro ně (viz updateWantedThumbnails()).
    // Na pomalém/síťovém úložišti by jinak čtení miniatur celé složky trvalo
    // desítky minut a brzdilo i právě prohlížený obrázek.
    void loadImages(const QStringList &paths);
    void setCurrentIndex(int index);
    void removeImage(int index);
    void updateImagePath(const QString &oldPath, const QString &newPath);
    QIcon iconAt(int index) const;   // náhled pro placeholder při async načítání

    // Indexy aktuálně vybraných položek (Ctrl/Shift+klik), setříděné vzestupně.
    QList<int> selectedIndices() const;

    // Generace aktuálně zobrazeného seznamu (bump při každém loadImages()).
    // VideoThumbnailWorker si ji předá při enqueue() a posílá zpátky s každým
    // thumbnailReady — jinak by zastaralá/pozdě doručená miniatura z PŘEDCHOZÍHO
    // seznamu (např. po rychlém přepnutí filtru obrázky/videa/PDF) mohla sáhnout
    // na už smazanou QListWidgetItem a spadnout (SIGSEGV).
    int generation() const { return m_generation; }

    // Aktualizuje miniaturu videa (voláno z VideoThumbnailWorker přes MainWindow).
    void setVideoThumbnail(int generation, const QString &path, const QImage &image);

    // Cancel the running worker and disconnect all its signals.
    // Must be called before QThreadPool::waitForDone() so the worker cannot
    // emit into a widget that is about to be destroyed.
    void shutdown();

    // Prohlížeč právě načítá obrázek (true) / nic nenačítá (false). Dokud
    // načítá, zahřívání cache na pozadí stojí — na pomalém úložišti by
    // soupeřilo o stejné spojení a prohlížený obrázek by se načítal
    // násobně déle.
    void setViewerBusy(bool busy);

    // Jen pro testy: zkrátí dobu klidu před zahájením zahřívání cache
    // (obrázky / videa) a škrcení mezi miniaturami (výchozí hodnoty jsou
    // voleny pro pomalé síťové úložiště, ne pro rychlé testy). videoIdleMs < 0
    // = stejné jako idleMs.
    void setWarmupTimingForTesting(int idleMs, int throttleMs, int videoIdleMs = -1);

    // Aktuální velikost miniatury v pixelech (= šířka docku − 24).
    // V Horizontal/Grid režimu vrací výchozí hodnotu.
    int thumbSize() const { return m_thumbSize; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    QSize sizeHint() const override;

signals:
    void imageSelected(int index);
    // Videa, jejichž miniatury se mají generovat, seřazená podle priority
    // (VideoThumbnailWorker::retarget() jimi nahradí svou frontu): prvních
    // foregroundCount je viditelných (hned), zbytek je zahřívání cache při
    // plném klidu (pomalu, přerušitelně). Prázdný seznam = nic není potřeba.
    void videoThumbnailsWanted(int generation, const QStringList &paths, int foregroundCount);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onThumbnailReady(int generation, const QString &path, const QImage &image);

private:
    // Naplánuje (s krátkým zpožděním, nejvýše jednou za interval) přepočet
    // toho, pro které položky je třeba miniatura — po posunu, změně velikosti,
    // zobrazení panelu nebo načtení nového seznamu. Zároveň je to "uživatel
    // něco dělá" — zahřívání cache na pozadí se pozastaví.
    void scheduleThumbnailUpdate();
    // Zjistí viditelné položky + okolí, přeuspořádá frontu čekajících miniatur
    // (nejdřív viditelné, od středu) a oznámí seznam potřebných videí.
    void updateWantedThumbnails();
    // Spustí další workery, dokud jich neběží kThumbnailThreads.
    void dispatchThumbnails();
    // Zruší a odpojí všechny běžící workery (nový seznam / ukončení).
    void cancelActiveWorkers();
    void applyThumbSize(int size);

    // ── Zahřívání cache na pozadí ────────────────────────────────────────
    // Po chvíli klidu a když popředí nemá práci se pomalu projdou VŠECHNY
    // miniatury složky (od středu ven) a uloží do diskové cache — příště se
    // čtou rychle z ní. Nic z toho nezasahuje do panelu (žádné ikony v
    // paměti) a jakákoli aktivita uživatele to pozastaví.
    void noteActivity();
    void maybeStartWarmup();
    void maybeStartVideoWarmup();
    void emitWantedVideos();

    static constexpr int kWarmupIdleMs = 5000;        // klid před zahřátím obrázků
    static constexpr int kVideoWarmupIdleMs = 30000;  // "plný klid" před zahřátím videí
    static constexpr int kWarmupThrottleMs = 150;     // pauza po vygenerované miniatuře
    int m_warmupIdleMs = kWarmupIdleMs;
    int m_videoWarmupIdleMs = kVideoWarmupIdleMs;
    int m_warmupThrottleMs = kWarmupThrottleMs;
    QTimer *m_videoIdleTimer = nullptr;
    bool m_videoIdleReady = false;
    bool m_videoTailPrepared = false;
    bool m_viewerBusy = false;
    QThreadPool m_warmPool;                         // 1 vlákno, nízká priorita
    ThumbnailWorker *m_warmWorker = nullptr;
    QTimer *m_idleTimer = nullptr;
    bool m_idleReady = false;      // uplynula doba klidu
    bool m_warmPrepared = false;   // pro aktuální seznam už byly seznamy sestaveny a worker spuštěn
    bool m_videoWarmupActive = false;   // videa z ocasu se právě mají generovat
    QStringList m_visibleVideos;   // videa z posledního přepočtu (viditelná + okolí)
    QStringList m_warmVideoTail;   // ostatní videa složky, až za nimi
    int m_centerRow = 0;

    static constexpr int kThumbnailThreads = 3;
    QThreadPool m_thumbPool;
    QSet<ThumbnailWorker *> m_activeWorkers;
    // Cesty, pro které se už miniatura spustila / vygenerovala v této generaci
    // (včetně neúspěšných — jinak by se vadný soubor zkoušel dokola). Sdílené
    // s workerem na pozadí, ať nedělá znovu, co popředí už udělalo.
    QSharedPointer<ThumbnailClaims> m_claims;
    QStringList m_pendingThumbs;   // fronta čekajících, nejdůležitější první
    QStringList m_lastWantedVideos;
    QTimer *m_updateTimer = nullptr;
    int m_generation;
    bool m_shuttingDown = false;
    DisplayMode m_displayMode = DisplayMode::Vertical;
    int m_thumbSize = 96;
    bool m_diskCacheEnabled = true;
    QString m_diskCacheDir;
    // O(1) lookup by path. QPersistentModelIndex (ne surový QListWidgetItem*):
    // přežívá posuny řádků při mazání uprostřed seznamu a při smazání svého
    // řádku se sám zneplatní — asynchronně doručený náhled (ThumbnailWorker /
    // VideoThumbnailWorker) tak NIKDY nemůže sáhnout na uvolněnou položku,
    // ať už ji smazala kterákoli cesta (use-after-free viděný v crash
    // reportech v setVideoThumbnail).
    QHash<QString, QPersistentModelIndex> m_pathToIndex;
    // Bezpečný převod: vrátí položku pro cestu, nebo nullptr (neexistuje /
    // řádek už byl smazán).
    QListWidgetItem *itemForPath(const QString &path) const;
};

} // namespace pictureviewer

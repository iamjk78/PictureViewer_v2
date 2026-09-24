#pragma once

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVideoFrame>

class QMediaPlayer;
class QVideoSink;
class QTimer;
class QImage;

namespace pictureviewer {

class VideoThumbnailWorker : public QObject
{
    Q_OBJECT

public:
    static constexpr int ThumbnailSize = 192;
    static constexpr int kTailPauseMs = 8000;

    explicit VideoThumbnailWorker(bool diskCacheEnabled, QString diskCacheDir,
                                  QObject *parent = nullptr);
    ~VideoThumbnailWorker() override;

    // Nahradí frontu čekajících videí těmito (v pořadí priority) — panel náhledů
    // sem posílá viditelná videa a jejich okolí a při posunu seznam mění.
    // Prvních foregroundCount cest je "popředí" (viditelná videa): zpracují se
    // hned. Zbytek je "ocas" (zahřívání cache při plném klidu): po každé
    // skutečně generované miniatuře se počká kTailPauseMs a rozdělané video
    // z ocasu se ZRUŠÍ, jakmile ho nový seznam už nechce (uživatel se začal
    // něčím zabývat) — jedno video přes síť je desítky MB a 4–11 s čtení.
    // generation se vrátí beze změny v každém thumbnailReady — volající
    // (MainWindow) předá ThumbnailPanel::generation(), aby příjemce poznal a
    // zahodil doručení patřící k už nahrazenému (zastaralému) seznamu souborů.
    // Video, které se v rámci jedné generace už zpracovávalo (i neúspěšně),
    // se znovu nezařadí — jinak by se při každém posunu seznamu opakovaně
    // otevíralo video, které se nedá přehrát, a čekalo se na jeho timeout.
    void retarget(const QStringList &paths, int generation, int foregroundCount);
    void cancel();

    // Pozastaví/obnoví zpracování fronty BEZ jejího zahození (na rozdíl od
    // cancel()). Slouží k vzájemnému vyloučení s VideoPlayerem: dva souběžně
    // pracující QMediaPlayery se na macOS/AVFoundation perou o teardown
    // AVPlayerItem a shazují aplikaci — viz komentář v suspend().
    void suspend();
    void resume();

    // Nastavení diskové cache se dá měnit za běhu (menu Nastavení → Cache
    // miniatur). Bez toho by generátor zůstal u hodnot z konstruktoru a psal
    // do staré složky i po jejím přenastavení nebo vypnutí cache.
    void setDiskCache(bool enabled, const QString &cacheDir);

signals:
    void thumbnailReady(int generation, const QString &path, const QImage &image);

private slots:
    void processNext();
    void onMediaStatusChanged(int status);
    void onVideoFrameChanged(const QVideoFrame &frame);
    void onTimeout();
    void processNextIfIdle();

private:
    QString cacheFilePath(const QString &path) const;
    QImage loadFromCache(const QString &path) const;
    void saveToCache(const QString &path, const QImage &image) const;
    void finishCurrent(const QImage &image);
    // Přeruší rozpracované video BEZ vydání miniatury a zapomene, že se o ně
    // zkoušelo — zkusí se znovu, až bude zase klid.
    void abortCurrent();
    // Vytvoří nový QMediaPlayer/QVideoSink a připojí signály. Volá se z
    // konstruktoru a znovu z discardPlayer() — nikdy se nepokračuje se starým
    // přehrávačem po přerušení rozpracovaného videa.
    void setupPlayer();
    // Zahodí rozpracovaný přehrávač a vyrobí čistý nový (viz komentář uvnitř).
    void discardPlayer();

    enum class State { Idle, Loading, WaitingFrame };

    QMediaPlayer *m_player;
    QVideoSink   *m_sink;
    QTimer       *m_timeoutTimer;
    QStringList   m_queue;
    QSet<QString> m_foreground;   // prvních N z posledního retarget()
    bool          m_currentIsTail = false;
    bool          m_tailPauseActive = false;
    QTimer       *m_tailPauseTimer = nullptr;
    QSet<QString> m_attempted;   // už zpracovaná (ok i neúspěšná) v aktuální generaci
    QString       m_currentPath;
    int           m_generation = 0;
    State         m_state      = State::Idle;
    bool          m_cancelled  = false;
    bool          m_suspended  = false;
    bool          m_diskCacheEnabled;
    QString       m_diskCacheDir;
};

} // namespace pictureviewer

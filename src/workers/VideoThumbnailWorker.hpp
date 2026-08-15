#pragma once

#include <QObject>
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

    explicit VideoThumbnailWorker(bool diskCacheEnabled, QString diskCacheDir,
                                  QObject *parent = nullptr);
    ~VideoThumbnailWorker() override;

    // generation se vrátí beze změny v každém thumbnailReady — volající (MainWindow)
    // předá aktuální ThumbnailPanel::generation(), aby příjemce poznal a zahodil
    // doručení patřící k už nahrazenému (zastaralému) seznamu souborů.
    void enqueue(const QStringList &paths, int generation);
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

private:
    QString cacheFilePath(const QString &path) const;
    QImage loadFromCache(const QString &path) const;
    void saveToCache(const QString &path, const QImage &image) const;
    void finishCurrent(const QImage &image);
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
    QString       m_currentPath;
    int           m_generation = 0;
    State         m_state      = State::Idle;
    bool          m_cancelled  = false;
    bool          m_suspended  = false;
    bool          m_diskCacheEnabled;
    QString       m_diskCacheDir;
};

} // namespace pictureviewer

#include "workers/VideoThumbnailWorker.hpp"

#include "core/DiagLog.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

namespace pictureviewer {

VideoThumbnailWorker::VideoThumbnailWorker(bool diskCacheEnabled, QString diskCacheDir,
                                           QObject *parent)
    : QObject(parent)
    , m_timeoutTimer(new QTimer(this))
    , m_diskCacheEnabled(diskCacheEnabled)
    , m_diskCacheDir(std::move(diskCacheDir))
{
    setupPlayer();

    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(15000);
    connect(m_timeoutTimer, &QTimer::timeout, this, &VideoThumbnailWorker::onTimeout);

    m_tailPauseTimer = new QTimer(this);
    m_tailPauseTimer->setSingleShot(true);
    m_tailPauseTimer->setInterval(kTailPauseMs);
    connect(m_tailPauseTimer, &QTimer::timeout, this, [this] {
        m_tailPauseActive = false;
        processNextIfIdle();
    });
}

VideoThumbnailWorker::~VideoThumbnailWorker() = default;

void VideoThumbnailWorker::setupPlayer()
{
    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &VideoThumbnailWorker::onMediaStatusChanged);
    connect(m_sink, &QVideoSink::videoFrameChanged,
            this, &VideoThumbnailWorker::onVideoFrameChanged);
}

void VideoThumbnailWorker::retarget(const QStringList &paths, int generation, int foregroundCount)
{
    if (generation != m_generation) {
        m_attempted.clear();   // nový seznam souborů — dosavadní paměť neplatí
        m_tailPauseActive = false;
        m_tailPauseTimer->stop();
    }
    m_cancelled = false;   // reset po cancel() – bez toho processNext() hned vrátí
    m_generation = generation;

    m_foreground.clear();
    for (int i = 0; i < qMin(foregroundCount, static_cast<int>(paths.size())); ++i) {
        m_foreground.insert(paths.at(i));
    }

    // Rozdělané video z ocasu, které nový seznam už nechce (uživatel se začal
    // něčím zabývat), se zruší — dojíždět ho by zbytečně zatěžovalo síť.
    if (m_state != State::Idle && m_currentIsTail && !paths.contains(m_currentPath)) {
        abortCurrent();
    }

    m_queue.clear();
    for (const QString &path : paths) {
        if (path != m_currentPath && !m_attempted.contains(path)) {
            m_queue.append(path);
        }
    }
    if (m_state == State::Idle) {
        processNext();
    }
}

void VideoThumbnailWorker::abortCurrent()
{
    m_timeoutTimer->stop();
    m_attempted.remove(m_currentPath);   // zkusí se znovu, až bude klid
    m_currentPath.clear();
    m_currentIsTail = false;
    m_state = State::Idle;
    discardPlayer();
}

void VideoThumbnailWorker::processNextIfIdle()
{
    if (m_state == State::Idle) {
        processNext();
    }
}

void VideoThumbnailWorker::cancel()
{
    m_cancelled = true;
    m_timeoutTimer->stop();
    m_tailPauseTimer->stop();
    m_tailPauseActive = false;
    m_queue.clear();
    m_attempted.clear();
    m_currentPath.clear();
    if (m_state != State::Idle) {
        m_state = State::Idle;
        discardPlayer();
    }
}

void VideoThumbnailWorker::discardPlayer()
{
    // Zahodit CELÝ přehrávač a video sink, ne jen stop()/setSource(prázdné url).
    // AVFoundation backend doručuje videoFrameChanged asynchronně přes
    // CVDisplayLink na hlavní vlákno i PO stop()/setSource() — starý frame
    // tak může dorazit AŽ PO startu dalšího videa (retarget() zavolané hned
    // po cancel()) a mylně se přiřadit k m_currentPath/m_generation už
    // NOVÉHO videa (stejné m_state hodnoty se cyklicky opakují pro každé
    // video, takže je nelze rozlišit jinak).
    // disconnect() je NUTNÝ a musí být PŘED deleteLater() — deleteLater()
    // samo o sobě odpojení odloží až do skutečné destrukce objektu (příští
    // běh event loopy), takže by mezitím starý přehrávač mohl ještě stihnout
    // emitovat signál do našich slotů.
    disconnect(m_player, nullptr, this, nullptr);
    disconnect(m_sink, nullptr, this, nullptr);
    m_player->deleteLater();
    m_sink->deleteLater();
    setupPlayer();
}

void VideoThumbnailWorker::suspend()
{
    if (m_suspended) {
        return;
    }
    m_suspended = true;

    if (m_state == State::Idle) {
        return;
    }

    // Rozpracované video vrátit na začátek fronty — po resume() se dogeneruje.
    if (!m_currentPath.isEmpty()) {
        m_attempted.remove(m_currentPath);   // nedokončené — po resume() se zkusí znovu
        m_queue.prepend(m_currentPath);
        m_currentPath.clear();
    }
    m_timeoutTimer->stop();
    m_state = State::Idle;
    // Přehrávač musí zmizet celý, ne jen stop(): smyslem suspendu je, aby
    // souběžně s VideoPlayerem nežila ŽÁDNÁ naše AVFoundation položka.
    discardPlayer();
}

void VideoThumbnailWorker::setDiskCache(bool enabled, const QString &cacheDir)
{
    m_diskCacheEnabled = enabled;
    m_diskCacheDir = cacheDir;
}

void VideoThumbnailWorker::resume()
{
    if (!m_suspended) {
        return;
    }
    m_suspended = false;

    if (!m_cancelled && m_state == State::Idle && !m_queue.isEmpty()) {
        QMetaObject::invokeMethod(this, &VideoThumbnailWorker::processNext,
                                  Qt::QueuedConnection);
    }
}

void VideoThumbnailWorker::processNext()
{
    if (m_cancelled || m_queue.isEmpty()) {
        m_state = State::Idle;
        return;
    }

    // Miniatura z cache se vydává I BĚHEM SUSPENDU — je to čisté čtení souboru
    // z disku, QMediaPlayer se nedotýká, takže s přehrávaným videem kolidovat
    // nemůže. Suspend blokuje jen dekódování videa níže. (Dřív tudy neprošlo
    // nic a ve složce s videi, kde se hned spustí přehrávání, se hotové
    // miniatury nezobrazily, dokud se přehrávání nezastavilo.)
    // Po jedné za průchod event loopou, ať se UI nezasekne u velké složky.
    const QImage cached = loadFromCache(m_queue.first());
    if (!cached.isNull()) {
        const QString path = m_queue.takeFirst();
        m_attempted.insert(path);
        emit thumbnailReady(m_generation, path, cached);
        QMetaObject::invokeMethod(this, &VideoThumbnailWorker::processNext,
                                  Qt::QueuedConnection);
        return;
    }

    // Zbytek už vyžaduje dekódování videa přes QMediaPlayer — to za suspendu
    // nesmí běžet. Fronta zůstává, resume() naváže na stejném místě.
    if (m_suspended) {
        m_state = State::Idle;
        return;
    }

    // Po vygenerované miniatuře z ocasu (zahřívání cache) pauza — video se čte
    // po síti celé a nechceme zahltit spojení jedním za druhým. Videa z
    // popředí (viditelná) pauzu nemají.
    if (m_tailPauseActive && !m_foreground.contains(m_queue.first())) {
        m_state = State::Idle;
        return;
    }

    m_currentPath = m_queue.takeFirst();
    m_currentIsTail = !m_foreground.contains(m_currentPath);
    m_attempted.insert(m_currentPath);
    m_state = State::Loading;
    m_player->setSource(QUrl::fromLocalFile(m_currentPath));
    m_player->play();
    m_timeoutTimer->start();
}

void VideoThumbnailWorker::onMediaStatusChanged(int rawStatus)
{
    if (m_state != State::Loading) {
        return;
    }

    const auto status = static_cast<QMediaPlayer::MediaStatus>(rawStatus);
    if (status == QMediaPlayer::LoadedMedia
        || status == QMediaPlayer::BufferedMedia
        || status == QMediaPlayer::BufferingMedia) {

        const qint64 dur = m_player->duration();
        m_state = State::WaitingFrame;
        if (dur > 0) {
            m_player->setPosition(dur / 10);
        }
    } else if (status == QMediaPlayer::InvalidMedia
               || status == QMediaPlayer::NoMedia) {
        m_timeoutTimer->stop();
        finishCurrent(QImage{});
    }
}

void VideoThumbnailWorker::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (m_state != State::WaitingFrame || !frame.isValid()) {
        return;
    }

    m_timeoutTimer->stop();
    m_state = State::Idle;

    QImage image = frame.toImage();
    if (image.isNull()) {
        m_player->stop();
        finishCurrent(QImage{});
        return;
    }

    if (image.width() > ThumbnailSize || image.height() > ThumbnailSize) {
        image = image.scaled(ThumbnailSize, ThumbnailSize,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_player->stop();
    saveToCache(m_currentPath, image);
    finishCurrent(image);
}

void VideoThumbnailWorker::onTimeout()
{
    m_player->stop();
    m_state = State::Idle;
    finishCurrent(QImage{});
}

void VideoThumbnailWorker::finishCurrent(const QImage &image)
{
    // Idle PŘED setSource(QUrl()) — změna zdroje emituje mediaStatusChanged(NoMedia)
    // synchronně a bez toho by se onMediaStatusChanged rekurzivně vrátil sem.
    m_state = State::Idle;
    m_player->stop();
    // Uvolnit handle souboru — na Windows by jinak poslední zpracované video
    // zůstalo zamčené a nešlo by přesunout do Delete ani smazat.
    m_player->setSource(QUrl());

    diag::log(QStringLiteral("video miniatura %1: %2, zbývá ve frontě %3")
                  .arg(QFileInfo(m_currentPath).fileName(),
                       image.isNull() ? QStringLiteral("SELHALA") : QStringLiteral("hotovo"))
                  .arg(m_queue.size()));
    if (!image.isNull()) {
        emit thumbnailReady(m_generation, m_currentPath, image);
    }
    m_currentPath.clear();
    if (m_currentIsTail) {
        m_currentIsTail = false;
        m_tailPauseActive = true;
        m_tailPauseTimer->start();
    }
    QMetaObject::invokeMethod(this, &VideoThumbnailWorker::processNext,
                              Qt::QueuedConnection);
}

QString VideoThumbnailWorker::cacheFilePath(const QString &path) const
{
    // Běží na UI vlákně — otisk z výpisu složky ušetří stat() přes síť.
    const FileStamp stamp = FileStampIndex::resolve(m_stamps, path);
    const QString keySource = path + QLatin1Char('|')
        + QString::number(stamp.mtimeSecs) + QLatin1Char('|')
        + QString::number(stamp.size) + QLatin1Char('|')
        + QStringLiteral("video|")
        + QString::number(ThumbnailSize);
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(keySource.toUtf8(), QCryptographicHash::Sha1).toHex());
    return m_diskCacheDir + QLatin1Char('/') + hash.left(2) + QLatin1Char('/')
         + hash + QStringLiteral(".thumb");
}

QImage VideoThumbnailWorker::loadFromCache(const QString &path) const
{
    if (!m_diskCacheEnabled || m_diskCacheDir.isEmpty()) {
        return {};
    }
    return QImage(cacheFilePath(path));
}

void VideoThumbnailWorker::saveToCache(const QString &path, const QImage &image) const
{
    if (!m_diskCacheEnabled || m_diskCacheDir.isEmpty() || image.isNull()) {
        return;
    }
    const QString cf = cacheFilePath(path);
    QDir().mkpath(QFileInfo(cf).absolutePath());
    image.save(cf, "JPG", 90);
}

} // namespace pictureviewer

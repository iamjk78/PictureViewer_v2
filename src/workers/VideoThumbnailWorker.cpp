#include "workers/VideoThumbnailWorker.hpp"

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
    , m_player(new QMediaPlayer(this))
    , m_sink(new QVideoSink(this))
    , m_timeoutTimer(new QTimer(this))
    , m_diskCacheEnabled(diskCacheEnabled)
    , m_diskCacheDir(std::move(diskCacheDir))
{
    m_player->setVideoSink(m_sink);

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &VideoThumbnailWorker::onMediaStatusChanged);
    connect(m_sink, &QVideoSink::videoFrameChanged,
            this, &VideoThumbnailWorker::onVideoFrameChanged);

    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(8000);
    connect(m_timeoutTimer, &QTimer::timeout, this, &VideoThumbnailWorker::onTimeout);
}

VideoThumbnailWorker::~VideoThumbnailWorker() = default;

void VideoThumbnailWorker::enqueue(const QStringList &paths)
{
    m_cancelled = false;   // reset po cancel() – bez toho processNext() hned vrátí
    m_queue.append(paths);
    if (m_state == State::Idle) {
        processNext();
    }
}

void VideoThumbnailWorker::cancel()
{
    m_cancelled = true;
    m_timeoutTimer->stop();
    m_queue.clear();
    if (m_state != State::Idle) {
        m_player->stop();
        m_state = State::Idle;
    }
}

void VideoThumbnailWorker::processNext()
{
    if (m_cancelled || m_queue.isEmpty()) {
        m_state = State::Idle;
        return;
    }

    m_currentPath = m_queue.takeFirst();

    const QImage cached = loadFromCache(m_currentPath);
    if (!cached.isNull()) {
        emit thumbnailReady(m_currentPath, cached);
        QMetaObject::invokeMethod(this, &VideoThumbnailWorker::processNext,
                                  Qt::QueuedConnection);
        return;
    }

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
    if (!image.isNull()) {
        emit thumbnailReady(m_currentPath, image);
    }
    m_currentPath.clear();
    QMetaObject::invokeMethod(this, &VideoThumbnailWorker::processNext,
                              Qt::QueuedConnection);
}

QString VideoThumbnailWorker::cacheFilePath(const QString &path) const
{
    const QFileInfo fi(path);
    const QString keySource = path + QLatin1Char('|')
        + QString::number(fi.lastModified().toSecsSinceEpoch()) + QLatin1Char('|')
        + QString::number(fi.size()) + QLatin1Char('|')
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

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

    void enqueue(const QStringList &paths);
    void cancel();

signals:
    void thumbnailReady(const QString &path, const QImage &image);

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

    enum class State { Idle, Loading, WaitingFrame };

    QMediaPlayer *m_player;
    QVideoSink   *m_sink;
    QTimer       *m_timeoutTimer;
    QStringList   m_queue;
    QString       m_currentPath;
    State         m_state      = State::Idle;
    bool          m_cancelled  = false;
    bool          m_diskCacheEnabled;
    QString       m_diskCacheDir;
};

} // namespace pictureviewer

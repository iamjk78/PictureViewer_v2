#pragma once

#include <QObject>
#include <QRunnable>
#include <QStringList>

#include <atomic>

class QImage;

namespace pictureviewer {

class ThumbnailWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    static constexpr int ThumbnailSize = 96;
    static constexpr int BatchSize = 5;

    ThumbnailWorker(QStringList paths, int generation, QObject *parent = nullptr);

    void cancel();
    void run() override;

signals:
    void thumbnailReady(int generation, int index, const QImage &image);
    void workerFinished(int generation);
    void workerError(int generation, const QString &error);

private:
    QImage loadThumbnail(const QString &path) const;

    QStringList m_paths;
    int m_generation;
    std::atomic_bool m_cancelled;
};

} // namespace pictureviewer

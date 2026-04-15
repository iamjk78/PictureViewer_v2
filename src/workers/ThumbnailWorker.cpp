#include "workers/ThumbnailWorker.hpp"

#include <QImage>
#include <QThread>

namespace pictureviewer {

ThumbnailWorker::ThumbnailWorker(QStringList paths, int generation, QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_generation(generation)
    , m_cancelled(false)
{
    setAutoDelete(false);
}

void ThumbnailWorker::cancel()
{
    m_cancelled.store(true);
}

void ThumbnailWorker::run()
{
    for (int batchStart = 0; batchStart < m_paths.size(); batchStart += BatchSize) {
        if (m_cancelled.load()) {
            emit workerFinished(m_generation);
            return;
        }

        const int batchEnd = std::min(batchStart + BatchSize, static_cast<int>(m_paths.size()));
        for (int index = batchStart; index < batchEnd; ++index) {
            if (m_cancelled.load()) {
                emit workerFinished(m_generation);
                return;
            }

            emit thumbnailReady(m_generation, index, loadThumbnail(m_paths.at(index)));
        }

        QThread::yieldCurrentThread();
    }

    emit workerFinished(m_generation);
}

QImage ThumbnailWorker::loadThumbnail(const QString &path) const
{
    const QImage image(path);
    if (image.isNull()) {
        return {};
    }

    return image.scaled(
        ThumbnailSize,
        ThumbnailSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
}

} // namespace pictureviewer

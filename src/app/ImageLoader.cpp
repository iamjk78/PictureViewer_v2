#include "app/ImageLoader.hpp"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QtConcurrent>

namespace {

QImage decodeImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);   // EXIF orientace
    return reader.read();
}

} // anonymous namespace

namespace pictureviewer {

ImageLoader::ImageLoader(QObject *parent)
    : QObject(parent)
    , m_cache(DefaultCacheLimitKb)
{
}

// static
QString ImageLoader::cacheKey(const QString &path)
{
    const QFileInfo fileInfo(path);
    return path + QLatin1Char('|')
         + QString::number(fileInfo.lastModified().toSecsSinceEpoch())
         + QLatin1Char('|')
         + QString::number(fileInfo.size());
}

QImage ImageLoader::cachedImage(const QString &path) const
{
    if (const QImage *image = m_cache.object(cacheKey(path))) {
        return *image;
    }
    return {};
}

void ImageLoader::request(const QString &path)
{
    if (m_shuttingDown || m_inFlight.contains(path)) {
        return;
    }
    startDecode(path);
}

void ImageLoader::prefetch(const QStringList &paths)
{
    if (m_shuttingDown) {
        return;
    }
    for (const QString &path : paths) {
        if (!m_inFlight.contains(path) && m_cache.object(cacheKey(path)) == nullptr) {
            startDecode(path);
        }
    }
}

void ImageLoader::shutdown()
{
    m_shuttingDown = true;
}

void ImageLoader::startDecode(const QString &path)
{
    m_inFlight.insert(path);

    auto *watcher = new QFutureWatcher<QImage>(nullptr);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, path] {
        if (m_shuttingDown) {
            watcher->deleteLater();
            return;
        }
        m_inFlight.remove(path);
        const QImage image = watcher->result();

        if (!image.isNull()) {
            const qsizetype costKb = qMax<qsizetype>(1, image.sizeInBytes() / 1024);
            m_cache.insert(cacheKey(path), new QImage(image), costKb);
        }
        emit imageReady(path, image);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(decodeImage, path));
}

} // namespace pictureviewer

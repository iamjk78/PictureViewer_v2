#include "app/ImageLoader.hpp"

#include "core/DiagLog.hpp"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QtConcurrent>

namespace pictureviewer {

ImageLoader::ImageLoader(QObject *parent)
    : QObject(parent)
    , m_cache(DefaultCacheLimitKb)
{
}

ImageLoader::~ImageLoader()
{
    // Čekat na všechny běžící futures. Lamdy ze QFutureWatcher mají guard
    // if (m_shuttingDown), ale destruktor se volá po shutdown() a přesto chceme
    // jistotu, že se žádná lambda nespustí s nyní neplatným objektem.
    for (QFutureWatcher<Decoded> *watcher : m_watchers) {
        watcher->waitForFinished();
        delete watcher;
    }
    m_watchers.clear();
    for (QFutureWatcher<bool> *watcher : m_validationWatchers) {
        watcher->waitForFinished();
        delete watcher;
    }
    m_validationWatchers.clear();
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

QImage ImageLoader::cachedImage(const QString &path)
{
    if (const Entry *entry = m_cache.object(path)) {
        validateAsync(path, entry->stamp);
        return entry->image;
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
    m_pendingPrefetch = paths;
    if (m_inFlight.isEmpty() && !m_prefetchPaused) {
        startPendingPrefetch();
    }
}

void ImageLoader::setPrefetchPaused(bool paused)
{
    m_prefetchPaused = paused;
    if (!paused && m_inFlight.isEmpty()) {
        startPendingPrefetch();
    }
}

void ImageLoader::startPendingPrefetch()
{
    if (m_prefetchPaused) {
        return;
    }
    const QStringList paths = std::exchange(m_pendingPrefetch, {});
    for (const QString &path : paths) {
        if (!m_inFlight.contains(path) && !m_cache.contains(path)) {
            startDecode(path);
        }
    }
}

void ImageLoader::shutdown()
{
    m_shuttingDown = true;
}

void ImageLoader::validateAsync(const QString &path, const QString &stamp)
{
    if (m_shuttingDown || m_validating.contains(path)) {
        return;
    }
    m_validating.insert(path);

    auto *watcher = new QFutureWatcher<bool>(nullptr);
    m_validationWatchers.append(watcher);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, path, stamp] {
        m_validationWatchers.removeAll(watcher);
        watcher->deleteLater();
        if (m_shuttingDown) {
            return;
        }
        m_validating.remove(path);
        if (watcher->result()) {
            return;
        }
        const Entry *entry = m_cache.object(path);
        if (entry != nullptr && entry->stamp == stamp) {
            m_cache.remove(path);
            diag::log(QStringLiteral("obrázek se změnil na disku, zahazuji z cache: %1").arg(path));
            emit imageChangedOnDisk(path);
        }
    });
    watcher->setFuture(QtConcurrent::run([path, stamp] { return cacheKey(path) == stamp; }));
}

void ImageLoader::startDecode(const QString &path)
{
    const bool wasIdle = m_inFlight.isEmpty();
    m_inFlight.insert(path);
    if (wasIdle) {
        emit busyChanged(true);
    }

    auto *watcher = new QFutureWatcher<Decoded>(nullptr);
    m_watchers.append(watcher);

    connect(watcher, &QFutureWatcher<Decoded>::finished, this, [this, watcher, path] {
        // Odebrat z aktivního seznamu — destruktor čekat nemusí
        m_watchers.removeAll(watcher);

        if (m_shuttingDown) {
            watcher->deleteLater();
            return;
        }

        m_inFlight.remove(path);
        const Decoded decoded = watcher->result();

        if (!decoded.image.isNull()) {
            const qsizetype costKb = qMax<qsizetype>(1, decoded.image.sizeInBytes() / 1024);
            m_cache.insert(path, new Entry{decoded.image, decoded.stamp}, costKb);
        }
        emit imageReady(path, decoded.image);
        watcher->deleteLater();

        if (m_inFlight.isEmpty()) {
            emit busyChanged(false);
            startPendingPrefetch();
        }
    });
    watcher->setFuture(QtConcurrent::run([path]() -> Decoded {
        // Otisk se bere PŘED dekódováním — změna během čtení se pak projeví
        // při další validaci, ne tichým uložením nového otisku ke starým datům.
        QElapsedTimer timer;
        timer.start();
        Decoded result;
        result.stamp = cacheKey(path);
        QImageReader reader(path);
        reader.setAutoTransform(true);   // EXIF orientace
        result.image = reader.read();
        diag::log(QStringLiteral("dekódování obrázku %1: %2 ms, výsledek %3x%4")
                      .arg(QFileInfo(path).fileName())
                      .arg(timer.elapsed())
                      .arg(result.image.width()).arg(result.image.height()));
        return result;
    }));
}

} // namespace pictureviewer

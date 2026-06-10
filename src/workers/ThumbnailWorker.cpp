#include "workers/ThumbnailWorker.hpp"

#include "core/ImageFormats.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPdfDocument>
#include <QThread>

namespace pictureviewer {

ThumbnailWorker::ThumbnailWorker(QStringList paths, int generation,
                                 bool diskCacheEnabled, QString diskCacheDir,
                                 QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_generation(generation)
    , m_cancelled(false)
    , m_diskCacheEnabled(diskCacheEnabled)
    , m_diskCacheDir(std::move(diskCacheDir))
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

// Cesta cache souboru: <dir>/ab/<sha1>.thumb
// Klíč obsahuje mtime a velikost — změna souboru = jiný klíč, stará položka
// přirozeně přestane dostávat hity. Dvě úrovně adresářů kvůli velkým složkám.
QString ThumbnailWorker::cacheFilePath(const QString &path) const
{
    const QFileInfo fileInfo(path);
    const QString keySource = path + QLatin1Char('|')
        + QString::number(fileInfo.lastModified().toSecsSinceEpoch()) + QLatin1Char('|')
        + QString::number(fileInfo.size()) + QLatin1Char('|')
        + QString::number(ThumbnailSize);
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(keySource.toUtf8(), QCryptographicHash::Sha1).toHex());
    return m_diskCacheDir + QLatin1Char('/') + hash.left(2) + QLatin1Char('/')
         + hash + QStringLiteral(".thumb");
}

QImage ThumbnailWorker::loadThumbnail(const QString &path) const
{
    QString cacheFile;
    if (m_diskCacheEnabled && !m_diskCacheDir.isEmpty()) {
        cacheFile = cacheFilePath(path);
        const QImage cached(cacheFile);   // formát se pozná z obsahu souboru
        if (!cached.isNull()) {
            return cached;
        }
    }

    const QImage thumbnail = generateThumbnail(path);

    if (!cacheFile.isEmpty() && !thumbnail.isNull()) {
        QDir().mkpath(QFileInfo(cacheFile).absolutePath());
        // JPEG pro fotky (10–20 kB), PNG jen při průhlednosti
        thumbnail.save(cacheFile, thumbnail.hasAlphaChannel() ? "PNG" : "JPG", 90);
    }

    return thumbnail;
}

QImage ThumbnailWorker::generateThumbnail(const QString &path) const
{
    const QString suffix = "." + QFileInfo(path).suffix();

    if (isPdfFile(suffix)) {
        QPdfDocument doc;
        doc.load(path);
        if (doc.pageCount() <= 0) {
            return {};
        }
        const QSizeF pageSize = doc.pagePointSize(0);
        QSize renderSize(ThumbnailSize, ThumbnailSize);
        if (pageSize.isValid() && pageSize.width() > 0) {
            renderSize = (pageSize.width() > pageSize.height())
                ? QSize(ThumbnailSize, qRound(ThumbnailSize * pageSize.height() / pageSize.width()))
                : QSize(qRound(ThumbnailSize * pageSize.width() / pageSize.height()), ThumbnailSize);
        }
        const QImage rendered = doc.render(0, renderSize);
        if (rendered.isNull()) {
            return {};
        }
        QImage white(rendered.size(), QImage::Format_RGB32);
        white.fill(Qt::white);
        QPainter painter(&white);
        painter.drawImage(0, 0, rendered);
        painter.end();
        return white;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);   // EXIF orientace

    // Zmenšené dekódování: u JPEG dekodér čte výrazně méně dat a je ~8× rychlejší
    // než dekódování plného rozlišení a následné zmenšení.
    const QSize fullSize = reader.size();
    if (fullSize.isValid() && !fullSize.isEmpty()) {
        reader.setScaledSize(
            fullSize.scaled(ThumbnailSize, ThumbnailSize, Qt::KeepAspectRatio));
    }

    QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }

    if (image.width() > ThumbnailSize || image.height() > ThumbnailSize) {
        image = image.scaled(ThumbnailSize, ThumbnailSize,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

} // namespace pictureviewer

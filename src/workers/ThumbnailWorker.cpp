#include "workers/ThumbnailWorker.hpp"

#include "core/DiagLog.hpp"
#include "core/ExifThumbnail.hpp"
#include "core/ImageFormats.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
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

void ThumbnailWorker::setCacheOnly(QSharedPointer<ThumbnailClaims> claims, int throttleMs)
{
    m_cacheOnly = true;
    m_claims = std::move(claims);
    m_throttleMs = throttleMs;
}

void ThumbnailWorker::sleepInterruptible(int ms) const
{
    for (int slept = 0; slept < ms && !m_cancelled.load(); slept += 25) {
        QThread::msleep(static_cast<unsigned long>(qMin(25, ms - slept)));
    }
}

void ThumbnailWorker::run()
{
    QElapsedTimer warmTimer;
    warmTimer.start();
    int warmProcessed = 0;
    int warmGenerated = 0;
    if (m_cacheOnly) {
        diag::log(QStringLiteral("zahřátí cache: START, %1 souborů, škrcení %2 ms")
                      .arg(m_paths.size()).arg(m_throttleMs));
    }
    for (int batchStart = 0; batchStart < m_paths.size(); batchStart += BatchSize) {
        if (m_cancelled.load()) {
            if (m_cacheOnly) {
                diag::log(QStringLiteral("zahřátí cache: PŘERUŠENO po %1 souborech (vygenerováno %2)")
                              .arg(warmProcessed).arg(warmGenerated));
            }
            emit workerFinished(m_generation);
            return;
        }

        const int batchEnd = std::min(batchStart + BatchSize, static_cast<int>(m_paths.size()));
        for (int index = batchStart; index < batchEnd; ++index) {
            // Pozastavený worker čeká (uživatel právě listuje / posouvá).
            while (m_paused.load() && !m_cancelled.load()) {
                QThread::msleep(100);
            }
            if (m_cancelled.load()) {
                emit workerFinished(m_generation);
                return;
            }

            const QString &path = m_paths.at(index);

            if (m_cacheOnly) {
                // Popředí už tuhle miniaturu má (nebo ji právě dělá).
                if (m_claims && m_claims->contains(path)) {
                    ++m_processed;
                    continue;
                }
                bool generated = false;
                try {
                    generated = warmOne(path);
                } catch (...) {
                    generated = false;
                }
                ++warmProcessed;
                ++m_processed;
                if (generated) {
                    ++warmGenerated;
                    sleepInterruptible(m_throttleMs);
                }
                if (warmProcessed % 100 == 0) {
                    diag::log(QStringLiteral("zahřátí cache: %1/%2 zpracováno, vygenerováno %3, %4 s")
                                  .arg(warmProcessed).arg(m_paths.size()).arg(warmGenerated)
                                  .arg(warmTimer.elapsed() / 1000));
                }
                continue;
            }

            // Jeden vadný soubor (např. std::bad_alloc u obřího rastru) nesmí
            // shodit celé vlákno z fondu — výjimka z QRunnable::run() nemá kdo
            // zachytit a skončila by v std::terminate. Miniatura se přeskočí.
            QImage thumbnail;
            try {
                thumbnail = loadThumbnail(path);
            } catch (...) {
                thumbnail = QImage();
            }
            emit thumbnailReady(m_generation, path, thumbnail);
        }

        QThread::yieldCurrentThread();
    }

    if (m_cacheOnly) {
        diag::log(QStringLiteral("zahřátí cache: HOTOVO, %1 souborů, vygenerováno %2, %3 s")
                      .arg(warmProcessed).arg(warmGenerated).arg(warmTimer.elapsed() / 1000));
    }
    emit workerFinished(m_generation);
}

// Cesta cache souboru: <dir>/ab/<sha1>.thumb
// Klíč obsahuje mtime a velikost — změna souboru = jiný klíč, stará položka
// přirozeně přestane dostávat hity. Dvě úrovně adresářů kvůli velkým složkám.
QString ThumbnailWorker::cacheFilePath(const QString &path) const
{
    // Otisk souboru z výpisu složky; bez něj stat() (přes síť desetiny sekundy).
    QElapsedTimer timer;
    timer.start();
    const FileStamp stamp = FileStampIndex::resolve(m_stamps, path);
    diag::thumb().keyNs += timer.nsecsElapsed();
    const QString keySource = path + QLatin1Char('|')
        + QString::number(stamp.mtimeSecs) + QLatin1Char('|')
        + QString::number(stamp.size) + QLatin1Char('|')
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
        QElapsedTimer readTimer;
        readTimer.start();
        const QImage cached(cacheFile);   // formát se pozná z obsahu souboru
        diag::thumb().cacheReadNs += readTimer.nsecsElapsed();
        if (!cached.isNull()) {
            ++diag::thumb().hit;
            return cached;
        }
    }

    QElapsedTimer generateTimer;
    generateTimer.start();
    const QImage thumbnail = generateThumbnail(path);
    diag::thumb().generateNs += generateTimer.nsecsElapsed();
    if (thumbnail.isNull()) {
        ++diag::thumb().failed;
    } else {
        ++diag::thumb().generated;
    }

    if (!cacheFile.isEmpty() && !thumbnail.isNull()) {
        QDir().mkpath(QFileInfo(cacheFile).absolutePath());
        // JPEG pro fotky (10–20 kB), PNG jen při průhlednosti
        thumbnail.save(cacheFile, thumbnail.hasAlphaChannel() ? "PNG" : "JPG", 90);
    }

    return thumbnail;
}

bool ThumbnailWorker::warmOne(const QString &path) const
{
    if (!m_diskCacheEnabled || m_diskCacheDir.isEmpty()) {
        return false;   // nemá se kam ukládat — zahřívání by nemělo smysl
    }
    const QString cacheFile = cacheFilePath(path);
    if (QFile::exists(cacheFile)) {
        return false;
    }
    QElapsedTimer generateTimer;
    generateTimer.start();
    const QImage thumbnail = generateThumbnail(path);
    diag::thumb().generateNs += generateTimer.nsecsElapsed();
    if (thumbnail.isNull()) {
        ++diag::thumb().failed;
        return false;
    }
    ++diag::thumb().generated;
    QDir().mkpath(QFileInfo(cacheFile).absolutePath());
    thumbnail.save(cacheFile, thumbnail.hasAlphaChannel() ? "PNG" : "JPG", 90);
    return true;
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

    // JPEG s vloženou EXIF miniaturou: stačí přečíst začátek souboru (desítky kB
    // místo několika MB — na síťovém disku rozhodující).
    if (suffix.compare(QLatin1String(".jpg"), Qt::CaseInsensitive) == 0
        || suffix.compare(QLatin1String(".jpeg"), Qt::CaseInsensitive) == 0) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const QImage exifThumb = extractExifThumbnail(file.read(kExifHeaderBytes));
            if (!exifThumb.isNull()) {
                ++diag::thumb().exifUsed;
                return exifThumb.width() > ThumbnailSize || exifThumb.height() > ThumbnailSize
                    ? exifThumb.scaled(ThumbnailSize, ThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                    : exifThumb;
            }
        }
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

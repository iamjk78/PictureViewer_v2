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
    // Náhledy se generují a cachují ve 192 px (2× zobrazovaná velikost 96 px
    // v panelu) — na Retina displejích jsou tak ostré a stačí pro mřížku Galerie.
    static constexpr int ThumbnailSize = 192;
    static constexpr int BatchSize = 5;

    ThumbnailWorker(QStringList paths, int generation,
                    bool diskCacheEnabled, QString diskCacheDir,
                    QObject *parent = nullptr);

    void cancel();
    void run() override;

signals:
    void thumbnailReady(int generation, int index, const QImage &image);
    void workerFinished(int generation);
    void workerError(int generation, const QString &error);

private:
    QImage loadThumbnail(const QString &path) const;
    QImage generateThumbnail(const QString &path) const;
    QString cacheFilePath(const QString &path) const;

    QStringList m_paths;
    int m_generation;
    std::atomic_bool m_cancelled;
    bool m_diskCacheEnabled;
    QString m_diskCacheDir;
};

} // namespace pictureviewer

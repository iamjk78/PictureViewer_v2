#pragma once

#include "core/FileStampIndex.hpp"
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QRunnable>
#include <QSet>
#include <QSharedPointer>
#include <QStringList>

#include <atomic>

class QImage;

namespace pictureviewer {

// Vlákně bezpečná evidence cest, pro které už popředí (viditelné položky)
// miniaturu spustilo / vygenerovalo. Sdílí ji panel náhledů s workerem na
// pozadí, aby pozadí nedělalo znovu, co popředí už udělalo.
class ThumbnailClaims
{
public:
    // Vrátí true, pokud cesta ještě nebyla zabraná (a teď se zabrala).
    bool claim(const QString &path)
    {
        QMutexLocker lock(&m_mutex);
        const qsizetype before = m_paths.size();
        m_paths.insert(path);
        return m_paths.size() != before;
    }
    bool contains(const QString &path) const
    {
        QMutexLocker lock(&m_mutex);
        return m_paths.contains(path);
    }
    void release(const QString &path)
    {
        QMutexLocker lock(&m_mutex);
        m_paths.remove(path);
    }
    // Soubor byl přejmenován — zabrání patří k souboru, ne k názvu.
    void rename(const QString &oldPath, const QString &newPath)
    {
        QMutexLocker lock(&m_mutex);
        if (m_paths.remove(oldPath)) {
            m_paths.insert(newPath);
        }
    }

private:
    mutable QMutex m_mutex;
    QSet<QString> m_paths;
};

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

    // Režim "zahřátí cache" (worker na pozadí): miniatury se jen zapíšou do
    // diskové cache, žádná se nevydává (panel tak nedrží ikony celé složky
    // v paměti). Soubory, které už v cache jsou nebo které zabralo popředí
    // (claims), se přeskočí. Po každé skutečně vygenerované miniatuře worker
    // počká throttleMs, ať nezahltí úložiště na úkor toho, co uživatel právě
    // prohlíží.
    void setCacheOnly(QSharedPointer<ThumbnailClaims> claims, int throttleMs);
    // Otisky souborů z výpisu složky — klíč cache pak nepotřebuje stat().
    void setFileStamps(QSharedPointer<FileStampIndex> stamps) { m_stamps = std::move(stamps); }
    // Pozastaví/obnoví zpracování (worker čeká mezi soubory, nezahazuje frontu).
    void setPaused(bool paused) { m_paused.store(paused); }

signals:
    void thumbnailReady(int generation, const QString &path, const QImage &image);
    void workerFinished(int generation);
    void workerError(int generation, const QString &error);

private:
    QImage loadThumbnail(const QString &path) const;
    // Zahřátí cache pro jeden soubor; vrací true, pokud se miniatura skutečně
    // generovala (false = už v cache / cache nedostupná).
    bool warmOne(const QString &path) const;
    // Spí po částech, aby šlo cancel() poznat hned.
    void sleepInterruptible(int ms) const;
    QImage generateThumbnail(const QString &path) const;
    QString cacheFilePath(const QString &path) const;

    QStringList m_paths;
    int m_generation;
    std::atomic_bool m_cancelled;
    std::atomic_bool m_paused{false};
    bool m_cacheOnly = false;
    int m_throttleMs = 0;
    QSharedPointer<ThumbnailClaims> m_claims;
    QSharedPointer<FileStampIndex> m_stamps;
    bool m_diskCacheEnabled;
    QString m_diskCacheDir;
};

} // namespace pictureviewer

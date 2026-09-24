#include "workers/FolderScanWorker.hpp"

#include "app/SettingsManager.hpp"
#include "core/DiagLog.hpp"
#include "core/ImageCatalog.hpp"

#include <QElapsedTimer>
#include <QThread>

#include <exception>

namespace pictureviewer {

FolderScanWorker::FolderScanWorker(const SettingsManager *settings, QString folderPath, int generation,
                                   QSharedPointer<FileStampIndex> stamps, QObject *parent)
    : QObject(parent)
    , m_folderPath(std::move(folderPath))
    , m_stamps(std::move(stamps))
    , m_generation(generation)
    , m_cancelled(false)
{
    setAutoDelete(false);

    // Zkopírovat hodnoty hned teď, na hlavním vlákně — settings je tu zaručeně
    // platný. run() pak běží čistě nad vlastními členy.
    if (settings != nullptr) {
        m_includePdf    = settings->enablePdfProcessing();
        m_includeImages = settings->enableImages();
        m_includeVideos = settings->enableVideos();
        m_sortKey       = static_cast<SortKey>(settings->sortKey());
        m_ascending     = settings->sortAscending();
    }
}

namespace {
std::atomic_int g_streamDelayMs{0};
}

void FolderScanWorker::setStreamDelayForTesting(int msPerBatch)
{
    g_streamDelayMs = msPerBatch;
}

void FolderScanWorker::cancel()
{
    m_cancelled.store(true);
}

void FolderScanWorker::run()
{
    if (m_cancelled.load()) {
        emit finished(m_generation);
        return;
    }

    QElapsedTimer scanTimer;
    scanTimer.start();
    int batches = 0;
    try {
        ImageCatalog catalog;
        QStringList paths;
        if (m_sortKey == SortKey::Name) {
            // Řazení podle jména nepotřebuje stat() souborů, takže se výpis
            // dá ukazovat postupně (na pomalém úložišti trvá minuty).
            paths = catalog.loadFolderStreaming(
                m_folderPath, m_includePdf, m_ascending, m_includeImages, m_includeVideos,
                [this] { return m_cancelled.load(); },
                [this, &batches, &scanTimer](const QStringList &batch) {
                    if (m_cancelled.load()) {
                        return;
                    }
                    if (++batches == 1) {
                        diag::log(QStringLiteral("sken: první dávka %1 souborů po %2 ms")
                                      .arg(batch.size()).arg(scanTimer.elapsed()));
                    }
                    emit scanProgress(m_generation, batch);
                    if (g_streamDelayMs > 0) {
                        QThread::msleep(static_cast<unsigned long>(g_streamDelayMs.load()));
                    }
                },
                500, 300, m_stamps.data());
        } else {
            paths = catalog.loadFolder(
                m_folderPath, m_includePdf, m_sortKey, m_ascending,
                m_includeImages, m_includeVideos, m_stamps.data());
        }
        diag::log(QStringLiteral("sken: %1 %2 souborů, %3 dávek, celkem %4 ms")
                      .arg(m_cancelled.load() ? QStringLiteral("ZRUŠEN po") : QStringLiteral("hotovo,"))
                      .arg(paths.size()).arg(batches).arg(scanTimer.elapsed()));
        if (!m_cancelled.load()) {
            emit scanComplete(m_generation, paths);
        }
    } catch (const std::exception &exception) {
        diag::log(QStringLiteral("!!! CHYBA SKENU: %1").arg(QString::fromUtf8(exception.what())));
        if (!m_cancelled.load()) {
            emit scanError(m_generation, QString::fromUtf8(exception.what()));
        }
    } catch (...) {
        // Výjimka nesmí uniknout z QRunnable::run() — propadla by mimo
        // catch handler vlákna z fondu a shodila aplikaci přes std::terminate.
        if (!m_cancelled.load()) {
            emit scanError(m_generation, QStringLiteral("Neznámá chyba při skenování složky."));
        }
    }

    emit finished(m_generation);
}

} // namespace pictureviewer

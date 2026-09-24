#include "workers/FolderScanWorker.hpp"

#include "app/SettingsManager.hpp"
#include "core/ImageCatalog.hpp"

#include <QThread>

#include <exception>

namespace pictureviewer {

FolderScanWorker::FolderScanWorker(const SettingsManager *settings, QString folderPath, int generation, QObject *parent)
    : QObject(parent)
    , m_folderPath(std::move(folderPath))
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

    try {
        ImageCatalog catalog;
        QStringList paths;
        if (m_sortKey == SortKey::Name) {
            // Řazení podle jména nepotřebuje stat() souborů, takže se výpis
            // dá ukazovat postupně (na pomalém úložišti trvá minuty).
            paths = catalog.loadFolderStreaming(
                m_folderPath, m_includePdf, m_ascending, m_includeImages, m_includeVideos,
                [this] { return m_cancelled.load(); },
                [this](const QStringList &batch) {
                    if (m_cancelled.load()) {
                        return;
                    }
                    emit scanProgress(m_generation, batch);
                    if (g_streamDelayMs > 0) {
                        QThread::msleep(static_cast<unsigned long>(g_streamDelayMs.load()));
                    }
                });
        } else {
            paths = catalog.loadFolder(
                m_folderPath, m_includePdf, m_sortKey, m_ascending,
                m_includeImages, m_includeVideos);
        }
        if (!m_cancelled.load()) {
            emit scanComplete(m_generation, paths);
        }
    } catch (const std::exception &exception) {
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

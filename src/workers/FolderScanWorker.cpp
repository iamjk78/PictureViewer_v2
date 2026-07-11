#include "workers/FolderScanWorker.hpp"

#include "app/SettingsManager.hpp"
#include "core/ImageCatalog.hpp"

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
        const QStringList paths = catalog.loadFolder(
            m_folderPath, m_includePdf, m_sortKey, m_ascending,
            m_includeImages, m_includeVideos);
        if (!m_cancelled.load()) {
            emit scanComplete(m_generation, paths);
        }
    } catch (const std::exception &exception) {
        if (!m_cancelled.load()) {
            emit scanError(m_generation, QString::fromUtf8(exception.what()));
        }
    }

    emit finished(m_generation);
}

} // namespace pictureviewer

#include "workers/FolderScanWorker.hpp"

#include "app/SettingsManager.hpp"
#include "core/ImageCatalog.hpp"

#include <exception>

namespace pictureviewer {

FolderScanWorker::FolderScanWorker(const SettingsManager *settings, QString folderPath, int generation, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_folderPath(std::move(folderPath))
    , m_generation(generation)
    , m_cancelled(false)
{
    setAutoDelete(false);
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
        const bool includePdf    = m_settings ? m_settings->enablePdfProcessing() : true;
        const bool includeImages = m_settings ? m_settings->enableImages() : true;
        const bool includeVideos = m_settings ? m_settings->enableVideos() : false;
        const SortKey sortKey    = m_settings
            ? static_cast<SortKey>(m_settings->sortKey()) : SortKey::Name;
        const bool ascending     = m_settings ? m_settings->sortAscending() : true;
        const QStringList paths  = catalog.loadFolder(
            m_folderPath, includePdf, sortKey, ascending, {}, nullptr,
            includeImages, includeVideos);
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

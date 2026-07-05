#include "workers/FolderNavWorker.hpp"

namespace pictureviewer {

FolderNavWorker::FolderNavWorker(QString currentFolder, int generation, QObject *parent)
    : QObject(parent)
    , m_currentFolder(std::move(currentFolder))
    , m_generation(generation)
    , m_cancelled(false)
{
    setAutoDelete(false);
    qRegisterMetaType<FolderNavResult>();
}

void FolderNavWorker::cancel()
{
    m_cancelled.store(true);
}

void FolderNavWorker::run()
{
    if (m_cancelled.load()) {
        emit finished(m_generation);
        return;
    }

    const FolderNavResult left  = FolderNavigator::siblingBefore(m_currentFolder);
    const FolderNavResult right = FolderNavigator::siblingAfter(m_currentFolder);
    const FolderNavResult down  = FolderNavigator::firstSubfolder(m_currentFolder);

    if (!m_cancelled.load()) {
        emit navDataReady(m_generation, left, right, down);
    }
    emit finished(m_generation);
}

} // namespace pictureviewer

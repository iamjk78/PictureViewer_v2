// MainWindow_FileOps.cpp — file/folder operations, image loading, saving, deleting
// QPushButton must be included BEFORE MainWindow.hpp to resolve the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/CategoryManager.hpp"
#include "app/ImageLoader.hpp"
#include "app/ImageView.hpp"
#include "app/MetadataPanel.hpp"
#include "app/SettingsManager.hpp"
#include "app/ThumbnailPanel.hpp"
#include "app/VideoPlayer.hpp"
#include "core/ImageFormats.hpp"
#include "workers/FolderScanWorker.hpp"
#include "workers/VideoThumbnailWorker.hpp"

#include <QApplication>
#include <QAbstractButton>
#include <QBuffer>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QThreadPool>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_MACOS
#include <sys/xattr.h>

namespace {
void removeQuarantine(const QString &path)
{
    const QByteArray p = path.toUtf8();
    removexattr(p.constData(), "com.apple.quarantine", 0);
}
} // namespace
#endif

namespace {

// Multimediální backend (Windows Media Foundation) uvolňuje handle souboru
// asynchronně i po stop() — první pokus o přesun/smazání právě přehrávaného
// videa proto může selhat na zamčený soubor. Mezi pokusy zpracujeme události
// (bez uživatelského vstupu), aby backend stihl handle uvolnit.
template <typename Op>
bool tryWithRetry(Op op, int attempts = 4, int delayMs = 150)
{
    for (int i = 0; i < attempts; ++i) {
        if (op()) {
            return true;
        }
        if (i + 1 < attempts) {
            QEventLoop loop;
            QTimer::singleShot(delayMs, &loop, &QEventLoop::quit);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
        }
    }
    return false;
}

} // namespace

namespace pictureviewer {

void MainWindow::openFolderDialog()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Otevřít složku"));
    if (!folder.isEmpty()) {
        m_requestedFile.clear();
        loadFolder(folder);
    }
}

void MainWindow::openFileDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Otevřít soubor"),
        QString(),
        tr("Podporované soubory (*.jpg *.jpeg *.png *.gif *.bmp *.webp *.tiff *.tif"
           " *.pdf *.mp4 *.mkv *.avi *.mov *.ts *.mpg *.webm *.wmv *.m4v);;"
           "Obrázky (*.jpg *.jpeg *.png *.gif *.bmp *.webp *.tiff *.tif);;"
           "Videa (*.mp4 *.mkv *.avi *.mov *.ts *.mpg *.webm *.wmv *.m4v);;"
           "PDF (*.pdf);;"
           "Všechny soubory (*)")
    );
    if (!path.isEmpty()) {
        m_requestedFile = path;
        loadFolder(path.section('/', 0, -2));
    }
}

void MainWindow::openFile(const QString &filePath)
{
    qDebug() << "openFile() called with:" << filePath;
    qDebug() << "MainWindow initialized:" << (m_imageView != nullptr);

    if (filePath.isEmpty()) {
        qDebug() << "ERROR: Empty file path received!";
        return;
    }

    QString cleanPath = filePath;
    if (cleanPath.startsWith("file://")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
        qDebug() << "Converted file URL to local path:" << cleanPath;
    }

    const QString canonical = QFileInfo(cleanPath).canonicalFilePath();
    m_requestedFile = canonical.isEmpty() ? cleanPath : canonical;
    const QString folderPath = m_requestedFile.section('/', 0, -2);
    qDebug() << "Extracted folder path:" << folderPath;
    loadFolder(folderPath);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        return;
    }
    for (const QUrl &url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        const QString suf = "." + info.suffix();
        if (info.isDir() || isSupportedFileExtension(suf) || isVideoFile(suf)) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        return;
    }
    for (const QUrl &url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString localPath = url.toLocalFile();
        const QFileInfo info(localPath);
        if (info.isDir()) {
            m_requestedFile.clear();
            loadFolder(localPath);
            event->acceptProposedAction();
            return;
        }
        const QString suf2 = "." + info.suffix();
        if (isSupportedFileExtension(suf2) || isVideoFile(suf2)) {
            openFile(localPath);
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::onScanComplete(int generation, const QStringList &paths)
{
    if (m_shuttingDown || generation != m_scanGeneration) {
        return;
    }

    if (paths.isEmpty()) {
        m_imagePaths.clear();
        m_unfilteredImagePaths.clear();
        m_currentIndex = -1;
        m_requestedFile.clear();
        m_thumbnailPanel->clear();
        m_imageView->clearImage();
        m_statusLabel->setText(tr("Ve složce nebyly nalezeny žádné obrázky."));
        return;
    }

    m_unfilteredImagePaths = paths;

    if (m_categoriesToolbar->isVisible()) {
        updateCategoryFilterButtons();
    }

    if (!m_categoryFilterIds.isEmpty()) {
        m_imagePaths = m_categoryManager->imagePathsWithAllCategories(m_categoryFilterIds);
    } else {
        m_imagePaths = paths;
    }

    m_thumbnailPanel->loadImages(m_imagePaths);

    // Spustit extrakci video miniatur (asynchronně, na hlavním vlákně přes QMediaPlayer)
    if (m_videoThumbnailWorker) {
        m_videoThumbnailWorker->cancel();
        QStringList videoPaths;
        for (const QString &p : m_imagePaths) {
            if (isVideoFile(QStringLiteral(".") + QFileInfo(p).suffix())) {
                videoPaths.append(p);
            }
        }
        if (!videoPaths.isEmpty()) {
            m_videoThumbnailWorker->enqueue(videoPaths);
        }
    }

    int index = 0;
    if (!m_requestedFile.isEmpty()) {
        qDebug() << "Looking for requested file:" << m_requestedFile;
        qDebug() << "Total images found:" << paths.count();

        int requestedIndex = paths.indexOf(m_requestedFile);
        qDebug() << "Exact match index:" << requestedIndex;

        if (requestedIndex < 0) {
            const QString requestedFileName = QFileInfo(m_requestedFile).fileName();
            qDebug() << "Trying to match filename:" << requestedFileName;

            for (int i = 0; i < paths.count(); ++i) {
                if (QFileInfo(paths.at(i)).fileName() == requestedFileName) {
                    requestedIndex = i;
                    qDebug() << "Found match at index:" << i;
                    break;
                }
            }
        }

        if (requestedIndex >= 0) {
            index = requestedIndex;
        } else {
            qDebug() << "File not found in list. First few paths:";
            for (int i = 0; i < qMin(3, paths.count()); ++i) {
                qDebug() << "  " << i << ":" << paths.at(i);
            }
        }
        m_requestedFile.clear();
    }

    showImage(index);

    if (m_rememberLastFolderAction->isChecked()) {
        m_settingsManager->setLastFolder(paths.first().section('/', 0, -2));
    }
}

void MainWindow::onScanError(int generation, const QString &error)
{
    if (generation == m_scanGeneration) {
        m_statusLabel->setText(tr("Chyba při skenování: %1").arg(error));
    }
}

void MainWindow::onScanFinished(int generation)
{
    if (generation != m_scanGeneration || m_folderScanWorker == nullptr) {
        return;
    }

    m_folderScanWorker = nullptr;
}

void MainWindow::reloadCurrentFolder()
{
    if (m_currentFolder.isEmpty()) {
        return;
    }
    if (m_currentIndex >= 0 && m_currentIndex < m_imagePaths.size()) {
        m_requestedFile = m_imagePaths.at(m_currentIndex);
    }
    loadFolder(m_currentFolder);
}

void MainWindow::loadFolder(const QString &folderPath)
{
    if (m_shuttingDown) {
        return;
    }

    m_currentFolder = folderPath;
    ++m_scanGeneration;
    if (m_reloadFolderAction) m_reloadFolderAction->setEnabled(true);
    m_statusLabel->setText(tr("Načítám složku…"));

    if (!m_requestedFile.isEmpty()) {
        displayPathEarly(m_requestedFile);
    }

    if (m_folderScanWorker != nullptr) {
        m_folderScanWorker->cancel();
        m_folderScanWorker = nullptr;
    }

    auto *worker = new FolderScanWorker(m_settingsManager, folderPath, m_scanGeneration, nullptr);
    connect(worker, &FolderScanWorker::scanComplete, this, &MainWindow::onScanComplete);
    connect(worker, &FolderScanWorker::scanError, this, &MainWindow::onScanError);
    connect(worker, &FolderScanWorker::finished, this, &MainWindow::onScanFinished);
    connect(worker, &FolderScanWorker::finished, worker, &FolderScanWorker::deleteLater);
    m_folderScanWorker = worker;
    QThreadPool::globalInstance()->start(worker);
}

void MainWindow::restoreLastFolder()
{
    if (!m_settingsManager->rememberLastFolder()) {
        return;
    }

    const QString lastFolder = m_settingsManager->lastFolder();
    if (!lastFolder.isEmpty()) {
        loadFolder(lastFolder);
    }
}

void MainWindow::showImage(int index)
{
    const QStringList imagePaths = m_imagePaths;
    if (index < 0 || index >= imagePaths.size()) {
        return;
    }
    m_isScreenshot = false;

    const QString path = imagePaths.at(index);
#ifdef Q_OS_MACOS
    removeQuarantine(path);
#endif

    const QString suffix = "." + QFileInfo(path).suffix();
    const bool isPdf   = isPdfFile(suffix);
    const bool isVideo = isVideoFile(suffix);
    const bool isGif   = QFileInfo(path).suffix().compare("gif", Qt::CaseInsensitive) == 0;

    if (isVideo) {
        // Pokud již přehráváme jiné video v auto-play režimu, zastavíme ho tiše.
        if (m_centralStack->currentWidget() == m_videoPlayer
            && m_previousImageAction->isEnabled()) {
            m_videoPlayer->stopQuietly();
        }
        m_currentIndex = index;
        m_thumbnailPanel->setCurrentIndex(index);
        // Auto-play videa NEZAKAZUJE procházení — šipky a tlačítka zůstávají funkční.
        m_centralStack->setCurrentWidget(m_videoPlayer);
        m_videoPlayer->playFile(path);
        updateStatus(path);
        return;
    }

    // Přechod z videa (auto-play) zpět na obrázek/PDF.
    if (m_centralStack->currentWidget() == m_videoPlayer
        && m_previousImageAction->isEnabled()) {
        m_videoPlayer->stopQuietly();
        m_centralStack->setCurrentWidget(m_imageView);
    }

    if (isPdf) {
        m_pendingDisplayPath.clear();
        if (!m_imageView->loadPdf(path)) {
            m_currentIndex = -1;
            m_statusLabel->setText(tr("Nepodařilo se načíst soubor: %1").arg(path));
            return;
        }
    } else if (isGif) {
        m_pendingDisplayPath.clear();
        if (!m_imageView->loadAnimation(path)) {
            const QImage cached = m_imageLoader->cachedImage(path);
            if (!cached.isNull()) {
                m_imageView->setImage(cached);
            } else {
                m_pendingDisplayPath = path;
                m_imageLoader->request(path);
            }
        }
    } else {
        const QImage cached = m_imageLoader->cachedImage(path);
        if (!cached.isNull()) {
            m_pendingDisplayPath.clear();
            m_imageView->setImage(cached);
        } else {
            m_pendingDisplayPath = path;
            const QIcon placeholder = m_thumbnailPanel->iconAt(index);
            if (!placeholder.isNull()) {
                m_imageView->setImage(placeholder.pixmap(QSize(192, 192)).toImage());
            }
            m_imageLoader->request(path);
        }
    }

    m_currentIndex = index;
    m_thumbnailPanel->setCurrentIndex(index);
    updateStatus(path);
    updateCategoryButtonStates();
    m_imageModified = false;
    updateSaveButtonStates();
    updatePdfToolbarVisibility(isPdf);

    disconnect(m_imageView, &ImageView::pdfPageChanged, this, nullptr);
    if (isPdf) {
        connect(m_imageView, &ImageView::pdfPageChanged, this, [this, path](int page, int totalPages) {
            m_statusLabel->setText(
                tr("%1   |   Stránka %2 z %3   |   PDF   |   %4 / %5")
                    .arg(QFileInfo(path).fileName())
                    .arg(page)
                    .arg(totalPages)
                    .arg(m_currentIndex + 1)
                    .arg(m_imagePaths.size())
            );
            if (m_pdfPageLabel) {
                m_pdfPageLabel->setText(tr("  %1 / %2  ").arg(page).arg(totalPages));
            }
        });
        m_imageView->emitCurrentPdfPageInfo();
    }

    if (m_galleryGridActive) {
        m_centralStack->setCurrentWidget(m_imageView);
    }

    prefetchNeighbors();
}

void MainWindow::displayPathEarly(const QString &path)
{
#ifdef Q_OS_MACOS
    removeQuarantine(path);
#endif
    const QString suffix = "." + QFileInfo(path).suffix();
    if (isPdfFile(suffix)) {
        if (m_settingsManager->enablePdfProcessing()) {
            m_imageView->loadPdf(path);
        }
        return;
    }

    const QImage cached = m_imageLoader->cachedImage(path);
    if (!cached.isNull()) {
        m_imageView->setImage(cached);
        return;
    }
    m_pendingDisplayPath = path;
    m_imageLoader->request(path);
}

void MainWindow::prefetchNeighbors()
{
    if (m_imagePaths.size() < 2 || m_currentIndex < 0) {
        return;
    }

    int direction = 0;
    if (m_lastPrefetchIndex >= 0 && m_lastPrefetchIndex != m_currentIndex) {
        direction = (m_currentIndex - m_lastPrefetchIndex > 0) ? 1 : -1;
    }
    m_lastPrefetchIndex = m_currentIndex;

    QStringList neighbors;
    const int size = m_imagePaths.size();

    auto worthPrefetching = [](const QString &path) {
        const QString suffix = "." + QFileInfo(path).suffix();
        const bool isGif = QFileInfo(path).suffix().compare("gif", Qt::CaseInsensitive) == 0;
        return !isPdfFile(suffix) && !isGif;
    };

    for (int i = 1; i <= 5; ++i) {
        int idx = m_currentIndex + (direction > 0 ? i : -i);
        idx = ((idx % size) + size) % size;
        if (worthPrefetching(m_imagePaths.at(idx))) {
            neighbors.append(m_imagePaths.at(idx));
        }
    }

    m_imageLoader->prefetch(neighbors);
}

void MainWindow::onImageDecoded(const QString &path, const QImage &image)
{
    if (path != m_pendingDisplayPath) {
        return;
    }
    m_pendingDisplayPath.clear();

    if (image.isNull()) {
        m_statusLabel->setText(tr("Nepodařilo se načíst soubor: %1").arg(path));
        return;
    }
    m_imageView->setImage(image);
}

void MainWindow::updateStatus(const QString &path)
{
    try {
        const ImageInfo info = m_imageMetadataReader.read(path);
        if (m_metadataPanel != nullptr) {
            m_metadataPanel->setMetadata(info);
        }

        QString categoryStr;
        if (m_categoryManager) {
            QList<Category> cats = m_categoryManager->categoriesForImage(path);
            QStringList catNames;
            for (int i = 0; i < qMin(3, cats.size()); ++i) {
                catNames.append(cats[i].name);
            }
            if (cats.size() > 3) {
                catNames.append("...");
            }
            if (!catNames.isEmpty()) {
                categoryStr = tr("   |   Štítky: ") + catNames.join(", ");
            }
        }

        m_statusLabel->setText(
            tr("%1   |   %2   |   %3   |   %4 kB   |   %5 / %6%7")
                .arg(info.path.section('/', -1))
                .arg(info.dimensionsString())
                .arg(info.format)
                .arg(QString::number(info.fileSizeKb(), 'f', 1))
                .arg(m_currentIndex + 1)
                .arg(m_imagePaths.size())
                .arg(categoryStr)
        );
    } catch (...) {
        m_statusLabel->setText(path.section('/', -1));
        if (m_metadataPanel != nullptr) {
            m_metadataPanel->clearMetadata();
        }
    }
}

void MainWindow::deleteOrMoveCurrentImage()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    bool deleteEnabled = m_settingsManager->enableDeleteImage();
    bool moveEnabled = m_settingsManager->enableMoveToDelete();

    if (!deleteEnabled && !moveEnabled) {
        return;
    }

    // Na Windows se video drží v paměti — zastavit jej PŘED pokusem o přesunutí/smazání
    // aby se soubor odemčil a dal se manipulovat.
    if (m_centralStack->currentWidget() == m_videoPlayer) {
        m_videoPlayer->stopQuietly();
    }

    bool shouldAskConfirmation = m_settingsManager->askConfirmationDelete();
    if (shouldAskConfirmation) {
        if (!showDeleteConfirmationDialog()) {
            return;
        }
    }

    if (deleteEnabled) {
        deleteImageToTrash();
    } else if (moveEnabled) {
        moveImageToDeleteFolder();
    }
}

void MainWindow::deleteImageToTrash()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    const QString currentPath = m_imagePaths.at(m_currentIndex);
    if (tryWithRetry([&] { return QFile::moveToTrash(currentPath); })) {
        if (m_categoryManager) {
            m_categoryManager->unassignAll(currentPath);
        }
        removeImageFromList(m_currentIndex);
    } else {
        m_statusLabel->setText(tr("Nepodařilo se odstranit obrázek: %1").arg(currentPath));
    }
}

void MainWindow::moveImageToDeleteFolder()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    const QString currentPath = m_imagePaths.at(m_currentIndex);
    const QFileInfo fileInfo(currentPath);
    const QString folderPath = fileInfo.absolutePath();
    const QString deleteFolderPath = folderPath + QDir::separator() + QStringLiteral("Delete");

    QDir deleteFolder(deleteFolderPath);
    if (!deleteFolder.exists()) {
        if (!QDir(folderPath).mkdir(QStringLiteral("Delete"))) {
            m_statusLabel->setText(tr("Nepodařilo se vytvořit složku Delete."));
            return;
        }
    }

    const QString newPath = deleteFolderPath + QDir::separator() + fileInfo.fileName();

    if (tryWithRetry([&] { return QFile::rename(currentPath, newPath); })) {
        if (m_categoryManager) {
            m_categoryManager->renameImagePath(currentPath, newPath);
        }
        m_deleteHistory.append({newPath, currentPath});
        updateRecycleButtonState();
        removeImageFromList(m_currentIndex);
    } else {
        m_statusLabel->setText(tr("Nepodařilo se přesunout obrázek do Delete."));
    }
}

void MainWindow::renameCurrentImage()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    const QString currentPath = m_imagePaths.at(m_currentIndex);
    QFileInfo fileInfo(currentPath);

    const QString baseName = fileInfo.baseName();
    const QString suffix = fileInfo.suffix();

    bool ok = false;
    QString newBaseName = QInputDialog::getText(
        this,
        tr("Přejmenování obrázku"),
        tr("Nový název:"),
        QLineEdit::Normal,
        baseName,
        &ok
    );

    if (!ok || newBaseName.isEmpty() || newBaseName == baseName) {
        return;
    }

    const QString folderPath = fileInfo.absolutePath();
    const QString newFileName = newBaseName + "." + suffix;
    const QString newPath = folderPath + "/" + newFileName;

    if (QFile::exists(newPath)) {
        QMessageBox::warning(this, tr("Chyba"), tr("Soubor '%1' již existuje.").arg(newFileName));
        return;
    }

    // Přehrávané video drží soubor zamčený — zastavit, přejmenovat a spustit
    // znovu z nové cesty.
    const bool videoWasPlaying = (m_centralStack->currentWidget() == m_videoPlayer);
    if (videoWasPlaying) {
        m_videoPlayer->stopQuietly();
    }

    if (tryWithRetry([&] { return QFile::rename(currentPath, newPath); })) {
        if (m_categoryManager) {
            m_categoryManager->renameImagePath(currentPath, newPath);
        }
        m_imagePaths[m_currentIndex] = newPath;
        m_thumbnailPanel->updateImagePath(currentPath, newPath);
        updateStatus(newPath);
        m_statusLabel->setText(tr("Obrázek přejmenován na '%1'.").arg(newFileName));
        if (videoWasPlaying) {
            m_videoPlayer->playFile(newPath);
        }
    } else {
        m_statusLabel->setText(tr("Nepodařilo se přejmenovat obrázek."));
        if (videoWasPlaying) {
            m_videoPlayer->playFile(currentPath);
        }
    }
}

void MainWindow::onDeleteFolder()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    const QString currentPath = m_imagePaths.at(m_currentIndex);
    const QString folderPath = currentPath.section('/', 0, -2);
    const QString deleteFolderPath = folderPath + "/Delete";

    QDir deleteFolder(deleteFolderPath);
    if (!deleteFolder.exists()) {
        QMessageBox::information(
            this,
            QString(),
            tr("Složka Delete neexistuje, nemohu ji smazat")
        );
        return;
    }

    const QFileInfoList entries = deleteFolder.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot
    );

    if (entries.isEmpty()) {
        if (deleteFolder.removeRecursively()) {
            QMessageBox::information(
                this,
                QString(),
                tr("Složka Delete neobsahovala soubory, smazal jsem ji")
            );
        }
        return;
    }

    int fileCount = 0;
    int dirCount = 0;

    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            dirCount++;
        } else {
            fileCount++;
        }
    }

    int result = QMessageBox::question(
        this,
        QString(),
        tr("Složka Delete obsahuje %1 souborů a %2 adresářů, chcete ji skutečně smazat?")
            .arg(fileCount)
            .arg(dirCount),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (result == QMessageBox::Yes) {
        if (deleteFolder.removeRecursively()) {
            m_statusLabel->setText(tr("Složka Delete byla smazána."));
            const QString deleteFolderPrefix = deleteFolderPath + "/";
            m_deleteHistory.removeIf([&](const QPair<QString, QString> &entry) {
                return entry.first.startsWith(deleteFolderPrefix);
            });
            updateRecycleButtonState();
        } else {
            m_statusLabel->setText(tr("Nepodařilo se smazat složku Delete."));
        }
    }
}

void MainWindow::removeImageFromList(int index)
{
    if (index < 0 || index >= m_imagePaths.size()) {
        return;
    }

    // Zastavit video, pokud se právě přehrává — aby se video nezastavilo až později
    if (m_centralStack->currentWidget() == m_videoPlayer) {
        m_videoPlayer->stopQuietly();
    }

    m_imagePaths.removeAt(index);
    m_thumbnailPanel->removeImage(index);

    if (m_imagePaths.isEmpty()) {
        m_currentIndex = -1;
        m_imageView->clearImage();
        m_statusLabel->setText(tr("Ve složce nebyly nalezeny žádné obrázky."));
        return;
    }

    int nextIndex = index;
    if (nextIndex >= m_imagePaths.size()) {
        nextIndex = m_imagePaths.size() - 1;
    }
    showImage(nextIndex);
}

void MainWindow::onUndoDelete()
{
    if (m_deleteHistory.isEmpty()) {
        return;
    }

    const auto [deletedPath, originalPath] = m_deleteHistory.last();

    if (!QFile::exists(deletedPath)) {
        m_deleteHistory.removeLast();
        updateRecycleButtonState();
        m_statusLabel->setText(tr("Soubor v Delete složce nenalezen, byl zřejmě odstraněn externě."));
        return;
    }

    if (QFile::exists(originalPath)) {
        QMessageBox::warning(this, tr("Nelze obnovit"),
            tr("V původním umístění již soubor '%1' existuje.")
                .arg(QFileInfo(originalPath).fileName()));
        return;
    }

    QDir().mkpath(QFileInfo(originalPath).absolutePath());

    if (QFile::rename(deletedPath, originalPath)) {
        if (m_categoryManager) {
            m_categoryManager->renameImagePath(deletedPath, originalPath);
        }
        m_deleteHistory.removeLast();
        updateRecycleButtonState();
        m_requestedFile = originalPath;
        loadFolder(QFileInfo(originalPath).absolutePath());
    } else {
        m_statusLabel->setText(tr("Nepodařilo se obnovit soubor."));
    }
}

void MainWindow::updateRecycleButtonState()
{
    if (m_recycleAction) {
        m_recycleAction->setEnabled(!m_deleteHistory.isEmpty());
    }
}

void MainWindow::updateSaveButtonStates()
{
    const bool hasStaticImage =
        m_currentIndex >= 0 &&
        !m_imageView->displayedImage().isNull() &&
        !m_imageView->isPdfLoaded();

    if (m_saveAction)   m_saveAction->setEnabled(hasStaticImage && m_imageModified);
    if (m_saveAsAction) m_saveAsAction->setEnabled(hasStaticImage);
}

void MainWindow::saveImageToPath(const QString &targetPath)
{
    const QImage img = m_imageView->displayedImage();
    if (img.isNull()) {
        return;
    }
    if (!img.save(targetPath, "JPEG", 92)) {
        QMessageBox::critical(this, tr("Chyba ukládání"),
            tr("Soubor se nepodařilo uložit:\n%1").arg(targetPath));
    }
}

void MainWindow::onSaveImage()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_imagePaths.size()) {
        return;
    }
    const QString currentPath = m_imagePaths.at(m_currentIndex);
    const QString fileName    = QFileInfo(currentPath).fileName();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Uložit"));
    msgBox.setText(tr("Chcete přepsat existující soubor?"));
    msgBox.setInformativeText(fileName);
    msgBox.setIcon(QMessageBox::Question);
    QPushButton *btnYes    = msgBox.addButton(tr("Ano"),         QMessageBox::AcceptRole);
    QPushButton *btnRename = msgBox.addButton(tr("Přejmenovat"), QMessageBox::ActionRole);
    /*QPushButton *btnNo =*/ msgBox.addButton(tr("Ne"),          QMessageBox::RejectRole);
    msgBox.setDefaultButton(btnYes);
    msgBox.exec();

    if (msgBox.clickedButton() == btnYes) {
        saveImageToPath(currentPath);
        m_imageModified = false;
        updateSaveButtonStates();
    } else if (msgBox.clickedButton() == btnRename) {
        const QString targetPath = runSaveAsDialog(currentPath);
        if (!targetPath.isEmpty()) {
            saveImageToPath(targetPath);
            const QString targetDir = QFileInfo(targetPath).absolutePath();
            m_requestedFile = targetPath;
            loadFolder(targetDir);
        }
    }
}

void MainWindow::onSaveAsImage()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_imagePaths.size()) {
        return;
    }
    const QString currentPath = m_imagePaths.at(m_currentIndex);
    const QString targetPath  = runSaveAsDialog(currentPath);
    if (!targetPath.isEmpty()) {
        saveImageToPath(targetPath);
        const QString targetDir = QFileInfo(targetPath).absolutePath();
        m_requestedFile = targetPath;
        loadFolder(targetDir);
    }
}

} // namespace pictureviewer

// MainWindow_FileOps.cpp — file/folder operations, image loading, saving, deleting
// QPushButton must be included BEFORE MainWindow.hpp to resolve the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/CategoryManager.hpp"
#include "app/FileRetry.hpp"
#include "app/FolderDeleteDialog.hpp"
#include "app/ImageLoader.hpp"
#include "app/ImageView.hpp"
#include "app/MetadataPanel.hpp"
#include "app/SettingsManager.hpp"
#include "app/ThumbnailPanel.hpp"
#include "app/VideoPlayer.hpp"
#include "core/CompanionFinder.hpp"
#include "core/FileNaming.hpp"
#include "core/FolderNavigator.hpp"
#include "core/ImageFormats.hpp"
#include "workers/FolderScanWorker.hpp"
#include "workers/VideoThumbnailWorker.hpp"

#include <QApplication>
#include <QAbstractButton>
#include <QBuffer>
#include <QDateTime>
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
// Rekurzivně zjistí, jestli složka obsahuje ALESPOŇ JEDEN soubor kdekoli ve
// svém podstromu (prázdné podsložky samy o sobě "prázdnost" neruší).
bool folderHasAnyFileRecursive(const QDir &dir)
{
    const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            if (folderHasAnyFileRecursive(QDir(entry.absoluteFilePath()))) {
                return true;
            }
        } else {
            return true;
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
    if (filePath.isEmpty()) {
        return;
    }

    QString cleanPath = filePath;
    if (cleanPath.startsWith("file://")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }

    const QString canonical = QFileInfo(cleanPath).canonicalFilePath();
    m_requestedFile = canonical.isEmpty() ? cleanPath : canonical;
    loadFolder(m_requestedFile.section('/', 0, -2));
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
        showEmptyContent(tr("Ve složce nebyly nalezeny žádné obrázky."));
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
            m_videoThumbnailWorker->enqueue(videoPaths, m_thumbnailPanel->generation());
        }
    }

    int index = 0;
    if (!m_requestedFile.isEmpty()) {
        int requestedIndex = paths.indexOf(m_requestedFile);
        if (requestedIndex < 0) {
            // Cesta se nemusí shodovat znak po znaku (symlink, jiná normalizace
            // unicode v názvu) — zkusit dohledat aspoň podle jména souboru.
            const QString requestedFileName = QFileInfo(m_requestedFile).fileName();
            for (int i = 0; i < paths.count(); ++i) {
                if (QFileInfo(paths.at(i)).fileName() == requestedFileName) {
                    requestedIndex = i;
                    break;
                }
            }
        }
        if (requestedIndex >= 0) {
            index = requestedIndex;
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

    // Navigace mezi složkami nezávisí na skenu obrázků (ImageCatalog) — je to
    // čistě informace o adresářové struktuře, proto se aktualizuje hned tady.
    refreshFolderNavData();

    if (!m_requestedFile.isEmpty()) {
        displayPathEarly(m_requestedFile);
    }

    if (m_folderScanWorker != nullptr) {
        m_folderScanWorker->cancel();
        m_folderScanWorker = nullptr;
    }

    auto *worker = new FolderScanWorker(m_settingsManager.get(), folderPath, m_scanGeneration, nullptr);
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

    const QString path = imagePaths.at(index);
#ifdef Q_OS_MACOS
    removeQuarantine(path);
#endif

    const QString suffix = "." + QFileInfo(path).suffix();
    const bool isPdf   = isPdfFile(suffix);
    const bool isVideo = isVideoFile(suffix);
    const bool isGif   = QFileInfo(path).suffix().compare("gif", Qt::CaseInsensitive) == 0;

    // Zrušit případné odložené spuštění videa z předchozí navigace — pokud
    // mezitím cílem přestalo být video, nesmí se přehrát se zpožděním.
    m_videoSwitchTimer->stop();

    if (isVideo) {
        // Generátor náhledů musí umlknout DŘÍV, než začne hrát VideoPlayer —
        // viz suspendVideoThumbnails().
        suspendVideoThumbnails();
        m_currentIndex = index;
        m_thumbnailPanel->setCurrentIndex(index);
        // Auto-play videa NEZAKAZUJE procházení — šipky a tlačítka zůstávají funkční.
        m_centralStack->setCurrentWidget(m_videoPlayer);
        updateStatus(path);
        m_imageModified = false;
        setContentKind(contentstate::ContentKind::Video);
        // Debounce — viz komentář u m_videoSwitchTimer v MainWindow.hpp. Rychlé
        // prolétnutí šipkami přes více videí tak skutečně přehraje jen to, na
        // kterém se navigace ustálí.
        m_pendingVideoPath = path;
        m_videoSwitchTimer->start();
        return;
    }

    // Přechod z videa (auto-play) zpět na obrázek/PDF. Bezpodmínečně —
    // dřív to bylo podmíněné stavem tlačítka „předchozí", takže při zamčeném
    // procházení (video spuštěné klávesou G) zůstal v centrální ploše
    // zastavený přehrávač a nově načtený obrázek nebyl vidět.
    if (m_centralStack->currentWidget() == m_videoPlayer) {
        m_videoPlayer->stopQuietly();
        m_centralStack->setCurrentWidget(m_imageView);
    }
    // Žádné video už nehraje — náhledy se můžou dogenerovat, jakmile se
    // navigace ustálí (odloženo, aby rychlé střídání obrázek/video worker
    // zbytečně nerozjíždělo a zase nezahazovalo).
    scheduleVideoThumbnailResume();

    contentstate::ContentKind kind = contentstate::ContentKind::Image;

    if (isPdf) {
        m_pendingDisplayPath.clear();
        if (!m_imageView->loadPdf(path)) {
            m_currentIndex = -1;
            m_statusLabel->setText(tr("Nepodařilo se načíst soubor: %1").arg(path));
            setContentKind(contentstate::ContentKind::None);
            return;
        }
        kind = contentstate::ContentKind::Pdf;
    } else if (isGif) {
        m_pendingDisplayPath.clear();
        if (m_imageView->loadAnimation(path)) {
            // Jen skutečně běžící animace je AnimatedGif — statický GIF, který
            // se načte přes ImageLoader níž, se chová jako obyčejný obrázek
            // (jde ho otočit i oříznout).
            kind = contentstate::ContentKind::AnimatedGif;
        } else {
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
    m_imageModified = false;
    setContentKind(kind);

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
    applyActionStates();
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
    // Klávesy D / Delete míří sem přes keyPressEvent PŘÍMO, mimo QAction —
    // zakázané tlačítko je tedy nechrání a pojistka musí být tady. Nad snímkem
    // obrazovky ukazuje index na soubor, který uživatel nevidí; smazat ho by
    // bylo nevratné.
    if (isCapture()) {
        return;
    }

    bool deleteEnabled = m_settingsManager->enableDeleteImage();
    bool moveEnabled = m_settingsManager->enableMoveToDelete();

    if (!deleteEnabled && !moveEnabled) {
        return;
    }

    // Na Windows se video drží v paměti — zastavit jej PŘED pokusem o přesunutí/smazání
    // aby se soubor odemčil a dal se manipulovat.
    stopVideoIfPlaying();

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

    bool cancelled = false;
    const QStringList filesToDelete = resolveCompanionSet(currentPath, tr("smazat"), cancelled);
    if (cancelled) {
        return;
    }

    // Koš nemá undo (jako dosud) — jen smazat každý soubor sady.
    const int anchorIndex = m_currentIndex;
    bool removedAny = false;
    // Na síťových discích bez podpory koše (viz stejný důvod u onDeleteCurrentFolder())
    // moveToTrash spolehlivě selže pro každý soubor skupiny — zeptat se na trvalé
    // smazání jen jednou a rozhodnutí uplatnit i na zbylé companion soubory.
    bool permanentDeleteAsked = false;
    bool permanentDeleteAllowed = false;
    for (const QString &filePath : filesToDelete) {
        if (!QFile::exists(filePath)) {
            continue;
        }
        // Zastavit i video případně auto-přehrané předchozím odebráním —
        // jinak by na Windows zůstalo zamčené a moveToTrash by selhal.
        stopVideoIfPlaying();
        bool removed = tryWithRetry([&] { return QFile::moveToTrash(filePath); });
        if (!removed) {
            if (!permanentDeleteAsked) {
                permanentDeleteAsked = true;
                permanentDeleteAllowed = QMessageBox::question(
                    this, QString(),
                    tr("Soubor se nepodařilo přesunout do koše (síťový disk košem "
                       "nedisponuje). Smazat trvale, bez možnosti obnovy?"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No) == QMessageBox::Yes;
            }
            if (permanentDeleteAllowed) {
                removed = QFile::remove(filePath);
            }
        }
        if (removed) {
            if (m_categoryManager) {
                m_categoryManager->unassignAll(filePath);
            }
            const int idx = m_imagePaths.indexOf(filePath);
            if (idx >= 0) {
                removeImageFromList(idx, /*showNext=*/false);
                removedAny = true;
            }
        } else {
            m_statusLabel->setText(tr("Nepodařilo se odstranit obrázek: %1").arg(filePath));
        }
    }
    if (removedAny) {
        showCurrentAfterRemoval(anchorIndex);
    }
}

void MainWindow::moveImageToDeleteFolder()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    const QString currentPath = m_imagePaths.at(m_currentIndex);

    bool cancelled = false;
    const QStringList filesToDelete = resolveCompanionSet(currentPath, tr("smazat"), cancelled);
    if (cancelled) {
        return;
    }

    // Všechny soubory sady jsou ve stejné složce → stejná složka Delete.
    const QString folderPath = QFileInfo(currentPath).absolutePath();
    const QString deleteFolderPath = folderPath + QDir::separator() + QStringLiteral("Delete");

    QDir deleteFolder(deleteFolderPath);
    if (!deleteFolder.exists()) {
        if (!QDir(folderPath).mkdir(QStringLiteral("Delete"))) {
            m_statusLabel->setText(tr("Nepodařilo se vytvořit složku Delete."));
            return;
        }
    }

    MoveGroup group;
    const int anchorIndex = m_currentIndex;
    bool removedAny = false;
    for (const QString &filePath : filesToDelete) {
        if (!QFile::exists(filePath)) {
            continue;
        }
        const QString newPath = deleteFolderPath + QDir::separator() + QFileInfo(filePath).fileName();

        // Zastavit i video případně auto-přehrané předchozím odebráním —
        // jinak by na Windows zůstalo zamčené a rename by selhal.
        stopVideoIfPlaying();
        if (tryWithRetry([&] { return QFile::rename(filePath, newPath); })) {
            if (m_categoryManager) {
                m_categoryManager->renameImagePath(filePath, newPath);
            }
            group.append({newPath, filePath});
            const int idx = m_imagePaths.indexOf(filePath);
            if (idx >= 0) {
                removeImageFromList(idx, /*showNext=*/false);
                removedAny = true;
            }
        } else {
            // Diagnostika: co se stalo?
            QString reason;
            if (!QFile::exists(filePath)) {
                reason = tr("soubor již neexistuje");
            } else if (QFile::exists(newPath)) {
                reason = tr("cílové umístění už existuje — zkuste ručně smazat Delete složku");
            } else if (!QFileInfo(folderPath).isWritable()) {
                reason = tr("složka nemá práva pro zápis");
            } else {
                reason = tr("soubor je stále zamčený — zkuste zavřít video a zkusit znovu");
            }
            m_statusLabel->setText(tr("Nepodařilo se přesunout obrázek do Delete: %1").arg(reason));
        }
    }

    if (removedAny) {
        showCurrentAfterRemoval(anchorIndex);
    }
    if (!group.isEmpty()) {
        appendToHistory(m_deleteHistory, group);
        updateRecycleButtonState();
    }
}

void MainWindow::renameCurrentImage()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }
    // Stejná pojistka jako u mazání — klávesa R jde sem mimo QAction.
    if (isCapture()) {
        return;
    }

    const QString currentPath = m_imagePaths.at(m_currentIndex);
    const QFileInfo fileInfo(currentPath);

    // completeBaseName(), ne baseName() — baseName() vrací text po PRVNÍ tečku,
    // zatímco suffix() po POSLEDNÍ. U "dovolena.2024.01.jpg" by dvojice
    // ("dovolena", "jpg") při složení zahodila prostřední část názvu. Stejnou
    // konvenci používá i CompanionFinder pro párování souborů.
    const QString baseName = fileInfo.completeBaseName();

    bool ok = false;
    const QString newBaseName = QInputDialog::getText(
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

    // Při zapnutém párování (Nastavení → Přesouvat/mazat i párové soubory)
    // musí přejmenování postihnout celou skupinu (obrázek + jeho video se
    // stejným základem jména) — jinak by po přejmenování jen aktivního
    // souboru pár osiřel a přestal se párovat.
    bool cancelled = false;
    const QStringList filesToRename = resolveCompanionSet(currentPath, tr("přejmenovat"), cancelled);
    if (cancelled) {
        return;
    }

    const QString folderPath = fileInfo.absolutePath();

    // Cílové cesty pro CELOU skupinu a kontrola kolizí PŘEDEM — kolize
    // kteréhokoli souboru přeruší operaci ještě před prvním přejmenováním,
    // aby skupina nezůstala rozpůlená.
    const QStringList targetPaths =
        filenaming::groupTargetPaths(filesToRename, folderPath, newBaseName);
    const QString collision = filenaming::firstExistingTarget(targetPaths);
    if (!collision.isEmpty()) {
        // U aktivního souboru je to prostá kolize jména; u párového souboru
        // (video k obrázku apod.) by bez upřesnění hláška zmínila jinou
        // příponu, než jakou uživatel psal do dialogu — matoucí bez
        // vysvětlení, že jde o párový soubor.
        const QString collisionName = QFileInfo(collision).fileName();
        const bool isActiveFile =
            targetPaths.indexOf(collision) == filesToRename.indexOf(currentPath);
        QMessageBox::warning(this, tr("Chyba"),
            isActiveFile
                ? tr("Soubor '%1' již existuje.").arg(collisionName)
                : tr("Nelze přejmenovat: párový soubor '%1' už existuje.").arg(collisionName));
        return;
    }

    // Přehrávané video drží soubor zamčený — zastavit, přejmenovat a spustit
    // znovu z nové cesty.
    const bool videoWasPlaying = stopVideoIfPlaying();

    bool activeRenamed = false;
    QString activeNewPath;
    QStringList failed;

    for (int i = 0; i < filesToRename.size(); ++i) {
        const QString &oldPath = filesToRename.at(i);
        const QString &newPath = targetPaths.at(i);

        if (tryWithRetry([&] { return QFile::rename(oldPath, newPath); })) {
            if (m_categoryManager) {
                m_categoryManager->renameImagePath(oldPath, newPath);
            }
            const int idx = m_imagePaths.indexOf(oldPath);
            if (idx >= 0) {
                m_imagePaths[idx] = newPath;
            }
            m_thumbnailPanel->updateImagePath(oldPath, newPath);
            if (oldPath == currentPath) {
                activeRenamed = true;
                activeNewPath = newPath;
                updateStatus(newPath);
            }
        } else {
            failed.append(QFileInfo(oldPath).fileName());
        }
    }

    if (failed.isEmpty()) {
        m_statusLabel->setText(
            tr("Přejmenováno na '%1'.").arg(QFileInfo(targetPaths.first()).fileName()));
    } else {
        m_statusLabel->setText(
            tr("Nepodařilo se přejmenovat: %1").arg(failed.join(QStringLiteral(", "))));
    }

    if (videoWasPlaying) {
        m_videoPlayer->playFile(activeRenamed ? activeNewPath : currentPath);
    }
}

void MainWindow::onDeleteFolder()
{
    // Složka Delete se smaže i když je aktuální složka prázdná (m_imagePaths.isEmpty()).
    // Použijeme m_currentFolder místo odvozování cesty z m_imagePaths.
    if (m_currentFolder.isEmpty()) {
        return;
    }

    const QString deleteFolderPath = m_currentFolder + "/Delete";

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
            // Soubory ve skupině sdílí jednu složku Delete — stačí zkontrolovat
            // první záznam. Fyzicky smazané skupiny odstranit z historie undo.
            m_deleteHistory.removeIf([&](const MoveGroup &group) {
                return !group.isEmpty() && group.first().first.startsWith(deleteFolderPrefix);
            });
            updateRecycleButtonState();
        } else {
            m_statusLabel->setText(tr("Nepodařilo se smazat složku Delete."));
        }
    }
}

void MainWindow::onDeleteCurrentFolder()
{
    if (m_currentFolder.isEmpty()) {
        return;
    }

    const QString folderToDelete = m_currentFolder;
    QDir dir(folderToDelete);
    if (!dir.exists()) {
        return;
    }

    // Cíl navigace po smazání se musí zjistit PŘED smazáním — potřebuje ještě
    // existující adresářovou strukturu (sourozence i rodiče).
    FolderNavResult next = FolderNavigator::siblingAfter(folderToDelete);
    if (!next.available) {
        next = FolderNavigator::siblingBefore(folderToDelete);
    }
    if (!next.available) {
        next = FolderNavigator::parentFolder(folderToDelete);
    }

    if (folderHasAnyFileRecursive(dir)) {
        const QFileInfoList topLevel = dir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst);
        QStringList entryNames;
        entryNames.reserve(topLevel.size());
        for (const QFileInfo &info : topLevel) {
            entryNames.append(info.isDir() ? tr("📁 %1").arg(info.fileName()) : info.fileName());
        }

        FolderDeleteConfirmDialog dialog(folderToDelete, entryNames, this);
        if (dialog.exec() != QDialog::Accepted || !dialog.confirmed()) {
            return;
        }
    }

    stopVideoIfPlaying();

    if (!tryWithRetry([&] { return QFile::moveToTrash(folderToDelete); })) {
        // Síťové disky (typicky SMB sdílení bez podpory koše na straně NAS)
        // moveToTrash spolehlivě odmítnou — NSFileManager/Qt to hlásí jako
        // "volume doesn't have a trash". Nabídnout trvalé smazání jako fallback,
        // stejně jako to dělá Finder u nekošovatelných síťových svazků.
        const int result = QMessageBox::question(
            this, QString(),
            tr("Složku se nepodařilo přesunout do koše (síťový disk košem "
               "nedisponuje). Smazat ji trvale, bez možnosti obnovy?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes || !QDir(folderToDelete).removeRecursively()) {
            m_statusLabel->setText(tr("Nepodařilo se smazat složku '%1'.").arg(folderToDelete));
            return;
        }
    }

    const QString deletedName = QFileInfo(folderToDelete).fileName();

    if (next.available && QDir(next.path).exists()) {
        loadFolder(next.path);
        m_statusLabel->setText(tr("Složka '%1' byla smazána.").arg(deletedName));
        return;
    }

    // Není kam navigovat (žádný sourozenec ani rodič) — vyprázdnit stav
    // stejným způsobem jako při přepnutí na profil bez zapamatované složky.
    m_imagePaths.clear();
    m_unfilteredImagePaths.clear();
    m_categoryFilterIds.clear();
    m_currentIndex = -1;
    m_currentFolder.clear();
    m_imageView->clearImage();
    m_thumbnailPanel->loadImages({});
    updateCategoryFilterButtons();
    refreshFolderNavData();
    m_statusLabel->setText(tr("Složka '%1' byla smazána. Žádná další složka k zobrazení.").arg(deletedName));
}

bool MainWindow::stopVideoIfPlaying()
{
    if (m_centralStack->currentWidget() != m_videoPlayer) {
        return false;
    }
    m_videoPlayer->stopQuietly();
    return true;
}

void MainWindow::removeImageFromList(int index, bool showNext)
{
    if (index < 0 || index >= m_imagePaths.size()) {
        return;
    }

    // Zastavit video, pokud se právě přehrává — aby se video nezastavilo až později
    stopVideoIfPlaying();

    m_imagePaths.removeAt(index);
    m_thumbnailPanel->removeImage(index);

    if (m_imagePaths.isEmpty()) {
        m_currentIndex = -1;
        showEmptyContent(tr("Ve složce nebyly nalezeny žádné obrázky."));
        return;
    }

    if (!showNext) {
        return;   // hromadná operace — zobrazení dořeší showCurrentAfterRemoval()
    }

    int nextIndex = index;
    if (nextIndex >= m_imagePaths.size()) {
        nextIndex = m_imagePaths.size() - 1;
    }
    showImage(nextIndex);
}

void MainWindow::showCurrentAfterRemoval(int anchorIndex)
{
    if (m_imagePaths.isEmpty()) {
        return;   // prázdný stav už nastavil removeImageFromList
    }
    showImage(qBound(0, anchorIndex, static_cast<int>(m_imagePaths.size()) - 1));
}

void MainWindow::onUndoDelete()
{
    undoLastGroup(m_deleteHistory,
                  tr("Soubory v Delete složce nenalezeny, byly zřejmě odstraněny externě."));
}

void MainWindow::updateRecycleButtonState()
{
    if (m_recycleAction) {
        m_recycleAction->setEnabled(!m_deleteHistory.isEmpty());
    }
}

void MainWindow::showEmptyContent(const QString &message)
{
    // Prázdný stav musí zrušit VŠECHNO, co po předchozím obsahu zůstalo —
    // dřív se volalo jen clearImage(), takže při přechodu do prázdné složky
    // hrálo video dál, PDF toolbar zůstal viset se starými čísly stránek a
    // Uložit jako zůstalo aktivní jako mrtvé tlačítko.
    stopSlideshowIfRunning();
    if (stopVideoIfPlaying()) {
        scheduleVideoThumbnailResume();
    }
    m_centralStack->setCurrentWidget(m_imageView);
    m_imageView->clearImage();
    m_imageModified = false;
    m_statusLabel->setText(message);
    setContentKind(contentstate::ContentKind::None);
}

void MainWindow::setContentKind(contentstate::ContentKind kind)
{
    m_contentKind = kind;
    applyActionStates();
}

void MainWindow::applyActionStates()
{
    contentstate::ContentStatus status;
    status.kind           = m_contentKind;
    status.modified       = m_imageModified;
    status.hasCurrentFile = m_currentIndex >= 0 && m_currentIndex < m_imagePaths.size();
    status.hasFiles       = !m_imagePaths.isEmpty();
    status.browsingLocked = m_browsingLocked;

    const contentstate::ActionStates s = contentstate::deriveActionStates(status);

    if (m_previousImageAction)  m_previousImageAction->setEnabled(s.previous);
    if (m_nextImageAction)      m_nextImageAction->setEnabled(s.next);
    if (m_toggleSlideshowAction) m_toggleSlideshowAction->setEnabled(s.slideshow);
    if (m_rotateLeftAction)     m_rotateLeftAction->setEnabled(s.rotate);
    if (m_rotateRightAction)    m_rotateRightAction->setEnabled(s.rotate);
    if (m_cropAction)           m_cropAction->setEnabled(s.crop);
    if (m_saveAction)           m_saveAction->setEnabled(s.save);
    if (m_saveAsAction)         m_saveAsAction->setEnabled(s.saveAs);
    if (m_deletePictureAction)  m_deletePictureAction->setEnabled(s.deleteFile);
    if (m_renameImageAction)    m_renameImageAction->setEnabled(s.rename);

    // Tlačítka přesunu a štítků žijí v sekundárních toolbarech a jsou to
    // widgety, ne akce — stav se jim ale odvozuje ze stejné pravdy.
    for (QPushButton *btn : m_moveButtons) {
        btn->setEnabled(s.moveToFolder);
    }
    updateCategoryButtonStates();

    updatePdfToolbarVisibility(s.pdfToolbar);
    if (m_fitControlsWidget) {
        m_fitControlsWidget->setVisible(s.fitControls);
    }
    if (!s.fitControls && m_zoomLabel) {
        m_zoomLabel->hide();
    }
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
    // Obranná pojistka — tlačítko je pro snímek vždy neaktivní (viz
    // deriveActionStates), ale "přepsat" cestu z m_imagePaths by u snímku
    // přepsalo cizí, náhodně otevřený soubor. Radši nikdy.
    if (isCapture()) {
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
        applyActionStates();
    } else if (msgBox.clickedButton() == btnRename) {
        // Sem se lze dostat jen pro statický obrázek (tlačítko Uložit je pro
        // video vždy neaktivní), výstup je tedy vždy JPEG.
        const QString targetPath = runSaveAsDialog(currentPath, QStringLiteral("jpg"));
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
    const bool hasCurrentFile = m_currentIndex >= 0 && m_currentIndex < m_imagePaths.size();
    // Snímek jde uložit i BEZ otevřené složky (hned po startu aplikace) —
    // dřív se tu skončilo na chybějícím indexu a snímek nešel uložit vůbec.
    if (!hasCurrentFile && !isCapture()) {
        return;
    }
    const QString currentPath = hasCurrentFile ? m_imagePaths.at(m_currentIndex) : QString();
    const QFileInfo currentInfo(currentPath);
    const QString suffix = QStringLiteral(".") + currentInfo.suffix();

    // !isCapture() — snímek se dá pořídit i nad přehrávaným videem (viz
    // onScreenshotCapture()); m_currentIndex pak dál ukazuje na cestu videa,
    // i když ImageView zobrazuje snímek. Bez téhle výjimky by se tu znovu
    // zkopíroval PŮVODNÍ VIDEOSOUBOR pod novým jménem místo uložení toho,
    // co je doopravdy zobrazené.
    if (!isCapture() && isVideoFile(suffix)) {
        // Video se neupravuje — Uložit jako je tu čistá duplikace souboru pod
        // novým názvem, ne re-enkódování přes ImageView (to u videa nic
        // nezobrazuje, viz deriveActionStates).
        const QString targetPath = runSaveAsDialog(currentPath, currentInfo.suffix());
        if (targetPath.isEmpty()) {
            return;
        }
        // QFile::copy() přes existující soubor odmítne zapsat, takže volba
        // „Přepsat" v dialogu vždy skončila chybou. Cíl už uživatel odsouhlasil
        // (jinak by ho runSaveAsDialog nevrátil), takže ho smažeme předem.
        if (QFileInfo(targetPath) == currentInfo) {
            return;   // uložit sám do sebe = nic k dělání
        }
        if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
            QMessageBox::critical(this, tr("Chyba ukládání"),
                tr("Původní soubor se nepodařilo nahradit:\n%1").arg(targetPath));
            return;
        }
        if (!QFile::copy(currentPath, targetPath)) {
            QMessageBox::critical(this, tr("Chyba ukládání"),
                tr("Soubor se nepodařilo uložit:\n%1").arg(targetPath));
            return;
        }
        m_requestedFile = targetPath;
        loadFolder(QFileInfo(targetPath).absolutePath());
        return;
    }

    // Snímek nemá žádný smysluplný "původní název" — currentPath je jen cesta
    // naposledy navigovaného souboru (viz komentář výše), ne soubor snímku.
    // Navrhnout místo toho název podle typu a času pořízení. ':' se v názvu
    // vynechává — na Windows je v souborech zakázaný znak.
    const QString defaultBaseName = isCapture()
        ? tr("Screenshot %1").arg(
              QDateTime::currentDateTime().toString(QStringLiteral("HH-mm dd.MM.yyyy")))
        : QString();

    const QString targetPath = runSaveAsDialog(currentPath, QStringLiteral("jpg"), defaultBaseName);
    if (!targetPath.isEmpty()) {
        saveImageToPath(targetPath);
        const QString targetDir = QFileInfo(targetPath).absolutePath();
        m_requestedFile = targetPath;
        loadFolder(targetDir);
    }
}

} // namespace pictureviewer

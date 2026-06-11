#include "app/CategoryManager.hpp"
#include "app/HelpDialog.hpp"
#include "app/ImageLoader.hpp"
#include "app/ImageView.hpp"
#include "app/MainWindow.hpp"
#include "app/MetadataPanel.hpp"
#include "app/SettingsManager.hpp"
#include "app/SlideshowController.hpp"
#include "app/ThumbnailCacheManager.hpp"
#include "app/ThumbnailPanel.hpp"
#include "app/VlcController.hpp"
#include "core/ImageFormats.hpp"
#include "workers/FolderScanWorker.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsColorizeEffect>
#include <QMessageBox>
#include <QUrl>

#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QActionGroup>
#include <QClipboard>
#include <QDesktopServices>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QProcess>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QStackedWidget>
#include <QToolButton>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QWidget>
#include <QThreadPool>

namespace {

#ifdef Q_OS_MACOS
// Removes the com.apple.quarantine xattr so the OS lets us load the file.
// Called before every loadImage(); safe to call even if the attribute is absent.
void removeQuarantine(const QString &path)
{
    const QByteArray p = path.toUtf8();
    removexattr(p.constData(), "com.apple.quarantine", 0);
}
#endif

QString uiLayoutToString(pictureviewer::MainWindow::UiLayout layout)
{
    using UiLayout = pictureviewer::MainWindow::UiLayout;
    switch (layout) {
    case UiLayout::Filmstrip: return QStringLiteral("filmstrip");
    case UiLayout::Immersive: return QStringLiteral("immersive");
    case UiLayout::Gallery:   return QStringLiteral("gallery");
    case UiLayout::Pro:       return QStringLiteral("pro");
    case UiLayout::Classic:   break;
    }
    return QStringLiteral("classic");
}

pictureviewer::MainWindow::UiLayout uiLayoutFromString(const QString &name)
{
    using UiLayout = pictureviewer::MainWindow::UiLayout;
    if (name == QLatin1String("filmstrip")) return UiLayout::Filmstrip;
    if (name == QLatin1String("immersive")) return UiLayout::Immersive;
    if (name == QLatin1String("gallery"))   return UiLayout::Gallery;
    if (name == QLatin1String("pro"))       return UiLayout::Pro;
    return UiLayout::Classic;
}

} // anonymous namespace

namespace pictureviewer {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_folderScanWorker(nullptr)
    , m_imageView(new ImageView(this))
    , m_settingsManager(new SettingsManager())
    , m_vlcController(new VlcController(m_settingsManager, this))
    , m_thumbnailPanel(new ThumbnailPanel(this))
    , m_thumbnailDock(nullptr)
    , m_statusLabel(new QLabel(this))
    , m_intervalSpinBox(new QSpinBox(this))
    , m_slideshowController(new SlideshowController(this))
    , m_openFolderAction(new QAction(tr("Otevřít složku…"), this))
    , m_openFileAction(new QAction(tr("Otevřít soubor…"), this))
    , m_previousImageAction(new QAction(tr("◀ Předchozí"), this))
    , m_nextImageAction(new QAction(tr("Další ▶"), this))
    , m_toggleSlideshowAction(new QAction(tr("▶ Slideshow (S)"), this))
    , m_fitToWindowAction(new QAction(tr("Přizpůsobit oknu"), this))
    , m_resetZoomAction(new QAction(tr("Originální velikost (1:1)"), this))
    , m_fullscreenAction(new QAction(tr("Celá obrazovka (F)"), this))
    , m_rememberLastFolderAction(new QAction(tr("Zapamatovat poslední složku"), this))
    , m_togglePanelAction(nullptr)
    , m_enableDeleteImageAction(new QAction(tr("Odstranění obrázku"), this))
    , m_enableMoveToDeleteAction(new QAction(tr("Přesunutí obrázku do složky Delete"), this))
    , m_askConfirmationAction(new QAction(tr("Ptát se na potvrzení"), this))
    , m_enablePdfProcessingAction(new QAction(tr("Zpracovávat PDF soubory"), this))
    , m_deleteFolderAction(new QAction(this))
    , m_deletePictureAction(new QAction(this))
    , m_renameImageAction(new QAction(this))
{
    // Inicializovat CategoryManager — databáze vedle config.ini
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/categories.db";
    m_categoryManager = new CategoryManager(dbPath);

    m_deleteFolderAction->setIcon(QIcon(":/icons/delete_folder_icon.ico"));
    m_deleteFolderAction->setToolTip(tr("Smazání složky Delete"));
    connect(m_deleteFolderAction, &QAction::triggered, this, &MainWindow::onDeleteFolder);

    m_deletePictureAction->setIcon(QIcon(":/icons/delete_picture_icon.ico"));
    m_deletePictureAction->setToolTip(tr("Smazání obrázku"));
    connect(m_deletePictureAction, &QAction::triggered, this, &MainWindow::deleteOrMoveCurrentImage);

    m_renameImageAction->setIcon(QIcon(":/icons/rename.ico"));
    m_renameImageAction->setToolTip(tr("Přejmenování obrázku (R)"));
    m_renameImageAction->setShortcut(QKeySequence("R"));
    connect(m_renameImageAction, &QAction::triggered, this, &MainWindow::renameCurrentImage);

    // Connect VLC signals
    connect(m_vlcController, &VlcController::statusChanged,
            this, [this](VlcState state) { onVlcStatusChanged(static_cast<int>(state)); });
    connect(m_vlcController, &VlcController::connectionLost,
            this, &MainWindow::onVlcConnectionLost);
    connect(m_vlcController, &VlcController::processCrashed,
            this, &MainWindow::onVlcProcessCrashed);

    setWindowTitle("PictureViewer v." + QCoreApplication::applicationVersion());
    resize(1200, 750);
    setWindowIcon(QIcon(":/icons/eye_icon.ico"));
    setAcceptDrops(true);   // přetažení složky/souboru do okna

    // Centrální stack: index 0 = ImageView; v režimu Galerie se sem dočasně
    // přesouvá ThumbnailPanel jako mřížka přes celé okno.
    m_centralStack = new QStackedWidget(this);
    m_centralStack->addWidget(m_imageView);
    setCentralWidget(m_centralStack);

    m_imageLoader = new ImageLoader(this);
    connect(m_imageLoader, &ImageLoader::imageReady, this, &MainWindow::onImageDecoded);
    connect(m_imageView, &ImageView::contextMenuRequested, this, &MainWindow::showImageContextMenu);

    m_thumbnailPanel->setDiskCache(m_settingsManager->thumbnailCacheEnabled(),
                                   m_settingsManager->effectiveThumbnailCacheDir());

    // Vyčistit cache náhledů pokud překročila limit (500 MB)
    ThumbnailCacheManager::cleanupIfNeeded(m_settingsManager->effectiveThumbnailCacheDir());

    connect(m_thumbnailPanel, &ThumbnailPanel::imageSelected, this, &MainWindow::showImage);
    connect(m_slideshowController, &SlideshowController::nextImageRequested, this, &MainWindow::showNextImage);
    setupDock();
    setupMenu();
    setupToolbar();
    setupStatusBar();
    setupOverlayToolbar();

    // Sledování myši pro imerzivní režim (zobrazení plovoucího ovládání)
    m_imageView->viewport()->setMouseTracking(true);
    m_imageView->viewport()->installEventFilter(this);

    applyUiLayout(uiLayoutFromString(m_settingsManager->uiLayout()));

    // Only restore last folder if no image file is being opened
    // This prevents race condition when opening image from Finder
    if (qApp->arguments().size() <= 1) {
        restoreLastFolder();
    } else {
        qDebug() << "Skipping restoreLastFolder() - image file passed as argument";
    }
}

// ── cancelAllWorkers ─────────────────────────────────────────────────────────
// Single choke-point for all background-task teardown.
// Safe to call multiple times (all branches are null-guarded).
void MainWindow::cancelAllWorkers()
{
    // Hard gate: set before anything else so every re-entrant call
    // to loadFolder() / onScanComplete() becomes a no-op immediately.
    m_shuttingDown = true;

    // Bump the generation counter so any cross-thread scanComplete / scanError
    // signals that are already queued in the event system are silently dropped
    // by the existing generation check in onScanComplete / onScanError.
    ++m_scanGeneration;

    if (m_folderScanWorker != nullptr) {
        m_folderScanWorker->cancel();
        // Sever connections from worker to this window; already-queued events
        // for these connections will be dropped by Qt when the connection is gone.
        disconnect(m_folderScanWorker, nullptr, this, nullptr);
        m_folderScanWorker = nullptr;
    }

    // Cancel + disconnect every signal from ThumbnailWorker to ThumbnailPanel.
    m_thumbnailPanel->shutdown();

    // Stop delivering decoded images; running decodes finish into the void.
    if (m_imageLoader != nullptr) {
        m_imageLoader->shutdown();
    }

}

// ── closeEvent ────────────────────────────────────────────────────────────────
// Called while the Qt event loop is still live (user pressed ✕, Cmd+W, etc.).
// Running the shutdown here — before exec() returns — guarantees that:
//   1. Workers are cancelled and their signal connections severed.
//   2. waitForDone() blocks until every run() exits (workers only touch their
//      own members from this point; all signals into UI are disconnected).
//   3. processEvents() flushes any cross-thread signals that were already
//      posted before disconnect() ran; they are dropped because the connections
//      no longer exist and no new workers will be started.
// By the time exec() returns, the thread pool is completely idle.
void MainWindow::closeEvent(QCloseEvent *event)
{
    cancelAllWorkers();
    QThreadPool::globalInstance()->waitForDone();
    // NOTE: processEvents() is intentionally omitted here.
    // cancelAllWorkers() sets m_shuttingDown=true so any stale queued signals
    // that processEvents() would have delivered are dropped at their own entry
    // points (loadFolder / onScanComplete / loadImages) without starting new work.
    // Calling processEvents() inside closeEvent() is re-entrant and was the
    // root cause of new workers being spawned after waitForDone() returned.
    QMainWindow::closeEvent(event);
}

// ── ~MainWindow ───────────────────────────────────────────────────────────────
// Fallback safety net for paths that bypass closeEvent (e.g. programmatic
// QApplication::quit() from Cmd+Q on macOS, or unit-test teardown).
// cancelAllWorkers() is idempotent, waitForDone() returns immediately when
// the pool is already idle (the common path after closeEvent ran).
// Child QObjects are still alive here; Qt destroys them after this body returns.
MainWindow::~MainWindow()
{
    cancelAllWorkers();
    QThreadPool::globalInstance()->waitForDone();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // ── VLC se právě spouští ─────────────────────────────────────────────────
    // initialize() blokuje (waitForStarted, případně modální dialog výběru VLC)
    // a po tu dobu může event loop doručit klávesy. m_vlcActive je ale stále
    // false (nastaví se až na Running), takže by klávesy spadly do normální
    // navigace a např. druhé 'g' by spustilo druhou inicializaci. Polkneme je.
    if (m_vlcController->state() == VlcState::Starting) {
        event->accept();
        return;
    }

    // ── VLC playback active ──────────────────────────────────────────────────
    if (m_vlcActive) {
        switch (event->key()) {
        case Qt::Key_Escape:
            m_vlcController->stop();
            event->accept();
            return;
        case Qt::Key_Space:
            m_vlcController->sendCommand("pause");
            event->accept();
            return;
        case Qt::Key_Left:
            m_vlcController->sendCommand("seek -10");
            event->accept();
            return;
        case Qt::Key_Right:
            m_vlcController->sendCommand("seek +10");
            event->accept();
            return;
        case Qt::Key_F:
            m_vlcController->sendCommand("f");
            event->accept();
            return;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            m_vlcController->sendCommand("volup");
            event->accept();
            return;
        case Qt::Key_Minus:
            m_vlcController->sendCommand("voldown");
            event->accept();
            return;
        default:
            event->ignore();
            return;
        }
    }

    // ── Normal image browsing ────────────────────────────────────────────────
    switch (event->key()) {
    case Qt::Key_Left:
        showPreviousImage();
        event->accept();
        return;
    case Qt::Key_Right:
        showNextImage();
        event->accept();
        return;
    case Qt::Key_F:
        toggleFullscreen();
        event->accept();
        return;
    case Qt::Key_Delete:
        deleteOrMoveCurrentImage();
        event->accept();
        return;
    case Qt::Key_Escape:
        if (m_isFullscreen) {
            exitFullscreen();
        } else if (m_galleryGridActive
                   && m_centralStack->currentWidget() == m_imageView) {
            // V režimu Galerie se Esc vrací z obrázku zpět do mřížky
            showGalleryGrid();
        } else {
            close();
        }
        event->accept();
        return;
    case Qt::Key_Up:
        showImage(0);
        event->accept();
        return;
    case Qt::Key_Down:
        showImage(m_imagePaths.size() - 1);
        event->accept();
        return;
    case Qt::Key_PageDown:
        // PDF page down
        if (m_imageView->nextPage()) {
            event->accept();
            return;
        }
        // If not PDF or last page, go to next image
        showNextImage();
        event->accept();
        return;
    case Qt::Key_PageUp:
        // PDF page up
        if (m_imageView->previousPage()) {
            event->accept();
            return;
        }
        // If not PDF or first page, go to previous image
        showPreviousImage();
        event->accept();
        return;
    default:
        // Handle 'g' and 'G' key for play video
        if (event->text() == 'g' || event->text() == 'G') {
            onPlayVideo();
            event->accept();
            return;
        }
        // Handle 's' and 'S' key for slideshow
        if (event->text() == 's' || event->text() == 'S') {
            toggleSlideshow();
            event->accept();
            return;
        }
        // Handle 'd' and 'D' key for delete
        if (event->text() == 'd' || event->text() == 'D') {
            deleteOrMoveCurrentImage();
            event->accept();
            return;
        }
        // Handle 'r' and 'R' key for rename
        if (event->text() == 'r' || event->text() == 'R') {
            renameCurrentImage();
            event->accept();
            return;
        }
        // Otočení obrázku řeší QAction zkratky ([ / ] / L) v setupToolbar() —
        // fungují window-wide bez ohledu na to, co má zrovna focus.
        QMainWindow::keyPressEvent(event);
        return;
    }
}

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
        tr("Obrázky (*.jpg *.jpeg *.png *.gif *.bmp *.webp *.tiff *.tif)")
    );
    if (!path.isEmpty()) {
        m_requestedFile = path;
        loadFolder(path.section('/', 0, -2));
    }
}

void MainWindow::openFile(const QString &filePath)
{
    // Called from macOS file open events (Finder, Open With, etc.)
    // Extract folder and set requested file for opening after folder scan
    qDebug() << "openFile() called with:" << filePath;
    qDebug() << "MainWindow initialized:" << (m_imageView != nullptr);

    if (filePath.isEmpty()) {
        qDebug() << "ERROR: Empty file path received!";
        return;
    }

    // Handle both absolute paths and file URLs (macOS sometimes uses file://)
    QString cleanPath = filePath;
    if (cleanPath.startsWith("file://")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
        qDebug() << "Converted file URL to local path:" << cleanPath;
    }

    m_requestedFile = cleanPath;
    const QString folderPath = cleanPath.section('/', 0, -2);
    qDebug() << "Extracted folder path:" << folderPath;
    loadFolder(folderPath);
}

// ── Drag & drop ──────────────────────────────────────────────────────────────
// Přijmeme přetažení, pokud aspoň jeden lokální URL je složka nebo podporovaný
// soubor. Drop pak otevře první takový — složku přes loadFolder(), soubor přes
// openFile() (ten dohledá složku a vybere přesně tento soubor).

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
        if (info.isDir() || isSupportedFileExtension("." + info.suffix())) {
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
        if (isSupportedFileExtension("." + info.suffix())) {
            openFile(localPath);
            event->acceptProposedAction();
            return;
        }
    }
}

// ── Kontextové menu nad obrázkem ─────────────────────────────────────────────

void MainWindow::showImageContextMenu(const QPoint &globalPos)
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0
        || m_currentIndex >= m_imagePaths.size()) {
        return;
    }
    const QString currentPath = m_imagePaths.at(m_currentIndex);

    QMenu menu(this);

    QAction *revealAction = menu.addAction(tr("Zobrazit ve Finderu"));
    connect(revealAction, &QAction::triggered, this, [currentPath] {
#if defined(Q_OS_MACOS)
        QProcess::startDetached("open", {"-R", currentPath});
#elif defined(Q_OS_WIN)
        QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(currentPath)});
#else
        // Linux/ostatní: otevřít obsahující složku
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(currentPath).absolutePath()));
#endif
    });

    QAction *copyImageAction = menu.addAction(tr("Kopírovat obrázek"));
    connect(copyImageAction, &QAction::triggered, this, [this] {
        const QImage image = m_imageView->displayedImage();
        if (!image.isNull()) {
            QApplication::clipboard()->setImage(image);
        }
    });

    QAction *copyPathAction = menu.addAction(tr("Kopírovat cestu k souboru"));
    connect(copyPathAction, &QAction::triggered, this, [currentPath] {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(currentPath));
    });

    menu.exec(globalPos);
}

void MainWindow::onRememberLastFolderToggled(bool checked)
{
    m_settingsManager->setRememberLastFolder(checked);
    if (!checked) {
        m_settingsManager->clearLastFolder();
    }
}

void MainWindow::onScanComplete(int generation, const QStringList &paths)
{
    if (m_shuttingDown || generation != m_scanGeneration) {
        return;
    }

    if (paths.isEmpty()) {
        m_imagePaths.clear();
        m_currentIndex = -1;
        m_requestedFile.clear();   // jinak zůstane viset pro další sken
        m_thumbnailPanel->clear();
        m_imageView->clearImage();
        m_statusLabel->setText(tr("Ve složce nebyly nalezeny žádné obrázky."));
        return;
    }

    m_imagePaths = paths;
    m_thumbnailPanel->loadImages(paths);

    int index = 0;
    if (!m_requestedFile.isEmpty()) {
        qDebug() << "Looking for requested file:" << m_requestedFile;
        qDebug() << "Total images found:" << paths.count();

        // First, try exact match
        int requestedIndex = paths.indexOf(m_requestedFile);
        qDebug() << "Exact match index:" << requestedIndex;

        // If not found, try matching by filename (in case path format differs)
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


void MainWindow::showPreviousImage()
{
    if (m_imagePaths.isEmpty()) {
        return;
    }

    const int nextIndex = (m_currentIndex - 1 + m_imagePaths.size()) % m_imagePaths.size();
    showImage(nextIndex);
}

void MainWindow::showNextImage()
{
    if (m_imagePaths.isEmpty()) {
        return;
    }

    const int nextIndex = (m_currentIndex + 1) % m_imagePaths.size();
    showImage(nextIndex);
}

void MainWindow::toggleSlideshow()
{
    m_slideshowController->toggle();
    if (m_slideshowController->isRunning()) {
        m_toggleSlideshowAction->setText(tr("⏸ Zastavit"));
        return;
    }

    m_toggleSlideshowAction->setText(tr("▶ Slideshow"));
}

void MainWindow::reloadCurrentFolder()
{
    if (m_currentFolder.isEmpty()) {
        return;
    }
    // Zachovat právě zobrazený soubor — po přeřazení se na něj vrátíme.
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
    m_statusLabel->setText(tr("Načítám složku…"));

    // Když uživatel otvírá konkrétní soubor, zobrazit ho hned — nečekat,
    // až doběhne sken celé složky (na síťovém disku i několik sekund).
    if (!m_requestedFile.isEmpty()) {
        displayPathEarly(m_requestedFile);
    }

    if (m_folderScanWorker != nullptr) {
        m_folderScanWorker->cancel();
        m_folderScanWorker = nullptr;
    }

    // Parent must be nullptr — memory is managed by the deleteLater connection
    // below. A Qt parent would create a second deletion path → double-free.
    auto *worker = new FolderScanWorker(m_settingsManager, folderPath, m_scanGeneration, nullptr);
    connect(worker, &FolderScanWorker::scanComplete, this, &MainWindow::onScanComplete);
    connect(worker, &FolderScanWorker::scanError, this, &MainWindow::onScanError);
    connect(worker, &FolderScanWorker::finished, this, &MainWindow::onScanFinished);
    connect(worker, &FolderScanWorker::finished, worker, &FolderScanWorker::deleteLater);
    m_folderScanWorker = worker;
    QThreadPool::globalInstance()->start(worker);
}

void MainWindow::enterFullscreen()
{
    m_isFullscreen = true;
    menuBar()->hide();
    for (QToolBar *toolbar : findChildren<QToolBar *>()) {
        toolbar->hide();
    }
    if (m_thumbnailDock != nullptr) {
        // Zapamatovat viditelnost PŘED hide() — hide() automaticky odškrtne
        // toggleViewAction, takže isChecked() by po hide() vrátilo false.
        m_thumbnailDockWasVisible = m_thumbnailDock->isVisible();
        m_thumbnailDock->hide();
    }
    statusBar()->hide();
    showFullScreen();
}

void MainWindow::exitFullscreen()
{
    m_isFullscreen = false;
    showNormal();
    menuBar()->show();
    // Viditelnost toolbaru, docků a status baru řídí aktuální rozložení —
    // bezpodmínečné show() by např. v imerzivním režimu vrátilo chrome zpět.
    applyUiLayout(m_uiLayout);
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
    // Take a snapshot of the current path list to avoid TOCTOU race condition
    // where list changes between validation and usage
    const QStringList imagePaths = m_imagePaths;
    if (index < 0 || index >= imagePaths.size()) {
        return;
    }

    const QString path = imagePaths.at(index);
#ifdef Q_OS_MACOS
    removeQuarantine(path);
#endif

    // Check the file type
    const QString suffix = "." + QFileInfo(path).suffix();
    const bool isPdf = isPdfFile(suffix);
    const bool isGif = QFileInfo(path).suffix().compare("gif", Qt::CaseInsensitive) == 0;

    if (isPdf) {
        m_pendingDisplayPath.clear();
        if (!m_imageView->loadPdf(path)) {
            m_currentIndex = -1;
            m_statusLabel->setText(tr("Nepodařilo se načíst soubor: %1").arg(path));
            return;
        }
    } else if (isGif) {
        // Animovaný GIF přehráváme přes QMovie (async dekodér by vrátil jen
        // první snímek). Při neúspěchu spadneme zpět na statické zobrazení.
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
        // Asynchronní cesta: cache hit → okamžité zobrazení; miss → rozmazaný
        // placeholder z náhledu a dekódování na pozadí (UI neblokuje).
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

    // Disconnect old signal if present and connect new one for PDF
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
        });
        // Emitovat signal s aktuální stránkou (v případě že byl emitován dříve než se handler připojil)
        m_imageView->emitCurrentPdfPageInfo();
    }

    // V režimu Galerie přepnout z mřížky na zobrazení obrázku
    if (m_galleryGridActive) {
        m_centralStack->setCurrentWidget(m_imageView);
    }

    // Přednačíst sousedy — při sekvenčním listování je další obrázek
    // už dekódovaný v cache, než uživatel stiskne šipku.
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

    // Detekovat směr listování (vpřed = +1, vzad = -1)
    int direction = 0;
    if (m_lastPrefetchIndex >= 0 && m_lastPrefetchIndex != m_currentIndex) {
        direction = (m_currentIndex - m_lastPrefetchIndex > 0) ? 1 : -1;
    }
    m_lastPrefetchIndex = m_currentIndex;

    QStringList neighbors;
    const int size = m_imagePaths.size();

    // Prefetch má smysl jen pro staticky dekódované obrázky. PDF jde přes PDF
    // render a GIF přes QMovie — ani jeden nečte z ImageLoader cache, takže by
    // jejich přednačítání jen zbytečně plnilo RAM cache.
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
    // Zobrazujeme jen výsledek, na který právě čekáme; prefetch a opožděné
    // výsledky se jen uložily do cache. (Slot běží v GUI vlákně — bez race.)
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
        m_statusLabel->setText(
            tr("%1   |   %2   |   %3   |   %4 kB   |   %5 / %6")
                .arg(info.path.section('/', -1))
                .arg(info.dimensionsString())
                .arg(info.format)
                .arg(QString::number(info.fileSizeKb(), 'f', 1))
                .arg(m_currentIndex + 1)
                .arg(m_imagePaths.size())
        );
    } catch (...) {
        m_statusLabel->setText(path.section('/', -1));
        if (m_metadataPanel != nullptr) {
            m_metadataPanel->clearMetadata();
        }
    }
}

void MainWindow::setupDock()
{
    auto *dock = new QDockWidget(tr("Náhledy"), this);
    dock->setWidget(m_thumbnailPanel);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    m_thumbnailDock = dock;
    m_togglePanelAction = dock->toggleViewAction();
    m_togglePanelAction->setText(tr("Panel náhledů"));

    // Panel metadat pro rozložení "Pro" — vytvořen předem, viditelný jen v Pro
    m_metadataPanel = new MetadataPanel(this);
    m_metadataDock = new QDockWidget(tr("Informace"), this);
    m_metadataDock->setWidget(m_metadataPanel);
    m_metadataDock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_metadataDock);
    m_metadataDock->hide();
}

void MainWindow::setupMenu()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Soubor"));
    m_openFolderAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    m_openFileAction->setShortcut(QKeySequence::Open);
    connect(m_openFolderAction, &QAction::triggered, this, &MainWindow::openFolderDialog);
    connect(m_openFileAction, &QAction::triggered, this, &MainWindow::openFileDialog);
    fileMenu->addAction(m_openFolderAction);
    fileMenu->addAction(m_openFileAction);
    fileMenu->addSeparator();

    // ── Oblíbené složky ──────────────────────────────────────────────────────
    QMenu *favoritesMenu = fileMenu->addMenu(tr("⭐ Oblíbené"));
    QAction *addFavAction = favoritesMenu->addAction(tr("[+ Přidat aktuální]"));
    connect(addFavAction, &QAction::triggered, this, [this] {
        if (!m_currentFolder.isEmpty() && m_settingsManager->favoriteFolders().size() < 10) {
            m_settingsManager->addFavoriteFolder(m_currentFolder);
            updateFavoritesMenu();
        }
    });
    favoritesMenu->addSeparator();
    updateFavoritesMenu();

    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Konec"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&Zobrazení"));
    connect(m_fitToWindowAction, &QAction::triggered, m_imageView, &ImageView::fitToWindow);
    connect(m_resetZoomAction, &QAction::triggered, m_imageView, &ImageView::resetZoom);
    connect(m_fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    viewMenu->addAction(m_fitToWindowAction);
    viewMenu->addAction(m_fullscreenAction);
    viewMenu->addAction(m_resetZoomAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_togglePanelAction);

    QMenu *settingsMenu = menuBar()->addMenu(tr("&Nastavení"));

    // ── Vzhled aplikace ───────────────────────────────────────────────────────
    QMenu *layoutMenu = settingsMenu->addMenu(tr("Vzhled aplikace"));
    m_layoutActionGroup = new QActionGroup(this);
    m_layoutActionGroup->setExclusive(true);

    const struct { UiLayout layout; QString label; } layouts[] = {
        { UiLayout::Classic,   tr("Klasický") },
        { UiLayout::Filmstrip, tr("Filmový pás") },
        { UiLayout::Immersive, tr("Imerzivní") },
        { UiLayout::Gallery,   tr("Galerie") },
        { UiLayout::Pro,       tr("Pro režim") },
    };
    const UiLayout savedLayout = uiLayoutFromString(m_settingsManager->uiLayout());
    for (const auto &entry : layouts) {
        QAction *action = layoutMenu->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(entry.layout == savedLayout);
        m_layoutActionGroup->addAction(action);
        const UiLayout layout = entry.layout;
        connect(action, &QAction::triggered, this, [this, layout] {
            applyUiLayout(layout);
            m_settingsManager->setUiLayout(uiLayoutToString(layout));
        });
    }

    // ── Řazení souborů ────────────────────────────────────────────────────────
    QMenu *sortMenu = settingsMenu->addMenu(tr("Řazení souborů"));

    auto *sortKeyGroup = new QActionGroup(this);
    sortKeyGroup->setExclusive(true);
    const struct { int key; QString label; } sortKeys[] = {
        { 0, tr("Podle názvu") },
        { 1, tr("Podle data změny") },
        { 2, tr("Podle velikosti") },
    };
    const int savedSortKey = m_settingsManager->sortKey();
    for (const auto &entry : sortKeys) {
        QAction *action = sortMenu->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(entry.key == savedSortKey);
        sortKeyGroup->addAction(action);
        const int key = entry.key;
        connect(action, &QAction::triggered, this, [this, key] {
            m_settingsManager->setSortKey(key);
            reloadCurrentFolder();
        });
    }

    sortMenu->addSeparator();

    auto *sortOrderGroup = new QActionGroup(this);
    sortOrderGroup->setExclusive(true);
    const bool ascending = m_settingsManager->sortAscending();
    const struct { bool asc; QString label; } sortOrders[] = {
        { true,  tr("Vzestupně") },
        { false, tr("Sestupně") },
    };
    for (const auto &entry : sortOrders) {
        QAction *action = sortMenu->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(entry.asc == ascending);
        sortOrderGroup->addAction(action);
        const bool asc = entry.asc;
        connect(action, &QAction::triggered, this, [this, asc] {
            m_settingsManager->setSortAscending(asc);
            reloadCurrentFolder();
        });
    }

    settingsMenu->addSeparator();

    m_rememberLastFolderAction->setCheckable(true);
    m_rememberLastFolderAction->setChecked(m_settingsManager->rememberLastFolder());
    connect(m_rememberLastFolderAction, &QAction::toggled, this, &MainWindow::onRememberLastFolderToggled);
    settingsMenu->addAction(m_rememberLastFolderAction);
    settingsMenu->addSeparator();

    m_enableDeleteImageAction->setCheckable(true);
    m_enableDeleteImageAction->setChecked(m_settingsManager->enableDeleteImage());
    connect(m_enableDeleteImageAction, &QAction::toggled, this, &MainWindow::onEnableDeleteImageToggled);
    settingsMenu->addAction(m_enableDeleteImageAction);

    m_enableMoveToDeleteAction->setCheckable(true);
    m_enableMoveToDeleteAction->setChecked(m_settingsManager->enableMoveToDelete());
    connect(m_enableMoveToDeleteAction, &QAction::toggled, this, &MainWindow::onEnableMoveToDeleteToggled);
    settingsMenu->addAction(m_enableMoveToDeleteAction);

    m_askConfirmationAction->setCheckable(true);
    m_askConfirmationAction->setChecked(m_settingsManager->askConfirmationDelete());
    connect(m_askConfirmationAction, &QAction::toggled, this, &MainWindow::onAskConfirmationToggled);
    settingsMenu->addAction(m_askConfirmationAction);

    settingsMenu->addSeparator();

    // ── Cache náhledů ─────────────────────────────────────────────────────────
    QMenu *cacheMenu = settingsMenu->addMenu(tr("Cache náhledů"));

    // Zobrazit velikost cache
    QAction *cacheSizeAction = cacheMenu->addAction(tr("Aktuální velikost cache: …"));
    cacheSizeAction->setEnabled(false);
    connect(cacheMenu, &QMenu::aboutToShow, this, [this, cacheSizeAction] {
        const QString cacheDir = m_settingsManager->effectiveThumbnailCacheDir();
        qint64 sizeBytes = ThumbnailCacheManager::calculateCacheSize(cacheDir);
        double sizeMB = sizeBytes / (1024.0 * 1024.0);
        cacheSizeAction->setText(tr("Aktuální velikost cache: %1 MB")
                                     .arg(QString::number(sizeMB, 'f', 1)));
    });

    cacheMenu->addSeparator();

    QAction *cacheEnabledAction = cacheMenu->addAction(tr("Používat diskovou cache"));
    cacheEnabledAction->setCheckable(true);
    cacheEnabledAction->setChecked(m_settingsManager->thumbnailCacheEnabled());
    connect(cacheEnabledAction, &QAction::toggled, this, [this](bool checked) {
        m_settingsManager->setThumbnailCacheEnabled(checked);
        m_thumbnailPanel->setDiskCache(checked,
                                       m_settingsManager->effectiveThumbnailCacheDir());
    });

    cacheMenu->addAction(tr("Vybrat složku pro cache…"), this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(
            this, tr("Vybrat složku pro cache miniatur"),
            m_settingsManager->thumbnailCacheRoot());
        if (folder.isEmpty()) {
            return;
        }
        m_settingsManager->setThumbnailCacheRoot(folder);
        m_thumbnailPanel->setDiskCache(m_settingsManager->thumbnailCacheEnabled(),
                                       m_settingsManager->effectiveThumbnailCacheDir());
        m_statusLabel->setText(tr("Cache miniatur: %1")
                                   .arg(m_settingsManager->effectiveThumbnailCacheDir()));
    });

    cacheMenu->addAction(tr("Vymazat cache…"), this, [this] {
        const QString cacheDir = m_settingsManager->effectiveThumbnailCacheDir();
        QDir dir(cacheDir);
        if (!dir.exists()) {
            QMessageBox::information(this, QString(),
                                     tr("Cache je prázdná, není co mazat."));
            return;
        }

        // Spočítat velikost a počet souborů
        qint64 totalBytes = ThumbnailCacheManager::calculateCacheSize(cacheDir);
        int fileCount = 0;
        QDirIterator it(cacheDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            ++fileCount;
        }

        const int result = QMessageBox::question(
            this,
            tr("Vymazat cache miniatur"),
            tr("Smazat %1 souborů (%2 MB) ze složky\n%3?")
                .arg(fileCount)
                .arg(QString::number(totalBytes / (1024.0 * 1024.0), 'f', 1))
                .arg(cacheDir),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) {
            return;
        }

        if (dir.removeRecursively()) {
            m_statusLabel->setText(tr("Cache miniatur byla vymazána."));
        } else {
            m_statusLabel->setText(tr("Cache se nepodařilo úplně vymazat."));
        }
    });

    settingsMenu->addSeparator();

    m_enablePdfProcessingAction->setCheckable(true);
    m_enablePdfProcessingAction->setChecked(m_settingsManager->enablePdfProcessing());
    connect(m_enablePdfProcessingAction, &QAction::toggled, this, &MainWindow::onEnablePdfProcessingToggled);
    settingsMenu->addAction(m_enablePdfProcessingAction);

    updateConfirmationActionState();

    // ── Nápověda ──────────────────────────────────────────────────────────────
    // POZOR: Při přidání nových funkcí nebo zkratek aktualizuj HelpDialog.cpp!
    QMenu *helpMenu = menuBar()->addMenu(tr("&Nápověda"));
    helpMenu->addAction(tr("O programu"),            this, [this] { HelpDialog::showAbout(this);     });
    helpMenu->addSeparator();
    helpMenu->addAction(tr("Podporované formáty"),   this, [this] { HelpDialog::showFormats(this);   });
    helpMenu->addAction(tr("Klávesové zkratky"),     this, [this] { HelpDialog::showShortcuts(this); });
    helpMenu->addSeparator();
    helpMenu->addAction(tr("Co je nového"),          this, [this] { HelpDialog::showWhatsNew(this);  });
}

void MainWindow::toggleFullscreen()
{
    if (m_isFullscreen) {
        exitFullscreen();
    } else {
        enterFullscreen();
    }
}

void MainWindow::setupToolbar()
{
    auto *toolbar = addToolBar(tr("Navigace"));
    toolbar->setMovable(false);
    m_mainToolbar = toolbar;

    m_previousImageAction->setShortcut(QKeySequence(Qt::Key_Left));
    m_nextImageAction->setShortcut(QKeySequence(Qt::Key_Right));
    m_toggleSlideshowAction->setShortcut(QKeySequence("S"));
    m_intervalSpinBox->setRange(1, 60);
    m_intervalSpinBox->setValue(m_slideshowController->intervalMs() / 1000);
    m_intervalSpinBox->setSuffix(tr(" s"));

    connect(m_previousImageAction, &QAction::triggered, this, &MainWindow::showPreviousImage);
    connect(m_nextImageAction, &QAction::triggered, this, &MainWindow::showNextImage);
    connect(m_toggleSlideshowAction, &QAction::triggered, this, &MainWindow::toggleSlideshow);
    connect(m_intervalSpinBox, &QSpinBox::valueChanged, this, [this](int seconds) {
        m_slideshowController->setInterval(seconds * 1000);
    });

    toolbar->addAction(m_previousImageAction);
    toolbar->addAction(m_nextImageAction);
    toolbar->addSeparator();
    toolbar->addAction(m_toggleSlideshowAction);
    toolbar->addWidget(m_intervalSpinBox);
    toolbar->addSeparator();
    // Otočení obrázku — zkratky fungují v celém okně (na rozdíl od keyPressEvent,
    // který by klávesy nedostal, když má focus panel náhledů). Doleva: '['/'L',
    // doprava: ']'. ('R' je obsazené Přejmenováním.)
    m_rotateLeftAction = new QAction(QStringLiteral("⟲"), this);
    m_rotateLeftAction->setToolTip(tr("Otočit doleva ([ nebo L)"));
    m_rotateLeftAction->setShortcuts({QKeySequence(Qt::Key_BracketLeft), QKeySequence(Qt::Key_L)});
    connect(m_rotateLeftAction, &QAction::triggered, m_imageView, &ImageView::rotateLeft);

    m_rotateRightAction = new QAction(QStringLiteral("⟳"), this);
    m_rotateRightAction->setToolTip(tr("Otočit doprava (])"));
    m_rotateRightAction->setShortcut(QKeySequence(Qt::Key_BracketRight));
    connect(m_rotateRightAction, &QAction::triggered, m_imageView, &ImageView::rotateRight);

    toolbar->addAction(m_renameImageAction);
    toolbar->addSeparator();
    toolbar->addAction(m_rotateLeftAction);
    toolbar->addAction(m_rotateRightAction);
    toolbar->addSeparator();
    toolbar->addAction(m_deletePictureAction);
    toolbar->addAction(m_deleteFolderAction);
}

void MainWindow::setupStatusBar()
{
    statusBar()->addWidget(m_statusLabel);
    m_statusLabel->setText(tr("Vyber složku s obrázky."));

    // Indikátor zoomu vpravo; skrytý dokud není zobrazen obrázek.
    m_zoomLabel = new QLabel(this);
    m_zoomLabel->hide();
    statusBar()->addPermanentWidget(m_zoomLabel);
    connect(m_imageView, &ImageView::zoomChanged, this, [this](double percent) {
        if (percent <= 0.0) {
            m_zoomLabel->hide();
        } else {
            m_zoomLabel->setText(tr("Zoom: %1 %").arg(qRound(percent)));
            m_zoomLabel->show();
        }
    });
}

// ── Přepínání rozložení UI ───────────────────────────────────────────────────

void MainWindow::applyUiLayout(UiLayout layout)
{
    // Nejdřív opustit režim Galerie, pokud je aktivní — panel se vrací do docku
    if (m_galleryGridActive) {
        leaveGalleryGrid();
    }

    m_uiLayout = layout;
    m_overlayToolbar->hide();

    switch (layout) {
    case UiLayout::Classic:
        m_thumbnailPanel->setDisplayMode(ThumbnailPanel::DisplayMode::Vertical);
        addDockWidget(Qt::LeftDockWidgetArea, m_thumbnailDock);
        m_thumbnailDock->show();
        m_metadataDock->hide();
        m_mainToolbar->show();
        statusBar()->show();
        break;

    case UiLayout::Filmstrip:
        m_thumbnailPanel->setDisplayMode(ThumbnailPanel::DisplayMode::Horizontal);
        addDockWidget(Qt::BottomDockWidgetArea, m_thumbnailDock);
        m_thumbnailDock->show();
        m_metadataDock->hide();
        m_mainToolbar->show();
        statusBar()->show();
        break;

    case UiLayout::Immersive:
        // Veškerý chrome skrytý; ovládání se objeví při pohybu myši.
        // Menu zůstává viditelné, aby šlo rozložení kdykoli přepnout zpět.
        m_thumbnailDock->hide();
        m_metadataDock->hide();
        m_mainToolbar->hide();
        statusBar()->hide();
        break;

    case UiLayout::Gallery:
        m_metadataDock->hide();
        m_mainToolbar->show();
        statusBar()->show();
        enterGalleryGrid();
        break;

    case UiLayout::Pro:
        m_thumbnailPanel->setDisplayMode(ThumbnailPanel::DisplayMode::Horizontal);
        addDockWidget(Qt::BottomDockWidgetArea, m_thumbnailDock);
        m_thumbnailDock->show();
        m_metadataDock->show();
        m_mainToolbar->show();
        statusBar()->show();
        break;
    }
}

void MainWindow::enterGalleryGrid()
{
    if (m_galleryGridActive) {
        return;
    }
    m_thumbnailDock->hide();
    m_thumbnailDock->setWidget(nullptr);
    m_thumbnailPanel->setDisplayMode(ThumbnailPanel::DisplayMode::Grid);
    m_centralStack->addWidget(m_thumbnailPanel);
    m_centralStack->setCurrentWidget(m_thumbnailPanel);
    m_galleryGridActive = true;
}

void MainWindow::leaveGalleryGrid()
{
    if (!m_galleryGridActive) {
        return;
    }
    // Restore dock ownership BEFORE removing from stack to avoid orphaning the widget
    m_thumbnailDock->setWidget(m_thumbnailPanel);
    m_centralStack->removeWidget(m_thumbnailPanel);
    m_centralStack->setCurrentWidget(m_imageView);
    m_galleryGridActive = false;
}

void MainWindow::showGalleryGrid()
{
    if (m_galleryGridActive) {
        m_centralStack->setCurrentWidget(m_thumbnailPanel);
    }
}

// ── Plovoucí ovládání (imerzivní režim) ──────────────────────────────────────

void MainWindow::setupOverlayToolbar()
{
    m_overlayToolbar = new QWidget(this);
    m_overlayToolbar->setObjectName("overlayToolbar");
    m_overlayToolbar->setStyleSheet(
        "#overlayToolbar { background-color: rgba(30, 30, 32, 220); border-radius: 22px; }"
        "QToolButton { color: #e8e6df; background: transparent; border: none;"
        "              font-size: 16px; padding: 4px 8px; }"
        "QToolButton:hover { color: #ffffff; background-color: rgba(255, 255, 255, 30);"
        "                    border-radius: 8px; }"
    );

    auto *layout = new QHBoxLayout(m_overlayToolbar);
    layout->setContentsMargins(16, 6, 16, 6);
    layout->setSpacing(6);

    const auto addButton = [this, layout](const QString &glyph, const QString &tooltip, auto slot) {
        auto *button = new QToolButton(m_overlayToolbar);
        button->setText(glyph);
        button->setToolTip(tooltip);
        connect(button, &QToolButton::clicked, this, slot);
        layout->addWidget(button);
    };

    addButton(QStringLiteral("◀"), tr("Předchozí"), &MainWindow::showPreviousImage);
    addButton(QStringLiteral("⏯"), tr("Slideshow"), &MainWindow::toggleSlideshow);
    addButton(QStringLiteral("▶"), tr("Další"), &MainWindow::showNextImage);
    addButton(QStringLiteral("⛶"), tr("Celá obrazovka"), &MainWindow::toggleFullscreen);
    addButton(QStringLiteral("✎"), tr("Přejmenovat"), &MainWindow::renameCurrentImage);
    addButton(QStringLiteral("🗑"), tr("Smazat"), &MainWindow::deleteOrMoveCurrentImage);

    m_overlayToolbar->hide();

    m_overlayHideTimer = new QTimer(this);
    m_overlayHideTimer->setSingleShot(true);
    m_overlayHideTimer->setInterval(2500);
    connect(m_overlayHideTimer, &QTimer::timeout, m_overlayToolbar, &QWidget::hide);
}

void MainWindow::positionOverlayToolbar()
{
    m_overlayToolbar->adjustSize();
    const int x = (width() - m_overlayToolbar->width()) / 2;
    const int y = height() - m_overlayToolbar->height() - 24;
    m_overlayToolbar->move(x, y);
}

void MainWindow::showOverlayToolbar()
{
    positionOverlayToolbar();
    m_overlayToolbar->show();
    m_overlayToolbar->raise();
    m_overlayHideTimer->start();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_overlayToolbar != nullptr && m_overlayToolbar->isVisible()) {
        positionOverlayToolbar();
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (m_uiLayout == UiLayout::Immersive
        && !m_vlcActive
        && watched == m_imageView->viewport()
        && event->type() == QEvent::MouseMove) {
        showOverlayToolbar();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onEnableDeleteImageToggled(bool checked)
{
    if (checked && m_enableMoveToDeleteAction->isChecked()) {
        m_enableMoveToDeleteAction->blockSignals(true);
        m_enableMoveToDeleteAction->setChecked(false);
        m_enableMoveToDeleteAction->blockSignals(false);
        m_settingsManager->setEnableMoveToDelete(false);
    }
    m_settingsManager->setEnableDeleteImage(checked);
    updateConfirmationActionState();
}

void MainWindow::onEnableMoveToDeleteToggled(bool checked)
{
    if (checked && m_enableDeleteImageAction->isChecked()) {
        m_enableDeleteImageAction->blockSignals(true);
        m_enableDeleteImageAction->setChecked(false);
        m_enableDeleteImageAction->blockSignals(false);
        m_settingsManager->setEnableDeleteImage(false);
    }
    m_settingsManager->setEnableMoveToDelete(checked);
    updateConfirmationActionState();
}

void MainWindow::onAskConfirmationToggled(bool checked)
{
    m_settingsManager->setAskConfirmationDelete(checked);
}

void MainWindow::onEnablePdfProcessingToggled(bool checked)
{
    m_settingsManager->setEnablePdfProcessing(checked);

    // Reload current folder to show/hide PDF files based on the new setting
    if (!m_imagePaths.isEmpty()) {
        const QString currentFolder = m_imagePaths.first().section('/', 0, -2);
        loadFolder(currentFolder);
    }
}

void MainWindow::updateConfirmationActionState()
{
    bool anyEnabled = m_enableDeleteImageAction->isChecked() || m_enableMoveToDeleteAction->isChecked();
    m_askConfirmationAction->setEnabled(anyEnabled);
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

    bool shouldAskConfirmation = m_settingsManager->askConfirmationDelete();
    if (shouldAskConfirmation) {
        if (!showDeleteConfirmationDialog()) {
            return;  // User cancelled
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
    if (QFile::moveToTrash(currentPath)) {
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
    const QString folderPath = currentPath.section('/', 0, -2);
    const QString deleteFolderPath = folderPath + "/Delete";

    QDir deleteFolder(deleteFolderPath);
    if (!deleteFolder.exists()) {
        if (!QDir(folderPath).mkdir("Delete")) {
            m_statusLabel->setText(tr("Nepodařilo se vytvořit složku Delete."));
            return;
        }
    }

    QFileInfo fileInfo(currentPath);
    const QString newPath = deleteFolderPath + "/" + fileInfo.fileName();

    if (QFile::rename(currentPath, newPath)) {
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

    const QString fileName = fileInfo.fileName();
    const QString baseName = fileInfo.baseName();  // filename without extension
    const QString suffix = fileInfo.suffix();      // extension

    // Create rename dialog
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
        return;  // User cancelled or didn't change the name
    }

    // Construct the new path
    const QString folderPath = fileInfo.absolutePath();
    const QString newFileName = newBaseName + "." + suffix;
    const QString newPath = folderPath + "/" + newFileName;

    // Check if file with new name already exists
    if (QFile::exists(newPath)) {
        QMessageBox::warning(this, tr("Chyba"), tr("Soubor '%1' již existuje.").arg(newFileName));
        return;
    }

    // Rename the file
    if (QFile::rename(currentPath, newPath)) {
        m_imagePaths[m_currentIndex] = newPath;
        m_thumbnailPanel->updateImagePath(currentPath, newPath);
        updateStatus(newPath);
        m_statusLabel->setText(tr("Obrázek přejmenován na '%1'.").arg(newFileName));
    } else {
        m_statusLabel->setText(tr("Nepodařilo se přejmenovat obrázek."));
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
        } else {
            m_statusLabel->setText(tr("Nepodařilo se smazat složku Delete."));
        }
    }
}

bool MainWindow::showDeleteConfirmationDialog()
{
    QString title;
    QString message;

    if (m_settingsManager->enableDeleteImage()) {
        title = tr("Smazat obrázek");
        message = tr("Opravdu chceš smazat tento obrázek?");
    } else if (m_settingsManager->enableMoveToDelete()) {
        title = tr("Přesunout obrázek");
        message = tr("Opravdu chceš přesunout tento obrázek do Delete?");
    } else {
        return false;
    }

    int result = QMessageBox::question(
        this,
        title,
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    return result == QMessageBox::Yes;
}

void MainWindow::removeImageFromList(int index)
{
    if (index < 0 || index >= m_imagePaths.size()) {
        return;
    }

    m_imagePaths.removeAt(index);
    m_thumbnailPanel->removeImage(index);

    if (m_imagePaths.isEmpty()) {
        m_currentIndex = -1;
        m_imageView->clearImage();
        m_statusLabel->setText(tr("Ve složce nebyly nalezeny žádné obrázky."));
        return;
    }

    // Show next image or previous if we deleted the last one
    int nextIndex = index;
    if (nextIndex >= m_imagePaths.size()) {
        nextIndex = m_imagePaths.size() - 1;
    }
    showImage(nextIndex);
}

// ── VLC Integration ─────────────────────────────────────────────────────────

void MainWindow::onPlayVideo()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    const QString imagePath = m_imagePaths.at(m_currentIndex);
    QString videoPath;

    // Find matching video file
    if (!VlcController::findVideoFile(imagePath, videoPath)) {
        m_statusLabel->setText(tr("Video se stejným názvem neexistuje."));
        return;
    }

    // Initialize VLC
    QString errorMsg;
    if (!m_vlcController->initialize(videoPath, errorMsg)) {
        QMessageBox::critical(this, tr("Chyba VLC"), errorMsg);
        m_statusLabel->setText(tr("Nepodařilo se spustit VLC."));
        return;
    }

    // UI will be updated in onVlcStatusChanged
}

void MainWindow::onVlcStatusChanged(int vlcState)
{
    const auto state = static_cast<VlcState>(vlcState);

    switch (state) {
    case VlcState::Running:
        m_vlcActive = true;
        disableImageBrowsing();
        applyGrayscaleEffect(true);
        updateVideoMetadata(m_imagePaths.at(m_currentIndex));
        if (!m_vlcKeyPollTimer) {
            m_vlcKeyPollTimer = new QTimer(this);
            connect(m_vlcKeyPollTimer, &QTimer::timeout, this, &MainWindow::pollVlcKeys);
        }
        m_vlcKeyPollTimer->start(80);
        break;

    case VlcState::Stopped:
    case VlcState::Error:
        m_vlcActive = false;
        if (m_vlcKeyPollTimer)
            m_vlcKeyPollTimer->stop();
        enableImageBrowsing();
        applyGrayscaleEffect(false);
        m_statusLabel->setText(tr("Vyber složku s obrázky."));
        break;

    default:
        break;
    }
}

void MainWindow::onVlcConnectionLost()
{
    QMessageBox::warning(this, tr("Chyba"), tr("Spojení s VLC bylo ztraceno."));
    m_vlcActive = false;
    enableImageBrowsing();
    applyGrayscaleEffect(false);
}

void MainWindow::onVlcProcessCrashed()
{
    const QString logPath = m_vlcController->lastLogPath();
    QString msg = tr("VLC se nečekaně ukončil.");
    if (!logPath.isEmpty())
        msg += tr("\n\nDiagnostický log:\n%1").arg(logPath);
    QMessageBox::critical(this, tr("Chyba VLC"), msg);
    m_vlcActive = false;
    enableImageBrowsing();
    applyGrayscaleEffect(false);
}

void MainWindow::pollVlcKeys()
{
#ifdef Q_OS_WIN
    // Edge-detection: only fire on key-down transition, not while held
    auto pressed = [](int vk) { return (GetAsyncKeyState(vk) & 0x8001) == 0x8001; };

    if (pressed(VK_ESCAPE)) {
        m_vlcController->stop();
        return;
    }
    if (pressed(VK_SPACE))
        m_vlcController->sendCommand("pause");
    if (pressed(VK_LEFT))
        m_vlcController->sendCommand("seek -10");
    if (pressed(VK_RIGHT))
        m_vlcController->sendCommand("seek +10");
    if (pressed(VK_ADD) || pressed(0xBB))   // numpad + or regular +
        m_vlcController->sendCommand("volup");
    if (pressed(VK_SUBTRACT) || pressed(0xBD))  // numpad - or regular -
        m_vlcController->sendCommand("voldown");
#endif
}

void MainWindow::disableImageBrowsing()
{
    m_openFolderAction->setEnabled(false);
    m_openFileAction->setEnabled(false);
    m_previousImageAction->setEnabled(false);
    m_nextImageAction->setEnabled(false);
    m_toggleSlideshowAction->setEnabled(false);
    m_fitToWindowAction->setEnabled(false);
    m_resetZoomAction->setEnabled(false);
    m_fullscreenAction->setEnabled(false);
    m_enableDeleteImageAction->setEnabled(false);
    m_enableMoveToDeleteAction->setEnabled(false);
    m_deletePictureAction->setEnabled(false);
    m_deleteFolderAction->setEnabled(false);

    m_slideshowController->stop();

    if (m_thumbnailDock) {
        m_thumbnailDock->setEnabled(false);
    }
}

void MainWindow::enableImageBrowsing()
{
    m_openFolderAction->setEnabled(true);
    m_openFileAction->setEnabled(true);
    m_previousImageAction->setEnabled(!m_imagePaths.isEmpty());
    m_nextImageAction->setEnabled(!m_imagePaths.isEmpty());
    m_toggleSlideshowAction->setEnabled(!m_imagePaths.isEmpty());
    m_fitToWindowAction->setEnabled(!m_imagePaths.isEmpty());
    m_resetZoomAction->setEnabled(!m_imagePaths.isEmpty());
    m_fullscreenAction->setEnabled(!m_imagePaths.isEmpty());
    m_enableDeleteImageAction->setEnabled(true);
    m_enableMoveToDeleteAction->setEnabled(true);
    m_deletePictureAction->setEnabled(!m_imagePaths.isEmpty());
    m_deleteFolderAction->setEnabled(!m_imagePaths.isEmpty());

    if (m_thumbnailDock) {
        m_thumbnailDock->setEnabled(true);
    }

    updateConfirmationActionState();
}

void MainWindow::applyGrayscaleEffect(bool enable)
{
    // QGraphicsColorizeEffect conflicts with VLC D3D11VA hardware decoding
    // (causes "QWidget::paintEngine: Should no longer be called" crash).
    // Use window opacity instead — safe with any renderer.
    if (enable) {
        setWindowOpacity(0.6);
    } else {
        setWindowOpacity(1.0);
    }
}

void MainWindow::updateVideoMetadata(const QString &videoPath)
{
    const QFileInfo fileInfo(videoPath);
    const QString fileName = fileInfo.fileName();
    const qint64 fileSize = fileInfo.size();

    // Format file size
    QString sizeStr;
    if (fileSize > 1024 * 1024) {
        sizeStr = QString::number(fileSize / (1024.0 * 1024.0), 'f', 1) + " MB";
    } else {
        sizeStr = QString::number(fileSize / 1024.0, 'f', 1) + " KB";
    }

    const QString statusText = tr("▶ Video: %1 (%2) [ESC = exit]").arg(fileName, sizeStr);
    m_statusLabel->setText(statusText);
}

void MainWindow::updateFavoritesMenu()
{
    // Najít a aktualizovat Favorites menu — včetně tlačítek "+ Přidat" a "-" smazat
    QMenuBar *mb = menuBar();
    if (!mb) {
        return;
    }

    // Najít menu "⭐ Oblíbené" v "Soubor" menu
    QMenu *fileMenu = nullptr;
    for (QAction *action : mb->actions()) {
        if (action->text().contains("Soubor")) {
            fileMenu = action->menu();
            break;
        }
    }

    if (!fileMenu) {
        return;
    }

    QMenu *favMenu = nullptr;
    for (QAction *action : fileMenu->actions()) {
        if (action->text().contains("Oblíbené")) {
            favMenu = action->menu();
            break;
        }
    }

    if (!favMenu) {
        return;
    }

    // Vyčistit menu (zachovat [+ Přidat] a separator na začátku)
    int sepIdx = -1;
    for (int i = 0; i < favMenu->actions().size(); ++i) {
        if (favMenu->actions()[i]->isSeparator()) {
            sepIdx = i;
            break;
        }
    }

    if (sepIdx >= 0) {
        // Smazat vše za separatorem
        while (favMenu->actions().size() > sepIdx + 1) {
            delete favMenu->actions().last();
        }
    }

    // Přidat seznam oblíbených
    const QStringList favorites = m_settingsManager->favoriteFolders();
    if (favorites.isEmpty()) {
        favMenu->addAction(tr("(prázdné)"))->setEnabled(false);
        return;
    }

    for (const QString &path : favorites) {
        QString displayName = QFileInfo(path).fileName();
        if (displayName.isEmpty()) {
            displayName = path;
        }

        QAction *folderAction = favMenu->addAction(displayName);
        folderAction->setToolTip(path);

        connect(folderAction, &QAction::triggered, this, [this, path] {
            loadFolder(path);
        });

        // Přidat malé "−" tlačítko pro odebrání (s ikonou nebo textem)
        // Poznámka: Qt menu nepodporuje custom UI vedle action textu jednoduše,
        // takže přidáme separátor a "-" akci pod ní. Nebo jednoduše přidáme
        // do action tooltipu instrukci "shift+click = odebrat"

        // Jednodušeji: spojit oba v jednu akci s pravým klikem, nebo
        // přidávat "-" položky vedle. Pojďme jednodušší přístup:
        // pravý klik na položku = smazat, nebo lepší: přidat odkaz vedle
        // Zatím pro jednoduchost: přidáme "- <název>" akce hned pod
    }

    // Alternativně: mít "-" items vedle. Ale to je komplexní. Zatím necháme
    // bez GUI na smazání z menu. Bude jen otevírání z menu, a mazání z UI.
    // (Později lze přidat pravý-klik handler.)
}

} // namespace pictureviewer

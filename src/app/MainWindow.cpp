#include "app/HelpDialog.hpp"
#include "app/ImageView.hpp"
#include "app/MainWindow.hpp"
#include "app/SettingsManager.hpp"
#include "app/SlideshowController.hpp"
#include "app/ThumbnailPanel.hpp"
#include "app/VlcController.hpp"
#include "workers/FolderScanWorker.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
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
#include <QFileDialog>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
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

} // anonymous namespace

namespace pictureviewer {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_folderScanWorker(nullptr)
    , m_imageView(new ImageView(this))
    , m_settingsManager(new SettingsManager())
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
    , m_deleteFolderAction(new QAction(this))
    , m_deletePictureAction(new QAction(this))
    , m_vlcController(new VlcController(m_settingsManager, this))
    , m_grayscaleEffect(nullptr)
{
    m_deleteFolderAction->setIcon(QIcon(":/icons/delete_folder_icon.ico"));
    m_deleteFolderAction->setToolTip(tr("Smazání složky Delete"));
    connect(m_deleteFolderAction, &QAction::triggered, this, &MainWindow::onDeleteFolder);

    m_deletePictureAction->setIcon(QIcon(":/icons/delete_picture_icon.ico"));
    m_deletePictureAction->setToolTip(tr("Smazání obrázku"));
    connect(m_deletePictureAction, &QAction::triggered, this, &MainWindow::deleteOrMoveCurrentImage);

    // Connect VLC signals
    connect(m_vlcController, QOverload<int>::of(&VlcController::statusChanged),
            this, &MainWindow::onVlcStatusChanged);
    connect(m_vlcController, &VlcController::connectionLost,
            this, &MainWindow::onVlcConnectionLost);
    connect(m_vlcController, &VlcController::processCrashed,
            this, &MainWindow::onVlcProcessCrashed);

    setWindowTitle("PictureViewer v." + QCoreApplication::applicationVersion());
    resize(1200, 750);
    setWindowIcon(QIcon(":/icons/eye_icon.ico"));
    setCentralWidget(m_imageView);
    connect(m_thumbnailPanel, &ThumbnailPanel::imageSelected, this, &MainWindow::showImage);
    connect(m_slideshowController, &SlideshowController::nextImageRequested, this, &MainWindow::showNextImage);
    setupDock();
    setupMenu();
    setupToolbar();
    setupStatusBar();

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

void MainWindow::loadFolder(const QString &folderPath)
{
    if (m_shuttingDown) {
        return;
    }

    ++m_scanGeneration;
    m_statusLabel->setText(tr("Načítám složku…"));

    if (m_folderScanWorker != nullptr) {
        m_folderScanWorker->cancel();
        m_folderScanWorker = nullptr;
    }

    // Parent must be nullptr — memory is managed by the deleteLater connection
    // below. A Qt parent would create a second deletion path → double-free.
    auto *worker = new FolderScanWorker(folderPath, m_scanGeneration, nullptr);
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
    for (QToolBar *toolbar : findChildren<QToolBar *>()) {
        toolbar->show();
    }
    if (m_thumbnailDock != nullptr && m_thumbnailDockWasVisible) {
        m_thumbnailDock->show();
    }
    statusBar()->show();
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
    if (index < 0 || index >= m_imagePaths.size()) {
        return;
    }

    const QString path = m_imagePaths.at(index);
#ifdef Q_OS_MACOS
    removeQuarantine(path);
#endif
    if (!m_imageView->loadImage(path)) {
        m_currentIndex = -1;
        m_statusLabel->setText(tr("Nepodařilo se načíst obrázek: %1").arg(path));
        return;
    }

    m_currentIndex = index;
    m_thumbnailPanel->setCurrentIndex(index);
    updateStatus(path);
}

void MainWindow::updateStatus(const QString &path)
{
    try {
        const ImageInfo info = m_imageMetadataReader.read(path);
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
    toolbar->addAction(m_deletePictureAction);
    toolbar->addAction(m_deleteFolderAction);
}

void MainWindow::setupStatusBar()
{
    statusBar()->addWidget(m_statusLabel);
    m_statusLabel->setText(tr("Vyber složku s obrázky."));
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
        break;

    case VlcState::Stopped:
    case VlcState::Error:
        m_vlcActive = false;
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
    QMessageBox::critical(this, tr("Chyba"), tr("VLC se nečekaně ukončil."));
    m_vlcActive = false;
    enableImageBrowsing();
    applyGrayscaleEffect(false);
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
    if (enable) {
        if (!m_grayscaleEffect) {
            m_grayscaleEffect = new QGraphicsColorizeEffect(this);
            m_grayscaleEffect->setColor(Qt::gray);
            m_grayscaleEffect->setStrength(1.0);  // Full grayscale
        }
        m_imageView->setGraphicsEffect(m_grayscaleEffect);
    } else {
        m_imageView->setGraphicsEffect(nullptr);
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

} // namespace pictureviewer

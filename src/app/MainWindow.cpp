#include "app/CategoryDialogs.hpp"
#include "app/CategoryManager.hpp"
#include "app/HelpDialog.hpp"
#include "app/ImageLoader.hpp"
#include "app/ImageView.hpp"
#include "app/MainWindow.hpp"
#include "app/MetadataPanel.hpp"
#include "app/ScreenCapture.hpp"
#include "app/SettingsManager.hpp"
#include "app/SlideshowController.hpp"
#include "app/ThumbnailCacheManager.hpp"
#include "app/ThumbnailPanel.hpp"
#include "app/VideoPlayer.hpp"
#include "core/ImageFormats.hpp"
#include "workers/FolderScanWorker.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsColorizeEffect>
#include <QMessageBox>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QActionGroup>
#include <QClipboard>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QDesktopServices>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QBuffer>
#include <QMimeData>
#include <QProcess>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QColorDialog>
#include <QLineEdit>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QPushButton>
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
    , m_settingsManager(createProfileAndSettings())
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
    // Inicializovat CategoryManager — databáze v adresáři aktivního profilu.
    // (ProfileManager a SettingsManager jsou již vytvořeny v init-listu.)
    m_categoryManager = new CategoryManager(
        m_profileManager->dbPath(m_profileManager->activeProfile()));

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

    setWindowTitle("PictureViewer v." + QCoreApplication::applicationVersion());
    resize(1200, 750);   // výchozí velikost; přepsána níže pokud rozlišení odpovídá
    setWindowIcon(QIcon(":/icons/eye_icon.ico"));
    setAcceptDrops(true);   // přetažení složky/souboru do okna

    // Centrální stack:
    //   index 0 = ImageView (obrázky)
    //   index 1 = VideoPlayer (přehrávání videa inline)
    // V režimu Galerie se dočasně přidá ThumbnailPanel jako index 2.
    m_centralStack = new QStackedWidget(this);
    m_videoPlayer  = new VideoPlayer(m_settingsManager, this);
    m_centralStack->addWidget(m_imageView);
    m_centralStack->addWidget(m_videoPlayer);
    setCentralWidget(m_centralStack);

    connect(m_videoPlayer, &VideoPlayer::stopped,
            this, &MainWindow::onVideoStopped);
    connect(m_videoPlayer, &VideoPlayer::fullscreenToggleRequested,
            this, &MainWindow::toggleFullscreen);
    connect(m_videoPlayer, &VideoPlayer::videoMetadataReady,
            this, [this](const VideoMeta &meta) {
                const QFileInfo fi(meta.path);
                const double mb = meta.fileSizeBytes / (1024.0 * 1024.0);
                QString text = tr("Přehrávám: %1   |   %2 MB")
                    .arg(fi.fileName())
                    .arg(mb, 0, 'f', 1);
                if (meta.resolution.isValid())
                    text += tr("   |   %1×%2")
                        .arg(meta.resolution.width())
                        .arg(meta.resolution.height());
                if (meta.durationMs > 0) {
                    const int s = static_cast<int>(meta.durationMs / 1000);
                    text += tr("   |   %1:%2")
                        .arg(s / 60)
                        .arg(s % 60, 2, 10, QChar('0'));
                }
                if (meta.fileSizeBytes > 0 && meta.durationMs > 0) {
                    const double mbitps = (meta.fileSizeBytes * 8.0)
                                          / (meta.durationMs / 1000.0)
                                          / 1'000'000.0;
                    text += tr("   |   %1 Mbit/s").arg(mbitps, 0, 'f', 1);
                }
                if (!meta.videoCodec.isEmpty())
                    text += tr("   |   %1").arg(meta.videoCodec);
                if (meta.videoBitRate > 0) {
                    if (meta.videoBitRate >= 1'000'000)
                        text += tr("   |   %1 Mb/s").arg(meta.videoBitRate / 1'000'000.0, 0, 'f', 1);
                    else
                        text += tr("   |   %1 kb/s").arg(meta.videoBitRate / 1000);
                }
                if (meta.frameRate > 0.0)
                    text += tr("   |   %1 fps").arg(meta.frameRate, 0, 'f', 2);
                m_statusLabel->setText(text);
            });

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
    setupFavoritesToolbar();
    setupCategoriesToolbar();
    setupPdfToolbar();
    setupStatusBar();
    setupOverlayToolbar();

    // Sledování myši pro imerzivní režim (zobrazení plovoucího ovládání)
    m_imageView->viewport()->setMouseTracking(true);
    m_imageView->viewport()->installEventFilter(this);

    applyUiLayout(uiLayoutFromString(m_settingsManager->uiLayout()));

    // Obnovit velikost okna — ale jen pokud se rozlišení nezměnilo
    {
        const QByteArray savedGeom = m_settingsManager->windowGeometry();
        const QSize savedScreen    = m_settingsManager->savedScreenSize();
        const QSize currentScreen  = QGuiApplication::primaryScreen()->size();
        if (!savedGeom.isEmpty() && savedScreen == currentScreen) {
            restoreGeometry(savedGeom);
        }
    }
    {
        const QByteArray savedState = m_settingsManager->windowState();
        if (!savedState.isEmpty()) {
            restoreState(savedState);
        }
    }

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
    // Uložit stav toolbarů
    if (m_favoritesToolbar) {
        m_settingsManager->setFavoritesToolbarVisible(m_favoritesToolbar->isVisible());
    }
    if (m_categoriesToolbar) {
        m_settingsManager->setCategoriesToolbarVisible(m_categoriesToolbar->isVisible());
    }

    // Uložit geometrii okna + stav doků + aktuální rozlišení obrazovky
    m_settingsManager->setWindowGeometry(saveGeometry());
    m_settingsManager->setWindowState(saveState());
    m_settingsManager->setSavedScreenSize(QGuiApplication::primaryScreen()->size());

    m_settingsManager->syncToDisk();

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
    // Klávesy při přehrávání videa zpracovává VideoPlayer::keyPressEvent přímo
    // (má focus nastavený v playFile()). Sem se dostanou jen při prohlížení obrázků.
    if (m_centralStack->currentWidget() == m_videoPlayer) {
        event->ignore();
        return;
    }

    // ── Normal image browsing ────────────────────────────────────────────────
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_0:
        m_imageView->resetZoom();
        event->accept();
        return;
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

// ── Kontextové menu nad obrázkem ─────────────────────────────────────────────

void MainWindow::showImageContextMenu(const QPoint &globalPos)
{
    const bool hasFile = !m_imagePaths.isEmpty() && m_currentIndex >= 0
                         && m_currentIndex < m_imagePaths.size();
    const bool hasImage = !m_imageView->displayedImage().isNull();

    if (!hasFile && !hasImage) {
        return;
    }

    const QString currentPath = hasFile ? m_imagePaths.at(m_currentIndex) : QString();

    QMenu menu(this);

    if (hasFile) {
        QAction *revealAction = menu.addAction(tr("Zobrazit ve Finderu"));
        connect(revealAction, &QAction::triggered, this, [currentPath] {
#if defined(Q_OS_MACOS)
            QProcess::startDetached("open", {"-R", currentPath});
#elif defined(Q_OS_WIN)
            QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(currentPath)});
#else
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(currentPath).absolutePath()));
#endif
        });
    }

    QAction *copyImageAction = menu.addAction(tr("Kopírovat obrázek"));
    connect(copyImageAction, &QAction::triggered, this, [this] {
        const QImage image = m_imageView->displayedImage();
        if (image.isNull()) {
            return;
        }
        // Screenshot nebo výřez: uložit jako JPEG soubor a vložit URL do schránky
        if (m_isScreenshot || m_imageView->hasCrop()) {
            QString configDir = QFileInfo(SettingsManager::configFilePath()).absolutePath();
            QString tempPath  = configDir + "/picture.jpg";
            if (image.save(tempPath, "JPEG", 90)) {
                QMimeData *mimeData = new QMimeData();
                mimeData->setUrls({QUrl::fromLocalFile(tempPath)});
                QApplication::clipboard()->setMimeData(mimeData);
            }
        } else {
            QApplication::clipboard()->setImage(image);
        }
    });

    if (hasFile) {
        QAction *copyPathAction = menu.addAction(tr("Kopírovat cestu k souboru"));
        connect(copyPathAction, &QAction::triggered, this, [currentPath] {
            QApplication::clipboard()->setText(QDir::toNativeSeparators(currentPath));
        });
    }

    menu.exec(globalPos);
}

void MainWindow::onRememberLastFolderToggled(bool checked)
{
    m_settingsManager->setRememberLastFolder(checked);
    if (!checked) {
        m_settingsManager->clearLastFolder();
    }
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

void MainWindow::toggleFullscreen()
{
    if (m_isFullscreen) {
        exitFullscreen();
    } else {
        enterFullscreen();
    }
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

QString MainWindow::runSaveAsDialog(const QString &originalPath)
{
    const QString origDir  = QFileInfo(originalPath).absolutePath();
    const QString origBase = QFileInfo(originalPath).completeBaseName();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Uložit jako"));
    dlg.setMinimumWidth(460);

    auto *layout = new QVBoxLayout(&dlg);

    // Název souboru
    auto *nameLabel = new QLabel(tr("Název souboru:"), &dlg);
    layout->addWidget(nameLabel);
    auto *nameEdit = new QLineEdit(origBase, &dlg);
    layout->addWidget(nameEdit);

    layout->addSpacing(8);

    // Destinace
    auto *destLabel = new QLabel(tr("Uložit do:"), &dlg);
    layout->addWidget(destLabel);

    QString selectedDir;

    auto makeDestButton = [&](const QString &label, const QString &dir) {
        auto *btn = new QPushButton(label, &dlg);
        btn->setToolTip(dir);
        connect(btn, &QPushButton::clicked, &dlg, [&dlg, &selectedDir, dir]() {
            selectedDir = dir;
            dlg.accept();
        });
        layout->addWidget(btn);
    };

    makeDestButton(tr("Stejné umístění jako originál"), origDir);

    // Oblíbené složky
    const QStringList favorites = m_settingsManager->favoriteFolders();
    for (const QString &fav : favorites) {
        makeDestButton(QDir(fav).dirName(), fav);
    }

    layout->addSpacing(8);

    // Zrušit
    auto *btnCancel = new QPushButton(tr("Zrušit"), &dlg);
    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    layout->addWidget(btnCancel);

    if (dlg.exec() != QDialog::Accepted || selectedDir.isEmpty()) {
        return {};
    }

    // Sestavit cílovou cestu — odstranit případnou příponu ze vstupu
    QString baseName = nameEdit->text().trimmed();
    if (baseName.isEmpty()) {
        baseName = origBase;
    }
    // Odebrat příponu pokud ji uživatel zadal
    if (baseName.endsWith(QLatin1String(".jpg"), Qt::CaseInsensitive)
        || baseName.endsWith(QLatin1String(".jpeg"), Qt::CaseInsensitive)) {
        baseName = QFileInfo(baseName).completeBaseName();
    }

    QString targetPath = QDir(selectedDir).filePath(baseName + QStringLiteral(".jpg"));

    // Pokud cílový soubor existuje, zeptat se
    if (QFile::exists(targetPath)) {
        const int ret = QMessageBox::question(
            this, tr("Soubor existuje"),
            tr("Soubor '%1' již existuje.\nChcete ho přepsat?").arg(baseName + ".jpg"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) {
            return {};
        }
    }

    return targetPath;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (m_uiLayout == UiLayout::Immersive
        && m_centralStack->currentWidget() == m_imageView
        && watched == m_imageView->viewport()
        && event->type() == QEvent::MouseMove) {
        showOverlayToolbar();
    }

    return QMainWindow::eventFilter(watched, event);
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
    connect(addFavAction, &QAction::triggered, this, &MainWindow::onAddCurrentFolderToFavorites);
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

    // ── Profily ──────────────────────────────────────────────────────────────
    setupProfileMenu(menuBar()->addMenu(tr("&Profily")));

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

    // Řazení souborů je přesunuto do hlavního toolbaru (dropdown tlačítko).

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

} // namespace pictureviewer

#include "app/ImageView.hpp"
#include "app/MainWindow.hpp"
#include "app/SettingsManager.hpp"
#include "app/SlideshowController.hpp"
#include "app/ThumbnailPanel.hpp"
#include "workers/FolderScanWorker.hpp"

#include <QAction>
#include <QDebug>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileDialog>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QWidget>
#include <QThreadPool>

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
    , m_toggleSlideshowAction(new QAction(tr("▶ Slideshow"), this))
    , m_fitToWindowAction(new QAction(tr("Přizpůsobit oknu"), this))
    , m_resetZoomAction(new QAction(tr("Originální velikost (1:1)"), this))
    , m_fullscreenAction(new QAction(tr("Celá obrazovka (F)"), this))
    , m_rememberLastFolderAction(new QAction(tr("Zapamatovat poslední složku"), this))
    , m_togglePanelAction(nullptr)
{
    setWindowTitle("PictureViewer");
    resize(1200, 750);
    setWindowIcon(QIcon(":/icons/eye_icon.ico"));
    setCentralWidget(m_imageView);
    connect(m_thumbnailPanel, &ThumbnailPanel::imageSelected, this, &MainWindow::showImage);
    connect(m_slideshowController, &SlideshowController::nextImageRequested, this, &MainWindow::showNextImage);
    setupDock();
    setupMenu();
    setupToolbar();
    setupStatusBar();
    restoreLastFolder();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        showPreviousImage();
        event->accept();
        return;
    case Qt::Key_Right:
        showNextImage();
        event->accept();
        return;
    case Qt::Key_Space:
        toggleSlideshow();
        event->accept();
        return;
    case Qt::Key_F:
        toggleFullscreen();
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
    m_requestedFile = filePath;
    const QString folderPath = filePath.section('/', 0, -2);
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
    if (generation != m_scanGeneration) {
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
    ++m_scanGeneration;
    m_statusLabel->setText(tr("Načítám složku…"));

    if (m_folderScanWorker != nullptr) {
        m_folderScanWorker->cancel();
        m_folderScanWorker = nullptr;
    }

    auto *worker = new FolderScanWorker(folderPath, m_scanGeneration, this);
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
    if (m_thumbnailDock != nullptr && m_togglePanelAction->isChecked()) {
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
    m_toggleSlideshowAction->setShortcut(QKeySequence(Qt::Key_Space));
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
}

void MainWindow::setupStatusBar()
{
    statusBar()->addWidget(m_statusLabel);
    m_statusLabel->setText(tr("Vyber složku s obrázky."));
}

} // namespace pictureviewer

#pragma once

#include "core/ImageCatalog.hpp"
#include "core/ImageMetadataReader.hpp"

#include <QKeyEvent>
#include <QMainWindow>
#include <QStringList>

class QAction;
class QDockWidget;
class QLabel;
class QSpinBox;

namespace pictureviewer {

class ImageView;
class FolderScanWorker;
class SettingsManager;
class SlideshowController;
class ThumbnailPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Called when a file is opened from macOS Finder or command line
    void openFile(const QString &filePath);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void openFolderDialog();
    void openFileDialog();
    void onRememberLastFolderToggled(bool checked);
    void onScanComplete(int generation, const QStringList &paths);
    void onScanError(int generation, const QString &error);
    void onScanFinished(int generation);
    void showPreviousImage();
    void showNextImage();
    void toggleSlideshow();
    void toggleFullscreen();

private:
    void loadFolder(const QString &folderPath);
    void enterFullscreen();
    void exitFullscreen();
    void restoreLastFolder();
    void showImage(int index);
    void updateStatus(const QString &path);
    void setupDock();
    void setupMenu();
    void setupStatusBar();
    void setupToolbar();

    ImageMetadataReader m_imageMetadataReader;
    QStringList m_imagePaths;
    QString m_requestedFile;
    int m_currentIndex = -1;
    int m_scanGeneration = 0;
    bool m_isFullscreen = false;
    FolderScanWorker *m_folderScanWorker;
    ImageView *m_imageView;
    SettingsManager *m_settingsManager;
    ThumbnailPanel *m_thumbnailPanel;
    QDockWidget *m_thumbnailDock;
    QLabel *m_statusLabel;
    QSpinBox *m_intervalSpinBox;
    SlideshowController *m_slideshowController;
    QAction *m_openFolderAction;
    QAction *m_openFileAction;
    QAction *m_previousImageAction;
    QAction *m_nextImageAction;
    QAction *m_toggleSlideshowAction;
    QAction *m_fitToWindowAction;
    QAction *m_resetZoomAction;
    QAction *m_fullscreenAction;
    QAction *m_rememberLastFolderAction;
    QAction *m_togglePanelAction;
};

} // namespace pictureviewer

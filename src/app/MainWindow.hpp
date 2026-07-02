#pragma once

#include "app/ProfileManager.hpp"
#include "core/ImageCatalog.hpp"
#include "core/ImageMetadataReader.hpp"

#include <QKeyEvent>
#include <QMainWindow>
#include <QStringList>

class QAction;
class QActionGroup;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QGraphicsColorizeEffect;
class QLabel;
class QMenu;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QToolBar;
class QToolButton;

namespace pictureviewer {

class CategoryManager;
class ImageLoader;
class ImageView;
class FolderScanWorker;
class MetadataPanel;
class SettingsManager;
class SlideshowController;
class ThumbnailPanel;
class VideoPlayer;
class VideoThumbnailWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // Přepínatelná rozložení UI (Nastavení → Vzhled aplikace)
    enum class UiLayout { Classic, Filmstrip, Immersive, Gallery, Pro };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Called when a file is opened from macOS Finder or command line
    void openFile(const QString &filePath);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void openFolderDialog();
    void openFileDialog();
    void onRememberLastFolderToggled(bool checked);
    void onEnableDeleteImageToggled(bool checked);
    void onEnableMoveToDeleteToggled(bool checked);
    void onAskConfirmationToggled(bool checked);
    void onEnablePdfProcessingToggled(bool checked);
    void onEnableImagesToggled(bool checked);
    void onEnableVideosToggled(bool checked);
    void onRotateLeft();
    void onRotateRight();
    void onScanComplete(int generation, const QStringList &paths);
    void onScanError(int generation, const QString &error);
    void onScanFinished(int generation);
    void showPreviousImage();
    void showNextImage();
    void toggleSlideshow();
    void toggleFullscreen();
    void deleteOrMoveCurrentImage();
    void deleteImageToTrash();
    void moveImageToDeleteFolder();
    void renameCurrentImage();
    void onDeleteFolder();
    void onImageDecoded(const QString &path, const QImage &image);
    void onPlayVideo();
    void onVideoStopped();
    void showImageContextMenu(const QPoint &globalPos);
    void onScreenshotCapture();   // zachytit výřez obrazovky a zobrazit ho v aplikaci

private:
    void cancelAllWorkers();   // cancel + disconnect every background task
    void loadFolder(const QString &folderPath);
    void reloadCurrentFolder();   // znovu naskenovat (po změně řazení), zachovat obrázek
    void applyUiLayout(UiLayout layout);
    void displayPathEarly(const QString &path);   // zobrazení souboru před koncem skenu
    void prefetchNeighbors();
    void enterGalleryGrid();
    void leaveGalleryGrid();
    void showGalleryGrid();
    void setupOverlayToolbar();
    void positionOverlayToolbar();
    void showOverlayToolbar();
    void enterFullscreen();
    void exitFullscreen();
    void restoreLastFolder();
    void showImage(int index);
    void updateStatus(const QString &path);
    void setupDock();
    void setupMenu();
    void setupStatusBar();
    void setupToolbar();
    bool showDeleteConfirmationDialog();
    void removeImageFromList(int index);
    void updateConfirmationActionState();
    void disableImageBrowsing();
    void enableImageBrowsing();
    void applyGrayscaleEffect(bool enable);
    void updateVideoMetadata(const QString &videoPath);
    void updateFavoritesMenu();   // obnovit menu oblíbených složek

    // ── Profily ────────────────────────────────────────────────────────────────
    void setupProfileMenu(QMenu *parentMenu);
    void switchProfile(const QString &profileName);
    void refreshProfileMenu();
    void manageProfiles();
    // Vytvoří ProfileManager, provede migraci a vrátí SettingsManager pro aktivní
    // profil. Volá se z member-initializer listu (proto vrací pointer).
    SettingsManager *createProfileAndSettings();
    QMenu *m_profileMenu = nullptr;

    // ── Oblíbené složky toolbar ──────────────────────────────────────────────
    void setupFavoritesToolbar();          // vytvořit toolbar pro oblíbené složky
    void refreshFavoriteButtons();         // znovu vytvořit tlačítka oblíbených
    void onAddCurrentFolderToFavorites();  // přidat aktuální složku do oblíbených
    void onFavoriteRemove(const QString &folderPath);  // odebrat složku z oblíbených
    QString pickRandomUnusedFavoriteColor() const;     // vybrat barvu pro novou oblíbenou

    // ── PDF toolbar ──────────────────────────────────────────────────────────
    void setupPdfToolbar();
    void updatePdfToolbarVisibility(bool isPdf);
    void onPdfGoToPage();
    void onPdfScreenshot();

    // ── Kategorie ────────────────────────────────────────────────────────────
    void setupCategoriesToolbar();         // vytvořit toolbar pro kategorie
    void refreshCategoryButtons();         // znovu vytvořit tlačítka pro přiřazení
    void onCategoryButtonToggled(int categoryId);  // toggle přiřazení kategorie
    void updateCategoryButtonStates();     // aktualizovat stavy tlačítek podle obrázku
    void updateCategoryFilterButtons();    // znovu vytvořit tlačítka pro filtrování
    void onCategoryFilterToggled(int categoryId);  // toggle filtr kategorie
    void clearFilters();                  // vyčistit filtr
    void onCategoryRemoveAll();            // smazat všechny kategorie z obrázku
    void onCategoryFilterChanged();        // filtrování podle vybraných kategorií
    void updateStatusBarCategories();      // aktualizovat kategorie ve status baru
    void onCategoryRename(int categoryId); // přejmenovat kategorii
    void onCategoryChangeColor(int categoryId); // změnit barvu kategorie
    void onCategoryDelete(int categoryId); // smazat kategorii

    ImageMetadataReader m_imageMetadataReader;
    QStringList m_imagePaths;     // obrázky po filtrování (pokud je filtr aktivní)
    QStringList m_unfilteredImagePaths;  // všechny obrázky bez filtru
    QString m_requestedFile;
    QString m_currentFolder;   // poslední načtená složka (pro reload při změně řazení)
    int m_currentIndex = -1;
    int m_lastPrefetchIndex = -1;   // pro detekci směru listování
    int m_scanGeneration = 0;
    bool m_isFullscreen = false;
    bool m_shuttingDown = false;
    bool m_thumbnailDockWasVisible = true;   // stav panelu před vstupem do fullscreenu
    QList<int> m_categoryFilterIds;   // vybrané kategorie pro filtrování
    QMap<int, QPushButton*> m_categoryButtons;  // mapa: categoryId → assignment button widget
    QMap<int, QPushButton*> m_categoryFilterButtons;  // mapa: categoryId → filter button widget
    QWidget *m_filterButtonsContainer = nullptr;  // kontejner pro filtr tlačítka
    FolderScanWorker *m_folderScanWorker;
    ImageLoader *m_imageLoader = nullptr;
    QString m_pendingDisplayPath;   // cesta, jejíž dekódování čeká na zobrazení
    ImageView *m_imageView;
    // Pozn.: m_profileManager MUSÍ být deklarován před m_settingsManager —
    // pořadí inicializace v konstruktoru odpovídá pořadí deklarace zde.
    ProfileManager *m_profileManager = nullptr;
    SettingsManager *m_settingsManager;
    CategoryManager *m_categoryManager = nullptr;
    VideoPlayer *m_videoPlayer = nullptr;
    VideoThumbnailWorker *m_videoThumbnailWorker = nullptr;
    ThumbnailPanel *m_thumbnailPanel;
    QDockWidget *m_thumbnailDock;
    UiLayout m_uiLayout = UiLayout::Classic;
    bool m_galleryGridActive = false;        // panel je v centrálním stacku (režim Galerie)
    QStackedWidget *m_centralStack = nullptr;
    QToolBar *m_mainToolbar = nullptr;
    QToolBar *m_favoritesToolbar = nullptr;    // Sekundární toolbar pro oblíbené složky
    QToolBar *m_categoriesToolbar = nullptr;   // Sekundární toolbar pro kategorie (skrytá/viditelná)
    QToolBar *m_pdfToolbar = nullptr;          // Toolbar pro PDF — viditelný jen při PDF
    QDockWidget *m_metadataDock = nullptr;
    MetadataPanel *m_metadataPanel = nullptr;
    QWidget *m_overlayToolbar = nullptr;
    QTimer *m_overlayHideTimer = nullptr;
    QActionGroup *m_layoutActionGroup = nullptr;
    QLabel *m_statusLabel;
    QLabel *m_zoomLabel = nullptr;    // indikátor zoomu vpravo ve status baru
    QLabel *m_pdfPageLabel = nullptr; // indikátor "strana X / Y" v PDF toolbaru
    QToolButton *m_sortButton = nullptr; // dropdown tlačítko řazení v toolbaru

    void updateSortButtonText(); // aktualizuje popisek m_sortButton podle nastavení
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
    QAction *m_enableDeleteImageAction;
    QAction *m_enableMoveToDeleteAction;
    QAction *m_askConfirmationAction;
    QAction *m_enablePdfProcessingAction;
    QAction *m_enableImagesAction = nullptr;
    QAction *m_enableVideosAction = nullptr;
    QAction *m_deleteFolderAction;
    QAction *m_deletePictureAction;
    QAction *m_recycleAction = nullptr;
    // Zásobník přesunutých souborů: {cesta v Delete složce, původní cesta}
    QList<QPair<QString, QString>> m_deleteHistory;

    void onUndoDelete();
    void updateRecycleButtonState();
    QAction *m_renameImageAction;
    QAction *m_reloadFolderAction = nullptr;
    QAction *m_screenshotAction = nullptr;
    QAction *m_rotateLeftAction = nullptr;
    QAction *m_rotateRightAction = nullptr;
    QAction *m_cropAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    bool m_imageModified = false;
    bool m_isScreenshot  = false;  // setImage() bez souboru — kopírovat jako JPEG

    // ── Ukládání obrázku ─────────────────────────────────────────────────────
    void onSaveImage();
    void onSaveAsImage();
    void updateSaveButtonStates();
    void saveImageToPath(const QString &targetPath);
    // Zobrazí dialog pro zadání názvu a výběr cílové složky.
    // Vrací absolutní cestu k cílovému souboru, nebo prázdný řetězec při zrušení.
    QString runSaveAsDialog(const QString &originalPath);
};

} // namespace pictureviewer

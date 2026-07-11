#pragma once

#include "app/ProfileManager.hpp"
#include "core/FolderNavigator.hpp"
#include "core/ImageCatalog.hpp"
#include "core/ImageMetadataReader.hpp"

#include <QKeyEvent>
#include <QMainWindow>
#include <QStringList>
#include <QUrl>

#include <memory>

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
class FolderNavWorker;
class MetadataPanel;
class SettingsManager;
class SlideshowController;
class ThumbnailPanel;
class UpdateChecker;
class VideoPlayer;
class VideoThumbnailWorker;
struct MoveButtonInfo;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // Přepínatelná rozložení UI (Nastavení → Vzhled aplikace)
    enum class UiLayout { Classic, Filmstrip, Immersive, Gallery, Pro };

    // Undo historie přesunů/mazání se ukládá po SKUPINÁCH: jedna skupina =
    // jedna uživatelská akce (aktivní soubor + jeho párové soubory). Undo vrací
    // celou skupinu jedním krokem. Pár = {cílová cesta, původní cesta}.
    using FileMovePair = QPair<QString, QString>;
    using MoveGroup    = QList<FileMovePair>;

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
    // Tiše zastaví video, pokud právě běží. Vrací true, když běželo —
    // jediné místo pro „uvolni soubor před manipulací" logiku.
    bool stopVideoIfPlaying();
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
    // Odebere soubor ze seznamu i panelu náhledů. showNext=false odloží
    // zobrazení dalšího souboru na volajícího (hromadné operace tak nedekódují
    // obrázek po každém odebrání) — volající pak zavolá showCurrentAfterRemoval().
    void removeImageFromList(int index, bool showNext = true);
    // Zobrazí soubor na (zaokrouhleném) indexu po sérii removeImageFromList(…, false).
    void showCurrentAfterRemoval(int anchorIndex);
    void updateConfirmationActionState();
    void disableImageBrowsing();
    void enableImageBrowsing();
    void applyGrayscaleEffect(bool enable);
    void updateFavoritesMenu();   // obnovit menu oblíbených složek

    // ── Profily ────────────────────────────────────────────────────────────────
    void setupProfileMenu(QMenu *parentMenu);
    void switchProfile(const QString &profileName);
    void refreshProfileMenu();
    void manageProfiles();
    void showProfileStartupSettings();

    // ── Aktualizace ─────────────────────────────────────────────────────────
    void setupUpdateChecker();
    void scheduleStartupUpdateCheck();
    void onUpdateAvailable(const QString &version, const QString &notes,
                           const QUrl &releasePageUrl, const QUrl &installerUrl,
                           const QUrl &checksumsUrl, const QString &installerName,
                           bool silent);
    UpdateChecker *m_updateChecker = nullptr;
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

    // ── Přesun do složky (Move toolbar) ──────────────────────────────────────
    void setupMoveToolbar();
    void refreshMoveButtons();
    void onMoveButtonAdd();
    void onMoveButtonClicked(int moveButtonId);
    void onMoveButtonRename(int moveButtonId);
    void onMoveButtonChangeColor(int moveButtonId);
    void onMoveButtonChangeFolder(int moveButtonId);
    void onMoveButtonDelete(int moveButtonId);
    // Provede přesun jednoho souboru do složky tlačítka. Úspěšný přesun zapíše
    // do `group` (kvůli skupinovému undo); vrací true při úspěchu.
    bool performSingleMove(const QString &filePath, const MoveButtonInfo &button,
                           MoveGroup &group);
    void onUndoMove();
    void updateMoveUndoButtonState();
    QString pickRandomUnusedMoveColor() const;

    // Párové soubory (obrázek/video se stejným názvem). Vrátí seznam souborů,
    // se kterými se má akce provést (aktivní první + zvolené páry). cancelled=true,
    // pokud uživatel u 2+ párů zvolil Storno. Když je volba vypnutá nebo 0 párů,
    // vrátí jen {activeFile}. verb = "přesunout" / "smazat" pro text dialogu.
    QStringList resolveCompanionSet(const QString &activeFile, const QString &verb,
                                    bool &cancelled);
    void onMoveCompanionToggled(bool checked);

    // Vrátí poslední skupinu z dané undo historie zpět na původní umístění
    // (sdíleno mezi ↩ Přesun a ♻ recyklace z Delete — logika je identická).
    // Nic nedělá, je-li historie prázdná; skupinu z historie odebere vždy,
    // kromě případu obsazeného původního umístění (pak ji ponechá k opakování).
    void undoLastGroup(QList<MoveGroup> &history, const QString &notFoundMessage);

    // ── Navigace mezi složkami (Folder nav toolbar) ──────────────────────────
    enum class FolderNavDirection { Left, Right, Up, Down };
    void setupFolderNavToolbar();
    void onToggleFolderNavToolbar();
    // Spustí (nebo zruší běžící a znovu spustí) FolderNavWorker pro aktuální
    // složku — jen pokud je toolbar viditelný. Nahoru se počítá synchronně.
    // Aktualizuje jen ZOBRAZENÍ tlačítek (název/počet) — není zdrojem pravdy
    // pro navigaci, ta se vždy přepočítá čerstvě až v okamžiku kliknutí.
    void refreshFolderNavData();
    void onFolderNavDataReady(int generation, FolderNavResult left, FolderNavResult right, FolderNavResult down);
    // Při kliknutí vždy nejdřív znovu zjistí aktuální stav adresářové
    // struktury (rychlé synchronní čtení) a teprve s čerstvým výsledkem
    // naviguje — tlačítko tak nikdy nepoužije zastaralou (např. mezitím
    // smazanou) cestu.
    void onFolderNavClicked(FolderNavDirection direction);
    void updateFolderNavButton(QPushButton *button, const QString &arrow, const FolderNavResult &result, bool loading);

    QToolBar *m_folderNavToolbar = nullptr;
    QPushButton *m_folderNavLeftButton = nullptr;
    QPushButton *m_folderNavRightButton = nullptr;
    QPushButton *m_folderNavUpButton = nullptr;
    QPushButton *m_folderNavDownButton = nullptr;
    FolderNavWorker *m_folderNavWorker = nullptr;
    int m_folderNavGeneration = 0;

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
    // Stav sekundárních toolbarů před vstupem do fullscreenu (viz enterFullscreen/exitFullscreen).
    bool m_favoritesToolbarWasVisible = false;
    bool m_categoriesToolbarWasVisible = false;
    bool m_moveToolbarWasVisible = false;
    bool m_folderNavToolbarWasVisible = false;
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
    std::unique_ptr<CategoryManager> m_categoryManager;
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
    QToolBar *m_moveToolbar = nullptr;         // Sekundární toolbar pro rychlý přesun do složky
    QMap<int, QPushButton*> m_moveButtons;     // mapa: moveButtonId → tlačítko
    QAction *m_moveUndoAction = nullptr;
    // Zásobník přesunutých souborů (přes Move toolbar), po skupinách (viz MoveGroup).
    QList<MoveGroup> m_moveHistory;
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
    QAction *m_moveCompanionAction = nullptr;
    QAction *m_enablePdfProcessingAction;
    QAction *m_enableImagesAction = nullptr;
    QAction *m_enableVideosAction = nullptr;
    QAction *m_deleteFolderAction;
    QAction *m_deletePictureAction;
    QAction *m_recycleAction = nullptr;
    // Zásobník smazaných souborů (do složky Delete), po skupinách (viz MoveGroup).
    QList<MoveGroup> m_deleteHistory;

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

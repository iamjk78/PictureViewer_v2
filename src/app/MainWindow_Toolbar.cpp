// MainWindow_Toolbar.cpp — toolbars, video, PDF, favorites, sort
// QPushButton must be included BEFORE MainWindow.hpp to resolve the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/ImageView.hpp"
#include "app/PredefinedColors.hpp"
#include "app/ScreenCapture.hpp"
#include "app/SettingsManager.hpp"
#include "app/SlideshowController.hpp"
#include "app/ThumbnailPanel.hpp"
#include "app/VideoPlayer.hpp"

#include <QAction>
#include <QActionGroup>
#include <QColor>
#include <QCursor>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QStackedWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace pictureviewer {

void MainWindow::setupToolbar()
{
    auto *toolbar = addToolBar(tr("Navigace"));
    toolbar->setObjectName("mainToolbar");
    toolbar->setMovable(false);
    m_mainToolbar = toolbar;

    // Bez explicitní velikosti si macOS/Qt volí velikost pixmapy per-ikona podle
    // poměru stran zdrojového .ico (delete_folder_icon.ico má jiný poměr stran
    // než delete_picture_icon.ico/rename.ico), takže bitmapové ikony vypadaly
    // v toolbaru různě velké. Pevná velikost sjednotí všechny QIcon-akce.
    toolbar->setIconSize(QSize(20, 20));

    constexpr int ICON_SIZE = 28;
    const QString iconButtonStyle = QStringLiteral(
        "QToolButton { border: 0.5px solid #ccc; border-radius: 3px; "
        "  padding: 2px; min-width: %1px; width: %1px; min-height: %1px; height: %1px; "
        "  background: transparent; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); }")
        .arg(ICON_SIZE);

    // Poznámka k Unicode znakům v toolbaru: ◀ ▶ ✂ ↕ mají oficiální emoji
    // (barevnou/tučnou) variantu přes variation selector U+FE0F ("◀️" "▶️" …),
    // což je vizuálně sjednotí s ostatními emoji ikonami (🗑 ➕ 💾 ⭐ ♻).
    // ⟲ ⟳ žádnou emoji variantu nemají — ty se řeší zvlášť větším písmem níže.
    m_previousImageAction->setShortcut(QKeySequence(Qt::ShiftModifier | Qt::Key_Left));
    m_previousImageAction->setText(QStringLiteral("◀️"));
    m_previousImageAction->setToolTip(tr("Předchozí obrázek (Shift+←)"));

    m_nextImageAction->setShortcut(QKeySequence(Qt::ShiftModifier | Qt::Key_Right));
    m_nextImageAction->setText(QStringLiteral("▶️"));
    m_nextImageAction->setToolTip(tr("Další obrázek (Shift+→)"));

    m_toggleSlideshowAction->setShortcut(QKeySequence("S"));
    m_toggleSlideshowAction->setText(QStringLiteral("▶️"));
    m_toggleSlideshowAction->setToolTip(tr("Spustit slideshow (S)"));

    m_intervalSpinBox->setRange(1, 60);
    m_intervalSpinBox->setValue(m_slideshowController->intervalMs() / 1000);
    m_intervalSpinBox->setSuffix(tr(" s"));
    m_intervalSpinBox->setMaximumWidth(50);

    connect(m_previousImageAction, &QAction::triggered, this, &MainWindow::showPreviousImage);
    connect(m_nextImageAction, &QAction::triggered, this, &MainWindow::showNextImage);
    connect(m_toggleSlideshowAction, &QAction::triggered, this, &MainWindow::toggleSlideshow);
    connect(m_intervalSpinBox, &QSpinBox::valueChanged, this, [this](int seconds) {
        m_slideshowController->setInterval(seconds * 1000);
    });

    m_openFolderAction->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    m_openFolderAction->setText(QString());
    m_openFolderAction->setToolTip(tr("Otevřít složku (Ctrl+O)"));
    toolbar->addAction(m_openFolderAction);

    m_reloadFolderAction = new QAction(QStringLiteral("🔄"), this);
    m_reloadFolderAction->setToolTip(tr("Znovu načíst složku (F5)"));
    m_reloadFolderAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_reloadFolderAction->setEnabled(false);
    connect(m_reloadFolderAction, &QAction::triggered, this, &MainWindow::reloadCurrentFolder);
    toolbar->addAction(m_reloadFolderAction);

    m_screenshotAction = new QAction(QStringLiteral("📷"), this);
    m_screenshotAction->setToolTip(tr("Snímek výřezu obrazovky (i mimo aplikaci)"));
    connect(m_screenshotAction, &QAction::triggered, this, &MainWindow::onScreenshotCapture);
    toolbar->addAction(m_screenshotAction);
    toolbar->addSeparator();

    toolbar->addAction(m_previousImageAction);
    toolbar->addAction(m_nextImageAction);
    toolbar->addSeparator();
    toolbar->addAction(m_toggleSlideshowAction);
    toolbar->addWidget(m_intervalSpinBox);
    toolbar->addSeparator();

    // Sort button
    m_sortButton = new QToolButton(toolbar);
    m_sortButton->setPopupMode(QToolButton::InstantPopup);
    m_sortButton->setText(QStringLiteral("↕️"));
    m_sortButton->setToolTip(tr("Řazení souborů"));
    m_sortButton->setStyleSheet(iconButtonStyle);

    auto *sortMenu = new QMenu(m_sortButton);

    auto *sortKeyGroup = new QActionGroup(sortMenu);
    sortKeyGroup->setExclusive(true);
    const struct { int key; QString label; } sortKeys[] = {
        { 0, tr("Podle názvu") },
        { 1, tr("Podle data změny") },
        { 2, tr("Podle velikosti") },
    };
    const int savedSortKey = m_settingsManager->sortKey();
    for (const auto &entry : sortKeys) {
        auto *action = sortMenu->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(entry.key == savedSortKey);
        sortKeyGroup->addAction(action);
        const int key = entry.key;
        connect(action, &QAction::triggered, this, [this, key] {
            m_settingsManager->setSortKey(key);
            reloadCurrentFolder();
            updateSortButtonText();
        });
    }

    sortMenu->addSeparator();

    auto *sortOrderGroup = new QActionGroup(sortMenu);
    sortOrderGroup->setExclusive(true);
    const bool ascending = m_settingsManager->sortAscending();
    const struct { bool asc; QString label; } sortOrders[] = {
        { true,  tr("Vzestupně ↑") },
        { false, tr("Sestupně ↓")  },
    };
    for (const auto &entry : sortOrders) {
        auto *action = sortMenu->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(entry.asc == ascending);
        sortOrderGroup->addAction(action);
        const bool asc = entry.asc;
        connect(action, &QAction::triggered, this, [this, asc] {
            m_settingsManager->setSortAscending(asc);
            reloadCurrentFolder();
            updateSortButtonText();
        });
    }

    m_sortButton->setMenu(sortMenu);
    toolbar->addWidget(m_sortButton);
    toolbar->addSeparator();

    m_rotateLeftAction = new QAction(QStringLiteral("⟲"), this);
    m_rotateLeftAction->setToolTip(tr("Otočit doleva ([ nebo L)"));
    m_rotateLeftAction->setShortcuts({QKeySequence(Qt::Key_BracketLeft), QKeySequence(Qt::Key_L)});
    connect(m_rotateLeftAction, &QAction::triggered, this, &MainWindow::onRotateLeft);

    m_rotateRightAction = new QAction(QStringLiteral("⟳"), this);
    m_rotateRightAction->setToolTip(tr("Otočit doprava (])"));
    m_rotateRightAction->setShortcut(QKeySequence(Qt::Key_BracketRight));
    connect(m_rotateRightAction, &QAction::triggered, this, &MainWindow::onRotateRight);

    m_renameImageAction->setText(QStringLiteral("✏️"));
    m_renameImageAction->setToolTip(tr("Přejmenovat obrázek (R)"));
    toolbar->addAction(m_renameImageAction);
    toolbar->addSeparator();

    toolbar->addAction(m_rotateLeftAction);
    toolbar->addAction(m_rotateRightAction);
    toolbar->addSeparator();

    // ⟲ ⟳ nemají emoji variantu (na rozdíl od ◀️ ▶️ ✂️ ↕️ výše), takže bez
    // zásahu vypadají mnohem menší než ostatní ikony — kompenzujeme větším písmem.
    const QString rotateGlyphStyle = QStringLiteral(
        "QToolButton { border: 0.5px solid #ccc; border-radius: 3px; "
        "  padding: 2px; min-width: 28px; width: 28px; min-height: 28px; height: 28px; "
        "  background: transparent; font-size: 30px; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); border: 0.5px solid #999; }");
    if (auto *btn = qobject_cast<QToolButton *>(toolbar->widgetForAction(m_rotateLeftAction))) {
        btn->setStyleSheet(rotateGlyphStyle);
    }
    if (auto *btn = qobject_cast<QToolButton *>(toolbar->widgetForAction(m_rotateRightAction))) {
        btn->setStyleSheet(rotateGlyphStyle);
    }

    m_cropAction = new QAction(QStringLiteral("✂️"), this);
    m_cropAction->setToolTip(tr("Ořez obrázku — označte oblast myší"));
    m_cropAction->setCheckable(true);
    connect(m_cropAction, &QAction::toggled, this, [this](bool checked) {
        m_imageView->setCropMode(checked);
    });
    connect(m_imageView, &ImageView::cropModeChanged, this, [this](bool active) {
        m_cropAction->setChecked(active);
    });
    toolbar->addAction(m_cropAction);
    toolbar->addSeparator();

    m_saveAction = new QAction(QStringLiteral("💾"), this);
    m_saveAction->setToolTip(tr("Uložit upravenou kopii (přepsat originál)"));
    m_saveAction->setEnabled(false);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveImage);

    m_saveAsAction = new QAction(QStringLiteral("➕"), this);
    m_saveAsAction->setToolTip(tr("Uložit jako nový soubor JPEG"));
    m_saveAsAction->setEnabled(false);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAsImage);

    toolbar->addAction(m_saveAction);
    toolbar->addAction(m_saveAsAction);
    toolbar->addSeparator();

    connect(m_imageView, &ImageView::imageModified, this, [this]() {
        m_imageModified = true;
        updateSaveButtonStates();
    });

    m_deletePictureAction->setText(QStringLiteral("🗑"));
    m_deletePictureAction->setToolTip(tr("Smazat obrázek (D)"));
    toolbar->addAction(m_deletePictureAction);

    m_deleteFolderAction->setText(QStringLiteral("❌"));
    m_deleteFolderAction->setToolTip(tr("Smazání složky Delete"));
    toolbar->addAction(m_deleteFolderAction);

    m_recycleAction = new QAction(QStringLiteral("♻"), this);
    m_recycleAction->setToolTip(tr("Vrátit poslední soubor"));
    m_recycleAction->setEnabled(false);
    connect(m_recycleAction, &QAction::triggered, this, &MainWindow::onUndoDelete);
    toolbar->addAction(m_recycleAction);

    // Apply consistent icon-only styling to all toolbar buttons.
    // font-size zvětšeno na 20px, aby textové/emoji glyfy (◀ ▶ ⟲ ✂ …) vizuálně
    // odpovídaly velikosti bitmapových ikon (setIconSize 20×20 výše).
    const QString toolButtonStyle = QStringLiteral(
        "QToolButton { border: 0.5px solid #ccc; border-radius: 3px; "
        "  padding: 2px; min-width: 28px; width: 28px; min-height: 28px; height: 28px; "
        "  background: transparent; font-size: 20px; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); border: 0.5px solid #999; }");
    toolbar->setStyleSheet(toolButtonStyle);
}

void MainWindow::setupStatusBar()
{
    statusBar()->addWidget(m_statusLabel);
    m_statusLabel->setText(tr("Vyber složku s obrázky."));

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

QString MainWindow::pickRandomUnusedFavoriteColor() const
{
    return pickRandomUnusedColor(m_settingsManager->favoriteFolderColors());
}

void MainWindow::setupFavoritesToolbar()
{
    addToolBarBreak();

    m_favoritesToolbar = addToolBar(tr("Oblíbené složky"));
    m_favoritesToolbar->setObjectName("favoritesToolbar");
    m_favoritesToolbar->setMovable(false);

    constexpr int ICON_SIZE = 28;
    const QString iconButtonStyle = QStringLiteral(
        "QToolButton { border: 0.5px solid #ccc; border-radius: 3px; "
        "  padding: 2px; min-width: %1px; width: %1px; min-height: %1px; height: %1px; "
        "  background: transparent; font-size: 14px; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); border: 0.5px solid #999; }")
        .arg(ICON_SIZE);

    QAction *addAction = m_favoritesToolbar->addAction(QStringLiteral("➕"));
    addAction->setToolTip(tr("Přidat aktuální složku do oblíbených"));
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddCurrentFolderToFavorites);

    if (auto *btn = qobject_cast<QToolButton *>(m_favoritesToolbar->widgetForAction(addAction))) {
        btn->setStyleSheet(iconButtonStyle);
    }

    m_favoritesToolbar->addSeparator();

    refreshFavoriteButtons();

    m_mainToolbar->addSeparator();
    QAction *toggleFavoritesAction = m_mainToolbar->addAction(QStringLiteral("⭐"));
    toggleFavoritesAction->setToolTip(tr("Zobrazit/skrýt panel oblíbených složek"));
    connect(toggleFavoritesAction, &QAction::triggered, this, [this] {
        m_favoritesToolbar->setVisible(!m_favoritesToolbar->isVisible());
        m_settingsManager->setFavoritesToolbarVisible(m_favoritesToolbar->isVisible());
    });

    m_favoritesToolbar->setVisible(m_settingsManager->favoritesToolbarVisible());
    m_favoritesToolbar->setStyleSheet(iconButtonStyle);
}

void MainWindow::refreshFavoriteButtons()
{
    while (m_favoritesToolbar->actions().size() > 2) {
        QAction *last = m_favoritesToolbar->actions().last();
        m_favoritesToolbar->removeAction(last);
        delete last;
    }

    const QStringList folders = m_settingsManager->favoriteFolders();
    const QStringList colors  = m_settingsManager->favoriteFolderColors();

    for (int i = 0; i < folders.size(); ++i) {
        const QString &path = folders.at(i);
        QString colorHex = (i < colors.size() && !colors.at(i).isEmpty())
                           ? colors.at(i)
                           : defaultItemColor();

        QString displayName = QDir(path).dirName();
        if (displayName.isEmpty()) {
            displayName = path;
        }

        QPushButton *btn = new QPushButton(displayName);
        btn->setFlat(false);
        btn->setToolTip(path);

        QColor color(colorHex);
        QString textColor = color.lightness() > 128 ? "#000000" : "#FFFFFF";
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 2px solid #ccc;"
            "  border-radius: 4px;"
            "  padding: 2px 8px;"
            "  font-weight: bold;"
            "  font-size: 14px;"
            "  min-height: 30px;"
            "}"
            "QPushButton:pressed {"
            "  border: 3px solid #333;"
            "}"
        ).arg(colorHex, textColor));

        connect(btn, &QPushButton::clicked, this, [this, path] {
            loadFolder(path);
        });

        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, this, [this, path]() {
            QMenu menu;
            menu.addAction(tr("Odebrat z oblíbených"), this, [this, path] {
                onFavoriteRemove(path);
            });
            menu.exec(QCursor::pos());
        });

        m_favoritesToolbar->addWidget(btn);
    }
}

void MainWindow::onAddCurrentFolderToFavorites()
{
    if (m_currentFolder.isEmpty()) {
        return;
    }

    if (m_settingsManager->isFavoriteFolder(m_currentFolder)) {
        return;
    }

    QString color = pickRandomUnusedFavoriteColor();
    if (!m_settingsManager->addFavoriteFolder(m_currentFolder, color)) {
        QMessageBox::warning(this, tr("Limit oblíbených"),
            tr("Byl dosažen maximální počet oblíbených složek (10).\n"
               "Před přidáním nové složky odeberte některou stávající\n"
               "(pravý klik na tlačítko složky → Odebrat z oblíbených)."));
        return;
    }

    refreshFavoriteButtons();
    updateFavoritesMenu();
}

void MainWindow::onFavoriteRemove(const QString &folderPath)
{
    m_settingsManager->removeFavoriteFolder(folderPath);
    refreshFavoriteButtons();
    updateFavoritesMenu();
}

void MainWindow::updateFavoritesMenu()
{
    QMenuBar *mb = menuBar();
    if (!mb) {
        return;
    }

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

    int sepIdx = -1;
    for (int i = 0; i < favMenu->actions().size(); ++i) {
        if (favMenu->actions()[i]->isSeparator()) {
            sepIdx = i;
            break;
        }
    }

    if (sepIdx >= 0) {
        while (favMenu->actions().size() > sepIdx + 1) {
            delete favMenu->actions().last();
        }
    }

    const QStringList favorites = m_settingsManager->favoriteFolders();
    if (favorites.isEmpty()) {
        favMenu->addAction(tr("(prázdné)"))->setEnabled(false);
        return;
    }

    for (const QString &path : favorites) {
        QString displayName = QDir(path).dirName();
        if (displayName.isEmpty()) {
            displayName = path;
        }

        QAction *folderAction = favMenu->addAction(displayName);
        folderAction->setToolTip(path);

        connect(folderAction, &QAction::triggered, this, [this, path] {
            loadFolder(path);
        });
    }
}

void MainWindow::updateSortButtonText()
{
    if (!m_sortButton) {
        return;
    }
    const bool asc = m_settingsManager->sortAscending();
    // ⬆/⬇ (ne ↑/↓) — mají výchozí emoji (tučnou) reprezentaci, konzistentní
    // s ostatními ikonami toolbaru.
    m_sortButton->setText(asc ? QStringLiteral("⬆") : QStringLiteral("⬇"));
}

void MainWindow::setupPdfToolbar()
{
    m_pdfToolbar = new QToolBar(tr("PDF"), this);
    m_pdfToolbar->setObjectName("pdfToolbar");
    m_pdfToolbar->setMovable(false);
    addToolBarBreak(Qt::TopToolBarArea);
    addToolBar(Qt::TopToolBarArea, m_pdfToolbar);
    m_pdfToolbar->hide();

    auto *prevAction = new QAction(QStringLiteral("◀"), m_pdfToolbar);
    prevAction->setToolTip(tr("Předchozí stránka (PgUp)"));
    connect(prevAction, &QAction::triggered, this, [this]() { m_imageView->previousPage(); });

    auto *nextAction = new QAction(QStringLiteral("▶"), m_pdfToolbar);
    nextAction->setToolTip(tr("Další stránka (PgDn)"));
    connect(nextAction, &QAction::triggered, this, [this]() { m_imageView->nextPage(); });

    m_pdfPageLabel = new QLabel(QStringLiteral("  -  "), m_pdfToolbar);
    m_pdfPageLabel->setAlignment(Qt::AlignCenter);
    m_pdfPageLabel->setMinimumWidth(70);

    auto *gotoAction = new QAction(tr("Přejít na stranu"), m_pdfToolbar);
    gotoAction->setToolTip(tr("Zadat číslo stránky a přejít na ni"));
    connect(gotoAction, &QAction::triggered, this, &MainWindow::onPdfGoToPage);

    auto *screenshotAction = new QAction(tr("Screenshot"), m_pdfToolbar);
    screenshotAction->setToolTip(tr("Uložit aktuální stránku jako obrázek (JPEG) — pak použijte Uložit jako"));
    connect(screenshotAction, &QAction::triggered, this, &MainWindow::onPdfScreenshot);

    m_pdfToolbar->addAction(prevAction);
    m_pdfToolbar->addWidget(m_pdfPageLabel);
    m_pdfToolbar->addAction(nextAction);
    m_pdfToolbar->addSeparator();
    m_pdfToolbar->addAction(gotoAction);
    m_pdfToolbar->addSeparator();
    m_pdfToolbar->addAction(screenshotAction);

    const QString style =
        "QToolButton {"
        "  font-size: 14px; font-weight: bold;"
        "  min-height: 30px; padding: 2px 10px; border-radius: 4px;"
        "}";
    m_pdfToolbar->setStyleSheet(style);
}

void MainWindow::updatePdfToolbarVisibility(bool isPdf)
{
    if (!m_pdfToolbar) {
        return;
    }
    if (isPdf) {
        m_pdfToolbar->show();
    } else {
        m_pdfToolbar->hide();
        if (m_pdfPageLabel) {
            m_pdfPageLabel->setText(QStringLiteral("  -  "));
        }
    }
}

void MainWindow::onPdfGoToPage()
{
    if (!m_imageView->isPdfLoaded()) {
        return;
    }
    const int total = m_imageView->pdfPageCount();
    bool ok = false;
    const int page = QInputDialog::getInt(
        this,
        tr("Přejít na stranu"),
        tr("Číslo strany (1 – %1):").arg(total),
        m_imageView->currentPdfPage() + 1,
        1, total, 1, &ok
    );
    if (ok) {
        m_imageView->goToPage(page - 1);
    }
}

void MainWindow::onPdfScreenshot()
{
    if (!m_imageView->isPdfLoaded()) {
        return;
    }
    const QImage img = m_imageView->displayedImage();
    if (img.isNull()) {
        return;
    }
    m_imageView->setImage(img);
    m_isScreenshot  = true;
    m_imageModified = true;
    updateSaveButtonStates();
    updatePdfToolbarVisibility(false);
    m_statusLabel->setText(tr("Stránka PDF zachycena jako obrázek — použijte Uložit jako pro uložení."));
}

void MainWindow::onScreenshotCapture()
{
    const ScreenCaptureResult result = captureScreenRegion(this);

    if (result.image.isNull()) {
        m_statusLabel->setText(
            tr("Snímek zrušen. Pokud jste právě povolili Screen Recording, "
               "restartujte aplikaci a zkuste znovu."));
        return;
    }

    m_imageView->setImage(result.image);
    m_isScreenshot  = true;
    m_imageModified = true;
    updateSaveButtonStates();
    m_statusLabel->setText(
        tr("Výřez obrazovky zachycen (%1×%2) — použijte Uložit jako pro trvalé uložení.")
            .arg(result.image.width())
            .arg(result.image.height()));
}

void MainWindow::onPlayVideo()
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    const QString imagePath = m_imagePaths.at(m_currentIndex);
    QString videoPath;

    if (!VideoPlayer::findVideoFile(imagePath, videoPath)) {
        m_statusLabel->setText(tr("Video se stejným názvem neexistuje."));
        return;
    }

    disableImageBrowsing();
    m_centralStack->setCurrentWidget(m_videoPlayer);
    m_videoPlayer->playFile(videoPath);
    m_statusLabel->setText(tr("Přehrávám: %1").arg(QFileInfo(videoPath).fileName()));
}

void MainWindow::onVideoStopped()
{
    m_centralStack->setCurrentWidget(m_imageView);
    enableImageBrowsing();
    m_statusLabel->setText(tr("Přehrávání ukončeno."));
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
    if (m_reloadFolderAction) m_reloadFolderAction->setEnabled(false);

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
    if (m_reloadFolderAction) m_reloadFolderAction->setEnabled(!m_currentFolder.isEmpty());

    if (m_thumbnailDock) {
        m_thumbnailDock->setEnabled(true);
    }

    updateConfirmationActionState();
}

void MainWindow::applyGrayscaleEffect(bool enable)
{
    if (enable) {
        setWindowOpacity(0.6);
    } else {
        setWindowOpacity(1.0);
    }
}

} // namespace pictureviewer

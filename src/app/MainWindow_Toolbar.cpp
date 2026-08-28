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
#include "app/ToolbarStyle.hpp"
#include "app/VideoPlayer.hpp"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
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
#include <QSizePolicy>
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
    constexpr int ICON_SIZE = kMainToolbarIconSize;
    toolbar->setIconSize(QSize(ICON_SIZE, ICON_SIZE));

    const QString iconButtonStyle = QStringLiteral(
        "QToolButton { border: none; border-radius: 3px; "
        "  padding: 0px; min-width: %1px; width: %1px; min-height: %2px; height: %2px; "
        "  background: transparent; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); }")
        .arg(kMainToolbarButtonWidth).arg(ICON_SIZE);

    // Poznámka k Unicode znakům v toolbaru: ◀ ▶ ✂ ↕ mají oficiální emoji
    // (barevnou/tučnou) variantu přes variation selector U+FE0F ("◀️" "▶️" …),
    // což je vizuálně sjednotí s ostatními emoji ikonami (🗑 ➕ 💾 ⭐ ♻).
    // ⟲ ⟳ žádnou emoji variantu nemají — ty se řeší zvlášť větším písmem níže.
    m_previousImageAction->setShortcut(QKeySequence(Qt::ShiftModifier | Qt::Key_Left));
    m_previousImageAction->setIcon(QIcon(QStringLiteral(":/icons/icon_previous.png")));
    m_previousImageAction->setText(QString());
    m_previousImageAction->setToolTip(tr("Předchozí obrázek (Shift+←)"));

    m_nextImageAction->setShortcut(QKeySequence(Qt::ShiftModifier | Qt::Key_Right));
    m_nextImageAction->setIcon(QIcon(QStringLiteral(":/icons/icon_next.png")));
    m_nextImageAction->setText(QString());
    m_nextImageAction->setToolTip(tr("Další obrázek (Shift+→)"));

    m_toggleSlideshowAction->setShortcut(QKeySequence("S"));
    m_toggleSlideshowAction->setIcon(QIcon(QStringLiteral(":/icons/icon_play_slideshow.png")));
    m_toggleSlideshowAction->setToolTip(tr("Spustit slideshow (S)"));

    m_slideshowIntervalButton = new QToolButton(toolbar);
    m_slideshowIntervalButton->setPopupMode(QToolButton::InstantPopup);
    m_slideshowIntervalButton->setIcon(QIcon(QStringLiteral(":/icons/icon_slideshow_interval.png")));
    m_slideshowIntervalButton->setStyleSheet(iconButtonStyle);

    auto *intervalMenu = new QMenu(m_slideshowIntervalButton);
    intervalMenu->setStyleSheet(QStringLiteral("QMenu { font-size: 16px; font-weight: bold; }"));

    auto *intervalGroup = new QActionGroup(intervalMenu);
    intervalGroup->setExclusive(true);
    const int intervalChoicesSeconds[] = { 1, 2, 3, 5, 10, 20, 30 };
    const int currentSeconds = m_slideshowController->intervalMs() / 1000;
    for (const int seconds : intervalChoicesSeconds) {
        auto *action = intervalMenu->addAction(tr("%1 s").arg(seconds));
        action->setCheckable(true);
        action->setChecked(seconds == currentSeconds);
        intervalGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, seconds] {
            m_slideshowController->setInterval(seconds * 1000);
            updateSlideshowIntervalButtonText();
        });
    }
    m_slideshowIntervalButton->setMenu(intervalMenu);
    updateSlideshowIntervalButtonText();

    connect(m_previousImageAction, &QAction::triggered, this, &MainWindow::showPreviousImage);
    connect(m_nextImageAction, &QAction::triggered, this, &MainWindow::showNextImage);
    connect(m_toggleSlideshowAction, &QAction::triggered, this, &MainWindow::toggleSlideshow);

    m_openFolderAction->setIcon(QIcon(QStringLiteral(":/icons/icon_open_folder.png")));
    m_openFolderAction->setText(QString());
    m_openFolderAction->setToolTip(tr("Otevřít složku (Ctrl+O)"));
    toolbar->addAction(m_openFolderAction);

    m_reloadFolderAction = new QAction(QIcon(QStringLiteral(":/icons/icon_reload.png")), QString(), this);
    m_reloadFolderAction->setToolTip(tr("Znovu načíst složku (F5)"));
    m_reloadFolderAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_reloadFolderAction->setEnabled(false);
    connect(m_reloadFolderAction, &QAction::triggered, this, &MainWindow::reloadCurrentFolder);
    toolbar->addAction(m_reloadFolderAction);

    m_screenshotAction = new QAction(QIcon(QStringLiteral(":/icons/icon_screenshot.png")), QString(), this);
    m_screenshotAction->setToolTip(tr("Snímek výřezu obrazovky (i mimo aplikaci)"));
    connect(m_screenshotAction, &QAction::triggered, this, &MainWindow::onScreenshotCapture);
    toolbar->addAction(m_screenshotAction);
    toolbar->addSeparator();

    toolbar->addAction(m_previousImageAction);
    toolbar->addAction(m_nextImageAction);
    toolbar->addSeparator();
    toolbar->addAction(m_toggleSlideshowAction);
    toolbar->addWidget(m_slideshowIntervalButton);
    toolbar->addSeparator();

    // Sort button
    m_sortButton = new QToolButton(toolbar);
    m_sortButton->setPopupMode(QToolButton::InstantPopup);
    m_sortButton->setIcon(QIcon(QStringLiteral(":/icons/icon_sort.png")));
    m_sortButton->setToolTip(tr("Řazení souborů"));
    m_sortButton->setStyleSheet(iconButtonStyle);

    auto *sortMenu = new QMenu(m_sortButton);
    sortMenu->setStyleSheet(QStringLiteral("QMenu { font-size: 16px; font-weight: bold; }"));

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

    m_rotateLeftAction = new QAction(QIcon(QStringLiteral(":/icons/icon_rotate_left.png")), QString(), this);
    m_rotateLeftAction->setToolTip(tr("Otočit doleva ([ nebo L)"));
    m_rotateLeftAction->setShortcuts({QKeySequence(Qt::Key_BracketLeft), QKeySequence(Qt::Key_L)});
    connect(m_rotateLeftAction, &QAction::triggered, this, &MainWindow::onRotateLeft);

    m_rotateRightAction = new QAction(QIcon(QStringLiteral(":/icons/icon_rotate_right.png")), QString(), this);
    m_rotateRightAction->setToolTip(tr("Otočit doprava (])"));
    m_rotateRightAction->setShortcut(QKeySequence(Qt::Key_BracketRight));
    connect(m_rotateRightAction, &QAction::triggered, this, &MainWindow::onRotateRight);

    m_renameImageAction->setIcon(QIcon(QStringLiteral(":/icons/icon_rename.png")));
    m_renameImageAction->setToolTip(tr("Přejmenovat obrázek (R)"));
    toolbar->addAction(m_renameImageAction);
    toolbar->addSeparator();

    toolbar->addAction(m_rotateLeftAction);
    toolbar->addAction(m_rotateRightAction);
    toolbar->addSeparator();

    m_cropAction = new QAction(QIcon(QStringLiteral(":/icons/icon_crop.png")), QString(), this);
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

    m_saveAction = new QAction(QIcon(QStringLiteral(":/icons/icon_save_overwrite.png")), QString(), this);
    m_saveAction->setToolTip(tr("Uložit upravenou kopii (přepsat originál)"));
    m_saveAction->setEnabled(false);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveImage);

    m_saveAsAction = new QAction(QIcon(QStringLiteral(":/icons/icon_save_as.png")), QString(), this);
    m_saveAsAction->setToolTip(tr("Uložit jako nový soubor (obrázek jako JPEG, video jako kopie pod novým názvem)"));
    m_saveAsAction->setEnabled(false);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAsImage);

    toolbar->addAction(m_saveAction);
    toolbar->addAction(m_saveAsAction);
    toolbar->addSeparator();

    connect(m_imageView, &ImageView::imageModified, this, [this]() {
        m_imageModified = true;
        updateSaveButtonStates();
    });

    m_deletePictureAction->setIcon(QIcon(QStringLiteral(":/icons/icon_delete_current.png")));
    m_deletePictureAction->setText(QString());
    m_deletePictureAction->setToolTip(tr("Smazat obrázek (D)"));
    toolbar->addAction(m_deletePictureAction);

    m_deleteFolderAction->setIcon(QIcon(QStringLiteral(":/icons/icon_move_to_trash.png")));
    m_deleteFolderAction->setText(QString());
    m_deleteFolderAction->setToolTip(tr("Smazání složky Delete"));
    toolbar->addAction(m_deleteFolderAction);

    toolbar->addAction(m_deleteCurrentFolderAction);

    m_recycleAction = new QAction(QIcon(QStringLiteral(":/icons/icon_remove_from_trash.png")), QString(), this);
    m_recycleAction->setToolTip(tr("Vrátit poslední soubor"));
    m_recycleAction->setEnabled(false);
    connect(m_recycleAction, &QAction::triggered, this, &MainWindow::onUndoDelete);
    toolbar->addAction(m_recycleAction);

    // Apply consistent icon-only styling to all toolbar buttons.
    // font-size škáluje s ICON_SIZE, aby textové/emoji glyfy (◀ ▶ ⟲ ✂ …) vizuálně
    // odpovídaly velikosti bitmapových ikon (setIconSize výše).
    const QString toolButtonStyle = QStringLiteral(
        "QToolButton { border: none; border-radius: 3px; "
        "  padding: 0px; min-width: %1px; width: %1px; min-height: %2px; height: %2px; "
        "  background: transparent; font-size: 26px; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); } "
        "QToolBar::separator { background: transparent; width: 0px; }")
        .arg(kMainToolbarButtonWidth).arg(ICON_SIZE);
    toolbar->setStyleSheet(toolButtonStyle);

    // Viz applyMainToolbarButtonSize() — CSS rozměry samy o sobě na macOS
    // nestačí. Textovým/emoji tlačítkům se nastaví jen velikost boxu (jejich
    // glyf řídí font-size ve stylu výše), ikonovým i velikost ikony.
    for (QAction *action : toolbar->actions()) {
        if (auto *btn = qobject_cast<QToolButton *>(toolbar->widgetForAction(action))) {
            if (btn->icon().isNull()) {
                btn->setFixedSize(kMainToolbarButtonWidth, ICON_SIZE);
            } else {
                applyMainToolbarButtonSize(btn);
            }
        }
    }
}

void MainWindow::setupStatusBar()
{
    statusBar()->addWidget(m_statusLabel);
    m_statusLabel->setText(tr("Vyber složku s obrázky."));

    const QString shrinkTooltip = tr(
        "Zmenšit na okno\n"
        "Zapnuto: velké obrázky se při načtení zmenší, aby se vešly do okna.\n"
        "Vypnuto: velké obrázky se zobrazí v původní velikosti, i když přesahují okno.");
    const QString zoomTooltip = tr(
        "Zvětšit na okno\n"
        "Zapnuto: malé obrázky se při načtení zvětší na velikost okna.\n"
        "Vypnuto: malé obrázky se zobrazí v původní velikosti, i když jsou menší než okno.");

    m_fitControlsWidget = new QWidget(this);
    auto *fitLayout = new QHBoxLayout(m_fitControlsWidget);
    fitLayout->setContentsMargins(0, 0, 8, 0);
    fitLayout->setSpacing(3);

    auto *shrinkIcon = new QLabel(QStringLiteral("⬇"), m_fitControlsWidget);
    shrinkIcon->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    shrinkIcon->setToolTip(shrinkTooltip);
    m_shrinkToFitCheckBox = new QCheckBox(m_fitControlsWidget);
    m_shrinkToFitCheckBox->setToolTip(shrinkTooltip);
    m_shrinkToFitCheckBox->setChecked(m_settingsManager->shrinkToFitEnabled());
    connect(m_shrinkToFitCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_settingsManager->setShrinkToFitEnabled(checked);
        m_imageView->setShrinkToFitEnabled(checked);
    });
    fitLayout->addWidget(shrinkIcon);
    fitLayout->addWidget(m_shrinkToFitCheckBox);

    fitLayout->addSpacing(10);

    auto *zoomIcon = new QLabel(QStringLiteral("⬆"), m_fitControlsWidget);
    zoomIcon->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    zoomIcon->setToolTip(zoomTooltip);
    m_zoomToFitCheckBox = new QCheckBox(m_fitControlsWidget);
    m_zoomToFitCheckBox->setToolTip(zoomTooltip);
    m_zoomToFitCheckBox->setChecked(m_settingsManager->zoomToFitEnabled());
    connect(m_zoomToFitCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_settingsManager->setZoomToFitEnabled(checked);
        m_imageView->setZoomToFitEnabled(checked);
    });
    fitLayout->addWidget(zoomIcon);
    fitLayout->addWidget(m_zoomToFitCheckBox);

    statusBar()->addPermanentWidget(m_fitControlsWidget);

    m_imageView->setShrinkToFitEnabled(m_shrinkToFitCheckBox->isChecked());
    m_imageView->setZoomToFitEnabled(m_zoomToFitCheckBox->isChecked());

    m_zoomLabel = new QLabel(this);
    m_zoomLabel->hide();
    statusBar()->addPermanentWidget(m_zoomLabel);
    connect(m_imageView, &ImageView::zoomChanged, this, [this](double percent) {
        const bool meaningful = percent > 0.0;
        m_fitControlsWidget->setVisible(meaningful);
        if (!meaningful) {
            m_zoomLabel->hide();
        } else {
            m_zoomLabel->setText(tr("Zoom: %1 %").arg(qRound(percent)));
            m_zoomLabel->show();
        }
    });
    m_fitControlsWidget->hide();
}

QString MainWindow::pickRandomUnusedFavoriteColor() const
{
    return pickRandomUnusedColor(m_settingsManager->favoriteFolderColors());
}

void MainWindow::setupFavoritesToolbar()
{
    m_favoritesToolbar = addToolBar(tr("Oblíbené složky"));
    m_favoritesToolbar->setObjectName("favoritesToolbar");
    m_favoritesToolbar->setMovable(false);

    const QString iconButtonStyle = secondaryToolbarStyle();

    QAction *addAction = m_favoritesToolbar->addAction(QStringLiteral("➕"));
    addAction->setToolTip(tr("Přidat aktuální složku do oblíbených"));
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddCurrentFolderToFavorites);

    if (auto *btn = qobject_cast<QToolButton *>(m_favoritesToolbar->widgetForAction(addAction))) {
        btn->setStyleSheet(iconButtonStyle);
    }

    m_favoritesToolbar->addSeparator();

    refreshFavoriteButtons();

    m_mainToolbar->addSeparator();
    QAction *toggleFavoritesAction = m_mainToolbar->addAction(
        QIcon(QStringLiteral(":/icons/icon_favorites_folders.png")), QString());
    toggleFavoritesAction->setToolTip(tr("Zobrazit/skrýt panel oblíbených složek"));
    // Přidáno až PO setupToolbar(), viz applyMainToolbarButtonSize().
    applyMainToolbarButtonSize(
        qobject_cast<QToolButton *>(m_mainToolbar->widgetForAction(toggleFavoritesAction)));
    connect(toggleFavoritesAction, &QAction::triggered, this, [this] {
        m_favoritesToolbar->setVisible(!m_favoritesToolbar->isVisible());
        m_settingsManager->setFavoritesToolbarVisible(m_favoritesToolbar->isVisible());
    });

    addFullscreenPinAction(m_favoritesToolbar, QStringLiteral("favorites"));

    m_favoritesToolbar->setVisible(m_settingsManager->favoritesToolbarVisible());
    m_favoritesToolbar->setStyleSheet(iconButtonStyle);
}

void MainWindow::addFullscreenPinAction(QToolBar *toolbar, const QString &toolbarId, int buttonSize)
{
    // Hlavní toolbar si vlastní velikost ikon spravuje sám (setupToolbar()) —
    // QToolBar::setIconSize() mění efektivní velikost VŠECH tlačítek na
    // toolbaru (i těch s vlastním setIconSize() na widgetu), takže by tohle
    // volání jeho nastavení přepsalo. U sekundárních toolbarů (bez vlastního
    // setIconSize()) je špendlík jediná ikona, tak si o velikost řekne sám.
    if (toolbar != m_mainToolbar) {
        toolbar->setIconSize(QSize(buttonSize, buttonSize));
    }

    // Roztáhnout mezeru, aby špendlík byl vždy na pravém konci toolbaru bez
    // ohledu na to, kolik místa zaberou předchozí (i dynamicky měněné) prvky.
    QWidget *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // Zapamatovat si akci mezery — dynamicky přestavovaný obsah (tlačítka
    // štítků/filtru/přesunu) se musí vkládat PŘED ni, jinak skončí až za
    // špendlíkem a ten pak vypadá "posunutý doleva" (viz addToolbarContent()).
    m_pinSpacerActions.insert(toolbar, toolbar->addWidget(spacer));

    // Dvě hotové ikony místo emoji 📌 — barvu emoji glyfu nelze přes QSS měnit,
    // takže by z tlačítka nebylo poznat, jestli je špendlík aktivní.
    static const QIcon pinIconOff = QIcon(QStringLiteral(":/icons/pin_red.png"));
    static const QIcon pinIconOn  = QIcon(QStringLiteral(":/icons/pin_green.png"));

    QAction *pinAction = new QAction(this);
    pinAction->setCheckable(true);
    pinAction->setToolTip(tr("Ponechat viditelné i v režimu celé obrazovky"));
    const bool pinned = m_settingsManager->toolbarPinnedFullscreen(toolbarId);
    pinAction->setChecked(pinned);
    pinAction->setIcon(pinned ? pinIconOn : pinIconOff);
    toolbar->setProperty("fullscreenPinned", pinned);
    connect(pinAction, &QAction::toggled, this, [this, toolbar, toolbarId, pinAction](bool checked) {
        pinAction->setIcon(checked ? pinIconOn : pinIconOff);
        toolbar->setProperty("fullscreenPinned", checked);
        m_settingsManager->setToolbarPinnedFullscreen(toolbarId, checked);
        // Přepnutí špendlíku PŘÍMO ve fullscreenu se má projevit okamžitě, ne
        // až při příštím vstupu/výstupu z celoobrazovkového režimu.
        if (m_isFullscreen) {
            toolbar->setVisible(checked);
        }
    });
    toolbar->addAction(pinAction);

    QToolButton *pinButton = qobject_cast<QToolButton *>(toolbar->widgetForAction(pinAction));
    if (toolbar == m_mainToolbar) {
        applyMainToolbarButtonSize(pinButton);
    } else {
        applyToolbarButtonSize(pinButton, buttonSize);
    }
}

QAction *MainWindow::addToolbarContent(QToolBar *toolbar, QWidget *widget)
{
    // Vložit PŘED roztahovací mezeru se špendlíkem (viz addFullscreenPinAction),
    // aby nový obsah skončil vlevo od špendlíku, ne za ním. Toolbar bez
    // špendlíku (ještě nezaregistrovaný) obsah prostě přidá na konec.
    if (QAction *spacerAction = m_pinSpacerActions.value(toolbar)) {
        return toolbar->insertWidget(spacerAction, widget);
    }
    return toolbar->addWidget(widget);
}

void MainWindow::refreshFavoriteButtons()
{
    QAction *oldContainerAction = nullptr;
    for (QAction *action : m_favoritesToolbar->actions()) {
        QWidget *widget = m_favoritesToolbar->widgetForAction(action);
        if (widget && widget->objectName() == "favoriteButtonsContainer") {
            oldContainerAction = action;
            break;
        }
    }

    for (QPushButton *btn : m_favoriteButtons) {
        btn->deleteLater();
    }
    m_favoriteButtons.clear();

    if (oldContainerAction) {
        m_favoritesToolbar->removeAction(oldContainerAction);
    }

    QWidget *newContainer = new QWidget(this);
    newContainer->setObjectName("favoriteButtonsContainer");
    QHBoxLayout *containerLayout = new QHBoxLayout(newContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(4);

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

        containerLayout->addWidget(btn);
        m_favoriteButtons.append(btn);
    }

    QList<QAction*> actions = m_favoritesToolbar->actions();
    if (actions.size() >= 2) {
        m_favoritesToolbar->insertWidget(actions.at(1), newContainer);
    } else {
        addToolbarContent(m_favoritesToolbar, newContainer);
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
    m_sortButton->setToolTip(asc ? tr("Řazení souborů (vzestupně ↑)")
                                  : tr("Řazení souborů (sestupně ↓)"));
}

void MainWindow::updateSlideshowIntervalButtonText()
{
    if (!m_slideshowIntervalButton) {
        return;
    }
    const int seconds = m_slideshowController->intervalMs() / 1000;
    m_slideshowIntervalButton->setToolTip(tr("Interval slideshow: %1 s").arg(seconds));
}

void MainWindow::setupPdfToolbar()
{
    m_pdfToolbar = new QToolBar(tr("PDF"), this);
    m_pdfToolbar->setObjectName("pdfToolbar");
    m_pdfToolbar->setMovable(false);
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
    m_pdfToolbar->addSeparator();

    addFullscreenPinAction(m_pdfToolbar, QStringLiteral("pdf"));

    const QString style =
        "QToolButton {"
        "  font-size: 14px; font-weight: bold;"
        "  min-height: 30px; padding: 2px 10px; border-radius: 4px;"
        "}"
        "QToolBar::separator { background: transparent; width: 0px; }";
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
    // Snímky jsou dočasné (pro trvalé uložení slouží Uložit jako) — uklidit ty
    // starší, jinak by se v temp složce hromadily donekonečna.
    pruneOldScreenshots();

    const ScreenCaptureResult result = captureScreenRegion(this);

    if (result.image.isNull()) {
        m_statusLabel->setText(
            tr("Snímek zrušen. Pokud jste právě povolili Screen Recording, "
               "restartujte aplikaci a zkuste znovu."));
        return;
    }

    // Nový snímek se má zobrazit VŽDY, bez ohledu na to, co bylo v aplikaci
    // právě aktivní — jinak zůstal captured obrázek tiše napsaný do skrytého
    // ImageView (video/PDF toolbar/Galerie zůstaly na svém) a screenshot
    // nebyl vidět, dokud uživatel danou funkci sám neukončil.
    stopSlideshowIfRunning();
    if (stopVideoIfPlaying()) {
        m_centralStack->setCurrentWidget(m_imageView);
        if (m_rotateLeftAction)  m_rotateLeftAction->setEnabled(true);
        if (m_rotateRightAction) m_rotateRightAction->setEnabled(true);
        if (m_cropAction)        m_cropAction->setEnabled(true);
        scheduleVideoThumbnailResume();
    }
    if (m_galleryGridActive) {
        leaveGalleryGrid();
    }
    updatePdfToolbarVisibility(false);

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
    scheduleVideoThumbnailResume();
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
    if (m_deleteCurrentFolderAction) m_deleteCurrentFolderAction->setEnabled(false);
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
    // Tlačítko Delete je dostupné, když Delete složka existuje, nezávisle na tom,
    // jsou-li v aktuální složce nějaké soubory.
    m_deleteFolderAction->setEnabled(deleteFolderExists());
    // Smazání aktuální složky je dostupné, kdykoli je nějaká složka otevřená —
    // nezávisle na tom, jsou-li v ní zobrazené soubory (viz onDeleteCurrentFolder()).
    if (m_deleteCurrentFolderAction) m_deleteCurrentFolderAction->setEnabled(!m_currentFolder.isEmpty());
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

bool MainWindow::deleteFolderExists() const
{
    if (m_currentFolder.isEmpty()) {
        return false;
    }
    return QDir(m_currentFolder + "/Delete").exists();
}

} // namespace pictureviewer

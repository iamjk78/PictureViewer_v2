// Přepínání a správa profilů aplikace.
//
// Každý profil má vlastní config.ini (SettingsManager) a categories.db
// (CategoryManager) v adresáři profiles/<jméno>/. ProfileManager spravuje
// seznam profilů a aktivní volbu v profiles.ini.

#include "app/MainWindow.hpp"

#include "app/CategoryManager.hpp"
#include "app/ImageView.hpp"
#include "app/SettingsManager.hpp"
#include "app/ThumbnailPanel.hpp"
#include "app/VideoPlayer.hpp"
#include "workers/FolderScanWorker.hpp"
#include "workers/VideoThumbnailWorker.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

// Lokální kopie (anonymní namespace v MainWindow.cpp není viditelný odsud).
pictureviewer::MainWindow::UiLayout profileUiLayoutFromString(const QString &name)
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

// ── Inicializace z init-listu ─────────────────────────────────────────────────
// Vytvoří ProfileManager v adresáři config souboru, provede migraci staré ploché
// struktury a vrátí SettingsManager pro aktivní profil.
SettingsManager *MainWindow::createProfileAndSettings()
{
    const QString appConfigDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

    m_profileManager = new ProfileManager(appConfigDir);
    m_profileManager->migrateIfNeeded();

    // Při režimu pevného profilu přepnout na něj hned při startu.
    if (m_profileManager->startupMode() == ProfileManager::StartupMode::FixedProfile) {
        const QString fixed = m_profileManager->startupProfile();
        if (!fixed.isEmpty() && m_profileManager->profiles().contains(fixed)) {
            m_profileManager->setActiveProfile(fixed);
        }
    }

    const QString active = m_profileManager->activeProfile();
    return new SettingsManager(m_profileManager->configPath(active), active);
}

// ── Menu ──────────────────────────────────────────────────────────────────────

void MainWindow::setupProfileMenu(QMenu *parentMenu)
{
    m_profileMenu = parentMenu;
    refreshProfileMenu();
}

void MainWindow::refreshProfileMenu()
{
    if (m_profileMenu == nullptr) {
        return;
    }
    m_profileMenu->clear();

    const QString active = m_profileManager->activeProfile();
    for (const QString &name : m_profileManager->profiles()) {
        QAction *a = m_profileMenu->addAction(name);
        a->setCheckable(true);
        a->setChecked(name == active);
        connect(a, &QAction::triggered, this, [this, name] { switchProfile(name); });
    }
    m_profileMenu->addSeparator();
    m_profileMenu->addAction(tr("Spravovat profily…"), this, &MainWindow::manageProfiles);
    m_profileMenu->addAction(tr("Nastavení spuštění…"), this, &MainWindow::showProfileStartupSettings);
}

// ── Přepnutí ────────────────────────────────────────────────────────────────

void MainWindow::switchProfile(const QString &profileName)
{
    if (profileName == m_profileManager->activeProfile()) {
        return;
    }

    // Uložit stav okna aktuálního profilu.
    m_settingsManager->setWindowGeometry(saveGeometry());
    m_settingsManager->setWindowState(saveState());
    m_settingsManager->syncToDisk();

    // Zrušit probíhající skenování složky — nechceme výsledky starého profilu.
    // cancelAllWorkers() se NESMÍ použít: nastavuje shutdown příznak i na
    // ThumbnailPanel a ImageLoader, které pak odmítají novou práci.
    ++m_scanGeneration;
    if (m_folderScanWorker != nullptr) {
        m_folderScanWorker->cancel();
        disconnect(m_folderScanWorker, nullptr, this, nullptr);
        m_folderScanWorker = nullptr;
    }

    // Zastavit případné přehrávání videa — starý profil končí a VideoPlayer
    // nesmí zůstat aktivním widgetem nad daty nového profilu.
    if (stopVideoIfPlaying()) {
        m_centralStack->setCurrentWidget(m_imageView);
    }

    // Přepnout profil.
    m_profileManager->setActiveProfile(profileName);

    // Znovu vytvořit SettingsManager.
    delete m_settingsManager;
    m_settingsManager = new SettingsManager(
        m_profileManager->configPath(profileName), profileName);

    // VideoPlayer drží ukazatel na SettingsManager — bez aktualizace by další
    // změna hlasitosti zapisovala do právě uvolněné paměti.
    m_videoPlayer->setSettingsManager(m_settingsManager);

    // Znovu vytvořit CategoryManager (destruktor uzavře a odregistruje starou DB).
    m_categoryManager = std::make_unique<CategoryManager>(
        m_profileManager->dbPath(profileName));

    // Obnovit UI.
    m_imagePaths.clear();
    m_unfilteredImagePaths.clear();
    m_categoryFilterIds.clear();
    m_currentIndex = -1;
    m_currentFolder.clear();
    m_imageView->clearImage();
    refreshCategoryButtons();
    updateCategoryFilterButtons();
    refreshFavoriteButtons();
    updateFavoritesMenu();
    updateSortButtonText();
    applyUiLayout(profileUiLayoutFromString(m_settingsManager->uiLayout()));

    // Aktualizovat cache miniatur pro nový profil
    m_thumbnailPanel->setDiskCache(m_settingsManager->thumbnailCacheEnabled(),
                                   m_settingsManager->effectiveThumbnailCacheDir());
    if (m_videoThumbnailWorker) {
        m_videoThumbnailWorker->cancel();
        delete m_videoThumbnailWorker;
        m_videoThumbnailWorker = new VideoThumbnailWorker(
            m_settingsManager->thumbnailCacheEnabled(),
            m_settingsManager->effectiveThumbnailCacheDir(),
            this);
        connect(m_videoThumbnailWorker, &VideoThumbnailWorker::thumbnailReady,
                m_thumbnailPanel, &ThumbnailPanel::setVideoThumbnail);
    }

    restoreLastFolder();

    refreshProfileMenu();
    m_statusLabel->setText(tr("Profil přepnut na: %1").arg(profileName));
}

// ── Správa profilů ─────────────────────────────────────────────────────────

void MainWindow::manageProfiles()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Správa profilů"));
    dialog.resize(360, 320);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Profily:"), &dialog));

    auto *list = new QListWidget(&dialog);
    layout->addWidget(list);

    auto fillList = [&]() {
        list->clear();
        const QString active = m_profileManager->activeProfile();
        for (const QString &name : m_profileManager->profiles()) {
            QString label = name;
            if (name == active) {
                label += tr("  (aktivní)");
            }
            auto *item = new QListWidgetItem(label, list);
            item->setData(Qt::UserRole, name);
        }
    };
    fillList();

    auto selectedName = [&]() -> QString {
        QListWidgetItem *item = list->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    };

    auto showError = [&](const QString &fallback) {
        const QString err = m_profileManager->lastError();
        QMessageBox::warning(&dialog, tr("Chyba"),
                             err.isEmpty() ? fallback : err);
    };

    auto *buttonRow = new QHBoxLayout();
    auto *newBtn       = new QPushButton(tr("Nový"), &dialog);
    auto *renameBtn    = new QPushButton(tr("Přejmenovat"), &dialog);
    auto *duplicateBtn = new QPushButton(tr("Duplikovat"), &dialog);
    auto *deleteBtn    = new QPushButton(tr("Smazat"), &dialog);
    buttonRow->addWidget(newBtn);
    buttonRow->addWidget(renameBtn);
    buttonRow->addWidget(duplicateBtn);
    buttonRow->addWidget(deleteBtn);
    layout->addLayout(buttonRow);

    auto *closeBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(closeBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(closeBox);

    connect(newBtn, &QPushButton::clicked, &dialog, [&] {
        bool ok = false;
        const QString name = QInputDialog::getText(
            &dialog, tr("Nový profil"), tr("Název profilu:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || name.isEmpty()) {
            return;
        }
        if (m_profileManager->createProfile(name.trimmed(), /*copyFromActive=*/false)) {
            fillList();
            refreshProfileMenu();
        } else {
            showError(tr("Nelze vytvořit profil."));
        }
    });

    connect(renameBtn, &QPushButton::clicked, &dialog, [&] {
        const QString old = selectedName();
        if (old.isEmpty()) {
            return;
        }
        bool ok = false;
        const QString name = QInputDialog::getText(
            &dialog, tr("Přejmenovat profil"), tr("Nový název:"),
            QLineEdit::Normal, old, &ok);
        if (!ok || name.isEmpty()) {
            return;
        }
        if (m_profileManager->renameProfile(old, name.trimmed())) {
            fillList();
            refreshProfileMenu();
        } else {
            showError(tr("Nelze přejmenovat profil."));
        }
    });

    connect(duplicateBtn, &QPushButton::clicked, &dialog, [&] {
        const QString src = selectedName();
        if (src.isEmpty()) {
            return;
        }
        bool ok = false;
        const QString name = QInputDialog::getText(
            &dialog, tr("Duplikovat profil"), tr("Název kopie:"),
            QLineEdit::Normal, src + tr(" (kopie)"), &ok);
        if (!ok || name.isEmpty()) {
            return;
        }
        if (m_profileManager->duplicateProfile(src, name.trimmed())) {
            fillList();
            refreshProfileMenu();
        } else {
            showError(tr("Nelze duplikovat profil."));
        }
    });

    connect(deleteBtn, &QPushButton::clicked, &dialog, [&] {
        const QString name = selectedName();
        if (name.isEmpty()) {
            return;
        }
        const auto reply = QMessageBox::question(
            &dialog, tr("Smazat profil"),
            tr("Opravdu smazat profil \"%1\" a všechna jeho data?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
        const bool wasActive = (name == m_profileManager->activeProfile());
        if (m_profileManager->deleteProfile(name)) {
            fillList();
            refreshProfileMenu();
            // Pokud jsme smazali aktivní profil, ProfileManager přepnul na jiný —
            // přenačíst data pro nově aktivní profil.
            if (wasActive) {
                if (stopVideoIfPlaying()) {
                    m_centralStack->setCurrentWidget(m_imageView);
                }
                const QString newActive = m_profileManager->activeProfile();
                delete m_settingsManager;
                m_settingsManager = new SettingsManager(
                    m_profileManager->configPath(newActive), newActive);
                m_videoPlayer->setSettingsManager(m_settingsManager);
                m_categoryManager = std::make_unique<CategoryManager>(
                    m_profileManager->dbPath(newActive));
                m_imagePaths.clear();
                m_unfilteredImagePaths.clear();
                m_categoryFilterIds.clear();
                m_currentIndex = -1;
                m_currentFolder.clear();
                m_imageView->clearImage();
                refreshCategoryButtons();
                updateCategoryFilterButtons();
                refreshFavoriteButtons();
                updateFavoritesMenu();
                updateSortButtonText();
                applyUiLayout(profileUiLayoutFromString(m_settingsManager->uiLayout()));
                restoreLastFolder();
            }
        } else {
            showError(tr("Nelze smazat profil."));
        }
    });

    dialog.exec();
}

// ── Nastavení spuštění profilu ────────────────────────────────────────────────

void MainWindow::showProfileStartupSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Nastavení spuštění profilu"));
    dialog.setMinimumWidth(380);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        tr("Který profil se má aktivovat při spuštění aplikace?"), &dialog));
    layout->addSpacing(8);

    auto *rememberRadio = new QRadioButton(
        tr("Zapamatovat poslední použitý profil"), &dialog);
    auto *fixedRadio = new QRadioButton(
        tr("Vždy spustit s profilem:"), &dialog);

    const bool isFixed =
        (m_profileManager->startupMode() == ProfileManager::StartupMode::FixedProfile);
    rememberRadio->setChecked(!isFixed);
    fixedRadio->setChecked(isFixed);

    layout->addWidget(rememberRadio);

    auto *fixedRow = new QHBoxLayout();
    fixedRow->setContentsMargins(24, 0, 0, 0);
    fixedRow->addWidget(fixedRadio);
    auto *profileCombo = new QComboBox(&dialog);
    for (const QString &name : m_profileManager->profiles()) {
        profileCombo->addItem(name);
    }
    const QString currentFixed = m_profileManager->startupProfile();
    const QString preselect = (!currentFixed.isEmpty()
                               && m_profileManager->profiles().contains(currentFixed))
                              ? currentFixed
                              : m_profileManager->activeProfile();
    profileCombo->setCurrentIndex(profileCombo->findText(preselect));
    profileCombo->setEnabled(isFixed);
    fixedRow->addWidget(profileCombo, 1);
    layout->addLayout(fixedRow);

    connect(fixedRadio, &QRadioButton::toggled, profileCombo, &QComboBox::setEnabled);

    layout->addSpacing(12);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (rememberRadio->isChecked()) {
        m_profileManager->setStartupMode(ProfileManager::StartupMode::RememberLast);
    } else {
        m_profileManager->setStartupMode(ProfileManager::StartupMode::FixedProfile);
        m_profileManager->setStartupProfile(profileCombo->currentText());
    }
}

} // namespace pictureviewer

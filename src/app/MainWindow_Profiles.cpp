// Přepínání a správa profilů aplikace.
//
// Každý profil má vlastní config.ini (SettingsManager) a categories.db
// (CategoryManager) v adresáři profiles/<jméno>/. ProfileManager spravuje
// seznam profilů a aktivní volbu v profiles.ini.

#include "app/MainWindow.hpp"

#include "app/CategoryManager.hpp"
#include "app/ImageView.hpp"
#include "app/SettingsManager.hpp"

#include <QApplication>
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

    const QString active = m_profileManager->activeProfile();
    return new SettingsManager(m_profileManager->configPath(active));
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

    // Zastavit běžící úlohy nad starou složkou, aby neukládaly do nové DB.
    cancelAllWorkers();
    m_shuttingDown = false;   // cancelAllWorkers nastaví true; pokračujeme dál

    // Přepnout profil.
    m_profileManager->setActiveProfile(profileName);

    // Znovu vytvořit SettingsManager.
    delete m_settingsManager;
    m_settingsManager = new SettingsManager(
        m_profileManager->configPath(profileName));

    // Znovu vytvořit CategoryManager (destruktor uzavře a odregistruje starou DB).
    delete m_categoryManager;
    m_categoryManager = new CategoryManager(
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
                const QString newActive = m_profileManager->activeProfile();
                delete m_settingsManager;
                m_settingsManager = new SettingsManager(
                    m_profileManager->configPath(newActive));
                delete m_categoryManager;
                m_categoryManager = new CategoryManager(
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

} // namespace pictureviewer

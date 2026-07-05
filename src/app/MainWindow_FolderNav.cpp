// MainWindow_FolderNav.cpp — toolbar pro rychlou navigaci mezi sourozeneckými
// složkami (doleva/doprava), do rodiče (nahoru) a do první podsložky (dolů).
// QPushButton must be included BEFORE MainWindow.hpp to satisfy the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/ProfileManager.hpp"
#include "workers/FolderNavWorker.hpp"

#include <QAction>
#include <QThreadPool>
#include <QToolBar>

namespace pictureviewer {

void MainWindow::setupFolderNavToolbar()
{
    addToolBarBreak();

    m_folderNavToolbar = addToolBar(tr("Navigace mezi složkami"));
    m_folderNavToolbar->setObjectName("folderNavToolbar");
    m_folderNavToolbar->setMovable(false);

    const QString navButtonStyle = QStringLiteral(
        "QPushButton { border: 0.5px solid #ccc; border-radius: 4px; "
        "  padding: 3px 10px; font-size: 13px; background: transparent; } "
        "QPushButton:hover:enabled { background-color: rgba(0, 0, 0, 0.05); } "
        "QPushButton:disabled { color: #999; }");

    auto makeButton = [this, &navButtonStyle]() {
        auto *btn = new QPushButton();
        btn->setStyleSheet(navButtonStyle);
        connect(btn, &QPushButton::clicked, this, [this, btn] {
            onFolderNavClicked(btn->property("targetPath").toString());
        });
        return btn;
    };

    m_folderNavUpButton    = makeButton();
    m_folderNavLeftButton  = makeButton();
    m_folderNavRightButton = makeButton();
    m_folderNavDownButton  = makeButton();

    m_folderNavToolbar->addWidget(m_folderNavUpButton);
    m_folderNavToolbar->addSeparator();
    m_folderNavToolbar->addWidget(m_folderNavLeftButton);
    m_folderNavToolbar->addWidget(m_folderNavRightButton);
    m_folderNavToolbar->addSeparator();
    m_folderNavToolbar->addWidget(m_folderNavDownButton);

    // Výchozí (neaktivní) stav, dokud nejsou k dispozici data pro aktuální složku.
    updateFolderNavButton(m_folderNavUpButton,    QStringLiteral("▲"), {}, false);
    updateFolderNavButton(m_folderNavLeftButton,  QStringLiteral("◀"), {}, false);
    updateFolderNavButton(m_folderNavRightButton, QStringLiteral("▶"), {}, false);
    updateFolderNavButton(m_folderNavDownButton,  QStringLiteral("▼"), {}, false);

    m_mainToolbar->addSeparator();
    QAction *toggleNavAction = m_mainToolbar->addAction(QStringLiteral("🧭"));
    toggleNavAction->setToolTip(tr("Zobrazit/skrýt panel navigace mezi složkami"));
    connect(toggleNavAction, &QAction::triggered, this, &MainWindow::onToggleFolderNavToolbar);

    // Viditelnost je záměrně globální (ProfileManager), ne per-profil —
    // adresářová struktura na disku je pro všechny profily stejná.
    const bool visible = m_profileManager->navigationToolbarVisible();
    m_folderNavToolbar->setVisible(visible);
    if (visible) {
        refreshFolderNavData();
    }
}

void MainWindow::onToggleFolderNavToolbar()
{
    const bool willBeVisible = !m_folderNavToolbar->isVisible();
    m_folderNavToolbar->setVisible(willBeVisible);
    m_profileManager->setNavigationToolbarVisible(willBeVisible);

    if (willBeVisible) {
        refreshFolderNavData();
    } else if (m_folderNavWorker != nullptr) {
        // Toolbar skrytý — přestat počítat adresářovou strukturu na pozadí.
        m_folderNavWorker->cancel();
    }
}

void MainWindow::refreshFolderNavData()
{
    if (m_folderNavToolbar == nullptr || !m_folderNavToolbar->isVisible()
        || m_currentFolder.isEmpty()) {
        return;
    }

    // "Nahoru" je triviální kontrola (existuje rodič?) bez čtení obsahu
    // adresáře — počítá se synchronně, žádný worker není potřeba.
    const FolderNavResult up = FolderNavigator::parentFolder(m_currentFolder);
    updateFolderNavButton(m_folderNavUpButton, QStringLiteral("▲"), up, false);

    if (m_folderNavWorker != nullptr) {
        m_folderNavWorker->cancel();
    }
    ++m_folderNavGeneration;

    updateFolderNavButton(m_folderNavLeftButton,  QStringLiteral("◀"), {}, true);
    updateFolderNavButton(m_folderNavRightButton, QStringLiteral("▶"), {}, true);
    updateFolderNavButton(m_folderNavDownButton,  QStringLiteral("▼"), {}, true);

    auto *worker = new FolderNavWorker(m_currentFolder, m_folderNavGeneration, nullptr);
    connect(worker, &FolderNavWorker::navDataReady, this, &MainWindow::onFolderNavDataReady);
    connect(worker, &FolderNavWorker::finished, worker, &FolderNavWorker::deleteLater);
    connect(worker, &FolderNavWorker::finished, this, [this, worker](int) {
        if (m_folderNavWorker == worker) {
            m_folderNavWorker = nullptr;
        }
    });
    m_folderNavWorker = worker;
    QThreadPool::globalInstance()->start(worker);
}

void MainWindow::onFolderNavDataReady(int generation, FolderNavResult left, FolderNavResult right, FolderNavResult down)
{
    if (generation != m_folderNavGeneration || m_folderNavToolbar == nullptr
        || !m_folderNavToolbar->isVisible()) {
        return;
    }
    updateFolderNavButton(m_folderNavLeftButton,  QStringLiteral("◀"), left, false);
    updateFolderNavButton(m_folderNavRightButton, QStringLiteral("▶"), right, false);
    updateFolderNavButton(m_folderNavDownButton,  QStringLiteral("▼"), down, false);
}

void MainWindow::onFolderNavClicked(const QString &targetPath)
{
    if (targetPath.isEmpty()) {
        return;
    }
    // Stejný ověřený vzor jako u přepnutí profilu / mazání — zastavit
    // přehrávané video PŘED opuštěním aktuální složky.
    stopVideoIfPlaying();
    loadFolder(targetPath);
}

void MainWindow::updateFolderNavButton(QPushButton *button, const QString &arrow,
                                       const FolderNavResult &result, bool loading)
{
    if (button == nullptr) {
        return;
    }

    if (loading) {
        button->setText(arrow);
        button->setToolTip(tr("Zjišťuji adresářovou strukturu…"));
        button->setProperty("targetPath", QString());
        button->setEnabled(false);
        return;
    }

    if (!result.available) {
        button->setText(arrow);
        button->setToolTip(tr("V tomto směru není žádná složka."));
        button->setProperty("targetPath", QString());
        button->setEnabled(false);
        return;
    }

    button->setText(QStringLiteral("%1 %2 (%3)").arg(arrow, result.name).arg(result.count));
    button->setToolTip(result.path);
    button->setProperty("targetPath", result.path);
    button->setEnabled(true);
}

} // namespace pictureviewer

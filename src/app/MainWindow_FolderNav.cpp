// MainWindow_FolderNav.cpp — toolbar pro rychlou navigaci mezi sourozeneckými
// složkami (doleva/doprava), do rodiče (nahoru) a do první podsložky (dolů).
// QPushButton must be included BEFORE MainWindow.hpp to satisfy the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/SettingsManager.hpp"
#include "core/FolderNavigator.hpp"

#include <QAction>
#include <QIcon>
#include <QLabel>
#include <QToolBar>
#include <QToolButton>

namespace pictureviewer {

void MainWindow::setupFolderNavToolbar()
{
    m_folderNavToolbar = addToolBar(tr("Navigace mezi složkami"));
    m_folderNavToolbar->setObjectName("folderNavToolbar");
    m_folderNavToolbar->setMovable(false);

    const QString navButtonStyle = QStringLiteral(
        "QPushButton { border: 0.5px solid #ccc; border-radius: 4px; "
        "  padding: 3px 10px; font-size: 13px; background: transparent; } "
        "QPushButton:hover:enabled { background-color: rgba(0, 0, 0, 0.05); } "
        "QPushButton:disabled { color: #999; }");

    auto makeButton = [this, &navButtonStyle](FolderNavDirection direction) {
        auto *btn = new QPushButton();
        btn->setStyleSheet(navButtonStyle);
        connect(btn, &QPushButton::clicked, this, [this, direction] {
            onFolderNavClicked(direction);
        });
        return btn;
    };

    m_folderNavUpButton    = makeButton(FolderNavDirection::Up);
    m_folderNavLeftButton  = makeButton(FolderNavDirection::Left);
    m_folderNavRightButton = makeButton(FolderNavDirection::Right);
    m_folderNavDownButton  = makeButton(FolderNavDirection::Down);

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
    QAction *toggleNavAction = m_mainToolbar->addAction(
        QIcon(QStringLiteral(":/icons/icon_navigation.png")), QString());
    toggleNavAction->setToolTip(tr("Zobrazit/skrýt panel navigace mezi složkami"));
    connect(toggleNavAction, &QAction::triggered, this, &MainWindow::onToggleFolderNavToolbar);
    // Přidáno až PO setupToolbar() (které nastavuje pevnou velikost 44×44 pro
    // své vlastní akce) — bez tohoto by tlačítko mělo jen výchozí malou
    // velikost, viz stejný důvod u setFixedSize() v setupToolbar().
    if (auto *btn = qobject_cast<QToolButton *>(m_mainToolbar->widgetForAction(toggleNavAction))) {
        btn->setFixedSize(44, 44);
        btn->setIconSize(QSize(42, 42));
    }

    addFullscreenPinAction(m_folderNavToolbar, QStringLiteral("folderNav"));

    // Viditelnost je per-profil (config.ini), stejně jako Oblíbené/Štítky/Přesun.
    const bool visible = m_settingsManager->navigationToolbarVisible();
    m_folderNavToolbar->setVisible(visible);
    if (visible) {
        refreshFolderNavData();
    }
}

void MainWindow::onToggleFolderNavToolbar()
{
    const bool willBeVisible = !m_folderNavToolbar->isVisible();
    m_folderNavToolbar->setVisible(willBeVisible);
    m_settingsManager->setNavigationToolbarVisible(willBeVisible);

    if (willBeVisible) {
        refreshFolderNavData();
    }
}

void MainWindow::refreshFolderNavData()
{
    // isHidden(), ne isVisible() — to druhé při volání z konstruktoru MainWindow
    // (před prvním show()) vrací false pro úplně všechny widgety bez ohledu na
    // setVisible(), protože závisí na viditelnosti celého řetězce rodičů.
    if (m_folderNavToolbar == nullptr || m_folderNavToolbar->isHidden()) {
        return;
    }

    if (m_currentFolder.isEmpty()) {
        // Žádná aktuální složka (např. po přepnutí na profil bez zapamatované
        // složky) — tlačítka musí zobrazit prázdný stav, ne si držet zastaralá
        // data z předchozího profilu/složky.
        updateFolderNavButton(m_folderNavUpButton,    QStringLiteral("▲"), {}, false);
        updateFolderNavButton(m_folderNavLeftButton,  QStringLiteral("◀"), {}, false);
        updateFolderNavButton(m_folderNavRightButton, QStringLiteral("▶"), {}, false);
        updateFolderNavButton(m_folderNavDownButton,  QStringLiteral("▼"), {}, false);
        return;
    }

    // "Nahoru" je triviální kontrola (existuje rodič?) bez čtení obsahu
    // adresáře — počítá se synchronně, žádný worker není potřeba.
    const FolderNavResult up = FolderNavigator::parentFolder(m_currentFolder);
    updateFolderNavButton(m_folderNavUpButton, QStringLiteral("▲"), up, false);

    // POZNÁMKA: doleva/doprava/dolů dřív běžely na pozadí (QThreadPool), ale na
    // některých síťových discích (SMB/AFP) čtení obsahu složky z vedlejšího
    // vlákna vracelo prázdný výsledek (tlačítka natrvalo zůstala zakázaná,
    // fungovalo jen "nahoru", které obsah nečte). Počítá se proto synchronně
    // na hlavním vlákně stejně jako "nahoru" — čtení jen jmen podsložek je
    // levné i po síti, na rozdíl od generování náhledů.
    const FolderNavResult left  = FolderNavigator::siblingBefore(m_currentFolder);
    const FolderNavResult right = FolderNavigator::siblingAfter(m_currentFolder);
    const FolderNavResult down  = FolderNavigator::firstSubfolder(m_currentFolder);
    updateFolderNavButton(m_folderNavLeftButton,  QStringLiteral("◀"), left, false);
    updateFolderNavButton(m_folderNavRightButton, QStringLiteral("▶"), right, false);
    updateFolderNavButton(m_folderNavDownButton,  QStringLiteral("▼"), down, false);
}

void MainWindow::onFolderNavClicked(FolderNavDirection direction)
{
    if (m_currentFolder.isEmpty()) {
        return;
    }

    // Vždy znovu zjistit AKTUÁLNÍ stav adresářové struktury (rychlé
    // synchronní čtení) — tlačítko nesmí spoléhat na starý cache z poslední
    // async aktualizace, který může být zastaralý (např. sourozenecká
    // složka byla mezitím smazána).
    FolderNavResult fresh;
    switch (direction) {
    case FolderNavDirection::Left:  fresh = FolderNavigator::siblingBefore(m_currentFolder); break;
    case FolderNavDirection::Right: fresh = FolderNavigator::siblingAfter(m_currentFolder);  break;
    case FolderNavDirection::Up:    fresh = FolderNavigator::parentFolder(m_currentFolder);  break;
    case FolderNavDirection::Down:  fresh = FolderNavigator::firstSubfolder(m_currentFolder); break;
    }

    if (!fresh.available) {
        m_statusLabel->setText(tr("Tímto směrem už žádná složka není — aktualizuji seznam."));
        refreshFolderNavData();
        return;
    }

    // Stejný ověřený vzor jako u přepnutí profilu / mazání — zastavit
    // přehrávané video PŘED opuštěním aktuální složky.
    stopVideoIfPlaying();
    loadFolder(fresh.path);
}

void MainWindow::updateFolderNavButton(QPushButton *button, const QString &arrow,
                                       const FolderNavResult &result, bool loading)
{
    if (button == nullptr) {
        return;
    }

    // Tlačítko slouží jen k ZOBRAZENÍ (název cílové složky / počet) —
    // skutečná navigace se v onFolderNavClicked() vždy přepočítá čerstvě,
    // takže tu žádnou cestu neukládáme.
    if (loading) {
        button->setText(arrow);
        button->setToolTip(tr("Zjišťuji adresářovou strukturu…"));
        button->setEnabled(false);
        return;
    }

    if (!result.available) {
        button->setText(arrow);
        button->setToolTip(tr("V tomto směru není žádná složka."));
        button->setEnabled(false);
        return;
    }

    button->setText(QStringLiteral("%1 %2 (%3)").arg(arrow, result.name).arg(result.count));
    button->setToolTip(result.path);
    button->setEnabled(true);
}

} // namespace pictureviewer

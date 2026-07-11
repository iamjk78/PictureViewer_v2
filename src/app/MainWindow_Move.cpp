// MainWindow_Move.cpp — rychlý přesun obrázku/videa/PDF do vybrané složky
// přes vlastní tlačítka v toolbaru (viz CLAUDE.md / plán funkce "Přesun").
// QPushButton must be included BEFORE MainWindow.hpp to satisfy the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/CategoryManager.hpp"
#include "app/FileRetry.hpp"
#include "app/MoveDialogs.hpp"
#include "app/PredefinedColors.hpp"
#include "app/SettingsManager.hpp"
#include "app/ThumbnailPanel.hpp"
#include "core/CompanionFinder.hpp"

#include <QColorDialog>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
#include <QToolBar>
#include <QToolButton>

namespace pictureviewer {

QString MainWindow::pickRandomUnusedMoveColor() const
{
    QStringList used;
    for (const MoveButtonInfo &btn : m_settingsManager->moveButtons()) {
        used.append(btn.color);
    }
    return pickRandomUnusedColor(used);
}

void MainWindow::setupMoveToolbar()
{
    addToolBarBreak();

    m_moveToolbar = addToolBar(tr("Přesun"));
    m_moveToolbar->setObjectName("moveToolbar");
    m_moveToolbar->setMovable(false);

    constexpr int ICON_SIZE = 28;
    const QString iconButtonStyle = QStringLiteral(
        "QToolButton { border: 0.5px solid #ccc; border-radius: 3px; "
        "  padding: 2px; min-width: %1px; width: %1px; min-height: %1px; height: %1px; "
        "  background: transparent; font-size: 14px; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); border: 0.5px solid #999; }")
        .arg(ICON_SIZE);

    QAction *addAction = m_moveToolbar->addAction(QStringLiteral("➕"));
    addAction->setToolTip(tr("Vytvořit nové tlačítko přesunu"));
    connect(addAction, &QAction::triggered, this, &MainWindow::onMoveButtonAdd);
    if (auto *btn = qobject_cast<QToolButton *>(m_moveToolbar->widgetForAction(addAction))) {
        btn->setStyleSheet(iconButtonStyle);
    }

    m_moveToolbar->addSeparator();

    refreshMoveButtons();

    m_moveToolbar->addSeparator();

    m_moveUndoAction = new QAction(QStringLiteral("↩"), this);
    m_moveUndoAction->setToolTip(tr("Vrátit poslední přesunutý soubor zpět"));
    m_moveUndoAction->setEnabled(false);
    connect(m_moveUndoAction, &QAction::triggered, this, &MainWindow::onUndoMove);
    m_moveToolbar->addAction(m_moveUndoAction);
    if (auto *btn = qobject_cast<QToolButton *>(m_moveToolbar->widgetForAction(m_moveUndoAction))) {
        btn->setStyleSheet(iconButtonStyle);
    }

    m_mainToolbar->addSeparator();
    QAction *toggleMoveAction = m_mainToolbar->addAction(QStringLiteral("➡️"));
    toggleMoveAction->setToolTip(tr("Zobrazit/skrýt panel přesunů"));
    connect(toggleMoveAction, &QAction::triggered, this, [this] {
        m_moveToolbar->setVisible(!m_moveToolbar->isVisible());
        m_settingsManager->setMoveToolbarVisible(m_moveToolbar->isVisible());
    });

    m_moveToolbar->setVisible(m_settingsManager->moveToolbarVisible());
    m_moveToolbar->setStyleSheet(iconButtonStyle);
}

void MainWindow::refreshMoveButtons()
{
    QAction *oldContainerAction = nullptr;
    for (QAction *action : m_moveToolbar->actions()) {
        QWidget *widget = m_moveToolbar->widgetForAction(action);
        if (widget && widget->objectName() == "moveButtonsContainer") {
            oldContainerAction = action;
            break;
        }
    }

    for (QPushButton *btn : m_moveButtons) {
        btn->deleteLater();
    }
    m_moveButtons.clear();

    if (oldContainerAction) {
        m_moveToolbar->removeAction(oldContainerAction);
    }

    QWidget *newContainer = new QWidget(this);
    newContainer->setObjectName("moveButtonsContainer");
    QHBoxLayout *layout = new QHBoxLayout(newContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    const QList<MoveButtonInfo> buttons = m_settingsManager->moveButtons();

    for (const MoveButtonInfo &info : buttons) {
        QPushButton *btn = new QPushButton(info.name);
        btn->setFlat(false);
        btn->setToolTip(info.folder);

        const QColor color(info.color.isEmpty() ? defaultItemColor() : info.color);
        const QString textColor = color.lightness() > 128 ? "#000000" : "#FFFFFF";

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
        ).arg(color.name(), textColor));

        const int id = info.id;
        connect(btn, &QPushButton::clicked, this, [this, id] {
            onMoveButtonClicked(id);
        });

        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, this, [this, id]() {
            QMenu menu;
            menu.addAction(tr("Přejmenovat"), this, [this, id] { onMoveButtonRename(id); });
            menu.addAction(tr("Změnit barvu"), this, [this, id] { onMoveButtonChangeColor(id); });
            menu.addAction(tr("Změnit cílovou složku"), this, [this, id] { onMoveButtonChangeFolder(id); });
            menu.addSeparator();
            menu.addAction(tr("Odstranit"), this, [this, id] { onMoveButtonDelete(id); });
            menu.exec(QCursor::pos());
        });

        layout->addWidget(btn);
        m_moveButtons[info.id] = btn;
    }

    layout->addStretch();

    QList<QAction*> actions = m_moveToolbar->actions();
    if (actions.size() >= 2) {
        m_moveToolbar->insertWidget(actions.at(1), newContainer);
    } else {
        m_moveToolbar->addWidget(newContainer);
    }
}

void MainWindow::onMoveButtonAdd()
{
    NewMoveButtonDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QColor color = dialog.selectedColor();
    const QString colorHex = color.isValid() ? color.name() : pickRandomUnusedMoveColor();

    const int id = m_settingsManager->addMoveButton(dialog.buttonName(), dialog.selectedFolder(), colorHex);
    if (id < 0) {
        QMessageBox::warning(this, tr("Chyba"), tr("Nelze vytvořit tlačítko přesunu."));
        return;
    }
    refreshMoveButtons();
}

void MainWindow::onMoveButtonRename(int moveButtonId)
{
    const QList<MoveButtonInfo> buttons = m_settingsManager->moveButtons();
    QString currentName;
    for (const MoveButtonInfo &info : buttons) {
        if (info.id == moveButtonId) {
            currentName = info.name;
            break;
        }
    }

    bool ok = false;
    const QString newName = QInputDialog::getText(this, tr("Přejmenovat tlačítko"),
        tr("Nový název:"), QLineEdit::Normal, currentName, &ok);
    if (!ok || newName.trimmed().isEmpty()) {
        return;
    }

    m_settingsManager->renameMoveButton(moveButtonId, newName.trimmed());
    refreshMoveButtons();
}

void MainWindow::onMoveButtonChangeColor(int moveButtonId)
{
    const QList<MoveButtonInfo> buttons = m_settingsManager->moveButtons();
    QColor currentColor;
    for (const MoveButtonInfo &info : buttons) {
        if (info.id == moveButtonId) {
            currentColor = QColor(info.color);
            break;
        }
    }

    const QColor newColor = QColorDialog::getColor(currentColor, this, tr("Vyberte novou barvu"));
    if (!newColor.isValid()) {
        return;
    }

    m_settingsManager->setMoveButtonColor(moveButtonId, newColor.name());
    refreshMoveButtons();
}

void MainWindow::onMoveButtonChangeFolder(int moveButtonId)
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Vybrat cílovou složku"));
    if (folder.isEmpty()) {
        return;
    }

    m_settingsManager->setMoveButtonFolder(moveButtonId, folder);
    refreshMoveButtons();
}

void MainWindow::onMoveButtonDelete(int moveButtonId)
{
    const QList<MoveButtonInfo> buttons = m_settingsManager->moveButtons();
    QString name;
    for (const MoveButtonInfo &info : buttons) {
        if (info.id == moveButtonId) {
            name = info.name;
            break;
        }
    }

    const int result = QMessageBox::question(this, tr("Odstranit tlačítko"),
        tr("Opravdu chcete odstranit tlačítko přesunu '%1'?\n"
           "Cílová složka ani přesunuté soubory nebudou dotčeny.").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    m_settingsManager->removeMoveButton(moveButtonId);
    refreshMoveButtons();
}

bool MainWindow::performSingleMove(const QString &filePath, const MoveButtonInfo &button,
                                   MoveGroup &group)
{
    const QFileInfo fileInfo(filePath);
    const QString targetFolder = button.folder;

    if (!QDir(targetFolder).exists()) {
        if (!QDir().mkpath(targetFolder)) {
            m_statusLabel->setText(tr("Nelze vytvořit cílovou složku: %1").arg(targetFolder));
            return false;
        }
    }

    if (!QFileInfo(targetFolder).isWritable()) {
        m_statusLabel->setText(tr("Cílová složka nemá práva pro zápis: %1").arg(targetFolder));
        return false;
    }

    QString targetPath = targetFolder + QStringLiteral("/") + fileInfo.fileName();

    if (QFile::exists(targetPath)) {
        MoveConflictDialog conflictDialog(filePath, targetPath, this);
        if (conflictDialog.exec() != QDialog::Accepted || !conflictDialog.renameConfirmed()) {
            return false;   // uživatel zrušil přesun tohoto souboru
        }
        targetPath = targetFolder + QStringLiteral("/") + conflictDialog.newFileName();
        if (QFile::exists(targetPath)) {
            m_statusLabel->setText(tr("Zvolený název už také existuje, přesun zrušen: %1")
                .arg(fileInfo.fileName()));
            return false;
        }
    }

    if (tryWithRetry([&] { return QFile::rename(filePath, targetPath); })) {
        if (m_categoryManager) {
            m_categoryManager->renameImagePath(filePath, targetPath);
        }
        group.append({targetPath, filePath});
        return true;
    }

    QString reason;
    if (!QFile::exists(filePath)) {
        reason = tr("soubor již neexistuje");
    } else {
        reason = tr("soubor je stále zamčený — zkuste zavřít video a zkusit znovu");
    }
    m_statusLabel->setText(tr("Nepodařilo se přesunout '%1': %2").arg(fileInfo.fileName(), reason));
    return false;
}

QStringList MainWindow::resolveCompanionSet(const QString &activeFile, const QString &verb,
                                            bool &cancelled)
{
    cancelled = false;
    QStringList result;
    result.append(activeFile);

    if (!m_settingsManager->moveCompanionFiles()) {
        return result;   // funkce vypnutá — jen aktivní soubor
    }

    const QStringList companions = CompanionFinder::findCompanions(activeFile);
    if (companions.isEmpty()) {
        return result;                       // 0 párů → jen aktivní
    }
    if (companions.size() == 1) {
        result.append(companions);           // 1 pár → automaticky bez dotazu
        return result;
    }

    // 2+ párů → dotázat se uživatele.
    QStringList companionNames;
    for (const QString &c : companions) {
        companionNames.append(QFileInfo(c).fileName());
    }
    CompanionActionDialog dialog(QFileInfo(activeFile).fileName(), companionNames, verb, this);
    dialog.exec();
    switch (dialog.choice()) {
    case CompanionActionDialog::Choice::All:
        result.append(companions);
        break;
    case CompanionActionDialog::Choice::ActiveOnly:
        break;                               // jen aktivní (result už obsahuje)
    case CompanionActionDialog::Choice::Cancel:
        cancelled = true;
        result.clear();
        break;
    }
    return result;
}

void MainWindow::onMoveButtonClicked(int moveButtonId)
{
    if (m_imagePaths.isEmpty() || m_currentIndex < 0) {
        return;
    }

    const QList<MoveButtonInfo> buttons = m_settingsManager->moveButtons();
    MoveButtonInfo button;
    bool found = false;
    for (const MoveButtonInfo &info : buttons) {
        if (info.id == moveButtonId) {
            button = info;
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }

    const QList<int> selected = m_thumbnailPanel->selectedIndices();
    QStringList filesToMove;
    if (selected.size() > 1) {
        for (int idx : selected) {
            if (idx >= 0 && idx < m_imagePaths.size()) {
                filesToMove.append(m_imagePaths.at(idx));
            }
        }
    } else {
        filesToMove.append(m_imagePaths.at(m_currentIndex));
    }

    int movedCount = 0;
    const int anchorIndex = m_currentIndex;
    bool removedAny = false;
    // Soubor, který už byl přesunut jako pár, přeskočit, pokud je i ve výběru.
    QSet<QString> handled;
    for (const QString &activeFile : filesToMove) {
        if (handled.contains(activeFile)) {
            continue;
        }
        // Soubor už byl mezitím odebrán (např. jako pár předchozího souboru).
        if (!QFile::exists(activeFile)) {
            continue;
        }

        bool cancelled = false;
        const QStringList filesInAction = resolveCompanionSet(activeFile, tr("přesunout"), cancelled);
        if (cancelled) {
            handled.insert(activeFile);   // storno pro tento soubor
            continue;
        }

        MoveGroup group;
        for (const QString &f : filesInAction) {
            // Na Windows se přehrávané video drží v paměti — zastavit PŘED
            // každým pokusem o přesun (video mohlo být auto-přehráno i uvnitř
            // smyčky přechodem na další soubor), jinak by bylo zamčené.
            stopVideoIfPlaying();
            if (performSingleMove(f, button, group)) {
                ++movedCount;
                handled.insert(f);
                // Odebrat z UI jen pokud je pár součástí aktuálního seznamu
                // (pár mimo filtr se jen přesune na disku).
                const int idx = m_imagePaths.indexOf(f);
                if (idx >= 0) {
                    removeImageFromList(idx, /*showNext=*/false);
                    removedAny = true;
                }
            }
        }
        if (!group.isEmpty()) {
            m_moveHistory.append(group);
        }
    }

    if (removedAny) {
        showCurrentAfterRemoval(anchorIndex);
    }
    updateMoveUndoButtonState();
    if (movedCount > 1) {
        m_statusLabel->setText(tr("Přesunuto %1 souborů do '%2'.").arg(movedCount).arg(button.name));
    }
}

void MainWindow::undoLastGroup(QList<MoveGroup> &history, const QString &notFoundMessage)
{
    if (history.isEmpty()) {
        return;
    }

    const MoveGroup group = history.last();

    // Nejdřív ověřit, že žádné z původních umístění není obsazené — jinak by
    // skupina zůstala rozpůlená. Chybějící cíle (smazané externě) přeskočíme.
    // Skupina se v tomto případě NEODEBÍRÁ z historie — uživatel může kolizi
    // vyřešit a undo zopakovat.
    for (const FileMovePair &pair : group) {
        const QString &movedPath    = pair.first;
        const QString &originalPath = pair.second;
        if (QFile::exists(movedPath) && QFile::exists(originalPath)) {
            QMessageBox::warning(this, tr("Nelze vrátit"),
                tr("V původním umístění již soubor '%1' existuje.")
                    .arg(QFileInfo(originalPath).fileName()));
            return;
        }
    }

    QString anyOriginal;
    QStringList failed;
    for (const FileMovePair &pair : group) {
        const QString &movedPath    = pair.first;
        const QString &originalPath = pair.second;
        if (!QFile::exists(movedPath)) {
            continue;   // v cíli chybí (odstraněn externě) — přeskočit
        }
        QDir().mkpath(QFileInfo(originalPath).absolutePath());
        if (tryWithRetry([&] { return QFile::rename(movedPath, originalPath); })) {
            if (m_categoryManager) {
                m_categoryManager->renameImagePath(movedPath, originalPath);
            }
            anyOriginal = originalPath;
        } else {
            failed.append(QFileInfo(movedPath).fileName());
        }
    }

    history.removeLast();
    updateMoveUndoButtonState();
    updateRecycleButtonState();

    if (!failed.isEmpty()) {
        m_statusLabel->setText(tr("Nepodařilo se vrátit: %1").arg(failed.join(QStringLiteral(", "))));
    }

    if (!anyOriginal.isEmpty()) {
        m_requestedFile = anyOriginal;
        loadFolder(QFileInfo(anyOriginal).absolutePath());
    } else if (failed.isEmpty()) {
        m_statusLabel->setText(notFoundMessage);
    }
}

void MainWindow::onUndoMove()
{
    undoLastGroup(m_moveHistory,
                  tr("Přesunuté soubory nenalezeny, byly zřejmě odstraněny externě."));
}

void MainWindow::updateMoveUndoButtonState()
{
    if (m_moveUndoAction) {
        m_moveUndoAction->setEnabled(!m_moveHistory.isEmpty());
    }
}

} // namespace pictureviewer

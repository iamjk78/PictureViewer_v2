// MainWindow_Move.cpp — rychlý přesun obrázku/videa/PDF do vybrané složky
// přes vlastní tlačítka v toolbaru (viz CLAUDE.md / plán funkce "Přesun").
// QPushButton must be included BEFORE MainWindow.hpp to satisfy the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/CategoryManager.hpp"
#include "app/MoveDialogs.hpp"
#include "app/SettingsManager.hpp"
#include "app/ThumbnailPanel.hpp"

#include <QColorDialog>
#include <QCursor>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

namespace {

// Stejná strategie jako u ostatních souborových operací (viz MainWindow_FileOps.cpp):
// Windows Media Foundation uvolňuje handle asynchronně, proto zkoušíme opakovaně.
template <typename Op>
bool tryWithRetry(Op op, int attempts = 8, int delayMs = 250)
{
    for (int i = 0; i < attempts; ++i) {
        if (op()) {
            return true;
        }
        if (i + 1 < attempts) {
            QEventLoop loop;
            QTimer::singleShot(delayMs, &loop, &QEventLoop::quit);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
        }
    }
    return false;
}

// 20 předdefinovaných barev — stejné jako oblíbené/kategorie.
constexpr const char *MovePredefinedColors[] = {
    "#FF6B6B", "#4ECDC4", "#45B7D1", "#FFA07A", "#98D8C8",
    "#F7DC6F", "#BB8FCE", "#85C1E2", "#F8B88B", "#A9DFBF",
    "#F5B7B1", "#D7BDE2", "#F9E79F", "#AED6F1", "#F8B4B8",
    "#B7E8D6", "#FDBFED", "#D4EFDF", "#FADBD8", "#EBD5B4"
};
constexpr int MovePredefinedColorCount = 20;

} // namespace

namespace pictureviewer {

QString MainWindow::pickRandomUnusedMoveColor() const
{
    QStringList used;
    for (const MoveButtonInfo &btn : m_settingsManager->moveButtons()) {
        used.append(btn.color);
    }

    for (int attempt = 0; attempt < MovePredefinedColorCount; ++attempt) {
        int idx = QRandomGenerator::global()->bounded(MovePredefinedColorCount);
        QString colorHex = MovePredefinedColors[idx];
        if (!used.contains(colorHex)) {
            return colorHex;
        }
    }
    int idx = QRandomGenerator::global()->bounded(MovePredefinedColorCount);
    return MovePredefinedColors[idx];
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

        const QColor color(info.color.isEmpty() ? QStringLiteral("#4ECDC4") : info.color);
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

bool MainWindow::performSingleMove(const QString &filePath, const MoveButtonInfo &button)
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
        m_moveHistory.append({targetPath, filePath});
        updateMoveUndoButtonState();
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

    // Na Windows se video drží v paměti — zastavit jej PŘED pokusem o přesun,
    // aby soubor nebyl zamčený (stejná logika jako deleteOrMoveCurrentImage()).
    stopVideoIfPlaying();

    int movedCount = 0;
    for (const QString &filePath : filesToMove) {
        const int idx = m_imagePaths.indexOf(filePath);
        if (idx < 0) {
            continue;   // soubor už byl mezitím odebrán (např. jiným přesunem)
        }
        if (performSingleMove(filePath, button)) {
            ++movedCount;
            removeImageFromList(idx);
        }
    }

    if (movedCount > 1) {
        m_statusLabel->setText(tr("Přesunuto %1 souborů do '%2'.").arg(movedCount).arg(button.name));
    }
}

void MainWindow::onUndoMove()
{
    if (m_moveHistory.isEmpty()) {
        return;
    }

    const auto [targetPath, originalPath] = m_moveHistory.last();

    if (!QFile::exists(targetPath)) {
        m_moveHistory.removeLast();
        updateMoveUndoButtonState();
        m_statusLabel->setText(tr("Přesunutý soubor nenalezen, byl zřejmě odstraněn externě."));
        return;
    }

    if (QFile::exists(originalPath)) {
        QMessageBox::warning(this, tr("Nelze vrátit"),
            tr("V původním umístění již soubor '%1' existuje.")
                .arg(QFileInfo(originalPath).fileName()));
        return;
    }

    QDir().mkpath(QFileInfo(originalPath).absolutePath());

    if (tryWithRetry([&] { return QFile::rename(targetPath, originalPath); })) {
        if (m_categoryManager) {
            m_categoryManager->renameImagePath(targetPath, originalPath);
        }
        m_moveHistory.removeLast();
        updateMoveUndoButtonState();
        m_requestedFile = originalPath;
        loadFolder(QFileInfo(originalPath).absolutePath());
    } else {
        m_statusLabel->setText(tr("Nepodařilo se vrátit soubor zpět."));
    }
}

void MainWindow::updateMoveUndoButtonState()
{
    if (m_moveUndoAction) {
        m_moveUndoAction->setEnabled(!m_moveHistory.isEmpty());
    }
}

} // namespace pictureviewer

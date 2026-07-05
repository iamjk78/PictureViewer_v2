// MainWindow_Labels.cpp — category/label management (štítky)
// QPushButton must be included BEFORE MainWindow.hpp to satisfy the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/CategoryDialogs.hpp"
#include "app/CategoryManager.hpp"
#include "app/SettingsManager.hpp"

#include <QColorDialog>
#include <QCursor>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>

namespace pictureviewer {

void MainWindow::setupCategoriesToolbar()
{
    addToolBarBreak();

    m_categoriesToolbar = addToolBar(tr("Štítky"));
    m_categoriesToolbar->setMovable(false);

    constexpr int ICON_SIZE = 28;
    const QString iconButtonStyle = QStringLiteral(
        "QToolButton { border: 0.5px solid #ccc; border-radius: 3px; "
        "  padding: 2px; min-width: %1px; width: %1px; min-height: %1px; height: %1px; "
        "  background: transparent; font-size: 14px; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); border: 0.5px solid #999; }")
        .arg(ICON_SIZE);

    QAction *newCatAction = m_categoriesToolbar->addAction(QStringLiteral("➕"));
    newCatAction->setToolTip(tr("Vytvořit nový štítek"));
    connect(newCatAction, &QAction::triggered, this, [this] {
        NewCategoryDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            Category cat = m_categoryManager->addCategory(dialog.categoryName(), dialog.selectedColor());
            if (cat.id > 0) {
                refreshCategoryButtons();
                updateCategoryFilterButtons();
            } else if (!m_categoryManager->lastError().isEmpty()) {
                m_statusLabel->setText(tr("Chyba při vytváření štítku: %1")
                    .arg(m_categoryManager->lastError()));
            }
        }
    });
    if (auto *btn = qobject_cast<QToolButton *>(m_categoriesToolbar->widgetForAction(newCatAction))) {
        btn->setStyleSheet(iconButtonStyle);
    }

    m_categoriesToolbar->addSeparator();

    refreshCategoryButtons();

    m_categoriesToolbar->addSeparator();

    QAction *removeAllAction = m_categoriesToolbar->addAction(QStringLiteral("❌"));
    removeAllAction->setToolTip(tr("Odebrat všechny štítky z obrázku"));
    connect(removeAllAction, &QAction::triggered, this, &MainWindow::onCategoryRemoveAll);
    if (auto *btn = qobject_cast<QToolButton *>(m_categoriesToolbar->widgetForAction(removeAllAction))) {
        btn->setStyleSheet(iconButtonStyle);
    }

    m_categoriesToolbar->addSeparator();

    m_categoriesToolbar->addWidget(new QLabel(tr("Filtr:")));

    QAction *clearFiltersAction = m_categoriesToolbar->addAction(QStringLiteral("✕"));
    clearFiltersAction->setToolTip(tr("Odebrat všechny štítky z filtru"));
    connect(clearFiltersAction, &QAction::triggered, this, &MainWindow::clearFilters);
    if (auto *btn = qobject_cast<QToolButton *>(m_categoriesToolbar->widgetForAction(clearFiltersAction))) {
        btn->setStyleSheet(iconButtonStyle);
    }

    m_mainToolbar->addSeparator();
    QAction *toggleCategoriesAction = m_mainToolbar->addAction(QStringLiteral("🏷️"));
    toggleCategoriesAction->setToolTip(tr("Zobrazit/skrýt panel štítků"));
    connect(toggleCategoriesAction, &QAction::triggered, this, [this] {
        bool willBeVisible = !m_categoriesToolbar->isVisible();
        m_categoriesToolbar->setVisible(willBeVisible);
        if (willBeVisible) {
            updateCategoryFilterButtons();
        }
    });

    bool wasVisible = m_settingsManager->categoriesToolbarVisible();
    m_categoriesToolbar->setVisible(wasVisible);
    m_categoriesToolbar->setStyleSheet(iconButtonStyle);
}

void MainWindow::onCategoryRemoveAll()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_imagePaths.size()) {
        return;
    }

    QString imagePath = m_imagePaths.at(m_currentIndex);
    m_categoryManager->unassignAll(imagePath);
    updateStatus(imagePath);
    updateCategoryButtonStates();

    if (m_categoriesToolbar->isVisible()) {
        updateCategoryFilterButtons();
    }
}

void MainWindow::refreshCategoryButtons()
{
    QAction *oldContainerAction = nullptr;
    for (QAction *action : m_categoriesToolbar->actions()) {
        QWidget *widget = m_categoriesToolbar->widgetForAction(action);
        if (widget && widget->objectName() == "categoryButtonsContainer") {
            oldContainerAction = action;
            break;
        }
    }

    for (QPushButton *btn : m_categoryButtons) {
        btn->deleteLater();
    }
    m_categoryButtons.clear();

    if (oldContainerAction) {
        m_categoriesToolbar->removeAction(oldContainerAction);
    }

    QWidget *newContainer = new QWidget(this);
    newContainer->setObjectName("categoryButtonsContainer");
    QHBoxLayout *layout = new QHBoxLayout(newContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QList<Category> allCategories = m_categoryManager->allCategories();

    for (const Category &cat : allCategories) {
        QPushButton *btn = new QPushButton(cat.name);
        btn->setCheckable(true);
        btn->setFlat(false);

        QString colorStr = cat.color.name();
        int lightness = cat.color.lightness();
        QString textColor = lightness > 128 ? "#000000" : "#FFFFFF";

        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 2px solid #ccc;"
            "  border-radius: 4px;"
            "  padding: 2px 6px;"
            "  font-weight: bold;"
            "  font-size: 14px;"
            "  min-height: 30px;"
            "}"
            "QPushButton:checked, QPushButton:pressed {"
            "  border: 3px solid #333;"
            "}"
        ).arg(colorStr, textColor));

        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, this, [this, cat]() {
            QMenu menu;
            menu.addAction(tr("Přejmenovat"), this, [this, cat] { onCategoryRename(cat.id); });
            menu.addAction(tr("Změnit barvu"), this, [this, cat] { onCategoryChangeColor(cat.id); });
            menu.addSeparator();
            menu.addAction(tr("Odstranit"), this, [this, cat] { onCategoryDelete(cat.id); });
            menu.exec(QCursor::pos());
        });

        connect(btn, &QPushButton::clicked, this, [this, cat] {
            onCategoryButtonToggled(cat.id);
        });

        layout->addWidget(btn);
        m_categoryButtons[cat.id] = btn;
    }

    layout->addStretch();

    QList<QAction*> actions = m_categoriesToolbar->actions();
    if (actions.size() >= 2) {
        m_categoriesToolbar->insertWidget(actions.at(1), newContainer);
    } else {
        m_categoriesToolbar->addWidget(newContainer);
    }

    updateCategoryButtonStates();
}

void MainWindow::onCategoryButtonToggled(int categoryId)
{
    if (m_currentIndex < 0 || m_currentIndex >= m_imagePaths.size()) {
        return;
    }

    QString imagePath = m_imagePaths.at(m_currentIndex);
    QPushButton *btn = m_categoryButtons.value(categoryId);
    if (!btn) return;

    QList<Category> currentCategories = m_categoryManager->categoriesForImage(imagePath);
    bool isAssigned = false;
    for (const Category &cat : currentCategories) {
        if (cat.id == categoryId) {
            isAssigned = true;
            break;
        }
    }

    if (isAssigned) {
        m_categoryManager->unassignCategory(imagePath, categoryId);
        btn->setChecked(false);
    } else {
        if (currentCategories.size() >= 5) {
            QMessageBox::warning(this, tr("Upozornění"),
                tr("Nelze přiřadit více než 5 štítků jednomu obrázku."));
            btn->setChecked(false);
            return;
        }
        m_categoryManager->assignCategory(imagePath, categoryId);
        btn->setChecked(true);
    }

    updateStatus(imagePath);

    if (m_categoriesToolbar->isVisible()) {
        updateCategoryFilterButtons();
    }
}

void MainWindow::updateCategoryButtonStates()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_imagePaths.size()) {
        for (QPushButton *btn : m_categoryButtons) {
            btn->setChecked(false);
            btn->setEnabled(false);
        }
        return;
    }

    QString imagePath = m_imagePaths.at(m_currentIndex);
    QList<Category> currentCategories = m_categoryManager->categoriesForImage(imagePath);

    QSet<int> assignedIds;
    for (const Category &cat : currentCategories) {
        assignedIds.insert(cat.id);
    }

    for (auto it = m_categoryButtons.begin(); it != m_categoryButtons.end(); ++it) {
        QPushButton *btn = it.value();
        bool assigned = assignedIds.contains(it.key());
        btn->setChecked(assigned);
        btn->setEnabled(true);
    }
}

void MainWindow::updateCategoryFilterButtons()
{
    QAction *oldContainerAction = nullptr;
    for (QAction *action : m_categoriesToolbar->actions()) {
        QWidget *widget = m_categoriesToolbar->widgetForAction(action);
        if (widget && widget == m_filterButtonsContainer) {
            oldContainerAction = action;
            break;
        }
    }

    for (QPushButton *btn : m_categoryFilterButtons) {
        btn->deleteLater();
    }
    m_categoryFilterButtons.clear();

    if (oldContainerAction) {
        m_categoriesToolbar->removeAction(oldContainerAction);
    }

    QWidget *newContainer = new QWidget(this);
    newContainer->setObjectName("filterButtonsContainer");
    QHBoxLayout *layout = new QHBoxLayout(newContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QList<Category> allCategories = m_categoryManager->categoriesUsedInPaths(m_unfilteredImagePaths);

    for (const Category &cat : allCategories) {
        QPushButton *btn = new QPushButton(cat.name);
        btn->setCheckable(true);
        btn->setFlat(false);
        btn->setToolTip(tr("Filtrovat podle: %1").arg(cat.name));

        QString colorStr = cat.color.name();
        int lightness = cat.color.lightness();
        QString textColor = lightness > 128 ? "#000000" : "#FFFFFF";

        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 2px solid #ccc;"
            "  border-radius: 4px;"
            "  padding: 2px 6px;"
            "  font-weight: bold;"
            "  font-size: 14px;"
            "  min-height: 30px;"
            "}"
            "QPushButton:checked, QPushButton:pressed {"
            "  border: 3px solid #333;"
            "}"
        ).arg(colorStr, textColor));

        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, this, [this, cat]() {
            QMenu menu;
            menu.addAction(tr("Přejmenovat"), this, [this, cat] { onCategoryRename(cat.id); });
            menu.addAction(tr("Změnit barvu"), this, [this, cat] { onCategoryChangeColor(cat.id); });
            menu.addSeparator();
            menu.addAction(tr("Odstranit"), this, [this, cat] { onCategoryDelete(cat.id); });
            menu.exec(QCursor::pos());
        });

        connect(btn, &QPushButton::clicked, this, [this, cat] {
            onCategoryFilterToggled(cat.id);
        });

        if (m_categoryFilterIds.contains(cat.id)) {
            btn->setChecked(true);
        }

        layout->addWidget(btn);
        m_categoryFilterButtons[cat.id] = btn;
    }

    layout->addStretch();

    QList<QAction*> actions = m_categoriesToolbar->actions();
    QAction *afterLabel = nullptr;
    for (int i = 0; i < actions.size(); ++i) {
        if (i > 0 && actions.at(i - 1)->text() == tr("Filtr:")) {
            afterLabel = actions.at(i);
            break;
        }
    }

    if (afterLabel) {
        m_categoriesToolbar->insertWidget(afterLabel, newContainer);
    } else {
        m_categoriesToolbar->addWidget(newContainer);
    }

    m_filterButtonsContainer = newContainer;
}

void MainWindow::onCategoryFilterToggled(int categoryId)
{
    if (m_categoryFilterIds.contains(categoryId)) {
        m_categoryFilterIds.removeAll(categoryId);
    } else {
        m_categoryFilterIds.append(categoryId);
    }

    onCategoryFilterChanged();
}

void MainWindow::clearFilters()
{
    m_categoryFilterIds.clear();

    for (QPushButton *btn : m_categoryFilterButtons) {
        btn->setChecked(false);
    }

    onCategoryFilterChanged();
}

void MainWindow::onCategoryFilterChanged()
{
    if (!m_currentFolder.isEmpty()) {
        loadFolder(m_currentFolder);
    }
}

void MainWindow::updateStatusBarCategories()
{
    if (m_currentIndex >= 0 && m_currentIndex < m_imagePaths.size()) {
        updateStatus(m_imagePaths.at(m_currentIndex));
    }
}

void MainWindow::onCategoryRename(int categoryId)
{
    QList<Category> allCategories = m_categoryManager->allCategories();
    Category currentCategory;
    bool found = false;

    for (const Category &cat : allCategories) {
        if (cat.id == categoryId) {
            currentCategory = cat;
            found = true;
            break;
        }
    }

    if (!found) {
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this,
        tr("Přejmenovat štítek"),
        tr("Nové jméno:"),
        QLineEdit::Normal,
        currentCategory.name,
        &ok);

    if (!ok || newName.trimmed().isEmpty()) {
        return;
    }

    if (!m_categoryManager->updateCategory(categoryId, newName.trimmed(), QColor())) {
        QMessageBox::warning(this, tr("Chyba"),
            tr("Nelze přejmenovat štítek. Možná štítek s tímto jménem již existuje."));
        return;
    }

    refreshCategoryButtons();
    if (m_categoriesToolbar->isVisible()) {
        updateCategoryFilterButtons();
    }
}

void MainWindow::onCategoryChangeColor(int categoryId)
{
    QList<Category> allCategories = m_categoryManager->allCategories();
    Category currentCategory;
    bool found = false;

    for (const Category &cat : allCategories) {
        if (cat.id == categoryId) {
            currentCategory = cat;
            found = true;
            break;
        }
    }

    if (!found) {
        return;
    }

    QColor newColor = QColorDialog::getColor(currentCategory.color, this,
        tr("Vyberte novou barvu"));

    if (!newColor.isValid()) {
        return;
    }

    if (!m_categoryManager->updateCategory(categoryId, QString(), newColor)) {
        QMessageBox::warning(this, tr("Chyba"),
            tr("Nelze změnit barvu štítku."));
        return;
    }

    refreshCategoryButtons();
    if (m_categoriesToolbar->isVisible()) {
        updateCategoryFilterButtons();
    }
}

void MainWindow::onCategoryDelete(int categoryId)
{
    QList<Category> allCategories = m_categoryManager->allCategories();
    Category categoryToDelete;
    bool found = false;

    for (const Category &cat : allCategories) {
        if (cat.id == categoryId) {
            categoryToDelete = cat;
            found = true;
            break;
        }
    }

    if (!found) {
        return;
    }

    int result = QMessageBox::warning(this,
        tr("Potvrzení smazání"),
        tr("Opravdu chcete smazat štítek '%1'?\n"
           "Tím se odebere ze všech obrázků, které ho mají přiřazený.").arg(categoryToDelete.name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    m_categoryManager->deleteCategory(categoryId);
    if (!m_categoryManager->lastError().isEmpty()) {
        m_statusLabel->setText(tr("Chyba při mazání štítku: %1")
            .arg(m_categoryManager->lastError()));
        return;
    }

    refreshCategoryButtons();
    if (m_categoriesToolbar->isVisible()) {
        updateCategoryFilterButtons();
    }

    if (m_currentIndex >= 0 && m_currentIndex < m_imagePaths.size()) {
        updateCategoryButtonStates();
    }
}

} // namespace pictureviewer

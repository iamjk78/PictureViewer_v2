#pragma once

#include "app/CategoryManager.hpp"

#include <QColor>
#include <QDialog>

class QCheckBox;
class QColorDialog;
class QLineEdit;
class QPushButton;

namespace pictureviewer {

// Dialog pro vytvoření nové kategorie (název + výběr barvy ze 20 možností)
class NewCategoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewCategoryDialog(QWidget *parent = nullptr);

    QString categoryName() const;
    QColor selectedColor() const;

private:
    void setupUI();

    QLineEdit *m_nameEdit;
    QPushButton *m_colorButtons[20];
    QColor m_selectedColor;
};

// Dialog pro přiřazení kategorií obrázku (seznam checkboxů)
// Zvýrazní kategorie, které má obrázek už přiřazené
class AssignCategoriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AssignCategoriesDialog(
        const QList<Category> &allCategories,
        const QList<Category> &currentCategories,
        QWidget *parent = nullptr
    );

    QList<int> selectedCategoryIds() const;

private:
    void setupUI();

    QList<QCheckBox *> m_checkboxes;
    QList<int> m_categoryIds;
};

} // namespace pictureviewer

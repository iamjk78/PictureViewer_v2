#include "app/CategoryDialogs.hpp"

#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QVBoxLayout>

namespace pictureviewer {

// 20 předdefinovaných barev (hex) — stejné jako v CategoryManager
static constexpr const char *PredefinedColors[] = {
    "#FF6B6B", "#4ECDC4", "#45B7D1", "#FFA07A", "#98D8C8",
    "#F7DC6F", "#BB8FCE", "#85C1E2", "#F8B88B", "#A9DFBF",
    "#F5B7B1", "#D7BDE2", "#F9E79F", "#AED6F1", "#F8B4B8",
    "#B7E8D6", "#FDBFED", "#D4EFDF", "#FADBD8", "#EBD5B4"
};

// ────────────────────────────────────────────────────────────────────────────
// NewCategoryDialog
// ────────────────────────────────────────────────────────────────────────────

NewCategoryDialog::NewCategoryDialog(QWidget *parent)
    : QDialog(parent)
    , m_selectedColor(PredefinedColors[0])
{
    setWindowTitle(tr("Nová kategorie"));
    setModal(true);
    setMinimumWidth(500);
    setupUI();
}

void NewCategoryDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Jméno kategorie
    mainLayout->addWidget(new QLabel(tr("Jméno kategorie:")));
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(tr("Např. Dovolená"));
    mainLayout->addWidget(m_nameEdit);

    mainLayout->addSpacing(15);

    // Výběr barvy — mřížka tlačítek
    mainLayout->addWidget(new QLabel(tr("Vyberte barvu:")));

    QWidget *colorWidget = new QWidget();
    QGridLayout *colorLayout = new QGridLayout(colorWidget);
    colorLayout->setSpacing(5);

    for (int i = 0; i < 20; ++i) {
        QPushButton *btn = new QPushButton();
        btn->setFixedSize(40, 40);
        btn->setStyleSheet(
            QString("background-color: %1; border: 2px solid #ccc; border-radius: 4px;")
                .arg(PredefinedColors[i])
        );

        connect(btn, &QPushButton::clicked, this, [this, i] {
            m_selectedColor = QColor(PredefinedColors[i]);
            // Aktualizovat vizuál — zvýraznit vybranou
            for (int j = 0; j < 20; ++j) {
                QString style = QString("background-color: %1; border: %2px solid %3; border-radius: 4px;")
                    .arg(PredefinedColors[j])
                    .arg(i == j ? 3 : 2)
                    .arg(i == j ? "#333" : "#ccc");
                m_colorButtons[j]->setStyleSheet(style);
            }
        });

        m_colorButtons[i] = btn;
        colorLayout->addWidget(btn, i / 5, i % 5);
    }

    // Zvýraznit výchozí barvu
    m_colorButtons[0]->setStyleSheet(
        QString("background-color: %1; border: 3px solid #333; border-radius: 4px;")
            .arg(PredefinedColors[0])
    );

    mainLayout->addWidget(colorWidget);

    mainLayout->addSpacing(15);

    // Tlačítka OK/Cancel
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(tr("OK"));
    QPushButton *cancelBtn = new QPushButton(tr("Zrušit"));

    connect(okBtn, &QPushButton::clicked, this, [this] {
        if (m_nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Chyba"), tr("Zadejte jméno kategorie"));
            return;
        }
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);
}

QString NewCategoryDialog::categoryName() const
{
    return m_nameEdit->text().trimmed();
}

QColor NewCategoryDialog::selectedColor() const
{
    return m_selectedColor;
}

// ────────────────────────────────────────────────────────────────────────────
// AssignCategoriesDialog
// ────────────────────────────────────────────────────────────────────────────

AssignCategoriesDialog::AssignCategoriesDialog(
    const QList<Category> &allCategories,
    const QList<Category> &currentCategories,
    QWidget *parent
)
    : QDialog(parent)
{
    setWindowTitle(tr("Přiřadit kategorie"));
    setModal(true);
    setMinimumWidth(400);

    // Vytvořit set ID aktuálních kategorií pro rychlé vyhledávání
    QSet<int> currentIds;
    for (const Category &cat : currentCategories) {
        currentIds.insert(cat.id);
    }

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel(tr("Vyberte kategorie (max 5):")));

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *listLayout = new QVBoxLayout(scrollWidget);

    for (const Category &cat : allCategories) {
        QCheckBox *chk = new QCheckBox(cat.name);
        chk->setChecked(currentIds.contains(cat.id));

        // Ikona barvy
        chk->setIcon(QIcon());  // Alternativně by se dala vytvořit pixmapa s barvou

        m_checkboxes.append(chk);
        m_categoryIds.append(cat.id);
        listLayout->addWidget(chk);
    }

    listLayout->addStretch();
    mainLayout->addWidget(scrollWidget);

    // Tlačítka
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(tr("OK"));
    QPushButton *cancelBtn = new QPushButton(tr("Zrušit"));

    connect(okBtn, &QPushButton::clicked, this, [this] {
        int count = 0;
        for (QCheckBox *chk : m_checkboxes) {
            if (chk->isChecked()) ++count;
        }
        if (count > 5) {
            QMessageBox::warning(this, tr("Chyba"), tr("Lze vybrat maximálně 5 kategorií"));
            return;
        }
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);
}

QList<int> AssignCategoriesDialog::selectedCategoryIds() const
{
    QList<int> result;
    for (int i = 0; i < m_checkboxes.size(); ++i) {
        if (m_checkboxes[i]->isChecked()) {
            result.append(m_categoryIds[i]);
        }
    }
    return result;
}

} // namespace pictureviewer

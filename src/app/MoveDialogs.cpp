#include "app/MoveDialogs.hpp"

#include "app/PredefinedColors.hpp"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace pictureviewer {

// ────────────────────────────────────────────────────────────────────────────
// NewMoveButtonDialog
// ────────────────────────────────────────────────────────────────────────────

NewMoveButtonDialog::NewMoveButtonDialog(QWidget *parent)
    : QDialog(parent)
    , m_selectedColor()   // invalidní — pokud uživatel nezvolí, přiřadí se náhodně
{
    setWindowTitle(tr("Nové tlačítko přesunu"));
    setModal(true);
    setMinimumWidth(500);
    setupUI();
}

void NewMoveButtonDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel(tr("Název tlačítka:")));
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(tr("Např. Krajina"));
    mainLayout->addWidget(m_nameEdit);

    mainLayout->addSpacing(10);

    mainLayout->addWidget(new QLabel(tr("Cílová složka:")));
    m_folderButton = new QPushButton(tr("Vybrat složku…"));
    connect(m_folderButton, &QPushButton::clicked, this, &NewMoveButtonDialog::pickFolder);
    mainLayout->addWidget(m_folderButton);

    mainLayout->addSpacing(15);

    mainLayout->addWidget(new QLabel(tr("Vyberte barvu (nepovinné):")));

    QWidget *colorWidget = new QWidget();
    QGridLayout *colorLayout = new QGridLayout(colorWidget);
    colorLayout->setSpacing(5);

    for (int i = 0; i < 20; ++i) {
        QPushButton *btn = new QPushButton();
        btn->setFixedSize(40, 40);
        btn->setStyleSheet(
            QString("background-color: %1; border: 2px solid #ccc; border-radius: 4px;")
                .arg(kPredefinedColors[i])
        );

        connect(btn, &QPushButton::clicked, this, [this, i] {
            m_selectedColor = QColor(kPredefinedColors[i]);
            for (int j = 0; j < 20; ++j) {
                QString style = QString("background-color: %1; border: %2px solid %3; border-radius: 4px;")
                    .arg(kPredefinedColors[j])
                    .arg(i == j ? 3 : 2)
                    .arg(i == j ? "#333" : "#ccc");
                m_colorButtons[j]->setStyleSheet(style);
            }
        });

        m_colorButtons[i] = btn;
        colorLayout->addWidget(btn, i / 5, i % 5);
    }

    mainLayout->addWidget(colorWidget);

    mainLayout->addSpacing(15);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(tr("OK"));
    QPushButton *cancelBtn = new QPushButton(tr("Zrušit"));

    connect(okBtn, &QPushButton::clicked, this, [this] {
        if (m_nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Chyba"), tr("Zadejte název tlačítka"));
            return;
        }
        if (m_selectedFolder.isEmpty()) {
            QMessageBox::warning(this, tr("Chyba"), tr("Vyberte cílovou složku"));
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

void NewMoveButtonDialog::pickFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Vybrat cílovou složku"));
    if (!folder.isEmpty()) {
        m_selectedFolder = folder;
        m_folderButton->setText(folder);
    }
}

QString NewMoveButtonDialog::buttonName() const
{
    return m_nameEdit->text().trimmed();
}

QColor NewMoveButtonDialog::selectedColor() const
{
    return m_selectedColor;
}

QString NewMoveButtonDialog::selectedFolder() const
{
    return m_selectedFolder;
}

// ────────────────────────────────────────────────────────────────────────────
// MoveConflictDialog
// ────────────────────────────────────────────────────────────────────────────

namespace {

QString suggestUniqueFileName(const QString &targetFolder, const QString &originalName)
{
    const QFileInfo info(originalName);
    const QString baseName = info.completeBaseName();
    const QString suffix = info.suffix();

    for (int n = 1; ; ++n) {
        const QString candidate = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(baseName).arg(n)
            : QStringLiteral("%1 (%2).%3").arg(baseName).arg(n).arg(suffix);
        if (!QFileInfo::exists(targetFolder + QStringLiteral("/") + candidate)) {
            return candidate;
        }
    }
}

QString fileInfoSummary(const QFileInfo &info)
{
    const double sizeKb = info.size() / 1024.0;
    const QDateTime created = info.birthTime().isValid() ? info.birthTime() : info.lastModified();
    return QObject::tr("Velikost: %1 kB   |   Vytvořeno: %2")
        .arg(QString::number(sizeKb, 'f', 1))
        .arg(created.toString("dd.MM.yyyy HH:mm"));
}

} // namespace

MoveConflictDialog::MoveConflictDialog(const QString &sourcePath, const QString &targetPath,
                                        QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Soubor již existuje"));
    setModal(true);
    setMinimumWidth(440);
    setupUI(sourcePath, targetPath);
}

void MoveConflictDialog::setupUI(const QString &sourcePath, const QString &targetPath)
{
    const QFileInfo sourceInfo(sourcePath);
    const QFileInfo targetInfo(targetPath);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel(
        tr("Soubor '%1' už v cílové složce existuje.").arg(sourceInfo.fileName())));

    mainLayout->addSpacing(10);

    QLabel *sourceLabel = new QLabel(tr("<b>Přesouvaný soubor</b><br>%1").arg(fileInfoSummary(sourceInfo)));
    mainLayout->addWidget(sourceLabel);

    mainLayout->addSpacing(6);

    QLabel *targetLabel = new QLabel(tr("<b>Existující soubor v cíli</b><br>%1").arg(fileInfoSummary(targetInfo)));
    mainLayout->addWidget(targetLabel);

    mainLayout->addSpacing(15);

    mainLayout->addWidget(new QLabel(tr("Nový název (pokud přejmenujete):")));
    m_nameEdit = new QLineEdit(suggestUniqueFileName(targetInfo.absolutePath(), sourceInfo.fileName()));
    mainLayout->addWidget(m_nameEdit);

    mainLayout->addSpacing(15);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *renameBtn = new QPushButton(tr("Přejmenovat a přesunout"));
    QPushButton *cancelBtn = new QPushButton(tr("Zrušit přesun"));

    connect(renameBtn, &QPushButton::clicked, this, [this] {
        if (m_nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Chyba"), tr("Zadejte název souboru"));
            return;
        }
        m_renameConfirmed = true;
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this] {
        m_renameConfirmed = false;
        reject();
    });

    buttonLayout->addStretch();
    buttonLayout->addWidget(renameBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);
}

QString MoveConflictDialog::newFileName() const
{
    return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

// ────────────────────────────────────────────────────────────────────────────
// CompanionActionDialog
// ────────────────────────────────────────────────────────────────────────────

CompanionActionDialog::CompanionActionDialog(const QString &activeName,
                                             const QStringList &companionNames,
                                             const QString &verb, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Nalezeny související soubory"));
    setModal(true);
    setMinimumWidth(440);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel(
        tr("K souboru '%1' byly ve stejné složce nalezeny související soubory:")
            .arg(activeName)));

    QLabel *listLabel = new QLabel(QStringLiteral("• ") + companionNames.join(QStringLiteral("\n• ")));
    listLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    listLabel->setStyleSheet(QStringLiteral("margin-left: 8px; font-weight: bold;"));
    mainLayout->addWidget(listLabel);

    mainLayout->addSpacing(10);
    mainLayout->addWidget(new QLabel(tr("Co si přejete udělat?")));
    mainLayout->addSpacing(10);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *allBtn    = new QPushButton(tr("%1 vše").arg(verb.left(1).toUpper() + verb.mid(1)));
    QPushButton *activeBtn = new QPushButton(tr("Jen '%1'").arg(activeName));
    QPushButton *cancelBtn = new QPushButton(tr("Storno"));

    connect(allBtn, &QPushButton::clicked, this, [this] {
        m_choice = Choice::All;
        accept();
    });
    connect(activeBtn, &QPushButton::clicked, this, [this] {
        m_choice = Choice::ActiveOnly;
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this] {
        m_choice = Choice::Cancel;
        reject();
    });

    buttonLayout->addStretch();
    buttonLayout->addWidget(allBtn);
    buttonLayout->addWidget(activeBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);
}

} // namespace pictureviewer

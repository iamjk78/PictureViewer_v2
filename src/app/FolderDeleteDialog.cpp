#include "app/FolderDeleteDialog.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace pictureviewer {

FolderDeleteConfirmDialog::FolderDeleteConfirmDialog(const QString &folderPath,
                                                      const QStringList &entries,
                                                      QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Smazat složku"));
    setModal(true);
    setMinimumWidth(460);
    setMinimumHeight(360);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *pathLabel = new QLabel(tr("<b>Smazat celou složku (do koše)?</b><br>%1").arg(folderPath));
    pathLabel->setWordWrap(true);
    mainLayout->addWidget(pathLabel);

    mainLayout->addSpacing(10);
    mainLayout->addWidget(new QLabel(tr("Obsahuje %1 položek:", "", entries.size()).arg(entries.size())));

    QListWidget *list = new QListWidget(this);
    list->addItems(entries);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    mainLayout->addWidget(list, /*stretch=*/1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal);
    QPushButton *deleteBtn = buttonBox->addButton(tr("Smazat včetně souborů"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = buttonBox->addButton(tr("Zrušit, nic nemazat"), QDialogButtonBox::RejectRole);
    deleteBtn->setStyleSheet(QStringLiteral("QPushButton { font-weight: bold; }"));

    connect(deleteBtn, &QPushButton::clicked, this, [this] {
        m_confirmed = true;
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this] {
        m_confirmed = false;
        reject();
    });

    mainLayout->addWidget(buttonBox);
}

} // namespace pictureviewer

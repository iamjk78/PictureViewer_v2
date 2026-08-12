#pragma once

#include <QDialog>
#include <QStringList>

namespace pictureviewer {

// Potvrzovací dialog pro smazání CELÉ aktuální složky (do koše) — zobrazí
// cestu a seznam jejího obsahu (top-level položky), nikdy nenabízí přepsání
// ani trvalé smazání bez koše.
class FolderDeleteConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    // entries: názvy top-level položek ve složce (soubory i podsložky),
    // setříděné; podsložky jsou v seznamu odlišeny předponou (viz .cpp).
    FolderDeleteConfirmDialog(const QString &folderPath, const QStringList &entries,
                              QWidget *parent = nullptr);

    bool confirmed() const { return m_confirmed; }

private:
    bool m_confirmed = false;
};

} // namespace pictureviewer

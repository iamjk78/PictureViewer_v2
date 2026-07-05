#pragma once

#include <QColor>
#include <QDateTime>
#include <QDialog>

class QLineEdit;
class QPushButton;

namespace pictureviewer {

// Dialog pro vytvoření nového tlačítka přesunu — název, barva (volitelná,
// jinak náhodná z nepoužitých) a cílová složka.
class NewMoveButtonDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewMoveButtonDialog(QWidget *parent = nullptr);

    QString buttonName() const;
    QColor selectedColor() const;   // invalidní = nevybráno, přiřadí se náhodně
    QString selectedFolder() const;

private:
    void setupUI();
    void pickFolder();

    QLineEdit *m_nameEdit;
    QPushButton *m_folderButton;
    QPushButton *m_colorButtons[20];
    QColor m_selectedColor;
    QString m_selectedFolder;
};

// Dialog při kolizi jmen — zobrazí velikost a datum vytvoření obou souborů,
// nabídne přejmenování (s návrhem nového jména) nebo zrušení přesunu.
// Nikdy nenabízí přepsání.
class MoveConflictDialog : public QDialog
{
    Q_OBJECT

public:
    MoveConflictDialog(const QString &sourcePath, const QString &targetPath,
                        QWidget *parent = nullptr);

    // True, pokud uživatel zvolil přejmenování (viz newFileName()).
    // False = zrušit přesun tohoto souboru.
    bool renameConfirmed() const { return m_renameConfirmed; }
    QString newFileName() const;

private:
    void setupUI(const QString &sourcePath, const QString &targetPath);

    QLineEdit *m_nameEdit = nullptr;
    bool m_renameConfirmed = false;
};

} // namespace pictureviewer

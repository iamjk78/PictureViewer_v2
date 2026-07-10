#pragma once

#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QStringList>

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

// Dialog při nálezu více párových souborů (obrázek/video se stejným názvem).
// Vypíše nalezené soubory a nechá uživatele zvolit, zda akci provést se všemi,
// jen s aktivním souborem, nebo ji stornovat.
class CompanionActionDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Choice { All, ActiveOnly, Cancel };

    // verb: sloveso akce v infinitivu, např. "přesunout" nebo "smazat".
    CompanionActionDialog(const QString &activeName, const QStringList &companionNames,
                          const QString &verb, QWidget *parent = nullptr);

    Choice choice() const { return m_choice; }

private:
    Choice m_choice = Choice::Cancel;
};

} // namespace pictureviewer

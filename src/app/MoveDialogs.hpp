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

// Dialog při kolizi jmen — zobrazí velikost, datum vytvoření a zdrojovou i
// cílovou složku, nabídne přejmenování (s návrhem nového jména) nebo zrušení
// přesunu. Nikdy nenabízí přepsání.
//
// Když jsou v akci i párové soubory (companionPaths), dialog kontroluje kolizi
// pro AKTIVNÍ soubor i pro KAŽDÝ pár (i když aktivní sám o sobě nekoliduje —
// stačí, že koliduje jeden z párů) a vypíše stav všech. Přejmenování i zrušení
// je vždy jedna volba pro celou skupinu — nový základ jména (stejný, jen jiná
// přípona) se použije na aktivní soubor i na všechny páry, aby si zachovaly
// shodný název; žádný další dotaz per-soubor se už nezobrazuje.
class MoveConflictDialog : public QDialog
{
    Q_OBJECT

public:
    // isBatch: true, když se přesouvá víc než jeden vybraný soubor najednou —
    // pak dialog nabídne i "Zrušit vše" pro přerušení celé zbývající fronty
    // (ne jen skupiny aktivní soubor + páry).
    MoveConflictDialog(const QString &activePath, const QString &targetFolder,
                        const QStringList &companionPaths = {}, bool isBatch = false,
                        QWidget *parent = nullptr);

    // True, pokud uživatel zvolil přejmenování (viz newFileName()).
    // False = zrušit přesun této skupiny (aktivní soubor i páry) — viz abortBatch().
    bool renameConfirmed() const { return m_renameConfirmed; }
    // Nový název AKTIVNÍHO souboru (s jeho příponou) — základ jména (bez přípony)
    // se použije i pro páry, každý se svou vlastní příponou.
    QString newFileName() const;

    // Relevantní jen když renameConfirmed() == false a isBatch bylo true:
    // True u "Zrušit vše" — přeruší se i zpracování zbývajících vybraných
    // souborů ve frontě. False u "Zrušit" — zruší se jen tato skupina,
    // fronta pokračuje dalším souborem.
    bool abortBatch() const { return m_abortBatch; }

private:
    void setupUI(const QString &activePath, const QString &targetFolder,
                const QStringList &companionPaths, bool isBatch);

    QLineEdit *m_nameEdit = nullptr;
    bool m_renameConfirmed = false;
    bool m_abortBatch = false;
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

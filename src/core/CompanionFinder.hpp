#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace pictureviewer {

// Vyhledávání „párových" souborů — souborů ve STEJNÉ složce se stejným základem
// názvu (completeBaseName, case-insensitive), jinou příponou, které jsou obrázek
// NEBO video (nikdy ne PDF). Slouží pro funkci „přesouvat/mazat i párové soubory".
class CompanionFinder
{
public:
    // Vrátí absolutní cesty párových souborů k danému souboru. Prázdné, pokud je
    // zdroj PDF (PDF se nikdy nepáruje) nebo žádný pár neexistuje. Sebe sama
    // nezahrnuje. Výsledek je locale-aware setříděný pro determinismus.
    static QStringList findCompanions(const QString &filePath);
};

// Totéž párování, ale z už známého seznamu souborů — BEZ jakéhokoli čtení
// z úložiště. findCompanions() dělá pro každý soubor desítky dotazů na
// existenci; na síťovém disku to při hromadném mazání znamenalo ~30 síťových
// dotazů na soubor (naměřeno ~15 s na soubor). Index se sestaví jednou pro
// celou dávku a pak odpovídá okamžitě.
//
// Výsledek je stejný jako u findCompanions() (stejná pravidla i řazení), ale
// jen pro soubory, které v seznamu JSOU — soubor, který se objevil na disku
// až po sestavení seznamu, nenajde. Volající proto index používá jen tehdy,
// když je seznam důvěryhodně úplný.
class CompanionIndex
{
public:
    // knownFiles: absolutní cesty (mohou být z různých složek; páruje se jen
    // v rámci téže složky).
    static CompanionIndex build(const QStringList &knownFiles);

    QStringList companionsOf(const QString &filePath) const;

private:
    // klíč: složka + '\n' + základ názvu malými písmeny
    QHash<QString, QStringList> m_byKey;
};

} // namespace pictureviewer

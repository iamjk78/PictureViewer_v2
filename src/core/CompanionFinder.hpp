#pragma once

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

} // namespace pictureviewer

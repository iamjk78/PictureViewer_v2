#pragma once

#include <QString>

#include <functional>

namespace pictureviewer {

// Metadata souboru získaná přímo z výpisu složky (bez stat() na každý soubor).
struct ListedFile {
    QString name;
    qint64 size = 0;
    qint64 mtimeSecs = 0;
};

// Výpis běžných souborů ve složce (bez skrytých). Callback vrací false =
// přerušit výpis.
//
// macOS: getattrlistbulk vrací stovky položek i s velikostí a časem změny na
// jedno volání; klasické readdir přes SMB dává jen ~9 položek na jednu
// síťovou odpověď a velikost/čas si pak žádá zvlášť. Ostatní platformy (a
// souborové systémy bez podpory getattrlistbulk) používají QDirIterator,
// který na Windows metadata dostává s výpisem také.
//
// Vrací false, jen když se složku nepodařilo otevřít.
bool listFilesBulk(const QString &directory,
                   const std::function<bool(const ListedFile &)> &onFile);

// Vynutí záložní cestu (QDirIterator) — pro testy.
void setBulkListingDisabledForTesting(bool disabled);

} // namespace pictureviewer

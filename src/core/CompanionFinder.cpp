#include "core/CompanionFinder.hpp"

#include "core/Collation.hpp"
#include "core/ImageFormats.hpp"

#include <QDir>
#include <QFileInfo>

namespace pictureviewer {

QStringList CompanionFinder::findCompanions(const QString &filePath)
{
    const QFileInfo sourceInfo(filePath);
    const QString sourceSuffix = QStringLiteral(".") + sourceInfo.suffix();

    // PDF se nikdy nepáruje — ani jako zdroj, ani jako cíl.
    if (isPdfFile(sourceSuffix)) {
        return {};
    }

    const QString base = sourceInfo.completeBaseName();
    const QDir dir = sourceInfo.absoluteDir();

    // Množina možných přípon párů (obrázek/video) je předem známá — místo
    // výpisu CELÉ složky (QDir::entryInfoList, O(počet souborů ve složce))
    // stačí ověřit existenci pár desítek konkrétních kandidátních cest
    // (O(počet přípon), konstantní). Výpis celé složky se volal při KAŽDÉM
    // mazání/přejmenování/přesunu se zapnutým párováním — ve velké nebo
    // síťové složce byl citelně pomalý.
    const QStringList candidateExtensions = supportedImageExtensions() + supportedVideoExtensions();

    QStringList companions;
    for (const QString &ext : candidateExtensions) {
        // Stejná přípona jako zdroj by dala IDENTICKÝ název souboru (stejný
        // základ + stejná přípona = stejný soubor) — nemůže jít o pár, jen
        // o zdroj sám; přeskočit bez zbytečného stat().
        if (ext.compare(sourceSuffix, Qt::CaseInsensitive) == 0) {
            continue;
        }
        const QFileInfo candidate(dir.filePath(base + ext));
        if (candidate.exists() && candidate.isFile()) {
            // canonicalFilePath(), ne absoluteFilePath() — na case-insensitive
            // systému souborů (macOS/Windows) najde exists() soubor i přes
            // kandidátní cestu s "vymyšleným" velikostí písmen (základ zdroje +
            // přípona ze seznamu); vrátit se musí skutečný název na disku.
            companions.append(candidate.canonicalFilePath());
        }
    }

    // Locale-aware řazení (sdílený collator — viz core/Collation.hpp).
    const QCollator collator = makeNaturalCollator();
    std::sort(companions.begin(), companions.end(),
              [&collator](const QString &a, const QString &b) {
                  return collator.compare(a, b) < 0;
              });

    return companions;
}

} // namespace pictureviewer

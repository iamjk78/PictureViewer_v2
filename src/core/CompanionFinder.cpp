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
            // Skutečný NÁZEV souboru na disku — na case-insensitive systému
            // souborů (macOS/Windows) najde exists() soubor i přes kandidátní
            // cestu s "vymyšleným" velikostí písmen (základ zdroje + přípona ze
            // seznamu). Složka ale zůstává v podobě, v jaké ji dostal zdroj:
            // canonicalFilePath() by přeložil symbolické odkazy (/var →
            // /private/var) a cesta by pak neodpovídala seznamu v aplikaci, takže
            // by se pár nenašel přes m_imagePaths.indexOf().
            companions.append(dir.absoluteFilePath(QFileInfo(candidate.canonicalFilePath()).fileName()));
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

namespace {

QString indexKey(const QFileInfo &info)
{
    return info.absolutePath() + QLatin1Char('\n') + info.completeBaseName().toLower();
}

bool isPairableSuffix(const QString &suffix)
{
    const QString dotted = QStringLiteral(".") + suffix;
    return isSupportedImageExtension(dotted) || isVideoFile(dotted);
}

} // namespace

CompanionIndex CompanionIndex::build(const QStringList &knownFiles)
{
    CompanionIndex index;
    for (const QString &path : knownFiles) {
        const QFileInfo info(path);   // jen rozbor řetězce, na úložiště se nesahá
        if (!isPairableSuffix(info.suffix())) {
            continue;   // PDF a ostatní se nepáruje
        }
        index.m_byKey[indexKey(info)].append(path);
    }
    return index;
}

QStringList CompanionIndex::companionsOf(const QString &filePath) const
{
    const QFileInfo source(filePath);
    const QString sourceSuffix = QStringLiteral(".") + source.suffix();

    // PDF se nikdy nepáruje — ani jako zdroj, ani jako cíl.
    if (isPdfFile(sourceSuffix)) {
        return {};
    }

    QStringList companions;
    for (const QString &candidate : m_byKey.value(indexKey(source))) {
        // Stejná přípona = zdroj sám (stejná logika jako findCompanions()).
        if (QFileInfo(candidate).suffix().compare(source.suffix(), Qt::CaseInsensitive) == 0) {
            continue;
        }
        companions.append(candidate);
    }

    const QCollator collator = makeNaturalCollator();
    std::sort(companions.begin(), companions.end(),
              [&collator](const QString &a, const QString &b) {
                  return collator.compare(a, b) < 0;
              });
    return companions;
}

} // namespace pictureviewer

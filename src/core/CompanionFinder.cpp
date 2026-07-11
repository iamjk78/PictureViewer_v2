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
    const QString sourceCanonical = sourceInfo.absoluteFilePath();
    const QDir dir = sourceInfo.absoluteDir();

    QStringList companions;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (entry.absoluteFilePath() == sourceCanonical) {
            continue;   // sebe sama nezahrnovat
        }
        if (entry.completeBaseName().compare(base, Qt::CaseInsensitive) != 0) {
            continue;   // jiný základ názvu
        }
        const QString suffix = QStringLiteral(".") + entry.suffix();
        // Pár je jen obrázek nebo video; PDF a ostatní se ignoruje.
        if (isSupportedImageExtension(suffix) || isVideoFile(suffix)) {
            companions.append(entry.absoluteFilePath());
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

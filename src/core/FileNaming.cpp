#include "core/FileNaming.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace pictureviewer::filenaming {

QStringList groupTargetPaths(const QStringList &files,
                             const QString &targetFolder,
                             const QString &newBaseName)
{
    QStringList targets;
    targets.reserve(files.size());

    const QDir dir(targetFolder);
    for (const QString &file : files) {
        // suffix(), ne completeSuffix() — u „dovolena.2024.01.jpg" je příponou
        // jen „jpg"; zbytek patří k názvu. Stejnou konvenci používá
        // CompanionFinder (completeBaseName), takže se skupina spáruje i po
        // přejmenování.
        const QString suffix = QFileInfo(file).suffix();
        const QString name = suffix.isEmpty() ? newBaseName
                                              : newBaseName + QLatin1Char('.') + suffix;
        targets.append(dir.filePath(name));
    }
    return targets;
}

QStringList groupTargetPaths(const QStringList &files, const QString &targetFolder)
{
    QStringList targets;
    targets.reserve(files.size());

    const QDir dir(targetFolder);
    for (const QString &file : files) {
        targets.append(dir.filePath(QFileInfo(file).fileName()));
    }
    return targets;
}

QString firstExistingTarget(const QStringList &targetPaths)
{
    for (const QString &path : targetPaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return {};
}

} // namespace pictureviewer::filenaming

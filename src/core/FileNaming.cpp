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

QString firstExistingTarget(const QStringList &targetPaths, const QStringList &sourcePaths)
{
    for (int i = 0; i < targetPaths.size(); ++i) {
        const QString &path = targetPaths.at(i);
        if (!QFile::exists(path)) {
            continue;
        }
        if (i < sourcePaths.size() && QFileInfo(sourcePaths.at(i)) == QFileInfo(path)) {
            continue;   // cíl je tentýž soubor (jen jiná velikost písmen)
        }
        return path;
    }
    return {};
}

bool isValidBaseName(const QString &name)
{
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..")) {
        return false;
    }
    for (const QChar c : name) {
        if (c == QLatin1Char('/') || c == QLatin1Char('\\') || c == QLatin1Char(':') || c.unicode() < 0x20) {
            return false;
        }
#ifdef Q_OS_WIN
        if (QStringLiteral("<>\"|?*").contains(c)) {
            return false;
        }
#endif
    }
#ifdef Q_OS_WIN
    if (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) {
        return false;
    }
#endif
    return true;
}

bool renameFile(const QString &from, const QString &to)
{
    if (from != to && QFileInfo(from) == QFileInfo(to)) {
        // Jen velikost písmen: oklikou přes dočasný název v téže složce.
        const QString temp = from + QStringLiteral(".pv-rename-tmp");
        if (!QFile::rename(from, temp)) {
            return false;
        }
        if (QFile::rename(temp, to)) {
            return true;
        }
        QFile::rename(temp, from);   // vrátit původní stav
        return false;
    }
    return QFile::rename(from, to);
}

} // namespace pictureviewer::filenaming

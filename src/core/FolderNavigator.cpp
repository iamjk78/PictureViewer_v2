#include "core/FolderNavigator.hpp"

#include "core/Collation.hpp"

#include <QDir>
#include <QFileInfo>

namespace {

using namespace pictureviewer;

bool isDeleteFolder(const QString &name)
{
    return QString::compare(name, QStringLiteral("Delete"), Qt::CaseInsensitive) == 0;
}

// Vrátí podsložky daného adresáře, locale-aware setříděné, bez "Delete"
// (sdílený collator — viz core/Collation.hpp).
QStringList sortedSubfolders(const QDir &dir)
{
    QStringList names = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
    names.removeIf(isDeleteFolder);

    const QCollator collator = makeNaturalCollator();
    std::sort(names.begin(), names.end(), [&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    });
    return names;
}

} // namespace

namespace pictureviewer {

FolderNavResult FolderNavigator::siblingBefore(const QString &currentFolder)
{
    const QFileInfo info(currentFolder);
    QDir parentDir = info.dir();

    const QStringList siblings = sortedSubfolders(parentDir);
    const int idx = siblings.indexOf(info.fileName());
    if (idx < 0) {
        return {};
    }

    FolderNavResult result;
    result.count = idx;   // počet sourozenců před aktuální složkou
    if (idx > 0) {
        result.name = siblings.at(idx - 1);
        result.path = parentDir.absoluteFilePath(result.name);
        result.available = true;
    }
    return result;
}

FolderNavResult FolderNavigator::siblingAfter(const QString &currentFolder)
{
    const QFileInfo info(currentFolder);
    QDir parentDir = info.dir();

    const QStringList siblings = sortedSubfolders(parentDir);
    const int idx = siblings.indexOf(info.fileName());
    if (idx < 0) {
        return {};
    }

    FolderNavResult result;
    result.count = siblings.size() - idx - 1;   // počet sourozenců za aktuální složkou
    if (idx + 1 < siblings.size()) {
        result.name = siblings.at(idx + 1);
        result.path = parentDir.absoluteFilePath(result.name);
        result.available = true;
    }
    return result;
}

FolderNavResult FolderNavigator::firstSubfolder(const QString &currentFolder)
{
    QDir currentDir(currentFolder);
    const QStringList subfolders = sortedSubfolders(currentDir);

    FolderNavResult result;
    result.count = subfolders.size();
    if (!subfolders.isEmpty()) {
        result.name = subfolders.first();
        result.path = currentDir.absoluteFilePath(result.name);
        result.available = true;
    }
    return result;
}

FolderNavResult FolderNavigator::parentFolder(const QString &currentFolder)
{
    QDir currentDir(currentFolder);

    FolderNavResult result;
    if (currentDir.isRoot() || !currentDir.cdUp()) {
        return result;   // na kořeni disku — count = 0, unavailable
    }

    result.count = 1;
    result.available = true;
    result.name = currentDir.dirName();
    result.path = currentDir.absolutePath();
    return result;
}

} // namespace pictureviewer

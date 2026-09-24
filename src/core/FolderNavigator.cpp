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

QStringList FolderNavigator::subfolderNames(const QString &parentFolder)
{
    return sortedSubfolders(QDir(parentFolder));
}

FolderNavSiblings FolderNavigator::siblingsFromParentListing(const QString &currentFolder,
                                                              const QStringList &parentSubfolderNames)
{
    const QFileInfo info(currentFolder);
    const QDir parentDir = info.dir();
    const int idx = parentSubfolderNames.indexOf(info.fileName());

    FolderNavSiblings result;
    if (idx < 0) {
        return result;
    }

    result.before.count = idx;   // počet sourozenců před aktuální složkou
    if (idx > 0) {
        result.before.name = parentSubfolderNames.at(idx - 1);
        result.before.path = parentDir.absoluteFilePath(result.before.name);
        result.before.available = true;
    }

    result.after.count = parentSubfolderNames.size() - idx - 1;   // počet sourozenců za aktuální složkou
    if (idx + 1 < parentSubfolderNames.size()) {
        result.after.name = parentSubfolderNames.at(idx + 1);
        result.after.path = parentDir.absoluteFilePath(result.after.name);
        result.after.available = true;
    }
    return result;
}

FolderNavSiblings FolderNavigator::siblings(const QString &currentFolder)
{
    const QString parentPath = QFileInfo(currentFolder).absolutePath();
    return siblingsFromParentListing(currentFolder, subfolderNames(parentPath));
}

FolderNavResult FolderNavigator::siblingBefore(const QString &currentFolder)
{
    return siblings(currentFolder).before;
}

FolderNavResult FolderNavigator::siblingAfter(const QString &currentFolder)
{
    return siblings(currentFolder).after;
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

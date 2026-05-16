#include "core/ImageCatalog.hpp"

#include "core/ImageFormats.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

#include <stdexcept>

namespace pictureviewer {

QStringList ImageCatalog::loadFolder(const QString &folderPath, bool includePdf) const
{
    const QDir directory(folderPath);
    if (!directory.exists()) {
        throw std::runtime_error(QString("Cesta není složka: %1").arg(folderPath).toStdString());
    }

    const QFileInfoList entries = directory.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase
    );

    QStringList files;
    files.reserve(entries.size());

    for (const QFileInfo &entry : entries) {
        if (isSupported(entry, includePdf)) {
            files.append(entry.absoluteFilePath());
        }
    }

    return files;
}

bool ImageCatalog::isSupported(const QFileInfo &fileInfo, bool includePdf) const
{
    if (!fileInfo.isFile()) {
        return false;
    }

    const QString suffix = QStringLiteral(".") + fileInfo.suffix();

    if (isSupportedImageExtension(suffix)) {
        return true;
    }

    if (includePdf && isSupportedDocumentExtension(suffix)) {
        return true;
    }

    return false;
}

} // namespace pictureviewer

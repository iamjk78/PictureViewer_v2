#include "core/ImageCatalog.hpp"

#include "core/ImageFormats.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

#include <stdexcept>

namespace pictureviewer {

QStringList ImageCatalog::loadFolder(const QString &folderPath) const
{
    const QDir directory(folderPath);
    if (!directory.exists()) {
        throw std::runtime_error(QString("Cesta není složka: %1").arg(folderPath).toStdString());
    }

    const QFileInfoList entries = directory.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase
    );

    QStringList images;
    images.reserve(entries.size());

    for (const QFileInfo &entry : entries) {
        if (isSupported(entry)) {
            images.append(entry.absoluteFilePath());
        }
    }

    return images;
}

bool ImageCatalog::isSupported(const QFileInfo &fileInfo) const
{
    return fileInfo.isFile() && isSupportedImageExtension(QStringLiteral(".") + fileInfo.suffix());
}

} // namespace pictureviewer

#include "core/ImageMetadataReader.hpp"

#include <QFileInfo>
#include <QImageReader>
#include <QSize>

#include <stdexcept>

namespace pictureviewer {

ImageInfo ImageMetadataReader::read(const QString &path) const
{
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        throw std::runtime_error(QString("Soubor neexistuje: %1").arg(path).toStdString());
    }

    QImageReader reader(path);
    const QSize size = reader.size();
    if (!size.isValid()) {
        throw std::runtime_error(QString("Nepodařilo se načíst metadata obrázku: %1").arg(path).toStdString());
    }

    QString format = QString::fromLatin1(reader.format()).toUpper();
    if (format.isEmpty()) {
        format = fileInfo.suffix().toUpper();
    }

    return ImageInfo{
        .path = fileInfo.absoluteFilePath(),
        .width = size.width(),
        .height = size.height(),
        .fileSize = fileInfo.size(),
        .format = format,
    };
}

} // namespace pictureviewer

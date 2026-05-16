#pragma once

#include <QString>
#include <QStringList>

namespace pictureviewer {

inline QStringList supportedImageExtensions()
{
    return {
        ".jpg",
        ".jpeg",
        ".png",
        ".gif",
        ".bmp",
        ".webp",
        ".tiff",
        ".tif",
    };
}

inline bool isSupportedImageExtension(const QString &suffix)
{
    return supportedImageExtensions().contains(suffix.toLower());
}

inline QStringList supportedDocumentExtensions()
{
    return {
        ".pdf",
    };
}

inline bool isSupportedDocumentExtension(const QString &suffix)
{
    return supportedDocumentExtensions().contains(suffix.toLower());
}

inline bool isSupportedFileExtension(const QString &suffix)
{
    return isSupportedImageExtension(suffix) || isSupportedDocumentExtension(suffix);
}

inline bool isPdfFile(const QString &suffix)
{
    return isSupportedDocumentExtension(suffix);
}

} // namespace pictureviewer

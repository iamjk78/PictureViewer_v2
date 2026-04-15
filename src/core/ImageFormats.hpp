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

} // namespace pictureviewer

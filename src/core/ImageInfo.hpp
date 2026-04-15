#pragma once

#include <QString>

namespace pictureviewer {

struct ImageInfo
{
    QString path;
    int width = 0;
    int height = 0;
    qint64 fileSize = 0;
    QString format;

    double fileSizeKb() const
    {
        return static_cast<double>(fileSize) / 1024.0;
    }

    QString dimensionsString() const
    {
        return QStringLiteral("%1 × %2 px").arg(width).arg(height);
    }
};

} // namespace pictureviewer

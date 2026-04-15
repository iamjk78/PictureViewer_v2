#pragma once

#include "core/ImageInfo.hpp"

#include <QString>

namespace pictureviewer {

class ImageMetadataReader
{
public:
    ImageInfo read(const QString &path) const;
};

} // namespace pictureviewer

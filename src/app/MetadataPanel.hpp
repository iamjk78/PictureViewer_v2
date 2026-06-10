#pragma once

#include "core/ImageInfo.hpp"

#include <QWidget>

class QLabel;

namespace pictureviewer {

// Postranní panel s metadaty souboru pro rozložení "Pro".
class MetadataPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MetadataPanel(QWidget *parent = nullptr);

    void setMetadata(const ImageInfo &info);
    void clearMetadata();

private:
    QLabel *m_nameLabel;
    QLabel *m_dimensionsValue;
    QLabel *m_formatValue;
    QLabel *m_sizeValue;
    QLabel *m_modifiedValue;
    QLabel *m_createdValue;
};

} // namespace pictureviewer

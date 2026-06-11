#include "app/ClipboardHelper.hpp"

#include <QApplication>
#include <QClipboard>
#include <QImage>

namespace pictureviewer {

void copyImageAsJpeg(const QImage &image)
{
    QApplication::clipboard()->setImage(image);
}

} // namespace pictureviewer

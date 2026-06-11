#pragma once

class QImage;

namespace pictureviewer {

// Vloží obrázek do schránky jako JPEG.
// macOS: používá NSPasteboard s NSPasteboardTypeJPEG.
// Ostatní platformy: fallback na QApplication::clipboard()->setImage().
void copyImageAsJpeg(const QImage &image);

} // namespace pictureviewer

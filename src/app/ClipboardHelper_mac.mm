#include "app/ClipboardHelper.hpp"

#include <QBuffer>
#include <QImage>

#import <AppKit/AppKit.h>

namespace pictureviewer {

void copyImageAsJpeg(const QImage &image)
{
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG", 90);
    buffer.close();

    NSData *data = [NSData dataWithBytes:jpegData.constData()
                                  length:(NSUInteger)jpegData.size()];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setData:data forType:@"public.jpeg"];
}

} // namespace pictureviewer

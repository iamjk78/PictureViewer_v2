#pragma once

#include <QBuffer>
#include <QByteArray>
#include <QImage>

// Pomocné funkce testů: JPEG s EXIF miniaturou.
namespace exiftest {

// Obrázek se čtyřmi barevnými kvadranty — z rohů se pozná otočení/zrcadlení.
inline QImage quadrantImage(int w, int h)
{
    QImage img(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const bool right = x >= w / 2;
            const bool bottom = y >= h / 2;
            img.setPixelColor(x, y, !bottom ? (right ? Qt::green : Qt::red)
                                            : (right ? Qt::yellow : Qt::blue));
        }
    }
    return img;
}
inline QByteArray jpegBytes(const QImage &img)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPEG", 95);
    return bytes;
}
inline void put16(QByteArray &b, quint16 v) { b.append(char(v & 0xFF)); b.append(char(v >> 8)); }
inline void put32(QByteArray &b, quint32 v) { put16(b, v & 0xFFFF); put16(b, v >> 16); }

// JPEG = hlavní snímek + EXIF APP1 (little-endian) s miniaturou v IFD1.
inline QByteArray jpegWithExifThumb(const QImage &mainImg, const QImage &thumb, int orientation)
{
    const QByteArray thumbBytes = jpegBytes(thumb);
    QByteArray tiff("II");
    put16(tiff, 42);
    put32(tiff, 8);
    put16(tiff, 1);                                             // IFD0: 1 položka
    put16(tiff, 0x0112); put16(tiff, 3); put32(tiff, 1); put16(tiff, quint16(orientation)); put16(tiff, 0);
    put32(tiff, 26);                                            // odkaz na IFD1
    put16(tiff, 2);                                             // IFD1: 2 položky
    put16(tiff, 0x0201); put16(tiff, 4); put32(tiff, 1); put32(tiff, 56);
    put16(tiff, 0x0202); put16(tiff, 4); put32(tiff, 1); put32(tiff, quint32(thumbBytes.size()));
    put32(tiff, 0);
    tiff.append(thumbBytes);

    QByteArray segment("\xFF\xE1");
    const quint16 length = quint16(2 + 6 + tiff.size());
    segment.append(char(length >> 8)); segment.append(char(length & 0xFF));
    segment.append("Exif\0\0", 6);
    segment.append(tiff);

    QByteArray mainBytes = jpegBytes(mainImg);
    return mainBytes.left(2) + segment + mainBytes.mid(2);
}

} // namespace exiftest

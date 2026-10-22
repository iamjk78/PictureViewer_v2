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

inline void put16be(QByteArray &b, quint16 v) { b.append(char(v >> 8)); b.append(char(v & 0xFF)); }
inline void put32be(QByteArray &b, quint32 v) { put16be(b, v >> 16); put16be(b, v & 0xFFFF); }

// JPEG = hlavní snímek + EXIF APP1 s miniaturou v IFD1. Pořadí bajtů: "II"
// (little-endian, telefony) nebo "MM" (big-endian, řada fotoaparátů).
inline QByteArray jpegWithExifThumb(const QImage &mainImg, const QImage &thumb, int orientation,
                                    bool bigEndian = false)
{
    const QByteArray thumbBytes = jpegBytes(thumb);
    QByteArray tiff(bigEndian ? "MM" : "II");
    auto p16 = [&](quint16 v) { bigEndian ? put16be(tiff, v) : put16(tiff, v); };
    auto p32 = [&](quint32 v) { bigEndian ? put32be(tiff, v) : put32(tiff, v); };
    p16(42);
    p32(8);
    p16(1);                                                     // IFD0: 1 položka
    p16(0x0112); p16(3); p32(1); p16(quint16(orientation)); p16(0);
    p32(26);                                                    // odkaz na IFD1
    p16(2);                                                     // IFD1: 2 položky
    p16(0x0201); p16(4); p32(1); p32(56);
    p16(0x0202); p16(4); p32(1); p32(quint32(thumbBytes.size()));
    p32(0);
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

#include "core/ExifThumbnail.hpp"

#include <QBuffer>
#include <QImageReader>
#include <QTransform>

#include <cmath>
#include <cstring>
#include <optional>

namespace pictureviewer {

namespace {

// Čtení s kontrolou hranic — poškozený/podvržený soubor nesmí číst mimo buffer.
class Reader
{
public:
    Reader(const uchar *data, qsizetype size, bool bigEndian)
        : m_data(data), m_size(size), m_big(bigEndian) {}

    std::optional<quint16> u16(qsizetype pos) const
    {
        if (pos < 0 || pos + 2 > m_size) {
            return std::nullopt;
        }
        return m_big ? quint16((m_data[pos] << 8) | m_data[pos + 1])
                     : quint16((m_data[pos + 1] << 8) | m_data[pos]);
    }
    std::optional<quint32> u32(qsizetype pos) const
    {
        const auto hi = u16(m_big ? pos : pos + 2);
        const auto lo = u16(m_big ? pos + 2 : pos);
        if (!hi || !lo) {
            return std::nullopt;
        }
        return (quint32(*hi) << 16) | *lo;
    }

private:
    const uchar *m_data;
    qsizetype m_size;
    bool m_big;
};

struct ExifInfo {
    int orientation = 1;
    qsizetype thumbOffset = -1;   // v rámci TIFF bloku
    qsizetype thumbLength = 0;
};

// Prochází IFD0 (orientace) a IFD1 (odkaz na miniaturu).
bool parseTiff(const uchar *tiff, qsizetype size, ExifInfo *info)
{
    if (size < 8) {
        return false;
    }
    bool big;
    if (tiff[0] == 'M' && tiff[1] == 'M') {
        big = true;
    } else if (tiff[0] == 'I' && tiff[1] == 'I') {
        big = false;
    } else {
        return false;
    }
    const Reader rd(tiff, size, big);
    if (rd.u16(2).value_or(0) != 42) {
        return false;
    }

    qsizetype ifdOffset = rd.u32(4).value_or(0);
    for (int ifdIndex = 0; ifdIndex < 2 && ifdOffset > 0; ++ifdIndex) {
        const auto count = rd.u16(ifdOffset);
        if (!count) {
            return ifdIndex > 0;
        }
        for (int i = 0; i < *count; ++i) {
            const qsizetype entry = ifdOffset + 2 + qsizetype(i) * 12;
            const auto tag = rd.u16(entry);
            const auto type = rd.u16(entry + 2);
            if (!tag || !type) {
                break;
            }
            // SHORT (3) má hodnotu v prvních 2 bajtech, LONG (4) ve všech 4.
            const auto value = (*type == 3) ? std::optional<quint32>(rd.u16(entry + 8))
                                            : rd.u32(entry + 8);
            if (!value) {
                continue;
            }
            if (ifdIndex == 0 && *tag == 0x0112) {
                info->orientation = int(*value);
            } else if (ifdIndex == 1 && *tag == 0x0201) {
                info->thumbOffset = qsizetype(*value);
            } else if (ifdIndex == 1 && *tag == 0x0202) {
                info->thumbLength = qsizetype(*value);
            }
        }
        const auto next = rd.u32(ifdOffset + 2 + qsizetype(*count) * 12);
        ifdOffset = next ? qsizetype(*next) : 0;
    }
    return info->thumbOffset > 0 && info->thumbLength > 0;
}

QImage applyOrientation(const QImage &image, int orientation)
{
    QTransform rotate90;
    rotate90.rotate(90);
    switch (orientation) {
    case 2: return image.mirrored(true, false);
    case 3: return image.mirrored(true, true);
    case 4: return image.mirrored(false, true);
    case 5: return image.transformed(rotate90).mirrored(true, false);
    case 6: return image.transformed(rotate90);
    case 7: return image.transformed(rotate90).mirrored(false, true);
    case 8: { QTransform r; r.rotate(270); return image.transformed(r); }
    default: return image;
    }
}

} // namespace

QImage extractExifThumbnail(const QByteArray &header, int minSide)
{
    const auto *data = reinterpret_cast<const uchar *>(header.constData());
    const qsizetype size = header.size();
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return {};
    }

    ExifInfo info;
    bool haveExif = false;
    qsizetype tiffStart = 0;
    QSize mainSize;

    qsizetype pos = 2;
    while (pos + 4 <= size && (!haveExif || !mainSize.isValid())) {
        if (data[pos] != 0xFF) {
            break;
        }
        const uchar marker = data[pos + 1];
        if (marker == 0xFF) {   // výplň
            ++pos;
            continue;
        }
        if (marker == 0xDA || marker == 0xD9) {   // začátek dat / konec — hlavičky skončily
            break;
        }
        const qsizetype length = (data[pos + 2] << 8) | data[pos + 3];
        const qsizetype body = pos + 4;
        if (length < 2 || pos + 2 + length > size) {
            break;
        }
        if (marker == 0xE1 && !haveExif && length >= 8 && std::memcmp(data + body, "Exif\0\0", 6) == 0) {
            tiffStart = body + 6;
            haveExif = parseTiff(data + tiffStart, pos + 2 + length - tiffStart, &info);
            if (!haveExif) {
                return {};   // EXIF bez miniatury
            }
        } else if ((marker == 0xC0 || marker == 0xC1 || marker == 0xC2) && length >= 8) {
            mainSize = QSize((data[body + 3] << 8) | data[body + 4],
                             (data[body + 1] << 8) | data[body + 2]);
        }
        pos += 2 + length;
    }
    if (!haveExif || !mainSize.isValid()) {
        return {};
    }

    const qsizetype thumbStart = tiffStart + info.thumbOffset;
    if (thumbStart < 0 || thumbStart + info.thumbLength > size) {
        return {};
    }
    QByteArray thumbBytes = QByteArray::fromRawData(header.constData() + thumbStart, info.thumbLength);
    QBuffer buffer(&thumbBytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "jpeg");
    reader.setAutoTransform(false);   // orientaci řešíme sami podle IFD0 hlavního snímku
    QImage thumb = reader.read();
    if (thumb.isNull() || qMax(thumb.width(), thumb.height()) < minSide) {
        return {};
    }

    // Miniatura po úpravě/ořezu v editoru často zůstane původní: poměr stran
    // pak nesedí k hlavnímu snímku (tolerance na zaokrouhlení rozměrů).
    const double thumbRatio = double(thumb.width()) / thumb.height();
    const double mainRatio = double(mainSize.width()) / mainSize.height();
    if (std::abs(thumbRatio / mainRatio - 1.0) > 0.03) {
        return {};
    }

    return applyOrientation(thumb, info.orientation);
}

} // namespace pictureviewer

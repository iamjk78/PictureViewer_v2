#pragma once

#include <QImage>
#include <QPainter>
#include <QImageWriter>
#include <QList>
#include <QSize>
#include <QString>

#include <algorithm>
#include <cmath>

namespace pictureviewer {

// Strop na výsledný obrázek: 150 Mpx × 4 B = 600 MB v paměti. Víc by zmenšovací
// tlačítko změnilo ve způsob, jak aplikaci shodit.
constexpr qint64 kMaxResizePixels = 150'000'000;
constexpr int kMaxResizeSide = 32000;

inline QSize sizeForPercent(const QSize &original, int percent)
{
    return {std::max(1, static_cast<int>(std::lround(original.width() * percent / 100.0))),
            std::max(1, static_cast<int>(std::lround(original.height() * percent / 100.0)))};
}

// Druhý rozměr při zachování poměru stran: origOther * newMain / origMain.
inline int proportionalSide(int origMain, int origOther, int newMain)
{
    if (origMain <= 0) {
        return std::max(1, origOther);
    }
    return std::max(1, static_cast<int>(std::lround(
        static_cast<double>(origOther) * newMain / origMain)));
}

inline bool isResizeTargetAllowed(const QSize &size)
{
    return size.width() >= 1 && size.height() >= 1
        && size.width() <= kMaxResizeSide && size.height() <= kMaxResizeSide
        && static_cast<qint64>(size.width()) * size.height() <= kMaxResizePixels;
}

inline QImage resizedImage(const QImage &source, const QSize &target)
{
    if (source.size() == target) {
        return source;
    }
    return source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

// JPEG průhlednost neumí — Qt by průhledné pixely vykreslil černě. Podklad je bílý.
inline QImage flattenedOnWhite(const QImage &source)
{
    if (!source.hasAlphaChannel()) {
        return source;
    }
    QImage flat(source.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    QPainter painter(&flat);
    painter.drawImage(0, 0, source);
    return flat;
}

// Formát, do kterého lze obrázek převést (a Qt ho umí zapsat).
struct OutputFormat {
    QString suffix;      // přípona bez tečky
    QString label;       // text v seznamu
    bool hasQuality;     // ztrátový formát s nastavením kvality
};

// Přípona v kanonickém tvaru (jpeg/jpe → jpg, tiff → tif), jinak malými písmeny.
inline QString canonicalSuffix(const QString &suffix)
{
    const QString s = suffix.toLower();
    if (s == QLatin1String("jpeg") || s == QLatin1String("jpe")) {
        return QStringLiteral("jpg");
    }
    if (s == QLatin1String("tiff")) {
        return QStringLiteral("tif");
    }
    return s;
}

inline QList<OutputFormat> writableOutputFormats()
{
    struct Candidate { const char *suffix; const char *qtFormat; const char *label; bool quality; };
    static const Candidate candidates[] = {
        {"jpg", "jpeg", "JPEG (.jpg)", true},
        {"png", "png",  "PNG (.png)",  false},
        {"webp", "webp", "WebP (.webp)", true},
        {"bmp", "bmp",  "BMP (.bmp)",  false},
        {"tif", "tiff", "TIFF (.tif)", false},
    };
    const QList<QByteArray> supported = QImageWriter::supportedImageFormats();
    QList<OutputFormat> result;
    for (const Candidate &c : candidates) {
        if (supported.contains(c.qtFormat)) {
            result.append({QString::fromLatin1(c.suffix), QString::fromUtf8(c.label), c.quality});
        }
    }
    return result;
}

} // namespace pictureviewer

#pragma once

#include <QByteArray>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QSaveFile>
#include <QString>

namespace pictureviewer {

// Formát, ve kterém se dá obrázek zapsat pod daný název (podle přípony), nebo
// prázdné pole, pokud ho Qt zapsat neumí (HEIC, GIF, …). Uložení pod názvem
// s příponou X musí vyrobit skutečný soubor formátu X — dřív se do .png nebo
// .webp zapisovala JPEG data.
inline QByteArray writableFormatForPath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") || suffix == QLatin1String("jpe")) {
        return "jpeg";
    }
    const QByteArray format = suffix.toLatin1();
    return !format.isEmpty() && QImageWriter::supportedImageFormats().contains(format) ? format : QByteArray();
}

// Zapíše obrázek přes dočasný soubor a přejmenování, takže se na disku nikdy
// neobjeví rozepsaný (useknutý) soubor — při pádu, ukončení aplikace nebo
// plném disku zůstane původní obsah. Dva souběžní zapisovatelé téhož souboru
// si nepřekážejí (každý má vlastní dočasný soubor, vyhrává poslední rename).
inline bool saveImageAtomically(const QImage &image, const QString &path,
                                const char *format, int quality = -1)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(true);   // sdílení, kde se dočasný soubor nedá vytvořit
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (!image.save(&file, format, quality)) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace pictureviewer

#pragma once

#include <QImageReader>
#include <QString>
#include <QStringList>

namespace pictureviewer {

inline QStringList supportedDocumentExtensions()
{
    return {
        ".pdf",
    };
}

// Přípony obrázků odvozené z Qt image pluginů nainstalovaných v systému
// (JPEG, PNG, GIF, BMP, WEBP, TIFF a podle build/pluginů i HEIC, AVIF, SVG…).
// Počítá se jednou (function-local static) — QImageReader vrací názvy formátů
// bez tečky a malými písmeny, doplníme úvodní tečku.
//
// POZOR: Qt může mezi "image" formáty hlásit i 'pdf' (přes nainstalovaný
// plugin). PDF ale musí jít přes PDF render, ne přes obrázkový dekodér, a řídí
// se přepínačem zpracování PDF — proto dokumentové přípony z tohoto seznamu
// explicitně vyřazujeme.
inline const QStringList &supportedImageExtensions()
{
    static const QStringList extensions = [] {
        const QStringList documents = supportedDocumentExtensions();
        QStringList result;
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        result.reserve(formats.size());
        for (const QByteArray &format : formats) {
            const QString ext = QStringLiteral(".") + QString::fromLatin1(format).toLower();
            if (!documents.contains(ext)) {
                result.append(ext);
            }
        }
        return result;
    }();
    return extensions;
}

inline bool isSupportedImageExtension(const QString &suffix)
{
    return supportedImageExtensions().contains(suffix.toLower());
}

inline bool isSupportedDocumentExtension(const QString &suffix)
{
    return supportedDocumentExtensions().contains(suffix.toLower());
}

inline bool isSupportedFileExtension(const QString &suffix)
{
    return isSupportedImageExtension(suffix) || isSupportedDocumentExtension(suffix);
}

inline bool isPdfFile(const QString &suffix)
{
    return isSupportedDocumentExtension(suffix);
}

// Video formáty kompatibilní s Qt Multimedia
inline const QStringList &supportedVideoExtensions()
{
    static const QStringList extensions = {
        ".mp4", ".mkv", ".avi", ".mov", ".ts", ".mpg", ".mpeg",
        ".webm", ".wmv", ".m4v", ".flv", ".f4v", ".m2ts", ".mts",
        ".3gp", ".3g2", ".divx", ".asf",
    };
    return extensions;
}

inline bool isVideoFile(const QString &suffix)
{
    return supportedVideoExtensions().contains(suffix.toLower());
}

} // namespace pictureviewer

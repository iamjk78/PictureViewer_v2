#include "core/ImageCatalog.hpp"

#include "core/ImageFormats.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QString>
#include <algorithm>

#include <stdexcept>

namespace pictureviewer {

QStringList ImageCatalog::loadFolder(const QString &folderPath, bool includePdf) const
{
    const QDir directory(folderPath);
    if (!directory.exists()) {
        throw std::runtime_error(QString("Cesta není složka: %1").arg(folderPath).toStdString());
    }

    // Bez řazení — budeme ručně třídit přirozeným řazením
    const QFileInfoList entries = directory.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot
    );

    QStringList files;
    files.reserve(entries.size());

    for (const QFileInfo &entry : entries) {
        if (isSupported(entry, includePdf)) {
            files.append(entry.absoluteFilePath());
        }
    }

    // Přirozené řazení: img2 < img10 (ne img10 < img2)
    std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
        return QString::localeAwareCompare(a, b) < 0;
    });

    return files;
}

bool ImageCatalog::isSupported(const QFileInfo &fileInfo, bool includePdf) const
{
    if (!fileInfo.isFile()) {
        return false;
    }

    const QString suffix = QStringLiteral(".") + fileInfo.suffix();

    if (isSupportedImageExtension(suffix)) {
        return true;
    }

    if (includePdf && isSupportedDocumentExtension(suffix)) {
        return true;
    }

    return false;
}

} // namespace pictureviewer

#pragma once

#include <QCollator>
#include <QLocale>

namespace pictureviewer {

// Collator pro přirozené, locale-aware řazení názvů souborů a složek
// (numericMode: "img2" < "img10", case-insensitive). Explicitní locale "en"
// zajistí numeric mode i na systémech bez nastaveného LANG (Linux CI s LANG=C).
// Sdíleno mezi ImageCatalog, FolderNavigator a CompanionFinder.
inline QCollator makeNaturalCollator()
{
    QCollator collator{QLocale(QLocale::English)};
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    return collator;
}

} // namespace pictureviewer

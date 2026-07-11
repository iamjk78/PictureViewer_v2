#pragma once

#include <QColor>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>

namespace pictureviewer {

// Jediný zdroj pravdy pro paletu 20 předdefinovaných barev sdílenou napříč
// funkcemi: štítky (CategoryManager/CategoryDialogs), oblíbené složky
// a tlačítka přesunu (MoveDialogs/MainWindow). Dřív byla paleta zkopírovaná
// na pěti místech — změna barvy by je rozjela.
inline constexpr const char *kPredefinedColors[] = {
    "#FF6B6B", "#4ECDC4", "#45B7D1", "#FFA07A", "#98D8C8",
    "#F7DC6F", "#BB8FCE", "#85C1E2", "#F8B88B", "#A9DFBF",
    "#F5B7B1", "#D7BDE2", "#F9E79F", "#AED6F1", "#F8B4B8",
    "#B7E8D6", "#FDBFED", "#D4EFDF", "#FADBD8", "#EBD5B4"
};
inline constexpr int kPredefinedColorCount =
    static_cast<int>(sizeof(kPredefinedColors) / sizeof(kPredefinedColors[0]));

// Výchozí barva pro položky bez explicitní volby (druhá v paletě — tyrkysová).
inline QString defaultItemColor()
{
    return QString::fromLatin1(kPredefinedColors[1]);
}

// Náhodná barva z palety, která ještě není v `usedHexColors`
// (porovnání case-insensitive). Jsou-li všechny použité, vrátí libovolnou
// náhodnou — duplicita je pak nevyhnutelná a přijatelná.
inline QString pickRandomUnusedColor(const QStringList &usedHexColors)
{
    for (int attempt = 0; attempt < kPredefinedColorCount; ++attempt) {
        const int idx = QRandomGenerator::global()->bounded(kPredefinedColorCount);
        const QString candidate = QString::fromLatin1(kPredefinedColors[idx]);
        if (!usedHexColors.contains(candidate, Qt::CaseInsensitive)) {
            return candidate;
        }
    }
    const int idx = QRandomGenerator::global()->bounded(kPredefinedColorCount);
    return QString::fromLatin1(kPredefinedColors[idx]);
}

} // namespace pictureviewer

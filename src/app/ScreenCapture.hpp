#pragma once

#include <QImage>
#include <QString>

class QWidget;

namespace pictureviewer {

// Výsledek interaktivního zachycení výřezu obrazovky.
struct ScreenCaptureResult
{
    QImage  image;      // zachycený výřez; prázdný při zrušení nebo chybě
    QString tempPath;   // cesta k dočasně uloženému PNG; prázdná při zrušení
};

// Interaktivně zachytí obdélníkový výřez kdekoli na připojených obrazovkách.
// Uživatel označí oblast myší (Esc = zrušit). Výsledek se uloží jako PNG do
// dočasné složky a zároveň vrátí jako QImage.
//
// macOS: používá systémový nástroj `screencapture` (řeší více monitorů,
//        oprávnění i nativní UI výběru).
// Ostatní platformy: vlastní celoobrazovkový overlay s gumovým obdélníkem.
//
// parentWindow je po dobu výběru dočasně skryto, aby nepřekáželo. Smí být null.
ScreenCaptureResult captureScreenRegion(QWidget *parentWindow);

} // namespace pictureviewer

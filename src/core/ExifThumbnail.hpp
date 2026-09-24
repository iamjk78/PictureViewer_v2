#pragma once

#include <QByteArray>
#include <QImage>

namespace pictureviewer {

// Kolik bajtů od začátku JPEG souboru stačí přečíst pro extractExifThumbnail
// (hlavičky s EXIF a rozměry hlavního snímku jsou hned na začátku).
constexpr qint64 kExifHeaderBytes = 128 * 1024;

// Vytáhne miniaturu vloženou do EXIF hlavičky JPEG (fotoaparáty a telefony ji
// ukládají, typicky 160×120 px) a otočí ji podle EXIF orientace. `header` je
// začátek souboru (viz kExifHeaderBytes) — celý soubor se číst nemusí, což
// je na síťovém disku rozdíl mezi 30 kB a několika MB na miniaturu.
//
// Vrací null QImage, pokud miniatura chybí, je příliš malá (< minSide) nebo
// nesedí k hlavnímu snímku (jiný poměr stran = zastaralá po úpravě/ořezu) —
// volající pak použije běžné dekódování celého souboru.
QImage extractExifThumbnail(const QByteArray &header, int minSide = 160);

} // namespace pictureviewer

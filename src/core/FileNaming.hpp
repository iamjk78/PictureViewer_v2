#pragma once

#include <QString>
#include <QStringList>

namespace pictureviewer::filenaming {

// Sestavování cílových cest pro SKUPINU souborů (aktivní soubor + jeho párové
// soubory, viz CompanionFinder). Skupina se vždy přesouvá/přejmenovává jako
// celek, aby si soubory zachovaly shodný základ názvu a dál se párovaly.
//
// Funkce jsou čisté — nesahají na disk (kromě firstExistingTarget, který
// existenci ověřuje záměrně) a nic neprovádějí. Volající si sám rozhodne,
// co s kolizí udělá.

// Cílové cesty se SDÍLENÝM novým základem názvu; každý soubor si ponechá
// vlastní příponu (obrázek .jpg, jeho video .mp4 …). Soubor bez přípony
// dostane jen holý základ.
//
// Pořadí odpovídá vstupu — volající se na shodu indexů spoléhá při párování
// zdroj → cíl.
QStringList groupTargetPaths(const QStringList &files,
                             const QString &targetFolder,
                             const QString &newBaseName);

// Cílové cesty se ZACHOVÁNÍM původních názvů (přesun do jiné složky beze
// změny jména).
QStringList groupTargetPaths(const QStringList &files, const QString &targetFolder);

// První cesta ze seznamu, která na disku už existuje; jinak prázdný QString.
// Kontrola se dělá nad CELOU skupinou předem, aby operace neskončila
// v půlce a skupina nezůstala rozpadlá.
QString firstExistingTarget(const QStringList &targetPaths);

// Totéž, ale cíl, který je TÍMTÉŽ souborem jako odpovídající zdroj (indexy se
// shodují), kolizí není — přejmenování "img" → "IMG" na souborovém systému
// bez rozlišení velikosti písmen (macOS, Windows) jinak hlásilo, že soubor už
// existuje (našlo sebe sama).
QString firstExistingTarget(const QStringList &targetPaths, const QStringList &sourcePaths);

// Je řetězec použitelný jako holý název souboru (bez přípony)? Zakazuje oddělovače
// cest, "." a "..", řídicí znaky a znaky, které by cestu opustily nebo rozbily
// na jiné platformě. Uživatelský vstup z dialogu přejmenování / uložení se
// jinak spojuje s cestou přímo: "../x" by soubor přesunulo o složku výš
// a "/tmp/x" by použilo absolutní cestu.
bool isValidBaseName(const QString &name);

// Přejmenuje soubor; změna jen velikosti písmen (zdroj a cíl jsou tentýž soubor
// na souborovém systému bez rozlišení velikosti) projde přes dočasný název,
// protože QFile::rename by cíl vyhodnotil jako existující.
bool renameFile(const QString &from, const QString &to);

} // namespace pictureviewer::filenaming

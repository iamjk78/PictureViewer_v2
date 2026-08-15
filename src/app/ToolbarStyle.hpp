#pragma once

#include <QString>

class QToolButton;

namespace pictureviewer {

// Jediný zdroj pravdy pro velikost tlačítek toolbarů. Hodnoty se používají
// jak v CSS (viz styly níže), tak při nastavení pevné velikosti na widgetu.
inline constexpr int kMainToolbarIconSize      = 35;   // hlavní toolbar
inline constexpr int kSecondaryToolbarIconSize = 28;   // Oblíbené, Štítky, Přesun

// Styl ikonových tlačítek sekundárních toolbarů (Oblíbené, Štítky, Přesun).
// Bez orámování, s jemným zvýrazněním při najetí a skrytými oddělovači.
QString secondaryToolbarStyle();

// Nastaví pevnou velikost tlačítka i jeho ikony PŘÍMO na widgetu.
// Nativní macOS styl (QMacStyle) nemusí respektovat CSS width/height/padding —
// vlastní vnitřní chrome mu ukusuje místo bez ohledu na stylesheet, takže ikony
// vypadaly menší, než mají. setFixedSize()/setIconSize() je tvrdé C++ omezení,
// které styl obejít nemůže.
//
// Nutné pro každé tlačítko přidané do hlavního toolbaru MIMO setupToolbar()
// (ta si na konci projde vlastní akce sama) — jinak by mělo výchozí malou
// velikost. Volá se tedy z setup*Toolbar() funkcí, které do hlavního toolbaru
// přidávají své přepínací akce.
void applyToolbarButtonSize(QToolButton *button, int size);

} // namespace pictureviewer

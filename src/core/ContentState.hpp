#pragma once

namespace pictureviewer::contentstate {

// Druh obsahu, který je PRÁVĚ zobrazený v centrální ploše. Jediný zdroj pravdy
// o tom, co uživatel vidí — na rozdíl od přípony souboru na m_currentIndex,
// která se se zobrazeným obsahem rozchází (snímek obrazovky pořízený nad
// přehrávaným videem, snímek stránky PDF, prázdná složka po smazání).
enum class ContentKind {
    None,          // nic — žádná složka, prázdná složka, nenačtený soubor
    Image,         // statický obrázek ze složky
    AnimatedGif,   // animovaný GIF ze složky (QMovie přemalovává snímky)
    Pdf,           // dokument PDF
    Video,         // video v přehrávači
    Capture,       // snímek obrazovky / stránky PDF — nemá soubor na disku
};

// Vstup pro odvození stavu akcí. Vše, na čem stav tlačítek smí záviset —
// když se sem nějaká podmínka nedostane, tlačítko na ni reagovat nemůže.
struct ContentStatus {
    ContentKind kind = ContentKind::None;
    // Obsah byl upraven (ořez / otočení) a ještě není uložený.
    bool modified = false;
    // m_currentIndex ukazuje na platný soubor v seznamu. Pozor: u Capture to
    // platí taky (index zůstal na naposledy navigovaném souboru), ale ten
    // soubor NENÍ to, co uživatel vidí — proto ho akce nad souborem nesmí brát.
    bool hasCurrentFile = false;
    // Seznam souborů složky není prázdný.
    bool hasFiles = false;
    // Procházení je zamčené (video spuštěné klávesou G — viz
    // MainWindow::disableImageBrowsing).
    bool browsingLocked = false;
};

// Stav akcí odvozený z obsahu. Pravda o tom, co smí být aktivní.
struct ActionStates {
    bool previous = false;
    bool next = false;
    bool slideshow = false;
    bool rotate = false;
    bool crop = false;
    bool save = false;
    bool saveAs = false;
    bool deleteFile = false;
    bool rename = false;
    bool moveToFolder = false;
    bool labels = false;
    // Viditelnost PDF toolbaru a ovládání zoom/fit ve status baru — řídí se
    // stejným stavem jako tlačítka, takže patří sem, ne do zvláštní logiky.
    bool pdfToolbar = false;
    bool fitControls = false;
};

// Odvodí stav VŠECH akcí z jednoho popisu obsahu. Čistá funkce — celá matice
// „typ obsahu × akce" je tím na jednom místě a dá se otestovat kompletně.
ActionStates deriveActionStates(const ContentStatus &status);

} // namespace pictureviewer::contentstate

# TODO – náměty na budoucí vylepšení

Sepsáno 2026-06-10 na základě revize logiky aplikace.
Aktualizováno po implementaci výkonových vylepšení (verze 0.5).

## Hotovo ✓

- [x] **EXIF orientace** – `QImageReader` + `setAutoTransform(true)` v ImageLoaderu,
      ImageView i ThumbnailWorkeru. *(2026-06-10)*
- [x] **Render hints** – `SmoothPixmapTransform` v ImageView; hladké škálování. *(2026-06-10)*
- [x] **Asynchronní načítání + prefetch ±1 + RAM LRU cache (256 MB)** – nová třída
      `ImageLoader`; placeholder z náhledu při cache missu. *(2026-06-10)*
- [x] **Zobrazení prvního obrázku před dokončením skenu složky** *(2026-06-10)*
- [x] **Disková cache náhledů** – 192 px, SHA-1 klíč (cesta+mtime+velikost),
      zmenšené dekódování JPEG; menu Nastavení → Cache náhledů
      (povolit/zakázat, volba složky, vymazání s potvrzením). *(2026-06-10)*
- [x] **PDF render podle zoomu a DPI** – rozlišení dle viewportu × DPR,
      správný poměr stran, debounce re-render při zoomu; PDF se korektně
      uvolní při přechodu na obrázek. *(2026-06-10)*
- [x] **Přepínatelná rozložení UI** – Klasický / Filmový pás / Imerzivní /
      Galerie / Pro režim, perzistentní volba. *(2026-06-10)*

## Opravy chyb v logice

- [ ] **Přirozené řazení souborů** – `ImageCatalog` řadí lexikograficky (`img10` před `img2`). Použít `QCollator` s `numericMode`.
- [ ] **Závod náhledů s mazáním** – `ThumbnailWorker` doručuje náhledy podle indexu; smazání obrázku během načítání posune indexy a náhledy se přiřadí špatným položkám. Párovat podle cesty.
- [ ] **Přejmenování neaktualizuje panel náhledů** – položka drží starou cestu v `Qt::UserRole` a starý tooltip.
- [ ] **Status bar u PDF po otevření** – `pdfPageChanged` se emituje dřív, než se připojí handler; číslo stránky se ukáže až po listování.
- [ ] **Slideshow běží při přehrávání videa** – akce se jen zakáže, ale `QTimer` běží dál a přepíná obrázky pod videem. Zastavit slideshow při startu VLC.
- [ ] **Esc zavírá celou aplikaci** – nečekané; zvážit potvrzení nebo Esc pouze pro fullscreen/galerii.

## Výkon

- [ ] **`QFileSystemWatcher`** – automatická reakce na změny složky na disku.
- [ ] **Úklid diskové cache náhledů** – strop velikosti (např. 500 MB) + mazání nejstarších záznamů podle času přístupu.
- [ ] **Směrový prefetch** – při listování vpřed přednačítat N+1 a N+2 místo N−1.

## Nové funkce

- [ ] Otočení obrázku o 90° (klávesy L/R), volitelně uložit do souboru
- [ ] Drag & drop složky/souboru do okna
- [ ] Animované GIFy přes `QMovie`
- [ ] Dynamický seznam formátů z `QImageReader::supportedImageFormats()` (HEIC, AVIF, SVG, …)
- [ ] Volba řazení (název / datum / velikost, vzestupně / sestupně)
- [ ] Náhodné pořadí slideshow
- [ ] „Zobrazit ve Finderu/Exploreru" + kopírovat obrázek do schránky
- [ ] Vícenásobný výběr v režimu Galerie (hromadné mazání/přesun)
- [ ] Rekurzivní sken podsložek (volitelně)
- [ ] Indikátor úrovně zoomu ve status baru
- [ ] EXIF data (clona, ISO, expozice) v Pro panelu metadat
- [ ] Aktualizace HelpDialog o nové režimy vzhledu a cache

# TODO – náměty na budoucí vylepšení

Sepsáno 2026-06-10 na základě revize logiky aplikace.
Aktualizováno po implementaci výkonových vylepšení (verze 0.5).
Aktualizováno po code review a bug fixech (verze 0.6).

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

- [x] **Přirozené řazení souborů** – `ImageCatalog` řadí lexikograficky (`img10` před `img2`). Použít `QCollator` s `numericMode`. *(2026-06-10)*
- [x] **Závod náhledů s mazáním** – `ThumbnailWorker` doručuje náhledy podle indexu; smazání obrázku během načítání posune indexy a náhledy se přiřadí špatným položkám. Párovat podle cesty. *(2026-06-10)*
- [x] **Přejmenování neaktualizuje panel náhledů** – položka drží starou cestu v `Qt::UserRole` a starý tooltip. *(2026-06-10)*
- [x] **Status bar u PDF po otevření** – `pdfPageChanged` se emituje dřív, než se připojí handler; číslo stránky se ukáže až po listování. *(2026-06-10)*
- [x] **Slideshow běží při přehrávání videa** – akce se jen zakáže, ale `QTimer` běží dál a přepíná obrázky pod videem. Zastavit slideshow při startu VLC. *(2026-06-10)*

## Výkon

- [ ] **`QFileSystemWatcher`** – automatická reakce na změny složky na disku. (Vypnuto – vytváří nekonečné cykly)
- [x] **Úklid diskové cache náhledů** – strop velikosti (500 MB) + mazání nejstarších záznamů. *(2026-06-10)*
- [x] **Směrový prefetch** – při listování vpřed přednačítat N+1 až N+5 místo N−1. *(2026-06-10)*

## UI/UX vylepšení

- [x] **Miniatury vycentrované v buňkách** – custom `CenteredIconDelegate` s zachováním aspect ratio. *(2026-06-10)*

## Opravy z code review (verze 0.6)

**Vyřešeno:**
- [x] ImageLoader race condition (watcher parent null)
- [x] VlcController onMonitorTimeout race condition
- [x] VlcController dangling pointer (generation checking)
- [x] MainWindow showImage() TOCTOU (imagePaths snapshot)
- [x] ImageLoader cache key collision (přidán size)
- [x] ThumbnailPanel O(N) lookup → O(1) hash lookup
- [x] MainWindow onImageDecoded() TOCTOU
- [x] VlcController dialog parent window
- [x] MainWindow leaveGalleryGrid() widget ownership
- [x] MainWindow prefetchNeighbors() modulo bug
- [x] ThumbnailWorker PDF error handling

**Druhé kolo – vyřešeno:**
- [x] MainWindow onScanComplete() – m_requestedFile se nyní čistí i u prázdné složky
- [x] HelpDialog – přidán changelog verze 0.6

**Zamítnuto po revizi (falešné poplachy / kosmetika):**
- SlideshowController „thread-safety" – objekt žije jen v GUI vlákně, žádný race
- keyPressEvent VLC guard – event loop během init blokuje synchronně, guard zbytečný
- ImageView PDF duplikace – metody se liší, sdílená logika je jeden řádek
- VlcController cleanup() – už chráněný null checky, bezpečný
- ImageCatalog throw – toStdString() konverze je správná

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
- [x] Indikátor úrovně zoomu ve status baru – jen pro obrázky (1:1 = 100 %),
      u PDF skrytý (rozlišení renderu se přizpůsobuje zoomu). *(2026-06-10)*
- [ ] EXIF data (clona, ISO, expozice) v Pro panelu metadat
- [ ] Aktualizace HelpDialog o nové režimy vzhledu a cache

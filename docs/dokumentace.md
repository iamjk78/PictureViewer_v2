# PictureViewer – Dokumentace

## Účel

Multiplatformní prohlížeč obrázků a PDF napsaný v **C++20 / Qt6** (macOS, Windows).
Umožňuje procházet obrázky ve složce, přibližovat/oddalovat, spouštět slideshow,
mazat a přejmenovávat soubory, prohlížet PDF dokumenty a přehrávat doprovodná
videa přes VLC. Nabízí pět přepínatelných rozložení UI a systém kategorizace
obrázků s barevnými štítky a filtrováním.

---

## Architektura

```
src/
├── main.cpp                  – vstupní bod
├── app/                      – GUI vrstva
│   ├── Application           – inicializace QApplication, file-open eventy (macOS)
│   ├── MainWindow            – hlavní okno, menu, toolbar, přepínání rozložení
│   ├── ImageView             – zobrazení obrázku/PDF, zoom/pan (QGraphicsView)
│   ├── ImageLoader           – asynchronní dekodér s RAM cache a prefetchem
│   ├── ThumbnailPanel        – panel náhledů (svislý / vodorovný / mřížka)
│   ├── MetadataPanel         – panel metadat pro rozložení „Pro"
│   ├── SlideshowController   – QTimer pro automatické přepínání
│   ├── SettingsManager       – perzistentní nastavení (config.ini)
│   ├── ThumbnailCacheManager – velikost a automatický úklid diskové cache
│   ├── CategoryManager       – správa kategorií v SQLite (CRUD, filtrování, přiřazení k obrázkům)
│   ├── CategoryDialogs       – dialogy pro tvorbu a hromadné přiřazení kategorií
│   ├── VlcController         – ovládání externího VLC přes RC rozhraní
│   └── HelpDialog            – dialogy nápovědy
├── core/                     – logika bez závislosti na GUI
│   ├── ImageCatalog          – výpis a řazení podporovaných souborů ze složky
│   ├── ImageFormats          – přípony odvozené z Qt image pluginů
│   ├── ImageInfo             – struktura metadat souboru
│   ├── ImageMetadataReader   – čtení rozměrů/formátu/velikosti
│   └── PdfHandler            – načítání a render PDF (Qt PDF)
├── workers/                  – úlohy v QThreadPool
│   ├── FolderScanWorker      – asynchronní sken složky (generační čítač)
│   └── ThumbnailWorker       – generování náhledů + disková cache
tests/                        – jednotkové testy jádra (Qt Test, ctest)
```

---

## Systém kategorií

### Přiřazení kategorií (`CategoryManager`)
- Kategorie se ukládají v SQLite databázi (`categories.db`) vedle `config.ini`.
- **Schéma**: tabulka `categories` (id, name, color) + `image_categories` (image_path, category_id).
  Vztah M:N s CASCADE DELETE — smazání kategorie automaticky odebere všechna přiřazení.
- **Limit**: max 5 kategorií na jeden obrázek.
- **20 předdefinovaných barev** (hex); při vytvoření bez zvolené barvy se náhodně vybere barva,
  která ještě není použita pro žádnou existující kategorii.
- Klíčem je absolutní cesta souboru — při přesunutí souboru se přiřazení ztratí.

### Toolbar kategorií
- **Sekundární toolbar** (skrytý/viditelný tlačítkem „🏷️ Kategorie") se skládá ze dvou řad:
  - *Horní řada*: tlačítko „+ Nová" + barevná tlačítka pro přiřazení k aktuálnímu obrázku
  - *Dolní řada*: nápis „Filtr:" + filtrační tlačítka (AND logika — zobrazí se jen obrázky mající všechny vybrané kategorie)
- **Lazy loading**: filtrační tlačítka se vytvoří/obnoví až když je toolbar viditelný (šetří DB dotazy).
- **Filtr zobrazuje jen použité kategorie**: SQL dotaz `INNER JOIN` vrátí jen kategorie přiřazené
  alespoň jednomu obrázku v aktuální složce.
- **Pravý klik** na libovolné tlačítko kategorie zobrazí context menu: Přejmenovat, Změnit barvu, Odstranit.

### Nastavení config.ini
| Sekce | Klíč | Význam |
|---|---|---|
| Categories | toolbar_visible | Zda je sekundární toolbar zobrazen |

---

## Klíčové mechanismy

### Asynchronní načítání obrázků (`ImageLoader`)
- Dekódování probíhá přes `QtConcurrent` mimo UI vlákno — listování nikdy neblokuje.
- **RAM cache** (LRU, limit 256 MB) s klíčem *cesta + mtime + velikost* — změna
  souboru na disku přirozeně zneplatní záznam.
- **Směrový prefetch**: po zobrazení obrázku se podle směru listování přednačte
  5 následujících (resp. předchozích) souborů. PDF a GIF se přeskakují — jdou
  jinou cestou (PDF render / QMovie), cache by jim nepomohla.
- Při cache missu se okamžitě zobrazí zvětšený náhled jako placeholder
  a po dekódování se obraz doostří.
- EXIF orientace se aplikuje automaticky (`QImageReader::setAutoTransform`).

### Disková cache náhledů (`ThumbnailWorker`)
- Náhledy 192 px (2× zobrazovaná velikost — ostré na Retina displejích).
- Uloženy v `<kořen>/PictureViewerThumbs/<ab>/<sha1>.thumb`; klíč hashuje
  *cestu + mtime + velikost souboru*, takže změna souboru = nový záznam.
- Výchozí kořen je systémový cache adresář; uživatel může zvolit vlastní složku.
- JPEG se dekóduje rovnou zmenšeně (`setScaledSize`) — výrazně méně čtení
  ze sítě a rychlejší dekódování.
- Ovládání v menu **Nastavení → Cache náhledů**: povolit/zakázat, vybrat
  složku, vymazat (s potvrzením a výpisem velikosti).
- **Automatický úklid** (`ThumbnailCacheManager`, kontrola při startu): pod
  500 MB se nemaže nic; po dosažení limitu se mažou nejstarší soubory, dokud
  cache neklesne pod 80 % limitu (400 MB).

### Animované GIFy
- GIF se přehrává přes `QMovie` (`ImageView::loadAnimation`) — každý snímek
  přemaluje zobrazovanou pixmapu. Přechod na jiný soubor animaci zastaví a uvolní.
- Rotace u běžícího GIFu je vypnutá (další snímek by ji přepsal).

### Řazení souborů
- `ImageCatalog` řadí podle názvu (přirozeně přes `QCollator` s numericMode —
  `img2` < `img10`), data změny nebo velikosti, vzestupně i sestupně.
- Sestupné pořadí se získá prohozením argumentů komparátoru (ne negací — ta by
  u shodných klíčů porušila ostré uspořádání).
- Volba v **Nastavení → Řazení souborů**; změna znovu naskenuje složku a
  zachová právě zobrazený obrázek.

### Párové soubory (obrázek/video) — `CompanionFinder`
- Volitelné (per-profil, `Nastavení → Přesouvat/mazat i párové soubory`). Při
  přesunu (toolbar Přesun) nebo mazání (koš i složka Delete) najde ve **stejné
  složce** ostatní **obrázky a videa** se stejným `completeBaseName`
  (case-insensitive) a jinou příponou — a provede s nimi stejnou akci.
- `CompanionFinder` (čisté jádro, `src/core/`) posuzuje kategorii přes
  `ImageFormats.hpp`; **PDF se nikdy nepáruje** (ani jako zdroj, ani jako cíl).
- 0 párů → jen aktivní; 1 pár → automaticky obojí; 2+ párů → `CompanionActionDialog`
  (vše / jen aktivní / storno).
- Undo je **skupinové**: `m_moveHistory` / `m_deleteHistory` jsou `QList<MoveGroup>`,
  kde jedna skupina = jedna akce (aktivní + páry). ↩ i ♻ vrátí celou skupinu
  jedním krokem. Bez zapnuté funkce je skupina jednoprvková (chování beze změny).

### PDF
- Render stránky v rozlišení podle viewportu × devicePixelRatio se zachováním
  poměru stran; při zoomu se stránka po 250 ms přerenderuje ostře.
- Bílé pozadí pod stránkou (PDF s průhledností by jinak splývalo s tmavým UI).
- Listování stránek PageUp/PageDown; zpracování PDF lze vypnout v Nastavení.

### Sken složky a generační čítače
- `FolderScanWorker` skenuje asynchronně; výsledky nesou číslo generace,
  opožděné signály ze zrušených skenů se zahazují.
- Při otevření konkrétního souboru (Finder, dialog) se soubor zobrazí
  **okamžitě**, sken složky doběhne na pozadí.

### Rozložení UI (menu Nastavení → Vzhled aplikace)
| Rozložení | Popis |
|---|---|
| Klasický | Panel náhledů vlevo, toolbar, status bar (výchozí) |
| Filmový pás | Náhledy jako vodorovný pás dole |
| Imerzivní | Skrytý chrome; plovoucí ovládání se objeví při pohybu myši |
| Galerie | Mřížka náhledů přes celé okno; klik otevře obrázek, Esc vrátí |
| Pro režim | Filmový pás dole + panel metadat vpravo |

Volba se ukládá a obnoví po restartu. Přepínání funguje za běhu.

---

## Ovládání klávesnicí

| Klávesa | Akce |
|---|---|
| `←` / `→` | Předchozí / další obrázek |
| `↑` / `↓` | První / poslední obrázek |
| `PageUp` / `PageDown` | Stránka PDF, jinak předchozí/další obrázek |
| `S` | Spustit / zastavit slideshow |
| `F` | Celá obrazovka |
| `D` / `Delete` | Smazat (do koše) nebo přesunout do složky Delete |
| `R` | Přejmenovat soubor |
| `[` / `L` , `]` | Otočit obrázek doleva / doprava (vizuálně) |
| `G` | Přehrát video se stejným názvem ve VLC |
| `+` / `-` / kolečko | Zoom |
| `0` / `Space` / prostřední tlačítko | Originální velikost (1:1) |
| `Esc` | Konec fullscreenu / návrat do galerie / zavřít aplikaci |

Při aktivním VLC: `Space` pauza, `←`/`→` posun ±10 s, `+`/`-` hlasitost,
`F` fullscreen videa, `Esc` ukončení přehrávání.

**Myš a další gesta**: pravý klik nad obrázkem otevře kontextové menu (Zobrazit
ve Finderu, kopírovat obrázek / cestu); složku nebo soubor lze přetáhnout do
okna (drag & drop). Indikátor zoomu (%) je vpravo ve status baru u obrázků.

---

## Nastavení (config.ini) — per-profil

Aplikace podporuje více profilů (`ProfileManager`); každý profil má vlastní
`config.ini` v `profiles/<jméno profilu>/config.ini`:

- macOS: `~/Library/Preferences/JiriKrejci/PictureViewer/profiles/<jméno>/config.ini`
- Windows: `%APPDATA%\JiriKrejci\PictureViewer\profiles\<jméno>\config.ini`

Seznam profilů, aktivní profil a startovní chování jsou uloženy zvlášť v
`profiles.ini` na úrovni nad profily (sdíleno napříč všemi profily).

**Okamžité ukládání**: `SettingsManager` volá po každé jednotlivé změně
nastavení `sync()` (zápis na disk) — nic nečeká na zavření okna ani na
přepnutí profilu. Nastavení tak přežije i tvrdé ukončení nebo pád aplikace.

| Sekce | Klíč | Význam |
|---|---|---|
| General | remember_last_folder, last_folder | Obnovení poslední složky |
| FileHandling | enable_delete_image, enable_move_to_delete, ask_confirmation_delete | Režim mazání |
| FileHandling | move_companion_files | Přesouvat/mazat i párové soubory (obrázek/video se stejným názvem) |
| Processing | enable_images, enable_videos | Zpracovávat obrázky / videa |
| PDF | enable_pdf_processing | Zobrazovat PDF soubory |
| UI | layout, window_geometry, window_state, saved_screen_size | Rozložení a geometrie okna |
| Sort | key, ascending | Řazení souborů (0=název, 1=datum, 2=velikost) a směr |
| Cache | thumbnail_cache_enabled, thumbnail_cache_root | Disková cache náhledů |
| Video | volume | Hlasitost přehrávače videa |
| Categories | toolbar_visible | Viditelnost toolbaru štítků |
| Favorites | folders, colors, toolbar_visible | Oblíbené složky (max 10) + jejich toolbar |
| Move | ids, names, colors, folders, toolbar_visible | Tlačítka Přesun do složky + jejich toolbar |
| Navigation | toolbar_visible | Viditelnost toolbaru navigace mezi složkami |
| Updates | update_check_delay_minutes, update_check_interval_days, last_update_check, skipped_version | Kontrola aktualizací |

Po přepnutí profilu (`switchProfile()`) i po návratu z fullscreenu se
viditelnost všech čtyř sekundárních toolbarů (Oblíbené/Štítky/Přesun/Navigace)
a zaškrtávací volby v menu Nastavení znovu vynutí podle aktivního profilu —
nezůstávají "viset" hodnoty ze starého profilu nebo ze stavu před fullscreenem.

---

## Sestavení

Vyžaduje CMake ≥ 3.21 a Qt 6 (moduly Core, Concurrent, Gui, Widgets, Network,
Pdf; pro testy navíc Test).

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure   # jednotkové testy
open build/PictureViewer.app                  # macOS
```

Překlad zapíná `-Wall -Wextra` (GCC/Clang) / `/W3` (MSVC); strom je bez varování.
Na macOS se bundle automaticky podepíše ad-hoc podpisem s entitlements pro
přístup k souborům. Detaily nasazení: `MACOS_DEPLOYMENT.md`.

---

## Podporované formáty

Obrázky: seznam se odvozuje z `QImageReader::supportedImageFormats()`, tedy
z Qt pluginů nainstalovaných v systému — vždy JPG/JPEG, PNG, GIF (animovaný),
BMP, WEBP, TIFF/TIF a podle prostředí i HEIC, HEIF, SVG, JP2 a další.
Dokumenty: PDF (volitelně). Dokumentové přípony jsou z obrázkového seznamu
vyřazené, aby PDF šlo přes PDF render, ne přes obrázkový dekodér.

---

## Známé limity

- Otočení obrázku je pouze vizuální — neukládá se zpět do souboru.
- EXIF data (clona, ISO, expozice) se zatím nečtou — panel metadat v Pro
  režimu zobrazuje jen údaje o souboru.

Další náměty: `docs/TODO.md`.

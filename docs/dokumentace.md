# PictureViewer – Dokumentace

## Účel

Multiplatformní prohlížeč obrázků a PDF napsaný v **C++20 / Qt6** (macOS, Windows).
Umožňuje procházet obrázky ve složce, přibližovat/oddalovat, spouštět slideshow,
mazat a přejmenovávat soubory, prohlížet PDF dokumenty a přehrávat doprovodná
videa přes VLC. Nabízí pět přepínatelných rozložení UI.

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
│   ├── VlcController         – ovládání externího VLC přes RC rozhraní
│   └── HelpDialog            – dialogy nápovědy
├── core/                     – logika bez závislosti na GUI
│   ├── ImageCatalog          – výpis podporovaných souborů ze složky
│   ├── ImageFormats          – seznam podporovaných přípon
│   ├── ImageInfo             – struktura metadat souboru
│   ├── ImageMetadataReader   – čtení rozměrů/formátu/velikosti
│   └── PdfHandler            – načítání a render PDF (Qt PDF)
└── workers/                  – úlohy v QThreadPool
    ├── FolderScanWorker      – asynchronní sken složky (generační čítač)
    └── ThumbnailWorker       – generování náhledů + disková cache
```

---

## Klíčové mechanismy

### Asynchronní načítání obrázků (`ImageLoader`)
- Dekódování probíhá přes `QtConcurrent` mimo UI vlákno — listování nikdy neblokuje.
- **RAM cache** (LRU, limit 256 MB) s klíčem *cesta + mtime* — změna souboru na
  disku přirozeně zneplatní záznam.
- **Prefetch**: po zobrazení obrázku se na pozadí přednačtou oba sousedé;
  při sekvenčním listování je další obrázek vždy připraven.
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
| `G` | Přehrát video se stejným názvem ve VLC |
| `+` / `-` / kolečko | Zoom |
| `0` / `Space` / prostřední tlačítko | Originální velikost (1:1) |
| `Esc` | Konec fullscreenu / návrat do galerie / zavřít aplikaci |

Při aktivním VLC: `Space` pauza, `←`/`→` posun ±10 s, `+`/`-` hlasitost,
`F` fullscreen videa, `Esc` ukončení přehrávání.

---

## Nastavení (config.ini)

Umístění: `~/Library/Preferences/JiriKrejci/PictureViewer/config.ini` (macOS),
`%APPDATA%\JiriKrejci\PictureViewer\config.ini` (Windows).

| Sekce | Klíč | Význam |
|---|---|---|
| General | remember_last_folder, last_folder | Obnovení poslední složky |
| FileHandling | enable_delete_image, enable_move_to_delete, ask_confirmation_delete | Režim mazání |
| PDF | enable_pdf_processing | Zobrazovat PDF soubory |
| UI | layout | Zvolené rozložení (classic/filmstrip/immersive/gallery/pro) |
| Cache | thumbnail_cache_enabled, thumbnail_cache_root | Disková cache náhledů |
| VLC | vlc_path, vlc_timeout_ms | Cesta k VLC |
| Updates | … | Kontrola aktualizací |

---

## Sestavení

Vyžaduje CMake ≥ 3.21 a Qt 6 (moduly Core, Concurrent, Gui, Widgets, Network, Pdf).

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
open build/PictureViewer.app          # macOS
```

Na macOS se bundle automaticky podepíše ad-hoc podpisem s entitlements
pro přístup k souborům. Detaily nasazení: `MACOS_DEPLOYMENT.md`.

---

## Podporované formáty

Obrázky: JPG, JPEG, PNG, GIF (statický), BMP, WEBP, TIFF/TIF.
Dokumenty: PDF (volitelně).

---

## Známé limity

- GIF animace se zobrazují jako statický první snímek.
- EXIF data (clona, ISO, expozice) se zatím nečtou — panel metadat v Pro
  režimu zobrazuje jen údaje o souboru.

Další náměty: `docs/TODO.md`.

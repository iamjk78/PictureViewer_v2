# PictureViewer v2

[![CI](https://github.com/iamjk78/PictureViewer_v2/actions/workflows/ci.yml/badge.svg)](https://github.com/iamjk78/PictureViewer_v2/actions/workflows/ci.yml)

Multiplatformní prohlížeč obrázků a PDF napsaný v C++20 / Qt6. Aktuální verze **0.20**.

## Funkce

- Procházení obrázků ve složce — seznam formátů se odvozuje z nainstalovaných
  Qt pluginů (JPG, PNG, GIF, BMP, WEBP, TIFF a podle prostředí i HEIC, HEIF,
  SVG, JP2…)
- **Animované GIFy** přehrávané přes QMovie
- Prohlížení PDF dokumentů s listováním stránek; **PDF toolbar** s tlačítky ◀/▶,
  indikátorem strany, skokem na konkrétní stranu a snímkem stránky jako JPEG
- Přibližování a posun obrázků (zoom, pan) + **indikátor zoomu** ve status baru
- **Otočení obrázku** o 90° (vizuální, klávesy `[`/`L` doleva, `]` doprava)
- **Ořez obrázku** (✂) — výběr oblasti myší, zobrazení se ořízne na vybranou část
- **Snímek výřezu obrazovky** (📷) — funguje i mimo aplikaci a napříč všemi monitory;
  označte oblast myší a výřez se otevře v aplikaci (dočasně uložen do temp složky).
  macOS používá systémový `screencapture`, ostatní platformy vlastní overlay
- **Přizpůsobitelná šířka panelu náhledů** — táhněte pravý okraj panelu doleva/doprava;
  miniatury se automaticky přizpůsobí šířce; nastavení se pamatuje mezi spuštěními
- **Uložit / Uložit jako** — uložení upraveného obrázku (ořez, otočení) jako JPEG;
  Uložit nabídne přepsání originálu nebo přejmenování; Uložit jako otevře dialog
  pro výběr názvu a cílové složky (originál nebo oblíbená)
- **Recyklace (♻)** — vrácení souborů přesunutých do Delete zpět do původní složky
  (LIFO — nejnovější jako první)
- **Obnovit složku (🔄 / F5)** — znovu naskenuje složku a zobrazí přidané/odebrané soubory
- **Řazení v toolbaru** — dropdown tlačítko (název / datum / velikost, ↑/↓)
- **Drag & drop** složky nebo souboru do okna
- **Kontextové menu** — Zobrazit ve Finderu, kopírovat obrázek / cestu
- Slideshow s nastavitelným intervalem (1–60 s)
- Mazání a přejmenování souborů
- **Štítky obrázků** — až 5 barevných štítků na obrázek; filtrování složky;
  správa (přejmenovat, změnit barvu, smazat) přes pravý klik
- **Oblíbené složky** (⭐) — toolbar s barevnými tlačítky pro rychlé přepínání;
  max 10 složek, klik otevře složku, pravý klik odebere
- 5 přepínatelných rozložení UI (Klasický, Filmový pás, Imerzivní, Galerie, Pro)
- Asynchronní načítání obrázků s RAM cache + disková cache náhledů (auto-úklid)
- **Inline přehrávač videa** (Qt Multimedia) — MP4, MKV, MOV, WebM a další přímo v okně aplikace;
  přehrávání v nekonečné smyčce; zoom (+/-), posun myší, fullscreen, posun šipkami,
  hlasitost, buffer overlay; metadata (velikost, rozlišení, délka, datový tok) ve status baru
- **Automatické aktualizace** — Nápověda → Zkontrolovat aktualizace… stáhne nové
  vydání z GitHubu, ověří SHA256 a na Windows rovnou nainstaluje
- **Pamatování velikosti okna** — obnoví polohu a velikost; při jiném rozlišení
  obrazovky se spustí v předvolené velikosti
- **Jednotná výška toolbarů** — všechny lišty (Oblíbené, Štítky, PDF) mají
  stejnou výšku a font jako hlavní toolbar
- Jednotkové testy jádra (Qt Test)

## Buildování

### Požadavky

Všechny platformy:
- **CMake** 3.21+ ([cmake.org](https://cmake.org))
- **Qt 6.5+** ([qt.io](https://www.qt.io/download))

### macOS

```bash
# Instalace Qt (přes Homebrew)
brew install qt

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Spuštění
./PictureViewer.app/Contents/MacOS/PictureViewer

# Jednotkové testy
ctest --output-on-failure
```

**Poznámka**: Pokud máš Qt nainstalovanou na jinou cestu, použij:
```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.5.3 -DCMAKE_BUILD_TYPE=Release
```

### Windows

#### 1. Instalace závislostí

**Visual Studio 2022** (Community Edition je free)
- Stáhni z [microsoft.com](https://visualstudio.microsoft.com)
- Při instalaci vyber "Desktop development with C++"

**CMake**
- Stáhni z [cmake.org](https://cmake.org)
- Instaluj běžným .exe installerem

**Qt 6.5.3+**
- Stáhni Qt Online Installer z [qt.io](https://www.qt.io/download)
- Spusť instalátor
- Vyber: Qt → 6.5.3 (nebo novější) → MSVC 2022 64-bit
- Zapamatuj si instalační cestu (např. `C:\Qt\6.5.3`)

#### 2. Build v PowerShellu nebo cmd

```powershell
git clone https://github.com/iamjk78/PictureViewer_v2.git
cd PictureViewer_v2
mkdir build
cd build

# Nahraď C:\Qt\6.5.3 svou cestou k Qt instalaci
cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.5.3\msvc2019_64" `
  -DCMAKE_BUILD_TYPE=Release

cmake --build . --config Release --parallel
```

#### 3. Spuštění

```powershell
.\Release\PictureViewer.exe
```

---

## Ovládání

| Klávesa | Akce |
|---|---|
| `←` / `→` | Předchozí / další obrázek; posun ±10 % (ve videu) |
| `Shift+←` / `Shift+→` | Předchozí / další soubor (ve videu) |
| `↑` / `↓` | První / poslední obrázek; začátek / stop (ve videu) |
| `PageUp` / `PageDown` | Předchozí / následující stránka PDF |
| `S` | Spustit / zastavit slideshow |
| `F` | Celá obrazovka |
| `D` / `Delete` | Smazat nebo přesunout do Delete |
| `R` | Přejmenovat soubor |
| `F5` | Znovu načíst složku |
| `[` / `L` , `]` | Otočit doleva / doprava |
| `V` | Spustit video |
| `Space` | Přehrát / pauza (ve videu) |
| `+` / `-` | Zoom (obrázky i video) |
| `0` / `Space` | Originální velikost / přizpůsobit oknu |
| `Esc` | Konec fullscreenu / zrušit ořez |

Pravým tlačítkem nad obrázkem se otevře kontextové menu (Zobrazit ve Finderu,
kopírovat obrázek / cestu). Složku nebo soubor lze také přetáhnout do okna.

---

## Nastavení

Konfigurace se ukládá v:
- **macOS**: `~/Library/Preferences/JiriKrejci/PictureViewer/config.ini`
- **Windows**: `%APPDATA%\JiriKrejci\PictureViewer\config.ini`

Menu Nastavení přistupuje k:
- Vzhled aplikace (5 rozložení UI)
- Řazení souborů (název / datum / velikost, vzestupně / sestupně)
- Zapamatování poslední složky
- Režim mazání souborů
- Cache náhledů (povolen/zakázán, volba složky, aktuální velikost)
- Zpracování PDF

---

## Dokumentace

Detailní dokumentace: [docs/dokumentace.md](docs/dokumentace.md)  
Náměty na vylepšení: [docs/TODO.md](docs/TODO.md)

---

## Vývoj

Projekt je napsaný v C++20 s Qt6 frameworkem. Architektura:
- `src/app/` — GUI komponenty (Qt widgety)
- `src/core/` — logika bez GUI
- `src/workers/` — asynchronní úlohy (FolderScanWorker, ThumbnailWorker)
- `tests/` — jednotkové testy jádra (Qt Test), spustitelné přes `ctest`

---

Máš otázky? Otevři [issue](https://github.com/iamjk78/PictureViewer_v2/issues).

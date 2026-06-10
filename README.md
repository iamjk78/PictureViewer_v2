# PictureViewer v2

Multiplatformní prohlížeč obrázků a PDF napsaný v C++20 / Qt6.

## Funkce

- Procházení obrázků ve složce (JPG, PNG, GIF, BMP, WEBP, TIFF)
- Prohlížení PDF dokumentů
- Přibližování a posun obrázků (zoom, pan)
- Slideshow s nastavitelným intervalem
- Mazání a přejmenování souborů
- 5 přepínatelných rozložení UI (Klasický, Filmový pás, Imerzivní, Galerie, Pro)
- Asynchronní načítání obrázků s cache
- Disková cache náhledů
- Přehrávání videí přes VLC

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
| `←` / `→` | Předchozí / další obrázek |
| `↑` / `↓` | První / poslední obrázek |
| `PageUp` / `PageDown` | Stránka PDF / další obrázek |
| `S` | Spustit / zastavit slideshow |
| `F` | Celá obrazovka |
| `D` / `Delete` | Smazat nebo přesunout do Delete |
| `R` | Přejmenovat soubor |
| `G` | Přehrát video ve VLC |
| `+` / `-` | Zoom |
| `0` / `Space` | Originální velikost |
| `Esc` | Konec fullscreenu / zavřít |

---

## Nastavení

Konfigurace se ukládá v:
- **macOS**: `~/Library/Preferences/JiriKrejci/PictureViewer/config.ini`
- **Windows**: `%APPDATA%\JiriKrejci\PictureViewer\config.ini`

Menu Nastavení přístupuje k:
- Zapamatování poslední složky
- Režim mazání souborů
- Cache náhledů (povolen/zakázán, volba složky)
- Zpracování PDF

---

## Dokumentace

Detailní dokumentace: [docs/dokumentace.md](docs/dokumentace.md)  
Náměty na vylepšení: [docs/TODO.md](docs/TODO.md)

---

## Známé limity

- GIF animace se zobrazují jako statický první snímek
- EXIF data (clona, ISO, expozice) se zatím nečtou

---

## Vývoj

Projekt je napsaný v C++20 s Qt6 frameworkem. Architektura:
- `src/app/` — GUI komponenty (Qt widgety)
- `src/core/` — logika bez GUI
- `src/workers/` — asynchronní úlohy (FolderScanWorker, ThumbnailWorker)

---

Máš otázky? Otevři [issue](https://github.com/iamjk78/PictureViewer_v2/issues).

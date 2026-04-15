# macOS Deployment Guide - PictureViewer

Kompletní průvodce pro vytvoření, testování a distribuci PictureViewer jako macOS .app balíčku.

---

## 📋 Obsah

1. [Požadavky](#požadavky)
2. [Automatizované Build](#automatizované-build)
3. [Struktura .app balíčku](#struktura-app-balíčku)
4. [Info.plist konfigurace](#infoplist-konfigurace)
5. [Podpora otevírání souborů](#podpora-otevírání-souborů)
6. [Testování](#testování)
7. [Distribuci](#distribuce)
8. [Řešení problémů](#řešení-problémů)

---

## Požadavky

### Povinné
- **macOS 11.0+** (nastaven v CMakeLists.txt)
- **Xcode Command Line Tools**: `xcode-select --install`
- **CMake 3.21+**: `brew install cmake`
- **Qt 6.5+**: `brew install qt`
- **Git** (pro správu verzí)

### Volitelné (pro pokročilé deployment)
- **Codesign certificate** (pro podepisování aplikace)
- **Notarization** (pro distribuce přes App Store/notarization)

### Ověření instalace
```bash
cmake --version              # CMake 3.21+
/opt/homebrew/bin/qmake -v  # Qt 6.5+
xcode-select --print-path   # Xcode tools
```

---

## Automatizované Build

Nejjednodušší cesta k vytvoření aplikace:

### Krok 1: Spustit build skript

```bash
cd /Users/jirikrejci/CladeAI/Projects/PictureViewer
./scripts/build-macos-app.sh
```

Skript automaticky:
- Konfiguruje CMake
- Zkompiluje aplikaci (Release mode)
- Zabalí dependencies (Qt libraries)
- Ověří strukturu balíčku
- Vytvoří `dist/PictureViewer.app`

### Krok 2: Spustit aplikaci

```bash
# Možnost A: Z Finderu
open dist/PictureViewer.app

# Možnost B: Z terminálu
dist/PictureViewer.app/Contents/MacOS/PictureViewer

# Možnost C: Instalace do Applications
cp -r dist/PictureViewer.app ~/Applications/PictureViewer.app
```

---

## Struktura .app balíčku

Po buildu vypadá struktura takto:

```
PictureViewer.app/
├── Contents/
│   ├── MacOS/
│   │   └── PictureViewer          # Hlavní executable
│   ├── Resources/
│   │   ├── eye_icon.ico           # Application icon
│   │   ├── qt.conf                # Qt konfigurace (automaticky)
│   │   └── qt_plugins/            # Qt plugin libraries (automaticky)
│   │       ├── imageformats/
│   │       ├── platforms/
│   │       └── ...
│   ├── Frameworks/                # Qt frameworks (automaticky)
│   │   ├── QtCore.framework/
│   │   ├── QtGui.framework/
│   │   ├── QtWidgets.framework/
│   │   └── ...
│   └── Info.plist                 # Bundle metadata
├── PkgInfo                        # Package type (automaticky)
└── _CodeSignature/                # Signature (pokud je app podepsaná)
```

### Klíčové soubory

| Soubor | Obsah |
|--------|-------|
| `Info.plist` | Metadata: version, bundle ID, file types, icon |
| `PictureViewer` | Compiled executable (binární) |
| `eye_icon.ico` | Application icon (128x128, 32-bit) |
| `Qt frameworks/` | Dynamické Qt libraries |

---

## Info.plist Konfigurace

### Vygenerovaná struktura

Info.plist je automaticky vygenerován z `cmake/PictureViewerInfo.plist.in` s těmito klíčovými sekcemi:

#### 1. Bundle Information
```xml
<key>CFBundleIdentifier</key>
<string>com.jk78.pictureviewer</string>

<key>CFBundleName</key>
<string>PictureViewer</string>

<key>CFBundleVersion</key>
<string>0.1.0</string>
```

#### 2. Ikona aplikace
```xml
<key>CFBundleIconFile</key>
<string>eye_icon.ico</string>
```

#### 3. **Typ souborů (CFBundleDocumentTypes)**

PictureViewer je registrován pro otevírání těchto typů obrázků:

```xml
<key>CFBundleDocumentTypes</key>
<array>
    <dict>
        <key>CFBundleTypeExtensions</key>
        <array>
            <string>jpg</string>
            <string>jpeg</string>
            <string>JPG</string>
            <string>JPEG</string>
        </array>
        <key>CFBundleTypeMIMETypes</key>
        <array>
            <string>image/jpeg</string>
        </array>
        <key>CFBundleTypeRole</key>
        <string>Viewer</string>
        <key>CFBundleTypeName</key>
        <string>JPEG Image</string>
    </dict>
    <!-- PNG, WebP, GIF, TIFF, BMP ... -->
</array>
```

Podporované formáty:
- **JPEG/JPG** (.jpg, .jpeg)
- **PNG** (.png)
- **WebP** (.webp)
- **GIF** (.gif)
- **TIFF** (.tiff, .tif)
- **BMP** (.bmp)

#### 4. Systémové požadavky
```xml
<key>LSMinimumSystemVersion</key>
<string>11.0</string>

<key>NSHighResolutionCapable</key>
<true/>
```

---

## Podpora otevírání souborů

### Architektura

PictureViewer používá **vlastní `PictureViewerApplication`** třídu (dědí z `QApplication`), která:

1. Sleduje `QEvent::FileOpen` události
2. Přijímá cesty souborů z:
   - **Finder** (Double-click na soubor)
   - **Open With** (Cmd+Rightclick → Open With)
   - **Drag & drop** do .app balíčku
   - **Command line**: `./PictureViewer.app/Contents/MacOS/PictureViewer image.jpg`

### Event flow

```
macOS sends QFileOpenEvent
    ↓
PictureViewerApplication::event()
    ↓
MainWindow::openFile(filePath)
    ↓
Load folder + select image
```

### Implementace

#### Application.hpp
```cpp
class PictureViewerApplication : public QApplication {
    Q_OBJECT
public:
    bool event(QEvent *event) override;
};
```

#### Application.cpp
```cpp
bool PictureViewerApplication::event(QEvent *event) {
    if (event->type() == QEvent::FileOpen) {
        auto fileEvent = dynamic_cast<QFileOpenEvent*>(event);
        if (fileEvent && m_mainWindow) {
            m_mainWindow->openFile(fileEvent->file());
            return true;
        }
    }
    return QApplication::event(event);
}
```

#### MainWindow.hpp
```cpp
void openFile(const QString &filePath);
```

#### MainWindow.cpp
```cpp
void MainWindow::openFile(const QString &filePath) {
    m_requestedFile = filePath;
    const QString folderPath = filePath.section('/', 0, -2);
    loadFolder(folderPath);
}
```

---

## Testování

### Test 1: Spuštění z Finderu

```bash
open dist/PictureViewer.app
```

**Expect**: Aplikace se otevře s prázdným UI (bez otevřené složky)

### Test 2: Otevření souboru - Direct Launch

```bash
open -a dist/PictureViewer.app ~/Pictures/sample.jpg
```

**Expect**: Aplikace se otevře a zobrazí `sample.jpg`

### Test 3: Otevření z terminálu

```bash
dist/PictureViewer.app/Contents/MacOS/PictureViewer ~/Pictures/sample.jpg
```

**Expect**: Stejné jako Test 2

### Test 4: Nastavení jako výchozí aplikace

1. Klikni pravým tlačítkem na `.jpg` soubor v Finderu
2. Vyber **Open With** → **Other...**
3. Naviguj k `dist/PictureViewer.app`
4. Zaškrtni **Always Open With**
5. Klikni **Open**

**Expect**: 
- Budoucí double-click na `.jpg` otevře PictureViewer
- Zkontroluj: `duti` utility nebo System Preferences → Default Apps

### Test 5: Ověření Info.plist

```bash
# Zobrazit Info.plist
plutil -p dist/PictureViewer.app/Contents/Info.plist

# Kontrola CFBundleDocumentTypes
plutil -p dist/PictureViewer.app/Contents/Info.plist | grep -A 50 CFBundleDocumentTypes

# Kontrola bundle ID
mdls dist/PictureViewer.app | grep com.jk78
```

### Test 6: Launch Services ověření

```bash
# Zkontrolovat, co macOS zná o aplikaci
launchctl list | grep pictureviewer

# Ověřit registraci typů souborů
duti com.jk78.pictureviewer
```

### Test 7: Code Signature Check

```bash
# Zkontrolovat signaturu (unsigned je ok pro vývoj)
codesign -v dist/PictureViewer.app
```

Očekávaný výstup pro unsigned app:
```
In architecture: x86_64
not signed at all
```

---

## Distribuce

### Možnost A: Lokální instalace

```bash
# Zkopíruj do Applications
cp -r dist/PictureViewer.app ~/Applications/

# Nyní je aplikace v Spotlight (Cmd+Space)
# a v Applications složce
```

### Možnost B: Archiv pro distribuci

```bash
# Vytvoř .zip balíček
ditto -c -k --sequesterRsrc dist/PictureViewer.app PictureViewer-0.1.0.zip

# Nebo vytvořit disk image (DMG)
hdiutil create -srcfolder dist/ -volname "PictureViewer" -ov -format UDZO PictureViewer-0.1.0.dmg
```

### Možnost C: Notarization (pro App Store / Gatekeeper)

Pro distribuci přes App Store nebo pro obcházení Gatekeeper varování:

```bash
# 1. Podepsat aplikaci (vyžaduje vývojářský certifikát)
codesign --deep --force --verify --verbose --sign - dist/PictureViewer.app

# 2. Vytvořit .zip
ditto -c -k --sequesterRsrc dist/PictureViewer.app dist.zip

# 3. Odeslat na notarization
xcrun notarytool submit dist.zip \
    --keychain-profile "AC_PASSWORD" \
    --wait

# 4. Staple ticket
xcrun stapler staple dist/PictureViewer.app
```

---

## Řešení problémů

### Chyba: "Soubor je poškozen"

**Symptom**: "PictureViewer cannot be opened because the developer cannot be verified"

**Řešení**:
```bash
# Povoluj aplikaci jednou
xattr -d com.apple.quarantine ~/Applications/PictureViewer.app

# Nebo v System Preferences:
# Security & Privacy → General → Allow apps from "Anywhere"
```

### Chyba: Qt libraries nenalezeny

**Symptom**: Aplikace se neotevře s chybou o chybějících Qt knihovnách

**Řešení**:
```bash
# Znovu spustit build script
./scripts/build-macos-app.sh

# Nebo ručně zabalit dependencies
/opt/homebrew/opt/qt/libexec/macdeployqt dist/PictureViewer.app
```

### Ikona se nezobrazuje

**Symptom**: Aplikace se zobrazuje s generickou ikonou

**Ověření**:
```bash
# Zkontroluj, že Icon existuje
ls -la dist/PictureViewer.app/Contents/Resources/eye_icon.ico

# Zkontroluj Info.plist
grep CFBundleIconFile dist/PictureViewer.app/Contents/Info.plist
```

**Řešení**:
```bash
# Restartuj Finder
killall Finder

# Nečisti Finder cache
rm -rf ~/.Trash/*
```

### Otevírání souboru nefunguje

**Symptom**: Double-click na soubor neotevře aplikaci

**Ověření**:
```bash
# Testuj přímo
open -a dist/PictureViewer.app ~/Pictures/test.jpg

# Zkontroluj Launch Services
LaunchServices.log
```

**Řešení**:
1. Smaž Launch Services cache: `rm -rf ~/Library/Preferences/com.apple.LaunchServices.QuarantineResolver`
2. Restartuj Finder: `killall Finder`
3. Znovu nastav výchozí aplikaci

### Chyba kompilace s Qt

**Chyba**: `Qt6::Widgets not found` apod.

**Řešení**:
```bash
# Zkontroluj Qt instalaci
brew list qt

# Znovu instaluj
brew uninstall qt && brew install qt

# Ověř Qt cestu
/opt/homebrew/opt/qt/bin/qmake -v
```

---

## Checklist pro produkční release

- [ ] Aplikace se otevírá bez chyb
- [ ] Ikona se zobrazuje správně v Dock
- [ ] Lze otevřít soubor přes Finder
- [ ] Lze nastavit jako výchozí aplikaci
- [ ] FileOpen events fungují (přechozí seznam - Open With)
- [ ] Zoom, Pan, Slideshow fungují
- [ ] Info.plist obsahuje všechny formáty obrázků
- [ ] Bundle ID je `com.jk78.pictureviewer`
- [ ] Version je správně nastavena
- [ ] Minimální macOS verze je 11.0
- [ ] Žádné chyby v konzoli (`log stream --predicate 'process == "PictureViewer"'`)
- [ ] Binární velikost je pod 500 MB (s Qt)

---

## Příslušné soubory v projektu

```
PictureViewer/
├── CMakeLists.txt                    # Nastavení buildu
├── cmake/
│   └── PictureViewerInfo.plist.in    # Template Info.plist
├── scripts/
│   └── build-macos-app.sh            # Build skript
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── Application.hpp/.cpp      # PictureViewerApplication
│   │   └── MainWindow.hpp/.cpp       # FileOpen support
│   └── ...
├── resources/
│   └── icons/
│       └── eye_icon.ico              # Application icon
└── dist/
    └── PictureViewer.app             # Výsledný balíček
```

---

## Příkazy pro rychlé otestování

```bash
# Kompilace
./scripts/build-macos-app.sh

# Spuštění
open dist/PictureViewer.app

# Test otevření souboru
open -a dist/PictureViewer.app ~/Pictures/photo.jpg

# Nainstalování do Applications
cp -r dist/PictureViewer.app ~/Applications/

# Smazání cache
rm -rf ~/Library/Preferences/com.apple.LaunchServices.QuarantineResolver
killall Finder

# Kontrola Log messages
log stream --predicate 'process == "PictureViewer"' --level debug
```

---

**Verze**: 1.0  
**Poslední update**: 2026-04-15  
**Platform**: macOS 11.0+, Apple Silicon + Intel  
**Qt verze**: 6.5+

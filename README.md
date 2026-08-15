# PictureViewer v2

*English version — [česká verze](README.cs.md)*

[![CI](https://github.com/iamjk78/PictureViewer_v2/actions/workflows/ci.yml/badge.svg)](https://github.com/iamjk78/PictureViewer_v2/actions/workflows/ci.yml)

A fast, no-nonsense image and PDF viewer written in C++20 / Qt6 for **macOS** and
**Windows**. Current version **0.29**.

> **Note on language:** the application's user interface and built-in help are in
> **Czech** — it is primarily a tool for Czech users. This README, commit messages
> and release notes are in English.

## Features

- Browse images in a folder — the supported format list is derived from the
  installed Qt plugins (JPG, PNG, GIF, BMP, WEBP, TIFF and, depending on the
  environment, HEIC, HEIF, SVG, JP2…)
- **Animated GIFs** played through QMovie
- View PDF documents with page navigation; a dedicated **PDF toolbar** with
  ◀/▶ buttons, a page indicator, jump-to-page and page-to-JPEG capture
- Zoom and pan, plus a **zoom indicator** in the status bar
- **Shrink / enlarge to window** — two toggles in the status bar decide how an
  image is displayed when loaded; with both off everything is shown at its
  original size (1:1). Manual zoom temporarily overrides the choice. The setting
  is per-profile and does not apply to PDFs or video
- **Rotate image** by 90° (visual only; `[`/`L` left, `]` right)
- **Crop** — select an area with the mouse and the view is cropped to it
- **Screen region capture** — works outside the application and across all
  monitors; select an area and the capture opens in the app (stored temporarily).
  macOS uses the system `screencapture`, other platforms a custom overlay
- **Resizable thumbnail panel** — drag its right edge; thumbnails scale
  automatically and the width is remembered between runs
- **Save / Save as** — store an edited image (crop, rotation) as JPEG; Save
  offers overwriting the original or renaming, Save as opens a dialog for the
  name and target folder
- **Restore last file** — moves files sent to the Delete folder back where they
  came from (LIFO — most recent first)
- **Reload folder** (F5) — rescans the folder and picks up added/removed files
- **Sorting** — dropdown in the toolbar (name / date / size, ascending/descending)
- **Drag & drop** a folder or file onto the window
- **Context menu** — reveal in Finder, copy image / path
- Slideshow with a selectable interval (1, 2, 3, 5, 10, 20 or 30 s) via a
  toolbar dropdown
- Deleting and renaming files; on volumes without trash support (typically SMB
  network shares) the app offers permanent deletion with an explicit confirmation
- **Image labels** — up to 5 colour labels per image; filter the folder by them;
  rename, recolour or delete a label from the right-click menu
- **Favourite folders** — a toolbar of colour buttons for quick switching;
  up to 10 folders, click to open, right-click to remove
- **Move to folder** — user-defined buttons for sorting files by topic (name,
  colour, target folder); bulk move via multi-selection in the thumbnail panel
  (Ctrl/Shift+click); never overwrites on a name clash; moves can be undone
  (LIFO); buttons are per-profile
- **Companion files (image/video)** — optional, per-profile: moving or deleting a
  file also moves/deletes other images and videos sharing its base name in the
  same folder (e.g. `123.jpg` + `123.mp4`); PDFs are never paired; with several
  matches a dialog offers all / active only / cancel; undo restores the whole
  group at once
- **Folder navigation** — four buttons for moving around the directory tree:
  ◀/▶ to the alphabetically previous/next sibling, ▲ one level up, ▼ into the
  first subfolder; each button shows the target name and how many folders lie in
  that direction; the internal "Delete" folder is excluded; the structure is
  probed only while the panel is enabled and re-checked on every click, so a
  folder deleted or renamed in the meantime causes no error
- **Settings and toolbars are per-profile and saved immediately** — every change
  (toolbars, image/video/PDF processing, deletion, cache, sorting…) is written to
  the config right away rather than on exit, so it survives a crash or a forced
  quit. Switching profiles or leaving fullscreen restores everything exactly as
  the active profile has it
- 5 switchable UI layouts (Classic, Filmstrip, Immersive, Gallery, Pro)
- Asynchronous image loading with a RAM cache plus a disk thumbnail cache
  (auto-pruned)
- **Inline video player** (Qt Multimedia) — MP4, MKV, MOV, WebM and more, played
  in the app window; loops until stopped; zoom (+/-), mouse pan, fullscreen,
  arrow-key seeking, volume, buffering overlay; metadata (size, resolution,
  duration, bitrate) and the file's position in the folder in the status bar
- **Automatic updates** — Help → Check for updates downloads a new release from
  GitHub, verifies its SHA256 and on Windows installs it directly
- **Window geometry is remembered** — restores position and size; falls back to
  the default size when the screen resolution differs
- **Consistent toolbar look** — custom colour icons, uniform button size, no
  borders or separator lines; the PDF toolbar always sits below the others
- Unit tests for the core (Qt Test)

## Building

### Requirements

All platforms:
- **CMake** 3.21+ ([cmake.org](https://cmake.org))
- **Qt 6.5+** ([qt.io](https://www.qt.io/download))

### macOS

```bash
# Install Qt (via Homebrew)
brew install qt

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Run
./PictureViewer.app/Contents/MacOS/PictureViewer

# Unit tests
ctest --output-on-failure
```

**Note**: if Qt is installed elsewhere, point CMake at it:
```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.5.3 -DCMAKE_BUILD_TYPE=Release
```

### Windows

#### 1. Install dependencies

**Visual Studio 2022** (the Community Edition is free)
- Download from [microsoft.com](https://visualstudio.microsoft.com)
- Select "Desktop development with C++" during installation

**CMake**
- Download from [cmake.org](https://cmake.org)
- Install with the regular .exe installer

**Qt 6.5.3+**
- Download the Qt Online Installer from [qt.io](https://www.qt.io/download)
- Run it and select: Qt → 6.5.3 (or newer) → MSVC 2022 64-bit
- Note the installation path (e.g. `C:\Qt\6.5.3`)

#### 2. Build in PowerShell or cmd

```powershell
git clone https://github.com/iamjk78/PictureViewer_v2.git
cd PictureViewer_v2
mkdir build
cd build

# Replace C:\Qt\6.5.3 with your Qt installation path
cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.5.3\msvc2019_64" `
  -DCMAKE_BUILD_TYPE=Release

cmake --build . --config Release --parallel
```

#### 3. Run

```powershell
.\Release\PictureViewer.exe
```

---

## Controls

| Key | Action |
|---|---|
| `←` / `→` | Previous / next image; seek ±10 % (in video) |
| `Shift+←` / `Shift+→` | Previous / next file (in video) |
| `↑` / `↓` | First / last image; restart / stop (in video) |
| `PageUp` / `PageDown` | Previous / next PDF page |
| `S` | Start / stop slideshow |
| `F` | Fullscreen |
| `D` / `Delete` | Delete or move to the Delete folder |
| `R` | Rename file |
| `F5` | Reload folder |
| `[` / `L` , `]` | Rotate left / right |
| `V` | Play video |
| `Space` | Play / pause (in video) |
| `+` / `-` | Zoom (images and video) |
| `0` / `Space` | Original size / fit to window |
| `Esc` | Leave fullscreen / cancel crop |

Right-clicking an image opens a context menu (reveal in Finder, copy image /
path). Folders and files can also be dropped onto the window.

---

## Settings

The configuration is **per-profile** and lives in:
- **macOS**: `~/Library/Preferences/JiriKrejci/PictureViewer/profiles/<profile>/config.ini`
- **Windows**: `%APPDATA%\JiriKrejci\PictureViewer\profiles\<profile>\config.ini`

The Settings menu covers:
- Application appearance (5 UI layouts)
- File sorting (name / date / size, ascending / descending)
- Remembering the last folder
- File deletion mode
- Thumbnail cache (enable/disable, folder, current size)
- PDF processing

---

## Development

Written in C++20 with the Qt6 framework. Layout:
- `src/app/` — GUI components (Qt widgets)
- `src/core/` — GUI-free logic
- `src/workers/` — asynchronous tasks (FolderScanWorker, ThumbnailWorker)
- `tests/` — unit tests for the core (Qt Test), run via `ctest`

---

Questions? Open an [issue](https://github.com/iamjk78/PictureViewer_v2/issues).

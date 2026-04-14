# PictureViewer – Dokumentace

## Účel

Multiplatformní prohlížeč obrázků s grafickým rozhraním (PySide6).
Umožňuje procházet obrázky ve složce, přibližovat/oddalovat,
spouštět slideshow a zobrazovat základní metadata souboru.

---

## Architektura

```
src/picture_viewer/
├── main.py              – vstupní bod
├── app.py               – inicializace QApplication
├── main_window.py       – hlavní okno (menu, toolbar, status bar)
├── image_view.py        – widget pro zobrazení + zoom/pan
├── thumbnail_panel.py   – postranní panel s náhledy
├── image_loader.py      – načítání obrázků, dataclass ImageInfo
└── slideshow.py         – řízení automatického přepínání
```

Závislosti mezi moduly:

```
main.py → app.py → main_window.py
main_window.py → image_view.py
               → thumbnail_panel.py
               → slideshow.py
               → image_loader.py
```

---

## Popis modulů

### `image_loader.py`
- `ImageInfo` – zmrazená dataclass s metadaty obrázku (cesta, rozměry, velikost, formát)
- `load_folder(folder)` – vrátí seřazený seznam obrázků ze složky
- `load_image_info(path)` – načte metadata jednoho obrázku pomocí Pillow

### `image_view.py`
- `ImageView(QGraphicsView)` – zobrazuje obrázek přes QGraphicsScene
- Zoom kolečkem myši nebo klávesami `+`/`-`/`0`/`F`
- Posun tažením myši (ScrollHandDrag)
- `load_image(path)` – načte a zobrazí obrázek
- `fit_to_window()` – přizpůsobí obrázek oknu

### `thumbnail_panel.py`
- `ThumbnailPanel(QListWidget)` – mřížka miniaturních náhledů
- Signál `image_selected(int)` – index kliknutého obrázku
- `load_images(paths)` – naplní panel náhledy
- `set_current_index(index)` – zvýrazní aktivní obrázek

### `slideshow.py`
- `SlideshowController(QObject)` – ovládá QTimer
- Signál `next_image()` – vysílán v každém intervalu
- `start()`, `stop()`, `toggle()`, `set_interval(ms)`
- Minimální interval: 500 ms, výchozí: 3 000 ms

### `main_window.py`
- `MainWindow(QMainWindow)` – propojuje všechny komponenty
- Menu: Soubor (otevřít složku/soubor, konec), Zobrazení (fit, 1:1, panel)
- Toolbar: navigace, slideshow, nastavení intervalu
- Status bar: název souboru, rozměry, formát, velikost, pozice v seznamu

### `app.py` / `main.py`
- Inicializace aplikace a spuštění event loop

---

## Použití

### Instalace

```bash
pip install -e .
```

### Spuštění

```bash
python -m picture_viewer
```

nebo po instalaci:

```bash
picture-viewer
```

### Ovládání klávesnicí

| Klávesa       | Akce                        |
|---------------|-----------------------------|
| `←` / `→`    | Předchozí / Další obrázek   |
| `Space`       | Spustit / Zastavit slideshow |
| `+` / `-`    | Přiblížit / Oddálit          |
| `0`           | Originální velikost (1:1)   |
| `F`           | Přizpůsobit oknu             |

---

## Spuštění testů

```bash
pytest tests/
```

---

## Známé problémy

- GIF animace jsou zobrazovány jako statický první snímek (PySide6 QPixmap nenačítá animace přímo).

---

## TODO

- [ ] Podpora pro animované GIF (použít QMovie)
- [ ] Přetahování souboru do okna (drag & drop)
- [ ] Otočení / převrácení obrázku
- [ ] Fullscreen režim

"""Hlavní okno aplikace PictureViewer."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtWidgets import (
    QDockWidget,
    QFileDialog,
    QLabel,
    QMainWindow,
    QSpinBox,
    QToolBar,
    QWidget,
)

from .image_loader import ImageInfo, load_folder, load_image_info
from .image_view import ImageView
from .slideshow import SlideshowController
from .thumbnail_panel import ThumbnailPanel


class MainWindow(QMainWindow):
    """Hlavní okno s menu, toolbarem, panelem náhledů a zobrazením obrázků."""

    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("PictureViewer")
        self.resize(1200, 750)

        self._image_paths: list[Path] = []
        self._current_index: int = -1

        self._image_view = ImageView(self)
        self.setCentralWidget(self._image_view)

        self._thumbnail_panel = ThumbnailPanel(self)
        self._thumbnail_panel.image_selected.connect(self._show_image)

        self._slideshow = SlideshowController(self)
        self._slideshow.next_image.connect(self._next_image)

        self._setup_dock()
        self._setup_menu()
        self._setup_toolbar()
        self._setup_statusbar()

    # ------------------------------------------------------------------
    # Sestavení UI
    # ------------------------------------------------------------------

    def _setup_dock(self) -> None:
        dock = QDockWidget("Náhledy", self)
        dock.setWidget(self._thumbnail_panel)
        dock.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetMovable
            | QDockWidget.DockWidgetFeature.DockWidgetClosable
        )
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, dock)
        self._thumbnail_dock = dock

    def _setup_menu(self) -> None:
        menubar = self.menuBar()

        soubor = menubar.addMenu("&Soubor")

        act_open_folder = QAction("Otevřít &složku…", self)
        act_open_folder.setShortcut(QKeySequence("Ctrl+Shift+O"))
        act_open_folder.triggered.connect(self._open_folder_dialog)
        soubor.addAction(act_open_folder)

        act_open_file = QAction("Otevřít &soubor…", self)
        act_open_file.setShortcut(QKeySequence.StandardKey.Open)
        act_open_file.triggered.connect(self._open_file_dialog)
        soubor.addAction(act_open_file)

        soubor.addSeparator()

        act_quit = QAction("&Konec", self)
        act_quit.setShortcut(QKeySequence.StandardKey.Quit)
        act_quit.triggered.connect(self.close)
        soubor.addAction(act_quit)

        zobraz = menubar.addMenu("&Zobrazení")

        act_fit = QAction("Přizpůsobit oknu  (F)", self)
        act_fit.triggered.connect(self._image_view.fit_to_window)
        zobraz.addAction(act_fit)

        act_1_1 = QAction("Originální velikost  (0)", self)
        act_1_1.triggered.connect(self._image_view.reset_zoom)
        zobraz.addAction(act_1_1)

        zobraz.addSeparator()
        act_panel = QAction("Panel náhledů", self)
        act_panel.setCheckable(True)
        act_panel.setChecked(True)
        act_panel.triggered.connect(self._thumbnail_dock.setVisible)
        zobraz.addAction(act_panel)

    def _setup_toolbar(self) -> None:
        tb = QToolBar("Navigace", self)
        tb.setMovable(False)
        self.addToolBar(tb)

        act_prev = QAction("◀  Předchozí", self)
        act_prev.setShortcut(QKeySequence(Qt.Key.Key_Left))
        act_prev.triggered.connect(self._prev_image)
        tb.addAction(act_prev)

        act_next = QAction("Další  ▶", self)
        act_next.setShortcut(QKeySequence(Qt.Key.Key_Right))
        act_next.triggered.connect(self._next_image)
        tb.addAction(act_next)

        tb.addSeparator()

        self._act_slideshow = QAction("▶  Slideshow", self)
        self._act_slideshow.setShortcut(QKeySequence("Space"))
        self._act_slideshow.triggered.connect(self._toggle_slideshow)
        tb.addAction(self._act_slideshow)

        tb.addWidget(QLabel("  Interval (s): "))
        self._interval_spin = QSpinBox()
        self._interval_spin.setRange(1, 60)
        self._interval_spin.setValue(self._slideshow.interval_ms // 1000)
        self._interval_spin.setSuffix(" s")
        self._interval_spin.valueChanged.connect(
            lambda v: self._slideshow.set_interval(v * 1000)
        )
        tb.addWidget(self._interval_spin)

    def _setup_statusbar(self) -> None:
        self._status_label = QLabel()
        self.statusBar().addWidget(self._status_label)

    # ------------------------------------------------------------------
    # Akce – otevření
    # ------------------------------------------------------------------

    def _open_folder_dialog(self) -> None:
        folder = QFileDialog.getExistingDirectory(self, "Otevřít složku")
        if folder:
            self._load_folder(Path(folder))

    def _open_file_dialog(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Otevřít soubor",
            "",
            "Obrázky (*.jpg *.jpeg *.png *.gif *.bmp *.webp *.tiff *.tif)",
        )
        if path:
            p = Path(path)
            self._load_folder(p.parent)
            # Přeskočí na vybraný soubor
            try:
                idx = self._image_paths.index(p)
                self._show_image(idx)
            except ValueError:
                pass

    def _load_folder(self, folder: Path) -> None:
        paths = load_folder(folder)
        if not paths:
            self._status_label.setText("Ve složce nebyly nalezeny žádné obrázky.")
            return
        self._image_paths = paths
        self._thumbnail_panel.load_images(paths)
        self._show_image(0)

    # ------------------------------------------------------------------
    # Navigace
    # ------------------------------------------------------------------

    def _show_image(self, index: int) -> None:
        if not self._image_paths or index < 0 or index >= len(self._image_paths):
            return
        self._current_index = index
        path = self._image_paths[index]
        self._image_view.load_image(path)
        self._thumbnail_panel.set_current_index(index)
        self._update_status(path)

    def _prev_image(self) -> None:
        if self._image_paths:
            idx = (self._current_index - 1) % len(self._image_paths)
            self._show_image(idx)

    def _next_image(self) -> None:
        if self._image_paths:
            idx = (self._current_index + 1) % len(self._image_paths)
            self._show_image(idx)

    # ------------------------------------------------------------------
    # Slideshow
    # ------------------------------------------------------------------

    def _toggle_slideshow(self) -> None:
        self._slideshow.toggle()
        if self._slideshow.is_running:
            self._act_slideshow.setText("⏸  Zastavit")
        else:
            self._act_slideshow.setText("▶  Slideshow")

    # ------------------------------------------------------------------
    # Status bar
    # ------------------------------------------------------------------

    def _update_status(self, path: Path) -> None:
        try:
            info: ImageInfo = load_image_info(path)
            self._status_label.setText(
                f"{path.name}   |   {info.dimensions_str}   |   "
                f"{info.format}   |   {info.file_size_kb:.1f} kB   |   "
                f"{self._current_index + 1} / {len(self._image_paths)}"
            )
        except Exception:
            self._status_label.setText(path.name)

    # ------------------------------------------------------------------
    # Klávesové zkratky
    # ------------------------------------------------------------------

    def keyPressEvent(self, event) -> None:
        key = event.key()
        if key == Qt.Key.Key_Left:
            self._prev_image()
        elif key == Qt.Key.Key_Right:
            self._next_image()
        elif key == Qt.Key.Key_Space:
            self._toggle_slideshow()
        else:
            super().keyPressEvent(event)

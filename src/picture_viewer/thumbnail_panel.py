"""Postranní panel s miniaturními náhledy obrázků ve složce."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt, Signal, QSize, QThreadPool
from PySide6.QtGui import QPixmap, QIcon
from PySide6.QtWidgets import QListWidget, QListWidgetItem

from .thumbnail_worker import ThumbnailWorker


THUMBNAIL_SIZE = 96  # px


class ThumbnailPanel(QListWidget):
    """Zobrazuje seznam náhledů; po kliknutí vyšle signál image_selected."""

    image_selected = Signal(int)  # index vybraného obrázku

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setIconSize(QSize(THUMBNAIL_SIZE, THUMBNAIL_SIZE))
        self.setFixedWidth(THUMBNAIL_SIZE + 24)
        self.setSpacing(4)
        self.setViewMode(QListWidget.ViewMode.IconMode)
        self.setResizeMode(QListWidget.ResizeMode.Adjust)
        self.setMovement(QListWidget.Movement.Static)
        self.setStyleSheet(
            "QListWidget { background-color: #2b2b2b; border: none; }"
            "QListWidget::item:selected { background-color: #0d6efd; }"
        )
        self.itemClicked.connect(self._on_item_clicked)
        self._current_worker: ThumbnailWorker | None = None

    # ------------------------------------------------------------------
    # Veřejné API
    # ------------------------------------------------------------------

    def load_images(self, paths: list[Path]) -> None:
        """Vytvoří prázdné položky a spustí asynchronní načítání náhledů.

        Náhledy se načítají v pozadí pomocí ThumbnailWorker v QThreadPool.
        """
        self.clear()

        # Vytvořit prázdné položky pro všechny cesty
        for path in paths:
            item = QListWidgetItem()
            item.setToolTip(path.name)
            item.setData(Qt.UserRole, path)
            self.addItem(item)

        # Spustit asynchronní načítání náhledů
        self._start_thumbnail_loader(paths)

    def set_current_index(self, index: int) -> None:
        """Zvýrazní obrázek na daném indexu."""
        if 0 <= index < self.count():
            self.setCurrentRow(index)
            self.scrollToItem(self.item(index))

    # ------------------------------------------------------------------
    # Asynchronní načítání náhledů
    # ------------------------------------------------------------------

    def _start_thumbnail_loader(self, paths: list[Path]) -> None:
        """Spustí ThumbnailWorker v QThreadPool pro asynchronní načítání."""
        self._current_worker = ThumbnailWorker(paths)
        self._current_worker.signals.thumbnail_ready.connect(self._on_thumbnail_ready)
        self._current_worker.signals.worker_finished.connect(self._on_thumbnails_complete)

        threadpool = QThreadPool.globalInstance()
        threadpool.start(self._current_worker)

    def _on_thumbnail_ready(self, index: int, icon: QIcon) -> None:
        """Slot: náhled je připraven, aktualizuj položku."""
        if 0 <= index < self.count():
            item = self.item(index)
            item.setIcon(icon)

    def _on_thumbnails_complete(self) -> None:
        """Slot: všechny náhledy jsou načteny."""
        self._current_worker = None

    # ------------------------------------------------------------------
    # Interní
    # ------------------------------------------------------------------

    def _on_item_clicked(self, item: QListWidgetItem) -> None:
        self.image_selected.emit(self.row(item))

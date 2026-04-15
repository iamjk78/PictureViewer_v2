"""Postranní panel s miniaturními náhledy obrázků ve složce."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt, Signal, QSize, QThreadPool
from PySide6.QtGui import QImage, QPixmap, QIcon
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
        self._generation: int = 0

    # ------------------------------------------------------------------
    # Veřejné API
    # ------------------------------------------------------------------

    def load_images(self, paths: list[Path]) -> None:
        """Vytvoří prázdné položky a spustí asynchronní načítání náhledů.

        Před spuštěním nového workeru zruší předchozí, aby stale signály
        od starého workeru neznečistily nový panel.
        """
        # Zrušit předchozí worker (stale signály by jinak mapovaly
        # náhledy z jiné složky na položky aktuálního panelu)
        if self._current_worker is not None:
            self._current_worker.cancel()
            self._current_worker = None

        # Každé volání load_images dostane novou generaci;
        # signály s jinou generací jsou v _on_thumbnail_ready ignorovány
        self._generation += 1

        self.clear()
        for path in paths:
            item = QListWidgetItem()
            item.setToolTip(path.name)
            item.setData(Qt.UserRole, path)
            self.addItem(item)

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
        worker = ThumbnailWorker(paths, self._generation)
        worker.signals.thumbnail_ready.connect(self._on_thumbnail_ready)
        worker.signals.worker_finished.connect(self._on_thumbnails_complete)
        self._current_worker = worker
        QThreadPool.globalInstance().start(worker)

    def _on_thumbnail_ready(self, generation: int, index: int, image: QImage) -> None:
        """Slot: náhled je připraven – konvertuj QImage → QIcon v hlavním vlákně.

        QPixmap NESMÍ být vytvářen ve vedlejším vlákně; tato konverze
        probíhá zde, tj. vždy v hlavním vlákně.
        Signály s nesprávnou generací jsou ignorovány (stale worker).
        """
        if generation != self._generation:
            return
        if 0 <= index < self.count():
            item = self.item(index)
            if not image.isNull():
                item.setIcon(QIcon(QPixmap.fromImage(image)))

    def _on_thumbnails_complete(self, generation: int) -> None:
        """Slot: worker dokončil načítání."""
        if generation == self._generation:
            self._current_worker = None

    # ------------------------------------------------------------------
    # Interní
    # ------------------------------------------------------------------

    def _on_item_clicked(self, item: QListWidgetItem) -> None:
        self.image_selected.emit(self.row(item))

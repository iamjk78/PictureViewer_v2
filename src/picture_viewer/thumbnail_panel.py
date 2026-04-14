"""Postranní panel s miniaturními náhledy obrázků ve složce."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt, Signal, QSize
from PySide6.QtGui import QPixmap, QIcon
from PySide6.QtWidgets import QListWidget, QListWidgetItem


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

    # ------------------------------------------------------------------
    # Veřejné API
    # ------------------------------------------------------------------

    def load_images(self, paths: list[Path]) -> None:
        """Naplní panel náhledy ze seznamu cest."""
        self.clear()
        for path in paths:
            item = QListWidgetItem()
            item.setToolTip(path.name)
            pixmap = QPixmap(str(path))
            if not pixmap.isNull():
                icon = QIcon(
                    pixmap.scaled(
                        THUMBNAIL_SIZE,
                        THUMBNAIL_SIZE,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.SmoothTransformation,
                    )
                )
            else:
                icon = QIcon()
            item.setIcon(icon)
            self.addItem(item)

    def set_current_index(self, index: int) -> None:
        """Zvýrazní obrázek na daném indexu."""
        if 0 <= index < self.count():
            self.setCurrentRow(index)
            self.scrollToItem(self.item(index))

    # ------------------------------------------------------------------
    # Interní
    # ------------------------------------------------------------------

    def _on_item_clicked(self, item: QListWidgetItem) -> None:
        self.image_selected.emit(self.row(item))

"""Widget pro zobrazení obrázku se zoomem a posouváním."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt, QRectF
from PySide6.QtGui import QPixmap, QWheelEvent, QKeyEvent, QTransform
from PySide6.QtWidgets import QGraphicsScene, QGraphicsView


class ImageView(QGraphicsView):
    """Zobrazuje obrázek s podporou zoomu (kolečko myši) a posunu (tažení)."""

    ZOOM_STEP = 1.15
    ZOOM_MIN = 0.05
    ZOOM_MAX = 20.0

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self._scene = QGraphicsScene(self)
        self.setScene(self._scene)
        self._pixmap_item = self._scene.addPixmap(QPixmap())
        self._zoom_level: float = 1.0

        # Povolí drag pro posun
        self.setDragMode(QGraphicsView.DragMode.ScrollHandDrag)
        self.setTransformationAnchor(QGraphicsView.ViewportAnchor.AnchorUnderMouse)
        self.setResizeAnchor(QGraphicsView.ViewportAnchor.AnchorViewCenter)
        self.setRenderHint(self.renderHints())
        self.setBackgroundRole(self.backgroundRole())
        self.setStyleSheet("background-color: #1e1e1e;")

    # ------------------------------------------------------------------
    # Veřejné API
    # ------------------------------------------------------------------

    def load_image(self, path: Path) -> None:
        """Načte obrázek ze souboru a přizpůsobí ho oknu."""
        pixmap = QPixmap(str(path))
        if pixmap.isNull():
            return
        self._pixmap_item.setPixmap(pixmap)
        self._scene.setSceneRect(QRectF(pixmap.rect()))
        self._zoom_level = 1.0
        self.setTransform(QTransform())
        self.fit_to_window()

    def fit_to_window(self) -> None:
        """Přizpůsobí obrázek tak, aby byl celý viditelný v okně."""
        if self._pixmap_item.pixmap().isNull():
            return
        self.fitInView(self._pixmap_item, Qt.AspectRatioMode.KeepAspectRatio)
        # Zjistí aktuální měřítko po fitInView
        self._zoom_level = self.transform().m11()

    def zoom_in(self) -> None:
        self._apply_zoom(self.ZOOM_STEP)

    def zoom_out(self) -> None:
        self._apply_zoom(1.0 / self.ZOOM_STEP)

    def reset_zoom(self) -> None:
        """Zobrazí obrázek v originální velikosti (1:1)."""
        self.setTransform(QTransform())
        self._zoom_level = 1.0

    # ------------------------------------------------------------------
    # Události
    # ------------------------------------------------------------------

    def wheelEvent(self, event: QWheelEvent) -> None:
        delta = event.angleDelta().y()
        if delta > 0:
            self._apply_zoom(self.ZOOM_STEP)
        elif delta < 0:
            self._apply_zoom(1.0 / self.ZOOM_STEP)

    def keyPressEvent(self, event: QKeyEvent) -> None:
        key = event.key()
        if key in (Qt.Key.Key_Plus, Qt.Key.Key_Equal):
            self.zoom_in()
        elif key == Qt.Key.Key_Minus:
            self.zoom_out()
        elif key == Qt.Key.Key_0:
            self.reset_zoom()
        elif key == Qt.Key.Key_F:
            self.fit_to_window()
        else:
            super().keyPressEvent(event)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        # Po změně velikosti okna automaticky přizpůsob, pokud je obrázek menší
        if not self._pixmap_item.pixmap().isNull():
            self.fit_to_window()

    # ------------------------------------------------------------------
    # Interní pomocné metody
    # ------------------------------------------------------------------

    def _apply_zoom(self, factor: float) -> None:
        new_zoom = self._zoom_level * factor
        if new_zoom < self.ZOOM_MIN or new_zoom > self.ZOOM_MAX:
            return
        self.scale(factor, factor)
        self._zoom_level = new_zoom

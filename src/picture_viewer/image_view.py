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
    PAN_STEP = 80  # pixelů při posunu šipkou

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self._scene = QGraphicsScene(self)
        self.setScene(self._scene)
        self._pixmap_item = self._scene.addPixmap(QPixmap())
        self._zoom_level: float = 1.0
        # Pokud uživatel manuálně změnil zoom, auto-fit při resize se přeskočí,
        # aby se zoom neresetoval při zobrazení scrollbarů.
        self._manually_zoomed: bool = False

        # Povolí drag pro posun
        self.setDragMode(QGraphicsView.DragMode.ScrollHandDrag)
        self.setTransformationAnchor(QGraphicsView.ViewportAnchor.AnchorUnderMouse)
        self.setResizeAnchor(QGraphicsView.ViewportAnchor.AnchorViewCenter)
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
        self._manually_zoomed = False
        self.setTransform(QTransform())
        self.fit_to_window()

    def fit_to_window(self) -> None:
        """Přizpůsobí obrázek tak, aby byl celý viditelný v okně."""
        if self._pixmap_item.pixmap().isNull():
            return
        self.fitInView(self._pixmap_item, Qt.AspectRatioMode.KeepAspectRatio)
        self._zoom_level = self.transform().m11()
        self._manually_zoomed = False

    def zoom_in(self) -> None:
        self._apply_zoom(self.ZOOM_STEP)

    def zoom_out(self) -> None:
        self._apply_zoom(1.0 / self.ZOOM_STEP)

    def reset_zoom(self) -> None:
        """Zobrazí obrázek v originální velikosti (1:1)."""
        self.setTransform(QTransform())
        self._zoom_level = 1.0
        self._manually_zoomed = True

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
        elif key in (Qt.Key.Key_Left, Qt.Key.Key_Right, Qt.Key.Key_Up, Qt.Key.Key_Down):
            if self._is_scrollable():
                # Obrázek je větší než viewport – šipky posouvají obrázek
                self._pan_by_key(key)
            else:
                # Obrázek se vejde – probublá do MainWindow pro přepínání snímků
                event.ignore()
        else:
            # F, Esc, Space atd. probublají do MainWindow
            event.ignore()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        # Auto-fit při resize jen pokud uživatel manuálně nezměnil zoom.
        # Jinak by zobrazení scrollbarů (při velkém zoomu) spustilo reset zoomu.
        if not self._pixmap_item.pixmap().isNull() and not self._manually_zoomed:
            self.fit_to_window()

    # ------------------------------------------------------------------
    # Interní pomocné metody
    # ------------------------------------------------------------------

    def _is_scrollable(self) -> bool:
        """Vrací True pokud je obrázek větší než viewport (lze posouvat)."""
        h = self.horizontalScrollBar()
        v = self.verticalScrollBar()
        return (h.maximum() > h.minimum()) or (v.maximum() > v.minimum())

    def _pan_by_key(self, key: Qt.Key) -> None:
        """Posune obrázek o PAN_STEP pixelů ve směru šipky."""
        h = self.horizontalScrollBar()
        v = self.verticalScrollBar()
        if key == Qt.Key.Key_Left:
            h.setValue(h.value() - self.PAN_STEP)
        elif key == Qt.Key.Key_Right:
            h.setValue(h.value() + self.PAN_STEP)
        elif key == Qt.Key.Key_Up:
            v.setValue(v.value() - self.PAN_STEP)
        elif key == Qt.Key.Key_Down:
            v.setValue(v.value() + self.PAN_STEP)

    def _apply_zoom(self, factor: float) -> None:
        new_zoom = self._zoom_level * factor
        if new_zoom < self.ZOOM_MIN:
            return
        self.scale(factor, factor)
        self._zoom_level = new_zoom
        self._manually_zoomed = True

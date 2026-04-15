"""Asynchronní načítání náhledů obrázků pomocí QThreadPool."""

from __future__ import annotations

from pathlib import Path
from time import sleep

from PySide6.QtCore import QObject, QRunnable, Signal, Qt
from PySide6.QtGui import QPixmap, QIcon


class WorkerSignals(QObject):
    """Signály emitované ThumbnailWorker."""

    thumbnail_ready = Signal(int, QIcon)  # (index, icon)
    batch_complete = Signal()
    worker_finished = Signal()
    worker_error = Signal(str)


class ThumbnailWorker(QRunnable):
    """Načítá náhledy obrázků v pozadí (v QThreadPool).

    Pracuje v samostatném vlákně a emituje signály zpět do GUI
    (thread-safe, automatické marshaling do hlavního vlákna).
    """

    BATCH_SIZE = 5
    THUMBNAIL_SIZE = 96

    def __init__(self, paths: list[Path]) -> None:
        super().__init__()
        self.paths = paths
        self.signals = WorkerSignals()

    def run(self) -> None:
        """Iteruje přes obrázky v dávkách a emituje náhledy."""
        try:
            for batch_start in range(0, len(self.paths), self.BATCH_SIZE):
                batch_end = min(batch_start + self.BATCH_SIZE, len(self.paths))

                for index in range(batch_start, batch_end):
                    path = self.paths[index]
                    icon = load_thumbnail(path, self.THUMBNAIL_SIZE)
                    self.signals.thumbnail_ready.emit(index, icon)

                # Nechat OS naplánovat ostatní vlákna
                sleep(0)
                self.signals.batch_complete.emit()

        except Exception as e:
            self.signals.worker_error.emit(f"Chyba při načítání náhledů: {str(e)}")

        finally:
            self.signals.worker_finished.emit()


def load_thumbnail(path: Path, size: int) -> QIcon:
    """Načte náhled (ikonu) z cesty k obrázku.

    Pure function bez GUI efektů. Vrací empty QIcon() pokud selhání.
    """
    try:
        pixmap = QPixmap(str(path))
        if pixmap.isNull():
            return QIcon()

        scaled = pixmap.scaled(
            size,
            size,
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.SmoothTransformation,
        )
        return QIcon(scaled)
    except Exception:
        return QIcon()

"""Asynchronní skenování složky a načítání náhledů obrázků."""

from __future__ import annotations

from pathlib import Path
from time import sleep

from PySide6.QtCore import QObject, QRunnable, Signal, Qt
from PySide6.QtGui import QImage

from .image_loader import load_folder


# ──────────────────────────────────────────────────────────────────
# Skenování složky v pozadí
# ──────────────────────────────────────────────────────────────────

class FolderScanSignals(QObject):
    scan_complete = Signal(list)  # list[Path] – seřazený seznam
    scan_error = Signal(str)


class FolderScanWorker(QRunnable):
    """Skenuje složku v pozadí; hlavní vlákno není blokováno.

    Znovu používá load_folder() z image_loader, takže logika filtrování
    a řazení zůstává na jednom místě.
    """

    def __init__(self, folder: Path) -> None:
        super().__init__()
        self.folder = folder
        self.signals = FolderScanSignals()

    def run(self) -> None:
        try:
            self.signals.scan_complete.emit(load_folder(self.folder))
        except Exception as e:
            self.signals.scan_error.emit(str(e))


# ──────────────────────────────────────────────────────────────────
# Načítání náhledů v pozadí
# ──────────────────────────────────────────────────────────────────

class ThumbnailSignals(QObject):
    """Signály emitované ThumbnailWorker."""

    # generation zajišťuje, že stale signály od starého workeru
    # neznečistí nový panel (ThumbnailPanel porovnává generaci)
    thumbnail_ready = Signal(int, int, QImage)  # (generation, index, image)
    worker_finished = Signal(int)               # generation
    worker_error = Signal(str)


class ThumbnailWorker(QRunnable):
    """Načítá náhledy v pozadí.

    DŮLEŽITÉ: emituje QImage (thread-safe), nikoliv QPixmap ani QIcon.
    QPixmap NESMÍ být vytvářen mimo hlavní vlákno – způsobuje korupci
    dat a vizuální artefakty (splývající náhledy).
    Konverze QImage → QPixmap → QIcon probíhá v hlavním vlákně
    v metodě ThumbnailPanel._on_thumbnail_ready().
    """

    BATCH_SIZE = 5
    THUMBNAIL_SIZE = 96

    def __init__(self, paths: list[Path], generation: int) -> None:
        super().__init__()
        self.paths = paths
        self.generation = generation
        self.signals = ThumbnailSignals()
        self._cancelled: bool = False

    def cancel(self) -> None:
        """Přeruší zpracování mezi položkami (ne okamžitě)."""
        self._cancelled = True

    def run(self) -> None:
        try:
            for batch_start in range(0, len(self.paths), self.BATCH_SIZE):
                if self._cancelled:
                    return
                batch_end = min(batch_start + self.BATCH_SIZE, len(self.paths))
                for index in range(batch_start, batch_end):
                    if self._cancelled:
                        return
                    image = load_thumbnail(self.paths[index], self.THUMBNAIL_SIZE)
                    self.signals.thumbnail_ready.emit(self.generation, index, image)
                # Nechat event loop (a ostatní vlákna) dostat ke slovu
                sleep(0)
        except Exception as e:
            self.signals.worker_error.emit(str(e))
        finally:
            if not self._cancelled:
                self.signals.worker_finished.emit(self.generation)


def load_thumbnail(path: Path, size: int) -> QImage:
    """Načte a škáluje náhled jako QImage (thread-safe).

    QImage lze bezpečně vytvářet ve vedlejším vlákně na rozdíl od QPixmap.
    Konverzi na QPixmap/QIcon musí vždy provést hlavní vlákno.
    Vrací prázdný QImage při selhání (caller to detekuje přes isNull()).
    """
    try:
        image = QImage(str(path))
        if image.isNull():
            return QImage()
        return image.scaled(
            size,
            size,
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.SmoothTransformation,
        )
    except Exception:
        return QImage()

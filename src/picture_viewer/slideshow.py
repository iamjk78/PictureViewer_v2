"""Řízení automatického přepínání obrázků (slideshow)."""

from __future__ import annotations

from PySide6.QtCore import QObject, QTimer, Signal


class SlideshowController(QObject):
    """Ovládá slideshow pomocí QTimer.

    Vysílá signál next_image() v pravidelném intervalu,
    dokud je spuštěna. Logika výběru dalšího obrázku
    je v MainWindow – tento objekt se stará jen o časování.
    """

    next_image = Signal()

    DEFAULT_INTERVAL_MS = 3_000

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._timer = QTimer(self)
        self._timer.timeout.connect(self.next_image)
        self._interval_ms: int = self.DEFAULT_INTERVAL_MS

    # ------------------------------------------------------------------
    # Veřejné API
    # ------------------------------------------------------------------

    @property
    def is_running(self) -> bool:
        return self._timer.isActive()

    @property
    def interval_ms(self) -> int:
        return self._interval_ms

    def start(self) -> None:
        """Spustí slideshow."""
        self._timer.start(self._interval_ms)

    def stop(self) -> None:
        """Zastaví slideshow."""
        self._timer.stop()

    def toggle(self) -> None:
        """Přepne mezi spuštěním a zastavením."""
        if self.is_running:
            self.stop()
        else:
            self.start()

    def set_interval(self, ms: int) -> None:
        """Nastaví interval přepínání v milisekundách (min. 500 ms)."""
        if ms < 500:
            ms = 500
        self._interval_ms = ms
        if self.is_running:
            # Restartuje timer s novým intervalem
            self._timer.start(self._interval_ms)

"""Inicializace QApplication a spuštění hlavního okna."""

from __future__ import annotations

import sys

from PySide6.QtWidgets import QApplication

from .main_window import MainWindow


def run() -> int:
    """Vytvoří aplikaci, zobrazí hlavní okno a spustí event loop."""
    app = QApplication.instance() or QApplication(sys.argv)
    app.setApplicationName("PictureViewer")
    app.setOrganizationName("JiriKrejci")

    window = MainWindow()
    window.show()

    return app.exec()

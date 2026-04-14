"""Testy pro slideshow.py."""

import pytest
from PySide6.QtCore import QCoreApplication
from picture_viewer.slideshow import SlideshowController


@pytest.fixture(scope="module")
def qt_app():
    """QCoreApplication potřebná pro QTimer."""
    app = QCoreApplication.instance() or QCoreApplication([])
    yield app


class TestSlideshowController:
    def test_vychozi_stav(self, qt_app) -> None:
        ctrl = SlideshowController()
        assert not ctrl.is_running
        assert ctrl.interval_ms == SlideshowController.DEFAULT_INTERVAL_MS

    def test_start_stop(self, qt_app) -> None:
        ctrl = SlideshowController()
        ctrl.start()
        assert ctrl.is_running
        ctrl.stop()
        assert not ctrl.is_running

    def test_toggle(self, qt_app) -> None:
        ctrl = SlideshowController()
        ctrl.toggle()
        assert ctrl.is_running
        ctrl.toggle()
        assert not ctrl.is_running

    def test_set_interval(self, qt_app) -> None:
        ctrl = SlideshowController()
        ctrl.set_interval(5000)
        assert ctrl.interval_ms == 5000

    def test_set_interval_minimum(self, qt_app) -> None:
        ctrl = SlideshowController()
        ctrl.set_interval(100)  # pod minimem
        assert ctrl.interval_ms == 500

    def test_set_interval_restartuje_timer(self, qt_app) -> None:
        ctrl = SlideshowController()
        ctrl.start()
        ctrl.set_interval(2000)
        assert ctrl.is_running
        assert ctrl.interval_ms == 2000
        ctrl.stop()

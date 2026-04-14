"""Testy pro image_loader.py."""

import io
from pathlib import Path

import pytest
from PIL import Image

from picture_viewer.image_loader import (
    SUPPORTED_EXTENSIONS,
    ImageInfo,
    load_folder,
    load_image_info,
)


def create_test_image(path: Path, width: int = 100, height: int = 80, fmt: str = "PNG") -> None:
    """Pomocná funkce – vytvoří testovací obrázek na disku."""
    img = Image.new("RGB", (width, height), color=(255, 0, 0))
    img.save(path, format=fmt)


class TestLoadFolder:
    def test_prazdna_slozka(self, tmp_path: Path) -> None:
        assert load_folder(tmp_path) == []

    def test_vraci_pouze_obrazky(self, tmp_path: Path) -> None:
        create_test_image(tmp_path / "a.png")
        create_test_image(tmp_path / "b.jpg")
        (tmp_path / "dokument.txt").write_text("text")
        result = load_folder(tmp_path)
        assert len(result) == 2
        assert all(p.suffix.lower() in SUPPORTED_EXTENSIONS for p in result)

    def test_razeni_podle_nazvu(self, tmp_path: Path) -> None:
        create_test_image(tmp_path / "c.png")
        create_test_image(tmp_path / "a.png")
        create_test_image(tmp_path / "b.png")
        result = load_folder(tmp_path)
        assert [p.name for p in result] == ["a.png", "b.png", "c.png"]

    def test_neexistujici_slozka(self) -> None:
        with pytest.raises(NotADirectoryError):
            load_folder(Path("/neexistuje/slozka"))

    def test_ignoruje_podslozky(self, tmp_path: Path) -> None:
        podslozka = tmp_path / "sub"
        podslozka.mkdir()
        create_test_image(podslozka / "x.png")
        create_test_image(tmp_path / "y.png")
        result = load_folder(tmp_path)
        assert len(result) == 1
        assert result[0].name == "y.png"


class TestLoadImageInfo:
    def test_nacte_metadata_png(self, tmp_path: Path) -> None:
        p = tmp_path / "test.png"
        create_test_image(p, width=200, height=150)
        info = load_image_info(p)
        assert info.width == 200
        assert info.height == 150
        assert info.format == "PNG"
        assert info.file_size > 0
        assert info.path == p

    def test_dimensions_str(self, tmp_path: Path) -> None:
        p = tmp_path / "test.png"
        create_test_image(p, width=640, height=480)
        info = load_image_info(p)
        assert info.dimensions_str == "640 × 480 px"

    def test_file_size_kb(self, tmp_path: Path) -> None:
        p = tmp_path / "test.png"
        create_test_image(p)
        info = load_image_info(p)
        assert info.file_size_kb == info.file_size / 1024

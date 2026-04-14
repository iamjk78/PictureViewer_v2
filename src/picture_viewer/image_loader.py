"""Načítání obrázků a čtení jejich metadat."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

# Podporované přípony obrázků
SUPPORTED_EXTENSIONS: frozenset[str] = frozenset(
    {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".tiff", ".tif"}
)


@dataclass(frozen=True)
class ImageInfo:
    """Metadata jednoho obrázku."""

    path: Path
    width: int
    height: int
    file_size: int  # bajty
    format: str    # např. "JPEG", "PNG"

    @property
    def file_size_kb(self) -> float:
        return self.file_size / 1024

    @property
    def dimensions_str(self) -> str:
        return f"{self.width} × {self.height} px"


def load_image_info(path: Path) -> ImageInfo:
    """Načte metadata obrázku ze souboru."""
    file_size = path.stat().st_size
    with Image.open(path) as img:
        width, height = img.size
        fmt = img.format or path.suffix.lstrip(".").upper()
    return ImageInfo(
        path=path,
        width=width,
        height=height,
        file_size=file_size,
        format=fmt,
    )


def load_folder(folder: Path) -> list[Path]:
    """Vrátí seřazený seznam obrázků ve složce (bez podsložek)."""
    if not folder.is_dir():
        raise NotADirectoryError(f"Cesta není složka: {folder}")

    images = [
        entry
        for entry in folder.iterdir()
        if entry.is_file() and entry.suffix.lower() in SUPPORTED_EXTENSIONS
    ]
    images.sort(key=lambda p: p.name.lower())
    return images

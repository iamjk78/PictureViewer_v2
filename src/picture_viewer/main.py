"""Vstupní bod aplikace – spouští se jako `python -m picture_viewer`."""

import sys
from .app import run

def main() -> None:
    sys.exit(run())

if __name__ == "__main__":
    main()

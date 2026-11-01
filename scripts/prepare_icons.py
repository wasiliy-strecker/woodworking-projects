#!/usr/bin/env python3
"""Derive Linux icon sizes from the existing artwork without redesigning it."""

from pathlib import Path

from PIL import Image


def main():
    branding = Path(__file__).resolve().parents[1] / "assets/branding"
    with Image.open(branding / "icon-1024.png") as original:
        image = original.convert("RGBA")
        for size in (16, 32, 48, 64, 128, 512):
            image.resize((size, size), Image.Resampling.LANCZOS).save(
                branding / f"icon-{size}.png"
            )


if __name__ == "__main__":
    main()

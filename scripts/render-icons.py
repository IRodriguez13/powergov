#!/usr/bin/env python3
"""Render PNG app icons from SVG or PIL fallback."""
from __future__ import annotations

import io
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SVG = ROOT / "data/icons/hicolor/scalable/apps/powergov.svg"
SIZES = (16, 24, 32, 48, 64, 128, 256)


def render_external(size: int) -> bytes | None:
    try:
        import cairosvg

        return cairosvg.svg2png(url=str(SVG), output_width=size, output_height=size)
    except ImportError:
        pass

    for cmd, args in (
        ("rsvg-convert", ["-w", str(size), "-h", str(size), str(SVG)]),
        ("inkscape", ["-w", str(size), "-h", str(size), str(SVG)]),
    ):
        try:
            return subprocess.check_output([cmd, *args], stderr=subprocess.DEVNULL)
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    return None


def render_pil(size: int):
    from PIL import Image, ImageDraw

    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    pad = max(1, size // 16)
    r = size // 4

    draw.rounded_rectangle((pad, pad, size - pad, size - pad), radius=r, fill=(27, 94, 32, 255))

    cx = cy = size // 2
    ring = int(size * 0.34)
    draw.ellipse(
        (cx - ring, cy - ring, cx + ring, cy + ring),
        outline=(77, 182, 172, 220),
        width=max(1, size // 26),
    )

    bolt = [
        (cx + size * 0.06, cy - size * 0.28),
        (cx - size * 0.13, cy + size * 0.04),
        (cx + size * 0.02, cy + size * 0.04),
        (cx - size * 0.05, cy + size * 0.28),
        (cx + size * 0.19, cy - size * 0.06),
        (cx + size * 0.06, cy - size * 0.06),
    ]
    draw.polygon(bolt, fill=(165, 214, 167, 255))

    dot = max(2, size // 16)
    draw.ellipse((cx - dot, cy - dot, cx + dot, cy + dot), fill=(224, 242, 241, 230))
    return img


def main() -> int:
    from PIL import Image

    if not SVG.is_file():
        print(f"missing {SVG}", file=sys.stderr)
        return 1

    used_fallback = False
    for size in SIZES:
        dest = ROOT / f"data/icons/hicolor/{size}x{size}/apps/powergov.png"
        dest.parent.mkdir(parents=True, exist_ok=True)
        raw = render_external(size)
        if raw:
            img = Image.open(io.BytesIO(raw)).convert("RGBA")
        else:
            used_fallback = True
            img = render_pil(size)
        img.save(dest, format="PNG")
        print(f"wrote {dest}")

    if used_fallback:
        print("note: used PIL fallback (install librsvg2-bin for SVG-accurate PNGs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

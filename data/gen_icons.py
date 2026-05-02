#!/usr/bin/env python3
"""
Generate PWA icons for ZombieVerter Dial Display.
Run once from the data/ folder: python3 gen_icons.py
Requires Pillow: pip install Pillow
"""

from PIL import Image, ImageDraw, ImageFont
import math

def draw_icon(size):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 255))
    draw = ImageDraw.Draw(img)

    cx, cy = size // 2, size // 2
    pad = size * 0.08

    # Outer circle (cyan ring)
    ring_w = max(4, size // 20)
    draw.ellipse(
        [pad, pad, size - pad, size - pad],
        outline=(0, 188, 212, 255),
        width=ring_w
    )

    # Arc gauge (270 degrees, like the SOC arc on the dial)
    arc_pad = pad + ring_w + size * 0.05
    arc_w = max(3, size // 28)
    # Draw from 135° to 405° (270° sweep) — PIL angles are clockwise from 3 o'clock
    draw.arc(
        [arc_pad, arc_pad, size - arc_pad, size - arc_pad],
        start=135, end=45,
        fill=(0, 230, 118, 255),
        width=arc_w
    )

    # Lightning bolt (⚡) in the centre using text
    # Scale font size to icon
    font_size = int(size * 0.38)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", font_size)
    except Exception:
        font = ImageFont.load_default()

    text = "⚡"
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    tx = cx - tw // 2
    ty = cy - th // 2
    draw.text((tx, ty), text, font=font, fill=(0, 188, 212, 255))

    return img

for size in [192, 512]:
    icon = draw_icon(size)
    filename = f"icon-{size}.png"
    icon.save(filename)
    print(f"Saved {filename}")

print("Done. Copy icon-192.png and icon-512.png into your data/ folder.")

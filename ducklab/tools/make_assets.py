"""Generate DuckLab's icon, banner image and banner sound.

Usage: python3 tools/make_assets.py   (needs Pillow: pip install pillow)
Writes game/icon.png (48x48), game/banner.png (256x128), game/banner.wav.
"""
import math
import os
import struct
import wave

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.join(HERE, "..", "game")

# 16x16 pixel duck. . = clear, Y = body, O = beak, K = eye, W = water
DUCK = [
    "................",
    "......YYYY......",
    ".....YYYYYY.....",
    ".....YYKYYY.....",
    "...OOOYYYYY.....",
    "...OOOYYYYY.....",
    "......YYYY......",
    ".....YYYYYYY..Y.",
    "...YYYYYYYYYYYY.",
    "..YYYYYYYYYYYYY.",
    "..YYYYYYYYYYYY..",
    "...YYYYYYYYYY...",
    "....YYYYYYYY....",
    "WWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWW",
    "................",
]
COLORS = {"Y": (255, 214, 51), "O": (255, 140, 26), "K": (20, 20, 30), "W": (60, 150, 230)}


def draw_duck(img, x0, y0, scale):
    d = ImageDraw.Draw(img)
    for y, row in enumerate(DUCK):
        for x, c in enumerate(row):
            if c in COLORS:
                d.rectangle([x0 + x * scale, y0 + y * scale,
                             x0 + (x + 1) * scale - 1, y0 + (y + 1) * scale - 1], fill=COLORS[c])


def make_icon():
    img = Image.new("RGBA", (48, 48), (30, 34, 54, 255))
    draw_duck(img, 0, 0, 3)
    img.save(os.path.join(GAME, "icon.png"))


def make_banner():
    img = Image.new("RGBA", (256, 128), (30, 34, 54, 255))
    d = ImageDraw.Draw(img)
    # Hex-code backdrop, because this is a hacking lab.
    for row in range(8):
        d.text((4, 4 + row * 15), " ".join(f"{(row * 37 + i * 91) & 0xFF:02X}" for i in range(14)),
               fill=(50, 60, 90))
    draw_duck(img, 8, 16, 6)
    d.text((120, 40), "DuckLab", fill=(255, 214, 51))
    d.text((120, 60), "cheat practice lab", fill=(220, 220, 230))
    img.save(os.path.join(GAME, "banner.png"))


def make_quack():
    rate, seconds = 22050, 0.6
    frames = bytearray()
    for i in range(int(rate * seconds)):
        t = i / rate
        f = 520 - 260 * t  # falling pitch
        env = min(1.0, t * 30) * max(0.0, 1 - t / seconds)
        s = math.sin(2 * math.pi * f * t) + 0.5 * math.sin(4 * math.pi * f * t) + 0.25 * math.sin(6 * math.pi * f * t)
        frames += struct.pack("<h", int(max(-1, min(1, s / 1.75 * env)) * 20000))
    with wave.open(os.path.join(GAME, "banner.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))


if __name__ == "__main__":
    make_icon()
    make_banner()
    make_quack()
    print("Wrote icon.png, banner.png, banner.wav to", os.path.normpath(GAME))

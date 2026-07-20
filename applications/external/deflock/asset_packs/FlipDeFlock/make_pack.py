#!/usr/bin/env python3
"""
Generate the FlipDeFlock "glitch / scanner HUD" desktop animation as a Flipper
asset pack (128x64, 1-bit). Output:
  Anims/manifest.txt
  Anims/L1_FlipDeFlock_Scan_128x64/{meta.txt, frame_0..N.png}

Convention: white background (off), black = lit pixel. Run: python make_pack.py
"""
import math
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ANIM = "L1_FlipDeFlock_Scan_128x64"
OUTDIR = os.path.join(HERE, "Anims", ANIM)
os.makedirs(OUTDIR, exist_ok=True)

W, H = 128, 64
N = 16  # frames
FRAME_RATE = 8

try:
    f_title = ImageFont.truetype("C:/Windows/Fonts/consolab.ttf", 9)
    f_small = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 8)
except Exception:
    f_title = ImageFont.load_default()
    f_small = f_title

# Fixed "detected signal" blips revealed as the sweep passes them.
BLIPS = [(22, 26), (44, 40), (66, 22), (88, 44), (104, 30), (34, 48)]


def draw_frame(i):
    img = Image.new("1", (W, H), 1)  # white bg
    d = ImageDraw.Draw(img)
    t = i / N

    # --- HUD frame + corner ticks ---
    d.rectangle([0, 0, W - 1, H - 1], outline=0)
    for (cx, cy) in [(0, 0), (W - 1, 0), (0, H - 1), (W - 1, H - 1)]:
        sx = 1 if cx == 0 else -1
        sy = 1 if cy == 0 else -1
        d.line([(cx, cy), (cx + sx * 6, cy)], fill=0)
        d.line([(cx, cy), (cx, cy + sy * 6)], fill=0)

    # --- title bar ---
    d.text((4, 1), "FLIPDEFLOCK", font=f_title, fill=0)
    d.line([(2, 12), (W - 3, 12)], fill=0)

    # status text blinks SCAN / TRACK
    label = "TRACK" if (i // 4) % 2 else "SCAN"
    d.text((W - 4 - len(label) * 6, 1), label, font=f_small, fill=0)

    # --- radar/scan window ---
    top, bot, left, right = 15, 49, 4, W - 5
    # sweeping vertical line
    sweep = int(left + t * (right - left))
    d.line([(sweep, top), (sweep, bot)], fill=0)
    # faint trailing ticks behind the sweep
    for back in (4, 8, 12):
        sx = sweep - back
        if sx > left:
            for yy in range(top, bot, 4):
                d.point((sx, yy), fill=0)
    # mid grid line
    midy = (top + bot) // 2
    for xx in range(left, right, 6):
        d.point((xx, midy), fill=0)

    # blips: light up once the sweep has passed them this cycle
    for (bx, by) in BLIPS:
        if sweep >= bx:
            d.ellipse([bx - 2, by - 2, bx + 2, by + 2], outline=0)
            d.point((bx, by), fill=0)
            # a "locked" reticle on the nearest-just-passed blip
            if 0 <= sweep - bx <= 6:
                d.line([(bx - 4, by), (bx - 2, by)], fill=0)
                d.line([(bx + 2, by), (bx + 4, by)], fill=0)
                d.line([(bx, by - 4), (bx, by - 2)], fill=0)
                d.line([(bx, by + 2), (bx, by + 4)], fill=0)

    # --- bottom spectrum bars ---
    base = bot + 3
    for b in range(20):
        bx = 5 + b * 6
        if bx > W - 6:
            break
        hgt = int(2 + 5 * (0.5 + 0.5 * math.sin((i * 0.9) + b * 0.8)))
        d.rectangle([bx, base + (8 - hgt), bx + 3, base + 8], fill=0)

    # --- glitch: displace a horizontal band on a couple of frames ---
    if i % 7 in (3,):
        band = img.crop((0, 24, W, 31))
        img.paste(1, (0, 24, W, 31))  # clear band
        img.paste(band, (5, 24))  # shifted
        ImageDraw.Draw(img).line([(0, 27), (W, 27)], fill=0)

    return img


def hud_frame(d):
    d.rectangle([0, 0, W - 1, H - 1], outline=0)
    for (cx, cy) in [(0, 0), (W - 1, 0), (0, H - 1), (W - 1, H - 1)]:
        sx = 1 if cx == 0 else -1
        sy = 1 if cy == 0 else -1
        d.line([(cx, cy), (cx + sx * 6, cy)], fill=0)
        d.line([(cx, cy), (cx, cy + sy * 6)], fill=0)


def build_lockscreen():
    """Themed full-screen lock screen -> Icons/Interface/Lockscreen.png (128x64)."""
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    hud_frame(d)
    d.text((4, 1), "FLIPDEFLOCK", font=f_title, fill=0)
    d.line([(2, 12), (W - 3, 12)], fill=0)

    # Padlock (left-of-center)
    bx0, by0, bx1, by1 = 30, 32, 54, 52  # body
    d.rectangle([bx0, by0, bx1, by1], outline=0)
    d.rectangle([bx0 + 1, by0 + 1, bx1 - 1, by1 - 1], outline=0)  # double line = bold
    cx = (bx0 + bx1) // 2
    d.ellipse([cx - 2, by0 + 5, cx + 2, by0 + 9], outline=0)  # keyhole
    d.line([(cx, by0 + 8), (cx, by0 + 14)], fill=0)
    d.arc([bx0 + 3, by0 - 14, bx1 - 3, by0 + 6], 180, 360, fill=0)  # shackle
    d.line([(bx0 + 3, by0 - 4), (bx0 + 3, by0)], fill=0)
    d.line([(bx1 - 3, by0 - 4), (bx1 - 3, by0)], fill=0)

    d.text((64, 30), "LOCKED", font=f_title, fill=0)
    d.text((64, 41), "secure", font=f_small, fill=0)

    # scanner accents (right side blips + a dotted sweep column)
    for (bx, by) in [(110, 24), (118, 36), (104, 46)]:
        d.ellipse([bx - 2, by - 2, bx + 2, by + 2], outline=0)
        d.point((bx, by), fill=0)
    for yy in range(16, 56, 4):
        d.point((90, yy), fill=0)

    os.makedirs(os.path.join(HERE, "Icons", "Interface"), exist_ok=True)
    img.save(os.path.join(HERE, "Icons", "Interface", "Lockscreen.png"))
    print("wrote Icons/Interface/Lockscreen.png")


def main():
    build_lockscreen()
    order = []
    for i in range(N):
        draw_frame(i).save(os.path.join(OUTDIR, f"frame_{i}.png"))
        order.append(str(i))

    with open(os.path.join(OUTDIR, "meta.txt"), "w", newline="\n") as f:
        f.write(
            "Filetype: Flipper Animation\nVersion: 1\n\n"
            f"Width: {W}\nHeight: {H}\n"
            f"Passive frames: {N}\nActive frames: 0\n"
            f"Frames order: {' '.join(order)}\n"
            "Active cycles: 0\n"
            f"Frame rate: {FRAME_RATE}\nDuration: 0\nActive cooldown: 0\n\n"
            "Bubble slots: 0\n"
        )

    with open(os.path.join(HERE, "Anims", "manifest.txt"), "w", newline="\n") as f:
        f.write(
            "Filetype: Flipper Animation Manifest\nVersion: 1\n\n"
            f"Name: {ANIM}\n"
            "Min butthurt: 0\nMax butthurt: 18\nMin level: 1\nMax level: 30\nWeight: 8\n"
        )
    print(f"wrote {N} frames + meta + manifest to {OUTDIR}")


if __name__ == "__main__":
    main()

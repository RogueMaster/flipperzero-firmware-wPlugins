#!/usr/bin/env python3
"""
Render a Flipper Zero–style demo GIF for Tesla FSD Unlock.

Mimics the actual screens from `scenes/fsd_running.c` and
`scenes/main_menu.c`, at Flipper's native 128×64 monochrome resolution,
then upscales with an amber backlight tint and a device bezel so it reads
as "a Flipper running our app" in a README hero image.

No hardware and no emulator required — everything is drawn from the
coordinates and text we know the real app produces.

Usage:
    python3 tools/make_demo_gif.py
    → writes assets/demo.gif
"""

import os
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

# ── Flipper screen dimensions ─────────────────────────────────────────────────
FLIPPER_W, FLIPPER_H = 128, 64
SCALE = 6  # upscale factor
PAD_X, PAD_Y = 24, 24  # bezel padding around the screen
FRAME_MS_FAST = 300
FRAME_MS_SLOW = 700

# Flipper's iconic amber backlight
BG_AMBER = (0xE8, 0x8A, 0x00)
FG_DARK = (0x1A, 0x10, 0x00)
BEZEL = (0x2A, 0x2A, 0x30)
BEZEL_EDGE = (0x10, 0x10, 0x14)


# Match Flipper's FontPrimary (8px) and FontSecondary (~6px)
def load_fonts():
    candidates = [
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.ttf",
        "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
    ]
    for path in candidates:
        if os.path.exists(path):
            return (
                ImageFont.truetype(path, 9),  # FontPrimary-ish
                ImageFont.truetype(path, 7),  # FontSecondary-ish
            )
    # Fallback to default bitmap
    f = ImageFont.load_default()
    return f, f


FONT_PRIMARY, FONT_SECONDARY = load_fonts()


# ── primitives ────────────────────────────────────────────────────────────────


def new_screen():
    """Blank 128×64 amber screen."""
    return Image.new("RGB", (FLIPPER_W, FLIPPER_H), BG_AMBER)


def draw_text(img, x, y, text, font=FONT_SECONDARY, align="left"):
    d = ImageDraw.Draw(img)
    if align == "center":
        bbox = d.textbbox((0, 0), text, font=font)
        w = bbox[2] - bbox[0]
        x = (FLIPPER_W - w) // 2
    d.text((x, y - 1), text, font=font, fill=FG_DARK)


def draw_hline(img, y, x0=0, x1=FLIPPER_W):
    ImageDraw.Draw(img).line([(x0, y), (x1 - 1, y)], fill=FG_DARK, width=1)


def draw_box(img, x0, y0, x1, y1, filled=False):
    d = ImageDraw.Draw(img)
    if filled:
        d.rectangle([x0, y0, x1, y1], fill=FG_DARK)
    else:
        d.rectangle([x0, y0, x1, y1], outline=FG_DARK, width=1)


def invert_line(img, y, h=10):
    """Submenu-style inverted highlight bar."""
    d = ImageDraw.Draw(img)
    d.rectangle([0, y, FLIPPER_W - 1, y + h - 1], fill=FG_DARK)
    return y


def draw_inverted_text(img, x, y, text, font=FONT_SECONDARY):
    d = ImageDraw.Draw(img)
    d.text((x, y - 1), text, font=font, fill=BG_AMBER)


# ── scenes ────────────────────────────────────────────────────────────────────


def scene_main_menu(selected=0):
    """Main menu with 'Auto Detect & Start' selected."""
    img = new_screen()
    items = [
        "Auto Detect & Start",
        "Force HW3 Mode",
        "Force HW4 Mode",
        "Force Legacy Mode",
        "Settings",
        "About",
    ]
    # Title bar
    draw_text(img, 0, 2, "Tesla FSD", font=FONT_PRIMARY, align="center")
    draw_hline(img, 14)
    # Menu items
    y = 18
    for i, item in enumerate(items[:5]):
        if i == selected:
            invert_line(img, y - 2, h=10)
            draw_inverted_text(img, 4, y, item)
        else:
            draw_text(img, 4, y, item)
        y += 9
    return img


def scene_detecting(phase):
    """HW detecting screen with animated dots."""
    img = new_screen()
    draw_text(img, 0, 8, "Tesla FSD", font=FONT_PRIMARY, align="center")
    draw_hline(img, 22)
    dots = "." * (1 + (phase % 3))
    draw_text(img, 0, 30, f"Detecting HW{dots}", font=FONT_PRIMARY, align="center")
    draw_text(img, 0, 46, f"listening 0x398", align="center")
    return img


def scene_detected():
    img = new_screen()
    draw_text(img, 0, 8, "Tesla FSD", font=FONT_PRIMARY, align="center")
    draw_hline(img, 22)
    draw_text(img, 0, 28, "HW4 detected", font=FONT_PRIMARY, align="center")
    draw_text(img, 0, 44, "FSD v14 path", align="center")
    draw_text(img, 0, 54, "[OK] to start", align="center")
    return img


def scene_running(tx, rx, err, fsd_on, nag_on, bms, mode="LSN"):
    """The live running screen from fsd_running.c."""
    img = new_screen()
    # Line 1: Title (FontPrimary @ y=2, center)
    draw_text(img, 0, 2, "Tesla FSD Active", font=FONT_PRIMARY, align="center")
    # Line 2: HW + profile (y=16)
    draw_text(img, 2, 16, "HW: HW4    Profile: 4/4")
    # Line 3: FSD + Nag + mode (y=26)
    fsd_str = "ON  " if fsd_on else "WAIT"
    nag_str = "OFF" if nag_on else "--"
    draw_text(img, 2, 26, f"FSD:{fsd_str} Nag:{nag_str} [{mode}]")
    # Line 4: counters (y=36)
    draw_text(img, 2, 36, f"TX:{tx} RX:{rx} Err:{err}")
    # Line 5 (y=46): BMS data or feature flags
    if bms:
        draw_text(img, 2, 46, "SoC:78% 12kW 25-28C")
    # Line 6: [BACK] to stop (y=56, center)
    draw_text(img, 0, 56, "[BACK] to stop", align="center")
    return img


# ── frame sequence ────────────────────────────────────────────────────────────


def build_frames():
    frames = []
    delays = []

    def add(img, ms):
        frames.append(img)
        delays.append(ms)

    # Main menu — briefly show each item highlighted to show it's selectable
    add(scene_main_menu(selected=0), FRAME_MS_SLOW)
    add(scene_main_menu(selected=0), FRAME_MS_SLOW)

    # Detecting — 3 dot-animation frames
    for phase in range(6):
        add(scene_detecting(phase), FRAME_MS_FAST)

    # Detected — pause
    add(scene_detected(), 1000)

    # Running: start in Listen-Only, no FSD yet
    add(
        scene_running(
            tx=0, rx=3, err=0, fsd_on=False, nag_on=False, bms=False, mode="LSN"
        ),
        FRAME_MS_FAST,
    )
    add(
        scene_running(
            tx=0, rx=12, err=0, fsd_on=False, nag_on=False, bms=False, mode="LSN"
        ),
        FRAME_MS_FAST,
    )
    add(
        scene_running(
            tx=0, rx=38, err=0, fsd_on=False, nag_on=False, bms=False, mode="LSN"
        ),
        FRAME_MS_FAST,
    )

    # User switches to Active (mode ACT) — still no FSD, RX climbing
    add(
        scene_running(
            tx=0, rx=74, err=0, fsd_on=False, nag_on=False, bms=False, mode="ACT"
        ),
        FRAME_MS_FAST,
    )
    add(
        scene_running(
            tx=0, rx=112, err=0, fsd_on=False, nag_on=False, bms=False, mode="ACT"
        ),
        FRAME_MS_FAST,
    )

    # User enables Traffic-Light toggle in the car → FSD activates
    add(
        scene_running(
            tx=3, rx=156, err=0, fsd_on=True, nag_on=True, bms=False, mode="ACT"
        ),
        FRAME_MS_FAST,
    )
    add(
        scene_running(
            tx=12, rx=203, err=0, fsd_on=True, nag_on=True, bms=False, mode="ACT"
        ),
        FRAME_MS_FAST,
    )

    # BMS frames arrive → line 4 switches to live BMS
    add(
        scene_running(
            tx=21, rx=268, err=0, fsd_on=True, nag_on=True, bms=True, mode="ACT"
        ),
        FRAME_MS_FAST,
    )
    add(
        scene_running(
            tx=34, rx=345, err=0, fsd_on=True, nag_on=True, bms=True, mode="ACT"
        ),
        FRAME_MS_FAST,
    )
    add(
        scene_running(
            tx=52, rx=441, err=0, fsd_on=True, nag_on=True, bms=True, mode="ACT"
        ),
        FRAME_MS_FAST,
    )
    add(
        scene_running(
            tx=78, rx=569, err=0, fsd_on=True, nag_on=True, bms=True, mode="ACT"
        ),
        FRAME_MS_SLOW,
    )

    # Hold on the final frame
    last = scene_running(
        tx=78, rx=569, err=0, fsd_on=True, nag_on=True, bms=True, mode="ACT"
    )
    add(last, 1500)

    return frames, delays


# ── finishing: upscale + bezel ────────────────────────────────────────────────


def frame_with_bezel(screen):
    """Upscale the 128×64 screen and wrap it in a simple device bezel."""
    up_w, up_h = FLIPPER_W * SCALE, FLIPPER_H * SCALE
    up = screen.resize((up_w, up_h), Image.NEAREST)
    out_w = up_w + PAD_X * 2
    out_h = up_h + PAD_Y * 2
    out = Image.new("RGB", (out_w, out_h), BEZEL)
    d = ImageDraw.Draw(out)
    # outer edge
    d.rectangle([0, 0, out_w - 1, out_h - 1], outline=BEZEL_EDGE, width=3)
    # inner screen border
    d.rectangle(
        [PAD_X - 3, PAD_Y - 3, PAD_X + up_w + 2, PAD_Y + up_h + 2],
        outline=BEZEL_EDGE,
        width=2,
    )
    out.paste(up, (PAD_X, PAD_Y))
    return out


def main():
    frames, delays = build_frames()
    framed = [frame_with_bezel(f) for f in frames]

    out_dir = Path(__file__).resolve().parent.parent / "assets"
    out_dir.mkdir(exist_ok=True)
    out_path = out_dir / "demo.gif"

    framed[0].save(
        out_path,
        save_all=True,
        append_images=framed[1:],
        duration=delays,
        loop=0,
        optimize=True,
        disposal=2,
    )
    size_kb = out_path.stat().st_size / 1024
    print(f"wrote {out_path} — {len(framed)} frames, {size_kb:.1f} KB")


if __name__ == "__main__":
    main()

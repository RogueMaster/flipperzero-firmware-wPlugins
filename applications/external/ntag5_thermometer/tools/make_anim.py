#!/usr/bin/env python3
"""Create celebration animations (.anim) for the Pet Thermometer app.

The app plays /ext/apps_data/ntag5_thermometer/pets/<ID>.anim when the tag with that ID
comes into the field. <ID> is the tag number as the app shows it, e.g. 276-000123456789
for FDX-B chips or E0040150A1B2C3D4 for NFC patches.

Examples:
    tools/make_anim.py cat.gif 276-000123456789.anim --name Mimi
    tools/make_anim.py frame1.png frame2.png frame3.png out.anim --frame-ms 150
    tools/make_anim.py --demo demo.anim --name Mimi

Images are scaled to fit 128x64 and converted to black and white: dark pixels become
black (set) pixels. Use --invert for images with a dark background, --dither for photos.

Requires Pillow (pip install pillow, or the python3-pil package).
"""

import argparse
import math
import struct
import sys

from PIL import Image, ImageDraw, ImageSequence

MAGIC = b"PETA"
VERSION = 1
WIDTH_MAX = 128
HEIGHT_MAX = 64
NAME_SIZE = 20
FRAME_MS_MIN = 20
FRAME_MS_MAX = 10000
FRAMES_MAX = 255


def load_frames(paths):
    """All frames of all input files as RGBA images with their duration in ms (or None)"""
    frames = []
    for path in paths:
        image = Image.open(path)
        for frame in ImageSequence.Iterator(image):
            duration = frame.info.get("duration")
            frames.append((frame.convert("RGBA"), duration))
    return frames


def fit_size(width, height):
    scale = min(WIDTH_MAX / width, HEIGHT_MAX / height)
    return max(1, round(width * scale)), max(1, round(height * scale))


def to_mono(image, size, invert, dither, threshold):
    # Transparent areas become white, like the Flipper background
    background = Image.new("RGBA", image.size, (255, 255, 255, 255))
    gray = Image.alpha_composite(background, image).convert("L")
    gray = gray.resize(size, Image.Resampling.LANCZOS)
    if invert:
        gray = gray.point(lambda value: 255 - value)
    if dither:
        return gray.convert("1", dither=Image.Dither.FLOYDSTEINBERG)
    return gray.point(lambda value: 255 if value >= threshold else 0).convert("1")


def pack_xbm(mono):
    """Rows top to bottom, LSB is the leftmost pixel, set bit = black"""
    width, height = mono.size
    pixels = mono.load()
    data = bytearray()
    for y in range(height):
        for byte_x in range(0, width, 8):
            value = 0
            for bit in range(8):
                x = byte_x + bit
                if x < width and pixels[x, y] == 0:
                    value |= 1 << bit
            data.append(value)
    return bytes(data)


def demo_frames(count=16):
    """A paw that hops while hearts rise, drawn in black on white"""
    frames = []
    for index in range(count):
        image = Image.new("RGBA", (WIDTH_MAX, HEIGHT_MAX), (255, 255, 255, 255))
        draw = ImageDraw.Draw(image)
        phase = index / count * 2 * math.pi

        # Paw: pad and four toes, hopping
        cx = 64
        cy = 30 - round(abs(math.sin(phase)) * 8)
        draw.ellipse((cx - 11, cy - 4, cx + 11, cy + 14), fill="black")
        for dx, dy in ((-15, -8), (-6, -15), (6, -15), (15, -8)):
            draw.ellipse(
                (cx + dx - 4, cy + dy - 5, cx + dx + 4, cy + dy + 5), fill="black"
            )

        # Hearts rising on both sides
        for column, offset in ((18, 0.0), (110, 0.5), (36, 0.25), (92, 0.75)):
            progress = (index / count + offset) % 1.0
            hx = column + round(math.sin(progress * 4 * math.pi) * 3)
            hy = round(56 - progress * 56)
            draw.ellipse((hx - 4, hy - 3, hx, hy + 1), fill="black")
            draw.ellipse((hx, hy - 3, hx + 4, hy + 1), fill="black")
            draw.polygon(((hx - 4, hy), (hx + 4, hy), (hx, hy + 5)), fill="black")

        frames.append((image, 80))
    return frames


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "files", nargs="+", help="input images or GIFs, then the output .anim"
    )
    parser.add_argument(
        "--demo", action="store_true", help="built-in demo instead of inputs"
    )
    parser.add_argument(
        "--name", default="", help=f"pet name shown on top, up to {NAME_SIZE} bytes"
    )
    parser.add_argument(
        "--loops", type=int, default=2, help="how often to play (1..255)"
    )
    parser.add_argument(
        "--frame-ms", type=int, help="frame duration, default: from the GIF or 100"
    )
    parser.add_argument(
        "--invert", action="store_true", help="invert, for dark backgrounds"
    )
    parser.add_argument(
        "--dither", action="store_true", help="Floyd-Steinberg dithering"
    )
    parser.add_argument(
        "--threshold", type=int, default=128, help="black below this (0..255)"
    )
    parser.add_argument("--preview", help="also write the converted frames as a GIF")
    args = parser.parse_args()

    *inputs, output = args.files
    if args.demo:
        if inputs:
            parser.error("--demo takes only the output file")
        frames = demo_frames()
    else:
        if not inputs:
            parser.error("no input images")
        frames = load_frames(inputs)

    if len(frames) > FRAMES_MAX:
        print(f"{len(frames)} frames, using the first {FRAMES_MAX}", file=sys.stderr)
        frames = frames[:FRAMES_MAX]

    name = args.name.encode("utf-8")
    if len(name) > NAME_SIZE:
        parser.error(f"name is longer than {NAME_SIZE} bytes")
    if not 1 <= args.loops <= 255:
        parser.error("loops must be 1..255")

    durations = [duration for _, duration in frames if duration]
    frame_ms = args.frame_ms or (
        round(sum(durations) / len(durations)) if durations else 100
    )
    frame_ms = min(max(frame_ms, FRAME_MS_MIN), FRAME_MS_MAX)

    size = fit_size(*frames[0][0].size)
    mono_frames = [
        to_mono(image, size, args.invert, args.dither, args.threshold)
        for image, _ in frames
    ]

    header = struct.pack(
        "<4sBBBBHBB20s",
        MAGIC,
        VERSION,
        size[0],
        size[1],
        len(mono_frames),
        frame_ms,
        args.loops,
        0,
        name,
    )
    with open(output, "wb") as file:
        file.write(header)
        for mono in mono_frames:
            file.write(pack_xbm(mono))

    if args.preview:
        preview = [mono.convert("L") for mono in mono_frames]
        preview[0].save(
            args.preview,
            save_all=True,
            append_images=preview[1:],
            duration=frame_ms,
            loop=0,
        )

    print(
        f"{output}: {size[0]}x{size[1]}, {len(mono_frames)} frames, {frame_ms} ms, "
        f"{args.loops} loops, {len(header) + sum(len(pack_xbm(m)) for m in mono_frames)} bytes"
    )


if __name__ == "__main__":
    main()

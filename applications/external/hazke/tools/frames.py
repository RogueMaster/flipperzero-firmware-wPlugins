"""Turn a Hazke capture (frames.bin) into PNGs and GIFs.

frames.bin holds records of a 4-byte little-endian tick (ms) and the
1024-byte screen buffer in display page layout. Pixels are drawn in the
qFlipper screenshot colours: set bits black, clear bits orange, scaled 4x
to 512x256.

  python3 frames.py frames.bin sheet OUT.png [STEP]     contact sheet, every STEP-th frame, numbered
  python3 frames.py frames.bin png N OUT.png            one frame
  python3 frames.py frames.bin gif A B OUT.gif [SCALE]  frames A..B as a GIF (needs ffmpeg)
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

W, H, REC = 128, 64, 1028
ORANGE, BLACK = (254, 138, 44), (0, 0, 0)


def load(path):
    data = open(path, "rb").read()
    out = []
    for i in range(len(data) // REC):
        rec = data[i * REC : (i + 1) * REC]
        out.append((struct.unpack("<I", rec[:4])[0], rec[4:]))
    return out


def pixels(buf):
    return [
        [bool(buf[(y >> 3) * W + x] & (1 << (y & 7))) for x in range(W)]
        for y in range(H)
    ]


def write_png(path, rows):
    h, w = len(rows), len(rows[0])
    raw = b"".join(b"\x00" + bytes(v for px in r for v in px) for r in rows)

    def chunk(t, d):
        return (
            struct.pack(">I", len(d))
            + t
            + d
            + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
        )

    png = b"\x89PNG\r\n\x1a\n" + chunk(
        b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    )
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def scaled(buf, s):
    px = pixels(buf)
    rows = []
    for y in range(H):
        row = []
        for x in range(W):
            row += [BLACK if px[y][x] else ORANGE] * s
        rows += [row] * s
    return rows


DIGITS = {  # 3x5, for frame numbers on the contact sheet
    "0": "111101101101111",
    "1": "010110010010111",
    "2": "111001111100111",
    "3": "111001111001111",
    "4": "101101111001001",
    "5": "111100111001111",
    "6": "111100111101111",
    "7": "111001001010010",
    "8": "111101111101111",
    "9": "111101111001111",
}


def sheet(frames, path, step):
    picks = list(range(0, len(frames), step))
    cols, s, pad = 6, 2, 10
    cw, ch = W * s + pad, H * s + pad
    rows_n = (len(picks) + cols - 1) // cols
    img = [[(40, 40, 40)] * (cols * cw + pad) for _ in range(rows_n * ch + pad)]
    for k, i in enumerate(picks):
        ox, oy = pad + (k % cols) * cw, pad + (k // cols) * ch
        for y, r in enumerate(scaled(frames[i][1], s)):
            img[oy + y][ox : ox + W * s] = r
        for d, c in enumerate(str(i)):
            g = DIGITS[c]
            for b in range(15):
                if g[b] == "1":
                    for dy in range(2):
                        for dx in range(2):
                            img[oy - 8 + (b // 3) * 2 + dy - 2 + 2][
                                ox + d * 8 + (b % 3) * 2 + dx
                            ] = (255, 255, 255)
    write_png(path, img)


def gif(frames, a, b, path, s):
    tmp = tempfile.mkdtemp()
    try:
        lst = os.path.join(tmp, "list.txt")
        with open(lst, "w") as f:
            for i in range(a, b + 1):
                p = os.path.join(tmp, f"{i:05d}.png")
                write_png(p, scaled(frames[i][1], s))
                dur = (
                    (frames[i + 1][0] - frames[i][0]) / 1000
                    if i + 1 < len(frames)
                    else 0.05
                )
                f.write(f"file '{p}'\nduration {max(0.02, min(dur, 0.5)):.3f}\n")
            f.write(f"file '{p}'\n")
        subprocess.run(
            [
                "ffmpeg",
                "-y",
                "-loglevel",
                "error",
                "-f",
                "concat",
                "-safe",
                "0",
                "-i",
                lst,
                "-vf",
                "split[a][b];[a]palettegen=max_colors=4:reserve_transparent=0[p];[b][p]paletteuse=dither=none",
                "-loop",
                "0",
                path,
            ],
            check=True,
        )
    finally:
        shutil.rmtree(tmp)


if __name__ == "__main__":
    frames = load(sys.argv[1])
    mode = sys.argv[2]
    if mode == "sheet":
        sheet(frames, sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 10)
        print(f"{len(frames)} frames, {(frames[-1][0] - frames[0][0]) / 1000:.1f} s")
    elif mode == "png":
        write_png(sys.argv[4], scaled(frames[int(sys.argv[3])][1], 4))
    elif mode == "gif":
        gif(
            frames,
            int(sys.argv[3]),
            int(sys.argv[4]),
            sys.argv[5],
            int(sys.argv[6]) if len(sys.argv) > 6 else 4,
        )

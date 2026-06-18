#!/usr/bin/env python3
import argparse
import struct
import sys

HEADER_SIZE_MIN = 64
MAGIC = 0x31574346
MIN_TILE = 3
MAX_TILE = 48

COLORS = {
    0: (18, 19, 22),
    1: (58, 166, 69),
    2: (133, 91, 50),
    3: (118, 121, 122),
    4: (96, 99, 101),
    5: (119, 76, 38),
    6: (42, 128, 47),
    7: (179, 126, 68),
    8: (45, 47, 49),
    9: (176, 136, 95),
    10: (218, 198, 118),
    11: (150, 212, 225),
    12: (52, 145, 55),
    13: (161, 108, 55),
    14: (83, 83, 83),
    15: (183, 119, 42),
}
UNKNOWN = (230, 0, 230)

DIGITS = {
    "-": ("000", "000", "111", "000", "000"),
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"),
    "7": ("111", "001", "010", "010", "010"),
    "8": ("111", "101", "111", "101", "111"),
    "9": ("111", "101", "111", "001", "111"),
}


def read_header(f):
    hdr = f.read(HEADER_SIZE_MIN)
    if len(hdr) < HEADER_SIZE_MIN:
        sys.exit("file too small")
    magic, version, cx, cz, sx, sy, sz, bpb, header_size = struct.unpack_from("<IHHHBBBBI", hdr, 0)
    if magic != MAGIC:
        sys.exit("bad magic")
    return {
        "version": version,
        "chunks_x": cx,
        "chunks_z": cz,
        "chunk_x": sx,
        "height": sy,
        "chunk_z": sz,
        "bpb": bpb,
        "header_size": header_size,
    }


def chunk_offset(meta, cx, cz):
    chunk_bytes = meta["chunk_x"] * meta["height"] * meta["chunk_z"] * meta["bpb"]
    return meta["header_size"] + (cz * meta["chunks_x"] + cx) * chunk_bytes


def load_world(path):
    with open(path, "rb") as f:
        meta = read_header(f)
        wx = meta["chunks_x"] * meta["chunk_x"]
        wz = meta["chunks_z"] * meta["chunk_z"]
        surf = [[0] * wx for _ in range(wz)]
        heights = [[-1] * wx for _ in range(wz)]
        chunk_bytes = meta["chunk_x"] * meta["height"] * meta["chunk_z"] * meta["bpb"]
        for cz in range(meta["chunks_z"]):
            for cx in range(meta["chunks_x"]):
                f.seek(chunk_offset(meta, cx, cz))
                chunk = f.read(chunk_bytes)
                if len(chunk) != chunk_bytes:
                    sys.exit("truncated world")
                for lz in range(meta["chunk_z"]):
                    for lx in range(meta["chunk_x"]):
                        bid = 0
                        top = -1
                        for y in range(meta["height"]):
                            v = chunk[(y * meta["chunk_x"] + lz) * meta["chunk_x"] + lx]
                            if v:
                                bid = v
                                top = y
                        x = cx * meta["chunk_x"] + lx
                        z = cz * meta["chunk_z"] + lz
                        surf[z][x] = bid
                        heights[z][x] = top
        return meta, surf, heights, wx, wz


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def shade(rgb, h, max_h):
    if h < 0:
        return rgb
    k = 0.62 + 0.52 * h / max(1, max_h - 1)
    return tuple(clamp(int(c * k), 0, 255) for c in rgb)


def text_color(rgb):
    lum = rgb[0] * 0.299 + rgb[1] * 0.587 + rgb[2] * 0.114
    return (20, 22, 24) if lum > 145 else (235, 238, 240)


def draw_digit_text(pg, screen, text, x, y, scale, color):
    text = str(text)
    w = len(text) * 4 * scale - scale
    ox = x - w // 2
    for i, ch in enumerate(text):
        glyph = DIGITS.get(ch)
        if not glyph:
            continue
        gx0 = ox + i * 4 * scale
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit == "1":
                    pg.draw.rect(screen, color, (gx0 + gx * scale, y + gy * scale, scale, scale))


def main():
    ap = argparse.ArgumentParser(description="Minimal pygame Flipcraft map viewer")
    ap.add_argument("world")
    ap.add_argument("--tile", type=int, default=8)
    args = ap.parse_args()

    try:
        import pygame as pg
    except ImportError:
        sys.exit("pygame is required")

    meta, surf, heights, wx, wz = load_world(args.world)
    pg.init()
    screen = pg.display.set_mode((1100, 850), pg.RESIZABLE)
    pg.display.set_caption("Flipcraft map")
    clock = pg.time.Clock()

    tile = clamp(args.tile, MIN_TILE, MAX_TILE)
    cam_x = max(0, wx * tile / 2 - screen.get_width() / 2)
    cam_z = max(0, wz * tile / 2 - screen.get_height() / 2)
    dragging = False
    last_mouse = (0, 0)
    show_heights = True

    def clamp_camera():
        nonlocal cam_x, cam_z
        cam_x = clamp(cam_x, 0, max(0, wx * tile - screen.get_width()))
        cam_z = clamp(cam_z, 0, max(0, wz * tile - screen.get_height()))

    def zoom_at(pos, new_tile):
        nonlocal tile, cam_x, cam_z
        new_tile = clamp(new_tile, MIN_TILE, MAX_TILE)
        if new_tile == tile:
            return
        mx, my = pos
        bx = (cam_x + mx) / tile
        bz = (cam_z + my) / tile
        tile = new_tile
        cam_x = bx * tile - mx
        cam_z = bz * tile - my
        clamp_camera()

    running = True
    while running:
        dt = clock.tick(60) / 1000.0
        for ev in pg.event.get():
            if ev.type == pg.QUIT:
                running = False
            elif ev.type == pg.KEYDOWN:
                if ev.key in (pg.K_ESCAPE, pg.K_q):
                    running = False
                elif ev.key in (pg.K_EQUALS, pg.K_PLUS, pg.K_KP_PLUS):
                    zoom_at(pg.mouse.get_pos(), tile + 2)
                elif ev.key in (pg.K_MINUS, pg.K_KP_MINUS):
                    zoom_at(pg.mouse.get_pos(), tile - 2)
                elif ev.key == pg.K_h:
                    show_heights = not show_heights
            elif ev.type == pg.MOUSEWHEEL:
                zoom_at(pg.mouse.get_pos(), tile + ev.y * 2)
            elif ev.type == pg.MOUSEBUTTONDOWN and ev.button == 1:
                dragging = True
                last_mouse = ev.pos
            elif ev.type == pg.MOUSEBUTTONUP and ev.button == 1:
                dragging = False
            elif ev.type == pg.MOUSEMOTION and dragging:
                dx = ev.pos[0] - last_mouse[0]
                dz = ev.pos[1] - last_mouse[1]
                cam_x -= dx
                cam_z -= dz
                last_mouse = ev.pos
                clamp_camera()
            elif ev.type == pg.VIDEORESIZE:
                clamp_camera()

        keys = pg.key.get_pressed()
        speed = max(180, tile * 24) * dt
        if keys[pg.K_LSHIFT] or keys[pg.K_RSHIFT]:
            speed *= 2.5
        if keys[pg.K_LEFT] or keys[pg.K_a]:
            cam_x -= speed
        if keys[pg.K_RIGHT] or keys[pg.K_d]:
            cam_x += speed
        if keys[pg.K_UP] or keys[pg.K_w]:
            cam_z -= speed
        if keys[pg.K_DOWN] or keys[pg.K_s]:
            cam_z += speed
        clamp_camera()

        sw, sh = screen.get_size()
        first_x = max(0, int(cam_x // tile))
        first_z = max(0, int(cam_z // tile))
        last_x = min(wx, int((cam_x + sw) // tile) + 2)
        last_z = min(wz, int((cam_z + sh) // tile) + 2)
        screen.fill((10, 11, 13))

        for z in range(first_z, last_z):
            py = int(z * tile - cam_z)
            for x in range(first_x, last_x):
                px = int(x * tile - cam_x)
                h = heights[z][x]
                color = shade(COLORS.get(surf[z][x], UNKNOWN), h, meta["height"])
                pg.draw.rect(screen, color, (px, py, tile + 1, tile + 1))
                if tile >= 9:
                    border = tuple(max(0, c - 34) for c in color)
                    pg.draw.rect(screen, border, (px, py, tile, tile), 1)
                if show_heights and tile >= 15:
                    scale = 1 if tile < 28 else 2
                    draw_digit_text(pg, screen, h if h >= 0 else "-", px + tile // 2, py + (tile - 5 * scale) // 2, scale, text_color(color))

        pg.display.flip()

    pg.quit()


if __name__ == "__main__":
    main()

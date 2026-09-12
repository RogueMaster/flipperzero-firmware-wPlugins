#!/usr/bin/env python3
# Copyright (c) 2026 ApertureFox Technology. MIT License.
"""
Flipcraft world generator -- a port of src/world/gen_core.h, the generator the
app itself runs on the device. Same seed, same size, same terrain preset and
the same flags produce a byte-identical .fcw; the C core is the reference and
this file follows it function for function, so port every change both ways.

Two things make the match exact rather than approximate:

  * every float is a numpy float32, because the C core computes in float and
    plain Python doubles round differently -- one ulp is enough to move a
    block boundary or flip a biome test;
  * the feature passes (cell maxima, the greedy spacing pass, the stamp order)
    follow the C control flow, including how ties are broken.

File format (v3), little-endian:

  Header : 64 bytes
           0  u32  magic 'FCW1' (0x31574346)
           4  u16  version (3)
           6  u16  chunksX        8  u16 chunksZ
           10 u8   chunkSX (8)    11 u8  chunkSY (16)   12 u8 chunkSZ (8)
           13 u8   bytesPerBlock (1)
           14 u32  headerSize (64)
           18 i32  playerX (pixels, 16 px = 1 block)   22 i32 playerY
           26 i32  playerZ         30 u8  rot          31 u8 reserved
           32 u32  rngState (the seed)
           36 u8   settings flags (plugin_api.h FlipcraftFlag*)
           37 u8   terrain preset (plugin_api.h FlipcraftWorldType)
           38..63 reserved (zero)
  Chunks : chunksX*chunksZ payloads, row-major (index = cz*chunksX + cx), each
           8*16*8 = 1024 bytes, one block id per byte, laid out [y][z][x]:
               payload[(y*8 + z)*8 + x]
           Chunk byte offset = headerSize + (cz*chunksX + cx) * 1024.
  Tiles  : the tile-entity region -- 32 B inventory, the dead 4096 B v2 pad,
           then 256 slots of 32 B. The generator writes the furnace and the
           chest of the world's single house and leaves the rest zeroed.

Usage:
    python3 tools/worldgen.py
    python3 tools/worldgen.py --chunks 32 --seed 12345 --type woods
    python3 tools/worldgen.py -o /tmp/flat.fcw --type flat --flags 2
"""

import argparse
import secrets
import struct
import sys
from pathlib import Path

import numpy as np

F32 = np.float32
U32 = np.uint32
MASK32 = 0xFFFFFFFF

CHUNK = 8
HEIGHT = 16
TILE_CHUNKS = 16
TILE_W = TILE_CHUNKS * CHUNK  # 128 blocks
MAX_CHUNKS = 128  # 1024 blocks
CHUNK_BYTES = CHUNK * HEIGHT * CHUNK
BLOCKSIZE = 16
HEADER_SIZE = 64
MAGIC = 0x31574346
VERSION = 3

# Terrain presets (plugin_api.h FlipcraftWorldType)
TYPE_NORMAL, TYPE_FLAT, TYPE_SUPERFLAT, TYPE_WOODS = 0, 1, 2, 3
TYPE_NAMES = {
    "normal": TYPE_NORMAL,
    "flat": TYPE_FLAT,
    "superflat": TYPE_SUPERFLAT,
    "woods": TYPE_WOODS,
}
FLAT_TOP = 4

# Block ids (flipcraft.h enum Block); water = source level
AIR, GRASS, DIRT, STONE, COBBLE, LOG, LEAVES, PLANK = 0, 1, 2, 3, 4, 5, 6, 7
COALORE, IRONORE, SAND = 8, 9, 10
TABLE, FURNACE, CHEST, WATER = 13, 14, 15, 20
SEA_LEVEL = 3

# v3 tile-entity region layout, must match world.cpp/game.cpp packStorage()
INV_REGION, PAD_V2 = 32, 4096
SLOT_CAP, SLOT_SIZE = 256, 32

ALLOW_ANY = 0xFFFF
ALLOW_AIR = 1 << AIR
ALLOW_AIR_LEAVES = (1 << AIR) | (1 << LEAVES)

# One byte per column: bits 0-3 terrain top y, then biome flags.
COL_TOP, COL_DESERT, COL_RAVINE, COL_FOREST, COL_RIVER = 0x0F, 0x10, 0x20, 0x40, 0x80

FALLEN_LEN = 4
MAX_TREES = 192
MAX_CANDS = 1024

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORLD_DIR = PROJECT_ROOT / "assets" / "worlds"
DEFAULT_WORLD_NAME = "word.fcw"

# Generator state, the C core's globals.
g_seed = 0
g_type = TYPE_NORMAL
g_worldW = 0
g_tileX0 = g_tileZ0 = 0
g_tileW = TILE_W
g_col = None  # uint8 [tileW][tileW], numpy
g_col_l = None  # the same map as lists of ints, for the feature passes


def resolve_output_path(out):
    path = Path(out)
    if path.is_absolute() or path.parent != Path("."):
        return path
    return DEFAULT_WORLD_DIR / path


# world-hash / noise: the C core's whash and its 256-entry gradient table.


def whash(x, z, salt):
    """Vectorised whash; x and z are integer arrays."""
    s = (salt + g_seed * 1013904223) & MASK32
    h = (
        np.asarray(x, dtype=np.int64).astype(U32) * U32(374761393)
        + np.asarray(z, dtype=np.int64).astype(U32) * U32(668265263)
        + U32((s * 362437) & MASK32)
    )
    h = (h ^ (h >> U32(13))) * U32(1274126177)
    return h ^ (h >> U32(16))


def whash_s(x, z, salt):
    """Scalar whash, plain Python ints; used by the feature passes."""
    s = (salt + g_seed * 1013904223) & MASK32
    h = (x * 374761393 + z * 668265263 + s * 362437) & MASK32
    h = ((h ^ (h >> 13)) * 1274126177) & MASK32
    return h ^ (h >> 16)


def _grad_table():
    # C: float a = (float)i * (6.28318530718f / 256.0f); cosf(a), sinf(a)
    step = F32(6.28318530718) / F32(256.0)
    a = (np.arange(256, dtype=np.float32) * step).astype(np.float64)
    return np.cos(a).astype(np.float32), np.sin(a).astype(np.float32)


GRAD_X, GRAD_Z = _grad_table()


def fadef(t):
    return t * t * t * (t * (t * F32(6) - F32(15)) + F32(10))


def lerpf(a, b, t):
    return a + (b - a) * t


def smoothstepf(e0, e1, x):
    e0, e1 = F32(e0), F32(e1)
    t = np.clip((x - e0) / (e1 - e0), F32(0.0), F32(1.0))
    return t * t * (F32(3.0) - F32(2.0) * t)


def ifloorf(x):
    i = x.astype(np.int32)
    return np.where(i.astype(np.float32) > x, i - 1, i)


def _grad(x, z, salt):
    idx = (whash(x, z, salt) >> U32(8)) & U32(0xFF)
    return GRAD_X[idx], GRAD_Z[idx]


def perlin(x, z, salt):
    x0, z0 = ifloorf(x), ifloorf(z)
    fx = x - x0.astype(np.float32)
    fz = z - z0.astype(np.float32)
    sx, sz = fadef(fx), fadef(fz)

    g00x, g00z = _grad(x0, z0, salt)
    g10x, g10z = _grad(x0 + 1, z0, salt)
    g01x, g01z = _grad(x0, z0 + 1, salt)
    g11x, g11z = _grad(x0 + 1, z0 + 1, salt)

    n00 = g00x * fx + g00z * fz
    n10 = g10x * (fx - F32(1)) + g10z * fz
    n01 = g01x * fx + g01z * (fz - F32(1))
    n11 = g11x * (fx - F32(1)) + g11z * (fz - F32(1))
    return lerpf(lerpf(n00, n10, sx), lerpf(n01, n11, sx), sz) * F32(1.41421356237)


def fbm(x, z, salt, octaves):
    total = np.zeros_like(x)
    amp, norm = F32(1), F32(0)
    for i in range(octaves):
        total = total + perlin(x, z, salt + i * 1013) * amp
        norm = norm + amp
        x = x * F32(2.0)
        z = z * F32(2.0)
        amp = amp * F32(0.5)
    return total / norm


def ridged(x, z, salt, octaves):
    a = np.abs(fbm(x, z, salt, octaves)) * F32(2.4)
    return F32(1.0) - np.where(a > F32(1.0), F32(1.0), a)


def compute_columns(xs, zs):
    """computeColumn over a grid: xs and zs are int arrays of the same shape."""
    if g_type in (TYPE_FLAT, TYPE_SUPERFLAT):
        flag = COL_FOREST if g_type == TYPE_FLAT else 0
        return np.full(np.shape(xs), FLAT_TOP | flag, dtype=np.uint8)

    x = np.asarray(xs, dtype=np.int32)
    z = np.asarray(zs, dtype=np.int32)
    xf, zf = x.astype(np.float32), z.astype(np.float32)
    S = F32(1.0) / F32(TILE_W)
    u, v = xf * S, zf * S

    warp_x = fbm(u * F32(3.0) + F32(17.0), v * F32(3.0) - F32(11.0), 20, 3) * F32(18.0)
    warp_z = fbm(u * F32(3.0) - F32(7.0), v * F32(3.0) + F32(23.0), 21, 3) * F32(18.0)
    px, pz = (xf + warp_x) * S, (zf + warp_z) * S

    continent = fbm(px * F32(2.2), pz * F32(2.2), 1, 5)
    detail = fbm(px * F32(7.5) + F32(9.0), pz * F32(7.5) - F32(4.0), 2, 3) * F32(0.55)
    hill_field = fbm(px * F32(3.2) - F32(13.0), pz * F32(3.2) + F32(5.0), 3, 4)
    hill_mask = smoothstepf(0.04, 0.31, hill_field)
    hill = hill_mask * smoothstepf(-0.08, 0.32, continent + detail) * F32(6.2)

    valley = ridged(px * F32(3.0) + F32(2.5), pz * F32(3.0) - F32(1.5), 4, 4)
    valley_gate = smoothstepf(
        0.04, 0.38, fbm(px * F32(1.5) + F32(4.0), pz * F32(1.5) - F32(6.0), 5, 3)
    )
    ravine_depth = (
        smoothstepf(0.72, 0.92, valley) * valley_gate * F32(4.0) + F32(0.5)
    ).astype(np.int32)

    moisture = fbm(px * F32(2.4) - F32(8.0), pz * F32(2.4) + F32(19.0), 6, 5)
    temperature = fbm(px * F32(1.8) + F32(31.0), pz * F32(1.8) - F32(17.0), 7, 4) + (
        xf / F32(g_worldW) - F32(0.5)
    ) * F32(0.35)
    desert_score = (
        temperature * F32(0.68)
        - moisture * F32(0.72)
        + fbm(px * F32(5.0), pz * F32(5.0), 8, 3) * F32(0.24)
    )
    desert = (g_type != TYPE_WOODS) & (desert_score > F32(0.18)) & (ravine_depth < 3)

    forest_score = (
        moisture * F32(0.78)
        - np.abs(temperature) * F32(0.22)
        + fbm(px * F32(6.0) + F32(5.0), pz * F32(6.0), 9, 3) * F32(0.18)
    )
    forest = (
        (ravine_depth == 0)
        & ~desert
        & ((g_type == TYPE_WOODS) | (forest_score > F32(0.02)))
    )

    top_f = (
        F32(4.4)
        + continent * F32(1.6)
        + detail
        + hill
        - ravine_depth.astype(np.float32)
    )
    top = (top_f + F32(0.5)).astype(np.int32)
    top = np.clip(top, 1, 10)

    # A ravine that cuts down to the local water table becomes a river.
    bed = (F32(1.4) + continent * F32(1.6) + F32(0.5)).astype(np.int32)
    bed = np.clip(bed, 1, 8)
    river = (ravine_depth > 0) & (top <= bed + 2)
    top = np.where(river, bed, top)

    return (
        top.astype(np.uint8)
        | np.where(desert, np.uint8(COL_DESERT), np.uint8(0))
        | np.where(ravine_depth > 0, np.uint8(COL_RAVINE), np.uint8(0))
        | np.where(forest, np.uint8(COL_FOREST), np.uint8(0))
        | np.where(river, np.uint8(COL_RIVER), np.uint8(0))
    )


# Per-tile column map and the feature passes.


def col_at(x, z):
    return g_col_l[z - g_tileZ0][x - g_tileX0]


def top_at(x, z):
    return col_at(x, z) & COL_TOP


def sand_column(c):
    return bool(c & (COL_DESERT | COL_RAVINE)) or (c & COL_TOP) <= 2


def local_slope(x, z):
    h = top_at(x, z)
    s = 0
    for dz in (-1, 0, 1):
        for dx in (-1, 0, 1):
            d = abs(h - top_at(x + dx, z + dz))
            if d > s:
                s = d
    return s


def collect_cell_maxima(cell_size, lo, hi, min_score, eligible, score):
    """Best candidate of every cell_size x cell_size cell, best score first."""
    cells = g_tileW // cell_size
    best = [None] * (cells * cells)
    for lz in range(lo, hi + 1):
        for lx in range(lo, hi + 1):
            x, z = g_tileX0 + lx, g_tileZ0 + lz
            if not eligible(x, z):
                continue
            s = score[lz, lx]
            if s <= min_score:
                continue
            i = (lz // cell_size) * cells + (lx // cell_size)
            cur = best[i]
            if cur is None or s > cur[0]:
                best[i] = (s, lx, lz)
    cand = [c for c in best if c is not None]
    cand.sort(key=lambda c: -float(c[0]))  # stable, as the C insertion sort
    return cand[:MAX_CANDS]


def greedy_place(cands, cap, min_dist2, fits):
    out = []
    for s, lx, lz in cands:
        if len(out) >= cap:
            break
        near = False
        for px, pz, _ in out:
            dx, dz = lx - px, lz - pz
            if dx * dx + dz * dz < min_dist2:
                near = True
                break
        if near:
            continue
        if fits and not fits(g_tileX0 + lx, g_tileZ0 + lz):
            continue
        out.append((lx, lz, 0))
    return out


def tile_score(salt_a, mul, off_x, off_z, octaves, extra_salt=None, extra_div=0.0):
    """fbm(x*mul + off_x, z*mul + off_z) over the tile, plus an optional hash term."""
    lx = np.arange(g_tileW, dtype=np.int32) + g_tileX0
    lz = np.arange(g_tileW, dtype=np.int32) + g_tileZ0
    xg, zg = np.meshgrid(lx, lz)
    xf, zf = xg.astype(np.float32), zg.astype(np.float32)
    s = fbm(xf * F32(mul) + F32(off_x), zf * F32(mul) + F32(off_z), salt_a, octaves)
    if extra_salt is not None:
        s = s + (whash(xg, zg, extra_salt) & U32(255)).astype(np.float32) / F32(
            extra_div
        )
    return s


def tree_eligible(x, z):
    return bool(col_at(x, z) & COL_FOREST) and local_slope(x, z) <= 1


def tree_fits(x, z):
    c = col_at(x, z)
    if sand_column(c):
        return False
    top = c & COL_TOP
    if whash_s(x, z, 64) & 1:
        trunk = 3 + whash_s(x, z, 61) % 2
        return top + trunk + 2 <= HEIGHT - 1
    lower = top + 3 + (whash_s(x, z, 61) & 1)
    return lower + 2 <= HEIGHT - 1


def clear_ground_eligible(x, z):
    c = col_at(x, z)
    return (
        not (c & (COL_DESERT | COL_RAVINE))
        and (c & COL_TOP) >= SEA_LEVEL
        and local_slope(x, z) <= 1
    )


def fallen_fits(x, z):
    along_x = (whash_s(x, z, 72) & 1) == 0
    for i in range(FALLEN_LEN):
        tx = x + i if along_x else x
        tz = z if along_x else z + i
        if tx - g_tileX0 < 1 or tx - g_tileX0 >= g_tileW - 1:
            return False
        if tz - g_tileZ0 < 1 or tz - g_tileZ0 >= g_tileW - 1:
            return False
        if col_at(tx, tz) & (COL_DESERT | COL_RAVINE):
            return False
    return True


def house_eligible(x, z):
    base = top_at(x + 2, z + 2)
    for dz in range(5):
        for dx in range(5):
            c = col_at(x + dx, z + dz)
            if (c & (COL_DESERT | COL_RAVINE)) or (c & COL_TOP) < SEA_LEVEL:
                return False
            d = (c & COL_TOP) - base
            if d < -1 or d > 1:
                return False
    return True


def place_features(allow_house):
    """The C placeFeatures: trees, fallen trunks, stone piles, then the house."""
    if g_type == TYPE_SUPERFLAT:
        return [], [], [], []

    w = g_tileW
    woods = g_type == TYPE_WOODS

    trees_score = tile_score(62, 0.19, 4.0, -9.0, 3, extra_salt=63, extra_div=512.0)
    cands = collect_cell_maxima(
        4, 3, w - 4, F32(-1e29) if woods else F32(0.23), tree_eligible, trees_score
    )
    tree_cap = w * w // 80 if woods else w * w // 190
    tree_cap = max(24, min(tree_cap, MAX_TREES))
    trees = greedy_place(cands, tree_cap, 16 if woods else 36, tree_fits)

    fallen_s = tile_score(70, 0.11, 0.0, 0.0, 3, extra_salt=71, extra_div=700.0)
    cands = collect_cell_maxima(
        8, 4, w - 5, F32(-1e29), clear_ground_eligible, fallen_s
    )
    fallen = greedy_place(cands, 5, 225, fallen_fits)

    pile_s = tile_score(80, 0.13, 11.0, -3.0, 3)
    cands = collect_cell_maxima(8, 3, w - 4, F32(-1e29), clear_ground_eligible, pile_s)
    piles = greedy_place(cands, 6, 196, None)

    houses = []
    if allow_house:
        house_s = tile_score(90, 0.08, 19.0, -7.0, 3, extra_salt=91, extra_div=900.0)
        cands = collect_cell_maxima(8, 5, w - 9, F32(-1e29), house_eligible, house_s)

        def house_fits(x, z):  # keep the walls clear of a placed tree canopy
            lx, lz = x - g_tileX0, z - g_tileZ0
            for tx, tz, _ in trees:
                if lx - 2 <= tx <= lx + 6 and lz - 2 <= tz <= lz + 6:
                    return False
            return True

        houses = greedy_place(cands, 1, 625, house_fits)
        if houses:
            lx, lz, _ = houses[0]
            houses[0] = (lx, lz, top_at(g_tileX0 + lx + 2, g_tileZ0 + lz + 2))
    return trees, fallen, piles, houses


# Terrain and feature stamps, written into one tile-sized [y][z][x] buffer.


def fill_terrain(tile):
    col = g_col
    top = (col & COL_TOP).astype(np.int32)
    y = np.arange(HEIGHT, dtype=np.int32)[:, None, None]
    ground = y <= top

    if g_type in (TYPE_FLAT, TYPE_SUPERFLAT):
        ids = np.where(
            y == top,
            np.uint8(GRASS),
            np.where(y == top - 1, np.uint8(DIRT), np.uint8(STONE)),
        )
    else:
        xs = np.arange(g_tileW, dtype=np.int32) + g_tileX0
        zs = np.arange(g_tileW, dtype=np.int32) + g_tileZ0
        xg, zg = np.meshgrid(xs, zs)
        r = whash(
            xg[None, :, :] * np.int32(17) + y,
            np.broadcast_to(zg, (HEIGHT,) + zg.shape),
            40,
        )
        stone = np.where(
            r % U32(47) == 0,
            np.uint8(COALORE),
            np.where(r % U32(67) == 0, np.uint8(IRONORE), np.uint8(STONE)),
        )
        sand = ((col & (COL_DESERT | COL_RAVINE)) != 0) | (top <= 2)
        surface = np.where(sand, np.uint8(SAND), np.uint8(GRASS))
        subsoil = np.where(sand, np.uint8(SAND), np.uint8(DIRT))
        ids = np.where(y == top, surface, np.where(y >= top - 2, subsoil, stone))

    tile[:] = np.where(ground, ids, np.uint8(AIR))

    # Sea fills every low column to SEA_LEVEL, a river to its own bed + 2.
    water_top = np.where((col & COL_RIVER) != 0, top + 2, np.int32(SEA_LEVEL))
    tile[:] = np.where((y > top) & (y <= water_top), np.uint8(WATER), tile)


def ch_set(tile, x, y, z, bid, allow):
    lx, lz = x - g_tileX0, z - g_tileZ0
    if not (0 <= lx < g_tileW and 0 <= lz < g_tileW and 0 <= y < HEIGHT):
        return
    cur = tile[y, lz, lx]
    if allow != ALLOW_ANY and not ((allow >> int(cur)) & 1):
        return
    tile[y, lz, lx] = bid


def stamp_tree(tile, x, z):
    top = top_at(x, z)
    if whash_s(x, z, 64) & 1:  # classic: diamond crown
        trunk = 3 + whash_s(x, z, 61) % 2
        crown = top + trunk
        for dz in range(-2, 3):
            for dx in range(-2, 3):
                d = abs(dx) + abs(dz)
                if d <= 3:
                    ch_set(tile, x + dx, crown, z + dz, LEAVES, ALLOW_AIR)
                if d <= 2:
                    ch_set(tile, x + dx, crown + 1, z + dz, LEAVES, ALLOW_AIR)
        ch_set(tile, x, crown + 2, z, LEAVES, ALLOW_AIR)
        for ty in range(top + 1, crown + 2):
            ch_set(tile, x, ty, z, LOG, ALLOW_AIR_LEAVES)
    else:  # sapling-shaped: two square layers
        y0 = top + 1
        lower = y0 + 2 + (whash_s(x, z, 61) & 1)
        upper = lower + 2
        for ly in (lower - 1, lower):
            for dx in range(-2, 3):
                for dz in range(-2, 3):
                    ch_set(tile, x + dx, ly, z + dz, LEAVES, ALLOW_AIR)
        for ly in (upper - 1, upper):
            for dx in range(-1, 2):
                for dz in range(-1, 2):
                    ch_set(tile, x + dx, ly, z + dz, LEAVES, ALLOW_AIR)
        for ty in range(y0, upper):
            ch_set(tile, x, ty, z, LOG, ALLOW_AIR_LEAVES)


def stamp_fallen(tile, x, z):
    along_x = (whash_s(x, z, 72) & 1) == 0
    for i in range(FALLEN_LEN):
        tx = x + i if along_x else x
        tz = z if along_x else z + i
        ch_set(tile, tx, top_at(tx, tz) + 1, tz, LOG, ALLOW_ANY)


def stamp_pile(tile, x, z):
    for dx, dz, bid in (
        (0, 0, STONE),
        (1, 0, COBBLE),
        (-1, 0, COBBLE),
        (0, 1, COBBLE),
        (0, -1, STONE),
    ):
        tx, tz = x + dx, z + dz
        ch_set(tile, tx, top_at(tx, tz) + 1, tz, bid, ALLOW_ANY)
    ch_set(tile, x, top_at(x, z) + 2, z, COBBLE, ALLOW_ANY)


def stamp_house(tile, x, z, floor_y):
    for dz in range(5):
        for dx in range(5):
            tx, tz = x + dx, z + dz
            ch_set(tile, tx, floor_y, tz, COBBLE, ALLOW_ANY)
            wall = dx == 0 or dx == 4 or dz == 0 or dz == 4
            for y in range(floor_y + 1, floor_y + 3):
                ch_set(tile, tx, y, tz, PLANK if wall else AIR, ALLOW_ANY)
            ch_set(tile, tx, floor_y + 3, tz, PLANK, ALLOW_ANY)
    ch_set(tile, x + 2, floor_y + 1, z, AIR, ALLOW_ANY)  # doorway
    ch_set(tile, x + 2, floor_y + 2, z, AIR, ALLOW_ANY)
    ch_set(tile, x + 1, floor_y + 1, z + 3, TABLE, ALLOW_ANY)
    ch_set(tile, x + 2, floor_y + 1, z + 3, FURNACE, ALLOW_ANY)
    ch_set(tile, x + 3, floor_y + 1, z + 3, CHEST, ALLOW_ANY)


def stamp_features(tile, trees, fallen, piles, houses):
    ox, oz = g_tileX0, g_tileZ0
    for lx, lz, _ in trees:
        stamp_tree(tile, ox + lx, oz + lz)
    for lx, lz, _ in fallen:
        stamp_fallen(tile, ox + lx, oz + lz)
    for lx, lz, _ in piles:
        stamp_pile(tile, ox + lx, oz + lz)
    for lx, lz, aux in houses:
        stamp_house(tile, ox + lx, oz + lz, aux)


def house_slots(hx, hz, by):
    """Furnace + chest tile-entity slots, both facing the doorway (dir 2)."""
    slots = bytearray(2 * SLOT_SIZE)
    for c in range(2):  # 0 furnace, 1 chest
        bx, bz = hx + 2 + c, hz + 3
        b = c * SLOT_SIZE
        slots[b + 0] = 0x01 | (c << 1)  # in-use | isChest
        slots[b + 1] = 2 | (((bx >> 8) & 3) << 4) | (((bz >> 8) & 3) << 6)
        slots[b + 2] = bx & 0xFF
        slots[b + 3] = by & 0xFF
        slots[b + 4] = bz & 0xFF
    loot = (0xE0, 0x80, 0xD0, 0xB1, 0x01, 0xB0, 0x10, 0xC0)
    slots[SLOT_SIZE + 6] = loot[whash_s(hx, hz, 100) & 7]
    slots[SLOT_SIZE + 7] = 1
    return slots


def generate(chunks, seed, flags, wtype, progress=None):
    """The C generate(): returns the finished .fcw as a bytearray."""
    global g_seed, g_type, g_worldW, g_tileX0, g_tileZ0, g_tileW, g_col, g_col_l

    if chunks < 1 or chunks > MAX_CHUNKS:
        raise ValueError("chunks must be 1..%d" % MAX_CHUNKS)
    if chunks > TILE_CHUNKS and chunks % TILE_CHUNKS:
        raise ValueError(
            "above %d chunks the size must be a multiple of %d"
            % (TILE_CHUNKS, TILE_CHUNKS)
        )
    g_seed = seed & MASK32
    g_type = wtype if wtype <= TYPE_WOODS else TYPE_NORMAL
    g_worldW = chunks * CHUNK

    inv_base = HEADER_SIZE + chunks * chunks * CHUNK_BYTES
    slot0 = inv_base + INV_REGION + PAD_V2
    out = bytearray(slot0 + SLOT_CAP * SLOT_SIZE)

    # Header. Spawn sits on the terrain surface at the world centre.
    sbx = sbz = g_worldW // 2
    g_tileX0 = g_tileZ0 = 0  # compute_columns is tile-independent
    spawn_top = int(compute_columns(np.array([sbx]), np.array([sbz]))[0] & COL_TOP)
    struct.pack_into(
        "<IHHHBBBBI",
        out,
        0,
        MAGIC,
        VERSION,
        chunks,
        chunks,
        CHUNK,
        HEIGHT,
        CHUNK,
        1,
        HEADER_SIZE,
    )
    struct.pack_into(
        "<iii", out, 18, sbx * BLOCKSIZE, (spawn_top + 1) * BLOCKSIZE, sbz * BLOCKSIZE
    )
    out[30] = 0x08
    struct.pack_into("<I", out, 32, g_seed)
    out[36] = flags & 0xFF  # per-world settings (plugin_api.h)
    out[37] = g_type  # terrain preset, generation-time only

    tiles = (chunks + TILE_CHUNKS - 1) // TILE_CHUNKS
    tile_chunks = min(chunks, TILE_CHUNKS)
    g_tileW = tile_chunks * CHUNK

    # The world's single house goes to the tile holding the global maximum of
    # the house score field, biome-checked on an 8-block grid.
    grid = np.arange(0, g_worldW - 4, 8, dtype=np.int32)
    xg, zg = np.meshgrid(grid, grid)
    biome_ok = (compute_columns(xg + 2, zg + 2) & (COL_DESERT | COL_RAVINE)) == 0
    hs = fbm(
        xg.astype(np.float32) * F32(0.08) + F32(19.0),
        zg.astype(np.float32) * F32(0.08) - F32(7.0),
        90,
        3,
    ) + (whash(xg, zg, 91) & U32(255)).astype(np.float32) / F32(900.0)
    hs = np.where(biome_ok, hs, F32(-1e30))
    house_tx = house_tz = 0
    if biome_ok.any():
        flat_idx = int(np.argmax(hs))
        house_tx = int(xg.flat[flat_idx]) // TILE_W
        house_tz = int(zg.flat[flat_idx]) // TILE_W

    stats = dict(trees=0, fallen=0, piles=0, houses=0)
    tile_total = tiles * tiles
    tile_idx = 0
    tile = np.empty((HEIGHT, g_tileW, g_tileW), dtype=np.uint8)
    slots_used = 0

    for tz in range(tiles):
        for tx in range(tiles):
            g_tileX0, g_tileZ0 = tx * TILE_W, tz * TILE_W

            xs = np.arange(g_tileW, dtype=np.int32) + g_tileX0
            zs = np.arange(g_tileW, dtype=np.int32) + g_tileZ0
            xg, zg = np.meshgrid(xs, zs)
            g_col = compute_columns(xg, zg)
            g_col_l = g_col.tolist()
            if progress:
                progress((tile_idx * 100 + 60) // tile_total)

            trees, fallen, piles, houses = place_features(
                tx == house_tx and tz == house_tz
            )
            stats["trees"] += len(trees)
            stats["fallen"] += len(fallen)
            stats["piles"] += len(piles)
            stats["houses"] += len(houses)

            for lx, lz, aux in houses:
                s = house_slots(g_tileX0 + lx, g_tileZ0 + lz, aux + 1)
                off = slot0 + slots_used * SLOT_SIZE
                out[off : off + len(s)] = s
                slots_used += 2

            fill_terrain(tile)
            stamp_features(tile, trees, fallen, piles, houses)

            for lcz in range(tile_chunks):
                for lcx in range(tile_chunks):
                    ccx, ccz = tx * TILE_CHUNKS + lcx, tz * TILE_CHUNKS + lcz
                    payload = tile[
                        :,
                        lcz * CHUNK : (lcz + 1) * CHUNK,
                        lcx * CHUNK : (lcx + 1) * CHUNK,
                    ]
                    off = HEADER_SIZE + (ccz * chunks + ccx) * CHUNK_BYTES
                    out[off : off + CHUNK_BYTES] = payload.tobytes()
            tile_idx += 1
            if progress:
                progress(tile_idx * 100 // tile_total)

    if progress:
        progress(100)
    return out, stats


def world_type(text):
    key = text.strip().lower()
    if key in TYPE_NAMES:
        return TYPE_NAMES[key]
    try:
        value = int(key, 0)
    except ValueError:
        raise argparse.ArgumentTypeError("unknown terrain preset %r" % text)
    if value not in TYPE_NAMES.values():
        raise argparse.ArgumentTypeError("terrain preset out of range: %d" % value)
    return value


def main():
    ap = argparse.ArgumentParser(description="Generate a Flipcraft world asset")
    ap.add_argument(
        "-o",
        "--out",
        default=DEFAULT_WORLD_NAME,
        help="output file name in assets/worlds, or an explicit path",
    )
    ap.add_argument(
        "--chunks",
        type=int,
        default=16,
        help="world size in chunks per side: 16, 32, 64 or 128",
    )
    ap.add_argument(
        "--seed", type=lambda s: int(s, 0), help="world seed; random if omitted"
    )
    ap.add_argument(
        "--flags",
        type=lambda s: int(s, 0),
        default=0,
        help="header settings byte (plugin_api.h FlipcraftFlag*)",
    )
    ap.add_argument(
        "--type",
        type=world_type,
        default=TYPE_NORMAL,
        dest="wtype",
        help="terrain preset: normal, flat, superflat or woods",
    )
    args = ap.parse_args()

    seed = (args.seed if args.seed is not None else secrets.randbits(32)) & MASK32

    def progress(pct):
        print("\r%3d%%" % pct, end="", file=sys.stderr, flush=True)

    data, stats = generate(args.chunks, seed, args.flags, args.wtype, progress)
    print("\r", end="", file=sys.stderr)

    out_path = resolve_output_path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(data)

    name = next(k for k, v in TYPE_NAMES.items() if v == args.wtype)
    print(
        f"Wrote {out_path}: {args.chunks}x{args.chunks} chunks "
        f"({args.chunks * CHUNK} blocks per side), {len(data)} bytes"
    )
    print(
        f"terrain seed={seed} type={name} flags={args.flags} "
        f"trees={stats['trees']} fallen={stats['fallen']} piles={stats['piles']} "
        f"houses={stats['houses']}"
    )


if __name__ == "__main__":
    main()

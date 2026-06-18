#!/usr/bin/env python3
"""
File format:

  Header  : 64 bytes, little-endian
            0  u32  magic   'FCW1' (0x31574346)
            4  u16  version (1)
            6  u16  chunksX
            8  u16  chunksZ
            10 u8   chunkSX (8)
            11 u8   chunkSY (16)
            12 u8   chunkSZ (8)
            13 u8   bytesPerBlock (1)
            14 u32  headerSize (64)
            18 i32  playerX  (pixels, 16 px = 1 block)
            22 i32  playerY
            26 i32  playerZ
            30 u8   rot
            31 u8   reserved
            32 u32  rngState
            36..63 reserved (zero)
  Chunks  : chunksX*chunksZ payloads in row-major order (index = cz*chunksX+cx),
            each 8*16*8 = 1024 bytes, one block id per byte, laid out [y][z][x]:
                payload[(y*8 + z)*8 + x]
            Chunk byte offset = headerSize + (cz*chunksX + cx) * 1024.

Usage:
    python3 tools/worldgen.py
    python3 tools/worldgen.py -o custom.fcw
"""

import argparse
import math
from pathlib import Path
import secrets
import struct

CHUNK = 8
HEIGHT = 16
CHUNK_BLOCKS = CHUNK * HEIGHT * CHUNK
HEADER_SIZE = 64
MAGIC = 0x31574346
VERSION = 1
BLOCKSIZE = 16
FALLEN_TREE_COUNT = 5
FALLEN_TREE_LENGTH = 4

# Block ids (flipcraft.h enum Block)
AIR, GRASS, DIRT, STONE, COBBLE, LOG, LEAVES, PLANK = 0, 1, 2, 3, 4, 5, 6, 7
COALORE, IRONORE, SAND, GLASS, SAPLING, TABLE, FURNACE, CHEST = 8, 9, 10, 11, 12, 13, 14, 15

MASK32 = 0xFFFFFFFF
WORLD_SEED = 0
PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORLD_DIR = PROJECT_ROOT / "assets" / "worlds"
DEFAULT_WORLD_NAME = "word.fcw"


def resolve_output_path(out):
    path = Path(out)
    if path.is_absolute() or path.parent != Path("."):
        return path
    return DEFAULT_WORLD_DIR / path


def whash(x, z, salt):
    salt = (salt + WORLD_SEED * 1013904223) & MASK32
    h = ((x & MASK32) * 374761393 + (z & MASK32) * 668265263 + (salt & MASK32) * 362437) & MASK32
    h = ((h ^ (h >> 13)) * 1274126177) & MASK32
    h ^= h >> 16
    return h & MASK32


def fade(t):
    return t * t * t * (t * (t * 6 - 15) + 10)


def lerp(a, b, t):
    return a + (b - a) * t


def grad(ix, iz, salt):
    h = whash(ix, iz, salt)
    angle = (h & 0xFFFF) * (math.tau / 65536.0)
    return math.cos(angle), math.sin(angle)


def perlin(x, z, salt):
    x0 = math.floor(x)
    z0 = math.floor(z)
    x1 = x0 + 1
    z1 = z0 + 1
    sx = fade(x - x0)
    sz = fade(z - z0)

    g00 = grad(x0, z0, salt)
    g10 = grad(x1, z0, salt)
    g01 = grad(x0, z1, salt)
    g11 = grad(x1, z1, salt)

    n00 = g00[0] * (x - x0) + g00[1] * (z - z0)
    n10 = g10[0] * (x - x1) + g10[1] * (z - z0)
    n01 = g01[0] * (x - x0) + g01[1] * (z - z1)
    n11 = g11[0] * (x - x1) + g11[1] * (z - z1)
    return lerp(lerp(n00, n10, sx), lerp(n01, n11, sx), sz) * 1.41421356237


def fbm(x, z, salt, octaves=4, lacunarity=2.0, gain=0.5):
    total = 0.0
    amp = 1.0
    norm = 0.0
    for i in range(octaves):
        total += perlin(x, z, salt + i * 1013) * amp
        norm += amp
        x *= lacunarity
        z *= lacunarity
        amp *= gain
    return total / norm


def ridged(x, z, salt, octaves=3):
    n = fbm(x, z, salt, octaves=octaves)
    return 1.0 - min(1.0, abs(n) * 2.4)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def smoothstep(edge0, edge1, x):
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def terrain_fields(x, z, wx, wz):
    nx = x / max(1, wx)
    nz = z / max(1, wz)

    warp_x = fbm(nx * 3.0 + 17.0, nz * 3.0 - 11.0, 20, octaves=3) * 18.0
    warp_z = fbm(nx * 3.0 - 7.0, nz * 3.0 + 23.0, 21, octaves=3) * 18.0
    px = (x + warp_x) / max(1, wx)
    pz = (z + warp_z) / max(1, wz)

    continent = fbm(px * 2.2, pz * 2.2, 1, octaves=5)
    detail = fbm(px * 7.5 + 9.0, pz * 7.5 - 4.0, 2, octaves=3) * 0.55
    hill_field = fbm(px * 3.2 - 13.0, pz * 3.2 + 5.0, 3, octaves=4)
    hill_mask = smoothstep(0.04, 0.31, hill_field)
    hill = hill_mask * smoothstep(-0.08, 0.32, continent + detail) * 6.2

    valley = ridged(px * 3.0 + 2.5, pz * 3.0 - 1.5, 4, octaves=4)
    valley_gate = smoothstep(0.04, 0.38, fbm(px * 1.5 + 4.0, pz * 1.5 - 6.0, 5, octaves=3))
    ravine_strength = smoothstep(0.72, 0.92, valley) * valley_gate
    ravine_depth = int(round(ravine_strength * 4.0))

    moisture = fbm(px * 2.4 - 8.0, pz * 2.4 + 19.0, 6, octaves=5)
    temperature = fbm(px * 1.8 + 31.0, pz * 1.8 - 17.0, 7, octaves=4) + (nx - 0.5) * 0.35
    desert_score = temperature * 0.68 - moisture * 0.72 + fbm(px * 5.0, pz * 5.0, 8, octaves=3) * 0.24
    desert = desert_score > 0.18 and ravine_depth < 3

    forest_score = moisture * 0.78 - abs(temperature) * 0.22 + fbm(px * 6.0 + 5.0, pz * 6.0, 9, octaves=3) * 0.18
    forest = forest_score > 0.02 and not desert and ravine_depth == 0

    top = int(round(4.4 + continent * 1.6 + detail + hill - ravine_depth))
    top = clamp(top, 1, 10)
    return {
        "top": top,
        "ravine_depth": ravine_depth,
        "ravine": ravine_depth > 0,
        "desert": desert,
        "forest": forest,
        "hill": hill > 0.35,
        "moisture": moisture,
        "temperature": temperature,
    }


def new_blocks(wx, wz):
    return [[[AIR for _ in range(wx)] for _ in range(wz)] for _ in range(HEIGHT)]


def set_block(blocks, wx, wz, x, y, z, bid):
    if 0 <= x < wx and 0 <= z < wz and 0 <= y < HEIGHT:
        blocks[y][z][x] = bid


def get_block(blocks, wx, wz, x, y, z):
    if 0 <= x < wx and 0 <= z < wz and 0 <= y < HEIGHT:
        return blocks[y][z][x]
    return AIR


def build_height_and_biomes(wx, wz):
    fields = [[terrain_fields(x, z, wx, wz) for x in range(wx)] for z in range(wz)]
    heights = [[fields[z][x]["top"] for x in range(wx)] for z in range(wz)]
    return fields, heights


def fill_terrain(blocks, fields, heights, wx, wz):
    for z in range(wz):
        for x in range(wx):
            f = fields[z][x]
            top = heights[z][x]
            sand_column = f["desert"] or f["ravine"] or top <= 2
            for y in range(top + 1):
                if y == top:
                    bid = SAND if sand_column else GRASS
                elif y >= top - 2:
                    bid = SAND if sand_column else DIRT
                else:
                    r = whash(x * 17 + y, z, 40)
                    bid = COALORE if r % 47 == 0 else IRONORE if r % 67 == 0 else STONE
                set_block(blocks, wx, wz, x, y, z, bid)


def local_slope(heights, wx, wz, x, z):
    h = heights[z][x]
    s = 0
    for dz in (-1, 0, 1):
        for dx in (-1, 0, 1):
            nx, nz = x + dx, z + dz
            if 0 <= nx < wx and 0 <= nz < wz:
                s = max(s, abs(h - heights[nz][nx]))
    return s


def place_tree(blocks, heights, wx, wz, x, z):
    top = heights[z][x]
    if top > HEIGHT - 6:
        return False
    if get_block(blocks, wx, wz, x, top, z) != GRASS:
        return False

    trunk_h = 3 + (whash(x, z, 61) % 2)
    for y in range(top + 1, top + trunk_h + 1):
        set_block(blocks, wx, wz, x, y, z, LOG)

    crown_y = top + trunk_h
    for dz in range(-2, 3):
        for dx in range(-2, 3):
            d = abs(dx) + abs(dz)
            if d <= 3:
                set_block(blocks, wx, wz, x + dx, crown_y, z + dz, LEAVES)
            if d <= 2:
                set_block(blocks, wx, wz, x + dx, crown_y + 1, z + dz, LEAVES)
    set_block(blocks, wx, wz, x, crown_y + 2, z, LEAVES)
    return True


def add_trees(blocks, fields, heights, wx, wz):
    candidates = []
    for z in range(3, wz - 3):
        for x in range(3, wx - 3):
            f = fields[z][x]
            if not f["forest"] or local_slope(heights, wx, wz, x, z) > 1:
                continue
            score = fbm(x * 0.19 + 4.0, z * 0.19 - 9.0, 62, octaves=3) + (whash(x, z, 63) & 255) / 512.0
            if score > 0.23:
                candidates.append((score, x, z))

    placed = []
    for _score, x, z in sorted(candidates, reverse=True):
        if len(placed) >= max(24, (wx * wz) // 190):
            break
        if any((x - px) * (x - px) + (z - pz) * (z - pz) < 36 for px, pz in placed):
            continue
        if place_tree(blocks, heights, wx, wz, x, z):
            placed.append((x, z))
    return placed


def add_fallen_trunks(blocks, fields, heights, wx, wz, count=FALLEN_TREE_COUNT):
    candidates = []
    for z in range(4, wz - 4):
        for x in range(4, wx - 4):
            f = fields[z][x]
            if f["desert"] or f["ravine"] or local_slope(heights, wx, wz, x, z) > 1:
                continue
            score = fbm(x * 0.11, z * 0.11, 70, octaves=3) + (whash(x, z, 71) & 255) / 700.0
            candidates.append((score, x, z))

    placed = []
    for _score, x, z in sorted(candidates, reverse=True):
        if len(placed) >= count:
            break
        along_x = (whash(x, z, 72) & 1) == 0
        coords = [(x + i, z) if along_x else (x, z + i) for i in range(FALLEN_TREE_LENGTH)]
        if any(not (1 <= tx < wx - 1 and 1 <= tz < wz - 1) for tx, tz in coords):
            continue
        if any(fields[tz][tx]["desert"] or fields[tz][tx]["ravine"] for tx, tz in coords):
            continue
        if any((x - px) * (x - px) + (z - pz) * (z - pz) < 225 for px, pz in placed):
            continue
        for tx, tz in coords:
            set_block(blocks, wx, wz, tx, heights[tz][tx] + 1, tz, LOG)
        placed.append((x, z))
    return placed


def add_stone_piles(blocks, fields, heights, wx, wz, count=6):
    candidates = []
    for z in range(3, wz - 3):
        for x in range(3, wx - 3):
            f = fields[z][x]
            if f["desert"] or f["ravine"] or local_slope(heights, wx, wz, x, z) > 1:
                continue
            score = fbm(x * 0.13 + 11.0, z * 0.13 - 3.0, 80, octaves=3)
            candidates.append((score, x, z))

    placed = []
    for _score, x, z in sorted(candidates, reverse=True):
        if len(placed) >= count:
            break
        if any((x - px) * (x - px) + (z - pz) * (z - pz) < 196 for px, pz in placed):
            continue
        pattern = ((0, 0, STONE), (1, 0, COBBLE), (-1, 0, COBBLE), (0, 1, COBBLE), (0, -1, STONE))
        for dx, dz, bid in pattern:
            tx, tz = x + dx, z + dz
            set_block(blocks, wx, wz, tx, heights[tz][tx] + 1, tz, bid)
        set_block(blocks, wx, wz, x, heights[z][x] + 2, z, COBBLE)
        placed.append((x, z))
    return placed


def add_buildings(blocks, fields, heights, wx, wz, count=3):
    candidates = []
    for z in range(5, wz - 8):
        for x in range(5, wx - 8):
            area = [(x + dx, z + dz) for dz in range(5) for dx in range(5)]
            base = heights[z + 2][x + 2]
            if any(abs(heights[tz][tx] - base) > 1 for tx, tz in area):
                continue
            if any(fields[tz][tx]["desert"] or fields[tz][tx]["ravine"] for tx, tz in area):
                continue
            score = fbm(x * 0.08 + 19.0, z * 0.08 - 7.0, 90, octaves=3) + (whash(x, z, 91) & 255) / 900.0
            candidates.append((score, x, z))

    placed = []
    for _score, x, z in sorted(candidates, reverse=True):
        if len(placed) >= count:
            break
        if any((x - px) * (x - px) + (z - pz) * (z - pz) < 625 for px, pz in placed):
            continue
        floor_y = heights[z + 2][x + 2]
        for dz in range(5):
            for dx in range(5):
                tx, tz = x + dx, z + dz
                heights[tz][tx] = floor_y
                set_block(blocks, wx, wz, tx, floor_y, tz, COBBLE)
                wall = dx in (0, 4) or dz in (0, 4)
                for y in range(floor_y + 1, floor_y + 3):
                    set_block(blocks, wx, wz, tx, y, tz, PLANK if wall else AIR)
                set_block(blocks, wx, wz, tx, floor_y + 3, tz, PLANK)
        set_block(blocks, wx, wz, x + 2, floor_y + 1, z, AIR)
        set_block(blocks, wx, wz, x + 2, floor_y + 2, z, AIR)
        placed.append((x, z))
    return placed


def build_world(chunks_x, chunks_z):
    wx = chunks_x * CHUNK
    wz = chunks_z * CHUNK
    fields, heights = build_height_and_biomes(wx, wz)
    blocks = new_blocks(wx, wz)
    fill_terrain(blocks, fields, heights, wx, wz)

    trees = add_trees(blocks, fields, heights, wx, wz)
    fallen = add_fallen_trunks(blocks, fields, heights, wx, wz)
    piles = add_stone_piles(blocks, fields, heights, wx, wz)
    buildings = add_buildings(blocks, fields, heights, wx, wz)
    stats = {
        "trees": len(trees),
        "fallen": len(fallen),
        "piles": len(piles),
        "buildings": len(buildings),
        "desert": sum(1 for z in range(wz) for x in range(wx) if fields[z][x]["desert"]),
        "ravine": sum(1 for z in range(wz) for x in range(wx) if fields[z][x]["ravine"]),
        "hills": sum(1 for z in range(wz) for x in range(wx) if fields[z][x]["hill"]),
        "height_min": min(min(row) for row in heights),
        "height_max": max(max(row) for row in heights),
    }
    return blocks, heights, stats


def chunk_payload(blocks, cx, cz):
    out = bytearray(CHUNK_BLOCKS)
    for y in range(HEIGHT):
        for lz in range(CHUNK):
            for lx in range(CHUNK):
                wx = cx * CHUNK + lx
                wz = cz * CHUNK + lz
                out[(y * CHUNK + lz) * CHUNK + lx] = blocks[y][wz][wx]
    return out


def build_header(chunks_x, chunks_z, heights, seed):
    hdr = bytearray(HEADER_SIZE)
    spawn_bx = chunks_x * CHUNK // 2
    spawn_bz = chunks_z * CHUNK // 2
    spawn_x = spawn_bx * BLOCKSIZE
    spawn_z = spawn_bz * BLOCKSIZE
    spawn_y = (heights[spawn_bz][spawn_bx] + 1) * BLOCKSIZE
    struct.pack_into("<IHHHBBBBI", hdr, 0,
                     MAGIC, VERSION, chunks_x, chunks_z,
                     CHUNK, HEIGHT, CHUNK, 1, HEADER_SIZE)
    struct.pack_into("<iii", hdr, 18, spawn_x, spawn_y, spawn_z)
    hdr[30] = 0x08
    struct.pack_into("<I", hdr, 32, seed & MASK32)
    return hdr


def main():
    global WORLD_SEED
    ap = argparse.ArgumentParser(description="Generate a Flipcraft world asset")
    ap.add_argument("-o", "--out", default=DEFAULT_WORLD_NAME,
                    help="output file name in assets/worlds, or an explicit path")
    ap.add_argument("--chunks-x", type=int, default=16)
    ap.add_argument("--chunks-z", type=int, default=16)
    ap.add_argument("--seed", type=lambda s: int(s, 0), help="world seed; random if omitted")
    args = ap.parse_args()

    WORLD_SEED = (args.seed if args.seed is not None else secrets.randbits(32)) & MASK32
    blocks, heights, stats = build_world(args.chunks_x, args.chunks_z)
    out_path = resolve_output_path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(build_header(args.chunks_x, args.chunks_z, heights, WORLD_SEED))
        for cz in range(args.chunks_z):
            for cx in range(args.chunks_x):
                f.write(chunk_payload(blocks, cx, cz))

    total = HEADER_SIZE + args.chunks_x * args.chunks_z * CHUNK_BLOCKS
    print(f"Wrote {out_path}: {args.chunks_x}x{args.chunks_z} chunks, {total} bytes")
    print(
        "terrain "
        f"seed={WORLD_SEED} "
        f"h={stats['height_min']}..{stats['height_max']} "
        f"desert={stats['desert']} ravine={stats['ravine']} hills={stats['hills']} "
        f"trees={stats['trees']} fallen={stats['fallen']} piles={stats['piles']} buildings={stats['buildings']}"
    )


if __name__ == "__main__":
    main()

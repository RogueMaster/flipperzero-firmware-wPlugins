#!/usr/bin/env python3
"""Build the old hardcoded Flipcraft world as a current .fcw asset.

The source world is the original blockRam.py 8x8x8 layout restored from old
Flipcraft history. Current saves use 8x16x8 chunks, so the old 8 layers are
written as y=0..7 and y=8..15 is left as air.
"""

import argparse
import os
import struct

CHUNK = 8
HEIGHT = 16
OLD_HEIGHT = 8
CHUNKS_X = 1
CHUNKS_Z = 1
CHUNK_BLOCKS = CHUNK * HEIGHT * CHUNK
HEADER_SIZE = 64
MAGIC = 0x31574346
VERSION = 1
BLOCKSIZE = 16
RNG_STATE = 0x1234

AIR, GRASS, DIRT, STONE, COBBLE, LOG, LEAVES, PLANK = 0, 1, 2, 3, 4, 5, 6, 7
COALORE, IRONORE, SAND, GLASS, SAPLING, TABLE, FURNACE, CHEST = 8, 9, 10, 11, 12, 13, 14, 15

A = AIR
G = GRASS
D = DIRT
S = STONE
C = COALORE
I = IRONORE
N = SAND
L = LOG
V = LEAVES

# Stored as [y][z][x] for the current chunk payload. This is the same data that
# old World::loadInitial() passed through setBlock(x, y, z, init[y][7 - z][x]).
OLD_WORLD_8 = (
    ((S, S, S, S, N, N, N, N), (S, S, S, S, S, C, C, S), (S, S, S, S, S, C, C, S), (S, S, S, S, S, C, S, S), (S, S, S, S, S, S, S, S), (S, S, S, S, S, S, S, S), (S, S, S, S, S, S, S, S), (S, S, S, S, S, S, S, S)),
    ((G, G, G, G, N, N, N, N), (G, G, G, G, G, N, N, N), (G, G, D, G, S, N, C, S), (G, G, G, S, C, C, C, S), (G, S, S, S, S, S, S, S), (S, S, S, S, S, S, S, S), (S, S, I, I, S, S, S, S), (S, S, I, S, S, S, S, S)),
    ((A, A, A, A, A, A, A, A), (A, A, A, A, A, N, N, N), (A, A, L, A, G, N, N, N), (A, A, A, G, G, S, S, S), (A, G, G, G, S, S, S, I), (G, G, S, S, S, S, I, I), (S, S, S, I, S, S, S, S), (S, S, I, I, S, S, S, S)),
    ((A, A, A, A, A, A, A, A), (A, A, A, A, A, A, A, A), (A, A, L, A, A, A, N, N), (A, A, A, A, A, G, N, N), (A, A, A, A, G, S, N, I), (A, A, G, G, S, S, I, I), (G, G, S, S, S, S, S, S), (S, S, S, S, S, S, S, S)),
    ((A, V, V, V, A, A, A, A), (V, V, V, V, V, A, A, A), (V, V, L, V, V, A, A, A), (V, V, V, V, V, A, N, N), (A, V, V, V, A, G, N, N), (A, A, A, A, G, G, S, N), (A, A, G, G, G, S, S, N), (G, G, G, S, S, S, S, S)),
    ((A, V, V, V, A, A, A, A), (V, V, V, V, V, A, A, A), (V, V, L, V, V, A, A, A), (V, V, V, V, V, A, A, A), (V, V, V, V, A, A, A, N), (A, A, A, A, A, A, G, N), (A, A, A, A, A, G, G, N), (A, A, A, G, G, G, G, G)),
    ((A, A, A, A, A, A, A, A), (A, A, V, A, A, A, A, A), (A, V, V, A, A, A, A, A), (A, V, V, V, A, A, A, A), (A, A, V, V, A, A, A, A), (A, A, A, A, A, A, A, A), (A, A, A, A, A, A, A, A), (A, A, A, A, A, A, A, A)),
    ((A, A, A, A, A, A, A, A), (A, A, V, A, A, A, A, A), (A, V, V, V, A, A, A, A), (A, A, V, A, A, A, A, A), (A, A, A, A, A, A, A, A), (A, A, A, A, A, A, A, A), (A, A, A, A, A, A, A, A), (A, A, A, A, A, A, A, A)),
)


def surface_y(x, z):
    for y in range(OLD_HEIGHT - 1, -1, -1):
        if OLD_WORLD_8[y][z][x] != AIR:
            return y
    return 0


def build_header():
    hdr = bytearray(HEADER_SIZE)
    spawn_bx = 4
    spawn_bz = 4
    spawn_y = (surface_y(spawn_bx, spawn_bz) + 1) * BLOCKSIZE
    struct.pack_into(
        "<IHHHBBBBI",
        hdr,
        0,
        MAGIC,
        VERSION,
        CHUNKS_X,
        CHUNKS_Z,
        CHUNK,
        HEIGHT,
        CHUNK,
        1,
        HEADER_SIZE,
    )
    struct.pack_into("<iii", hdr, 18, spawn_bx * BLOCKSIZE, spawn_y, spawn_bz * BLOCKSIZE)
    hdr[30] = 0x08
    struct.pack_into("<I", hdr, 32, RNG_STATE)
    return hdr


def chunk_payload(cx, cz):
    out = bytearray(CHUNK_BLOCKS)
    if cx != 0 or cz != 0:
        return out
    for y in range(OLD_HEIGHT):
        for z in range(CHUNK):
            for x in range(CHUNK):
                out[(y * CHUNK + z) * CHUNK + x] = OLD_WORLD_8[y][z][x]
    return out


def write_world(path):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "wb") as f:
        f.write(build_header())
        for cz in range(CHUNKS_Z):
            for cx in range(CHUNKS_X):
                f.write(chunk_payload(cx, cz))


def main():
    parser = argparse.ArgumentParser(description="Generate the old 8x8x8 Flipcraft world in current .fcw format")
    parser.add_argument("-o", "--out", default="assets/worlds/mini.fcw")
    args = parser.parse_args()

    write_world(args.out)
    total = HEADER_SIZE + CHUNKS_X * CHUNKS_Z * CHUNK_BLOCKS
    print(f"Wrote {args.out}: old 8x8x8 world in chunk (0,0), height expanded to {HEIGHT}, {total} bytes")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Pack a z/x/y tree of .mvt tiles into an uncompressed PMTiles v3 archive.

Everything is written with compression = None, because the Flipper firmware has
no gzip: CompressType offers only heatshrink and there is no inflate anywhere in
the tree. The result is still a valid PMTiles file that standard tooling reads.

This writes a root-only archive (no leaf directories). That keeps the on-device
reader simple, at the cost of a large root directory for big tile counts; add
leaves before going statewide.

Usage:
  build_pmtiles.py <tile-dir> <out.pmtiles> [--min-zoom N] [--max-zoom N]
"""

import argparse
import os
import struct
import sys

PMTILES_MAGIC = b"PMTiles"
COMPRESSION_NONE = 1
TILETYPE_MVT = 1


def zxy_to_tileid(z: int, x: int, y: int) -> int:
    """PMTiles orders tiles along a Hilbert curve, per zoom level."""
    acc = ((1 << (z * 2)) - 1) // 3
    n = 1 << z
    d = 0
    tx, ty = x, y
    s = n >> 1
    while s > 0:
        rx = 1 if (tx & s) > 0 else 0
        ry = 1 if (ty & s) > 0 else 0
        d += s * s * ((3 * rx) ^ ry)
        # rotate the quadrant
        if ry == 0:
            if rx == 1:
                tx = s - 1 - tx
                ty = s - 1 - ty
            tx, ty = ty, tx
        s >>= 1
    return acc + d


def write_varint(buf: bytearray, value: int) -> None:
    while value >= 0x80:
        buf.append((value & 0x7F) | 0x80)
        value >>= 7
    buf.append(value)


def serialize_directory(entries) -> bytes:
    """entries: sorted list of (tile_id, offset, length, run_length)."""
    buf = bytearray()
    write_varint(buf, len(entries))

    last = 0
    for tid, _, _, _ in entries:
        write_varint(buf, tid - last)
        last = tid
    for _, _, _, run in entries:
        write_varint(buf, run)
    for _, _, length, _ in entries:
        write_varint(buf, length)

    # offset 0 is a back-reference meaning "immediately after the previous
    # entry"; anything else is stored as offset+1.
    for i, (_, offset, _, _) in enumerate(entries):
        if i > 0 and offset == entries[i - 1][1] + entries[i - 1][2]:
            write_varint(buf, 0)
        else:
            write_varint(buf, offset + 1)
    return bytes(buf)


def collect_tiles(root, min_z, max_z):
    tiles = []
    for zd in sorted(os.listdir(root)):
        if not zd.isdigit():
            continue
        z = int(zd)
        if z < min_z or z > max_z:
            continue
        zp = os.path.join(root, zd)
        for xd in sorted(os.listdir(zp)):
            if not xd.isdigit():
                continue
            x = int(xd)
            xp = os.path.join(zp, xd)
            for fn in sorted(os.listdir(xp)):
                if not fn.endswith(".mvt"):
                    continue
                y = int(fn[:-4])
                tiles.append((z, x, y, os.path.join(xp, fn)))
    return tiles


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tiledir")
    ap.add_argument("out")
    ap.add_argument("--min-zoom", type=int, default=0)
    ap.add_argument("--max-zoom", type=int, default=20)
    args = ap.parse_args()

    tiles = collect_tiles(args.tiledir, args.min_zoom, args.max_zoom)
    if not tiles:
        sys.exit("no .mvt tiles found")

    # Deduplicate identical tile bodies; rural areas repeat a lot of empty ones.
    blobs = {}
    data = bytearray()
    entries = []
    zooms = set()

    for z, x, y, path in tiles:
        with open(path, "rb") as fh:
            body = fh.read()
        if not body:
            continue
        zooms.add(z)
        key = hash(body)
        if key in blobs:
            offset, length = blobs[key]
        else:
            offset, length = len(data), len(body)
            data.extend(body)
            blobs[key] = (offset, length)
        entries.append((zxy_to_tileid(z, x, y), offset, length, 1))

    entries.sort(key=lambda e: e[0])
    root_dir = serialize_directory(entries)

    header_len = 127
    root_off = header_len
    meta = b"{}"
    meta_off = root_off + len(root_dir)
    leaf_off = meta_off + len(meta)
    data_off = leaf_off  # no leaf directories

    h = bytearray(header_len)
    h[0:7] = PMTILES_MAGIC
    h[7] = 3
    struct.pack_into("<Q", h, 8, root_off)
    struct.pack_into("<Q", h, 16, len(root_dir))
    struct.pack_into("<Q", h, 24, meta_off)
    struct.pack_into("<Q", h, 32, len(meta))
    struct.pack_into("<Q", h, 40, leaf_off)
    struct.pack_into("<Q", h, 48, 0)
    struct.pack_into("<Q", h, 56, data_off)
    struct.pack_into("<Q", h, 64, len(data))
    struct.pack_into("<Q", h, 72, len(entries))
    struct.pack_into("<Q", h, 80, len(entries))
    struct.pack_into("<Q", h, 88, len(blobs))
    h[96] = 1  # clustered
    h[97] = COMPRESSION_NONE  # internal
    h[98] = COMPRESSION_NONE  # tile data
    h[99] = TILETYPE_MVT
    h[100] = min(zooms)
    h[101] = max(zooms)

    with open(args.out, "wb") as fh:
        fh.write(bytes(h))
        fh.write(root_dir)
        fh.write(meta)
        fh.write(bytes(data))

    total = header_len + len(root_dir) + len(meta) + len(data)
    print(f"wrote {args.out}")
    print(f"  tiles      : {len(entries)} ({len(blobs)} unique bodies)")
    print(f"  zooms      : {min(zooms)}-{max(zooms)}")
    print(f"  root dir   : {len(root_dir)} bytes at {root_off}")
    print(f"  tile data  : {len(data)} bytes at {data_off}")
    print(f"  total      : {total} bytes")


if __name__ == "__main__":
    main()

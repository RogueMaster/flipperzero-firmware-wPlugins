#!/usr/bin/env python3
"""Fetch a z/x/y tree of .mvt tiles for a bounding box.

Defaults to the VersaTiles OSM endpoint, which serves the Shortbread schema
that lib/carto and zeromesh_mvtlabel.c expect. Tiles arrive gzipped; they are
stored decompressed because the Flipper has no inflate (CompressType offers
only heatshrink), which is also why build_pmtiles.py packs with compression
set to none.

This is someone else's free service. Requests are sequential and rate limited,
already-downloaded tiles are skipped so an interrupted run resumes cheaply, and
a 429 backs off rather than retrying hard. Do not remove those.

Usage:
  fetch_tiles.py <out-dir> --bbox W,S,E,N --min-zoom N --max-zoom N
  fetch_tiles.py map --bbox -72.56,42.69,-70.70,45.31 --min-zoom 10 --max-zoom 12
"""

import argparse
import gzip
import math
import os
import sys
import time
import urllib.error
import urllib.request

DEFAULT_URL = "https://tiles.versatiles.org/tiles/osm/{z}/{x}/{y}"
USER_AGENT = "ZeroMesh-tilefetch/1.0 (Flipper Zero offline maps; +https://github.com/SAMS0N1TE)"


def xtile(lon, z):
    return int((lon + 180.0) / 360.0 * (1 << z))


def ytile(lat, z):
    r = math.radians(lat)
    return int((1.0 - math.asinh(math.tan(r)) / math.pi) / 2.0 * (1 << z))


def fetch_one(url, retries=3):
    """Returns bytes, or None when the tile genuinely does not exist."""
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": USER_AGENT,
            "Accept": "application/x-protobuf, application/vnd.mapbox-vector-tile, */*",
            "Accept-Encoding": "gzip",
        },
    )

    for attempt in range(retries):
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                raw = r.read()
            if len(raw) >= 2 and raw[0] == 0x1F and raw[1] == 0x8B:
                raw = gzip.decompress(raw)
            return raw
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            if e.code == 429:
                wait = 5.0 * (attempt + 1)
                print("    429 rate limited, waiting %.0fs" % wait, flush=True)
                time.sleep(wait)
                continue
            if attempt == retries - 1:
                raise
            time.sleep(2.0 * (attempt + 1))
        except (urllib.error.URLError, TimeoutError, OSError):
            if attempt == retries - 1:
                raise
            time.sleep(2.0 * (attempt + 1))
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir")
    ap.add_argument("--bbox", required=True, help="W,S,E,N in degrees")
    ap.add_argument("--min-zoom", type=int, required=True)
    ap.add_argument("--max-zoom", type=int, required=True)
    ap.add_argument("--url", default=DEFAULT_URL)
    ap.add_argument("--rate", type=float, default=5.0, help="requests per second")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    try:
        w, s, e, n = (float(v) for v in args.bbox.split(","))
    except ValueError:
        sys.exit("--bbox must be W,S,E,N")

    delay = 1.0 / args.rate if args.rate > 0 else 0.0

    work = []
    for z in range(args.min_zoom, args.max_zoom + 1):
        x0, x1 = xtile(w, z), xtile(e, z)
        y0, y1 = ytile(n, z), ytile(s, z)
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                work.append((z, x, y))
        print("z%-3d %d x %d = %d tiles" % (z, x1 - x0 + 1, y1 - y0 + 1,
                                            (x1 - x0 + 1) * (y1 - y0 + 1)))

    print()
    print("%d tiles total, %.1f req/s -> about %.1f minutes"
          % (len(work), args.rate, len(work) * delay / 60.0))
    if args.dry_run:
        return

    got = skipped = empty = failed = 0
    total_bytes = 0
    started = time.time()

    for i, (z, x, y) in enumerate(work):
        path = os.path.join(args.outdir, str(z), str(x), "%d.mvt" % y)
        if os.path.exists(path) and os.path.getsize(path) > 0:
            skipped += 1
            continue

        os.makedirs(os.path.dirname(path), exist_ok=True)
        url = args.url.format(z=z, x=x, y=y)

        try:
            body = fetch_one(url)
        except Exception as ex:
            print("  FAILED z%d/%d/%d: %s" % (z, x, y, ex), flush=True)
            failed += 1
            time.sleep(delay)
            continue

        if not body:
            empty += 1
        else:
            with open(path, "wb") as fh:
                fh.write(body)
            got += 1
            total_bytes += len(body)

        if (i + 1) % 50 == 0:
            rate = (i + 1) / max(1e-9, time.time() - started)
            left = (len(work) - i - 1) / max(1e-9, rate)
            print("  %d/%d  got %d  skip %d  empty %d  fail %d  ~%.1f min left"
                  % (i + 1, len(work), got, skipped, empty, failed, left / 60.0),
                  flush=True)

        time.sleep(delay)

    print()
    print("downloaded %d tiles (%.1f MB), %d already present, %d empty, %d failed"
          % (got, total_bytes / 1048576.0, skipped, empty, failed))
    if failed:
        print("re-run to retry the failures; existing tiles are skipped")


if __name__ == "__main__":
    main()

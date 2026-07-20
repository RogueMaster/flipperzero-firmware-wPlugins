#!/usr/bin/env python3
"""Deploy Hotspot Arcade to a Flipper Zero over USB (serial CLI).

Uploads three things to the SD card, each verified by on-device md5:
  - the built fap             -> /ext/apps/GPIO/hotspot_arcade.fap
  - web bundle (web/dist/*)   -> /ext/apps_data/hotspot_arcade/web/<name>
      (the *.gz files AND manifest.json; the uncompressed index.html is skipped)
  - trivia packs (*.txt)      -> /ext/apps_data/hotspot_arcade/trivia/<name>

The ESP firmware bundle is NOT deployed here: it ships inside the .fap
(fap_file_assets) and the loader extracts it to
/ext/apps_assets/hotspot_arcade/firmware/ on launch, so the on-device flasher
finds it with no SD setup. Build the fap with tools/build-fap.sh to bundle it.

Usage: python3 tools/deploy-to-flipper.py --port /dev/cu.usbmodemflip_XXXX
Requires: pyserial

Only adds files; stale files left on the SD card from earlier sessions are not
removed.
"""
import argparse
import glob
import hashlib
import os
import sys
import time

import serial  # pyserial

PROMPT = b">: "
BLOCK = 4096  # small blocks keep the Flipper's per-write_chunk malloc tiny
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FAP = os.path.join(REPO, "flipper", "hotspot-arcade", "dist", "hotspot_arcade.fap")
WEB_DIST = os.path.join(REPO, "web", "dist")
TRIVIA = os.path.join(REPO, "trivia-packs")

APP_DIR = "/ext/apps_data/hotspot_arcade"


def read_until(s, marker, timeout=8):
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        n = s.in_waiting
        chunk = s.read(n if n else 1)
        if chunk:
            buf += chunk
            if marker in buf:
                return buf
    return buf


def sync(s):
    s.reset_input_buffer()
    s.write(b"\r")
    read_until(s, PROMPT, timeout=4)
    s.reset_input_buffer()


def cmd(s, command, timeout=8):
    s.write(command.encode() + b"\r")
    return read_until(s, PROMPT, timeout=timeout).decode(errors="replace")


def local_md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def upload(s, local, remote):
    cmd(s, f"storage remove {remote}")  # write_chunk appends; start clean
    with open(local, "rb") as f:
        while True:
            block = f.read(BLOCK)
            if not block:
                break
            s.reset_input_buffer()
            s.write(f"storage write_chunk {remote} {len(block)}\r".encode())
            if b"Ready" not in read_until(s, b"Ready", timeout=6):
                raise RuntimeError(f"no Ready for {remote}")
            s.write(block)
            read_until(s, PROMPT, timeout=8)
    out = cmd(s, f"storage md5 {remote}")
    return local_md5(local) in out.lower()


def web_files():
    """web/dist/*.gz plus manifest.json (skip the uncompressed index.html)."""
    files = sorted(glob.glob(os.path.join(WEB_DIST, "*.gz")))
    manifest = os.path.join(WEB_DIST, "manifest.json")
    if os.path.exists(manifest):
        files.append(manifest)
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Flipper serial port")
    args = ap.parse_args()

    if not os.path.exists(FAP):
        sys.exit(
            f"fap not found: {FAP}\n"
            "build it first: cd flipper/hotspot-arcade && ufbt"
        )
    web = web_files()
    if not web:
        sys.exit(
            f"web bundle not found in {WEB_DIST}\n"
            "build it first: cd web && node build.mjs"
        )

    s = serial.Serial(args.port, timeout=3)
    time.sleep(0.2)
    fails = []
    jobs = []
    try:
        sync(s)
        for d in [
            "/ext/apps/GPIO",
            "/ext/apps_data",
            APP_DIR,
            f"{APP_DIR}/web",
            f"{APP_DIR}/trivia",
            f"{APP_DIR}/logs",
        ]:
            cmd(s, f"storage mkdir {d}")

        jobs.append((FAP, "/ext/apps/GPIO/hotspot_arcade.fap"))
        for p in web:
            jobs.append((p, f"{APP_DIR}/web/{os.path.basename(p)}"))
        for p in sorted(glob.glob(os.path.join(TRIVIA, "*.txt"))):
            jobs.append((p, f"{APP_DIR}/trivia/{os.path.basename(p)}"))

        for local, remote in jobs:
            ok = upload(s, local, remote)
            print(f"{'OK  ' if ok else 'FAIL'} {os.path.basename(remote)}")
            if not ok:
                fails.append(remote)
    finally:
        s.close()

    print(f"\n{len(jobs)} files, {len(fails)} failures")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 ReconGrunt
"""
Merge an arduino-cli ESP32 build into ONE image flashable whole at offset 0x0,
then verify it.

WHY THIS EXISTS, TWICE OVER.

1. FlipDeFlock's in-app flasher always writes a single file at 0x0, so a release
   asset has to be a merged image rather than the four separate build outputs.

2. The obvious shortcut -- shipping the core's own `*.merged.bin` -- pads the
   file out to the FULL FLASH SIZE (`--pad-to-size` in the core's platform.txt).
   For the ESP32-C5 that is 4 MB for ~1.4 MB of real firmware. That padding is
   not free: helpers/esp_flasher.c writes the whole file with no skip-blank
   optimisation, so flash time scales with file size, and over the Flipper's
   UART a padded C5 image took 2-3x longer than its useful bytes justify.

OFFSETS ARE NEVER HARDCODED HERE. They come from the `flash_args` file the core
emits next to the build output, which already carries the right values for the
chip that was actually built. This matters: the bootloader lives at 0x1000 on a
classic ESP32, 0x2000 on a C5, and 0x0 on a C3, and a wrong offset does not fail
loudly -- it boot-loops the board with `invalid header`. Reading the core's own
answer means this script cannot drift as chips are added.

REQUIRES ARDUINO CORE 3.x. `flash_args` is a 3.x output -- it does not exist in
2.0.x (checked against 2.0.11's platform.txt, which has neither the flash_args
nor the merged.bin recipe). That is fine for the targets this drives, since the
C5 only exists on 3.x, but it is why the classic-ESP32 release job still merges
with explicit offsets: that job pins 2.0.17 for field-proven output, and would
fail here with a confusing "flash_args not found". If you ever point this script
at a 2.x build, teach it an explicit-offset mode rather than guessing.

Usage:
    python tools/merge_esp_image.py <build_dir> <chip> <output.bin>

Exits non-zero on any inconsistency, so CI fails rather than publishing a
silently broken image.
"""

import subprocess
import sys
from pathlib import Path

ESP_IMAGE_MAGIC = 0xE9

# INDEPENDENT cross-check of the bootloader offset per chip.
#
# The magic-byte check alone is circular: it reads the expected offset from
# flash_args, merges there, then confirms the byte landed there -- which it
# always will. That catches a broken merge, not a wrong offset. These values are
# the second opinion, from the core's boards.txt (`<chip>.build.bootloader_addr`).
#
# A chip absent from this table is NOT an error: unknown chips fall through to
# whatever flash_args says, so adding a new target does not require editing this
# file. It only fails when we hold a specific expectation AND it is contradicted.
#
# Every value below was read out of an installed core's boards.txt, not recalled
# from memory. c6/h2 are deliberately ABSENT rather than guessed -- an unverified
# entry here would be worse than no entry, because it would fail correct builds.
KNOWN_BOOTLOADER_ADDR = {
    "esp32": 0x1000,  # boards.txt 2.0.11 + 3.3.11
    "esp32s2": 0x1000,  # boards.txt 2.0.11
    "esp32s3": 0x0,  # boards.txt 2.0.11
    "esp32c3": 0x0,  # boards.txt 2.0.11 + 3.3.11
    "esp32c5": 0x2000,  # boards.txt 3.3.11 -- NOT the classic 0x1000
}


def read_flash_args(build_dir: Path):
    """Parse the core's flash_args into [(offset:int, path:Path), ...]."""
    fa = build_dir / "flash_args"
    if not fa.exists():
        sys.exit(
            f"ERROR: {fa} not found, so the flash offsets are unknown and this "
            f"script will not guess them.\n"
            f"  The core writes flash_args into the BUILD directory, and "
            f"arduino-cli's --output-dir receives only a subset of the build "
            f"products (flash_args is not among them on Linux). Point this "
            f"script at the path given to --build-path, not --output-dir.\n"
            f"  Also note flash_args is a core 3.x output: it does not exist on "
            f"2.0.x at all."
        )

    chunks = []
    for line in fa.read_text().splitlines():
        line = line.strip()
        # First line is --flash-mode/--flash-freq/--flash-size; skip flags.
        if not line or line.startswith("-"):
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        off_s, name = parts
        try:
            off = int(off_s, 16)
        except ValueError:
            continue
        path = build_dir / name
        if not path.exists():
            sys.exit(
                f"ERROR: flash_args lists {name} at {off_s}, but that file "
                f"is not in {build_dir}."
            )
        chunks.append((off, path))

    if not chunks:
        sys.exit(f"ERROR: no flashable chunks parsed from {fa}.")
    return sorted(chunks)


def verify(out: Path, chunks, chip: str):
    """Assert the merged image really is what we claim before it ships."""
    data = out.read_bytes()
    boot_off, boot_file = chunks[0]

    # Independent cross-check FIRST: does flash_args agree with what we know this
    # chip's bootloader offset to be? The magic-byte test below cannot answer
    # that -- it merges where flash_args says and then confirms the byte is
    # there, which is circular by construction. This is the only check here that
    # can actually catch a wrong offset, i.e. the `invalid header` boot-loop.
    expected = KNOWN_BOOTLOADER_ADDR.get(chip)
    if expected is not None and boot_off != expected:
        sys.exit(
            f"ERROR: flash_args puts the bootloader at {boot_off:#x}, but "
            f"{chip} expects {expected:#x} (per the core's boards.txt). "
            f"Flashing this would boot-loop the board with `invalid "
            f"header`. If {chip} genuinely changed, update "
            f"KNOWN_BOOTLOADER_ADDR deliberately."
        )

    # The bootloader must also actually be present where it was placed -- this
    # catches a broken merge, a corrupt bootloader file, or a truncated image.
    if len(data) <= boot_off:
        sys.exit(
            f"ERROR: merged image is {len(data)} bytes, shorter than the "
            f"bootloader offset {boot_off:#x}."
        )
    if data[boot_off] != ESP_IMAGE_MAGIC:
        sys.exit(
            f"ERROR: expected ESP image magic {ESP_IMAGE_MAGIC:#04x} at "
            f"{boot_off:#x} (per flash_args), found {data[boot_off]:#04x}. "
            f"The image would not boot."
        )

    # Every other chunk should also start with something at its offset.
    for off, path in chunks[1:]:
        if len(data) <= off:
            sys.exit(
                f"ERROR: {path.name} should start at {off:#x} but the image "
                f"is only {len(data)} bytes."
            )

    # Guard the regression this script exists to fix: a merged image should be
    # about the sum of its parts, not padded to the full flash size.
    useful = sum(p.stat().st_size for _, p in chunks)
    if len(data) > useful * 2:
        sys.exit(
            f"ERROR: merged image is {len(data)} bytes for {useful} bytes "
            f"of real content -- it looks padded to the flash size, which "
            f"is what this script exists to avoid."
        )

    print(f"  chip            : {chip}")
    print(f"  bootloader at   : {boot_off:#x}  (magic {data[boot_off]:#04x} OK)")
    for off, path in chunks[1:]:
        print(f"  {path.name:<38} {off:#x}")
    print(f"  useful content  : {useful:,} bytes")
    print(f"  merged image    : {len(data):,} bytes")
    print(f"  padding overhead: {len(data) - useful:,} bytes")


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    build_dir, chip, out = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])

    chunks = read_flash_args(build_dir)
    print(f"Merging {len(chunks)} chunks for {chip} (offsets from flash_args):")

    # esptool's merge_bin spans the lowest offset to the end of the last chunk
    # and fills the gaps with 0xFF. Without --pad-to-size it does NOT extend to
    # the flash size, which is the whole point.
    cmd = [sys.executable, "-m", "esptool", "--chip", chip, "merge_bin", "-o", str(out)]
    for off, path in chunks:
        cmd += [hex(off), str(path)]

    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(res.stdout)
        print(res.stderr, file=sys.stderr)
        sys.exit(f"ERROR: esptool merge_bin failed ({res.returncode}).")

    verify(out, chunks, chip)
    print("RESULT: PASS")


if __name__ == "__main__":
    main()

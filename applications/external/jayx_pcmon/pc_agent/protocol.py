"""JAYX protocol v3 — must match apps/jayx/jayx.h."""

from __future__ import annotations

import struct

MAGIC = 0x4A58
VERSION = 3
MSG_LIVE = 0
MSG_SPECS_SECTION = 1

GAME_NAME_LEN = 20
SPEC_TITLE_LEN = 12
SPEC_LINE_LEN = 36
SPEC_LINES = 3

LIVE_FORMAT = f"<HBBBBHB4sBBHB4sH{GAME_NAME_LEN}s"
LIVE_SIZE = struct.calcsize(LIVE_FORMAT)
assert LIVE_SIZE == 44, LIVE_SIZE

# magic, ver, type, section_index, section_count, title[12], line0..2[36]
SPECS_SECTION_FORMAT = (
    f"<HBBBB{SPEC_TITLE_LEN}s"
    + f"{SPEC_LINE_LEN}s" * SPEC_LINES
)
SPECS_SECTION_SIZE = struct.calcsize(SPECS_SECTION_FORMAT)
assert SPECS_SECTION_SIZE == 126, SPECS_SECTION_SIZE


def _pad_ascii(text: str, length: int) -> bytes:
    raw = (text or "").encode("ascii", errors="replace")[:length]
    return raw.ljust(length, b"\0")


def pack_live(
    *,
    cpu_usage: int,
    cpu_temp_c: int,
    ram_max: int,
    ram_usage: int,
    ram_unit: bytes,
    gpu_usage: int,
    gpu_temp_c: int,
    vram_max: int,
    vram_usage: int,
    vram_unit: bytes,
    fps: int,
    game_name: str,
) -> bytes:
    name = (game_name or "").encode("utf-8", errors="replace")[:GAME_NAME_LEN]
    name = name.ljust(GAME_NAME_LEN, b"\0")
    return struct.pack(
        LIVE_FORMAT,
        MAGIC,
        VERSION,
        MSG_LIVE,
        max(0, min(int(cpu_usage), 100)),
        int(cpu_temp_c) & 0xFF,
        min(int(ram_max), 0xFFFF),
        max(0, min(int(ram_usage), 100)),
        ram_unit[:4].ljust(4, b"\0"),
        int(gpu_usage) & 0xFF,
        int(gpu_temp_c) & 0xFF,
        min(int(vram_max), 0xFFFF),
        int(vram_usage) & 0xFF,
        vram_unit[:4].ljust(4, b"\0"),
        min(int(fps), 0xFFFF),
        name,
    )


def pack_specs_section(
    *,
    section_index: int,
    section_count: int,
    title: str,
    lines: list[str],
) -> bytes:
    while len(lines) < SPEC_LINES:
        lines.append("")
    lines = lines[:SPEC_LINES]
    return struct.pack(
        SPECS_SECTION_FORMAT,
        MAGIC,
        VERSION,
        MSG_SPECS_SECTION,
        section_index & 0xFF,
        section_count & 0xFF,
        _pad_ascii(title, SPEC_TITLE_LEN),
        _pad_ascii(lines[0], SPEC_LINE_LEN),
        _pad_ascii(lines[1], SPEC_LINE_LEN),
        _pad_ascii(lines[2], SPEC_LINE_LEN),
    )


# Back-compat aliases
pack_v1 = pack_live

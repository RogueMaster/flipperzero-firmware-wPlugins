#!/usr/bin/env python3
"""Pack the canonical Terminus 24 source into the FAP storage asset.

Format: magic(4) version(1) glyph_count(1) reserved(2), then 256 fixed 36-byte
full-height packed glyph slots.  Direct character offsets keep the firmware
loader small while redraws remain allocation- and I/O-free.
"""
from __future__ import annotations

import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools" / "terminus24_source.h"
OUTPUT = ROOT / "assets" / "terminus24.bin"
TOKEN = {
    "MORSE_FLIPPER_CW_TOKEN_SK": 0x80,
    "MORSE_FLIPPER_CW_TOKEN_BK": 0x81,
    "MORSE_FLIPPER_CW_TOKEN_CT_KA": 0x82,
    "MORSE_FLIPPER_CW_TOKEN_VE_SN": 0x83,
    "MORSE_FLIPPER_CW_TOKEN_AA": 0x84,
    "MORSE_FLIPPER_CW_TOKEN_SOS": 0x85,
}


def parse(source: Path = SOURCE) -> list[tuple[int, int, int, list[int]]]:
    glyphs = []
    pattern = re.compile(r"^\s*X\((.*)\)\s*\\?\s*$")
    for line in source.read_text().splitlines():
        match = pattern.match(line)
        if not match:
            continue
        content = match.group(1)
        token_match = re.match(r"\s*(MORSE_FLIPPER_CW_TOKEN_[A-Z_]+|'(?:\\.|[^'])')\s*,(.*)$", content)
        if token_match is None:
            raise ValueError(f"invalid glyph entry: {line}")
        token, rest = token_match.groups()
        fields = [token] + [field.strip() for field in rest.split(",")]
        value = TOKEN.get(token)
        if value is None:
            value = ord(ast.literal_eval(fields[0]))
        first, count = map(lambda s: int(s, 0), fields[1:3])
        rows = [int(row, 0) for row in fields[4:]]
        if (
            not 0 <= value <= 0xFF
            or not 0 <= first <= 24
            or not 0 <= count <= 24
            or len(rows) != count
            or first + count > 24
            or any(not 0 <= row <= 0xFFF for row in rows)
        ):
            raise ValueError(f"invalid glyph {fields[0]}")
        glyphs.append((value, first, count, rows))
    if not glyphs or len({item[0] for item in glyphs}) != len(glyphs):
        raise ValueError("missing or duplicate glyphs")
    return glyphs


def pack_rows(first: int, rows: list[int]) -> bytes:
    full = [0] * 24
    full[first : first + len(rows)] = rows
    output = bytearray()
    for left, right in zip(full[::2], full[1::2]):
        output.extend((left >> 4, ((left & 0xF) << 4) | (right >> 8), right & 0xFF))
    return bytes(output)


def pack_active_rows(rows: list[int]) -> bytes:
    output = bytearray()
    padded = rows + ([0] if len(rows) & 1 else [])
    for left, right in zip(padded[::2], padded[1::2]):
        output.extend((left >> 4, ((left & 0xF) << 4) | (right >> 8), right & 0xFF))
    return bytes(output)


def build() -> bytes:
    glyphs = parse()
    header_size = 8
    glyph_size = 36
    fallback = next((pack_rows(first, glyph_rows) for value, first, _, glyph_rows in glyphs if value == ord("#")), None)
    if fallback is None:
        raise ValueError("missing fallback glyph")
    rows = bytearray(fallback * 256)
    for value, first, count, glyph_rows in glyphs:
        offset = value * glyph_size
        rows[offset : offset + glyph_size] = pack_rows(first, glyph_rows)
    for value in range(ord("a"), ord("z") + 1):
        upper = (value - (ord("a") - ord("A"))) * glyph_size
        offset = value * glyph_size
        rows[offset : offset + glyph_size] = rows[upper : upper + glyph_size]
    return b"MF24" + bytes((1, len(glyphs), 0, 0)) + rows


def main() -> int:
    data = build()
    OUTPUT.write_bytes(data)
    print(f"wrote {OUTPUT.relative_to(ROOT)} ({len(data)} bytes, {data[5]} glyphs)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

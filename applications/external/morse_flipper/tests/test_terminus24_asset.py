#!/usr/bin/env python3
"""Focused host checks for the storage-backed Terminus 24 format."""
import hashlib
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import generate_terminus24_asset as font

HEADER_SIZE = 8
GLYPH_SIZE = 36
GLYPH_COUNT = 61
ASSET_SIZE = HEADER_SIZE + 256 * GLYPH_SIZE


def load(data: bytes, requested: list[int]) -> dict[int, bytes]:
    if (
        len(data) != ASSET_SIZE
        or data[:4] != b"MF24"
        or data[4] != 1
        or data[5] != GLYPH_COUNT
        or data[6:8] != b"\0\0"
    ):
        raise ValueError("header")
    found = {}
    for ch in requested:
        offset = HEADER_SIZE + ch * GLYPH_SIZE
        found[ch] = data[offset : offset + GLYPH_SIZE]
    return found


def test_exact_pixels_and_regeneration() -> None:
    data = (ROOT / "assets" / "terminus24.bin").read_bytes()
    assert data == font.build()
    # Locks the pre-asset custom glyph set, including all private CW tokens.
    assert (
        hashlib.sha256(data).hexdigest()
        == "ef2717f609f41aa566b80804c35f9c8081409095f448131f9ae8089aa872db22"
    )
    glyphs = font.parse()
    assert len(glyphs) == GLYPH_COUNT
    packed = load(data, [ch for ch, _, _, _ in glyphs])
    for ch, first, _, rows in glyphs:
        assert packed[ch] == font.pack_rows(first, rows)


def test_header_and_range_corruption_are_rejected() -> None:
    data = bytearray(font.build())
    data[6] = 1
    try:
        load(bytes(data), [ord("A")])
    except ValueError:
        pass
    else:
        raise AssertionError("reserved-header corruption accepted")

    data = bytearray(font.build())
    data.pop()
    try:
        load(bytes(data), [ord("A")])
    except ValueError:
        pass
    else:
        raise AssertionError("truncated asset accepted")


def test_missing_char_and_maximum_working_set() -> None:
    data = font.build()
    # Unsupported and lowercase slots are generated as the original # and uppercase fallbacks.
    assert load(data, [ord("$")])[ord("$")] == load(data, [ord("#")])[ord("#")]
    assert load(data, [ord("a")])[ord("a")] == load(data, [ord("A")])[ord("A")]
    # Five target plus five answer glyphs is the full fixed workspace.
    chars = [ord(ch) for ch in "ABCDE12345"]
    assert list(load(data, chars)) == chars
    assert len(chars) == 10


def test_generator_rejects_missing_and_duplicate_glyphs() -> None:
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "source.h"
        source.write_text("X('A', 0, 1, 0, 0x001)\\\nX('A', 0, 1, 3, 0x001)\\\n")
        try:
            font.parse(source)
        except ValueError:
            pass
        else:
            raise AssertionError("duplicate source glyph accepted")
        source.write_text("")
        try:
            font.parse(source)
        except ValueError:
            pass
        else:
            raise AssertionError("empty source accepted")


if __name__ == "__main__":
    test_exact_pixels_and_regeneration()
    test_header_and_range_corruption_are_rejected()
    test_missing_char_and_maximum_working_set()
    test_generator_rejects_missing_and_duplicate_glyphs()
    print("terminus24 asset tests: ok")

#!/usr/bin/env python3
"""Build or deterministically convert MFVA v1 runtime voice packs."""

from __future__ import annotations

import argparse
import os
import struct
import tempfile
import zlib
from pathlib import Path

TOKENS = tuple("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") + ("stroke", "period", "comma", "question-mark")
CODECS = {
    "s16_16k": (0, 16000),
    "s16_8k": (0, 8000),
    "u8_16k": (1, 16000),
    "u8_8k": (1, 8000),
    "mulaw_8k": (2, 8000),
    "ima_adpcm_8k": (3, 8000),
}
HEADER = struct.Struct("<4sBBHIIIIII")
ENTRY = struct.Struct("<B3xIIIhBB")


def parse_pcm16_16k_pack(blob: bytes) -> tuple[list[tuple[int, bytes, int]], int]:
    """Validate a production PCM16 MFVA and return its ordered payloads."""
    if len(blob) < HEADER.size:
        raise ValueError("MFVA header is truncated")
    magic, version, codec, count, rate, table_offset, data_offset, file_size, table_crc, data_crc = HEADER.unpack_from(blob)
    if magic != b"MFVA" or version != 1 or codec != 0 or rate != 16000 or count != len(TOKENS):
        raise ValueError("MFVA format is not the selected PCM16/16 kHz pack")
    table_size = ENTRY.size * count
    table_end = table_offset + table_size
    if file_size != len(blob) or table_offset < HEADER.size or table_end > data_offset or data_offset > file_size:
        raise ValueError("MFVA offsets are invalid")
    table = blob[table_offset:table_end]
    data = blob[data_offset:file_size]
    if zlib.crc32(table) & 0xFFFFFFFF != table_crc:
        raise ValueError("MFVA table CRC is invalid")
    if zlib.crc32(data) & 0xFFFFFFFF != data_crc:
        raise ValueError("MFVA data CRC is invalid")
    tokens: list[tuple[int, bytes, int]] = []
    seen: set[int] = set()
    ranges: list[tuple[int, int]] = []
    for index in range(count):
        token_id, offset, length, samples, predictor, ima_index, reserved = ENTRY.unpack_from(table, index * ENTRY.size)
        del predictor, ima_index, reserved
        end = offset + length
        if token_id >= count or token_id in seen or length == 0 or samples == 0 or length != samples * 2:
            raise ValueError("MFVA token metadata is invalid")
        if offset < data_offset or end < offset or end > file_size:
            raise ValueError("MFVA token bounds are invalid")
        seen.add(token_id)
        ranges.append((offset, end))
        tokens.append((token_id, blob[offset:end], samples))
    if seen != set(range(count)):
        raise ValueError("MFVA token IDs are incomplete")
    for index, (start, end) in enumerate(sorted(ranges)):
        if index and start < sorted(ranges)[index - 1][1]:
            raise ValueError("MFVA token payloads overlap")
        if end <= start:
            raise ValueError("MFVA token payload is empty")
    return sorted(tokens), data_offset


def pcm16_to_u8(payload: bytes) -> bytes:
    if len(payload) & 1:
        raise ValueError("PCM16 payload has an odd byte length")
    converted = bytearray(len(payload) // 2)
    for index, (sample,) in enumerate(struct.iter_unpack("<h", payload)):
        converted[index] = max(0, min(255, (sample + 32768) >> 8))
    return bytes(converted)


def convert_pcm16_16k_to_u8(source: Path, output: Path) -> None:
    """Atomically replace output with the deterministic U8/16 kHz conversion."""
    source_blob = source.read_bytes()
    tokens, _ = parse_pcm16_16k_pack(source_blob)
    payloads = [(token_id, pcm16_to_u8(payload), samples) for token_id, payload, samples in tokens]
    table_offset = HEADER.size
    data_offset = table_offset + ENTRY.size * len(payloads)
    table = bytearray()
    data = bytearray()
    for token_id, payload, samples in payloads:
        table += ENTRY.pack(token_id, data_offset + len(data), len(payload), samples, 0, 0, 0)
        data += payload
    header = HEADER.pack(
        b"MFVA", 1, CODECS["u8_16k"][0], len(payloads), 16000, table_offset, data_offset,
        data_offset + len(data), zlib.crc32(table) & 0xFFFFFFFF, zlib.crc32(data) & 0xFFFFFFFF)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, prefix=f".{output.name}.", delete=False) as temporary:
        temporary.write(header + table + data)
        temporary.flush()
        os.fsync(temporary.fileno())
        temporary_name = temporary.name
    try:
        os.replace(temporary_name, output)
    finally:
        if os.path.exists(temporary_name):
            os.unlink(temporary_name)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, help="directory containing token-id .bin payloads")
    parser.add_argument("--variant", choices=CODECS)
    parser.add_argument("--from-mfva", type=Path, help="selected PCM16/16 kHz MFVA input to convert")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=Path, help="JSON-like line file: token-id sample-count")
    args = parser.parse_args()
    if args.from_mfva is not None:
        if args.input is not None or args.variant is not None or args.samples is not None:
            parser.error("--from-mfva cannot be combined with payload build options")
        convert_pcm16_16k_to_u8(args.from_mfva, args.output)
        return 0
    if args.input is None or args.variant is None or args.samples is None:
        parser.error("--input, --variant, and --samples are required for a payload build")
    codec, rate = CODECS[args.variant]
    logical = {}
    for line in args.samples.read_text(encoding="ascii").splitlines():
        token, count = line.split()
        logical[token] = int(count)
    payloads = []
    for token_id, token in enumerate(TOKENS):
        path = args.input / f"{token}.bin"
        data = path.read_bytes()
        if not data or token not in logical or logical[token] <= 0:
            raise ValueError(f"invalid token {token}")
        samples = logical[token]
        expected = samples * 2 if codec == 0 else (samples + 1) // 2 if codec == 3 else samples
        if len(data) != expected:
            raise ValueError(f"encoded length mismatch for {token}: {len(data)} != {expected}")
        payloads.append((token_id, data, samples))
    table_offset = HEADER.size
    data_offset = table_offset + ENTRY.size * len(payloads)
    table = bytearray()
    data = bytearray()
    for token_id, payload, samples in payloads:
        table += ENTRY.pack(token_id, data_offset + len(data), len(payload), samples, 0, 0, 0)
        data += payload
    header = HEADER.pack(
        b"MFVA", 1, codec, len(payloads), rate, table_offset, data_offset,
        data_offset + len(data), zlib.crc32(table) & 0xFFFFFFFF,
        zlib.crc32(data) & 0xFFFFFFFF)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(header + table + data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

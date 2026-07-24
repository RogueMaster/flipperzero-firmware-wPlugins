#!/usr/bin/env python3
"""Build the deterministic MFVA v1 runtime voice pack from encoded token files."""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

TOKENS = tuple("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") + ("stroke", "period", "comma", "question-mark")
CODECS = {"s16_16k": (0, 16000), "s16_8k": (0, 8000), "u8_8k": (1, 8000), "mulaw_8k": (2, 8000), "ima_adpcm_8k": (3, 8000)}
HEADER = struct.Struct("<4sBBHIIIIII")
ENTRY = struct.Struct("<B3xIIIhBB")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True, help="directory containing token-id .bin payloads")
    parser.add_argument("--variant", choices=CODECS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=Path, required=True, help="JSON-like line file: token-id sample-count")
    args = parser.parse_args()
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
        payloads.append((token_id, data, logical[token]))
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

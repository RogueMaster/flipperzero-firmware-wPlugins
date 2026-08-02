#!/usr/bin/env python3
"""Host checks for deterministic production MFVA PCM16-to-U8 conversion."""

import importlib.util
import os
import struct
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/mfva/build_passive_voice_pack.py"
ASSET = ROOT / "assets/audio/voice_en_gb_amy_v1.mfa"
SOURCE_ROOT = ROOT / "tools/mfva/voice_en_gb_amy_v1"
SOURCE_PCM16 = SOURCE_ROOT / "voice_en_gb_amy_v1_s16_16k.mfa"
SOURCE_WAVS = SOURCE_ROOT / "wav"
SPEC = importlib.util.spec_from_file_location("passive_voice_pack", TOOL)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def expect_invalid(blob: bytes) -> None:
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "source.mfa"
        output = Path(directory) / "output.mfa"
        source.write_bytes(blob)
        try:
            MODULE.convert_pcm16_16k_to_u8(source, output)
        except ValueError:
            return
        raise AssertionError("corrupt MFVA conversion unexpectedly succeeded")


def make_pcm16_pack() -> bytes:
    payloads = []
    for token_id in range(40):
        payload = struct.pack("<hhhh", -32768, -1, token_id, 32767)
        payloads.append((token_id, payload, 4))
    table_offset = MODULE.HEADER.size
    data_offset = table_offset + MODULE.ENTRY.size * len(payloads)
    table = bytearray()
    data = bytearray()
    for token_id, payload, samples in payloads:
        table += MODULE.ENTRY.pack(
            token_id, data_offset + len(data), len(payload), samples, 0, 0, 0
        )
        data += payload
    return (
        MODULE.HEADER.pack(
            b"MFVA",
            1,
            0,
            len(payloads),
            16000,
            table_offset,
            data_offset,
            data_offset + len(data),
            MODULE.zlib.crc32(table) & 0xFFFFFFFF,
            MODULE.zlib.crc32(data) & 0xFFFFFFFF,
        )
        + table
        + data
    )


def assert_production_u8_pack(blob: bytes) -> None:
    (
        magic,
        version,
        codec,
        count,
        rate,
        table_offset,
        data_offset,
        file_size,
        table_crc,
        data_crc,
    ) = MODULE.HEADER.unpack_from(blob)
    assert (magic, version, codec, count, rate) == (b"MFVA", 1, 1, 40, 16000)
    assert file_size == len(blob) and table_offset == MODULE.HEADER.size
    assert table_offset + MODULE.ENTRY.size * count == data_offset <= file_size
    table = blob[table_offset:data_offset]
    data = blob[data_offset:file_size]
    assert MODULE.zlib.crc32(table) & 0xFFFFFFFF == table_crc
    assert MODULE.zlib.crc32(data) & 0xFFFFFFFF == data_crc
    seen = set()
    ranges = []
    for index in range(count):
        token_id, offset, length, samples, predictor, ima_index, reserved = (
            MODULE.ENTRY.unpack_from(table, index * MODULE.ENTRY.size)
        )
        assert predictor == ima_index == reserved == 0
        assert token_id not in seen and token_id < count
        assert length == samples and samples > 0
        assert data_offset <= offset < offset + length <= file_size
        seen.add(token_id)
        ranges.append((offset, offset + length))
    assert seen == set(range(count))
    assert all(
        left[1] <= right[0] for left, right in zip(sorted(ranges), sorted(ranges)[1:])
    )


def main() -> None:
    source = SOURCE_PCM16.read_bytes()
    tokens, _ = MODULE.parse_pcm16_16k_pack(source)
    assert len(tokens) == 40
    assert_production_u8_pack(ASSET.read_bytes())
    assert MODULE.pcm16_to_u8(struct.pack("<hhhh", -32768, -1, 0, 32767)) == bytes(
        (0, 127, 128, 255)
    )
    with tempfile.TemporaryDirectory() as directory:
        first = Path(directory) / "first.mfa"
        second = Path(directory) / "second.mfa"
        source_path = Path(directory) / "selected-pcm16.mfa"
        rebuilt_source = Path(directory) / "rebuilt-source.mfa"
        source_path.write_bytes(source)
        MODULE.write_pack(
            MODULE.read_pcm16_wavs(SOURCE_WAVS),
            *MODULE.CODECS["s16_16k"],
            rebuilt_source
        )
        assert rebuilt_source.read_bytes() == source
        MODULE.convert_pcm16_16k_to_u8(source_path, first)
        MODULE.convert_pcm16_16k_to_u8(source_path, second)
        assert first.read_bytes() == second.read_bytes()
        assert os.stat(first).st_mode & 0o777 == 0o644
        assert first.read_bytes() == ASSET.read_bytes()
        assert_production_u8_pack(first.read_bytes())
    corrupt = bytearray(make_pcm16_pack())
    corrupt[0] ^= 1
    expect_invalid(bytes(corrupt))
    corrupt = bytearray(source)
    corrupt[24] ^= 1
    expect_invalid(bytes(corrupt))
    corrupt = bytearray(source)
    corrupt[MODULE.HEADER.size + 4 : MODULE.HEADER.size + 8] = (0).to_bytes(4, "little")
    expect_invalid(bytes(corrupt))
    corrupt = bytearray(source)
    corrupt[MODULE.HEADER.size + 12 : MODULE.HEADER.size + 16] = (1).to_bytes(
        4, "little"
    )
    expect_invalid(bytes(corrupt))
    print("test_passive_voice_pack_tool: passed")


if __name__ == "__main__":
    main()

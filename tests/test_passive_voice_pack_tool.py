#!/usr/bin/env python3
"""Host checks for deterministic production MFVA PCM16-to-U8 conversion."""

import importlib.util
import subprocess
import struct
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/audio_assets/build_passive_voice_pack.py"
ASSET = ROOT / "assets/audio/voice_en_gb_amy_v1.mfa"
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


def main() -> None:
    source = subprocess.check_output(
        ["git", "show", "5adbc88:assets/audio/voice_en_gb_amy_v1.mfa"], cwd=ROOT)
    tokens, _ = MODULE.parse_pcm16_16k_pack(source)
    assert len(tokens) == 40
    assert MODULE.pcm16_to_u8(struct.pack("<hhhh", -32768, -1, 0, 32767)) == bytes((0, 127, 128, 255))
    with tempfile.TemporaryDirectory() as directory:
        first = Path(directory) / "first.mfa"
        second = Path(directory) / "second.mfa"
        source_path = Path(directory) / "selected-pcm16.mfa"
        source_path.write_bytes(source)
        MODULE.convert_pcm16_16k_to_u8(source_path, first)
        MODULE.convert_pcm16_16k_to_u8(source_path, second)
        assert first.read_bytes() == second.read_bytes()
        assert first.read_bytes() == ASSET.read_bytes()
        magic, version, codec, count, rate, table_offset, data_offset, file_size, table_crc, data_crc = MODULE.HEADER.unpack_from(first.read_bytes())
        assert (magic, version, codec, count, rate) == (b"MFVA", 1, 1, 40, 16000)
        assert file_size == first.stat().st_size and table_offset == MODULE.HEADER.size
        table = first.read_bytes()[table_offset:data_offset]
        data = first.read_bytes()[data_offset:file_size]
        assert MODULE.zlib.crc32(table) & 0xFFFFFFFF == table_crc
        assert MODULE.zlib.crc32(data) & 0xFFFFFFFF == data_crc
    corrupt = bytearray(source)
    corrupt[0] ^= 1
    expect_invalid(bytes(corrupt))
    corrupt = bytearray(source)
    corrupt[24] ^= 1
    expect_invalid(bytes(corrupt))
    corrupt = bytearray(source)
    corrupt[MODULE.HEADER.size + 4:MODULE.HEADER.size + 8] = (0).to_bytes(4, "little")
    expect_invalid(bytes(corrupt))
    corrupt = bytearray(source)
    corrupt[MODULE.HEADER.size + 12:MODULE.HEADER.size + 16] = (1).to_bytes(4, "little")
    expect_invalid(bytes(corrupt))
    print("test_passive_voice_pack_tool: passed")


if __name__ == "__main__":
    main()

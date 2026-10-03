#!/usr/bin/env python3
"""Tests for the packaged FAP/FAL consistency gate."""

from __future__ import annotations

import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "check_fap_bundle", ROOT / "tools" / "check_fap_bundle.py"
)
assert SPEC and SPEC.loader
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


def asset_archive(files: dict[str, bytes]) -> bytes:
    signature = b"test-signature"
    output = bytearray(
        struct.pack(
            "<IIIII",
            CHECK.ASSET_MAGIC,
            CHECK.ASSET_VERSION,
            1,
            len(files),
            len(signature),
        )
    )
    output.extend(signature)
    directory = b"plugins\0"
    output.extend(struct.pack("<I", len(directory)))
    output.extend(directory)
    for name, contents in files.items():
        encoded = name.encode() + b"\0"
        output.extend(struct.pack("<I", len(encoded)))
        output.extend(encoded)
        output.extend(struct.pack("<I", len(contents)))
        output.extend(contents)
    return bytes(output)


def elf_with_assets(assets: bytes) -> bytes:
    section_names = b"\0.shstrtab\0.fapassets\0"
    header_size = 52
    names_offset = header_size
    assets_offset = names_offset + len(section_names)
    section_offset = (assets_offset + len(assets) + 3) & ~3
    section_size = 40
    output = bytearray(section_offset + 3 * section_size)
    output[:16] = b"\x7fELF\x01\x01\x01" + b"\0" * 9
    struct.pack_into(
        "<HHIIIIIHHHHHH",
        output,
        16,
        1,
        40,
        1,
        0,
        0,
        section_offset,
        0,
        header_size,
        0,
        0,
        section_size,
        3,
        1,
    )
    output[names_offset:assets_offset] = section_names
    output[assets_offset : assets_offset + len(assets)] = assets
    struct.pack_into(
        "<IIIIIIIIII",
        output,
        section_offset + section_size,
        1,
        3,
        0,
        0,
        names_offset,
        len(section_names),
        0,
        0,
        1,
        0,
    )
    struct.pack_into(
        "<IIIIIIIIII",
        output,
        section_offset + 2 * section_size,
        11,
        1,
        0,
        0,
        assets_offset,
        len(assets),
        0,
        0,
        1,
        0,
    )
    return bytes(output)


class BundleCheckTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.plugin_dir = self.root / "plugins"
        self.plugin_dir.mkdir()
        self.plugin_data = {
            name: f"current:{name}".encode() for name in CHECK.REQUIRED_FALS
        }
        for name, contents in self.plugin_data.items():
            (self.plugin_dir / name).write_bytes(contents)
        files = {
            f"plugins/{name}": contents for name, contents in self.plugin_data.items()
        }
        self.fap_data = elf_with_assets(asset_archive(files))
        self.dist_fap = self.root / "dist.fap"
        self.build_fap = self.root / "build.fap"
        self.dist_fap.write_bytes(self.fap_data)
        self.build_fap.write_bytes(self.fap_data)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_matching_bundle_passes(self) -> None:
        result = CHECK.verify_bundle(self.dist_fap, self.build_fap, self.plugin_dir)
        self.assertEqual(tuple(result), CHECK.REQUIRED_FALS)

    def test_stale_distributable_fails(self) -> None:
        self.build_fap.write_bytes(self.fap_data + b"new build")
        with self.assertRaisesRegex(CHECK.BundleError, "stale"):
            CHECK.verify_bundle(self.dist_fap, self.build_fap, self.plugin_dir)

    def test_missing_embedded_plugin_fails(self) -> None:
        files = {
            f"plugins/{name}": contents
            for name, contents in self.plugin_data.items()
            if name != CHECK.REQUIRED_FALS[-1]
        }
        data = elf_with_assets(asset_archive(files))
        self.dist_fap.write_bytes(data)
        self.build_fap.write_bytes(data)
        with self.assertRaisesRegex(CHECK.BundleError, "is missing"):
            CHECK.verify_bundle(self.dist_fap, self.build_fap, self.plugin_dir)

    def test_mismatched_embedded_plugin_fails(self) -> None:
        (self.plugin_dir / CHECK.REQUIRED_FALS[0]).write_bytes(b"different")
        with self.assertRaisesRegex(CHECK.BundleError, "does not match"):
            CHECK.verify_bundle(self.dist_fap, self.build_fap, self.plugin_dir)


if __name__ == "__main__":
    unittest.main()

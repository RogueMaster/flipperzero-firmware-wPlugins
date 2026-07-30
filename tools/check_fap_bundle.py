#!/usr/bin/env python3
"""Verify that the distributable FAP contains the current embedded FALs."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path


ASSET_MAGIC = 0x4F4C5A44
ASSET_VERSION = 1
REQUIRED_FALS = (
    "morse_flipper_help_about.fal",
    "morse_flipper_icr.fal",
    "morse_flipper_passive_listening.fal",
    "morse_flipper_passive_settings.fal",
    "morse_flipper_radio.fal",
    "morse_flipper_rx_practice.fal",
    "morse_flipper_settings.fal",
    "morse_flipper_tx_groups.fal",
)


class BundleError(ValueError):
    """Raised when a FAP or its embedded asset archive is inconsistent."""


def _range(data: bytes, offset: int, size: int, label: str) -> bytes:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise BundleError(f"{label} is outside the file")
    return data[offset : offset + size]


def read_elf_section(path: Path, section_name: str) -> bytes:
    data = path.read_bytes()
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise BundleError(f"{path}: not an ELF file")
    if data[4] != 1 or data[5] != 1:
        raise BundleError(f"{path}: expected a 32-bit little-endian ELF")

    header = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    section_offset = header[5]
    section_entry_size = header[10]
    section_count = header[11]
    string_table_index = header[12]
    expected_entry_size = struct.calcsize("<IIIIIIIIII")
    if section_entry_size < expected_entry_size:
        raise BundleError(f"{path}: invalid section-header size")
    if not section_count or string_table_index >= section_count:
        raise BundleError(f"{path}: invalid section-header table")

    def section(index: int) -> tuple[int, ...]:
        offset = section_offset + index * section_entry_size
        raw = _range(data, offset, expected_entry_size, "section header")
        return struct.unpack("<IIIIIIIIII", raw)

    string_section = section(string_table_index)
    names = _range(data, string_section[4], string_section[5], "section-name table")
    for index in range(section_count):
        entry = section(index)
        name_offset = entry[0]
        if name_offset >= len(names):
            raise BundleError(f"{path}: invalid section name")
        name_end = names.find(b"\0", name_offset)
        if name_end < 0:
            raise BundleError(f"{path}: unterminated section name")
        name = names[name_offset:name_end].decode("ascii", errors="strict")
        if name == section_name:
            return _range(data, entry[4], entry[5], section_name)
    raise BundleError(f"{path}: missing {section_name} section")


def read_asset_archive(data: bytes) -> dict[str, bytes]:
    header_size = struct.calcsize("<IIIII")
    if len(data) < header_size:
        raise BundleError("asset archive is truncated")
    magic, version, directory_count, file_count, signature_size = struct.unpack_from(
        "<IIIII", data
    )
    if magic != ASSET_MAGIC:
        raise BundleError("asset archive has the wrong magic")
    if version != ASSET_VERSION:
        raise BundleError(f"unsupported asset archive version {version}")

    cursor = header_size

    def take(size: int, label: str) -> bytes:
        nonlocal cursor
        value = _range(data, cursor, size, label)
        cursor += size
        return value

    def read_u32(label: str) -> int:
        return struct.unpack("<I", take(4, label))[0]

    def read_name(label: str) -> str:
        size = read_u32(f"{label} length")
        raw = take(size, label)
        if not raw or raw[-1] != 0:
            raise BundleError(f"{label} is not null-terminated")
        try:
            return raw[:-1].decode("utf-8")
        except UnicodeDecodeError as error:
            raise BundleError(f"{label} is not valid UTF-8") from error

    take(signature_size, "asset signature")
    for index in range(directory_count):
        read_name(f"directory {index}")

    files: dict[str, bytes] = {}
    for index in range(file_count):
        name = read_name(f"file {index} name")
        size = read_u32(f"file {index} size")
        if name in files:
            raise BundleError(f"duplicate asset path: {name}")
        files[name] = take(size, f"file {name}")
    if cursor != len(data):
        raise BundleError(f"asset archive has {len(data) - cursor} trailing bytes")
    return files


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_bundle(dist_fap: Path, build_fap: Path, plugin_dir: Path) -> list[str]:
    if not dist_fap.is_file():
        raise BundleError(f"missing distributable FAP: {dist_fap}")
    if not build_fap.is_file():
        raise BundleError(f"missing current build FAP: {build_fap}")

    dist_data = dist_fap.read_bytes()
    build_data = build_fap.read_bytes()
    if dist_data != build_data:
        raise BundleError(
            f"{dist_fap} is stale or from another build; run ufbt faps after "
            "the final source change"
        )

    assets = read_asset_archive(read_elf_section(dist_fap, ".fapassets"))
    verified: list[str] = []
    for name in REQUIRED_FALS:
        asset_name = f"plugins/{name}"
        embedded = assets.get(asset_name)
        if embedded is None:
            raise BundleError(f"packaged FAP is missing {asset_name}")
        built_path = plugin_dir / name
        if not built_path.is_file():
            raise BundleError(f"missing current plugin build: {built_path}")
        built = built_path.read_bytes()
        if embedded != built:
            raise BundleError(
                f"{asset_name} does not match {built_path} "
                f"(embedded {digest(embedded)[:12]}, built {digest(built)[:12]})"
            )
        verified.append(name)
    return verified


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path("/env/ufbt/build"),
        help="uFBT build directory",
    )
    parser.add_argument(
        "--dist-fap",
        type=Path,
        default=Path("dist/morse_flipper.fap"),
        help="FAP that will be installed or distributed",
    )
    args = parser.parse_args()
    try:
        verified = verify_bundle(
            args.dist_fap,
            args.build_dir / "morse_flipper.fap",
            args.build_dir / "morse_flipper" / "assets" / "plugins",
        )
    except (BundleError, OSError) as error:
        print(f"FAIL bundle: {error}")
        return 1

    print(f"PASS bundle: distributable matches current build ({len(verified)} embedded FALs)")
    for name in verified:
        print(f"  PASS plugins/{name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

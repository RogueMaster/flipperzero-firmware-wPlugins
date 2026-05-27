"""Symbol and release-FAP launchability gates.

This script has two related jobs: validate that imported symbols are available
in the pinned Flipper SDK API surface, and validate/produce release FAP files.
"""

from __future__ import annotations

import argparse
import csv
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


REQUIRED_SYMBOLS = {
    "Function": {
        "furi_hal_usb_set_config",
        "furi_hal_usb_get_config",
        "furi_hal_hid_u2f_set_callback",
        "furi_hal_hid_u2f_get_request",
        "furi_hal_hid_u2f_send_response",
        "furi_hal_crypto_enclave_ensure_key",
        "furi_hal_crypto_enclave_load_key",
        "furi_hal_crypto_enclave_unload_key",
    },
    "Variable": {
        "usb_hid_u2f",
    },
}

FAP_ALLOWED_EXPORTS = {"zerofido_main"}
FAP_MAX_DEFINED_GLOBALS = 1
FAP_MAX_SYMTAB_BYTES = 64
FAP_MAX_STRTAB_BYTES = 64
FAP_DEFAULT_OUTPUT = Path("dist/zerofido-release.fap")


@dataclass(frozen=True)
class ElfSection:
    name: str
    section_type: str
    size: int


@dataclass(frozen=True)
class ElfSymbol:
    name: str
    symbol_type: str
    bind: str
    visibility: str
    section_index: str


def load_symbols(api_symbols: Path) -> dict[str, set[str]]:
    found: dict[str, set[str]] = {"Function": set(), "Variable": set()}
    with api_symbols.open(newline="") as handle:
        reader = csv.reader(handle)
        for row in reader:
            if len(row) < 3:
                continue
            kind, exported, name = row[:3]
            if exported != "+":
                continue
            if kind in found:
                found[kind].add(name)
    return found


def check_sdk_symbol_gate(root: Path) -> int:
    api_symbols = root / "targets" / "f7" / "api_symbols.csv"
    if not api_symbols.exists():
        print(f"missing {api_symbols}")
        return 2

    found = load_symbols(api_symbols)
    missing: list[str] = []
    for kind, names in REQUIRED_SYMBOLS.items():
        for name in sorted(names):
            if name not in found[kind]:
                missing.append(f"{kind}: {name}")

    if missing:
        print("symbol gate failed")
        for item in missing:
            print(f"  - {item}")
        return 1

    print("symbol gate passed")
    print(f"checked {api_symbols}")
    return 0


def _tool_from_env(env_name: str) -> str | None:
    configured = os.environ.get(env_name)
    if configured and Path(configured).exists():
        return configured
    return None


def _find_tool(tool: str, env_name: str) -> str:
    configured = _tool_from_env(env_name)
    if configured:
        return configured

    found = shutil.which(tool)
    if found:
        return found

    ufbt_root = Path.home() / ".ufbt" / "toolchain"
    for candidate in sorted(ufbt_root.glob(f"*/bin/{tool}")):
        if candidate.exists():
            return str(candidate)

    raise FileNotFoundError(f"could not find {tool}; set {env_name}")


def _run_tool(args: list[str]) -> str:
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout


def parse_readelf_sections(output: str) -> list[ElfSection]:
    sections: list[ElfSection] = []
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line.startswith("["):
            continue
        parts = line.split()
        if len(parts) < 7 or not parts[0].startswith("["):
            continue

        name = parts[2] if parts[1].endswith("]") else parts[1]
        section_type = parts[3] if parts[1].endswith("]") else parts[2]
        size_index = 6 if parts[1].endswith("]") else 5
        try:
            size = int(parts[size_index], 16)
        except (IndexError, ValueError):
            continue
        sections.append(ElfSection(name=name, section_type=section_type, size=size))
    return sections


def parse_readelf_section_indices(output: str) -> dict[int, str]:
    sections: dict[int, str] = {}
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line.startswith("["):
            continue
        parts = line.split()
        if len(parts) < 3:
            continue
        try:
            if parts[0] == "[":
                index = int(parts[1].rstrip("]"))
                name = parts[2]
            else:
                index = int(parts[0].lstrip("[").rstrip("]"))
                name = parts[1]
        except ValueError:
            continue
        sections[index] = name
    return sections


def parse_readelf_symbols(output: str) -> list[ElfSymbol]:
    symbols: list[ElfSymbol] = []
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line or not line[0].isdigit():
            continue
        parts = line.split(maxsplit=7)
        if len(parts) < 8 or not parts[0].endswith(":"):
            continue
        symbols.append(
            ElfSymbol(
                name=parts[7],
                symbol_type=parts[3],
                bind=parts[4],
                visibility=parts[5],
                section_index=parts[6],
            )
        )
    return symbols


def read_elf_sections(readelf: str, fap_path: Path) -> list[ElfSection]:
    return parse_readelf_sections(_run_tool([readelf, "-S", "--wide", str(fap_path)]))


def read_elf_section_indices(readelf: str, fap_path: Path) -> dict[int, str]:
    return parse_readelf_section_indices(_run_tool([readelf, "-S", "--wide", str(fap_path)]))


def read_elf_symbols(readelf: str, fap_path: Path) -> list[ElfSymbol]:
    return parse_readelf_symbols(_run_tool([readelf, "-s", "--wide", str(fap_path)]))


def fap_budget_violations(sections: list[ElfSection], symbols: list[ElfSymbol]) -> list[str]:
    violations: list[str] = []
    section_by_name = {section.name: section for section in sections}
    defined_globals = sorted(
        symbol.name for symbol in symbols if symbol.bind == "GLOBAL" and symbol.section_index != "UND"
    )
    unexpected_exports = sorted(set(defined_globals) - FAP_ALLOWED_EXPORTS)
    missing_exports = sorted(FAP_ALLOWED_EXPORTS - set(defined_globals))
    standard_relocations = sorted(
        section.name
        for section in sections
        if section.name.startswith(".rel.") or section.name.startswith(".rela.")
    )
    fast_relocations = sorted(section.name for section in sections if section.name.startswith(".fast.rel."))

    if unexpected_exports:
        violations.append(
            "unexpected globally defined FAP symbols: " + ", ".join(unexpected_exports[:12])
        )
    if missing_exports:
        violations.append("missing required FAP export(s): " + ", ".join(missing_exports))
    if len(defined_globals) > FAP_MAX_DEFINED_GLOBALS:
        violations.append(
            f"defined global symbol budget exceeded: {len(defined_globals)} > "
            f"{FAP_MAX_DEFINED_GLOBALS}"
        )
    if standard_relocations:
        violations.append(
            "standard relocation sections remain after fastfap: "
            + ", ".join(standard_relocations)
        )
    if not fast_relocations:
        violations.append("fast relocation sections missing from optimized release FAP")

    symtab_size = section_by_name.get(".symtab", ElfSection(".symtab", "SYMTAB", 0)).size
    strtab_size = section_by_name.get(".strtab", ElfSection(".strtab", "STRTAB", 0)).size
    if symtab_size > FAP_MAX_SYMTAB_BYTES:
        violations.append(f".symtab budget exceeded: {symtab_size} > {FAP_MAX_SYMTAB_BYTES}")
    if strtab_size > FAP_MAX_STRTAB_BYTES:
        violations.append(f".strtab budget exceeded: {strtab_size} > {FAP_MAX_STRTAB_BYTES}")

    return violations


def _remap_fast_relocation_data(data: bytes, old_to_new_section: dict[int, int]) -> bytes:
    remapped = bytearray(data)
    if len(remapped) < 5:
        raise RuntimeError("fast relocation section is truncated")

    offset = 0
    version = remapped[offset]
    offset += 1
    if version != 1:
        raise RuntimeError(f"unsupported fast relocation version {version}")

    group_count = struct.unpack_from("<I", remapped, offset)[0]
    offset += 4
    for _ in range(group_count):
        if offset + 5 > len(remapped):
            raise RuntimeError("fast relocation group is truncated")
        flags = remapped[offset]
        offset += 1
        if flags & 0x80:
            old_section = struct.unpack_from("<I", remapped, offset)[0]
            try:
                new_section = old_to_new_section[old_section]
            except KeyError as exc:
                raise RuntimeError(
                    f"fast relocation references removed section index {old_section}"
                ) from exc
            struct.pack_into("<I", remapped, offset, new_section)
            offset += 8
        else:
            offset += 4

        if offset + 4 > len(remapped):
            raise RuntimeError("fast relocation offset list is truncated")
        relocation_count = struct.unpack_from("<I", remapped, offset)[0]
        offset += 4 + relocation_count * 3

    if offset != len(remapped):
        raise RuntimeError("fast relocation section has trailing bytes")
    return bytes(remapped)


def _remap_fast_relocation_sections(
    objcopy: str,
    readelf: str,
    fap_path: Path,
    original_section_indices: dict[int, str],
) -> None:
    final_section_indices = read_elf_section_indices(readelf, fap_path)
    final_index_by_name = {name: index for index, name in final_section_indices.items()}
    old_to_new_section = {
        old_index: final_index_by_name[name]
        for old_index, name in original_section_indices.items()
        if name in final_index_by_name
    }
    fast_relocations = [
        section.name
        for section in read_elf_sections(readelf, fap_path)
        if section.name.startswith(".fast.rel.")
    ]

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        for section_name in fast_relocations:
            dumped = temp_path / f"{section_name.removeprefix('.')}.bin"
            subprocess.run(
                [objcopy, f"--dump-section={section_name}={dumped}", str(fap_path)],
                check=True,
            )
            dumped.write_bytes(
                _remap_fast_relocation_data(dumped.read_bytes(), old_to_new_section)
            )
            subprocess.run(
                [objcopy, f"--update-section={section_name}={dumped}", str(fap_path)],
                check=True,
            )


def optimize_fap_exports(objcopy: str, readelf: str, fap_path: Path) -> None:
    sections = read_elf_sections(readelf, fap_path)
    original_section_indices = read_elf_section_indices(readelf, fap_path)
    has_fast_relocations = any(section.name.startswith(".fast.rel.") for section in sections)
    has_standard_relocations = any(
        section.name.startswith(".rel.") or section.name.startswith(".rela.") for section in sections
    )

    if has_standard_relocations and not has_fast_relocations:
        raise RuntimeError("refusing to strip relocations before fastfap has produced .fast.rel.*")
    if has_fast_relocations and not has_standard_relocations:
        raise RuntimeError("refusing to package fast-only FAP without standard relocations")

    # Fast relocation payloads encode section indexes produced before post-build
    # stripping. Remap them after removing standard relocations so the compact
    # fast-only FAP still points at the final section table.
    subprocess.run(
        [
            objcopy,
            "--strip-all",
            "--keep-symbol=zerofido_main",
            "--remove-section=.gnu_debuglink",
            "--remove-section=.rel.text",
            "--remove-section=.rel.rodata",
            "--remove-section=.rel.data",
            str(fap_path),
        ],
        check=True,
    )
    _remap_fast_relocation_sections(objcopy, readelf, fap_path, original_section_indices)


def package_optimized_fap(objcopy: str, readelf: str, source_path: Path, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if source_path.resolve() != output_path.resolve():
        shutil.copy2(source_path, output_path)
    optimize_fap_exports(objcopy, readelf, output_path)


def check_fap_symbol_budget(fap_path: Path, *, fix: bool, output_fap: Path | None = None) -> int:
    if not fap_path.exists():
        print(f"missing {fap_path}")
        return 2

    checked_path = fap_path
    original_size = fap_path.stat().st_size

    try:
        readelf = _find_tool("arm-none-eabi-readelf", "ARM_NONE_EABI_READELF")
        objcopy = _find_tool("arm-none-eabi-objcopy", "ARM_NONE_EABI_OBJCOPY")
        if output_fap:
            package_optimized_fap(objcopy, readelf, fap_path, output_fap)
            checked_path = output_fap
        elif fix:
            optimize_fap_exports(objcopy, readelf, fap_path)
        sections = read_elf_sections(readelf, checked_path)
        symbols = read_elf_symbols(readelf, checked_path)
    except (FileNotFoundError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"FAP symbol budget failed: {exc}")
        return 2

    violations = fap_budget_violations(sections, symbols)
    if violations:
        print("FAP symbol budget failed")
        for violation in violations:
            print(f"  - {violation}")
        if not fix:
            print(
                "run again with --output-fap "
                f"{FAP_DEFAULT_OUTPUT} to package an optimized release copy"
            )
        return 1

    checked_size = checked_path.stat().st_size
    print("FAP symbol budget passed")
    print(f"checked {checked_path}")
    if checked_path != fap_path:
        saved = original_size - checked_size
        print(f"packaged from {fap_path}")
        print(f"size: {original_size} -> {checked_size} bytes ({saved} bytes saved)")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check SDK imports and FAP export budgets")
    parser.add_argument(
        "legacy_sdk_root",
        nargs="?",
        help="legacy form: flipperzero-firmware root for SDK symbol availability checks",
    )
    parser.add_argument("--sdk-root", type=Path, help="flipperzero-firmware root")
    parser.add_argument("--fap", type=Path, help="built .fap to check")
    parser.add_argument(
        "--fix-fap",
        action="store_true",
        help="strip/localize the input FAP in place so only zerofido_main remains exported",
    )
    parser.add_argument(
        "--output-fap",
        type=Path,
        help=(
            "write an optimized release FAP copy at this path; leaves --fap input unchanged "
            f"(default recommendation: {FAP_DEFAULT_OUTPUT})"
        ),
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    statuses: list[int] = []

    sdk_root = args.sdk_root
    if args.legacy_sdk_root and not args.fap and not args.sdk_root:
        sdk_root = Path(args.legacy_sdk_root)
    elif args.legacy_sdk_root:
        print("positional SDK root cannot be combined with --sdk-root or --fap")
        return 2

    if sdk_root:
        statuses.append(check_sdk_symbol_gate(sdk_root))
    if args.fap:
        if args.fix_fap and args.output_fap:
            print("--fix-fap cannot be combined with --output-fap")
            return 2
        statuses.append(
            check_fap_symbol_budget(args.fap, fix=args.fix_fap, output_fap=args.output_fap)
        )
    if not statuses:
        print("usage: check_symbol_gate.py <flipperzero-firmware-root>")
        print("   or: check_symbol_gate.py --fap dist/zerofido.fap")
        print(f"   or: check_symbol_gate.py --fap dist/zerofido.fap --output-fap {FAP_DEFAULT_OUTPUT}")
        return 2

    return max(statuses)


if __name__ == "__main__":
    raise SystemExit(main())

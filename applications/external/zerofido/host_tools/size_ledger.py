"""Report ZeroFIDO firmware section sizes and largest symbols.

This helper keeps footprint checks repeatable while protocol helpers are being
extracted. It is intentionally read-only: build/package artifacts first, then
run this script to capture the current ledger output.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ARTIFACTS = (
    Path("dist/zerofido.fap"),
    Path("dist/zerofido-release.fap"),
    Path("dist/zerofido-usb-diagnostics.fap"),
    Path("dist/zerofido-nfc-diagnostics.fap"),
    Path("dist/zerofido-full-diagnostics.fap"),
)
DEFAULT_DEBUG_ELF = Path("dist/debug/zerofido_d.elf")


def find_tool(name: str) -> str:
    found = shutil.which(name)
    if found:
        return found

    for candidate in sorted((Path.home() / ".ufbt" / "toolchain").glob(f"*/bin/{name}")):
        if candidate.exists():
            return str(candidate)

    raise SystemExit(f"missing {name}; install UFBT or put the ARM toolchain on PATH")


def run_tool(args: list[str]) -> str:
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout


def parse_text_size(size_output: str) -> int | None:
    for line in size_output.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] == ".text":
            return int(parts[1])
    return None


def report_sections(size_tool: str, artifacts: list[Path]) -> None:
    print("# ZeroFIDO Size Ledger")
    print()
    print("| Artifact | .text bytes |")
    print("| --- | ---: |")
    for artifact in artifacts:
        path = artifact if artifact.is_absolute() else ROOT / artifact
        if not path.exists():
            print(f"| `{artifact}` | missing |")
            continue
        output = run_tool([size_tool, "-A", str(path)])
        text_size = parse_text_size(output)
        text = str(text_size) if text_size is not None else "unknown"
        print(f"| `{artifact}` | {text} |")
    print()


def report_top_symbols(nm_tool: str, debug_elf: Path, limit: int) -> None:
    path = debug_elf if debug_elf.is_absolute() else ROOT / debug_elf
    if not path.exists():
        print(f"`{debug_elf}` missing; skipping symbol report.")
        return

    output = run_tool([nm_tool, "--print-size", "--size-sort", "--radix=d", str(path)])
    symbols = [line for line in output.splitlines() if line.strip()]
    print(f"## Largest Symbols: `{debug_elf}`")
    print()
    print("```")
    for line in symbols[-limit:]:
        print(line)
    print("```")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Report ZeroFIDO firmware footprint.")
    parser.add_argument(
        "--artifact",
        action="append",
        type=Path,
        dest="artifacts",
        help="artifact to measure; may be passed more than once",
    )
    parser.add_argument("--debug-elf", type=Path, default=DEFAULT_DEBUG_ELF)
    parser.add_argument("--top-symbols", type=int, default=40)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    size_tool = find_tool("arm-none-eabi-size")
    nm_tool = find_tool("arm-none-eabi-nm")
    artifacts = args.artifacts if args.artifacts else list(DEFAULT_ARTIFACTS)

    report_sections(size_tool, artifacts)
    report_top_symbols(nm_tool, args.debug_elf, args.top_symbols)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

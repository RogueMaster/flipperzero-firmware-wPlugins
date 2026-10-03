#!/usr/bin/env python3
"""Build and verify the Morse Flipper distributable FAP."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def default_build_dir() -> Path:
    configured = os.environ.get("UFBT_BUILD_DIR")
    if configured:
        return Path(configured).expanduser()
    env_build = Path("/env/ufbt/build")
    if env_build.is_dir():
        return env_build
    return Path.home() / ".ufbt" / "build"


def resolve_ufbt(configured: str | None) -> str:
    candidate = configured or os.environ.get("UFBT") or shutil.which("ufbt")
    if not candidate:
        raise SystemExit(
            "ufbt executable not found; activate the uFBT environment or pass "
            "--ufbt /path/to/ufbt"
        )
    return candidate


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--ufbt",
        help="uFBT executable (or set UFBT); defaults to ufbt on PATH",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=default_build_dir(),
        help="uFBT build directory (or set UFBT_BUILD_DIR)",
    )
    args = parser.parse_args()
    ufbt = resolve_ufbt(args.ufbt)

    run([ufbt, "faps"])
    run(
        [
            sys.executable,
            str(ROOT / "tools" / "check_fap_memory.py"),
            "--build-dir",
            str(args.build_dir),
        ]
    )
    run(
        [
            sys.executable,
            str(ROOT / "tools" / "check_fap_bundle.py"),
            "--build-dir",
            str(args.build_dir),
            "--dist-fap",
            str(ROOT / "dist" / "morse_flipper.fap"),
        ]
    )
    print(f"Verified distributable: {ROOT / 'dist' / 'morse_flipper.fap'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Compile a .dfc with the same feature limits as the Flipper app."""

from pathlib import Path
import subprocess
import sys
import tempfile

from probe_dfcb import build_tool


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} input.dfc output.dfcb", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory(prefix="dfc-compile-") as tmp:
        executable = Path(tmp) / "compile_dfcb"
        build_tool(Path(__file__).with_suffix(".c"), executable)
        return subprocess.run([str(executable), sys.argv[1], sys.argv[2]]).returncode


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run APDUs against a .dfcb with the Flipper app's core configuration."""

import ast
import os
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "lib/core"
CRYPTO = ROOT / "lib/tiny-crypto-c"
DEFAULT_APDUS = ("9060000000", "90AF000000", "90AF000000", "906A000000")


def flipper_defines():
    manifest = ast.parse((ROOT / "application.fam").read_text())
    app = next(node.value for node in manifest.body if isinstance(node, ast.Expr))
    definitions = next(
        keyword.value for keyword in app.keywords if keyword.arg == "cdefines"
    )
    if isinstance(definitions, ast.Name):
        definitions = next(
            node.value
            for node in manifest.body
            if isinstance(node, ast.Assign)
            and any(
                isinstance(target, ast.Name) and target.id == definitions.id
                for target in node.targets
            )
        )
    return [
        f"-D{name}={value}"
        for name, value in ast.literal_eval(definitions)
        if name != "DFC_FIRMWARE_BUILD"
    ]


def build_tool(source, executable):
    command = [
        os.environ.get("CC", "cc"),
        "-std=c11",
        "-O2",
        f"-I{CORE / 'src'}",
        f"-I{CORE / 'port'}",
        f"-I{CRYPTO / 'src'}",
        *flipper_defines(),
        str(source),
        *(str(path) for path in sorted((CORE / "src").glob("*.c"))),
        str(CORE / "port/host/dfc_port_host.c"),
        str(CORE / "port/host/dfc_bytebuf_host.c"),
        *(str(CRYPTO / "src" / name) for name in ("aes.c", "des.c", "common.c")),
        "-o",
        str(executable),
    ]
    subprocess.run(command, check=True, cwd=ROOT)


def main():
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} credential.dfcb [hex-apdu ...]", file=sys.stderr)
        return 2

    credential = Path(sys.argv[1])
    if not credential.is_file():
        print(f"credential not found: {credential}", file=sys.stderr)
        return 2
    if credential.stat().st_size > 16 * 1024:
        print("credential exceeds the Flipper's 16 KiB file limit", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="dfc-probe-") as tmp:
        executable = Path(tmp) / "dfc_probe_apdu"
        build_tool(CORE / "tests/dfc_probe_apdu.c", executable)
        result = subprocess.run(
            [str(executable), str(credential), *(sys.argv[2:] or DEFAULT_APDUS)],
            cwd=ROOT,
        )
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())

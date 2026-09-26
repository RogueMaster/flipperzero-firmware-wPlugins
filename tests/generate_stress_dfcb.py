#!/usr/bin/env python3
"""Generate a small ladder of credentials for on-device memory testing."""

from pathlib import Path
import subprocess
import sys
import tempfile

from probe_dfcb import ROOT, build_tool, flipper_defines


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} output-directory", file=sys.stderr)
        return 2
    output = Path(sys.argv[1]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    definitions = dict(definition[2:].split("=", 1) for definition in flipper_defines())
    pool_size = int(definitions["DFC_FILE_POOL_SIZE"])
    max_apps = int(definitions["DFC_MAX_APPS"])
    has_8k = definitions["DFC_ENABLE_STORAGE_8K"] == "1"
    cases = [
        (1, 0),
        (1, 512),
        (1, 1024),
        (1, 2048),
        (1, 3072),
        (1, 4096),
        (4, 3072),
        (4, 4096),
        (8, 4096),
    ]
    if has_8k and pool_size >= 8192:
        cases.extend(((1, 8192), (8, 8192)))
    with tempfile.TemporaryDirectory(prefix="dfc-stress-") as tmp:
        executable = Path(tmp) / "make_stress_dfcb"
        build_tool(ROOT / "tests/make_stress_dfcb.c", executable)
        for apps, payload in cases:
            if apps > max_apps or payload > pool_size:
                continue
            path = output / f"stress-a{apps}-d{payload}.dfcb"
            subprocess.run([str(executable), str(path), str(apps), str(payload), "3"], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

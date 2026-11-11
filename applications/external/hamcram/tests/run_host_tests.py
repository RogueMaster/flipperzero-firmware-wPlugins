#!/usr/bin/env python3
"""Build and run Ham Cram's host-side tests.

The app's own C sources (core/ and views/quiz_view.c) are compiled for the
host against a small stand-in for the Flipper SDK (tests/host/shim), then:

  1. every pool is dumped through the C reader and compared, field by field,
     with what tools/hampool.py says the pool JSON should contain;
  2. the C test suite runs (exams, study, review, saved progress, scrolling).

A C compiler is found via $CC, then gcc/clang/cc on PATH, then the `ziglang`
pip package (`pip install ziglang`), which is the easy option on Windows.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import hampool  # noqa: E402

SOURCES = [
    "tests/host/test_main.c",
    "tests/host/shim/shim.c",
    "core/pool.c",
    "core/progress.c",
    "core/session.c",
]
CFLAGS = ["-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Itests/host/shim"]


def find_compiler() -> list[str]:
    if os.environ.get("CC"):
        return os.environ["CC"].split()
    for name in ("gcc", "clang", "cc"):
        if shutil.which(name):
            return [name]
    try:
        import ziglang  # noqa: F401

        return [sys.executable, "-m", "ziglang", "cc"]
    except ImportError:
        sys.exit("No C compiler found. Install gcc or clang, or run `pip install ziglang`.")


def expected_dump(pool: dict) -> list[str]:
    """The pool as the app should see it, in host_tests' dump format."""
    lines = [
        "\t".join(
            str(v)
            for v in (
                "POOL",
                pool["name"],
                pool["years"],
                pool["element"],
                pool["exam_questions"],
                pool["pass_correct"],
                hampool.date_int(pool["valid_from"]),
                hampool.date_int(pool["valid_to"]),
                pool["question_count"],
            )
        )
    ]
    for sub in pool["subelements"]:
        count = sum(len(g["questions"]) for g in sub["groups"])
        title = hampool.to_ascii(sub["title"], sub["id"]).decode()
        lines.append(f"SUB\t{sub['id']}\t{title}\t{len(sub['groups'])}\t{count}")
    for sub in pool["subelements"]:
        for group in sub["groups"]:
            for q in group["questions"]:
                text = [hampool.to_ascii(t, q["id"]).decode() for t in (q["ref"], q["question"], *q["answers"])]
                flags = (hampool.FLAG_FIGURE if "figure" in q else 0) | (
                    hampool.FLAG_PIN_LAST if hampool.RE_PIN_LAST.match(text[5]) else 0
                )
                correct = hampool.ANSWER_LETTERS.index(q["correct"])
                lines.append("\t".join(["Q", q["id"], str(correct), str(flags), group["id"], *text]))
    return lines


def main() -> int:
    compiler = find_compiler()
    with tempfile.TemporaryDirectory(prefix="hamcram-tests-") as tmp:
        exe = Path(tmp) / ("host_tests.exe" if os.name == "nt" else "host_tests")
        cmd = [*compiler, *CFLAGS, *SOURCES, "-o", str(exe)]
        if os.environ.get("HAMCRAM_SANITIZE"):
            cmd[len(compiler) : len(compiler)] = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
        print("build:", " ".join(compiler), "...")
        subprocess.run(cmd, cwd=ROOT, check=True)

        failed = False
        for src in hampool.load_sources():
            stem = src["stem"]
            pool = json.loads((ROOT / "pools" / f"{stem}.json").read_text(encoding="utf-8"))
            result = subprocess.run(
                [str(exe), "dump", str(ROOT / "files" / "pools" / f"{stem}.hcp")],
                capture_output=True,
                text=True,
                encoding="ascii",
            )
            got = result.stdout.splitlines()
            want = expected_dump(pool)
            if result.returncode != 0 or got != want:
                failed = True
                print(f"dump {stem}: MISMATCH {result.stderr.strip()}")
                for a, b in zip(got, want, strict=False):
                    if a != b:
                        print(f"  app reads: {a}\n  expected:  {b}")
                        break
                else:
                    print(f"  app read {len(got)} lines, expected {len(want)}")
            else:
                print(f"dump {stem}: {len(got) - 1 - len(pool['subelements'])} questions match the JSON")

        result = subprocess.run([str(exe), "test", str(ROOT / "files" / "pools"), tmp])
        failed = failed or result.returncode != 0

    print("FAILED" if failed else "OK")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

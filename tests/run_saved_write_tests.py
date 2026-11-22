#!/usr/bin/env python3
"""Exercise actual Saved Write app functions and write encoding on a mock reader.

Run: python3 tests/run_saved_write_tests.py
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "uhf_expansion.c").read_text()


def function(name):
    match = re.search(r"^static [^;\n]*\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    if not match:
        raise AssertionError(f"Missing application function: {name}")
    end = source.index("\n}", match.end()) + 2
    return source[match.start():end]


names = [
    "uhf_write_selected_bank_at", "uhf_write_selected_bank",
    "uhf_finish_saved_write", "uhf_saved_write_failure", "uhf_saved_write_value",
    "uhf_start_saved_write", "uhf_capture_saved_write_report",
    "uhf_service_saved_write_target", "uhf_draw_saved_tags", "uhf_handle_saved_tags",
]
harness = (ROOT / "tests/saved_write_harness.c.inc").read_text()
assert harness.count("/* APPLICATION_SAVED_WRITE_BLOCK */") == 1
combined = harness.replace("/* APPLICATION_SAVED_WRITE_BLOCK */", "\n\n".join(map(function, names)))
with tempfile.TemporaryDirectory(prefix="uhf-saved-write-test-") as work:
    path = Path(work)
    (path / "saved_write_test.c").write_text(combined)
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-Wno-unused-function", "-Wno-unused-parameter", "-fsanitize=address,undefined", "-g",
        "-I", str(ROOT), str(path / "saved_write_test.c"), "-o", str(path / "saved_write_test"),
    ], check=True)
    subprocess.run([str(path / "saved_write_test")], check=True)

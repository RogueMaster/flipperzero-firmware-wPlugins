#!/usr/bin/env python3
"""Run the actual settings state machine against a mocked reader and storage.

The application is a single Flipper translation unit; extract its settings block
into a host harness rather than reimplementing its state transitions in tests.
Run: python3 tests/run_settings_state_tests.py
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "uhf_expansion.c").read_text()
start = source.index("static UhfSettingsSnapshot uhf_settings_snapshot(")
end = source.index("static bool uhf_handle_home_tools(", start)
block = source[start:end]
harness = (ROOT / "tests/settings_state_harness.c.inc").read_text()
assert harness.count("/* APPLICATION_SETTINGS_BLOCK */") == 1
combined = harness.replace("/* APPLICATION_SETTINGS_BLOCK */", block)
with tempfile.TemporaryDirectory(prefix="uhf-settings-test-") as work:
    path = Path(work)
    (path / "settings_test.c").write_text(combined)
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-Wno-unused-function", "-fsanitize=address,undefined", "-g",
        "-I", str(ROOT), str(path / "settings_test.c"),
        str(ROOT / "uhf_frequency.c"), "-o", str(path / "settings_test"),
    ], check=True)
    subprocess.run([str(path / "settings_test")], check=True)

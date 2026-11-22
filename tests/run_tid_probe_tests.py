#!/usr/bin/env python3
"""Exercise the application's TID probe with a scripted reader under ASan/UBSan.

Run: python3 tests/run_tid_probe_tests.py
The app functions and custom RX handler are extracted, never reimplemented.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "uhf_expansion.c").read_text()


def braced_block(start):
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def function(name):
    match = re.search(r"^static [^;{}]*?\b" + name + r"\s*\(", source, re.MULTILINE)
    assert match, f"Missing application function: {name}"
    return braced_block(match.start())


names = [
    "uhf_force_clear_epc_filter", "uhf_read_selected_bank_words",
    "uhf_last_tag_reply_error", "uhf_error_means_window_too_long",
    "uhf_settle_reader", "uhf_custom_tid_round",
    "uhf_begin_s0_session", "uhf_restore_session", "uhf_apply_tid_length",
    "uhf_tid_hex_declared", "uhf_read_tid_without_epc", "uhf_probe_tid_only_tag",
]
# Access-key fixes split the bank reader into a keyed implementation and wrapper.
if "static bool uhf_read_selected_bank_words_with_key(" in source:
    names.insert(1, "uhf_read_selected_bank_words_with_key")
session = source.index("typedef struct {", source.index("static bool uhf_custom_tid_round("))
session_end = source.index("} UhfSavedSession;", session) + len("} UhfSavedSession;")
block = source[session:session_end] + "\n\n" + "\n\n".join(map(function, names))
handler_start = source.index("    if(app->custom_tid_read &&", source.index("static void uhf_handle_frame("))
handler = braced_block(handler_start)
harness = (ROOT / "tests/tid_probe_harness.c.inc").read_text()
for marker, replacement in [("/* APPLICATION_TID_BLOCK */", block),
                            ("/* APPLICATION_CUSTOM_HANDLER */", handler)]:
    assert harness.count(marker) == 1
    harness = harness.replace(marker, replacement)
with tempfile.TemporaryDirectory(prefix="uhf-tid-probe-test-") as work:
    path = Path(work)
    (path / "tid_probe_test.c").write_text(harness)
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-Wno-unused-function", "-fsanitize=address,undefined", "-g",
        "-I", str(ROOT), str(path / "tid_probe_test.c"),
        str(ROOT / "uhf_tag.c"), str(ROOT / "uhf_protocol.c"),
        "-o", str(path / "tid_probe_test"),
    ], check=True)
    subprocess.run([str(path / "tid_probe_test")], check=True)

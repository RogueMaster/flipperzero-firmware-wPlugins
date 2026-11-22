#!/usr/bin/env python3
"""Run portable protocol, storage and actual settings-handler regression tests."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
compiler = os.environ.get("CC", "cc")
flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-fsanitize=address,undefined", "-g", "-I", str(root)]
cases = {
    "frequency": ["tests/uhf_frequency_test.c", "uhf_frequency.c", "uhf_protocol.c"],
    "saved_tag": ["tests/uhf_saved_tag_test.c", "uhf_saved_tag.c", "uhf_tag.c", "uhf_protocol.c"],
    "settings_storage": ["tests/uhf_settings_storage_test.c", "uhf_settings.c", "uhf_storage.c"],
}
with tempfile.TemporaryDirectory(prefix="uhf-host-tests-") as directory:
    for name, sources in cases.items():
        executable = str(Path(directory) / name)
        extra = ["-I", str(root / "tests/storage_stubs")] if name == "settings_storage" else []
        subprocess.run([compiler, *flags, *extra,
                        *(str(root / source) for source in sources), "-o", executable], check=True)
        subprocess.run([executable], check=True)
for script in ("run_settings_state_tests.py", "run_saved_write_tests.py", "run_tid_probe_tests.py"):
    subprocess.run([sys.executable, str(root / "tests" / script)], check=True)

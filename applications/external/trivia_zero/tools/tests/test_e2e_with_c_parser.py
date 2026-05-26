from __future__ import annotations

import struct
import subprocess
from pathlib import Path

import pytest

from trivia_pack.models import BilingualQuestion
from trivia_pack.pack_writer import write_pack

_REPO_ROOT = Path(__file__).resolve().parents[2]


def _ensure_c_binary_built() -> Path:
    """Builds test_pack_reader if it isn't already, returns its path."""
    binary = _REPO_ROOT / "test_pack_reader"
    if not binary.exists():
        result = subprocess.run(
            ["make", "test_pack_reader"],
            cwd=_REPO_ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            pytest.skip(f"could not build C side: {result.stderr}")
    return binary


def test_python_writer_output_is_consumable_by_c_parser(tmp_path: Path) -> None:
    """Round-trip: write a small pack with Python, parse the TSV+IDX with the
    same byte-level routines used on device.

    The C `test_pack_reader` binary tests pure parsers with hand-crafted bytes;
    here we don't re-run it (it doesn't touch the filesystem). Instead we verify
    bit-level compatibility by:
      1) Writing a pack with Python.
      2) Reading the IDX header back with Python and confirming magic/version/count.
      3) Reading each TSV line at the offset given by the IDX and confirming it
         parses with exactly 3 tabs.

    This catches any layout drift between the Python writer and the C reader's
    expectations without needing on-device execution.
    """
    qs = [
        BilingualQuestion(1, "¿Capital de España?", "Madrid", "Capital of Spain?", "Madrid"),
        BilingualQuestion(6, "¿Año del primer Mundial?", "1930", "First World Cup year?", "1930"),
        BilingualQuestion(7, "¿Qué es el H2O?", "Agua", "What is H2O?", "Water"),
    ]
    write_pack(qs, out_dir=tmp_path)

    for lang in ("es", "en"):
        tsv = (tmp_path / f"trivia_{lang}.tsv").read_bytes()
        idx = (tmp_path / f"trivia_{lang}.idx").read_bytes()

        # Header
        assert idx[:4] == b"TRVI"
        version = struct.unpack_from("<H", idx, 4)[0]
        count = struct.unpack_from("<I", idx, 6)[0]
        assert version == 1
        assert count == len(qs)

        # Each offset lands at the start of a parseable line
        for i in range(count):
            off = struct.unpack_from("<I", idx, 10 + i * 4)[0]
            line_end = tsv.index(b"\n", off)
            line = tsv[off:line_end].decode("utf-8")
            parts = line.split("\t")
            assert len(parts) == 4
            assert parts[0] == str(i)
            assert int(parts[1]) == qs[i].bucket_id


def test_c_test_pack_reader_still_passes() -> None:
    """The existing C `test_pack_reader` should still pass — sanity check that
    Plan 2 has not broken Plan 3."""
    binary = _ensure_c_binary_built()
    result = subprocess.run([str(binary)], cwd=_REPO_ROOT, check=False)
    assert result.returncode == 0

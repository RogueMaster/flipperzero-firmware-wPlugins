"""Writes the (TSV, IDX) pair per language. Format mirrors the C pack_reader.

Two output forms share the same byte-level encoding:

  write_pack(qs, out_dir)            -> data/trivia_{es,en}.{tsv,idx}
      Binary artifacts. Useful for review (TSV is human-readable) and for
      integration tests that need real files on disk.

  write_embedded_pack(qs, c_out_dir) -> src/data/embedded_pack_{es,en}.c
      C source files declaring `const uint8_t trivia_<lang>_idx[]`,
      `const char trivia_<lang>_tsv[]`, plus their `_len` companions.
      The FAP compiles these directly into its binary so the user does
      not need to copy any auxiliary data to the SD card.
"""

from __future__ import annotations

import struct
import unicodedata
from collections.abc import Sequence
from pathlib import Path

from trivia_pack.models import BilingualQuestion

_MAGIC = b"TRVI"
_VERSION = 1
_MIN_BUCKET_ID = 1
_MAX_BUCKET_ID = 7
_MIN_PRINTABLE_BYTE = 0x20

_NON_DECOMPOSABLE_REPLACEMENTS = {
    "—": "-",
    "–": "-",
    "…": "...",
    "“": '"',
    "”": '"',
    "‘": "'",
    "’": "'",
    "«": '"',
    "»": '"',
    "¿": "",
    "¡": "",
}


def _to_ascii(s: str) -> str:
    """Transliterate UTF-8 to ASCII so the on-device bitmap font can render it.

    Flipper's stock FontSecondary/FontPrimary are ASCII-only (no Latin-1
    supplement). Spanish accents and `ñ` would render as garbage bytes.
    NFKD decomposes accented characters into base + combining mark; the
    ascii-encode then drops the combining marks. Punctuation that does not
    decompose (em dash, curly quotes, inverted ?/!) is mapped explicitly.
    """
    for src, dst in _NON_DECOMPOSABLE_REPLACEMENTS.items():
        s = s.replace(src, dst)
    decomposed = unicodedata.normalize("NFKD", s)
    return decomposed.encode("ascii", errors="ignore").decode("ascii")


def _sanitize(field: str) -> str:
    """Make a field safe for the on-device parser and renderer:
    transliterate to ASCII (font constraint) and strip the field separators
    `\\t` / `\\n` / `\\r` defensively."""
    ascii_only = _to_ascii(field)
    return ascii_only.replace("\t", " ").replace("\n", " ").replace("\r", " ")


def _validate_buckets(questions: Sequence[BilingualQuestion]) -> None:
    for q in questions:
        if not (_MIN_BUCKET_ID <= q.bucket_id <= _MAX_BUCKET_ID):
            raise ValueError(f"bucket_id {q.bucket_id} is out of range 1..7")


def _build_tsv_bytes(rows: list[tuple[str, str, int]]) -> tuple[bytes, list[int]]:
    """Returns (tsv_bytes, offsets) where offsets[i] is the byte position where
    row i starts in tsv_bytes."""
    tsv = bytearray()
    offsets: list[int] = []
    for i, (question, answer, bucket_id) in enumerate(rows):
        offsets.append(len(tsv))
        tsv.extend(f"{i}\t{bucket_id}\t{question}\t{answer}\n".encode())
    return bytes(tsv), offsets


def _build_idx_bytes(offsets: list[int]) -> bytes:
    blob = bytearray()
    blob += _MAGIC
    blob += struct.pack("<H", _VERSION)
    blob += struct.pack("<I", len(offsets))
    for off in offsets:
        blob += struct.pack("<I", off)
    return bytes(blob)


def write_pack(questions: Sequence[BilingualQuestion], out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    _validate_buckets(questions)

    es_rows = [(_sanitize(q.question_es), _sanitize(q.answer_es), q.bucket_id) for q in questions]
    en_rows = [(_sanitize(q.question_en), _sanitize(q.answer_en), q.bucket_id) for q in questions]

    _write_one(out_dir / "trivia_es.tsv", out_dir / "trivia_es.idx", es_rows)
    _write_one(out_dir / "trivia_en.tsv", out_dir / "trivia_en.idx", en_rows)


def _write_one(
    tsv_path: Path,
    idx_path: Path,
    rows: list[tuple[str, str, int]],
) -> None:
    tsv_bytes, offsets = _build_tsv_bytes(rows)
    tsv_path.write_bytes(tsv_bytes)
    idx_path.write_bytes(_build_idx_bytes(offsets))


def write_embedded_pack(questions: Sequence[BilingualQuestion], c_out_dir: Path) -> None:
    """Emits two C source files (one per language) that compile the pack into
    the FAP binary as `const` arrays. The runtime reads from these arrays via
    pointer arithmetic — no SD lookup, no auxiliary install step."""
    c_out_dir.mkdir(parents=True, exist_ok=True)
    _validate_buckets(questions)

    es_rows = [(_sanitize(q.question_es), _sanitize(q.answer_es), q.bucket_id) for q in questions]
    en_rows = [(_sanitize(q.question_en), _sanitize(q.answer_en), q.bucket_id) for q in questions]

    _emit_c_source(c_out_dir / "embedded_pack_es.c", "trivia_es", es_rows)
    _emit_c_source(c_out_dir / "embedded_pack_en.c", "trivia_en", en_rows)


def _escape_c_string_literal(s: str) -> str:
    """Escape a single string for use inside C double quotes. UTF-8 bytes pass
    through (the C compiler handles them as-is); only ASCII control / quote /
    backslash get escaped."""
    out: list[str] = []
    for ch in s:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ord(ch) < _MIN_PRINTABLE_BYTE:
            out.append(f"\\x{ord(ch):02x}")
        else:
            out.append(ch)
    return "".join(out)


def _emit_c_source(path: Path, prefix: str, rows: list[tuple[str, str, int]]) -> None:
    tsv_bytes, offsets = _build_tsv_bytes(rows)
    idx_bytes = _build_idx_bytes(offsets)

    lines: list[str] = [
        "/* Auto-generated by tools/build_pack.py — do not edit by hand. */",
        "/* clang-format off */",
        "",
        '#include "include/data/embedded_pack.h"',
        "",
        f"const uint8_t {prefix}_idx[{len(idx_bytes)}] = {{",
    ]
    bytes_per_row = 16
    for start in range(0, len(idx_bytes), bytes_per_row):
        chunk = idx_bytes[start : start + bytes_per_row]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_str},")
    lines.append("};")
    lines.append(f"const size_t {prefix}_idx_len = {len(idx_bytes)}u;")
    lines.append("")

    lines.append(f"const char {prefix}_tsv[] =")
    tsv_text = tsv_bytes.decode("utf-8")
    text_lines = tsv_text.split("\n")
    if text_lines and text_lines[-1] == "":
        text_lines = text_lines[:-1]
    if not text_lines:
        lines.append('    "";')
    else:
        for tl in text_lines:
            escaped = _escape_c_string_literal(tl + "\n")
            lines.append(f'    "{escaped}"')
        lines[-1] += ";"
    lines.append(f"const size_t {prefix}_tsv_len = sizeof({prefix}_tsv) - 1u;")
    lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8")

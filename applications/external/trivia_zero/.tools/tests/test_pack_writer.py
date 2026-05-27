from __future__ import annotations

from pathlib import Path

import pytest

from trivia_pack.models import BilingualQuestion
from trivia_pack.pack_writer import write_embedded_pack, write_pack


def _read_idx_header(blob: bytes) -> tuple[int, int]:
    assert blob[:4] == b"TRVI"
    version = int.from_bytes(blob[4:6], "little")
    count = int.from_bytes(blob[6:10], "little")
    return version, count


def _read_idx_offset(blob: bytes, i: int) -> int:
    base = 10 + i * 4
    return int.from_bytes(blob[base : base + 4], "little")


def test_writes_two_tsvs_and_two_idxs(tmp_path: Path) -> None:
    qs = [
        BilingualQuestion(
            bucket_id=1,
            question_es="¿Capital de España?",
            answer_es="Madrid",
            question_en="Capital of Spain?",
            answer_en="Madrid",
        ),
        BilingualQuestion(
            bucket_id=6,
            question_es="¿Año del primer Mundial?",
            answer_es="1930",
            question_en="First World Cup year?",
            answer_en="1930",
        ),
    ]
    write_pack(qs, out_dir=tmp_path)

    for name in ("trivia_es.tsv", "trivia_en.tsv", "trivia_es.idx", "trivia_en.idx"):
        assert (tmp_path / name).exists()


def test_tsv_has_dense_ids_and_correct_fields(tmp_path: Path) -> None:
    qs = [
        BilingualQuestion(1, "Q1es", "A1es", "Q1en", "A1en"),
        BilingualQuestion(7, "Q2es", "A2es", "Q2en", "A2en"),
        BilingualQuestion(3, "Q3es", "A3es", "Q3en", "A3en"),
    ]
    write_pack(qs, out_dir=tmp_path)

    es = (tmp_path / "trivia_es.tsv").read_text(encoding="utf-8").splitlines()
    en = (tmp_path / "trivia_en.tsv").read_text(encoding="utf-8").splitlines()
    assert len(es) == 3 and len(en) == 3

    for i, line in enumerate(es):
        parts = line.split("\t")
        assert parts[0] == str(i)
        assert parts[1] == str(qs[i].bucket_id)
        assert parts[2] == qs[i].question_es
        assert parts[3] == qs[i].answer_es

    for i, line in enumerate(en):
        parts = line.split("\t")
        assert parts[0] == str(i)
        assert parts[1] == str(qs[i].bucket_id)
        assert parts[2] == qs[i].question_en
        assert parts[3] == qs[i].answer_en


def test_idx_is_consistent_with_tsv_offsets(tmp_path: Path) -> None:
    qs = [BilingualQuestion(1, "AAA", "Madrid", "AAA", "Madrid") for _ in range(5)]
    write_pack(qs, out_dir=tmp_path)

    tsv_bytes = (tmp_path / "trivia_es.tsv").read_bytes()
    idx_blob = (tmp_path / "trivia_es.idx").read_bytes()

    version, count = _read_idx_header(idx_blob)
    assert version == 1
    assert count == 5

    # Verify each offset lands at the start of a line.
    for i in range(count):
        off = _read_idx_offset(idx_blob, i)
        assert off >= 0
        # offset must be 0 or one past a newline
        if off > 0:
            assert tsv_bytes[off - 1 : off] == b"\n"
        # the 4 tab-separated fields starting at this offset must include exactly 3 tabs
        # before the next newline
        line_end = tsv_bytes.index(b"\n", off)
        line = tsv_bytes[off:line_end].decode("utf-8")
        assert line.count("\t") == 3


def test_tabs_and_newlines_in_fields_are_sanitized(tmp_path: Path) -> None:
    q = BilingualQuestion(
        bucket_id=2,
        question_es="hola\tmundo\nlinea2",
        answer_es="ok\nok",
        question_en="hello\tworld\nline2",
        answer_en="ok\nok",
    )
    write_pack([q], out_dir=tmp_path)
    es = (tmp_path / "trivia_es.tsv").read_text(encoding="utf-8")
    assert "\t" in es  # the 3 separator tabs
    # but no stray tabs inside fields → exactly 3 tabs total per line
    assert es.strip("\n").count("\t") == 3
    # no embedded newlines (the file ends with one terminating newline)
    assert es.count("\n") == 1


def test_invalid_bucket_id_raises(tmp_path: Path) -> None:
    with pytest.raises(ValueError):
        write_pack(
            [BilingualQuestion(99, "q", "a", "q", "a")],
            out_dir=tmp_path,
        )
    with pytest.raises(ValueError):
        write_pack(
            [BilingualQuestion(0, "q", "a", "q", "a")],
            out_dir=tmp_path,
        )


def test_write_embedded_pack_emits_two_c_files(tmp_path: Path) -> None:
    qs = [
        BilingualQuestion(1, "¿Capital?", "Madrid", "Capital?", "Madrid"),
        BilingualQuestion(7, 'Quote "test"', "OK", 'Quote "test"', "OK"),
    ]
    write_embedded_pack(qs, c_out_dir=tmp_path)

    es = (tmp_path / "embedded_pack_es.c").read_text(encoding="utf-8")
    en = (tmp_path / "embedded_pack_en.c").read_text(encoding="utf-8")

    for src in (es, en):
        assert "embedded_pack.h" in src
        assert "_idx[" in src and "_idx_len" in src
        assert "_tsv[]" in src and "_tsv_len" in src

    assert "0x54, 0x52, 0x56, 0x49" in es

    assert "\\t1\\t" in es
    assert "\\n" in es

    assert 'Quote \\"test\\"' in es


def test_special_chars_transliterated_to_ascii(tmp_path: Path) -> None:
    """Spanish accents and inverted punctuation are stripped/normalized so
    they render on Flipper's ASCII-only bitmap font."""
    qs = [
        BilingualQuestion(
            bucket_id=1,
            question_es="¿Capital de España?",
            answer_es="Madrid",
            question_en="Capital of Spain?",
            answer_en="Madrid",
        ),
        BilingualQuestion(
            bucket_id=3,
            question_es="¿Quién pintó La Mona Lisa?",
            answer_es="Da Vinci",
            question_en="Who painted the Mona Lisa?",
            answer_en="Da Vinci",
        ),
    ]
    write_pack(qs, out_dir=tmp_path)
    es = (tmp_path / "trivia_es.tsv").read_text(encoding="utf-8")

    assert "Capital de Espana?" in es
    assert "Quien pinto La Mona Lisa?" in es
    assert "España" not in es
    assert "ñ" not in es
    assert "¿" not in es
    assert "ó" not in es


def test_only_ascii_bytes_emitted(tmp_path: Path) -> None:
    qs = [BilingualQuestion(1, "¡Hola, ñoño!", "Sí", "Hi!", "Yes")]
    write_pack(qs, out_dir=tmp_path)
    raw = (tmp_path / "trivia_es.tsv").read_bytes()
    assert all(b < 0x80 for b in raw), f"non-ASCII bytes found: {raw!r}"


def test_write_embedded_pack_validates_bucket_id(tmp_path: Path) -> None:
    with pytest.raises(ValueError):
        write_embedded_pack(
            [BilingualQuestion(8, "q", "a", "q", "a")],
            c_out_dir=tmp_path,
        )

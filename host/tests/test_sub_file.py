"""Тесты офлайн-разбора таймингов."""

from __future__ import annotations

from tpms.decoder import build_frame, chips_to_timings, frame_to_chips
from tpms.sub_file import load, parse_raw_dump, parse_sub

RAW = build_frame(0x7AD779, 243.75, 22)
TIMINGS = chips_to_timings(frame_to_chips(RAW))


def _sub_text(timings: list[int], per_line: int = 32) -> str:
    lines = [
        "Filetype: Flipper SubGhz RAW File",
        "Version: 1",
        "Frequency: 433920000",
        "Preset: FuriHalSubGhzPresetCustom",
        "Protocol: RAW",
    ]
    for start in range(0, len(timings), per_line):
        chunk = timings[start : start + per_line]
        lines.append("RAW_Data: " + " ".join(str(value) for value in chunk))
    return "\n".join(lines) + "\n"


def test_parse_sub_reads_metadata_and_timings():
    capture = parse_sub(_sub_text(TIMINGS))

    assert capture.frequency == 433_920_000
    assert capture.preset == "FuriHalSubGhzPresetCustom"
    assert capture.timings == TIMINGS


def test_parse_sub_decodes_frame():
    frames = parse_sub(_sub_text(TIMINGS)).decode()
    assert [f.raw for f in frames] == [RAW]


def test_parse_raw_dump_decodes_frame():
    dump = "\n".join(
        " ".join(f"{'+' if v > 0 else '-'}{abs(v)}" for v in TIMINGS[i : i + 16])
        for i in range(0, len(TIMINGS), 16)
    )
    capture = parse_raw_dump(dump)
    assert [f.raw for f in capture.decode()] == [RAW]


def test_parse_raw_dump_skips_json_lines():
    dump = '{"event":"started","freq":433920000}\n+50 -50 +50\n{"event":"stopped"}'
    capture = parse_raw_dump(dump)
    assert capture.timings == [50, -50, 50]


def test_load_detects_format(tmp_path):
    sub = tmp_path / "capture.sub"
    sub.write_text(_sub_text(TIMINGS), encoding="utf-8")
    assert [f.raw for f in load(sub).decode()] == [RAW]

    dump = tmp_path / "capture.txt"
    dump.write_text(" ".join(str(v) for v in TIMINGS), encoding="utf-8")
    assert [f.raw for f in load(dump).decode()] == [RAW]

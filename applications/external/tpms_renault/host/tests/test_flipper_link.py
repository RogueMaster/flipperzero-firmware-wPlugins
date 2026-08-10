"""Tests for parsing lines coming from the Flipper. No port is opened."""

from __future__ import annotations

import queue

from tpms.decoder import build_frame
from tpms.flipper_link import FlipperLink, Reading, _clean

RAW = build_frame(0x7AD779, 243.75, 22)
RAW_HEX = RAW.hex()


def _link(mode: str = "json") -> FlipperLink:
    return FlipperLink(port="/dev/null", mode=mode, events=queue.Queue())


def _events(link: FlipperLink) -> list[tuple[str, object]]:
    out = []
    while not link.events.empty():
        out.append(link.events.get_nowait())
    return out


def test_frame_line_becomes_reading():
    link = _link()
    link._handle_line(
        '{"t":123456,"proto":"renault","id":"7ad779","raw":"%s",'
        '"pressure_kpa_x100":24375,"temp_c":22,"flags":54,"unknown":65535,'
        '"rssi_dbm_x10":-625}' % RAW_HEX
    )

    events = _events(link)
    assert len(events) == 1
    kind, reading = events[0]
    assert kind == "reading"
    assert isinstance(reading, Reading)
    assert reading.sensor_id == "7ad779"
    assert reading.frame.pressure_kpa == 243.75
    assert reading.frame.temperature_c == 22
    assert reading.rssi_dbm == -62.5
    assert reading.device_tick == 123456


def test_fields_are_recomputed_from_raw_not_trusted():
    """The firmware may compute fields differently — trust only raw bytes."""
    link = _link()
    link._handle_line(
        '{"t":1,"raw":"%s","pressure_kpa_x100":999999,"temp_c":-99}' % RAW_HEX
    )
    kind, reading = _events(link)[0]
    assert kind == "reading"
    assert reading.frame.pressure_kpa == 243.75
    assert reading.frame.temperature_c == 22


def test_frame_with_bad_crc_is_reported_not_decoded():
    broken = bytearray(RAW)
    broken[8] ^= 0xFF
    link = _link()
    link._handle_line('{"t":1,"raw":"%s"}' % bytes(broken).hex())

    kinds = [kind for kind, _ in _events(link)]
    assert kinds == ["line"]


def test_error_and_event_lines():
    link = _link()
    link._handle_line('{"error":"radio busy"}')
    link._handle_line('{"event":"started","freq":433920000,"mode":"json"}')

    kinds = [kind for kind, _ in _events(link)]
    assert kinds == ["error", "status"]


def test_raw_mode_collects_timing_lines():
    link = _link(mode="raw")
    link._handle_line("+52 -49 +101 -50")
    kind, payload = _events(link)[0]
    assert kind == "raw"
    assert payload == "+52 -49 +101 -50"


def test_raw_lines_ignored_in_json_mode():
    link = _link(mode="json")
    link._handle_line("+52 -49")
    assert [kind for kind, _ in _events(link)] == ["line"]


def test_echo_and_prompt_are_filtered():
    link = _link()
    link._handle_line("tpms_rx 433920000 json")
    link._handle_line(">: ")
    link._handle_line("")
    assert _events(link) == []


def test_ansi_sequences_are_stripped():
    assert _clean(b"\x1b[0;32mPackets\x1b[0m received") == "Packets received"


def test_garbage_json_falls_back_to_line():
    link = _link()
    link._handle_line("{not json at all")
    assert [kind for kind, _ in _events(link)] == ["line"]

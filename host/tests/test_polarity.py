"""Receiver polarity.

A real 407003VU0B sensor revealed that the polarity of the sync word and
the Manchester convention in the data may disagree, so the decoder has to
try them independently — all four combinations.
"""

from __future__ import annotations

import pytest

from tpms.decoder import (
    SYNC,
    build_frame,
    chips_to_timings,
    decode_chips,
    decode_timings,
    frame_to_chips,
    parse_frame,
)

RAW = build_frame(0x7AD779, 243.75, 22)

# A real frame captured from a 407003VU0B sensor: normal sync, inverted
# data. The CRC matches, temperature 26 C, pressure near zero — the sensor
# was lying on a table rather than mounted in a tyre.
REAL_RAW = bytes.fromhex("cc04389dc902c680b9")


def _invert(chips: str) -> str:
    return chips.translate(str.maketrans("01", "10"))


def _mixed_polarity(raw: bytes) -> str:
    """Sync in normal polarity, data inverted."""
    chips = frame_to_chips(raw)
    sync_end = chips.index(SYNC) + len(SYNC)
    return chips[:sync_end] + _invert(chips[sync_end:])


@pytest.mark.parametrize(
    "name,transform",
    [
        ("everything normal", lambda c: c),
        ("everything inverted", _invert),
        ("normal sync, inverted data", lambda c: _mixed_polarity(RAW)),
    ],
)
def test_all_polarity_combinations_decode(name, transform):
    frames = decode_chips(transform(frame_to_chips(RAW)))
    assert [f.raw for f in frames] == [RAW], name


def test_mixed_polarity_survives_timings_roundtrip():
    frames = decode_timings(chips_to_timings(_mixed_polarity(RAW)))
    assert [f.raw for f in frames] == [RAW]


def test_real_sensor_frame_parses():
    frame = parse_frame(REAL_RAW)
    assert frame is not None
    assert frame.id_hex == "02c99d"
    assert frame.temperature_c == 26
    assert frame.pressure_kpa == 3.0
    assert frame.flags == 0x33


def test_real_sensor_frame_decodes_from_chips():
    frames = decode_chips(_mixed_polarity(REAL_RAW))
    assert [f.raw for f in frames] == [REAL_RAW]

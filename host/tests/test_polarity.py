"""Полярность приёмника.

На живом датчике 407003VU0B обнаружилось, что полярность sync-слова и
конвенция Manchester в данных могут не совпадать, поэтому декодер обязан
перебирать их независимо — все четыре сочетания.
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

# Реальный кадр, снятый с датчика 407003VU0B: sync прямой, данные
# инвертированные. CRC сходится, температура 26 °C, давление около нуля —
# датчик лежал на столе, а не стоял в шине.
REAL_RAW = bytes.fromhex("cc04389dc902c680b9")


def _invert(chips: str) -> str:
    return chips.translate(str.maketrans("01", "10"))


def _mixed_polarity(raw: bytes) -> str:
    """Sync в прямой полярности, данные в обратной."""
    chips = frame_to_chips(raw)
    sync_end = chips.index(SYNC) + len(SYNC)
    return chips[:sync_end] + _invert(chips[sync_end:])


@pytest.mark.parametrize(
    "name,transform",
    [
        ("всё прямое", lambda c: c),
        ("всё инвертированное", _invert),
        ("sync прямой, данные инвертированные", lambda c: _mixed_polarity(RAW)),
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

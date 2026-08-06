"""Тесты декодера — без Flipper и без датчика."""

from __future__ import annotations

import random

import pytest

from tpms.decoder import (
    CHIP_US,
    SYNC,
    build_frame,
    chips_to_timings,
    crc8,
    decode_chips,
    decode_timings,
    frame_to_chips,
    manchester_decode,
    parse_frame,
    timings_to_chips,
)

# Тестовый вектор из ProtoView (protocols/tpms/renault.c). По его же
# комментариям это ID 0x7AD779, 244 кПа, 22 °C.
PROTOVIEW_VECTOR = (
    "01010101010101010110"  # хвост преамбулы + sync
    "010110010110"  # флаги
    "10011001101010011001"  # давление, 10 бит
    "1010010110011010"  # температура
    "1001010101101001"
    "0101100110010101"
    "1001010101100110"  # ID, 24 бита
    "0101010101010101"
    "0101010101010101"  # два байта 0xFF
    "0110010101010101"  # CRC-8
)

EXPECTED_RAW = bytes.fromhex("d9453479d77affffbf")


def test_protoview_vector_decodes():
    frames = decode_chips(PROTOVIEW_VECTOR)
    assert len(frames) == 1
    frame = frames[0]

    assert frame.raw == EXPECTED_RAW
    assert frame.id_hex == "7ad779"
    assert frame.sensor_id == 0x7AD779
    assert frame.pressure_kpa == pytest.approx(243.75)
    assert round(frame.pressure_kpa) == 244
    assert frame.temperature_c == 22
    assert frame.unknown == 0xFFFF


def test_protoview_vector_survives_inverted_polarity():
    """Приёмник мог отдать поток с обратной полярностью."""
    inverted = PROTOVIEW_VECTOR.translate(str.maketrans("01", "10"))
    frames = decode_chips(inverted)
    assert len(frames) == 1
    assert frames[0].raw == EXPECTED_RAW


def test_crc8_matches_reference_frame():
    assert crc8(EXPECTED_RAW[:8]) == EXPECTED_RAW[8]


def test_parse_frame_rejects_bad_crc():
    broken = bytearray(EXPECTED_RAW)
    broken[8] ^= 0xFF
    assert parse_frame(bytes(broken)) is None


def test_parse_frame_rejects_wrong_length():
    assert parse_frame(EXPECTED_RAW[:8]) is None


def test_manchester_rejects_broken_pair():
    chips = PROTOVIEW_VECTOR[: len(SYNC)] + "00" + PROTOVIEW_VECTOR[len(SYNC) + 2 :]
    assert manchester_decode(chips, len(SYNC)) is None


def test_build_frame_roundtrip():
    raw = build_frame(sensor_id=0x7AD779, pressure_kpa=243.75, temperature_c=22, flags=0x36)
    assert raw == EXPECTED_RAW

    frame = parse_frame(raw)
    assert frame is not None
    assert frame.sensor_id == 0x7AD779
    assert frame.temperature_c == 22


@pytest.mark.parametrize(
    "sensor_id,kpa,temp",
    [
        (0x000001, 0.0, -30),
        (0xFFFFFF, 767.25, 225),
        (0x123456, 220.5, 21),
        (0xABCDEF, 101.25, 0),
    ],
)
def test_synthetic_roundtrip_through_timings(sensor_id, kpa, temp):
    raw = build_frame(sensor_id, kpa, temp)
    timings = chips_to_timings(frame_to_chips(raw))

    frames = decode_timings(timings)
    assert len(frames) == 1
    frame = frames[0]
    assert frame.sensor_id == sensor_id
    assert frame.pressure_kpa == pytest.approx(kpa, abs=0.75)
    assert frame.temperature_c == temp


def test_decode_survives_timing_jitter():
    """CC1101 отдаёт тайминги с разбросом — декодер обязан это пережить."""
    rng = random.Random(1234)
    raw = build_frame(0x7AD779, 243.75, 22)
    timings = chips_to_timings(frame_to_chips(raw))

    jittered = []
    for value in timings:
        scale = rng.uniform(0.85, 1.15)
        jittered.append(int(round(value * scale)))

    frames = decode_timings(jittered)
    assert [f.raw for f in frames] == [raw]


def test_decode_finds_frame_in_noisy_stream():
    """Кадр внутри шума и с шумом после него."""
    rng = random.Random(7)
    raw = build_frame(0x0AB1C2, 250.5, 18)

    noise_before = [rng.choice([-1, 1]) * rng.randint(20, 300) for _ in range(200)]
    noise_after = [rng.choice([-1, 1]) * rng.randint(20, 300) for _ in range(200)]
    timings = noise_before + chips_to_timings(frame_to_chips(raw)) + noise_after

    frames = decode_timings(timings)
    assert raw in [f.raw for f in frames]


def test_corrupted_frame_is_rejected():
    raw = bytearray(build_frame(0x7AD779, 243.75, 22))
    raw[4] ^= 0x20  # портим ID, CRC больше не сойдётся
    timings = chips_to_timings(frame_to_chips(bytes(raw)))
    assert decode_timings(timings) == []


def test_repeated_frame_reported_once_per_burst():
    raw = build_frame(0x7AD779, 243.75, 22)
    chips = frame_to_chips(raw)
    frames = decode_chips(chips + chips)
    assert len(frames) == 1


def test_timings_to_chips_splits_on_long_gap():
    timings = [50, -50, 50, -50] + [-5000] + [50, -50, 50, -50]
    bursts = timings_to_chips(timings)
    assert bursts == ["1010", "1010"]


def test_timings_to_chips_counts_multi_chip_pulses():
    # 100 мкс -> 2 чипа, 150 -> 3, 52 -> 1
    assert timings_to_chips([100, -150, 52]) == ["110001"]


def test_chip_duration_matches_protocol():
    """Защита от случайной правки константы: чип ~50 мкс."""
    assert CHIP_US == 50

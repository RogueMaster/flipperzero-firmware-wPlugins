"""Декодер TPMS Renault: тайминги -> чипы -> Manchester -> 9 байт -> поля.

Протокол (сверено с rtl_433 src/devices/tpms_renault.c и ProtoView
protocols/tpms/renault.c):

  * 2-FSK, 433.92 МГц, длительность чипа ~50 мкс (~20 kBaud).
  * Преамбула 55 55 55 56, в потоке чипов ищется 20-битный sync
    "01010101010101010110".
  * Дальше 144 чипа Manchester -> 72 бита -> 9 байт.
  * CRC-8 (poly 0x07, init 0x00) по первым 8 байтам == байт 8.

Про полярность Manchester. rtl_433 инвертирует битбуфер и ищет aa a9,
ProtoView ищет "...0110" напрямую; в обоих случаях пара чипов "01"
означает единицу, а "10" — ноль. Комментарий в шапке renault.c у
ProtoView утверждает обратное, но его же тестовый вектор декодируется
в заявленные ID 0x7AD779 / 244 кПа / 22 °C только при "01" -> 1.

Приёмник может отдать поток с любой полярностью, поэтому decode_chips()
пробует обе и оставляет ту, где сходится CRC.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable, Iterator, Sequence

CHIP_US = 50
"""Номинальная длительность одного чипа, мкс."""

SYNC = "01010101010101010110"
"""Хвост преамбулы + sync-слово в чипах."""

FRAME_BYTES = 9
FRAME_BITS = FRAME_BYTES * 8
FRAME_CHIPS = FRAME_BITS * 2

MAX_CHIPS_PER_PULSE = 8
"""Импульс длиннее — это уже не часть кадра, поток рвётся."""


def crc8(data: bytes, poly: int = 0x07, init: int = 0x00) -> int:
    """CRC-8 в том же виде, что crc8() в rtl_433 и ProtoView."""
    crc = init
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


@dataclass(frozen=True)
class RenaultFrame:
    """Разобранный кадр датчика."""

    raw: bytes
    sensor_id: int
    pressure_kpa: float
    temperature_c: int
    flags: int
    unknown: int

    @property
    def id_hex(self) -> str:
        return f"{self.sensor_id:06x}"

    @property
    def raw_hex(self) -> str:
        return self.raw.hex()

    @property
    def pressure_bar(self) -> float:
        return self.pressure_kpa / 100.0

    @property
    def pressure_psi(self) -> float:
        return self.pressure_kpa * 0.1450377

    def to_dict(self) -> dict:
        return {
            "proto": "renault",
            "id": self.id_hex,
            "raw": self.raw_hex,
            "pressure_kpa": round(self.pressure_kpa, 2),
            "temp_c": self.temperature_c,
            "flags": self.flags,
            "unknown": self.unknown,
        }


def parse_frame(raw: bytes) -> RenaultFrame | None:
    """9 байт -> поля. Возвращает None, если CRC не сошлась."""
    if len(raw) != FRAME_BYTES:
        return None
    if crc8(raw[:8]) != raw[8]:
        return None

    pressure_raw = ((raw[0] & 0x03) << 8) | raw[1]
    return RenaultFrame(
        raw=bytes(raw),
        sensor_id=(raw[5] << 16) | (raw[4] << 8) | raw[3],  # little-endian
        pressure_kpa=pressure_raw * 0.75,
        temperature_c=raw[2] - 30,
        flags=raw[0] >> 2,
        unknown=(raw[7] << 8) | raw[6],
    )


def manchester_decode(
    chips: str,
    offset: int,
    nbytes: int = FRAME_BYTES,
    one: str = "01",
) -> bytes | None:
    """Пары чипов -> байты.

    `one` задаёт, какая пара означает единицу. Возвращает None, если чипов
    не хватает или встретилась нарушенная пара ("00"/"11") — то есть кадр
    оборвался.
    """
    zero = "10" if one == "01" else "01"
    need = nbytes * 16
    if offset + need > len(chips):
        return None

    out = bytearray()
    acc = 0
    nbits = 0
    for i in range(offset, offset + need, 2):
        pair = chips[i : i + 2]
        if pair == one:
            bit = 1
        elif pair == zero:
            bit = 0
        else:
            return None
        acc = (acc << 1) | bit
        nbits += 1
        if nbits == 8:
            out.append(acc)
            acc = 0
            nbits = 0
    return bytes(out)


def _invert(chips: str) -> str:
    return chips.translate(str.maketrans("01", "10"))


SYNC_INVERTED = _invert(SYNC)

# Приёмник может отдать поток с любой полярностью, и — как показали живые
# датчики — полярность sync-слова не обязана совпадать с конвенцией
# Manchester в данных. Поэтому перебираем оба независимо: правильной
# считается та комбинация, на которой сходится CRC.
_VARIANTS = [
    (SYNC, "01"),
    (SYNC, "10"),
    (SYNC_INVERTED, "01"),
    (SYNC_INVERTED, "10"),
]


def decode_chips(chips: str) -> list[RenaultFrame]:
    """Найти в потоке чипов все валидные кадры."""
    frames: list[RenaultFrame] = []
    seen: set[bytes] = set()

    for pattern, one in _VARIANTS:
        pos = 0
        while True:
            idx = chips.find(pattern, pos)
            if idx < 0:
                break
            raw = manchester_decode(chips, idx + len(pattern), one=one)
            if raw is not None:
                frame = parse_frame(raw)
                if frame is not None and frame.raw not in seen:
                    seen.add(frame.raw)
                    frames.append(frame)
            # Преамбула — это "0101...", поэтому следующий кандидат может
            # начаться уже через два чипа.
            pos = idx + 2
    return frames


def timings_to_chips(
    timings: Iterable[int],
    chip_us: int = CHIP_US,
    gap_us: int | None = None,
) -> list[str]:
    """Знаковые длительности -> строки чипов, разбитые по паузам.

    Знак задаёт уровень: положительная длительность — высокий уровень,
    отрицательная — низкий (тот же формат, что в RAW_Data файлов .sub и
    в raw-режиме FAP-а). Длительность округляется до целого числа чипов;
    слишком длинный импульс или пауза обрывают текущую серию.
    """
    if gap_us is None:
        gap_us = chip_us * MAX_CHIPS_PER_PULSE

    bursts: list[str] = []
    current: list[str] = []

    for value in timings:
        if value == 0:
            continue
        level = "1" if value > 0 else "0"
        duration = abs(int(value))

        if duration > gap_us:
            # Длинная пауза/несущая: закрываем серию.
            if current:
                bursts.append("".join(current))
                current = []
            continue

        count = int((duration + chip_us // 2) // chip_us)
        if count < 1:
            count = 1
        current.append(level * count)

    if current:
        bursts.append("".join(current))
    return bursts


def decode_timings(timings: Iterable[int], chip_us: int = CHIP_US) -> list[RenaultFrame]:
    """Полный путь: тайминги -> кадры."""
    frames: list[RenaultFrame] = []
    seen: set[bytes] = set()
    for burst in timings_to_chips(timings, chip_us=chip_us):
        if len(burst) < len(SYNC) + FRAME_CHIPS:
            continue
        for frame in decode_chips(burst):
            if frame.raw not in seen:
                seen.add(frame.raw)
                frames.append(frame)
    return frames


# --------------------------------------------------------------------------
# Обратная сторона: сборка кадра. Нужна для тестов без железа и для
# проверки декодера на синтетике.
# --------------------------------------------------------------------------


def build_frame(
    sensor_id: int,
    pressure_kpa: float,
    temperature_c: int,
    flags: int = 0x36,
    unknown: int = 0xFFFF,
) -> bytes:
    """Собрать 9 байт кадра с корректной CRC."""
    pressure_raw = int(round(pressure_kpa / 0.75))
    raw = bytearray(9)
    raw[0] = ((flags & 0x3F) << 2) | ((pressure_raw >> 8) & 0x03)
    raw[1] = pressure_raw & 0xFF
    raw[2] = (temperature_c + 30) & 0xFF
    raw[3] = sensor_id & 0xFF
    raw[4] = (sensor_id >> 8) & 0xFF
    raw[5] = (sensor_id >> 16) & 0xFF
    raw[6] = unknown & 0xFF
    raw[7] = (unknown >> 8) & 0xFF
    raw[8] = crc8(bytes(raw[:8]))
    return bytes(raw)


def frame_to_chips(raw: bytes, preamble_chips: int = 32) -> str:
    """9 байт -> поток чипов с преамбулой и sync."""
    # Преамбула "0101...", последние 20 чипов которой и есть SYNC.
    lead = "01" * ((preamble_chips - len(SYNC)) // 2)
    chips = [lead, SYNC]
    for byte in raw:
        for i in range(7, -1, -1):
            chips.append("01" if (byte >> i) & 1 else "10")
    return "".join(chips)


def chips_to_timings(chips: str, chip_us: int = CHIP_US) -> list[int]:
    """Чипы -> знаковые длительности (склеивая одинаковые уровни)."""
    timings: list[int] = []
    if not chips:
        return timings

    run_level = chips[0]
    run_len = 0
    for chip in chips:
        if chip == run_level:
            run_len += 1
        else:
            timings.append(run_len * chip_us * (1 if run_level == "1" else -1))
            run_level = chip
            run_len = 1
    timings.append(run_len * chip_us * (1 if run_level == "1" else -1))
    return timings

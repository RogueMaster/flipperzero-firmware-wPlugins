"""Renault TPMS decoder: timings -> chips -> Manchester -> 9 bytes -> fields.

Protocol (cross-checked against rtl_433 src/devices/tpms_renault.c and
ProtoView protocols/tpms/renault.c):

  * 2-FSK, 433.92 MHz, chip duration ~50 us (~20 kBaud).
  * Preamble 55 55 55 56; the 20-bit sync "01010101010101010110" is
    searched for in the chip stream.
  * Then 144 Manchester chips -> 72 bits -> 9 bytes.
  * CRC-8 (poly 0x07, init 0x00) over the first 8 bytes == byte 8.

About the Manchester polarity. rtl_433 inverts the bit buffer and looks
for aa a9, ProtoView looks for "...0110" directly; in both cases the chip
pair "01" means one and "10" means zero. The header comment in ProtoView's
renault.c claims the opposite, but its own test vector only decodes into
the stated ID 0x7AD779 / 244 kPa / 22 C with "01" -> 1.

The receiver may hand over the stream in either polarity, so decode_chips()
tries both and keeps the one whose CRC checks out.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable, Iterator, Sequence

CHIP_US = 50
"""Nominal duration of a single chip, us."""

SYNC = "01010101010101010110"
"""Preamble tail plus sync word, in chips."""

FRAME_BYTES = 9
FRAME_BITS = FRAME_BYTES * 8
FRAME_CHIPS = FRAME_BITS * 2

MAX_CHIPS_PER_PULSE = 8
"""A longer pulse is no longer part of a frame; the stream is broken."""


def crc8(data: bytes, poly: int = 0x07, init: int = 0x00) -> int:
    """CRC-8 in the same form as crc8() in rtl_433 and ProtoView."""
    crc = init
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


@dataclass(frozen=True)
class TpmsFrame:
    """A decoded sensor frame, whichever protocol it came from.

    The firmware decodes every protocol it knows and reports the fields in
    shared units, so pressure and temperature can be missing: a few
    sensors send them in separate transmissions, and one or two carry
    neither.
    """

    raw: bytes
    sensor_id: int
    pressure_kpa: float | None = None
    temperature_c: int | None = None
    flags: int = 0
    unknown: int = 0
    proto: str = "renault"
    battery_ok: bool | None = None
    id_digits: int = 6

    @property
    def id_hex(self) -> str:
        return f"{self.sensor_id:0{self.id_digits}x}"

    @property
    def raw_hex(self) -> str:
        return self.raw.hex()

    @property
    def pressure_bar(self) -> float | None:
        return None if self.pressure_kpa is None else self.pressure_kpa / 100.0

    @property
    def pressure_psi(self) -> float | None:
        return None if self.pressure_kpa is None else self.pressure_kpa * 0.1450377

    def to_dict(self) -> dict:
        return {
            "proto": self.proto,
            "id": self.id_hex,
            "raw": self.raw_hex,
            "pressure_kpa": (
                None if self.pressure_kpa is None else round(self.pressure_kpa, 2)
            ),
            "temp_c": self.temperature_c,
            "flags": self.flags,
            "unknown": self.unknown,
        }


# The Renault decoder in this file predates the rest and is what the
# offline paths still use, so its old name stays as an alias.
RenaultFrame = TpmsFrame


def parse_frame(raw: bytes) -> TpmsFrame | None:
    """9 bytes -> fields. Returns None if the CRC does not match."""
    if len(raw) != FRAME_BYTES:
        return None
    if crc8(raw[:8]) != raw[8]:
        return None

    pressure_raw = ((raw[0] & 0x03) << 8) | raw[1]
    return TpmsFrame(
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
    """Chip pairs -> bytes.

    `one` says which pair means a one. Returns None if there are not
    enough chips or if a broken pair ("00"/"11") shows up, meaning the
    frame was cut short.
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

# The receiver may hand over the stream in either polarity and — as real
# sensors have shown — the polarity of the sync word does not have to match
# the Manchester convention in the data. So we try both independently: the
# right combination is the one whose CRC checks out.
_VARIANTS = [
    (SYNC, "01"),
    (SYNC, "10"),
    (SYNC_INVERTED, "01"),
    (SYNC_INVERTED, "10"),
]


def decode_chips(chips: str) -> list[TpmsFrame]:
    """Find every valid frame in a chip stream."""
    frames: list[TpmsFrame] = []
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
            # The preamble is "0101...", so the next candidate may start
            # just two chips later.
            pos = idx + 2
    return frames


def timings_to_chips(
    timings: Iterable[int],
    chip_us: int = CHIP_US,
    gap_us: int | None = None,
) -> list[str]:
    """Signed durations -> chip strings, split on gaps.

    The sign carries the level: a positive duration is high, a negative one
    is low (the same format as RAW_Data in .sub files and as the raw mode
    of the FAP). Durations are rounded to a whole number of chips; a pulse
    or gap that is too long ends the current burst.
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
            # Long gap or carrier: close the burst.
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


def decode_timings(timings: Iterable[int], chip_us: int = CHIP_US) -> list[TpmsFrame]:
    """The whole path: timings -> frames."""
    frames: list[TpmsFrame] = []
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
# The other direction: building a frame. Needed for tests without hardware
# and for checking the decoder against synthetic data.
# --------------------------------------------------------------------------


def build_frame(
    sensor_id: int,
    pressure_kpa: float,
    temperature_c: int,
    flags: int = 0x36,
    unknown: int = 0xFFFF,
) -> bytes:
    """Build the 9 frame bytes with a correct CRC."""
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
    """9 bytes -> a chip stream with preamble and sync."""
    # The preamble is "0101...", whose last 20 chips are the SYNC itself.
    lead = "01" * ((preamble_chips - len(SYNC)) // 2)
    chips = [lead, SYNC]
    for byte in raw:
        for i in range(7, -1, -1):
            chips.append("01" if (byte >> i) & 1 else "10")
    return "".join(chips)


def chips_to_timings(chips: str, chip_us: int = CHIP_US) -> list[int]:
    """Chips -> signed durations (merging runs of the same level)."""
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

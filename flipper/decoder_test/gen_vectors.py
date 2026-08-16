#!/usr/bin/env python3
"""Generate vectors.h: one synthetic frame per protocol, checked by rtl_433.

Every decoder in tpms_bridge is a port of one in rtl_433. To make sure a
port says the same thing as its original, this script builds a frame for
each protocol on the bit level — preamble, coding, checksum — hands it to
a real rtl_433 binary, and writes down both the bit stream and the values
rtl_433 reported. decoder_test then feeds the very same bits to the
firmware decoder and compares.

The expectations are therefore rtl_433's, not ours: a port that reads a
field from the wrong place or scales it wrongly fails the test.

Usage:  RTL433=/path/to/rtl_433 ./gen_vectors.py
The generated vectors.h is committed, so running rtl_433 is only needed
when protocols are added or changed.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys

RTL433 = os.environ.get("RTL433") or shutil.which("rtl_433")

# --------------------------------------------------------------------------
# Bit level helpers
# --------------------------------------------------------------------------


def bits_of(data: bytes) -> list[int]:
    return [(byte >> i) & 1 for byte in data for i in range(7, -1, -1)]


def manchester(bits: list[int]) -> str:
    """rtl_433 reads a pair as "not b, b" and outputs the second chip."""
    return "".join("01" if b else "10" for b in bits)


def diff_manchester(bits: list[int], first: int) -> str:
    """A transition inside the cell means zero, none means one."""
    out: list[str] = []
    second = 1 - first
    for value in bits:
        bit1 = 1 - second
        bit2 = bit1 if value else 1 - bit1
        out.append(f"{bit1}{bit2}")
        second = bit2
    return "".join(out)


def slicer_encode(bits: str) -> str:
    """Bits as they leave a Manchester slicer, back into chips.

    rtl_433's Manchester slicer calls a falling data edge a one, so a bit
    is the first chip of a pair and the second is its opposite. Protocols
    whose modulation is MANCHESTER_ZEROBIT are described in sliced bits,
    and this puts them back on the air.
    """
    return "".join(bit + ("0" if bit == "1" else "1") for bit in bits)


def invert(chips: str) -> str:
    return chips.translate(str.maketrans("01", "10"))


def to_code(chips: str) -> str:
    """A chip string as rtl_433 wants it on the -y command line.

    The length is declared exactly: several of these frames are not a
    whole number of bytes, and some decoders accept only one length.
    """
    padded = chips + "0" * ((8 - len(chips) % 8) % 8)
    packed = "".join(f"{int(padded[i:i + 8], 2):02x}" for i in range(0, len(padded), 8))
    return "{%d}%s" % (len(chips), packed)


def crc8(data: bytes, poly: int, init: int) -> int:
    crc = init
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def crc16(data: bytes, poly: int, init: int) -> int:
    crc = init
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def xor_bytes(data: bytes) -> int:
    result = 0
    for byte in data:
        result ^= byte
    return result


# --------------------------------------------------------------------------
# One frame per protocol
#
# `sync` is the sync word as the firmware table has it, `lead` the chips in
# front of it. `payload` returns the chips after the sync word; where a
# coding needs to find its footing, several variants are offered and the
# one rtl_433 accepts is kept.
# --------------------------------------------------------------------------

FILLER = "01" * 48


def mc_frame(data: bytes) -> list[str]:
    return [manchester(bits_of(data))]


def dmc_frame(data: bytes, prefixes=("", "0", "1")) -> list[str]:
    return [
        prefix + diff_manchester(bits_of(data), first)
        for prefix in prefixes
        for first in (0, 1)
    ]


def renault_frame() -> list[str]:
    b = bytearray(9)
    flags, pressure_raw, temp_c, sensor_id = 0x36, 325, 22, 0x7AD779
    b[0] = ((flags & 0x3F) << 2) | ((pressure_raw >> 8) & 3)
    b[1] = pressure_raw & 0xFF
    b[2] = (temp_c + 30) & 0xFF
    b[3] = sensor_id & 0xFF
    b[4] = (sensor_id >> 8) & 0xFF
    b[5] = (sensor_id >> 16) & 0xFF
    b[6] = 0xFF
    b[7] = 0xFF
    b[8] = crc8(bytes(b[:8]), 0x07, 0x00)
    return mc_frame(bytes(b))


def renault_0435r_frame() -> list[str]:
    b = bytearray(9)
    b[0], b[1], b[2] = 0x12, 0x34, 0x56  # id
    b[3] = 0xC0  # flags
    b[4] = 0xC8  # pressure, raw / 0.75 kPa
    b[5] = 22 + 50  # temperature
    b[6] = 12  # centrifugal acceleration
    b[8] = 0x80 | 5  # counter, top bit set
    b[7] = xor_bytes(bytes(b[:7])) ^ b[8]
    return mc_frame(bytes(b))


def citroen_frame() -> list[str]:
    b = bytearray(10)
    b[0] = 0x0F  # state, outside the checksum
    b[1], b[2], b[3], b[4] = 0x12, 0x34, 0x56, 0x78  # id
    b[5] = 0xC3  # flags and repeat counter
    b[6] = 0xB4  # pressure, 1.364 kPa a step
    b[7] = 22 + 50  # temperature
    b[8] = 0x2A  # battery?
    b[9] = xor_bytes(bytes(b[1:9]))
    return mc_frame(bytes(b))


def ford_frame() -> list[str]:
    b = bytearray(8)
    b[0], b[1], b[2], b[3] = 0x12, 0x34, 0x56, 0x78  # id
    b[4] = 0x80  # pressure, quarter PSI a step
    b[5] = 15 + 56  # temperature
    b[6] = 0x44  # moving
    b[7] = sum(b[:7]) & 0xFF
    return mc_frame(bytes(b))


def toyota_frame() -> list[str]:
    pressure, temperature, status = 0xB4, 0x3E, 0x00
    b = bytearray(9)
    b[0], b[1], b[2], b[3] = 0xC1, 0x34, 0x56, 0x78  # id, top bit set
    b[4] = (status & 0x80) | (pressure >> 1)
    b[5] = ((pressure & 1) << 7) | (temperature >> 1)
    b[6] = ((temperature & 1) << 7) | (status & 0x7F)
    b[7] = pressure ^ 0xFF
    b[8] = crc8(bytes(b[:8]), 0x07, 0x80)
    return dmc_frame(bytes(b))


def tg1c_frame() -> list[str]:
    b = bytearray(9)
    b[0], b[1], b[2], b[3] = 0x12, 0x34, 0x56, 0x78  # id
    b[4] = 0x0A
    b[5] = 0xB4  # pressure, 1.38 kPa a step
    b[6] = 22 + 50  # temperature
    b[7] = 0x30  # status
    b[8] = xor_bytes(bytes(b[:8]))
    return mc_frame(bytes(b))


def q85_frame() -> list[str]:
    b = bytearray(12)
    b[0], b[1], b[2], b[3] = 0x12, 0x34, 0x56, 0x78  # id
    b[4] = 0x0A
    b[5] = 0x50  # pressure, 3 kPa a step
    b[6] = 22 + 55  # temperature
    b[7] = 0x30  # status
    b[8] = xor_bytes(bytes(b[:8]))
    b[9] = 0x40
    crc = crc16(bytes(b[:10]), 0x1021, 0xFFFF)
    b[10] = crc & 0xFF
    b[11] = crc >> 8
    return mc_frame(bytes(b))


def jansite_frame() -> list[str]:
    b = bytearray(7)
    b[0], b[1], b[2] = 0x12, 0x34, 0x56
    b[3] = 0x70  # low nibble is flags
    b[4] = 0x8C  # pressure, 1.7 kPa a step
    b[5] = 22 + 50  # temperature
    b[6] = 0x5A
    return mc_frame(bytes(b))


def jansite_solar_frame() -> list[str]:
    c = bytearray(9)
    c[0], c[1], c[2] = 0x12, 0x34, 0x56  # id
    c[3] = 0x08  # flags
    c[4] = 22 + 55  # temperature
    c[5] = 0x96  # pressure, 1.6 kPa a step
    c[6] = 0x00
    crc = crc16(bytes(c[:7]), 0x8005, 0x0000)
    c[7] = crc >> 8
    c[8] = crc & 0xFF
    # The decoder inverts what it has Manchester decoded.
    inverted = bytes((~byte) & 0xFF for byte in c)
    return mc_frame(inverted)


def jansite_ty588_frame() -> list[str]:
    b = bytearray(8)
    b[0] = 0x53
    b[1] = 0x23  # low nibble matches b[0]
    b[2] = 0x40
    b[3] = 0x35
    b[4] = (0x30 - b[3]) & 0xFF
    b[5] = 0x66  # temperature comes from b[2] + b[5]
    b[6] = 0x74  # pressure comes from b[5] + b[6]
    b[7] = b[0]
    return mc_frame(bytes(b))


def porsche_frame() -> list[str]:
    b = bytearray(10)
    b[0], b[1], b[2], b[3] = 0xC1, 0x34, 0x56, 0x78  # id
    b[4] = 0x8C  # pressure, 2.5 kPa a step, offset -100
    b[5] = 22 + 40  # temperature
    b[6], b[7] = 0x00, 0x11  # flags
    crc = crc16(bytes(b[:8]), 0x1021, 0xFFFF)
    b[8] = crc >> 8
    b[9] = crc & 0xFF
    return dmc_frame(bytes(b))


def truck_frame() -> list[str]:
    b = bytearray(9)
    b[0], b[1], b[2], b[3] = 0x12, 0x34, 0x56, 0x78  # id
    b[4] = 0x02  # wheel
    b[5] = 0x30 | 0x03  # flags, then the top nibble of the pressure
    b[6] = 0xE8  # pressure in kPa, 12 bit
    b[7] = 22  # temperature
    b[8] = xor_bytes(bytes(b[:8]))
    # The payload starts four bits into the decoded stream.
    return [manchester([0, 1, 0, 1] + bits_of(bytes(b)))]


def hyundai_vdo_frame() -> list[str]:
    b = bytearray(10)
    b[0] = 0x0F  # state
    b[1], b[2], b[3], b[4] = 0x12, 0x34, 0x56, 0x78  # id
    b[5] = 0xC3  # flags and repeat counter
    b[6] = 0xB4  # pressure, 1.375 kPa a step
    b[7] = 22 + 50  # temperature
    b[8] = 0x2A
    b[9] = crc8(bytes(b[:9]), 0x07, 0xAA)
    return mc_frame(bytes(b))


def elantra2012_frame() -> list[str]:
    b = bytearray(8)
    b[0] = 250 - 60  # pressure in kPa, offset +60
    b[1] = 22 + 50  # temperature
    b[2], b[3], b[4], b[5] = 0x12, 0x34, 0x56, 0x78  # id
    b[6] = 0xC0  # flags: battery ok, not triggered
    b[7] = crc8(bytes(b[:7]), 0x07, 0x00)
    return mc_frame(bytes(b))


def honda_frame() -> list[str]:
    b = bytearray(8)
    b[0] = 175  # pressure, 0.2 PSI a step
    b[1] = 22 + 50  # temperature
    b[2], b[3], b[4], b[5] = 0x12, 0x34, 0x56, 0x78  # id
    b[6] = 0xE1  # flags
    b[7] = crc8(bytes(b[:7]), 0x07, 0x00)
    return mc_frame(bytes(b))


def kia_frame() -> list[str]:
    pressure, temperature, sensor_id = 175, 22 + 50, 0x12345678
    for unknown2 in range(0x1000):
        b = bytearray(9)
        b[0] = (0x0 << 4) | (pressure >> 4)
        b[1] = ((pressure & 0x0F) << 4) | (temperature >> 4)
        b[2] = ((temperature & 0x0F) << 4) | ((sensor_id >> 28) & 0x0F)
        b[3] = (sensor_id >> 20) & 0xFF
        b[4] = (sensor_id >> 12) & 0xFF
        b[5] = (sensor_id >> 4) & 0xFF
        b[6] = ((sensor_id & 0x0F) << 4) | ((unknown2 >> 8) & 0x0F)
        b[7] = unknown2 & 0xFF
        crc = crc8(bytes(b[:8]), 0x07, 0x76)
        # Only the top five bits of the last byte belong to the frame, so
        # the checksum has to come out with its low three bits clear.
        if crc & 0x07:
            continue
        b[8] = crc
        # The frame is 138 Manchester bits: nine bytes and then padding.
        return [manchester(bits_of(bytes(b)) + [0] * (138 - 72))]
    raise RuntimeError("no Kia payload with a fitting checksum")


def sefis_m3_frame() -> list[str]:
    b = bytearray(9)
    b[2] = 0x08  # temperature comes from b[2] + b[5]
    b[4] = (2 << 5) | 0x12  # pressure page 3 and the high bits
    b[5] = 0x00
    crc = crc16(bytes(b[:7]), 0x1021, 0x0000)
    b[7] = crc >> 8
    b[8] = crc & 0xFF
    inverted = bytes((~byte) & 0xFF for byte in b)
    return mc_frame(inverted)


def airpuxem_frame() -> list[str]:
    payload = bytearray(8)
    payload[0], payload[1], payload[2], payload[3] = 0x12, 0x34, 0x56, 0x78  # id
    payload[4] = 0xA1  # flags, wheel position, top bit of the pressure
    payload[5] = 0x5E  # pressure, offset -100, so 350 - 100 = 250 kPa
    payload[6] = 22  # temperature, signed
    payload[7] = 0x96  # battery, 0.02 V a step
    crc = crc8(bytes(payload), 0x2F, 0xAA)

    bits = [0, 1, 0, 1]  # the constant header nibble
    bits += bits_of(bytes(payload))
    bits += bits_of(bytes([crc, 0x00]))
    return [manchester(bits)]


def bmw_g3_frame() -> list[str]:
    b = bytearray(11)
    b[0], b[1], b[2], b[3] = 0x12, 0x34, 0x56, 0x78  # id
    b[4] = 143  # pressure, 2.5 kPa a step, offset -43 steps
    b[5] = 22 + 40  # temperature
    b[6], b[7], b[8] = 0xF8, 0x00, 0x03  # flags
    crc = crc16(bytes(b[:9]), 0x1021, 0x0000)
    b[9] = crc >> 8
    b[10] = crc & 0xFF
    return dmc_frame(bytes(b))

def raw_bits(data: bytes) -> str:
    return "".join(str(bit) for bit in bits_of(data))


def dmc_bits_frame(bits: list[int], prefixes=("", "0", "1")) -> list[str]:
    return [
        prefix + diff_manchester(bits, first) for prefix in prefixes for first in (0, 1)
    ]


def ave_frame() -> list[str]:
    b = bytearray(8)
    b[0], b[1], b[2], b[3] = 0x12, 0x34, 0x56, 0x78  # id
    b[4] = 153  # pressure: mode 0 reads (raw - 47) * 2.352 kPa
    b[5] = 22 + 50  # temperature
    b[6] = 0x00  # mode 0, battery full, no flags
    b[7] = crc8(bytes(b[:7]), 0x31, 0xFF)
    return dmc_frame(bytes(b))


def pmv107j_frame() -> list[str]:
    b = bytearray(9)
    b[0] = 0x02  # only two bits of it are on the air
    b[1], b[2], b[3] = 0x34, 0x56, 0x78
    b[4] = 0x00  # status
    b[5] = 141  # pressure, 2.48 kPa a step, offset -40 steps
    b[6] = b[5] ^ 0xFF
    b[7] = 22 + 40  # temperature
    b[8] = crc8(bytes(b[:8]), 0x13, 0x00)

    # Two bits of b[0], then eight whole bytes, then one spare bit.
    bits = [(b[0] >> 1) & 1, b[0] & 1] + bits_of(bytes(b[1:])) + [0]
    return dmc_bits_frame(bits)


def nissan_checksum(b: bytes) -> int:
    chk = 0
    for i in range(4):
        chk += b[i] >> 7
        chk += b[i] >> 5
        chk += b[i] >> 3
        chk += b[i] >> 1
        chk += (b[i] << 1) & 0xFF
    chk += b[4] >> 7
    chk += b[4] >> 5
    chk += b[4] >> 3
    return ~chk & 0x03


def nissan_frame() -> list[str]:
    mode, sensor_id, pressure_raw = 0x2, 0x0AD779, 0x94  # (raw / 4 - 3) PSI
    for spare in range(32):
        b = bytearray(5)
        b[0] = (mode << 5) | ((sensor_id >> 19) & 0x1F)
        b[1] = (sensor_id >> 11) & 0xFF
        b[2] = (sensor_id >> 3) & 0xFF
        b[3] = ((sensor_id & 0x07) << 5) | ((pressure_raw >> 3) & 0x1F)
        b[4] = ((pressure_raw & 0x07) << 5) | spare
        if nissan_checksum(bytes(b)) == 0:
            # The decoder inverts what it has Manchester decoded.
            inverted = bytes((~byte) & 0xFF for byte in b)
            return [manchester(bits_of(inverted))]
    raise RuntimeError("no Nissan payload with a fitting checksum")


def bmw_frame() -> list[str]:
    b = bytearray(11)
    b[0] = 0x03  # brand: HUF/Beru
    b[1], b[2], b[3], b[4] = 0x12, 0x34, 0x56, 0x78  # id
    b[5] = 102  # pressure, 2.45 kPa a step
    b[6] = 22 + 52  # temperature
    b[7], b[8], b[9] = 0x00, 0x00, 0x00  # flags
    b[10] = crc8(bytes(b[:10]), 0x2F, 0xAA)
    inverted = bytes((~byte) & 0xFF for byte in b)
    return mc_frame(inverted)


def mercedes_benz_frame() -> list[str]:
    b = bytearray(10)
    b[0] = 0x83  # header
    b[1], b[2], b[3], b[4] = 0x12, 0x34, 0x56, 0x78  # id
    b[5] = 100  # pressure, 1/2.75 PSI a step
    b[6] = 22 + 51  # temperature
    b[7], b[8] = 0x05, 0x00  # counter and flags
    b[9] = crc8(bytes(b[:9]), 0x2F, 0xAA)
    # Manchester coded on the wire, so this is already the sliced payload.
    return [raw_bits(bytes(b))]


def steelmate_frame() -> list[str]:
    body = bytearray(6)
    body[0], body[1] = 0x12, 0x34  # id
    body[2] = 80  # pressure, 3.125 kPa a step
    body[3] = 22 + 50  # temperature
    body[4] = 60  # battery: 3.9 V less 10 mV a step
    body[5] = (0x01 + sum(body[:5])) & 0xFF
    # Every byte travels with its bits the other way round; the payload is
    # already what the slicer would hand over.
    reversed_bytes = bytes(int(f"{byte:08b}"[::-1], 2) for byte in body)
    return [raw_bits(reversed_bytes)]




def crc7(data: bytes, poly: int, init: int) -> int:
    remainder = init << 1
    polynomial = poly << 1
    for byte in data:
        remainder ^= byte
        for _ in range(8):
            remainder = ((remainder << 1) ^ polynomial) & 0xFF if remainder & 0x80 \
                else (remainder << 1) & 0xFF
    return (remainder >> 1) & 0x7F


def trw_frame() -> list[str]:
    b = bytearray(10)
    b[0] = 0x5C  # mode
    b[1], b[2], b[3], b[4] = 0x12, 0x34, 0x56, 0x78  # id
    b[5] = 0xB1  # flags and sequence number
    b[6] = 91  # pressure, 0.4 PSI a step
    b[7] = 22 + 50  # temperature
    b[8] = 0x0E  # parked
    b[9] = crc8(bytes(b[:9]), 0x07, 0x00)
    return [raw_bits(bytes(b)) + "00"]


def schrader_frame() -> list[str]:
    b = bytearray(8)
    b[0] = 0x30  # flags
    b[1], b[2], b[3], b[4] = 0x12, 0x34, 0x56, 0x78  # id
    b[5] = 100  # pressure, 25 mbar a step
    b[6] = 22 + 50  # temperature
    b[7] = crc8(bytes(b[:7]), 0x07, 0xF0)
    return ["010" + raw_bits(bytes(b))]


def schrader_eg53ma4_frame() -> list[str]:
    b = bytearray(10)
    b[0], b[1], b[2], b[3] = 0x01, 0x02, 0x03, 0x04  # preamble and flags
    b[4], b[5], b[6] = 0x12, 0x34, 0x56  # id
    b[7] = 100  # pressure, 25 mbar a step
    b[8] = 72  # temperature in degrees Fahrenheit
    b[9] = sum(b[:9]) & 0xFF
    return ["0" * 39 + raw_bits(bytes(b))]


def schrader_smd3ma4_frame() -> list[str]:
    flags, sensor_id, pressure = 0x7, 0x123456, 175  # 0.2 PSI a step
    for check in range(4):
        b = bytearray(5)
        b[0] = 0x80 | (flags << 4) | ((sensor_id >> 20) & 0x0F)
        b[1] = (sensor_id >> 12) & 0xFF
        b[2] = (sensor_id >> 4) & 0xFF
        b[3] = ((sensor_id & 0x0F) << 4) | ((pressure >> 4) & 0x0F)
        b[4] = ((pressure & 0x0F) << 4) | (check << 2)
        total = 0
        for byte in b:
            total += (byte & 3) + ((byte >> 2) & 3) + ((byte >> 4) & 3) + ((byte >> 6) & 3)
        if (total & 3) == 1:
            bits = [int(bit) for bit in raw_bits(bytes(b))[:38]]
            return [manchester([1 - bit for bit in bits])]
    raise RuntimeError("no Schrader SMD3MA4 payload with a fitting checksum")


def schrader_mrxbc5a4_frame() -> list[str]:
    flags, sensor_id, pressure, temperature = 0x7, 0x123456, 250, 22 + 50
    for check in range(4):
        b = bytearray(6)
        b[0] = (flags << 5) | ((sensor_id >> 19) & 0x1F)
        b[1] = (sensor_id >> 11) & 0xFF
        b[2] = (sensor_id >> 3) & 0xFF
        b[3] = ((sensor_id & 0x07) << 5) | ((pressure >> 4) & 0x1F)
        b[4] = ((pressure & 0x0F) << 4) | (check << 2) | ((temperature >> 6) & 0x03)
        b[5] = (temperature << 3) & 0xFF
        bits = raw_bits(bytes(b))
        ones = sum(1 for i in range(3, 38) if bits[i] == "1")
        even_ones = sum(1 for i in range(3, 38) if bits[i] == "1" and (i - 3) % 2 == 0)
        if ((even_ones + 2 * ones - 1) & 3) == check:
            return [bits[:45]]
    raise RuntimeError("no Schrader MRXBC5A4 payload with a fitting check")


def schrader_motorcycle_frame() -> list[str]:
    sensor_id, pressure = 0x123456, 500  # 0.5 kPa a step
    b = bytearray(7)
    b[0] = (sensor_id >> 22) & 0x03
    b[1] = (sensor_id >> 14) & 0xFF
    b[2] = (sensor_id >> 6) & 0xFF
    b[3] = ((sensor_id & 0x3F) << 2) | ((pressure >> 8) & 0x03)
    b[4] = pressure & 0xFF
    b[5] = 22 + 50  # temperature
    b[6] = crc8(bytes(b[:6]), 0x07, 0xE0)
    return [raw_bits(bytes(b))]


def gm_frame() -> list[str]:
    b = bytearray(10)
    b[0], b[1] = 0x4C, 0x90  # flags
    b[2], b[3] = 0x00, 0x78  # device type and the top of the id
    b[4], b[5], b[6] = 0x49, 0x17, 0x66  # id
    b[7] = 91  # pressure, 2.75 kPa a step
    b[8] = 22 + 60  # temperature
    b[9] = sum(b[:9]) & 0xFF
    return [raw_bits(bytes(b)) + "00"]


def gear_hive_frame() -> list[str]:
    p = bytearray(9)
    p[0], p[1] = 0x34, 0x12  # counter and sensor class 2
    p[2], p[3], p[4] = 0x12, 0x34, 0x56  # id
    sensor_class = p[1] & 0x0F
    base = (80 + sensor_class * 64) & 0xFF
    p[5] = (base + 40) & 0xFF  # 40 steps of 6.25 kPa
    p[6] = 0x20 | 0x01  # fixed flags, and the top bits of the temperature
    p[7] = 0x35 | (1 << 6)
    p[8] = 0x00
    raw = bytearray(9)
    raw[0] = p[0] ^ 0x94
    for i in range(1, 9):
        raw[i] = raw[i - 1] ^ p[i]
    return [raw_bits(bytes(raw))]


def tyreguard400_frame() -> list[str]:
    b = bytearray(11)
    b[0], b[1], b[2] = 0xFD, 0x5F, 0xD5  # the sync word, covered by the CRC
    b[3] = 0xF1  # sync nibble and the top of the id
    b[4], b[5], b[6] = 0x23, 0x45, 0x67  # id
    b[7] = 250  # pressure in kPa
    b[8] = 22 + 40  # temperature
    b[9] = 0x00  # flags, including the top bits of the pressure
    b[10] = crc8(bytes(b[:10]), 0x31, 0xDD)
    return [raw_bits(bytes(b))[28:]]


def smartire_frame() -> list[str]:
    for tail in range(256):
        b = bytearray(6)
        b[0] = 40 + 100  # pressure, 2.5 kPa a step, offset -40
        b[1] = (0 << 6) | 0x12  # message type 0, top of the id
        b[2], b[3] = 0x34, 0x56  # id
        b[4] = 0x00  # flags
        b[5] = tail
        if crc7(bytes(b), 0x45, 0x6F) == 0:
            return dmc_frame(bytes(b))
    raise RuntimeError("no SmarTire payload with a fitting CRC")


def eezrv_frame() -> list[str]:
    b = bytearray(7)
    b[0], b[1], b[2] = 0x0D, 0x17, 0x7E  # id
    b[3] = 100  # pressure, 2.5 kPa a step
    b[4] = 22 + 50  # temperature
    b[5] = 0x10  # flags
    b[6] = 0x00
    checksum = sum(b)
    if checksum > 0xFF:
        checksum |= 0x80
    return [raw_bits(bytes([checksum & 0xFF])) + raw_bits(bytes(b))]


def sp372_frame(checksum: int) -> list[str]:
    b = bytearray(8)
    b[0] = 0x53
    b[1] = 0x23  # low nibble matches b[0]
    b[2] = 0x40
    b[3] = 0x35
    b[4] = (checksum - b[3]) & 0xFF
    b[5] = 0x66
    b[6] = 0x74
    b[7] = b[0]
    inverted = bytes((~byte) & 0xFF for byte in b)
    return mc_frame(inverted)


def jansite_ty468_frame() -> list[str]:
    return sp372_frame(0xFB)


def imars_t240_frame() -> list[str]:
    return sp372_frame(0x41)


# Some drivers pick a variant by the length of the whole row — the VDO
# TG1C is told from the EGQ Q85 that way — so the trailing filler is part
# of the frame description rather than a constant.
PROTOCOLS = [
    # id in the firmware table, rtl_433 number, chip us, sync word, builder,
    # trailing filler chips
    ("renault", 90, 52, "10101010101010101001", renault_frame, FILLER),
    ("renault0435r", 212, 52, "10101010101010101001", renault_0435r_frame, FILLER),
    ("citroen", 82, 52, "10101010101010101001", citroen_frame, FILLER),
    ("ford", 89, 52, "10101010101010101001", ford_frame, FILLER),
    ("toyota", 88, 52, "10101001111", toyota_frame, FILLER),
    ("vdo_tg1c", 156, 52, "101010101010101010101001", tg1c_frame, "01" * 8),
    ("egq_q85", 156, 52, "101010101010101010101001", q85_frame, FILLER),
    ("jansite", 123, 52, "10101010101010101010101010101001", jansite_frame, FILLER),
    ("jansite_solar", 180, 50, "10100110101001100101101001011010", jansite_solar_frame, FILLER),
    ("jansite_ty588", 362, 50,
     "10011001101010100101101001101010100110101010", jansite_ty588_frame, FILLER),
    ("porsche", 203, 52, "00110011001100110010", porsche_frame, FILLER),
    ("truck", 201, 52, "101010101010101010101001", truck_frame, FILLER),
    ("hyundai_vdo", 186, 52, "10101010101010101010101010101001", hyundai_vdo_frame, FILLER),
    ("elantra2012", 140, 50, "0111000101010101", elantra2012_frame, FILLER),
    ("honda", 381, 50, "11011010111000110101010", honda_frame, FILLER),
    ("kia", 226, 50, "1110110101110001", kia_frame, FILLER),
    ("sefis_m3", 378, 52, "01100110100110011001011010100110", sefis_m3_frame, FILLER),
    ("airpuxem", 295, 52, "101010101010101010101001", airpuxem_frame, FILLER),
    ("bmw_g3", 257, 52, "1100110011001101", bmw_g3_frame, FILLER),
    ("ave", 208, 100, "11001100110011001100110011001101", ave_frame, FILLER),
    ("pmv107j", 110, 100, "1111110", pmv107j_frame, FILLER),
    ("nissan", 248, 120,
     "111101010101010101010101010101011110", nissan_frame, FILLER),
    ("bmw", 252, 25, "1010101001011001", bmw_frame, FILLER),
    ("mercedes_benz", 365, 25, "000000000010", mercedes_benz_frame, FILLER),
    ("steelmate", 59, 50, "111111111111111110000000", steelmate_frame, FILLER),
    ("trw_fsk", 299, 52, "0111111111111111", trw_frame, ""),
    ("trw_ook", 298, 52, "0000000000000001", trw_frame, ""),
    ("schrader", 60, 120, "", schrader_frame, ""),
    ("schrader_eg53", 95, 120, "", schrader_eg53ma4_frame, ""),
    ("schrader_smd3", 168, 120,
     "1111010101010101010101010101010111", schrader_smd3ma4_frame, ""),
    ("schrader_bmw", 328, 120, "0111111111111111", schrader_mrxbc5a4_frame, ""),
    ("schrader_moto", 321, 120, "0111111111111", schrader_motorcycle_frame, ""),
    ("gm", 275, 120, "0" * 48, gm_frame, ""),
    ("gear_hive", 322, 120, "0010010110010100", gear_hive_frame, FILLER),
    ("tyreguard400", 225, 100, "1111110101011111110101011111", tyreguard400_frame, ""),
    ("smartire", 343, 167, "0011001010110100", smartire_frame, FILLER),
    ("eezrv", 241, 50, "1111111111111111", eezrv_frame, FILLER),
    ("jansite_ty468", 355, 50, "10101010101010101010101010101010",
     jansite_ty468_frame, FILLER),
    ("imars_t240", 354, 50, "10101010101010101010101010101010",
     imars_t240_frame, FILLER),
]




# Protocols whose modulation is MANCHESTER_ZEROBIT: rtl_433 describes
# them in bits its slicer has already decoded, so the frame has to be put
# back into chips before the firmware decoder sees it.
SLICERS = {
    "mercedes_benz": "mc",
    "steelmate": "mc",
    "trw_fsk": "mc",
    "trw_ook": "mc",
    "schrader": "mc",
    "schrader_eg53": "mc",
    "schrader_bmw": "mc",
    "schrader_moto": "mc",
    "gm": "mc",
    "gear_hive": "mc",
    "tyreguard400": "mc",
    "eezrv": "mc",
}

# rtl_433 sees rows that begin with the zero bit its Manchester slicer
# always prepends. Protocols found by row length rather than by a sync
# word need that bit in front of the frame to line up.
RTL_PREFIX = {"schrader": "0", "schrader_eg53": "0"}

# The Honda decoder insists on finding its marker at the very start of the
# row, and the Steelmate one on a row of exactly 72 bits, so those two
# frames get no lead and no filler.
LEADS = {
    "honda": "",
    "steelmate": "",
    "trw_fsk": "",
    "trw_ook": "",
    "schrader": "",
    "schrader_eg53": "",
    "schrader_bmw": "",
    "schrader_moto": "",
    "gm": "",
    "tyreguard400": "",
    # These carry their whole preamble in the sync word: any lead in front
    # would shift where rtl_433 starts decoding.
    "schrader_smd3": "",
    "eezrv": "",
    "jansite_ty468": "",
    "imars_t240": "",
    "gear_hive": "0" * 16,
}
FILLERS = {"steelmate": ""}


def lead_for(sync: str) -> str:
    """Chips in front of the sync word: more of the same preamble.

    Repeating the first eight chips keeps the run lengths a real preamble
    has. A lead of, say, sixteen zeros in front of a preamble that starts
    with zeros would merge into one very long pulse, which no decoder —
    ours or rtl_433's — treats as part of a frame.
    """
    return sync[:8] * 2


def run_rtl433(number: int, chips: str) -> dict | None:
    """Decode a chip stream with rtl_433, trying both polarities."""
    for stream in (chips, invert(chips)):
        result = subprocess.run(
            [RTL433, "-R", str(number), "-F", "json", "-y", to_code(stream)],
            capture_output=True,
            text=True,
        )
        for line in result.stdout.splitlines():
            line = line.strip()
            if line.startswith("{"):
                return json.loads(line)
    return None


def expected_from(report: dict) -> tuple[int, int, int, bool]:
    """rtl_433's report in the units the firmware uses."""
    raw_id = report.get("id", 0)
    sensor_id = int(str(raw_id), 16) if isinstance(raw_id, str) else int(raw_id)

    if "pressure_kPa" in report:
        pressure = int(round(float(report["pressure_kPa"]) * 100))
    elif "pressure_PSI" in report:
        pressure = int(round(float(report["pressure_PSI"]) * 689.476))
    else:
        pressure = 0

    has_temp = "temperature_C" in report
    temperature = int(round(float(report["temperature_C"]))) if has_temp else 0
    return sensor_id, pressure, temperature, has_temp


def main() -> int:
    if not RTL433:
        print("rtl_433 not found: set RTL433 to its path", file=sys.stderr)
        return 1

    rows = []
    for name, number, chip_us, sync, builder, filler in PROTOCOLS:
        chosen = None
        for payload in builder():
            stream = (LEADS.get(name, lead_for(sync)) + sync + payload +
                      FILLERS.get(name, filler))
            report = run_rtl433(number, RTL_PREFIX.get(name, "") + stream)
            if report:
                # rtl_433 works in sliced bits; the firmware decoder gets
                # what would actually be on the air.
                chips = stream if SLICERS.get(name, "nrz") == "nrz" else slicer_encode(stream)
                chosen = (chips, report)
                break

        if not chosen:
            print(f"FAIL {name}: rtl_433 did not decode the generated frame", file=sys.stderr)
            return 1

        chips, report = chosen
        sensor_id, pressure, temperature, has_temp = expected_from(report)
        rows.append((name, chip_us, chips, sensor_id, pressure, temperature, has_temp))
        print(f"ok  {name:14s} {report.get('model', '')}")

    out = ['/* Generated by gen_vectors.py — do not edit.',
           ' *',
           ' * One synthetic frame per protocol. The bits are what would come off',
           ' * the air; the expected values are what rtl_433 reports for those',
           ' * very bits. */',
           '#pragma once',
           '',
           '#include <stdbool.h>',
           '#include <stdint.h>',
           '',
           'typedef struct {',
           '    const char* protocol;',
           '    uint16_t chip_us;',
           '    const char* chips;',
           '    uint32_t id;',
           '    int32_t pressure_kpa_x100;',
           '    int16_t temperature_c;',
           '    bool has_temperature;',
           '} TpmsTestVector;',
           '',
           'static const TpmsTestVector tpms_test_vectors[] = {']

    for name, chip_us, chips, sensor_id, pressure, temperature, has_temp in rows:
        out.append('    {')
        out.append(f'        .protocol = "{name}",')
        out.append(f'        .chip_us = {chip_us},')
        out.append(f'        .chips = "{chips}",')
        out.append(f'        .id = 0x{sensor_id:x},')
        out.append(f'        .pressure_kpa_x100 = {pressure},')
        out.append(f'        .temperature_c = {temperature},')
        out.append(f'        .has_temperature = {"true" if has_temp else "false"},')
        out.append('    },')

    out.append('};')
    out.append('')
    out.append('#define TPMS_TEST_VECTOR_COUNT '
               '(sizeof(tpms_test_vectors) / sizeof(tpms_test_vectors[0]))')
    out.append('')

    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "vectors.h")
    with open(path, "w") as handle:
        handle.write("\n".join(out))
    print(f"\nwrote {path}: {len(rows)} vectors")
    return 0


if __name__ == "__main__":
    sys.exit(main())

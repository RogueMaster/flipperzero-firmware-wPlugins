#!/usr/bin/env python3

import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44100
AMP = 0.6
MODEM = "1200bd"
MERGE_SAME_LEVEL_SEGMENTS = False
MODEMS = {
    "1200bd": (1200.0, 1200.0, 2200.0),
    "300bd": (300.0, 1600.0, 1800.0),
}

BAUD, MARK, SPACE = MODEMS[MODEM]
BIT_TIME = 1.0 / BAUD
PREAMBLE_MS = 50.0

BASE_DIR = Path(__file__).resolve().parent

SRC_CALL = "YO0FLP"
DST_CALL = "W0RLD"

MSG1 = "TEST TEST 123"
MSG2 = "HELLO WORLD"
MSG3 = "Hello world, I am Flipper Zero :D"


def crc16_x25(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0x8408
            else:
                crc >>= 1
    return crc ^ 0xFFFF


def makeLSB(data: bytes):
    for b in data:
        for i in range(8):
            yield (b >> i) & 1


def _bit_stuff(bits):
    c = 0
    for bit in bits:
        yield bit
        if bit:
            c += 1
            if c == 5:
                yield 0
                c = 0
        else:
            c = 0


def ax25_addr(call: str, ssid: int, last: bool) -> bytes:
    call = call.upper().ljust(6)
    out = bytearray((ord(c) << 1) & 0xFE for c in call[:6])
    out.append(0x60 | ((ssid & 0x0F) << 1) | (1 if last else 0))
    return bytes(out)


def mkHeader(dest: str, ssid: int = 0, qso: int = 0, cmd: bool = False) -> bytes:
    dest = dest.upper().ljust(6)
    vals = [(ord(c) - 0x20) & 0x3F for c in dest[:6]]

    b0 = (qso >> 6) & 0xFF
    b1 = ((qso & 0x3F) << 2)        | (2 if cmd else 0) | 1
    b2 = ((vals[0] & 0x3F) << 2)    | ((vals[1] >> 4) & 0x03)
    b3 = ((vals[1] & 0x0F) << 4)    | ((vals[2] >> 2) & 0x0F)
    b4 = ((vals[2] & 0x03) << 6)    | (vals[3] &   0x3F)
    b5 = ((vals[4] & 0x3F) << 2)    | ((vals[5] >> 4)  & 0x03)
    b6 = ((vals[5] & 0x0F) << 4)    | (ssid & 0x0F)

    return bytes([b0, b1, b2, b3, b4, b5, b6])


def append_segment(segments, level: bool, duration_us: float):
    if duration_us <= 1e-9:
        return

    if MERGE_SAME_LEVEL_SEGMENTS and segments and segments[-1][0] == level:
        segments[-1][1] += duration_us
    else:
        segments.append([level, duration_us])


def edge_durations_us_from_bits(bits):
    freq = MARK
    level = True
    bit_time_us = BIT_TIME * 1000000.0
    segments = []

    for bit in bits:
        if bit == 0:
            freq = SPACE if freq == MARK else MARK

        half_period_us = 1000000.0 / (2.0 * freq)
        elapsed_us = 0.0

        while elapsed_us + half_period_us < bit_time_us - 1e-9:
            append_segment(segments, level, half_period_us)
            level = not level
            elapsed_us += half_period_us

        append_segment(segments, level, bit_time_us - elapsed_us)

    out = []
    carry_us = 0.0

    for _, duration_us in segments:
        rounded_us = int(round(duration_us + carry_us))
        carry_us += duration_us - rounded_us
        if rounded_us <= 0:
            continue
        out.append(rounded_us)

    return out


def waveform_from_durations_us(
    durations_us,
    start_level: bool,
    lead_silence: float = 0.25,
    tail_silence: float = 0.25,
):
    samples = []
    level = not start_level
    time_s = 0.0

    samples.extend([0.0] * int(lead_silence * SAMPLE_RATE))

    for duration_us in durations_us:
        next_time_s = time_s + (duration_us / 1000000.0)
        n0 = round(time_s * SAMPLE_RATE)
        n1 = round(next_time_s * SAMPLE_RATE)
        n = max(1, n1 - n0)
        samples.extend(([AMP] if level else [-AMP]) * n)
        level = not level
        time_s = next_time_s

    samples.extend([0.0] * int(tail_silence * SAMPLE_RATE))
    return samples


def write_wav(path: Path, samples):
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)

        data = bytearray()
        for sample in samples:
            sample = max(-1.0, min(1.0, sample))
            data += struct.pack("<h", int(sample * 32767))

        wf.writeframes(data)


def build_frame(body: bytes) -> bytes:
    fcs = crc16_x25(body)
    return body + bytes([fcs & 0xFF, (fcs >> 8) & 0xFF])


def c_array_text(name: str, body: bytes, bits, durations_us):
    frame = build_frame(body)
    hex_frame = ", ".join(f"0x{b:02X}" for b in frame)
    hex_body = ", ".join(f"0x{b:02X}" for b in body)

    dur_lines = []
    for i in range(0, len(durations_us), 16):
        chunk = durations_us[i:i + 16]
        dur_lines.append("    " + ", ".join(str(x) for x in chunk) + ",")

    return (
        f"/* {name} */\n"
        f"/* src={SRC_CALL} dst={DST_CALL} */\n"
        f"static const uint8_t {name}_frame[] = {{ {hex_frame} }};\n"
        f"static const uint8_t {name}_body[] = {{ {hex_body} }};\n"
        f"static const uint16_t {name}_durations_us[] = {{\n" + "\n".join(dur_lines) + "\n};\n"
        f"static const uint32_t {name}_durations_count = {len(durations_us)};\n"
        f"static const uint32_t {name}_bits_count = {len(bits)};\n"
        f"static const bool {name}_start_level = false;\n"
    )


def write_afsk(path: Path, txt_path: Path, array_name: str, body: bytes):
    frame = build_frame(body)
    flags_pre = bytes([0x7E] * 50)
    flags_post = bytes([0x7E] * 3)
    preamble_bits = [1] * int(round((PREAMBLE_MS / 1000.0) / BIT_TIME))

    bits = list(preamble_bits)
    bits += list(makeLSB(flags_pre))
    bits += list(_bit_stuff(makeLSB(frame)))
    bits += list(makeLSB(flags_post))
    durations_us = edge_durations_us_from_bits(bits)
    write_wav(path, waveform_from_durations_us(durations_us, False))
    txt_path.write_text(c_array_text(array_name, body, bits, durations_us), encoding="ascii")


def main():
    out1 = BASE_DIR / "1_code_minimum.wav"
    out2 = BASE_DIR / "2_ax25_minimum.wav"
    out3 = BASE_DIR / "3_aprs_packet.wav"
    txt1 = BASE_DIR / "1_code_minimum.txt"
    txt2 = BASE_DIR / "2_ax25_minimum.txt"
    txt3 = BASE_DIR / "3_aprs_packet.txt"
    msgfile = BASE_DIR / "messages.txt"

    body1 = bytearray()
    body1 += mkHeader(DST_CALL, 0, 0, False)
    body1 += bytes([0x03, 0xF0])
    body1 += MSG1.encode("ascii")
    write_afsk(out1, txt1, "packet1_code_minimum", bytes(body1))

    body2 = bytearray()
    body2 += ax25_addr(DST_CALL, 0, False)
    body2 += ax25_addr(SRC_CALL, 0, True)
    body2 += bytes([0x03, 0xF0])
    body2 += MSG2.encode("ascii")
    write_afsk(out2, txt2, "packet2_ax25_minimum", bytes(body2))

    body3 = bytearray()
    body3 += ax25_addr(DST_CALL, 0, False)
    body3 += ax25_addr(SRC_CALL, 0, True)
    body3 += bytes([0x03, 0xF0])
    body3 += b">" + MSG3.encode("ascii")
    write_afsk(out3, txt3, "packet3_aprs_packet", bytes(body3))

    msgfile.write_text( MSG1 + "\n" + MSG2 + "\n" + MSG3 + "\n", encoding="ascii",)

    print(out1)
    print(txt1)

    print(out2)
    print(txt2)
    
    print(out3)
    print(txt3)
    print(msgfile)


if __name__ == "__main__":
    main()

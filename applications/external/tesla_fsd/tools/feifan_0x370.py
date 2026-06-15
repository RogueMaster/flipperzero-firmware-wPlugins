#!/usr/bin/env python3
"""
feifan_0x370.py — reference for the 0x370 EPAS3P_sysStatus nag-suppression scheme.

Decoded from a 22,598-frame candump of a working in-the-wild commander
(V4.1.00) captured on X179 Bus 6 (pin 13/14) of a HW4 car while it was
successfully suppressing the steering-nag during FSD. Contributed by
@DrStrangeglovebox in issue #100.

The headline finding — and the reason a naive nag killer trips Tesla's 2026.14.x
preflight while this scheme does not — is that the working commander does NOT
flip a "hands on" flag. It transmits a *realistic steering torque* and lets the
car infer hands-on, while reproducing the genuine EPAS frame's rolling counter
and checksum exactly. The frame looks like EPAS, so the content check passes.

Frame layout observed (8 bytes, ~100 Hz):

    byte0   status nibble, alternates 0x11 / 0x12 (low nibble 1/2)
    byte1   torsion-bar torque, signed, random-walks around 0 (~ -5..+2)
    byte2   slow status, mostly 0x07 / 0x08
    byte3   second torque channel, signed, random-walks around 0
    byte4   bits[7:6] = handsOnLevel — LEFT AT 0 (never forced high);
            low bits a slowly-varying value around 0x1F-0x21
    byte5   high-entropy low byte (likely the fast byte of a 16-bit angle)
    byte6   rolling counter, low nibble 0x0..0xF, +1 per frame (high nibble 0)
    byte7   checksum

CHECKSUM (validated 22,598 / 22,598 frames, 100%):

    byte7 = (sum(byte0..byte6) + 0x73) & 0xFF

The additive constant 0x73 = 0x70 + 0x03, i.e. the low and high bytes of the
message id 0x370 — the classic Tesla "sum of payload + id bytes" checksum. Note
it is ARITHMETIC-additive, not XOR: changing only the counter by +1 raises the
checksum by +1 (a true CRC8 would not).

This module is the reference encoder for the v2.17 nag-killer redesign. Run with
a candump path to re-validate the checksum against a capture.

SAFETY: injecting steering torque during FSD is dangerous. A bad disengagement
on these cars has been reported to trip the efuses and disable drive, regen and
power steering until serviced (@DrStrangeglovebox, #100). Test only in a safe,
empty area with hands ready. This file is a research reference, not a
ready-to-run injector.
"""

import sys

MSG_ID = 0x370
CHECKSUM_CONST = (MSG_ID & 0xFF) + ((MSG_ID >> 8) & 0xFF)  # 0x70 + 0x03 = 0x73


def feifan_checksum(data7):
    """Tesla sum checksum for 0x370: (sum(byte0..byte6) + id_lo + id_hi) & 0xFF."""
    if len(data7) != 7:
        raise ValueError("expected the 7 payload bytes (byte0..byte6)")
    return (sum(data7) + CHECKSUM_CONST) & 0xFF


def build_0x370(counter, torque1=0x00, torque2=0x00,
                byte0=0x11, byte2=0x08, byte4=0x1F, byte5=0x00):
    """Build a full 8-byte 0x370 frame with a valid counter + checksum.

    counter: 0..15 (placed in byte6 low nibble)
    torque1/torque2: signed-ish small torque values for byte1/byte3
    handsOnLevel (byte4 bits[7:6]) is deliberately left at 0.
    """
    b = [
        byte0 & 0xFF,
        torque1 & 0xFF,
        byte2 & 0xFF,
        torque2 & 0xFF,
        byte4 & 0x3F,            # keep bits[7:6]=0 -> handsOnLevel stays 0
        byte5 & 0xFF,
        counter & 0x0F,
    ]
    b.append(feifan_checksum(b))
    return bytes(b)


def _parse_dump(path):
    """Yield 8-byte payloads of 0x370 from a candump-style log."""
    for line in open(path):
        if "370#" not in line:
            continue
        hexpart = line.split("370#", 1)[1].strip()[:16]
        if len(hexpart) < 16:
            continue
        try:
            yield bytes(int(hexpart[i:i + 2], 16) for i in range(0, 16, 2))
        except ValueError:
            continue


def _validate(path):
    total = ok = 0
    counter_steps_ok = 0
    prev_counter = None
    handsonlevel_nonzero = 0
    for f in _parse_dump(path):
        total += 1
        if feifan_checksum(list(f[:7])) == f[7]:
            ok += 1
        c = f[6] & 0x0F
        if prev_counter is not None and c == (prev_counter + 1) & 0x0F:
            counter_steps_ok += 1
        prev_counter = c
        if (f[4] >> 6) & 0x03:
            handsonlevel_nonzero += 1
    if total == 0:
        print("no 0x370 frames found in", path)
        return 1
    print(f"frames:            {total}")
    print(f"checksum matches:  {ok}/{total} ({100*ok/total:.1f}%)")
    print(f"counter +1 steps:  {counter_steps_ok}/{total-1}")
    print(f"handsOnLevel != 0: {handsonlevel_nonzero} "
          f"({'EPAS-style, flag never forced' if handsonlevel_nonzero == 0 else 'flag is being set'})")
    return 0 if ok == total else 2


if __name__ == "__main__":
    if len(sys.argv) == 2:
        sys.exit(_validate(sys.argv[1]))
    # No dump given: self-demo the encoder.
    print("Feifan 0x370 reference encoder (no dump given — demo mode)\n")
    print(f"  checksum constant = 0x{CHECKSUM_CONST:02X} (id 0x370: 0x70+0x03)\n")
    for ctr in range(4):
        frame = build_0x370(ctr, torque1=0xFE, torque2=0xF7,
                            byte4=0x1F, byte5=0xA9)
        print(f"  counter={ctr:X}: " + frame.hex().upper())
    print("\nUsage: feifan_0x370.py <candump.log>   # validate checksum on a capture")

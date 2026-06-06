#!/usr/bin/env python3
"""
Brute-force the checksum algorithm for a counter+CRC-protected Tesla CAN frame.

Newer Tesla frames (e.g. 0x485 PM_locState, 0x229 SCCM_rightStalk) carry a
rolling counter plus a checksum byte. To inject a frame the car will accept you
must reproduce that checksum, but Tesla doesn't publish the parameters. This
tool recovers them from a capture of real frames: feed it consecutive frames
for one CAN ID (the counter cycling, the checksum byte changing with it) and it
finds every (algorithm, polynomial, init, xorout, reflection, covered-byte
range) combination that reproduces the observed checksum on ALL frames.

It tries, in order:
  1. Additive checksum  (sum(bytes) + optional CAN-ID fold) & 0xFF
     — what 0x370 / 0x3FD / 0x399 use; our handlers already do this.
  2. CRC8 over a parameter sweep (poly × refin × refout) across every covered-
     byte SUBSET, optionally folding the CAN ID into the input. It tries every
     subset of the non-checksum bytes because real Tesla CRCs often cover only
     some bytes (e.g. excluding a free-running counter/timestamp). init and
     xorout are recovered together as one constant (they collapse for the
     fixed-length frames seen here).

The more distinct frames you give it, the fewer false matches survive. ~12+
frames usually pins a unique parameter set.

Usage:
    # candump log (SocketCAN): "(1700000000.000) can0 485#00112233445566AA"
    python3 tools/tesla_crc_cracker.py --id 0x485 capture.log

    # plain hex, one 8-byte frame per line: "00 11 22 33 44 55 66 AA"
    python3 tools/tesla_crc_cracker.py --id 0x485 --checksum-byte 7 frames.txt

    # the CRC's covered byte range is discovered automatically (every subset is
    # swept), so you don't need to identify the counter/payload split yourself.
    #
    # Capture tip: grab the frame from the DIRECT vehicle bus (e.g. Tesla X179
    # pin 9/10 or OBD-II), not a gateway-forwarded subset bus — forwarded frames
    # may be re-emitted without the original counter/CRC.

On success it prints the matching parameters plus ready-to-paste C and Python
implementations so the result drops straight into a firmware handler.

No dependencies — pure stdlib.
"""

import argparse
import itertools
import re
import sys
from collections import Counter


# ── frame parsing ──────────────────────────────────────────────────────────────

_CANDUMP = re.compile(r"([0-9A-Fa-f]{1,8})#([0-9A-Fa-f]*)")


def parse_frames(path, want_id):
    """Return list of 8-byte tuples for frames matching want_id.

    Accepts candump (`ID#DATA`) lines or plain whitespace/comma-separated hex
    bytes (one frame per line). Lines that don't parse are skipped.
    """
    frames = []
    with open(path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue

            m = _CANDUMP.search(line)
            if m:
                fid = int(m.group(1), 16)
                if fid != want_id:
                    continue
                data = bytes.fromhex(m.group(2))
            else:
                # plain hex bytes, no ID column → assume every line is the frame
                toks = re.split(r"[\s,]+", line)
                try:
                    data = bytes(int(t, 16) for t in toks if t)
                except ValueError:
                    continue

            if len(data) >= 1:
                frames.append(tuple(data))
    return frames


# ── checksum algorithms ────────────────────────────────────────────────────────


def rev8(b):
    b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4)
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2)
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1)
    return b


def crc8(data, poly, init, xorout, refin, refout):
    """Generic bitwise CRC8."""
    crc = init
    for byte in data:
        b = rev8(byte) if refin else byte
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    if refout:
        crc = rev8(crc)
    return crc ^ xorout


def crc8_raw(data, poly, refin, refout):
    """CRC8 with init=0 and xorout=0 — the building block for the affine subset
    search below."""
    crc = 0
    for byte in data:
        b = rev8(byte) if refin else byte
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    return rev8(crc) if refout else crc


def covered_input(frame, csum_idx, id_bytes):
    """Bytes the checksum is computed over: every data byte except the checksum
    byte itself, optionally prefixed with the CAN-ID bytes (Tesla folds the ID
    into several frame checksums)."""
    body = [b for i, b in enumerate(frame) if i != csum_idx]
    return bytes(list(id_bytes) + body)


# ── crackers ───────────────────────────────────────────────────────────────────


def try_additive(frames, csum_idx, can_id):
    """sum(body) [+ id_lo + id_hi] + const, & 0xFF."""
    hits = []
    id_lo, id_hi = can_id & 0xFF, (can_id >> 8) & 0xFF
    for fold in ("none", "id"):
        for const in range(256):
            ok = True
            for f in frames:
                body = sum(b for i, b in enumerate(f) if i != csum_idx)
                if fold == "id":
                    body += id_lo + id_hi
                if (body + const) & 0xFF != f[csum_idx]:
                    ok = False
                    break
            if ok:
                hits.append(("additive", {"fold_id": fold == "id", "const": const}))
    return hits


def try_crc8(frames, csum_idx, can_id):
    """Find CRC8 parameters over any byte SUBSET of the frame.

    Real Tesla CRCs often cover only some bytes (e.g. excluding a free-running
    counter or timestamp), so this sweeps every non-empty subset of the
    non-checksum bytes — not just "all of them," which is what trips up a naive
    cracker. For a fixed covered length the init and xorout collapse into one
    constant XOR, so per (subset, poly, refin, refout, id-prefix) we compute the
    raw CRC (init=0) and require (raw XOR checksum) to be the SAME constant K on
    every frame. The match is reported as init=0, xorout=K (one valid
    parameterization; init folds into K). Requires uniform-length frames (the
    caller enforces it).
    """
    hits = []
    id_lo, id_hi = can_id & 0xFF, (can_id >> 8) & 0xFF
    id_prefix_opts = {"none": (), "id_lo": (id_lo,), "id_lo_hi": (id_lo, id_hi)}
    n = len(frames[0])
    positions = [i for i in range(n) if i != csum_idx]
    targets = [f[csum_idx] for f in frames]

    # Larger subsets first: a CRC covering more bytes is far less likely to be a
    # spurious match than a 1-byte "cover."
    for r in range(len(positions), 0, -1):
        for sub in itertools.combinations(positions, r):
            for prefix_name, prefix in id_prefix_opts.items():
                inputs = [bytes(list(prefix) + [f[i] for i in sub]) for f in frames]
                for poly in range(256):
                    for refin in (False, True):
                        for refout in (False, True):
                            k = crc8_raw(inputs[0], poly, refin, refout) ^ targets[0]
                            ok = True
                            for inp, t in zip(inputs[1:], targets[1:]):
                                if (crc8_raw(inp, poly, refin, refout) ^ t) != k:
                                    ok = False
                                    break
                            if ok:
                                hits.append(
                                    (
                                        "crc8",
                                        {
                                            "poly": poly,
                                            "init": 0x00,
                                            "xorout": k,
                                            "refin": refin,
                                            "refout": refout,
                                            "id_prefix": prefix_name,
                                            "cover": list(sub),
                                        },
                                    )
                                )
    return hits


# ── output ─────────────────────────────────────────────────────────────────────


def emit_c(algo, p, can_id, csum_idx):
    if algo == "additive":
        fold = (
            f" + 0x{can_id & 0xFF:02X} + 0x{(can_id >> 8) & 0xFF:02X}"
            if p["fold_id"]
            else ""
        )
        return (
            "uint8_t tesla_checksum(const uint8_t *d, int len) {\n"
            "    uint16_t s = 0;\n"
            f"    for (int i = 0; i < len; i++) if (i != {csum_idx}) s += d[i];\n"
            f"    return (uint8_t)(s{fold} + 0x{p['const']:02X});\n"
            "}"
        )
    cover = p.get("cover")
    cover_str = (
        ("bytes " + ",".join(str(b) for b in cover))
        if cover is not None
        else "all non-checksum bytes"
    )
    idnote = (
        "" if p["id_prefix"] == "none" else f", prefixed with CAN id ({p['id_prefix']})"
    )
    return (
        f"// CRC8 poly=0x{p['poly']:02X} init=0x{p['init']:02X} xorout=0x{p['xorout']:02X} "
        f"refin={int(p['refin'])} refout={int(p['refout'])}\n"
        f"// covered input: {cover_str}{idnote}; checksum stored in byte {csum_idx}\n"
        "// (standard bitwise CRC8 with these params over exactly the covered bytes)"
    )


def main():
    ap = argparse.ArgumentParser(description="Brute-force a Tesla CAN frame checksum.")
    ap.add_argument("capture", help="candump log or hex-per-line frame file")
    ap.add_argument("--id", required=True, help="CAN ID (hex, e.g. 0x485)")
    ap.add_argument(
        "--checksum-byte",
        type=int,
        default=7,
        help="index of the checksum byte (default 7)",
    )
    args = ap.parse_args()

    can_id = int(args.id, 16)
    frames = parse_frames(args.capture, can_id)
    if not frames:
        sys.exit(f"No frames for ID 0x{can_id:X} found in {args.capture}")

    # keep only distinct frames — duplicates add no constraint
    uniq = list(dict.fromkeys(frames))

    # The CRC subset search needs uniform-length frames; keep the modal length.
    lengths = Counter(len(f) for f in uniq)
    modal_len = lengths.most_common(1)[0][0]
    off_len = [f for f in uniq if len(f) != modal_len]
    if off_len:
        print(
            f"NOTE: dropping {len(off_len)} frame(s) not of the modal length {modal_len}."
        )
        uniq = [f for f in uniq if len(f) == modal_len]

    csum = args.checksum_byte
    if csum >= modal_len:
        sys.exit(f"--checksum-byte {csum} is out of range for {modal_len}-byte frames")

    print(
        f"ID 0x{can_id:X}: {len(frames)} frames ({len(uniq)} distinct, {modal_len} bytes). "
        f"Checksum byte = {csum}."
    )
    if len(uniq) < 6:
        print(
            "WARNING: <6 distinct frames — expect multiple spurious matches. "
            "Capture more (let the counter cycle several times)."
        )

    hits = try_additive(uniq, csum, can_id)
    hits += try_crc8(uniq, csum, can_id)

    if not hits:
        print("\nNo additive or CRC8 parameter set reproduces the checksum.")
        print(
            "Possibilities: checksum byte index is wrong, the CRC covers a "
            "different byte range, it's a CRC16/other width, or the capture "
            "mixed frames from two IDs. Double-check --checksum-byte and that "
            "all lines are the same CAN ID."
        )
        return

    print(f"\n{len(hits)} matching parameter set(s):\n")
    for algo, p in hits:
        print(f"  [{algo}] {p}")
    # Show C for the first (and, if it's the lone match, definitive) hit.
    algo, p = hits[0]
    print("\n--- candidate implementation (first match) ---")
    print(emit_c(algo, p, can_id, csum))
    if len(hits) > 1:
        print(
            "\nMore than one match survived — add more distinct frames to "
            "disambiguate, then re-run."
        )


if __name__ == "__main__":
    main()

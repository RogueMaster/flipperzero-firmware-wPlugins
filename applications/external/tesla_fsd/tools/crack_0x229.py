#!/usr/bin/env python3
"""
crack_0x229.py — recover the checksum scheme of a counter+CRC Tesla CAN frame.

Built for 0x229 SCCM_rightStalk (the pre-Highland native AP-engage stalk), but
works on any candump of a single ID. Per opendbc tesla_model3_vehicle.dbc, 0x229
is a 3-byte frame:

    byte0  = SCCM_rightStalkCrc      (8-bit checksum)
    byte1  = [reserved:1][status:3][counter:4]   -> bits[3:0] = rolling counter,
                                                    bits[6:4] = stalk position
                                                    (0 idle, 4 pulled-down)
    byte2  = [reserved:6][parkButton:2]

A plain CRC8 over byte1..byteN does NOT close (the counter->crc map is provably
non-affine). The reason is almost certainly AUTOSAR E2E Profile 2 style: the CRC
is computed over the data bytes PLUS a per-message "Data ID" byte selected from a
16-entry table indexed by the counter. That counter-indexed byte is what makes
the map non-affine in the counter.

Key trick: a CRC8 is affine over GF(2) in all of its input bytes, so for a fixed
(poly, init, refl, byte-order) the appended Data-ID byte and the final xorout
both collapse into a single per-counter constant:

    actual_crc = crc_raw(data, dataID=0)  XOR  T[counter]

So we never have to brute-force the Data-ID table or xorout. For each candidate
(poly, init, refin, refout, order) we compute T_i = actual_i XOR crc_raw_i, group
by counter, and accept the candidate iff T is constant within every counter group.
T[counter] is then the recovered constant, and the injection formula is simply:

    crc = crc_raw(new_data, dataID=0; poly, init, refl, order)  XOR  T[counter]

which lets us build a valid frame (e.g. a pulled-down stalk) without ever isolating
the Data-ID list or xorout.

Schemes tried, in order:
  1. Additive  : (sum(cover) + K) & 0xFF                         (Tesla 0x370/0x3FD style)
  2. Plain CRC8: crc closes over the data with one global constant (T identical for
                 every counter) — the special case of (3) with one group.
  3. E2E/DataID: T constant per counter but varies across counters (the 0x229 case).

Usage:
    python3 crack_0x229.py CAPTURE.dump [--id 229] [--crc-byte 0] [--counter-byte 1]
                            [--counter-mask 0x0F] [--inline]    # read frames from stdin

Exit status 0 if a scheme was recovered, 1 otherwise.
"""

import argparse
import re
import sys
from collections import Counter


# ----------------------------------------------------------------------------- parsing
LINE_RE = re.compile(r'\(([\d.]+)\)\s+\S+\s+([0-9A-Fa-f]+)#([0-9A-Fa-f]*)')


def load_frames(lines, want_id):
    """Return list of (timestamp, id, bytes) for frames matching want_id (or all)."""
    out = []
    for ln in lines:
        m = LINE_RE.search(ln.strip())
        if not m:
            continue
        t = float(m.group(1))
        fid = int(m.group(2), 16)
        data = bytes.fromhex(m.group(3)) if m.group(3) else b''
        if want_id is None or fid == want_id:
            out.append((t, fid, data))
    return out


# ----------------------------------------------------------------------------- crc core
def _reflect(x, bits):
    r = 0
    for i in range(bits):
        if x & (1 << i):
            r |= 1 << (bits - 1 - i)
    return r


_REFLECT8 = [_reflect(i, 8) for i in range(256)]


def crc8_raw(data, poly, init, refin, refout):
    """CRC8 with no xorout (xorout is absorbed into the per-counter constant)."""
    c = init
    for b in data:
        if refin:
            b = _REFLECT8[b]
        c ^= b
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFF if (c & 0x80) else (c << 1) & 0xFF
    if refout:
        c = _REFLECT8[c]
    return c


# ----------------------------------------------------------------------------- detection
def detect_counter_byte(frames, crc_byte):
    """Pick the byte (other than the crc byte) whose low nibble increments most
    like a +1/frame rolling counter on consecutive frames."""
    if not frames:
        return None
    dlc = len(frames[0][2])
    best, best_score = None, -1.0
    for bi in range(dlc):
        if bi == crc_byte:
            continue
        good = total = 0
        prev = None
        for _, _, d in frames:
            if bi >= len(d):
                prev = None
                continue
            v = d[bi] & 0x0F
            if prev is not None:
                total += 1
                if v == (prev + 1) & 0x0F:
                    good += 1
            prev = v
        score = good / total if total else 0.0
        if score > best_score:
            best, best_score = bi, score
    return best if best_score >= 0.5 else None


def detect_crc_byte(frames):
    """Highest-entropy byte is the most likely checksum."""
    dlc = len(frames[0][2])
    best, best_n = 0, -1
    for bi in range(dlc):
        n = len({d[bi] for _, _, d in frames if bi < len(d)})
        if n > best_n:
            best, best_n = bi, n
    return best


# ----------------------------------------------------------------------------- crackers
def try_additive(samples, can_id):
    """samples: list of (cover_bytes, crc, counter). Tesla additive = (idlo+idhi+sum)&0xFF."""
    id_lo, id_hi = can_id & 0xFF, (can_id >> 8) & 0xFF
    for fold in ('none', 'id', 'idlo'):
        k0 = {'none': 0, 'id': id_lo + id_hi, 'idlo': id_lo}[fold]
        const = None
        ok = True
        for cover, crc, _ in samples:
            calc = (k0 + sum(cover)) & 0xFF
            d = (crc - calc) & 0xFF
            if const is None:
                const = d
            elif d != const:
                ok = False
                break
        if ok:
            return {'scheme': 'additive', 'fold': fold, 'offset': const}
    return None


# Common automotive CRC8 polynomials, preferred when several parameterisations fit.
CANONICAL_POLYS = (0x2F, 0x1D, 0x07, 0x31, 0x9B, 0x39, 0xD5, 0x4D, 0x8C)


def _cover_builders(can_id):
    id_lo, id_hi = can_id & 0xFF, (can_id >> 8) & 0xFF
    return {
        'data':      lambda data: list(data),
        'idlo+data': lambda data: [id_lo] + list(data),
        'data+idlo': lambda data: list(data) + [id_lo],
        'idbe+data': lambda data: [id_hi, id_lo] + list(data),
        'data+idbe': lambda data: list(data) + [id_hi, id_lo],
    }


def _global_const(samples, poly, refin, refout, cover_of):
    """Single global constant k s.t. crc == crc8_raw(cover) ^ k for ALL frames, else None.
    init is fixed at 0: for a fixed cover length it shifts crc8_raw by a constant that k
    absorbs, so (init=0, k) fully parameterises the function."""
    k = None
    for data, crc, _ in samples:
        d = crc ^ crc8_raw(cover_of(data), poly, 0, refin, refout)
        if k is None:
            k = d
        elif d != k:
            return None
    return k


def _per_counter(samples, poly, refin, refout, cover_of):
    """(T, constrained_groups) where T[ctr] is constant within each counter group, else
    (None, 0). constrained_groups = #counters whose group has >=2 distinct data vectors
    (only those actually pin T — the constraint-strength signal)."""
    T, rows = {}, {}
    for data, crc, ctr in samples:
        d = crc ^ crc8_raw(cover_of(data), poly, 0, refin, refout)
        if ctr in T:
            if T[ctr] != d:
                return None, 0
        else:
            T[ctr] = d
        rows.setdefault(ctr, set()).add(tuple(data))
    constrained = sum(1 for c in rows if len(rows[c]) >= 2)
    return T, constrained


def _holdout(samples, poly, refin, refout, cover_of):
    """Fit T on the first half of frames, predict the second half (counters present in
    both). Returns (checked, mismatches) — the generalisation signal that separates a
    real scheme from a memorised per-counter lookup table. Contiguous halves are used so
    that a cleanly +1/frame counter (index parity == counter parity) still overlaps."""
    half = len(samples) // 2
    fit, test = samples[:half], samples[half:]
    Tfit, _ = _per_counter(fit, poly, refin, refout, cover_of)
    if Tfit is None:
        return (0, 0)
    checked = mism = 0
    for data, crc, ctr in test:
        if ctr not in Tfit:
            continue
        checked += 1
        if (crc8_raw(cover_of(data), poly, 0, refin, refout) ^ Tfit[ctr]) != crc:
            mism += 1
    return (checked, mism)


def try_crc(samples, can_id):
    """Recover a CRC8 scheme, preferring the simplest hypothesis (Occam) and guarding
    against under-constrained false positives. init is absorbed into the constant(s).

    1. plain-crc8: a single global constant fits every frame.
    2. e2e-dataid: a per-counter constant fits, but ONLY accepted when at least two
       counter groups are pinned by >=2 distinct data rows (else the 16 free per-counter
       constants can memorise almost any byte0 — the singleton/decimation false positive).
    poly==0 is skipped (crc8_raw would be input-independent -> a pure lookup table).
    """
    builders = _cover_builders(can_id)

    # ---- 1. plain-crc8 (single global constant) — preferred over e2e ----
    plain = []
    for cname, cover_of in builders.items():
        for refin in (False, True):
            for refout in (False, True):
                for poly in range(1, 256):
                    k = _global_const(samples, poly, refin, refout, cover_of)
                    if k is not None:
                        plain.append((poly, refin, refout, cname, k))
    if plain:
        plain.sort(key=lambda c: (c[0] not in CANONICAL_POLYS, c[0]))
        poly, refin, refout, cname, k = plain[0]
        return {'scheme': 'plain-crc8', 'poly': poly, 'init': 0, 'refin': refin,
                'refout': refout, 'cover': cname, 'xorout': k, 'T': None,
                'constrained_groups': None, 'holdout': None, 'n_fits': len(plain)}

    # ---- 2. e2e-dataid (per-counter constant) — guarded ----
    e2e = []
    for cname, cover_of in builders.items():
        for refin in (False, True):
            for refout in (False, True):
                for poly in range(1, 256):
                    T, constrained = _per_counter(samples, poly, refin, refout, cover_of)
                    if T is None or len(set(T.values())) < 2:
                        continue
                    if constrained < 2:           # P0: under-constrained -> not trustworthy
                        continue
                    e2e.append((constrained, poly, refin, refout, cname, T))
    if e2e:
        e2e.sort(key=lambda c: (-c[0], c[1] not in CANONICAL_POLYS, c[1]))
        constrained, poly, refin, refout, cname, T = e2e[0]
        hold = _holdout(samples, poly, refin, refout, builders[cname])
        return {'scheme': 'e2e-dataid', 'poly': poly, 'init': 0, 'refin': refin,
                'refout': refout, 'cover': cname, 'xorout': None, 'T': T,
                'constrained_groups': constrained, 'holdout': hold, 'n_fits': len(e2e)}
    return None


# ----------------------------------------------------------------------------- report
_COVER_EXPR = {
    'data':      "list(data_no_crc)",
    'idlo+data': "[ID_LO] + list(data_no_crc)",
    'data+idlo': "list(data_no_crc) + [ID_LO]",
    'idbe+data': "[ID_HI, ID_LO] + list(data_no_crc)",
    'data+idbe': "list(data_no_crc) + [ID_HI, ID_LO]",
}


def emit_injection_snippet(res, can_id):
    if res['scheme'] == 'additive':
        return (f"# additive: crc = ({res['offset']} + sum(cover[{res['fold']}])) & 0xFF\n"
                "# (cover = the data bytes excluding the crc byte)")
    cover = _COVER_EXPR[res['cover']]
    head = (f"ID_LO, ID_HI = 0x{can_id & 0xFF:02X}, 0x{(can_id >> 8) & 0xFF:02X}\n"
            f"# recovered: poly=0x{res['poly']:02X} init=0 (absorbed) "
            f"refin={res['refin']} refout={res['refout']} cover={res['cover']}\n"
            "# uses crc8_raw() from crack_0x229.py")
    if res['scheme'] == 'plain-crc8':
        return (head + "\n"
                "def crc(data_no_crc):\n"
                f"    return crc8_raw({cover}, 0x{res['poly']:02X}, 0, {res['refin']}, {res['refout']})"
                f" ^ 0x{res['xorout']:02X}")
    # e2e-dataid
    tbl = "{" + ", ".join(f"0x{k:X}: 0x{v:02X}" for k, v in sorted(res['T'].items())) + "}"
    return (head + "\n"
            f"T = {tbl}\n"
            "def crc(data_no_crc, counter):\n"
            f"    return crc8_raw({cover}, 0x{res['poly']:02X}, 0, {res['refin']}, {res['refout']})"
            " ^ T[counter]")


def main():
    ap = argparse.ArgumentParser(description="Crack a Tesla counter+CRC frame (default 0x229).")
    ap.add_argument('capture', nargs='?', help="candump file (omit to read stdin)")
    ap.add_argument('--id', default='229', help="CAN id hex (default 229)")
    ap.add_argument('--crc-byte', type=int, default=None, help="index of the CRC byte (auto if unset)")
    ap.add_argument('--counter-byte', type=int, default=None, help="index of the counter byte (auto)")
    ap.add_argument('--counter-mask', default='0x0F', help="counter mask within its byte (default 0x0F)")
    args = ap.parse_args()

    can_id = int(args.id, 16)
    counter_mask = int(args.counter_mask, 16)
    src = open(args.capture) if args.capture else sys.stdin
    frames = load_frames(src, can_id)
    if args.capture:
        src.close()

    if len(frames) < 4:
        print(f"need >=4 frames for id 0x{can_id:X}, got {len(frames)}", file=sys.stderr)
        return 2

    dlcs = Counter(len(d) for _, _, d in frames)
    dlc = dlcs.most_common(1)[0][0]
    frames = [(t, i, d) for t, i, d in frames if len(d) == dlc]  # keep the dominant DLC
    print(f"id=0x{can_id:X}  frames={len(frames)}  dlc={dlc}  (dlc histogram {dict(dlcs)})")

    crc_byte = args.crc_byte if args.crc_byte is not None else detect_crc_byte(frames)
    counter_byte = args.counter_byte if args.counter_byte is not None else detect_counter_byte(frames, crc_byte)
    if counter_byte is None:
        counter_byte = crc_byte  # degenerate; e2e grouping just becomes plain-crc8
        print("WARNING: no clean rolling counter detected; treating all frames as one group")
    print(f"crc_byte=byte{crc_byte}  counter_byte=byte{counter_byte}  counter_mask=0x{counter_mask:02X}")
    for bi in range(dlc):
        vals = Counter(d[bi] for _, _, d in frames)
        tag = " <- crc" if bi == crc_byte else (" <- counter" if bi == counter_byte else "")
        print(f"  byte{bi}: {len(vals)} distinct{tag}")

    # build sample tuples
    samples = []
    for _, _, d in frames:
        crc = d[crc_byte]
        ctr = (d[counter_byte] >> 0) & counter_mask if counter_byte != crc_byte else 0
        data_no_crc = bytes(b for j, b in enumerate(d) if j != crc_byte)
        samples.append((data_no_crc, crc, ctr))

    add_samples = [(s[0], s[1], s[2]) for s in samples]
    res = try_additive(add_samples, can_id) or try_crc(samples, can_id)

    print()
    if not res:
        print("NO SCHEME RECOVERED.")
        print("  - If the counter byte looks wrong, pass --counter-byte/--counter-mask.")
        print("  - If frames came through truncated, recapture at full DLC (?ids= filter).")
        print("  - Coverage tried: additive(+id folds), plain-CRC8 and AUTOSAR-E2E/Data-ID")
        print("    over {data, ±id-lo, ±id-be}, all poly/init/refin/refout.")
        return 1

    print(f"*** RECOVERED: {res['scheme']} ***")
    if res['scheme'] == 'additive':
        print(f"  fold={res['fold']}  offset=0x{res['offset']:02X}")
    else:
        print(f"  poly=0x{res['poly']:02X} init=0 (absorbed) "
              f"refin={res['refin']} refout={res['refout']} cover={res['cover']}  "
              f"({res['n_fits']} equivalent fit(s))")
        if res['scheme'] == 'e2e-dataid':
            print(f"  per-counter constant T[counter] (Data-ID + xorout, collapsed):")
            print("   " + "  ".join(f"{k:X}:{v:02X}" for k, v in sorted(res['T'].items())))
            print(f"  constraint strength: {res['constrained_groups']} counter group(s) "
                  f"pinned by >=2 distinct data rows")
            chk, mism = res['holdout']
            verdict = "GENERALISES" if (chk > 0 and mism == 0) else \
                      ("FAILS HOLD-OUT" if mism else "no held-out overlap")
            print(f"  hold-out: fit on half, predicted {chk - mism}/{chk} of the other half "
                  f"correctly -> {verdict}")
            if chk == 0:
                print("  WARNING: no held-out overlap — capture too sparse to confirm; "
                      "recapture at full rate (?ids= filter) before trusting this.")
            elif mism:
                print("  WARNING: scheme did NOT generalise — likely an over-fit on sparse data.")
        else:
            print(f"  xorout=0x{res['xorout']:02X}")
    print("\n--- injection helper ---")
    print(emit_injection_snippet(res, can_id))
    return 0


if __name__ == '__main__':
    sys.exit(main())

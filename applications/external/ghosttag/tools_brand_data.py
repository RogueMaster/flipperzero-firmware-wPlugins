#!/usr/bin/env python3
"""Read the brand's facts out of the source, so nothing on an asset is typed twice.

The version, the dwell threshold, the sighting minimum and the entire demo cast
all come from the C that ships. The banner's one measured element is the
approach curve of the demo AirTag, computed here with the same linear
interpolation helpers/demo_source.c uses on the device - so the curve on the
banner is the curve the firmware draws, not an artist's impression of it.

If a constant moves in the source, the asset moves with it or the parse fails
loudly. That is the point.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))

NEVER = 0xFFFFFFFF


def _read(rel):
    with open(os.path.join(HERE, rel)) as fh:
        return fh.read()


def version():
    m = re.search(r'#define\s+GHOSTTAG_VERSION\s+"([^"]+)"', _read("ghosttag_i.h"))
    if not m:
        raise SystemExit("GHOSTTAG_VERSION not found in ghosttag_i.h")
    return m.group(1)


def define_int(rel, name):
    m = re.search(rf"#define\s+{name}\s+(\d+)", _read(rel))
    if not m:
        raise SystemExit(f"{name} not found in {rel}")
    return int(m.group(1))


def demo_follow_ms():
    return define_int("helpers/demo_source.h", "DEMO_FOLLOW_MS")


def demo_loop_ms():
    return define_int("helpers/demo_source.c", "DEMO_LOOP_MS")


def demo_tick_ms():
    return define_int("helpers/demo_source.c", "DEMO_TICK_MS")


def min_detections():
    return define_int("helpers/tracker_db.h", "TRACKER_DB_MIN_DETECTIONS")


def stale_ms():
    return define_int("helpers/tracker_db.h", "TRACKER_STALE_MS")


CAST = re.compile(
    r"\{\{(?P<mac>[^}]*)\}\s*,\s*"
    r"(?P<type>Tracker\w+)\s*,\s*"
    r'"(?P<name>[^"]*)"\s*,\s*'
    r"(?P<appear>\w+)\s*,\s*"
    r"(?P<depart>\w+)\s*,\s*"
    r"(?P<rfrom>-?\d+)\s*,\s*"
    r"(?P<rto>-?\d+)\s*,\s*"
    r"(?P<jitter>\d+)\s*\}",
    re.S,
)

THREAT_TYPES = {
    "TrackerTypeAppleFindMy",
    "TrackerTypeTile",
    "TrackerTypeSamsungSmartTag",
    "TrackerTypeChipolo",
}


def demo_cast():
    """Every simulated broadcaster, exactly as demo_source.c declares it."""
    src = _read("helpers/demo_source.c")
    body = src[src.index("demo_cast[] = {") :]
    out = []
    for m in CAST.finditer(body):
        depart = m.group("depart")
        out.append(
            dict(
                mac=[
                    int(b, 16)
                    for b in re.findall(r"0x([0-9A-Fa-f]{2})", m.group("mac"))
                ],
                type=m.group("type"),
                name=m.group("name"),
                appear=int(m.group("appear")),
                depart=NEVER if depart == "DEMO_NEVER" else int(depart),
                rssi_from=int(m.group("rfrom")),
                rssi_to=int(m.group("rto")),
                jitter=int(m.group("jitter")),
                threat=m.group("type") in THREAT_TYPES,
            )
        )
    if not out:
        raise SystemExit("could not parse demo_cast[] out of helpers/demo_source.c")
    return out


def _jitter(seed, amplitude):
    """Bit-for-bit the demo_jitter() in demo_source.c, in 32-bit arithmetic."""
    if not amplitude:
        return 0
    seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
    return ((seed >> 16) % (amplitude * 2 + 1)) - amplitude


def rssi_series(dev, loop_ms=None, tick_ms=None):
    """(t_ms, rssi) for one cast member, the same maths the firmware runs.

    Returns only the ticks where the device is actually present, so a gap in
    the list is a gap in the air rather than a line drawn through nothing.
    """
    loop_ms = loop_ms or demo_loop_ms()
    tick_ms = tick_ms or demo_tick_ms()

    pts = []
    steps = loop_ms // tick_ms
    for step in range(steps):
        t = step * tick_ms
        if t < dev["appear"]:
            continue
        if dev["depart"] != NEVER and t >= dev["depart"]:
            continue

        life_end = loop_ms if dev["depart"] == NEVER else dev["depart"]
        span = max(life_end - dev["appear"], 1)
        into = min(t - dev["appear"], span)

        lo, hi = dev["rssi_from"], dev["rssi_to"]
        level = lo + ((hi - lo) * into) // span
        level += _jitter((step * 31 + dev["mac"][5]) & 0xFFFFFFFF, dev["jitter"])
        level = max(-99, min(-30, level))
        pts.append((t, level))
    return pts


def alert_moment(dev, follow_ms=None, tick_ms=None, min_hits=None):
    """When this device would actually trip the follow heuristic, in ms.

    Mirrors tracker_db_update: a known tracker type, at least min_hits
    sightings, and (last_seen - first_seen) >= the dwell window. Returns None
    for anything that never trips - which is the whole point of having the
    Tile in the cast.
    """
    if not dev["threat"]:
        return None
    follow_ms = follow_ms if follow_ms is not None else demo_follow_ms()
    tick_ms = tick_ms or demo_tick_ms()
    min_hits = min_hits if min_hits is not None else min_detections()

    pts = rssi_series(dev, tick_ms=tick_ms)
    if not pts:
        return None
    first = pts[0][0]
    hits = 0
    for t, _r in pts:
        hits += 1
        if hits >= min_hits and (t - first) >= follow_ms:
            return t
    return None


if __name__ == "__main__":
    print(f"GhostTag v{version()}")
    print(f"  dwell window (demo) : {demo_follow_ms()} ms")
    print(f"  scenario loop       : {demo_loop_ms()} ms @ {demo_tick_ms()} ms/tick")
    print(f"  min sightings       : {min_detections()}")
    print(f"  stale after         : {stale_ms()} ms")
    print("  cast:")
    for d in demo_cast():
        a = alert_moment(d)
        when = f"trips at {a/1000:.1f}s" if a else "never trips"
        pts = rssi_series(d)
        print(
            f"    {d['type']:<28} {d['name'] or '(no name)':<14} "
            f"{d['rssi_from']:>4}..{d['rssi_to']:>4} dBm  "
            f"{len(pts):>3} sightings  {when}"
        )

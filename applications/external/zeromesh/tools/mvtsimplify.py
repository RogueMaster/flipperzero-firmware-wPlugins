#!/usr/bin/env python3
"""Shrink .mvt tiles down to what the ZeroMesh renderer actually draws.

The device reads far less of a vector tile than a general-purpose renderer
does. zeromesh_map.c rasterises CARTO_LAYER_WATER and CARTO_LAYER_ROAD only,
and scans three label layers by name. Every other layer -- land, landuse,
buildings, sites, street_labels -- is decoded and thrown away, so removing it
at pack time costs nothing on screen.

Roads are filtered the same way the firmware filters them: mvt.c computes
min_road_prio = 16 - zoom and skips anything below it, so dropping those
features here is likewise invisible. Only the levels above SAFE_LEVEL trade
detail for size, and build_pmtiles.py escalates through them only when a tile
is still over budget.
"""

# Layer names carto_classify_layer maps to WATER or ROAD, plus the label
# layers zeromesh_map.c scans. Keep these in sync with lib/carto/style.c.
RENDER_LAYERS = {
    "water",
    "ocean",
    "rivers",
    "lakes",
    "water_polygons",
    "water_lines",
    "waterway",
    "roads",
    "transportation",
    "streets",
    "street_polygons",
    "bridges",
}

LABEL_LAYERS = {
    "place_labels",
    "water_polygons_labels",
    "water_lines_labels",
}

ROAD_LAYERS = {"roads", "transportation", "streets", "street_polygons", "bridges"}

# carto_road_priority() in lib/carto/style.c
ROAD_PRIORITY = {
    "highway": 10,
    "motorway": 10,
    "trunk": 9,
    "primary": 8,
    "secondary": 7,
    "tertiary": 6,
    "minor_road": 5,
    "residential": 4,
    "street": 4,
    "service": 3,
    "path": 2,
    "footway": 2,
    "cycleway": 2,
    "track": 2,
    "other": 1,
}

# key_is_class() in lib/carto/mvt.c
CLASS_KEYS = {"kind", "class", "pmap:kind"}

SAFE_LEVEL = 0
MAX_LEVEL = 6


def read_varint(buf, i):
    r = 0
    s = 0
    while True:
        c = buf[i]
        i += 1
        r |= (c & 0x7F) << s
        if not c & 0x80:
            return r, i
        s += 7


def enc_varint(v):
    out = bytearray()
    while v >= 0x80:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    out.append(v)
    return bytes(out)


def enc_tag(fn, wt):
    return enc_varint((fn << 3) | wt)


def enc_bytes(fn, b):
    return enc_tag(fn, 2) + enc_varint(len(b)) + bytes(b)


def enc_uint(fn, v):
    return enc_tag(fn, 0) + enc_varint(v)


def iter_fields(buf):
    """Yield (field_number, wire_type, scalar, payload) over a protobuf message."""
    i = 0
    n = len(buf)
    while i < n:
        key, i = read_varint(buf, i)
        fn, wt = key >> 3, key & 7
        if wt == 0:
            v, i = read_varint(buf, i)
            yield fn, wt, v, None
        elif wt == 2:
            ln, i = read_varint(buf, i)
            yield fn, wt, None, buf[i:i + ln]
            i += ln
        elif wt == 5:
            yield fn, wt, None, buf[i:i + 4]
            i += 4
        elif wt == 1:
            yield fn, wt, None, buf[i:i + 8]
            i += 8
        else:
            return


def value_string(payload):
    """A Value message; field 1 is string_value, which is all we classify on."""
    for fn, _wt, _v, p in iter_fields(payload):
        if fn == 1:
            try:
                return p.decode("utf-8")
            except UnicodeDecodeError:
                return None
    return None


def feature_tags(payload):
    """Field 2 is the tag list, packed or repeated depending on the encoder."""
    tags = []
    for fn, wt, v, p in iter_fields(payload):
        if fn != 2:
            continue
        if wt == 0:
            tags.append(v)
        elif wt == 2:
            i = 0
            while i < len(p):
                t, i = read_varint(p, i)
                tags.append(t)
    return tags


def parse_layer(payload):
    name = None
    version = None
    extent = None
    keys = []
    values = []
    features = []

    for fn, wt, v, p in iter_fields(payload):
        if fn == 1 and wt == 2:
            name = p.decode("utf-8", "replace")
        elif fn == 2 and wt == 2:
            features.append(p)
        elif fn == 3 and wt == 2:
            keys.append(p)
        elif fn == 4 and wt == 2:
            values.append(p)
        elif fn == 5 and wt == 0:
            extent = v
        elif fn == 15 and wt == 0:
            version = v

    return name, version, extent, keys, values, features


def build_layer(name, version, extent, keys, values, features):
    out = bytearray()
    out += enc_bytes(1, name.encode("utf-8"))
    for f in features:
        out += enc_bytes(2, f)
    for k in keys:
        out += enc_bytes(3, k)
    for v in values:
        out += enc_bytes(4, v)
    if extent is not None:
        out += enc_uint(5, extent)
    if version is not None:
        out += enc_uint(15, version)
    return bytes(out)


def road_floor(zoom, level):
    """Mirror mvt.c: min_road_prio = 16 - zoom, clamped to 1..10."""
    mp = 16 - zoom
    mp = max(1, min(10, mp))
    if level >= 1:
        mp += 1
    if level >= 2:
        mp += 1
    if level >= 4:
        mp += 1
    if level >= 6:
        mp += 1
    return max(1, min(10, mp))


def zigzag_dec(v):
    return (v >> 1) ^ (-(v & 1))


def zigzag_enc(v):
    return (v << 1) if v >= 0 else ((-v) << 1) - 1


def decode_packed(p):
    vals = []
    i = 0
    while i < len(p):
        v, i = read_varint(p, i)
        vals.append(v)
    return vals


def encode_packed(vals):
    out = bytearray()
    for v in vals:
        out += enc_varint(v)
    return bytes(out)


def decode_geom(vals):
    """Returns [(command_id, [(x, y), ...]), ...] in absolute coordinates."""
    parts = []
    i = 0
    cx = cy = 0
    while i < len(vals):
        cmd = vals[i]
        i += 1
        cid, count = cmd & 7, cmd >> 3
        if cid == 7:
            parts.append((7, []))
            continue
        if cid not in (1, 2):
            return None
        pts = []
        for _ in range(count):
            if i + 1 >= len(vals):
                return None
            cx += zigzag_dec(vals[i])
            cy += zigzag_dec(vals[i + 1])
            i += 2
            pts.append((cx, cy))
        parts.append((cid, pts))
    return parts


def encode_geom(parts):
    vals = []
    cx = cy = 0
    for cid, pts in parts:
        if cid == 7:
            vals.append((1 << 3) | 7)
            continue
        if not pts:
            continue
        vals.append((len(pts) << 3) | cid)
        for x, y in pts:
            vals.append(zigzag_enc(x - cx))
            vals.append(zigzag_enc(y - cy))
            cx, cy = x, y
    return vals


def quant_step(level):
    """Extent units to snap to. The renderer draws a 4096-unit tile into 256
    pixels, so 16 units is one pixel; 8 is half a pixel and invisible."""
    if level >= 6:
        return 32
    if level >= 4:
        return 24
    if level >= 2:
        return 16
    return 8


def drop_threshold(level, ftype):
    """Extent units below which a feature is invisible. mvt.c skips lines
    under CARTO_MIN_LINE_PX and polygons under CARTO_MIN_POLY_PX in both
    dimensions, and 1 px is 16 extent units at 256 px per 4096 tile, so at
    level 0 this drops exactly what the renderer would have thrown away."""
    base = 3.0 if ftype == 2 else 2.0
    return base * (1.0 + 0.5 * level) * 16.0


def parts_bbox(parts):
    xs = [p[0] for _cid, pts in parts for p in pts]
    ys = [p[1] for _cid, pts in parts for p in pts]
    if not xs:
        return 0.0, 0.0
    return float(max(xs) - min(xs)), float(max(ys) - min(ys))


def quantize_parts(parts, step, ftype):
    pieces = []
    cur = None
    for cid, pts in parts:
        if cid == 1:
            for p in pts:
                if cur:
                    pieces.append(cur)
                cur = {"pts": [p], "closed": False}
        elif cid == 2:
            if cur is None:
                cur = {"pts": [], "closed": False}
            cur["pts"].extend(pts)
        elif cid == 7:
            if cur:
                cur["closed"] = True
                pieces.append(cur)
                cur = None
    if cur:
        pieces.append(cur)

    out = []
    for pc in pieces:
        q = []
        for x, y in pc["pts"]:
            qx = int(round(float(x) / step)) * step
            qy = int(round(float(y) / step)) * step
            if q and q[-1] == (qx, qy):
                continue
            q.append((qx, qy))

        if ftype == 3:
            if len(q) < 3:
                continue
        elif ftype == 2:
            if len(q) < 2:
                continue
        elif not q:
            continue

        out.append((1, [q[0]]))
        if len(q) > 1:
            out.append((2, q[1:]))
        if pc["closed"]:
            out.append((7, []))

    return out or None


def requantize_feature(payload, step, level=0):
    """Drop sub-pixel precision from one feature. Returns None if nothing
    survives, in which case the caller drops the feature."""
    fid = None
    ftype = None
    geom = None
    for fn, wt, v, p in iter_fields(payload):
        if fn == 1 and wt == 0:
            fid = v
        elif fn == 3 and wt == 0:
            ftype = v
        elif fn == 4 and wt == 2:
            geom = p

    if geom is None or ftype is None:
        return payload

    parts = decode_geom(decode_packed(geom))
    if parts is None:
        return payload

    parts = quantize_parts(parts, step, ftype)
    if parts is None:
        return None

    if ftype in (2, 3):
        bw, bh = parts_bbox(parts)
        thresh = drop_threshold(level, ftype)
        if bw < thresh and bh < thresh:
            return None

    tags = feature_tags(payload)

    out = bytearray()
    if fid is not None:
        out += enc_uint(1, fid)
    if tags:
        out += enc_bytes(2, encode_packed(tags))
    out += enc_uint(3, ftype)
    out += enc_bytes(4, encode_packed(encode_geom(parts)))
    return bytes(out)


def simplify(body, zoom, level=SAFE_LEVEL):
    """Return a smaller tile holding only what the device will draw."""
    keep_water_lines = level < 3
    keep_water_labels = level < 5
    floor = road_floor(zoom, level)

    out = bytearray()
    for fn, wt, _v, payload in iter_fields(body):
        if fn != 3 or wt != 2:
            continue

        name, version, extent, keys, values, features = parse_layer(payload)
        if name is None:
            continue

        if name in LABEL_LAYERS:
            if name != "place_labels" and not keep_water_labels:
                continue
        elif name in RENDER_LAYERS:
            if name == "water_lines" and not keep_water_lines:
                continue
        else:
            continue

        if name in ROAD_LAYERS:
            class_idx = {i for i, k in enumerate(keys)
                         if k.decode("utf-8", "replace") in CLASS_KEYS}
            strings = [value_string(v) for v in values]

            kept = []
            for f in features:
                prio = 1
                tags = feature_tags(f)
                for j in range(0, len(tags) - 1, 2):
                    if tags[j] in class_idx:
                        vi = tags[j + 1]
                        if 0 <= vi < len(strings) and strings[vi]:
                            prio = ROAD_PRIORITY.get(strings[vi], 1)
                        break
                if prio >= floor:
                    kept.append(f)
            features = kept

        step = quant_step(level)
        requantized = []
        for f in features:
            qf = requantize_feature(f, step, level)
            if qf is not None:
                requantized.append(qf)
        features = requantized

        if not features:
            continue

        out += enc_bytes(3, build_layer(name, version, extent, keys, values, features))

    return bytes(out)


def simplify_to_budget(body, zoom, budget):
    """Escalate only as far as the budget demands. Returns (tile, level)."""
    best = simplify(body, zoom, SAFE_LEVEL)
    if budget is None or len(best) <= budget:
        return best, SAFE_LEVEL

    for level in range(SAFE_LEVEL + 1, MAX_LEVEL + 1):
        cand = simplify(body, zoom, level)
        if len(cand) <= budget:
            return cand, level
        best = cand
    return best, MAX_LEVEL

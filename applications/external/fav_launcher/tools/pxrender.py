import re
from PIL import Image, ImageDraw


def parse_rects(d):
    toks = re.findall(r"[MmHhVvZzLl]|-?\d+\.?\d*", d)
    rects = []
    i = 0
    x = y = 0
    sx = sy = 0
    cmd = None
    pts = []

    def flush():
        if len(pts) >= 2:
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            rects.append((min(xs), min(ys), max(xs), max(ys)))

    while i < len(toks):
        t = toks[i]
        if t in "MmHhVvZzLl":
            cmd = t
            i += 1
            if cmd in "Zz":
                flush()
                x, y = sx, sy
                pts = [(x, y)]
            continue
        if cmd in "Mm":
            nx = float(toks[i])
            ny = float(toks[i + 1])
            i += 2
            if cmd == "m":
                nx += x
                ny += y
            flush()
            x, y = nx, ny
            sx, sy = x, y
            pts = [(x, y)]
            cmd = "L"
        elif cmd in "Ll":
            nx = float(toks[i])
            ny = float(toks[i + 1])
            i += 2
            if cmd == "l":
                nx += x
                ny += y
            x, y = nx, ny
            pts.append((x, y))
        elif cmd in "Hh":
            nx = float(toks[i])
            i += 1
            if cmd == "h":
                nx += x
            x = nx
            pts.append((x, y))
        elif cmd in "Vv":
            ny = float(toks[i])
            i += 1
            if cmd == "v":
                ny += y
            y = ny
            pts.append((x, y))
        else:
            i += 1
    flush()
    return rects


def render_px(path, size=18, S=12):
    d = re.search(r'd="([^"]+)"', open(path).read()).group(1)
    big = Image.new("L", (24 * S, 24 * S), 0)
    dr = ImageDraw.Draw(big)
    for x0, y0, x1, y1 in parse_rects(d):
        dr.rectangle([x0 * S, y0 * S, x1 * S - 1, y1 * S - 1], fill=255)
    im = big.resize((size, size), Image.LANCZOS)
    return im.point(lambda p: 255 if p > 95 else 0).convert("1")


def pack(img, out):
    px = img.load()
    W, H = img.size
    data = bytearray([0])
    for y in range(H):
        for bx in range(0, W, 8):
            b = 0
            for i in range(8):
                if bx + i < W and px[bx + i, y]:
                    b |= 1 << i
            data.append(b)
    open(out, "wb").write(bytes(data))

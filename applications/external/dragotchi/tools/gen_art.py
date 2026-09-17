#!/usr/bin/env python3
"""Generate Dragotchi's 1-bit dragon sprites (60x60, black-on-white PNGs).
Alignment is encoded as fill style so white/grey/black dragons read clearly on
the Flipper's 1-bit screen: white=outline only, grey=dithered, black=solid."""
from PIL import Image, ImageDraw

W = H = 60
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)

def new_img():
    im = Image.new("RGB", (W, H), WHITE)
    return im, ImageDraw.Draw(im)

def dither(dr, box, on=BLACK):
    """Fill a bounding box with a 50% checkerboard (grey on 1-bit)."""
    x0, y0, x1, y1 = box
    for y in range(int(y0), int(y1)):
        for x in range(int(x0), int(x1)):
            if (x + y) % 2 == 0:
                dr.point((x, y), fill=on)

def fill_shape(im, dr, draw_fn, mode):
    """draw_fn(d) draws the silhouette in solid black on a scratch mask.
    mode: 'black' solid, 'white' outline-only, 'grey' dithered; all keep outline."""
    mask = Image.new("1", (W, H), 0)
    md = ImageDraw.Draw(mask)
    draw_fn(md)  # solid white(1) silhouette on mask
    px = mask.load()
    if mode == "black":
        for y in range(H):
            for x in range(W):
                if px[x, y]:
                    dr.point((x, y), fill=BLACK)
    elif mode == "grey":
        for y in range(H):
            for x in range(W):
                if px[x, y] and (x + y) % 2 == 0:
                    dr.point((x, y), fill=BLACK)
    # outline: any silhouette pixel adjacent to background
    for y in range(H):
        for x in range(W):
            if px[x, y]:
                if x == 0 or y == 0 or x == W-1 or y == H-1 or \
                   not px[x-1, y] or not px[x+1, y] or not px[x, y-1] or not px[x, y+1]:
                    dr.point((x, y), fill=BLACK)

def dragon_silhouette(scale, frame, horns, big_wings):
    """Return a function drawing a side-view dragon silhouette (white on mask)."""
    def fn(md):
        cx, cy = 30, 34
        s = scale
        # body
        bw, bh = int(16*s), int(13*s)
        md.ellipse([cx-bw, cy-bh, cx+bw, cy+bh], fill=1)
        # tail (curls right), flaps with frame
        ty = cy + (2 if frame else -1)
        md.polygon([(cx+bw-2, cy), (cx+bw+int(16*s), ty-3),
                    (cx+bw+int(18*s), ty+2), (cx+bw-2, cy+6)], fill=1)
        md.ellipse([cx+bw+int(14*s), ty-6, cx+bw+int(22*s), ty+2], fill=1)  # tail spade
        # neck + head (up-left)
        hx, hy = cx-int(12*s), cy-int(12*s)
        md.polygon([(cx-4, cy-bh+2), (cx-bw, cy-2), (hx+6, hy+8), (hx+8, hy-2)], fill=1)
        hr = int(8*s)
        md.ellipse([hx-hr, hy-hr, hx+hr, hy+hr], fill=1)          # head
        md.ellipse([hx-hr-int(6*s), hy-2, hx-hr+2, hy+int(6*s)], fill=1)  # snout
        # horns
        if horns:
            md.polygon([(hx, hy-hr), (hx-3, hy-hr-int(8*s)), (hx+3, hy-hr+1)], fill=1)
            md.polygon([(hx+4, hy-hr+1), (hx+7, hy-hr-int(5*s)), (hx+8, hy-hr+3)], fill=1)
        # wing (behind body, a pointed bat wing; flaps with frame)
        if big_wings:
            lift = int(6*s) if frame else 0
            base = (cx-2, cy-2)
            md.polygon([base,
                        (cx+int(2*s), cy-int(24*s)-lift),
                        (cx+int(9*s), cy-int(16*s)-lift),
                        (cx+int(7*s), cy-int(9*s)),
                        (cx+int(15*s), cy-int(13*s)-lift//2),
                        (cx+int(12*s), cy-int(4*s)),
                        (cx+int(18*s), cy-int(6*s))], fill=1)
        else:
            wy = cy - (7 if frame else 3)
            md.polygon([(cx, cy-3), (cx+4, wy-11), (cx+9, wy-4), (cx+12, wy-6)], fill=1)
        # legs
        md.ellipse([cx-8, cy+bh-3, cx-2, cy+bh+int(6*s)], fill=1)
        md.ellipse([cx+2, cy+bh-3, cx+8, cy+bh+int(6*s)], fill=1)
    return fn

def eye(dr, hx, hy, mode, closed=False):
    col = WHITE if mode == "black" else BLACK
    if closed:
        dr.line([hx-2, hy, hx+2, hy], fill=col)   # closed eye
    else:
        dr.ellipse([hx-2, hy-2, hx+1, hy+1], fill=col)

def mouth_open(dr, hx, hy, hr, mode):
    """Draw an open mouth (a wedge) under the snout for the eating pose."""
    col = WHITE if mode == "black" else BLACK
    sx = hx - hr - int(4)
    dr.polygon([(sx, hy+1), (sx-5, hy+3), (sx, hy+5)], fill=col)

def save(im, name):
    im.save(f"assets/{name}_60x60.png")

def make_dragon(name, scale, horns, big_wings, mode):
    hx, hy = 30-int(12*scale), 34-int(12*scale)
    hr = int(8*scale)
    for frame in (0, 1):
        im, dr = new_img()
        fill_shape(im, dr, dragon_silhouette(scale, frame, horns, big_wings), mode)
        eye(dr, hx, hy, mode)
        save(im, f"{name}_0{frame}")
    # sleeping pose: eyes closed
    im, dr = new_img()
    fill_shape(im, dr, dragon_silhouette(scale, 0, horns, big_wings), mode)
    eye(dr, hx, hy, mode, closed=True)
    save(im, f"{name}_sleep")
    # eating pose: open mouth
    im, dr = new_img()
    fill_shape(im, dr, dragon_silhouette(scale, 0, horns, big_wings), mode)
    eye(dr, hx, hy, mode)
    mouth_open(dr, hx, hy, hr, mode)
    save(im, f"{name}_eat")

def make_egg():
    for frame in (0, 1):
        im, dr = new_img()
        def fn(md):
            md.ellipse([18, 12, 42, 50], fill=1)
        fill_shape(im, dr, fn, "white")
        # scales / spots
        for (x, y) in [(24, 22), (32, 20), (28, 30), (34, 34), (23, 38)]:
            dr.ellipse([x, y, x+3, y+3], fill=BLACK)
        # crack (frame 1 wider)
        pts = [(30, 12), (27, 22), (33, 26), (28, 34), (31, 44)] if frame else \
              [(30, 12), (28, 22), (32, 26), (29, 34), (30, 44)]
        dr.line(pts, fill=BLACK, width=1)
        save(im, f"egg_0{frame}")

def make_dead():
    for frame in (0, 1):
        im, dr = new_img()
        def fn(md):
            md.ellipse([12, 34, 48, 50], fill=1)   # lying body
            md.ellipse([8, 26, 26, 44], fill=1)     # head down
        fill_shape(im, dr, fn, "white")
        # X eyes
        for ex in (14, 19):
            dr.line([ex, 32, ex+3, 35], fill=BLACK)
            dr.line([ex, 35, ex+3, 32], fill=BLACK)
        # tombstone-ish tick on frame1
        if frame:
            dr.line([40, 20, 40, 34], fill=BLACK)
            dr.line([34, 24, 46, 24], fill=BLACK)
        save(im, "dead_0%d" % frame)

if __name__ == "__main__":
    make_egg()
    make_dragon("hatch", 0.55, horns=False, big_wings=False, mode="black")
    make_dragon("wyrm", 0.75, horns=False, big_wings=False, mode="black")
    make_dragon("drake", 0.95, horns=True, big_wings=True, mode="black")
    make_dragon("adult_white", 1.1, horns=True, big_wings=True, mode="white")
    make_dragon("adult_grey", 1.1, horns=True, big_wings=True, mode="grey")
    make_dragon("adult_black", 1.1, horns=True, big_wings=True, mode="black")
    make_dead()
    print("generated dragon sprites")

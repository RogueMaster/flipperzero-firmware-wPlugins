#!/usr/bin/env python3
# Generate 128x64 monochrome PNG screenshots mimicking the in-game look.
# Output: /workspace/screenshots/ss0.png .. ss4.png
from PIL import Image, ImageDraw, ImageFont
import os
import math

OUT = "/workspace/screenshots"
os.makedirs(OUT, exist_ok=True)
W, H = 128, 64

# Try to find a CJK font (for Chinese menu)
CJK_PATHS = [
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
]
def load_cjk(size):
    for p in CJK_PATHS:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()

# ---- Maze map for raycasting demo (1=wall, 0=empty) ----
MAP = [
    "1111111111111111",
    "1..............1",
    "1.111.1.111.1..1",
    "1...1.....1....1",
    "1.1.1111.1.111.1",
    "1.1........1...1",
    "1.1.111.1.1.1.1",
    "1.....1.1.1.1.1",
    "1.111.1.1.1.1.1",
    "1.....1...1...1",
    "1.111.11111.1.1",
    "1.1.........1.1",
    "1.1.1111111.1.1",
    "1...........1.1",
    "1111111111111E1",
]
MW = len(MAP[0])
MH = len(MAP)

def is_wall(mx, my):
    if mx < 0 or mx >= MW or my < 0 or my >= MH:
        return True
    c = MAP[my][mx]
    return c == '1'

def cast(px, py, angle):
    # Returns (distance, side, mx, my, is_exit)
    dx = math.cos(angle)
    dy = math.sin(angle)
    step = 0.04
    dist = 0.0
    last_mx = int(px)
    last_my = int(py)
    side = 0
    while dist < 16.0:
        dist += step
        x = px + dx * dist
        y = py + dy * dist
        mx, my = int(x), int(y)
        if is_wall(mx, my):
            # determine side
            side = 0 if (abs(x - mx) > abs(y - my)) else 1
            c = MAP[my][mx] if 0 <= my < MH and 0 <= mx < MW else '1'
            return dist, side, mx, my, (c == 'E')
    return 16.0, 0, last_mx, last_my, False

def render_scene(img, px, py, facing):
    d = ImageDraw.Draw(img)
    # sky (top) and floor (bottom)
    horizon = 32
    # floor dithering
    for y in range(horizon, H):
        t = (y - horizon) / (H - horizon)
        if (y % 2) == 0:
            d.point([(x, y) for x in range(0, W, 2)], fill=1)
        else:
            d.point([(x, y) for x in range(1, W, 2)], fill=1)
    # raycast
    fov = math.pi / 3
    for col in range(0, W, 2):  # half-res
        ray = facing - fov/2 + (col / W) * fov
        dist, side, mx, my, is_exit = cast(px, py, ray)
        if dist < 0.05:
            continue
        # fisheye correction
        dist = dist * math.cos(ray - facing)
        lineH = min(H, int(H / max(dist, 0.1)))
        drawStart = max(0, horizon - lineH // 2)
        drawEnd = min(H, horizon + lineH // 2)
        # shade: distant walls lighter (skip pixels)
        shade = 1
        if dist > 4: shade = 2
        if dist > 8: shade = 3
        for y in range(drawStart, drawEnd):
            if shade == 1:
                on = True
            elif shade == 2:
                on = ((y + col) % 2 == 0)
            else:
                on = ((y % 2 == 0) and (col % 4 == 0))
            # add some texture variation
            if on and (mx + my) % 2 == 0 and y % 3 == 0:
                on = not on
            if is_exit:
                # pulsing exit (always bright)
                on = (y % 2 == 0)
            if on:
                img.putpixel((col, y), 1)
                img.putpixel((col+1, y), 1)

# ---- HUD ----
def draw_hud(img, lang_zh=True, level=3, floor=0, keys=2, torches=1, hp=3, show_items=False, show_hp=False):
    d = ImageDraw.Draw(img)
    fnt = load_cjk(8)
    # black bar background already inverted in image; draw black text on white
    x = 1
    def text(s, fx, fy):
        d.text((fx, fy), s, fill=0, font=fnt)
    def num(n, fx, fy):
        text(str(n), fx, fy)
    if level > 0 and floor == 0:
        num(level, x, 0); text("关" if lang_zh else "LV", x+7, 0); x += 16
    elif floor > 0:
        num(floor, x, 0); text("层" if lang_zh else "FL", x+7, 0); x += 16
    if show_items:
        num(keys, x, 0); text("钥" if lang_zh else "K", x+6, 0); x += 13
        num(torches, x, 0); text("火" if lang_zh else "T", x+6, 0); x += 13
    if show_hp:
        num(hp, x, 0); text("血" if lang_zh else "HP", x+6, 0)

def draw_compass(img, exit_dx, exit_dy):
    d = ImageDraw.Draw(img)
    # compass at top-right
    cx, cy = 122, 5
    if exit_dx == 0 and exit_dy == 0:
        return
    ang = math.atan2(exit_dy, exit_dx)
    ex = cx + int(math.cos(ang) * 4)
    ey = cy + int(math.sin(ang) * 4)
    d.line([(cx, cy), (ex, ey)], fill=1, width=1)
    d.point([(cx, cy)], fill=1)

def draw_minimap(img, px, py, facing, mx_range=5):
    d = ImageDraw.Draw(img)
    # minimap in bottom-right corner (28x28 area, position 98..126 x, 36..62 y)
    bx, by = 100, 36
    bs = 4
    for my in range(-3, 4):
        for mx in range(-3, 4):
            wx = int(px) + mx
            wy = int(py) + my
            cell = is_wall(wx, wy)
            sx = bx + (mx + 3) * bs
            sy = by + (my + 3) * bs
            if cell:
                d.rectangle([(sx, sy), (sx+bs-1, sy+bs-1)], fill=1)
    # player
    sx = bx + 3 * bs + bs//2
    sy = by + 3 * bs + bs//2
    d.point([(sx, sy)], fill=0)

# ---- Frame 0: Chinese menu ----
def gen_menu(lang_zh, idx):
    img = Image.new("1", (W, H), 0)
    d = ImageDraw.Draw(img)
    title_f = load_cjk(13)
    item_f = load_cjk(10)
    small_f = load_cjk(8)
    if lang_zh:
        d.text((46, 1), "3D迷宫", fill=1, font=title_f)
        items = ["1.闯关模式", "2.无尽挑战", "3.游客漫游"]
        hint = "上下选 OK开始 Back退"
        lang = "Lang: CN  <-  ->"
    else:
        d.text((44, 1), "3D MAZE", fill=1, font=title_f)
        items = ["1. Campaign", "2. Endless", "3. Visitor"]
        hint = "Up/Dn Select  OK Start  Back Exit"
        lang = "Lang: EN  <-  ->"
    d.line([(0, 16), (127, 16)], fill=1)
    for i, it in enumerate(items):
        yy = 22 + i * 11
        if i == 0:
            d.rectangle([(0, yy - 9), (127, yy + 1)], fill=1)
            d.text((6, yy - 8), it, fill=0, font=item_f)
        else:
            d.text((6, yy - 8), it, fill=1, font=item_f)
    d.text((4, 22 + 3 * 11 + 2), lang, fill=1, font=small_f)
    d.text((2, 56), hint, fill=1, font=small_f)
    img.save(f"{OUT}/ss{idx}.png")

# ---- Frame 2: 3D gameplay (campaign) ----
def gen_gameplay(idx, level=3, lang_zh=True, exit_ahead=False):
    img = Image.new("1", (W, H), 0)
    px, py = 7.5, 7.5
    facing = 0.0 if not exit_ahead else math.pi/2  # face exit
    render_scene(img, px, py, facing)
    draw_hud(img, lang_zh=lang_zh, level=level, show_items=(level >= 11), show_hp=(level >= 21))
    # compass pointing to exit (bottom-right corner of map)
    draw_compass(img, 0, 1)
    draw_minimap(img, px, py, facing)
    if exit_ahead:
        f = load_cjk(8)
        d = ImageDraw.Draw(img)
        msg = "出口在前方" if lang_zh else "Exit Ahead"
        d.text((2, 56), msg, fill=1, font=f)
    img.save(f"{OUT}/ss{idx}.png")

# ---- Frame 4: Level clear overlay ----
def gen_clear(idx, lang_zh=True):
    img = Image.new("1", (W, H), 0)
    px, py = 7.5, 7.5
    render_scene(img, px, py, 0.3)
    d = ImageDraw.Draw(img)
    # overlay panel
    d.rectangle([(16, 16), (112, 50)], fill=0, outline=1)
    f1 = load_cjk(16)
    f2 = load_cjk(8)
    if lang_zh:
        d.text((48, 20), "过关!", fill=1, font=f1)
        d.text((28, 42), "OK继续  Back回主菜单", fill=1, font=f2)
    else:
        d.text((44, 22), "CLEAR!", fill=1, font=f1)
        d.text((28, 42), "OK:Next    Back:Menu", fill=1, font=f2)
    img.save(f"{OUT}/ss{idx}.png")

# ---- Generate all ----
gen_menu(True, 0)   # ss0: Chinese menu
gen_menu(False, 1)  # ss1: English menu
gen_gameplay(2, level=3, lang_zh=True, exit_ahead=True)   # ss2: campaign, exit ahead
gen_gameplay(3, level=15, lang_zh=False, exit_ahead=False) # ss3: English, puzzle stage
gen_clear(4, lang_zh=True)  # ss4: level clear

print("Generated:")
for f in sorted(os.listdir(OUT)):
    print(" ", f, os.path.getsize(os.path.join(OUT, f)), "bytes")

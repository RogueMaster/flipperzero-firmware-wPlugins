#!/usr/bin/env python3
# 生成设置/音效/开场/开/关 等紧凑size8位图, 追加到 zh_chars.h
from PIL import Image, ImageDraw, ImageFont

font_path = "/root/.fonts/NotoSansCJKsc-Regular.otf"
F = ImageFont.truetype(font_path, 8)

items = [
    ("m4",           "4.设置",     10, 12),
    ("settings_hdr", "设置",       11, 14),
    ("set_sfx",      "音效",       8, 10),
    ("set_opening",  "开场动画",   8, 10),
    ("set_on",       "开",         7, 10),
    ("set_off",      "关",         7, 10),
    ("set_back",     "Back返回",   8, 10),
    ("set_select",   "上下选 OK切",8, 10),
    ("open_by",      "k20120509 presents", 8, 10),
]

def render(text, font_size, target_h):
    bbox = F.getbbox(text)
    ox, oy = bbox[0], bbox[1]
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    img = Image.new("1", (w, h), 0)
    d = ImageDraw.Draw(img)
    d.text((-ox, -oy), text, fill=1, font=F)
    return w, h, img

def to_xbm_bytes(w, h, img):
    px = img.load()
    bpr = (w + 7) // 8
    out = bytearray()
    for y in range(h):
        for bcol in range(bpr):
            byte = 0
            for bit in range(8):
                x = bcol * 8 + bit
                if x < w and px[x, y]:
                    byte |= 1 << bit
            out.append(byte)
    return out, bpr

out_lines = []
out_lines.append("")
out_lines.append("// ===== v4.3 设置与开场 中文位图 (紧凑 size 8) =====")

for name, s, fs, th in items:
    w, h, img = render(s, fs, th)
    data, bpr = to_xbm_bytes(w, h, img)
    out_lines.append(f"// {s}")
    out_lines.append(f"#define {name.upper()}_W {w}")
    out_lines.append(f"#define {name.upper()}_H {h}")
    out_lines.append(f"#define {name.upper()}_BPR {bpr}")
    out_lines.append(f"static const uint8_t {name}_bits[] = {{")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        out_lines.append("    " + ",".join(f"0x{b:02x}" for b in chunk) + ",")
    out_lines.append("};")
    out_lines.append("")

with open("/workspace/zh_chars.h", "a") as f:
    f.write("\n".join(out_lines))
print("appended settings/opening bitmaps:", len(items))

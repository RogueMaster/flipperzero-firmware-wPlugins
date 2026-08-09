#!/usr/bin/env python3
# 生成所有中文字符串的 XBM 位图, 输出到 zh_chars.h
from PIL import Image, ImageDraw, ImageFont

font_path = "/root/.fonts/NotoSansCJKsc-Regular.otf"
F = ImageFont.truetype(font_path, 11)

strings = [
    # 标题
    ("title", "3D迷宫", 16, 28),
    # 菜单项 (高8的字体)
    ("m1", "1.剧情模式", 10, 12),
    ("m2", "2.无尽模式", 10, 12),
    ("m3", "3.游客漫游", 10, 12),
    # 菜单底部提示 (高8字体,更细)
    ("hint_menu", "上下选 左右切语言 OK开始", 9, 12),
    # HUD 标签
    ("hud_lv", "关", 7, 8),
    ("hud_floor", "层", 7, 8),
    ("hud_hp", "血", 7, 8),
    ("hud_key", "钥", 7, 8),
    ("hud_torch", "火", 7, 8),
    # 覆盖层标题
    ("ov_clear", "过关!", 11, 16),
    ("ov_over", "阵亡", 11, 16),
    ("ov_paused", "暂停", 11, 16),
    # 覆盖层按钮
    ("ov_btns", "OK继续  Back回主菜单", 8, 12),
    ("ov_btns2", "OK重试  Back回主菜单", 8, 12),
    ("ov_btns3", "OK恢复  Back回主菜单", 8, 12),
    # 游戏内提示
    ("msg_key", "拿到钥匙!", 9, 12),
    ("msg_torch", "火把+1", 9, 12),
    ("msg_trap", "陷阱!-1血", 9, 12),
    ("msg_door", "门已开", 9, 12),
    ("msg_needkey", "需要钥匙", 9, 12),
    ("msg_findexit", "去找出口!", 9, 12),
    ("msg_care", "小心敌人!", 9, 12),
    ("msg_puzzle", "找钥匙开门", 9, 12),
    ("msg_visitor", "游客模式", 9, 12),
    ("msg_run", "无尽挑战", 9, 12),
    ("msg_hit", "受伤!-1血", 9, 12),
    ("msg_exit", "出口在前方", 9, 12),
    # ===== v3.2.0 新增: 剧情模式 / 物品栏 / 层级选择 中文 =====
    # 剧情标题
    ("story_t0", "序章", 11, 14),
    ("story_t1", "下行", 11, 14),
    ("story_t2", "脱险", 11, 14),
    # 剧情页码提示
    ("story_hint", "OK翻页 Back跳过", 9, 12),
    ("story_choose", "OK选甲 →选乙", 9, 12),
    # 序章页1 (4 行)
    ("p0_0_0", "你在石殿迷宫中醒来", 9, 12),
    ("p0_0_1", "不记来路火把摇曳", 9, 12),
    ("p0_0_2", "墙壁刻满奇异符文", 9, 12),
    ("p0_0_3", "远处回响你非独处", 9, 12),
    # 序章页2
    ("p0_1_0", "传说变幻之迷宫", 9, 12),
    ("p0_1_1", "是吞噬迷者的牢", 9, 12),
    ("p0_1_2", "层层更深少有人逃", 9, 12),
    ("p0_1_3", "更少有人记得归路", 9, 12),
    # 序章页3
    ("p0_2_0", "两条路在面前展开", 9, 12),
    ("p0_2_1", "符文低语命运抉择", 9, 12),
    ("p0_2_2", "心要坚硬墙在倾听", 9, 12),
    ("p0_2_3", "请慎重选择：", 9, 12),
    # 选项 A / B
    ("choice_a", "甲 武者 +血 无道具", 9, 12),
    ("choice_b", "乙 探索 -血 +1火把", 9, 12),
    # 过场页
    ("p1_0_0", "地面塌陷", 9, 12),
    ("p1_0_1", "你坠入更深处", 9, 12),
    ("p1_0_2", "墙更古老更饥饿", 9, 12),
    ("p1_0_3", "另一出口在等待", 9, 12),
    ("p1_0_4", "下行 求生", 9, 12),
    # 结局页
    ("p2_0_0", "光透过石头", 9, 12),
    ("p2_0_1", "你跌出迷宫", 9, 12),
    ("p2_0_2", "低语渐远", 9, 12),
    ("p2_0_3", "你逃出了……暂时", 9, 12),
    ("p2_0_4", "迷宫永不放手", 9, 12),
    # 物品栏
    ("inv_title", "物品栏", 11, 14),
    ("inv_hint", "上下选 OK用 Back返", 9, 12),
    ("inv_key", "钥匙", 9, 12),
    ("inv_torch", "火把", 9, 12),
    ("inv_potion", "药水", 9, 12),
    ("inv_amulet", "护符", 9, 12),
    ("inv_empty", "(空)", 9, 12),
    # 层级选择
    ("ls_title_s", "剧情 选层", 11, 14),
    ("ls_title_e", "无尽 选层", 11, 14),
    ("ls_hint", "上下层 OK进 Back返", 9, 12),
    ("ls_locked", "锁", 9, 12),
    ("ls_cleared", "通", 9, 12),
]


def render(text, font_size, target_h):
    f = F
    bbox = f.getbbox(text)
    ox, oy = bbox[0], bbox[1]
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    img = Image.new("1", (w, h), 0)
    d = ImageDraw.Draw(img)
    d.text((-ox, -oy), text, fill=1, font=f)
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
out_lines.append("#pragma once")
out_lines.append("#include <stdint.h>")
out_lines.append("")

for name, s, fs, th in strings:
    w, h, img = render(s, fs, th)
    data, bpr = to_xbm_bytes(w, h, img)
    out_lines.append(f"// {s}")
    out_lines.append(f"#define {name.upper()}_W {w}")
    out_lines.append(f"#define {name.upper()}_H {h}")
    out_lines.append(f"#define {name.upper()}_BPR {bpr}")
    out_lines.append(f"static const uint8_t {name}_bits[] = {{")
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        out_lines.append("    " + ",".join(f"0x{b:02x}" for b in chunk) + ",")
    out_lines.append("};")
    out_lines.append("")

with open("/workspace/zh_chars.h", "w") as f:
    f.write("\n".join(out_lines))
print("wrote zh_chars.h, items:", len(strings))

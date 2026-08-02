#include "maze3d.h"
#include "zh_chars.h"
#include "i18n.h"
#include <gui/gui.h>
#include <string.h>
#include <stdio.h>

#define EVENT_INPUT 1

typedef struct {
    uint8_t type;
    InputEvent input;
} AppEvent;

typedef enum {
    M_CAMPAIGN = 0,
    M_ENDLESS  = 1,
    M_VISITOR  = 2,
    M_COUNT = 3,
} MenuItem;

static int s_sel = 0;
static GameMode s_resume_mode = MODE_CAMPAIGN;

// 根据 msg_id 返回对应中文位图
static void get_msg_bmp(int id, const uint8_t** bits, int* w, int* h, int* bpr) {
    switch(id) {
        case MSG_KEY:     *bits = msg_key_bits;     *w = MSG_KEY_W;     *h = MSG_KEY_H;     *bpr = MSG_KEY_BPR;     break;
        case MSG_TORCH:   *bits = msg_torch_bits;   *w = MSG_TORCH_W;   *h = MSG_TORCH_H;   *bpr = MSG_TORCH_BPR;   break;
        case MSG_TRAP:    *bits = msg_trap_bits;    *w = MSG_TRAP_W;    *h = MSG_TRAP_H;    *bpr = MSG_TRAP_BPR;    break;
        case MSG_DOOR:    *bits = msg_door_bits;    *w = MSG_DOOR_W;    *h = MSG_DOOR_H;    *bpr = MSG_DOOR_BPR;    break;
        case MSG_NEEDKEY: *bits = msg_needkey_bits; *w = MSG_NEEDKEY_W; *h = MSG_NEEDKEY_H; *bpr = MSG_NEEDKEY_BPR; break;
        case MSG_FINDEXIT:*bits = msg_findexit_bits;*w = MSG_FINDEXIT_W;*h = MSG_FINDEXIT_H;*bpr = MSG_FINDEXIT_BPR;break;
        case MSG_CARE:    *bits = msg_care_bits;    *w = MSG_CARE_W;    *h = MSG_CARE_H;    *bpr = MSG_CARE_BPR;    break;
        case MSG_PUZZLE:  *bits = msg_puzzle_bits;  *w = MSG_PUZZLE_W;  *h = MSG_PUZZLE_H;  *bpr = MSG_PUZZLE_BPR;  break;
        case MSG_VISITOR: *bits = msg_visitor_bits; *w = MSG_VISITOR_W; *h = MSG_VISITOR_H; *bpr = MSG_VISITOR_BPR; break;
        case MSG_RUN:     *bits = msg_run_bits;     *w = MSG_RUN_W;     *h = MSG_RUN_H;     *bpr = MSG_RUN_BPR;     break;
        case MSG_HIT:     *bits = msg_hit_bits;     *w = MSG_HIT_W;     *h = MSG_HIT_H;     *bpr = MSG_HIT_BPR;     break;
        case MSG_EXIT:    *bits = msg_exit_bits;    *w = MSG_EXIT_W;    *h = MSG_EXIT_H;    *bpr = MSG_EXIT_BPR;    break;
        default: *bits = NULL; *w = *h = *bpr = 0;
    }
}

// 在 canvas 上直接绘制数字(Flipper 内置ASCII即可)
static void canvas_draw_num(Canvas* c, int x, int y, int n) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", n);
    canvas_draw_str(c, x, y, buf);
}

// ---- 新界面绘制 ----
static const char* en_item_name(int t) {
    switch(t) {
        case ITEM_KEY:    return EN_INV_KEY;
        case ITEM_TORCH:  return EN_INV_TORCH;
        case ITEM_POTION: return EN_INV_POTION;
        case ITEM_AMULET: return EN_INV_AMULET;
        default: return "";
    }
}

// 中文剧情页行数表 (与下方 zh_story_lines 一一对应)
static const int zh_story_lines_per_page[3][5] = {
    {4, 4, 4, 0, 0},   // story 0 (intro): 3 页, 每页 4 行
    {5, 0, 0, 0, 0},   // story 1 (between): 1 页 5 行
    {5, 0, 0, 0, 0},   // story 2 (ending): 1 页 5 行
};
// 中文剧情每行位图查表: story_id, page, line -> (bits, w, h)
static void zh_story_line(int sid, int page, int line,
                          const uint8_t** bits, int* w, int* h) {
    // 把 (sid,page,line) 压成一个 case id
    int id = sid * 100 + page * 10 + line;
    switch(id) {
        // story 0 page 0
        case 0:   *bits = p0_0_0_bits; *w = P0_0_0_W; *h = P0_0_0_H; break;
        case 1:   *bits = p0_0_1_bits; *w = P0_0_1_W; *h = P0_0_1_H; break;
        case 2:   *bits = p0_0_2_bits; *w = P0_0_2_W; *h = P0_0_2_H; break;
        case 3:   *bits = p0_0_3_bits; *w = P0_0_3_W; *h = P0_0_3_H; break;
        // story 0 page 1
        case 10:  *bits = p0_1_0_bits; *w = P0_1_0_W; *h = P0_1_0_H; break;
        case 11:  *bits = p0_1_1_bits; *w = P0_1_1_W; *h = P0_1_1_H; break;
        case 12:  *bits = p0_1_2_bits; *w = P0_1_2_W; *h = P0_1_2_H; break;
        case 13:  *bits = p0_1_3_bits; *w = P0_1_3_W; *h = P0_1_3_H; break;
        // story 0 page 2
        case 20:  *bits = p0_2_0_bits; *w = P0_2_0_W; *h = P0_2_0_H; break;
        case 21:  *bits = p0_2_1_bits; *w = P0_2_1_W; *h = P0_2_1_H; break;
        case 22:  *bits = p0_2_2_bits; *w = P0_2_2_W; *h = P0_2_2_H; break;
        case 23:  *bits = p0_2_3_bits; *w = P0_2_3_W; *h = P0_2_3_H; break;
        // story 1 page 0
        case 100: *bits = p1_0_0_bits; *w = P1_0_0_W; *h = P1_0_0_H; break;
        case 101: *bits = p1_0_1_bits; *w = P1_0_1_W; *h = P1_0_1_H; break;
        case 102: *bits = p1_0_2_bits; *w = P1_0_2_W; *h = P1_0_2_H; break;
        case 103: *bits = p1_0_3_bits; *w = P1_0_3_W; *h = P1_0_3_H; break;
        case 104: *bits = p1_0_4_bits; *w = P1_0_4_W; *h = P1_0_4_H; break;
        // story 2 page 0
        case 200: *bits = p2_0_0_bits; *w = P2_0_0_W; *h = P2_0_0_H; break;
        case 201: *bits = p2_0_1_bits; *w = P2_0_1_W; *h = P2_0_1_H; break;
        case 202: *bits = p2_0_2_bits; *w = P2_0_2_W; *h = P2_0_2_H; break;
        case 203: *bits = p2_0_3_bits; *w = P2_0_3_W; *h = P2_0_3_H; break;
        case 204: *bits = p2_0_4_bits; *w = P2_0_4_W; *h = P2_0_4_H; break;
        default:  *bits = NULL; *w = *h = 0;
    }
}
static const uint8_t* zh_story_title(int sid, int* w, int* h) {
    switch(sid) {
        case 0:  *w = STORY_T0_W; *h = STORY_T0_H; return story_t0_bits;
        case 1:  *w = STORY_T1_W; *h = STORY_T1_H; return story_t1_bits;
        case 2:  *w = STORY_T2_W; *h = STORY_T2_H; return story_t2_bits;
        default: *w = *h = 0; return NULL;
    }
}

static void draw_story(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    if(g.lang == LANG_ZH) {
        // 中文: 标题位图居中
        int tw, th; const uint8_t* tb = zh_story_title(g.story_id, &tw, &th);
        if(tb) canvas_draw_xbm(c, (128 - tw) / 2, 1, tw, th, tb);
        // 页码 (数字 ASCII)
        char pg[16];
        snprintf(pg, sizeof(pg), "%d/%d", g.story_page + 1, story_pages(g.story_id));
        canvas_draw_str_aligned(c, 126, 2, AlignRight, AlignTop, pg);
        // 正文行位图
        int nlines = zh_story_lines_per_page[g.story_id][g.story_page];
        int y = 16;
        for(int i = 0; i < nlines && y < 50; i++) {
            int lw, lh; const uint8_t* lb;
            zh_story_line(g.story_id, g.story_page, i, &lb, &lw, &lh);
            if(lb) {
                canvas_draw_xbm(c, 2, y, lw, lh, lb);
                y += lh + 1;
            }
        }
        int np = story_pages(g.story_id);
        if(g.story_page < np - 1) {
            canvas_draw_xbm(c, 2, 62 - STORY_HINT_H + 1, STORY_HINT_W, STORY_HINT_H, story_hint_bits);
        } else {
            // 最后一页: 选项 A/B 位图 + 光标
            int selY = (g.story_choice == 0) ? 48 : 58;
            canvas_draw_str(c, 0, selY, ">");
            canvas_draw_xbm(c, 8, 48 - CHOICE_A_H + 1, CHOICE_A_W, CHOICE_A_H, choice_a_bits);
            canvas_draw_xbm(c, 8, 58 - CHOICE_B_H + 1, CHOICE_B_W, CHOICE_B_H, choice_b_bits);
        }
        return;
    }

    // 英文 (原逻辑)
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, story_title(g.story_id));
    canvas_set_font(c, FontSecondary);
    const char* text = story_page_text(g.story_id, g.story_page);
    const char* p = text;
    int y = 14;
    while(*p && y < 50) {
        const char* nl = p;
        while(*nl && *nl != '\n') nl++;
        int len = nl - p;
        char line[24];
        if(len > 23) len = 23;
        memcpy(line, p, len); line[len] = 0;
        canvas_draw_str(c, 2, y, line);
        y += 8;
        p = (*nl == '\n') ? nl + 1 : nl;
    }
    char pg[16];
    snprintf(pg, sizeof(pg), "%d/%d", g.story_page + 1, story_pages(g.story_id));
    canvas_draw_str_aligned(c, 126, 1, AlignRight, AlignTop, pg);
    int np = story_pages(g.story_id);
    if(g.story_page < np - 1) {
        canvas_draw_str(c, 2, 62, EN_STORY_HINT);
    } else {
        canvas_draw_str(c, 8, 50, story_choice_a(g.story_id));
        canvas_draw_str(c, 8, 60, story_choice_b(g.story_id));
        canvas_draw_str(c, 0, (g.story_choice == 0) ? 50 : 60, ">");
    }
}

// 中文物品名位图
static void zh_item_name(int item, const uint8_t** bits, int* w, int* h) {
    switch(item) {
        case 0:  *bits = inv_key_bits;    *w = INV_KEY_W;    *h = INV_KEY_H;    break;
        case 1:  *bits = inv_torch_bits;  *w = INV_TORCH_W;  *h = INV_TORCH_H;  break;
        case 2:  *bits = inv_potion_bits; *w = INV_POTION_W; *h = INV_POTION_H; break;
        case 3:  *bits = inv_amulet_bits; *w = INV_AMULET_W; *h = INV_AMULET_H; break;
        default: *bits = inv_empty_bits;  *w = INV_EMPTY_W;  *h = INV_EMPTY_H;  break;
    }
}

static void draw_inventory(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    if(g.lang == LANG_ZH) {
        // 标题
        canvas_draw_xbm(c, (128 - INV_TITLE_W) / 2, 1, INV_TITLE_W, INV_TITLE_H, inv_title_bits);
        // 物品行
        for(int i = 0; i < ITEM_COUNT; i++) {
            int y = 16 + i * 10;
            int nw, nh; const uint8_t* nb;
            zh_item_name(i, &nb, &nw, &nh);
            // 数量 ASCII
            char cnt[8]; snprintf(cnt, sizeof(cnt), "x%d", item_count(i));
            if(i == g.inv_sel) {
                canvas_draw_box(c, 0, y - 1, 128, 11);
                canvas_set_color(c, ColorWhite);
            }
            // 光标 >
            if(i == g.inv_sel) canvas_draw_str(c, 0, y + 8, ">");
            if(nb) canvas_draw_xbm(c, 10, y, nw, nh, nb);
            canvas_draw_str(c, 110, y + 8, cnt);
            canvas_set_color(c, ColorBlack);
        }
        canvas_draw_xbm(c, 2, 62 - INV_HINT_H + 1, INV_HINT_W, INV_HINT_H, inv_hint_bits);
        return;
    }

    // 英文 (原逻辑)
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, EN_INV_TITLE);
    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < ITEM_COUNT; i++) {
        int y = 14 + i * 9;
        if(i == g.inv_sel) {
            canvas_draw_box(c, 0, y - 8, 128, 9);
            canvas_set_color(c, ColorWhite);
        }
        char line[24];
        snprintf(line, sizeof(line), "%s  x%d", en_item_name(i), item_count(i));
        canvas_draw_str(c, 4, y, line);
        canvas_set_color(c, ColorBlack);
    }
    canvas_draw_str(c, 2, 62, EN_INV_HINT);
}

static void draw_level_select(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    if(g.lang == LANG_ZH) {
        // 标题
        const uint8_t* tb; int tw, th;
        if(g.ls_for_campaign) { tb = ls_title_s_bits; tw = LS_TITLE_S_W; th = LS_TITLE_S_H; }
        else                  { tb = ls_title_e_bits; tw = LS_TITLE_E_W; th = LS_TITLE_E_H; }
        canvas_draw_xbm(c, (128 - tw) / 2, 1, tw, th, tb);
        // 关卡列表
        for(int i = 0; i < 6; i++) {
            int lvl = g.ls_sel - 2 + i;
            if(lvl < 1) continue;
            int y = 16 + i * 8;
            bool locked = g.ls_for_campaign && (lvl > g.campaign_cleared + 1);
            bool cleared = g.ls_for_campaign && (lvl <= g.campaign_cleared);
            if(lvl == g.ls_sel && !locked) {
                canvas_draw_box(c, 0, y - 1, 128, 9);
                canvas_set_color(c, ColorWhite);
            }
            // 关卡号 ASCII (前缀用英文 L 避免中文"层"无法用 canvas_draw_str)
            char lv[8]; snprintf(lv, sizeof(lv), "L%d", lvl);
            canvas_draw_str(c, 4, y + 7, lv);
            // 状态标签 (位图)
            int tag_w, tag_h; const uint8_t* tag_b = NULL;
            if(locked)         { tag_b = ls_locked_bits;  tag_w = LS_LOCKED_W;  tag_h = LS_LOCKED_H; }
            else if(cleared)   { tag_b = ls_cleared_bits; tag_w = LS_CLEARED_W; tag_h = LS_CLEARED_H; }
            if(tag_b) canvas_draw_xbm(c, 100, y, tag_w, tag_h, tag_b);
            canvas_set_color(c, ColorBlack);
        }
        canvas_draw_xbm(c, 2, 62 - LS_HINT_H + 1, LS_HINT_W, LS_HINT_H, ls_hint_bits);
        return;
    }

    // 英文 (原逻辑)
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop,
        g.ls_for_campaign ? EN_LS_TITLE_STORY : EN_LS_TITLE_ENDLESS);
    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < 6; i++) {
        int lvl = g.ls_sel - 2 + i;
        if(lvl < 1) continue;
        int y = 13 + i * 8;
        bool locked = g.ls_for_campaign && (lvl > g.campaign_cleared + 1);
        bool cleared = g.ls_for_campaign && (lvl <= g.campaign_cleared);
        if(lvl == g.ls_sel && !locked) {
            canvas_draw_box(c, 0, y - 7, 128, 9);
            canvas_set_color(c, ColorWhite);
        }
        char line[24];
        const char* tag = locked ? "  LOCKED" : (cleared ? "  ok" : "");
        snprintf(line, sizeof(line), "Lv %d%s", lvl, tag);
        canvas_draw_str(c, 4, y, line);
        canvas_set_color(c, ColorBlack);
    }
    canvas_draw_str(c, 2, 62, EN_LS_HINT);
}

// ---- 绘制回调 ----
static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    if(g.mode == MODE_MENU) {
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);
        if(g.lang == LANG_ZH) {
            // 中文标题(位图)
            canvas_draw_xbm(canvas, 46, 1, TITLE_W, TITLE_H, title_bits);
        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, EN_TITLE);
            canvas_set_font(canvas, FontSecondary);
        }
        // 菜单分隔线
        canvas_draw_line(canvas, 0, 16, 127, 16);

        // 菜单项: 1.闯关模式  2.无尽挑战  3.游客漫游
        const uint8_t* zh_items[M_COUNT] = { m1_bits, m2_bits, m3_bits };
        const char*    en_items[M_COUNT] = { EN_M1, EN_M2, EN_M3 };
        const int ws[M_COUNT] = { M1_W, M2_W, M3_W };
        const int hs[M_COUNT] = { M1_H, M2_H, M3_H };
        for(int i = 0; i < M_COUNT; i++) {
            int yy = 22 + i * 11;
            if(i == s_sel) {
                canvas_draw_box(canvas, 0, yy - 9, 128, 10);
                canvas_set_color(canvas, ColorWhite);
            }
            if(g.lang == LANG_ZH) {
                canvas_draw_xbm(canvas, 4, yy - 9, ws[i], hs[i], zh_items[i]);
            } else {
                canvas_draw_str(canvas, 6, yy - 1, en_items[i]);
            }
            canvas_set_color(canvas, ColorBlack);
        }

        // 语言切换行
        int lang_y = 22 + M_COUNT * 11 + 2;
        const char* lang_label = (g.lang == LANG_ZH) ? "Lang: CN  <-  ->" : "Lang: EN  <-  ->";
        canvas_draw_str(canvas, 4, lang_y, lang_label);

        // 底部提示
        if(g.lang == LANG_ZH) {
            canvas_draw_xbm(canvas, 2, 62 - HINT_MENU_H + 1, HINT_MENU_W, HINT_MENU_H, hint_menu_bits);
        } else {
            canvas_draw_str(canvas, 2, 62, EN_HINT_MENU);
        }
        return;
    }

    if(g.mode == MODE_STORY)         { draw_story(canvas); return; }
    if(g.mode == MODE_INVENTORY)     { draw_inventory(canvas); return; }
    if(g.mode == MODE_LEVEL_SELECT)  { draw_level_select(canvas); return; }

    // 游戏画面: 先把渲染好的 framebuffer 拷到 canvas
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, SCREEN_W, SCREEN_H, g.fb);

    // HUD 信息: 关卡/层数/钥匙/火把/血
    int x = 1;
    canvas_set_font(canvas, FontSecondary);
    if(g.mode == MODE_CAMPAIGN) {
        canvas_draw_num(canvas, x, 9, g.level);
        if(g.lang == LANG_ZH)
            canvas_draw_xbm(canvas, x + 7, 1, HUD_LV_W, HUD_LV_H, hud_lv_bits);
        else
            canvas_draw_str(canvas, x + 7, 9, EN_HUD_LV);
        x += 16;
    } else {
        canvas_draw_num(canvas, x, 9, g.endless_floor);
        if(g.lang == LANG_ZH)
            canvas_draw_xbm(canvas, x + 7, 1, HUD_FLOOR_W, HUD_FLOOR_H, hud_floor_bits);
        else
            canvas_draw_str(canvas, x + 7, 9, EN_HUD_FLOOR);
        x += 16;
    }
    if(g.stage == STAGE_PUZZLE || g.stage == STAGE_COMBAT) {
        canvas_draw_num(canvas, x, 9, g.player.keys);
        if(g.lang == LANG_ZH)
            canvas_draw_xbm(canvas, x + 6, 1, HUD_KEY_W, HUD_KEY_H, hud_key_bits);
        else
            canvas_draw_str(canvas, x + 6, 9, EN_HUD_KEY);
        x += 13;
        canvas_draw_num(canvas, x, 9, g.player.torches);
        if(g.lang == LANG_ZH)
            canvas_draw_xbm(canvas, x + 6, 1, HUD_TORCH_W, HUD_TORCH_H, hud_torch_bits);
        else
            canvas_draw_str(canvas, x + 6, 9, EN_HUD_TORCH);
        x += 13;
        // 药水 (P)
        canvas_draw_num(canvas, x, 9, g.player.potions);
        canvas_draw_str(canvas, x + 6, 9, "P");
        x += 11;
    }
    if(g.stage == STAGE_COMBAT) {
        // 护符 (A)
        canvas_draw_num(canvas, x, 9, g.player.amulets);
        canvas_draw_str(canvas, x + 6, 9, "A");
        x += 11;
        canvas_draw_num(canvas, x, 9, g.player.health);
        if(g.lang == LANG_ZH)
            canvas_draw_xbm(canvas, x + 6, 1, HUD_HP_W, HUD_HP_H, hud_hp_bits);
        else
            canvas_draw_str(canvas, x + 6, 9, EN_HUD_HP);
    }

    // 提示信息
    if(g.msg_id >= 0 && g.msg_ttl > 0) {
        if(g.lang == LANG_ZH) {
            const uint8_t* b; int w, h, bpr;
            get_msg_bmp(g.msg_id, &b, &w, &h, &bpr);
            if(b) canvas_draw_xbm(canvas, 2, 62 - h + 1, w, h, b);
        } else {
            const char* s = en_msg_str(g.msg_id);
            canvas_draw_str(canvas, 2, 62, s);
        }
    }

    // 覆盖层
    int cx, cy;
    if(g.mode == MODE_LEVEL_CLEAR) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 16, 16, 96, 34);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 16, 16, 96, 34);
        if(g.lang == LANG_ZH) {
            cx = 128/2 - OV_CLEAR_W/2; cy = 24 - OV_CLEAR_H/2;
            canvas_draw_xbm(canvas, cx, cy, OV_CLEAR_W, OV_CLEAR_H, ov_clear_bits);
            cx = 128/2 - OV_BTNS_W/2; cy = 44;
            canvas_draw_xbm(canvas, cx, cy, OV_BTNS_W, OV_BTNS_H, ov_btns_bits);
        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, EN_OV_CLEAR);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, EN_OV_BTNS);
        }
    } else if(g.mode == MODE_GAME_OVER) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 20, 18, 88, 32);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 20, 18, 88, 32);
        if(g.lang == LANG_ZH) {
            cx = 128/2 - OV_OVER_W/2; cy = 26;
            canvas_draw_xbm(canvas, cx, cy, OV_OVER_W, OV_OVER_H, ov_over_bits);
            cx = 128/2 - OV_BTNS2_W/2; cy = 42;
            canvas_draw_xbm(canvas, cx, cy, OV_BTNS2_W, OV_BTNS2_H, ov_btns2_bits);
        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, EN_OV_OVER);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, EN_OV_BTNS2);
        }
    } else if(g.mode == MODE_PAUSED) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 32, 18, 64, 28);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 32, 18, 64, 28);
        if(g.lang == LANG_ZH) {
            cx = 128/2 - OV_PAUSED_W/2; cy = 26;
            canvas_draw_xbm(canvas, cx, cy, OV_PAUSED_W, OV_PAUSED_H, ov_paused_bits);
            cx = 128/2 - OV_BTNS3_W/2; cy = 40;
            canvas_draw_xbm(canvas, cx, cy, OV_BTNS3_W, OV_BTNS3_H, ov_btns3_bits);
        } else {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, EN_OV_PAUSED);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, EN_OV_BTNS3);
        }
    }
}

static void input_callback(InputEvent* ev, void* ctx) {
    FuriMessageQueue* q = ctx;
    AppEvent e = {.type = EVENT_INPUT, .input = *ev};
    furi_message_queue_put(q, &e, FuriWaitForever);
}

static void handle_overlay_input(InputKey key) {
    if(g.mode == MODE_LEVEL_CLEAR) {
        if(key == InputKeyOk) { game_next_level(); }
        else if(key == InputKeyBack) { g.mode = MODE_MENU; }
    } else if(g.mode == MODE_GAME_OVER) {
        GameMode old = s_resume_mode;
        if(key == InputKeyOk) {
            if(old == MODE_CAMPAIGN) game_init_campaign(g.level);
            else game_init_endless(g.endless_floor, (old == MODE_ENDLESS_VISITOR));
        } else if(key == InputKeyBack) { g.mode = MODE_MENU; }
    } else if(g.mode == MODE_PAUSED) {
        if(key == InputKeyOk) { g.mode = s_resume_mode; }
        else if(key == InputKeyBack) { g.mode = MODE_MENU; }
    }
}

// ---- 新流程: 层级选择 / 剧情 / 物品栏 ----
static void enter_level_select(bool for_campaign) {
    g.mode = MODE_LEVEL_SELECT;
    g.ls_for_campaign = for_campaign;
    g.ls_max = 30;
    g.ls_sel = for_campaign ? (g.campaign_cleared + 1) : (g.endless_floor > 0 ? g.endless_floor : 1);
    if(g.ls_sel < 1) g.ls_sel = 1;
    if(g.ls_sel > g.ls_max) g.ls_sel = g.ls_max;
}

static void enter_story(int sid, GameMode ret) {
    g.mode = MODE_STORY;
    g.story_id = sid;
    g.story_page = 0;
    g.story_choice = 0;
    g.story_return = ret;
}

static void handle_new_modes_input(InputKey key, InputType type) {
    if(type != InputTypeShort) return;
    if(g.mode == MODE_STORY) {
        int np = story_pages(g.story_id);
        if(key == InputKeyOk) {
            if(g.story_page < np - 1) {
                g.story_page++;
            } else {
                // 最后一页确认: story_choice 已由 Left/Right 选好(默认0=A)
                if(g.story_return == MODE_CAMPAIGN) {
                    game_init_campaign(1);
                } else {
                    g.mode = g.story_return;
                }
            }
        } else if(key == InputKeyRight) {
            if(g.story_page == np - 1) g.story_choice = 1;
        } else if(key == InputKeyLeft) {
            if(g.story_page == np - 1) g.story_choice = 0;
        } else if(key == InputKeyBack) {
            g.mode = (g.story_return == MODE_CAMPAIGN) ? MODE_MENU : g.story_return;
        }
    } else if(g.mode == MODE_INVENTORY) {
        if(key == InputKeyUp)        g.inv_sel = (g.inv_sel + ITEM_COUNT - 1) % ITEM_COUNT;
        else if(key == InputKeyDown) g.inv_sel = (g.inv_sel + 1) % ITEM_COUNT;
        else if(key == InputKeyOk)   item_use(g.inv_sel);
        else if(key == InputKeyBack) g.mode = s_resume_mode;
    } else if(g.mode == MODE_LEVEL_SELECT) {
        if(key == InputKeyUp)        { if(g.ls_sel > 1) g.ls_sel--; }
        else if(key == InputKeyDown) { if(g.ls_sel < g.ls_max) g.ls_sel++; }
        else if(key == InputKeyOk) {
            bool locked = g.ls_for_campaign && (g.ls_sel > g.campaign_cleared + 1);
            if(!locked) {
                if(g.ls_for_campaign) {
                    // 剧情模式: 第1关且未通关过 -> 先看开场剧情
                    if(g.ls_sel == 1 && g.campaign_cleared == 0) enter_story(0, MODE_CAMPAIGN);
                    else game_init_campaign(g.ls_sel);
                } else {
                    game_init_endless(g.ls_sel, false);
                }
            }
        } else if(key == InputKeyBack) g.mode = MODE_MENU;
    }
}

// 主循环 - **关键**: 渲染受 dirty 控制; 主循环定时器用 120ms 间隔而不是 50ms,减少 CPU
int32_t maze3d_app(void* p) {
    UNUSED(p);
    memset(&g, 0, sizeof(g));
    g.mode = MODE_MENU;
    storage_load();

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(AppEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_callback, NULL);
    view_port_input_callback_set(vp, input_callback, q);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    AppEvent ev;
    bool running = true;
    uint32_t last_update_tick = 0;
    const uint32_t UPDATE_MS = 120; // ~8Hz 世界更新,避免刷太快

    while(running) {
        bool did_update_world = false;
        bool did_input = false;

        FuriStatus st = furi_message_queue_get(q, &ev, UPDATE_MS);
        if(st == FuriStatusOk && ev.type == EVENT_INPUT) {
            InputKey key = ev.input.key;
            InputType type = ev.input.type;

            if(g.mode == MODE_MENU) {
                if(type == InputTypeShort) {
                    if(key == InputKeyUp) s_sel = (s_sel + M_COUNT - 1) % M_COUNT;
                    else if(key == InputKeyDown) s_sel = (s_sel + 1) % M_COUNT;
                    else if(key == InputKeyLeft || key == InputKeyRight)
                        g.lang = (g.lang == LANG_ZH) ? LANG_EN : LANG_ZH;
                    else if(key == InputKeyOk) {
                        storage_load();
                        if(s_sel == M_CAMPAIGN) enter_level_select(true);
                        else if(s_sel == M_ENDLESS) enter_level_select(false);
                        else game_init_endless(g.endless_floor, true);
                    } else if(key == InputKeyBack) running = false;
                }
                did_input = true;
            } else if(g.mode == MODE_LEVEL_CLEAR || g.mode == MODE_GAME_OVER || g.mode == MODE_PAUSED) {
                if(type == InputTypeShort) handle_overlay_input(key);
                did_input = true;
            } else if(g.mode == MODE_STORY || g.mode == MODE_INVENTORY || g.mode == MODE_LEVEL_SELECT) {
                handle_new_modes_input(key, type);
                did_input = true;
            } else {
                // 游戏中
                if(key == InputKeyBack) {
                    if(type == InputTypeLong) running = false;
                    else if(type == InputTypeShort) {
                        s_resume_mode = g.mode;
                        g.mode = MODE_PAUSED;
                    }
                } else if(key == InputKeyOk && type == InputTypeLong) {
                    // 长按 OK 进入物品栏
                    s_resume_mode = g.mode;
                    g.mode = MODE_INVENTORY;
                    g.inv_sel = 0;
                } else {
                    game_handle_input(key, type);
                }
                did_input = true;
            }
        }

        // 定时更新世界(无论是否有事件): 敌人移动、闪烁tick
        uint32_t now = furi_get_tick(); // 注: furi HAL tick = ms
        if((now - last_update_tick) >= UPDATE_MS || did_input) {
            if(g.mode == MODE_CAMPAIGN || g.mode == MODE_ENDLESS_RUN || g.mode == MODE_ENDLESS_VISITOR) {
                game_update();
                did_update_world = true;
            }
            last_update_tick = now;
        }

        // 按需渲染,避免 20Hz 的全速 raycasting(那是死机根源)
        bool need_render = did_input || did_update_world;
        bool in_game_view = (g.mode == MODE_CAMPAIGN || g.mode == MODE_ENDLESS_RUN ||
                             g.mode == MODE_ENDLESS_VISITOR || g.mode == MODE_PAUSED ||
                             g.mode == MODE_LEVEL_CLEAR || g.mode == MODE_GAME_OVER);
        if(in_game_view && need_render) {
            engine_render();
            g.dirty = false;
        }
        view_port_update(vp);
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_message_queue_free(q);
    furi_record_close(RECORD_GUI);
    return 0;
}

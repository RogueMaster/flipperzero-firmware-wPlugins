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
    M_SETTINGS = 3,
    M_COUNT = 4,
} MenuItem;

static int s_sel = 0;
static GameMode s_resume_mode = MODE_CAMPAIGN;
// 物品栏分页: 0=物品 1=任务 (仅有任务时才可切到第2页)
static uint8_t s_inv_page = 0;
// 设置页光标 (0=音效 1=开场)
static uint8_t s_set_sel = 0;

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

// 任务标签位图 (中文)
static void zh_task_label(TaskType t, const uint8_t** bits, int* w, int* h) {
    switch(t) {
        case TASK_FIND_EXIT:  *bits = t_findexit_bits; *w = T_FINDEXIT_W; *h = T_FINDEXIT_H; break;
        case TASK_GET_KEY:    *bits = t_getkey_bits;   *w = T_GETKEY_W;   *h = T_GETKEY_H;   break;
        case TASK_OPEN_DOOR:  *bits = t_opendoor_bits; *w = T_OPENDOOR_W; *h = T_OPENDOOR_H; break;
        case TASK_KILL_ENEMY: *bits = t_kill_bits;     *w = T_KILL_W;     *h = T_KILL_H;     break;
        default: *bits = NULL; *w = *h = 0;
    }
}
// 英文任务名
static const char* en_task_name(TaskType t) {
    switch(t) {
        case TASK_FIND_EXIT:  return "Find Exit";
        case TASK_GET_KEY:    return "Get Key";
        case TASK_OPEN_DOOR:  return "Open Door";
        case TASK_KILL_ENEMY: return "Kill Enemy";
        case TASK_SURVIVE:    return "Survive";
        default: return "";
    }
}

static void draw_inventory(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // 顶部 HUD 状态条 (关卡/层数 + 血量)
    bool is_campaign = (s_resume_mode == MODE_CAMPAIGN);
    {
        int hx = 2;
        char buf[8];
        if(is_campaign) {
            snprintf(buf, sizeof(buf), "%d", g.level);
            canvas_draw_str(c, hx, 8, buf);
            if(g.lang == LANG_ZH)
                canvas_draw_xbm(c, hx + 7, 1, HUD_LV_W, HUD_LV_H, hud_lv_bits);
            else
                canvas_draw_str(c, hx + 7, 8, EN_HUD_LV);
            hx += 22;
        } else {
            snprintf(buf, sizeof(buf), "%d", g.endless_floor);
            canvas_draw_str(c, hx, 8, buf);
            if(g.lang == LANG_ZH)
                canvas_draw_xbm(c, hx + 7, 1, HUD_FLOOR_W, HUD_FLOOR_H, hud_floor_bits);
            else
                canvas_draw_str(c, hx + 7, 8, EN_HUD_FLOOR);
            hx += 22;
        }
        // 血量
        snprintf(buf, sizeof(buf), "%d", g.player.health);
        canvas_draw_str(c, hx, 8, buf);
        if(g.lang == LANG_ZH)
            canvas_draw_xbm(c, hx + 7, 1, HUD_HP_W, HUD_HP_H, hud_hp_bits);
        else
            canvas_draw_str(c, hx + 7, 8, EN_HUD_HP);
        hx += 22;
        // 钥匙 / 火把 (物品栏已列, 这里不重复)
    }
    // 分隔线
    canvas_draw_line(c, 0, 11, 127, 11);

    bool has_quest = g.quest.active;

    // ===== 第 2 页: 任务面板 (仅有任务时可切到) =====
    if(has_quest && s_inv_page == 1) {
        // 标题 "任务" 位图
        if(g.lang == LANG_ZH) {
            canvas_draw_xbm(c, 2, 14, QUEST_HDR_W, QUEST_HDR_H, quest_hdr_bits);
        } else {
            canvas_draw_str(c, 2, 22, "QUEST");
        }
        // 页码
        canvas_draw_str_aligned(c, 126, 16, AlignRight, AlignTop, "2/2");
        canvas_draw_line(c, 0, 26, 127, 26);

        // 任务行
        int y = 30;
        for(int i = 0; i < g.quest.sub_count && y < 56; i++) {
            SubTask* s = &g.quest.subs[i];
            // 复选框 (8x8): 完成则实心, 否则空心框
            if(s->done) canvas_draw_box(c, 3, y + 1, 7, 7);
            else        canvas_draw_frame(c, 3, y + 1, 7, 7);
            // 任务名
            if(g.lang == LANG_ZH) {
                int lw, lh; const uint8_t* lb;
                zh_task_label(s->type, &lb, &lw, &lh);
                if(lb) canvas_draw_xbm(c, 14, y, lw, lh, lb);
            } else {
                canvas_draw_str(c, 14, y + 8, en_task_name(s->type));
            }
            // 进度 (右对齐)
            char prog[24];
            if(s->target > 1)
                snprintf(prog, sizeof(prog), "%d/%d", s->progress, s->target);
            else
                snprintf(prog, sizeof(prog), "%d/%d", s->done ? 1 : 0, 1);
            canvas_draw_str_aligned(c, 124, y + 8, AlignRight, AlignBottom, prog);
            y += 13;
        }
        // 完成状态
        if(g.quest.all_done) {
            if(g.lang == LANG_ZH)
                canvas_draw_str(c, 2, 63, "任务完成! 血已满");
            else
                canvas_draw_str(c, 2, 63, "Quest Done! HP Full");
        } else {
            if(g.lang == LANG_ZH)
                canvas_draw_str(c, 2, 63, "<-物品  Back返");
            else
                canvas_draw_str(c, 2, 63, "<-Items  Back");
        }
        return;
    }

    // ===== 第 1 页: 物品 =====
    // 页码指示 (有任务时显示 1/2)
    if(has_quest) canvas_draw_str_aligned(c, 126, 16, AlignRight, AlignTop, "1/2");

    if(g.lang == LANG_ZH) {
        // 物品行
        for(int i = 0; i < ITEM_COUNT; i++) {
            int y = 14 + i * 10;
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
        if(has_quest)
            canvas_draw_str(c, 2, 63, "上下选 OK用  ->任务 Back返");
        else
            canvas_draw_xbm(c, 2, 62 - INV_HINT_H + 1, INV_HINT_W, INV_HINT_H, inv_hint_bits);
        return;
    }

    // 英文 (原逻辑)
    for(int i = 0; i < ITEM_COUNT; i++) {
        int y = 14 + i * 9;
        if(i == g.inv_sel) {
            canvas_draw_box(c, 0, y - 1, 128, 10);
            canvas_set_color(c, ColorWhite);
        }
        char line[24];
        snprintf(line, sizeof(line), "%s  x%d", en_item_name(i), item_count(i));
        canvas_draw_str(c, 4, y + 7, line);
        canvas_set_color(c, ColorBlack);
    }
    if(has_quest)
        canvas_draw_str(c, 2, 62, "OK use  ->Quest  Back");
    else
        canvas_draw_str(c, 2, 62, EN_INV_HINT);
}

// 层级选择: 可见行数与行高 (滚动列表)
#define LS_VISIBLE 7
#define LS_ROW_H   7
#define LS_Y_START 15

static void draw_level_select(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);  // 显式使用小字体

    // 滚动偏移: 确保 ls_sel 在可见窗口内
    if(g.ls_offset < 1) g.ls_offset = 1;
    if(g.ls_sel < g.ls_offset) g.ls_offset = g.ls_sel;
    if(g.ls_sel > g.ls_offset + LS_VISIBLE - 1) g.ls_offset = g.ls_sel - LS_VISIBLE + 1;
    int max_off = g.ls_max - LS_VISIBLE + 1;
    if(max_off < 1) max_off = 1;
    if(g.ls_offset > max_off) g.ls_offset = max_off;

    if(g.lang == LANG_ZH) {
        // 标题
        const uint8_t* tb; int tw, th;
        if(g.ls_for_campaign) { tb = ls_title_s_bits; tw = LS_TITLE_S_W; th = LS_TITLE_S_H; }
        else                  { tb = ls_title_e_bits; tw = LS_TITLE_E_W; th = LS_TITLE_E_H; }
        canvas_draw_xbm(c, (128 - tw) / 2, 1, tw, th, tb);
        // 关卡列表 (滚动)
        for(int i = 0; i < LS_VISIBLE; i++) {
            int lvl = g.ls_offset + i;
            if(lvl > g.ls_max) break;
            int y = LS_Y_START + i * LS_ROW_H;
            bool locked = g.ls_for_campaign && (lvl > g.campaign_cleared + 1);
            bool cleared = g.ls_for_campaign && (lvl <= g.campaign_cleared);
            if(lvl == g.ls_sel && !locked) {
                canvas_draw_box(c, 0, y - 1, 128, LS_ROW_H + 1);
                canvas_set_color(c, ColorWhite);
            }
            // 关卡号 ASCII (前缀用英文 L 避免中文"层"无法用 canvas_draw_str)
            char lv[8]; snprintf(lv, sizeof(lv), "L%d", lvl);
            canvas_draw_str(c, 4, y + 6, lv);
            // 状态标签 (位图)
            int tag_w, tag_h; const uint8_t* tag_b = NULL;
            if(locked)         { tag_b = ls_locked_bits;  tag_w = LS_LOCKED_W;  tag_h = LS_LOCKED_H; }
            else if(cleared)   { tag_b = ls_cleared_bits; tag_w = LS_CLEARED_W; tag_h = LS_CLEARED_H; }
            if(tag_b) canvas_draw_xbm(c, 113, y - 1, tag_w, tag_h, tag_b);
            canvas_set_color(c, ColorBlack);
        }
        // 滚动指示器
        if(g.ls_offset > 1)             canvas_draw_str(c, 118, LS_Y_START + 5, "^");
        if(g.ls_offset + LS_VISIBLE - 1 < g.ls_max)
            canvas_draw_str(c, 118, LS_Y_START + (LS_VISIBLE - 1) * LS_ROW_H + 6, "v");
        canvas_draw_xbm(c, 2, 62 - LS_HINT_H + 1, LS_HINT_W, LS_HINT_H, ls_hint_bits);
        return;
    }

    // 英文 (原逻辑)
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop,
        g.ls_for_campaign ? EN_LS_TITLE_STORY : EN_LS_TITLE_ENDLESS);
    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < LS_VISIBLE; i++) {
        int lvl = g.ls_offset + i;
        if(lvl > g.ls_max) break;
        int y = LS_Y_START + i * LS_ROW_H;
        bool locked = g.ls_for_campaign && (lvl > g.campaign_cleared + 1);
        bool cleared = g.ls_for_campaign && (lvl <= g.campaign_cleared);
        if(lvl == g.ls_sel && !locked) {
            canvas_draw_box(c, 0, y - 1, 128, LS_ROW_H + 1);
            canvas_set_color(c, ColorWhite);
        }
        char line[24];
        const char* tag = locked ? "  LOCKED" : (cleared ? "  ok" : "");
        snprintf(line, sizeof(line), "Lv %d%s", lvl, tag);
        canvas_draw_str(c, 4, y + 6, line);
        canvas_set_color(c, ColorBlack);
    }
    if(g.ls_offset > 1)             canvas_draw_str(c, 120, LS_Y_START + 5, "^");
    if(g.ls_offset + LS_VISIBLE - 1 < g.ls_max)
        canvas_draw_str(c, 120, LS_Y_START + (LS_VISIBLE - 1) * LS_ROW_H + 6, "v");
    canvas_draw_str(c, 2, 62, EN_LS_HINT);
}

// ---- 小地图面板: 全迷宫 + 玩家位置 + 出口大箭头 + 紧凑 HUD ----
static void draw_map_panel(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // ========= 顶部紧凑 HUD (极小文字, FontSecondary 已经是最小字号) =========
    // 行 1-2: 关卡/层数/血量/钥匙/火把/药水/护符
    int hy = 1;
    char b[8];
    int hx = 1;
    bool is_camp = (s_resume_mode == MODE_CAMPAIGN);

    // 关卡/层数
    if(is_camp) {
        snprintf(b, sizeof(b), "%d", g.level);
        canvas_draw_str(c, hx, hy + 7, b);
        if(g.lang == LANG_ZH) canvas_draw_xbm(c, hx+6, hy, HUD_LV_W, HUD_LV_H, hud_lv_bits);
        else                   canvas_draw_str(c, hx+6, hy+7, EN_HUD_LV);
        hx += 20;
    } else {
        snprintf(b, sizeof(b), "%d", g.endless_floor);
        canvas_draw_str(c, hx, hy + 7, b);
        if(g.lang == LANG_ZH) canvas_draw_xbm(c, hx+6, hy, HUD_FLOOR_W, HUD_FLOOR_H, hud_floor_bits);
        else                   canvas_draw_str(c, hx+6, hy+7, EN_HUD_FLOOR);
        hx += 20;
    }
    // 血量
    snprintf(b, sizeof(b), "%d", g.player.health);
    canvas_draw_str(c, hx, hy+7, b);
    if(g.lang == LANG_ZH) canvas_draw_xbm(c, hx+6, hy, HUD_HP_W, HUD_HP_H, hud_hp_bits);
    else                   canvas_draw_str(c, hx+6, hy+7, EN_HUD_HP);
    hx += 18;
    // 钥匙
    snprintf(b, sizeof(b), "%d", g.player.keys);
    canvas_draw_str(c, hx, hy+7, b);
    if(g.lang == LANG_ZH) canvas_draw_xbm(c, hx+5, hy, HUD_KEY_W, HUD_KEY_H, hud_key_bits);
    else                   canvas_draw_str(c, hx+5, hy+7, EN_HUD_KEY);
    hx += 16;
    // 火把
    snprintf(b, sizeof(b), "%d", g.player.torches);
    canvas_draw_str(c, hx, hy+7, b);
    if(g.lang == LANG_ZH) canvas_draw_xbm(c, hx+5, hy, HUD_TORCH_W, HUD_TORCH_H, hud_torch_bits);
    else                   canvas_draw_str(c, hx+5, hy+7, EN_HUD_TORCH);
    hx += 16;
    // 药水 P / 护符 A (直接用 ASCII)
    snprintf(b, sizeof(b), "%dP", g.player.potions);
    canvas_draw_str(c, hx, hy+7, b);
    hx += 13;
    snprintf(b, sizeof(b), "%dA", g.player.amulets);
    canvas_draw_str(c, hx, hy+7, b);

    // 分隔线
    canvas_draw_line(c, 0, 10, 127, 10);

    // ========= 小地图 =========
    // 地图绘制区: x=[2..125]  y=[12..55], 宽 124, 高 44
    const int MX = 2, MY = 12, MW = 124, MH = 44;
    int m_w = g.map_w, m_h = g.map_h;
    if(m_w < 1) m_w = 1;
    if(m_h < 1) m_h = 1;
    // 单元像素大小 (取最小值保证整张地图能装下)
    int cs = MW / m_w;
    if(MH / m_h < cs) cs = MH / m_h;
    if(cs < 1) cs = 1;
    // 地图实际绘制大小,居中
    int dw = m_w * cs;
    int dh = m_h * cs;
    int mx0 = MX + (MW - dw) / 2;
    int my0 = MY + (MH - dh) / 2;

    // 边框
    canvas_draw_frame(c, mx0 - 1, my0 - 1, dw + 2, dh + 2);

    // 绘制墙壁/门/出口/物品
    for(int y = 0; y < m_h; y++) {
        for(int x = 0; x < m_w; x++) {
            uint8_t cell = g.map[(uint16_t)y * MAP_MAX + x];
            bool wall = (cell == WALL_BRICK || cell == WALL_STONE ||
                         cell == WALL_METAL || cell == WALL_VINE || cell == CELL_DOOR);
            int dx = mx0 + x * cs;
            int dy = my0 + y * cs;
            if(wall) {
                if(cs >= 2)
                    canvas_draw_box(c, dx, dy, cs, cs);
                else
                    canvas_draw_dot(c, dx, dy);
            } else if(cell == CELL_EXIT) {
                // 出口: 画一个实心方框标记 (闪烁交替)
                bool flick = (g.tick & 4) ? true : false;
                if(cs >= 2) {
                    if(flick) canvas_draw_box(c, dx, dy, cs, cs);
                    else      canvas_draw_frame(c, dx, dy, cs, cs);
                } else {
                    canvas_draw_dot(c, dx, dy);
                }
            } else if(cell == CELL_KEY || cell == CELL_TORCH ||
                      cell == CELL_POTION || cell == CELL_AMULET) {
                // 物品: 单点
                if(cs >= 2)
                    canvas_draw_dot(c, dx + cs/2, dy + cs/2);
                else
                    canvas_draw_dot(c, dx, dy);
            }
        }
    }

    // ===== 出口大箭头 (指向出口格, 巨大黑色箭头) =====
    if(g.exit_found && m_w > 0 && m_h > 0) {
        int ex = mx0 + g.exit_x * cs + cs/2;
        int ey = my0 + g.exit_y * cs + cs/2;
        // 箭头大小取决于可用空间
        int ar = 8; // 箭头臂长
        // 选离出口距离最短的边(但保证箭头长度够):
        // pick: 0=从左指→右, 1=从右指→左, 2=从上指↓下, 3=从下指↑上
        int pick = 0; // 0=up,1=dn,2=lf,3=rt
        int d_up = abs(ex - (mx0 - 1));
        int d_dn = abs((mx0 + dw) - ex);
        int d_lf = abs(ey - (my0 - 1));
        int d_rt = abs((my0 + dh) - ey);
        int min_d = d_up;
        if(d_dn < min_d) { min_d = d_dn; pick = 1; }
        if(d_lf < min_d) { min_d = d_lf; pick = 2; }
        if(d_rt < min_d) { min_d = d_rt; pick = 3; }
        // 限制箭头长度至少 ar, 如果空间不够则减少
        int len = ar;
        if(min_d - 2 < len) len = min_d - 2;
        if(len < 4) len = 4;
        int sx, sy;
        if(pick == 0)      { sx = ex - len; sy = ey; }
        else if(pick == 1) { sx = ex + len; sy = ey; }
        else if(pick == 2) { sx = ex; sy = ey - len; }
        else               { sx = ex; sy = ey + len; }
        // 箭头尾至头线段
        canvas_draw_line(c, sx, sy, ex, ey);
        // 箭头头部: 两条短线 (巨大黑色)
        int ah = 5; // 箭头头部长度
        int aw = 4; // 箭头头部展开宽度
        if(pick == 0) {
            // 从左指向右, 箭尾(左)<-起点,箭头头(右)->终点
            canvas_draw_line(c, ex, ey, ex - ah, ey - aw);
            canvas_draw_line(c, ex, ey, ex - ah, ey + aw);
            canvas_draw_line(c, ex, ey, ex - ah + 1, ey - aw + 1); // 加粗
            canvas_draw_line(c, ex, ey, ex - ah + 1, ey + aw - 1);
        } else if(pick == 1) {
            canvas_draw_line(c, ex, ey, ex - ah, ey - aw);
            canvas_draw_line(c, ex, ey, ex - ah, ey + aw);
            canvas_draw_line(c, ex, ey, ex - ah + 1, ey - aw + 1);
            canvas_draw_line(c, ex, ey, ex - ah + 1, ey + aw - 1);
        } else if(pick == 2) {
            canvas_draw_line(c, ex, ey, ex - aw, ey - ah);
            canvas_draw_line(c, ex, ey, ex + aw, ey - ah);
            canvas_draw_line(c, ex, ey, ex - aw + 1, ey - ah + 1);
            canvas_draw_line(c, ex, ey, ex + aw - 1, ey - ah + 1);
        } else {
            canvas_draw_line(c, ex, ey, ex - aw, ey - ah);
            canvas_draw_line(c, ex, ey, ex + aw, ey - ah);
            canvas_draw_line(c, ex, ey, ex - aw + 1, ey - ah + 1);
            canvas_draw_line(c, ex, ey, ex + aw - 1, ey - ah + 1);
        }
        // 在出口格位置额外画一个实心方块(加黑加粗出口)
        int bx = mx0 + g.exit_x * cs;
        int by = my0 + g.exit_y * cs;
        if(cs >= 2) {
            canvas_draw_box(c, bx, by, cs, cs);
        }
    }

    // ===== 玩家标记 (黑色实心圆或十字) =====
    {
        int px = mx0 + (int)g.player.x * cs + cs/2;
        int py = my0 + (int)g.player.y * cs + cs/2;
        // 玩家十字标记 (比1像素大,明显)
        int ps = (cs >= 3) ? cs - 1 : 2;
        canvas_draw_line(c, px - ps, py, px + ps, py);
        canvas_draw_line(c, px, py - ps, px, py + ps);
        // 玩家朝向短线
        int dx = (int)(g.player.dir_x * (ps + 2));
        int dy = (int)(g.player.dir_y * (ps + 2));
        if(dx || dy)
            canvas_draw_line(c, px, py, px + dx, py + dy);
    }

    // ===== 底部提示 =====
    canvas_draw_line(c, 0, 56, 127, 56);
    if(g.lang == LANG_ZH) {
        canvas_draw_str(c, 2, 63, "OK/Back返回游戏");
    } else {
        canvas_draw_str(c, 2, 63, "OK/Back Resume");
    }
}

// ---- 开场动画: 4阶段流畅嗨皮动画 + BGM ----
//   stage 0 (tick 0-7):  Logo 从顶部弹跳落入, 配合 BGM 上行琶音
//   stage 1 (tick 8-15): 粒子/星星从 Logo 中心迸发扩散
//   stage 2 (tick 16-23): 副标题 "k20120509 presents" 从底部滑入
//   stage 3 (tick 24-37): "按任意键开始" 闪烁, BGM 渐收
//   tick 38+: 自动进菜单
static void draw_opening(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    int t = g.opening_tick;
    int s = g.opening_stage;
    int cx = (128 - TITLE_W) / 2;  // logo 居中 X
    int cy_final = 18;              // logo 最终 Y

    // ===== Stage 0: Logo 从顶部弹跳落入 (ease-in + bounce) =====
    if(s == 0) {
        // 0..7 tick: 从 y=-20 弹跳到 y=cy_final
        // 前5tick自由落体加速, 后2tick弹跳回弹
        int y;
        if(t <= 5) {
            // 自由落体: y = -20 + (t^2)*1.5, t=0:-20, t=5:-20+37=17
            y = -20 + t * t * 2;
        } else {
            // 弹跳: t=6 略过冲到 cy_final-4, t=7 回到 cy_final
            int bt = t - 5;  // 1..2
            y = cy_final - 4 + bt * 2;  // 16, 18
            if(bt >= 2) y = cy_final;
        }
        if(y > cy_final) y = cy_final;
        // logo 渐现: 前2tick不画(太高), 后5tick画
        if(t >= 2) {
            canvas_draw_xbm(c, cx, y, TITLE_W, TITLE_H, title_bits);
            // 着地瞬间(t=5)画一个"冲击波"横线
            if(t == 5 || t == 6) {
                int wave_y = cy_final + TITLE_H + 2;
                int wave_w = (t == 5) ? 40 : 60;
                canvas_draw_line(c, 64 - wave_w/2, wave_y, 64 + wave_w/2, wave_y);
            }
        }
    }
    // ===== Stage 1: 粒子/星星从 Logo 中心迸发 =====
    else if(s == 1) {
        // Logo 稳定居中
        canvas_draw_xbm(c, cx, cy_final, TITLE_W, TITLE_H, title_bits);
        // 粒子: 8个方向放射, 距离随 (t-8) 增长
        int pt = t - 8;  // 0..7
        int px = 64, py = cy_final + TITLE_H / 2;  // 中心
        // 8个方向的单位向量 (x,y)
        static const int dx[8] = { 1, 1, 0,-1,-1,-1, 0, 1};
        static const int dy[8] = { 0, 1, 1, 1, 0,-1,-1,-1};
        for(int i = 0; i < 8; i++) {
            int dist = pt * 3 + 2;  // 距离随时间增大
            int x = px + dx[i] * dist;
            int y = py + dy[i] * dist;
            // 限制在屏幕内
            if(x >= 0 && x < 128 && y >= 0 && y < 64) {
                // 画一个小十字星
                canvas_draw_dot(c, x, y);
                if(pt < 5) {
                    canvas_draw_dot(c, x + 1, y);
                    canvas_draw_dot(c, x, y + 1);
                }
            }
        }
        // Logo 周围闪烁星星 (随机位置, 用 tick 伪随机)
        if((g.tick & 3) == 0) {
            for(int i = 0; i < 4; i++) {
                int sx = (g.tick * 37 + i * 23) % 120 + 4;
                int sy = (g.tick * 53 + i * 17) % 40 + 4;
                canvas_draw_dot(c, sx, sy);
            }
        }
    }
    // ===== Stage 2: 副标题从底部滑入 =====
    else if(s == 2) {
        // Logo 居中
        canvas_draw_xbm(c, cx, cy_final, TITLE_W, TITLE_H, title_bits);
        // 残留粒子 (淡出)
        int pt = t - 16;  // 0..7
        if(pt < 4) {
            for(int i = 0; i < 8; i++) {
                static const int dx[8] = { 1, 1, 0,-1,-1,-1, 0, 1};
                static const int dy[8] = { 0, 1, 1, 1, 0,-1,-1,-1};
                int dist = (pt + 8) * 3 + 2;
                int x = 64 + dx[i] * dist;
                int y = cy_final + TITLE_H / 2 + dy[i] * dist;
                if(x >= 0 && x < 128 && y >= 0 && y < 64)
                    canvas_draw_dot(c, x, y);
            }
        }
        // 副标题从底部滑入: y = 64 -> 50
        int sub_y = 64 - pt * 2;
        if(sub_y < 50) sub_y = 50;
        if(g.lang == LANG_ZH)
            canvas_draw_str_aligned(c, 64, sub_y, AlignCenter, AlignTop, "k20120509");
        else
            canvas_draw_xbm(c, (128 - OPEN_BY_W)/2, sub_y, OPEN_BY_W, OPEN_BY_H, open_by_bits);
    }
    // ===== Stage 3: "按任意键开始" 闪烁 =====
    else {
        // Logo + 副标题 静止
        canvas_draw_xbm(c, cx, cy_final, TITLE_W, TITLE_H, title_bits);
        if(g.lang == LANG_ZH)
            canvas_draw_str_aligned(c, 64, 50, AlignCenter, AlignTop, "k20120509");
        else
            canvas_draw_xbm(c, (128 - OPEN_BY_W)/2, 50, OPEN_BY_W, OPEN_BY_H, open_by_bits);
        // "按任意键开始" 闪烁 (1.5Hz)
        if((g.tick & 4) == 0)
            canvas_draw_str_aligned(c, 64, 62, AlignCenter, AlignTop,
                g.lang == LANG_ZH ? "按任意键开始" : "Press any key");
    }
}

// ---- 设置页
static void draw_settings(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // 标题
    if(g.lang == LANG_ZH) {
        canvas_draw_xbm(c, (128 - SETTINGS_HDR_W) / 2, 1,
                         SETTINGS_HDR_W, SETTINGS_HDR_H, settings_hdr_bits);
    } else {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 2, AlignCenter, AlignTop, "SETTINGS");
        canvas_set_font(c, FontSecondary);
    }
    canvas_draw_line(c, 0, 16, 127, 16);

    // 两项: 音效开关
    int y = 22;
    const uint8_t* labels_zh[2] = { set_sfx_bits, set_opening_bits };
    const char*    labels_en[2] = { "SFX", "Opening" };
    const int lws[2] = { SET_SFX_W, SET_OPENING_W };
    const int lhs[2] = { SET_SFX_H, SET_OPENING_H };
    bool vals[2]     = { g.sfx_enabled, g.opening_enabled };

    for(int i = 0; i < 2; i++) {
        if(i == s_set_sel) {
            canvas_draw_box(c, 0, y - 1, 128, 12);
            canvas_set_color(c, ColorWhite);
        }
        canvas_draw_str(c, 2, y + 8, ">");
        if(g.lang == LANG_ZH)
            canvas_draw_xbm(c, 12, y, lws[i], lhs[i], labels_zh[i]);
        else
            canvas_draw_str(c, 12, y + 8, labels_en[i]);
        // 值
        if(vals[i]) {
            if(g.lang == LANG_ZH)
                canvas_draw_xbm(c, 110, y, SET_ON_W, SET_ON_H, set_on_bits);
            else
                canvas_draw_str(c, 110, y + 8, "ON");
        } else {
            if(g.lang == LANG_ZH)
                canvas_draw_xbm(c, 110, y, SET_OFF_W, SET_OFF_H, set_off_bits);
            else
                canvas_draw_str(c, 110, y + 8, "OFF");
        }
        canvas_set_color(c, ColorBlack);
        y += 14;
    }
    canvas_draw_line(c, 0, 56, 127, 56);
    // 提示
    if(g.lang == LANG_ZH) {
        canvas_draw_xbm(c, 2, 58, SET_SELECT_W, SET_SELECT_H, set_select_bits);
        canvas_draw_xbm(c, 128 - SET_BACK_W - 2, 58, SET_BACK_W, SET_BACK_H, set_back_bits);
    } else {
        canvas_draw_str(c, 2, 63, "Up/Dn  OK Toggle");
        canvas_draw_str_aligned(c, 126, 63, AlignRight, AlignBottom, "Back Exit");
    }
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

        // 菜单项: 1.闯关模式  2.无尽挑战  3.游客漫游  4.设置
        const uint8_t* zh_items[M_COUNT] = { m1_bits, m2_bits, m3_bits, m4_bits };
        const char*    en_items[M_COUNT] = { EN_M1, EN_M2, EN_M3, "4. Settings" };
        const int ws[M_COUNT] = { M1_W, M2_W, M3_W, M4_W };
        const int hs[M_COUNT] = { M1_H, M2_H, M3_H, M4_H };
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

    if(g.mode == MODE_OPENING)       { draw_opening(canvas); return; }
    if(g.mode == MODE_SETTINGS)      { draw_settings(canvas); return; }
    if(g.mode == MODE_STORY)         { draw_story(canvas); return; }
    if(g.mode == MODE_INVENTORY)     { draw_inventory(canvas); return; }
    if(g.mode == MODE_LEVEL_SELECT)  { draw_level_select(canvas); return; }
    if(g.mode == MODE_MAP_PANEL)     { draw_map_panel(canvas); return; }

    // 游戏画面: 先把渲染好的 framebuffer 拷到 canvas
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, SCREEN_W, SCREEN_H, g.fb);

    // HUD 信息 (默认隐藏, 长按 OK 切换显示): 关卡/层数/钥匙/火把/血
    if(g.show_hud) {
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

    // 提示信息 (HUD 显示时一并展示)
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
    } // end if(g.show_hud)

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
    // 初始化滚动偏移: 让选中项尽量居中
    g.ls_offset = g.ls_sel > 3 ? g.ls_sel - 3 : 1;
    if(g.ls_offset > g.ls_max - LS_VISIBLE + 1 && g.ls_max >= LS_VISIBLE)
        g.ls_offset = g.ls_max - LS_VISIBLE + 1;
    if(g.ls_offset < 1) g.ls_offset = 1;
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
        // 有任务时: 左右切换 物品/任务 页
        if(g.quest.active) {
            if(key == InputKeyRight)      { s_inv_page = 1; }
            else if(key == InputKeyLeft)  { s_inv_page = 0; }
        }
        // 物品页才可操作物品
        if(s_inv_page == 0) {
            if(key == InputKeyUp)        g.inv_sel = (g.inv_sel + ITEM_COUNT - 1) % ITEM_COUNT;
            else if(key == InputKeyDown) g.inv_sel = (g.inv_sel + 1) % ITEM_COUNT;
            else if(key == InputKeyOk)   item_use(g.inv_sel);
        }
        if(key == InputKeyBack) { g.mode = s_resume_mode; s_inv_page = 0; }
    } else if(g.mode == MODE_MAP_PANEL) {
        // 小地图面板: 短按 OK / Back 关闭
        if(key == InputKeyOk || key == InputKeyBack) {
            g.mode = s_resume_mode;
        }
        // 长按 OK 则在外部处理中进物品栏
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
    // 默认设置 (storage_load 会覆盖)
    settings_defaults();
    storage_load();
    sfx_init();
    // 开场动画
    g.opening_stage = 0;
    g.opening_tick = 0;
    g.mode = g.opening_enabled ? MODE_OPENING : MODE_MENU;

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(AppEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_callback, NULL);
    view_port_input_callback_set(vp, input_callback, q);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    // 播放开场 BGM (嗨皮旋律, 独立通道)
    if(g.mode == MODE_OPENING) sfx_bgm_play();

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

            if(g.mode == MODE_OPENING) {
                // 开场动画: 任意键跳过,或等其自然结束
                if(type == InputTypeShort || type == InputTypeLong) {
                    sfx_bgm_stop();
                    g.mode = MODE_MENU;
                    sfx_play(SFX_MENU_OK);
                }
                did_input = true;
            } else if(g.mode == MODE_SETTINGS) {
                if(type == InputTypeShort) {
                    if(key == InputKeyUp)        { s_set_sel = (s_set_sel + 1) % 2; sfx_play(SFX_MENU_MOVE); }
                    else if(key == InputKeyDown) { s_set_sel = (s_set_sel + 1) % 2; sfx_play(SFX_MENU_MOVE); }
                    else if(key == InputKeyLeft || key == InputKeyRight || key == InputKeyOk) {
                        if(s_set_sel == 0) { g.sfx_enabled = !g.sfx_enabled; sfx_play(g.sfx_enabled ? SFX_MENU_OK : SFX_NEED_KEY); }
                        else               { g.opening_enabled = !g.opening_enabled; sfx_play(SFX_MENU_OK); }
                        storage_save();
                    } else if(key == InputKeyBack) {
                        g.mode = MODE_MENU;
                        sfx_play(SFX_MENU_OK);
                    }
                }
                did_input = true;
            } else if(g.mode == MODE_MENU) {
                if(type == InputTypeShort) {
                    if(key == InputKeyUp) { s_sel = (s_sel + M_COUNT - 1) % M_COUNT; sfx_play(SFX_MENU_MOVE); }
                    else if(key == InputKeyDown) { s_sel = (s_sel + 1) % M_COUNT; sfx_play(SFX_MENU_MOVE); }
                    else if(key == InputKeyLeft || key == InputKeyRight)
                        g.lang = (g.lang == LANG_ZH) ? LANG_EN : LANG_ZH;
                    else if(key == InputKeyOk) {
                        storage_load();
                        sfx_play(SFX_MENU_OK);
                        if(s_sel == M_CAMPAIGN) enter_level_select(true);
                        else if(s_sel == M_ENDLESS) enter_level_select(false);
                        else if(s_sel == M_VISITOR) game_init_endless(g.endless_floor, true);
                        else if(s_sel == M_SETTINGS) { g.mode = MODE_SETTINGS; s_set_sel = 0; }
                    } else if(key == InputKeyBack) { running = false; sfx_stop_all(); }
                }
                did_input = true;
            } else if(g.mode == MODE_LEVEL_CLEAR || g.mode == MODE_GAME_OVER || g.mode == MODE_PAUSED) {
                if(type == InputTypeShort) {
                    if(g.mode == MODE_LEVEL_CLEAR && key == InputKeyOk) sfx_play(SFX_QUEST_DONE);
                    else if(g.mode == MODE_GAME_OVER && key == InputKeyOk) sfx_play(SFX_MENU_OK);
                    else if(g.mode == MODE_PAUSED) sfx_play(SFX_MENU_OK);
                    handle_overlay_input(key);
                }
                did_input = true;
            } else if(g.mode == MODE_STORY || g.mode == MODE_INVENTORY ||
                      g.mode == MODE_LEVEL_SELECT || g.mode == MODE_MAP_PANEL) {
                // 物品栏中长按 OK → 进入小地图面板 (查看全图+出口箭头)
                if(g.mode == MODE_INVENTORY && key == InputKeyOk && type == InputTypeLong) {
                    g.mode = MODE_MAP_PANEL;
                    sfx_play(SFX_MENU_OK);
                } else if(type == InputTypeShort) {
                    // 翻页/选层时播放音效
                    SfxType old = SFX_NONE;
                    GameMode mm = g.mode;
                    if(mm == MODE_STORY && (key == InputKeyOk || key == InputKeyLeft || key == InputKeyRight)) old = SFX_STORY_TURN;
                    if(mm == MODE_LEVEL_SELECT && (key == InputKeyUp || key == InputKeyDown)) old = SFX_MENU_MOVE;
                    if(mm == MODE_LEVEL_SELECT && key == InputKeyOk) old = SFX_MENU_OK;
                    if(mm == MODE_INVENTORY && (key == InputKeyUp || key == InputKeyDown || key == InputKeyLeft || key == InputKeyRight)) old = SFX_MENU_MOVE;
                    if(mm == MODE_INVENTORY && key == InputKeyOk) old = SFX_PICK_ITEM;
                    if(mm == MODE_INVENTORY && key == InputKeyBack) old = SFX_MENU_OK;
                    if(mm == MODE_MAP_PANEL) old = SFX_MENU_OK;
                    if(old != SFX_NONE) sfx_play(old);
                    handle_new_modes_input(key, type);
                }
                did_input = true;
            } else {
                // 游戏中
                if(key == InputKeyBack) {
                    if(type == InputTypeLong) { running = false; sfx_stop_all(); }
                    else if(type == InputTypeShort) {
                        s_resume_mode = g.mode;
                        g.mode = MODE_PAUSED;
                        sfx_play(SFX_MENU_OK);
                    }
                } else if(key == InputKeyOk && type == InputTypeLong) {
                    // 长按 OK: 暂停 + 打开物品栏 (含任务面板)
                    s_resume_mode = g.mode;
                    g.mode = MODE_INVENTORY;
                    g.inv_sel = 0;
                    s_inv_page = 0;
                    sfx_play(SFX_MENU_OK);
                } else {
                    game_handle_input(key, type);
                }
                did_input = true;
            }
        }

        // 定时更新世界(无论是否有事件): 敌人移动、闪烁tick + 音效tick + 开场tick
        uint32_t now = furi_get_tick(); // 注: furi HAL tick = ms
        bool timer_fired = ((now - last_update_tick) >= UPDATE_MS);
        if(timer_fired || did_input) {
            if(timer_fired) {
                g.tick++;
                sfx_tick_update();
                if(g.mode == MODE_OPENING) {
                    g.opening_tick++;
                    // 4阶段: 0(0-7) 1(8-15) 2(16-23) 3(24+)
                    if(g.opening_stage == 0 && g.opening_tick >= 8) {
                        g.opening_stage = 1;
                        g.opening_tick = 0;
                    } else if(g.opening_stage == 1 && g.opening_tick >= 8) {
                        g.opening_stage = 2;
                        g.opening_tick = 0;
                    } else if(g.opening_stage == 2 && g.opening_tick >= 8) {
                        g.opening_stage = 3;
                        g.opening_tick = 0;
                    } else if(g.opening_stage == 3 && g.opening_tick >= 14) {
                        // 开场结束 -> 菜单 (BGM 应已播完)
                        g.mode = MODE_MENU;
                        sfx_bgm_stop();
                        sfx_play(SFX_MENU_OK);
                    }
                }
            }
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
    sfx_deinit();
    furi_record_close(RECORD_GUI);
    return 0;
}

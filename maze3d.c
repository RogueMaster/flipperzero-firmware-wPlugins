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
    M_MC       = 4,   // MC 沙盒模式 (Beta)
    M_SHOP     = 5,   // v6.11: 积分商城 (主菜单入口)
    M_COUNT = 6,
} MenuItem;

#define MENU_VISIBLE 5   // 菜单可见行数 (选中项永远在中间第3行, 即 MENU_SEL_IDX=2)
#define MENU_SEL_IDX  2
#define MENU_ROW_H   10
#define MENU_Y_START 19   // 标题+分隔线后开始 (y=17 分隔线, 到 52 底分隔 = 35px 可用)
#define MENU_Y_END   51

// v6.9: 设置页可见区常量 (与菜单类似布局, 选中项永远居中)
#define SET_VISIBLE  5   // 设置页可见行数
#define SET_SEL_IDX  2
#define SET_ROW_H    8
#define SET_Y_START  19
#define SET_Y_END    52

static int s_sel = 0;
static uint8_t s_menu_off = 0;   // 菜单滚动偏移
static GameMode s_resume_mode = MODE_CAMPAIGN;
// 物品栏分页: 0=物品 1=任务 (仅有任务时才可切到第2页)
static uint8_t s_inv_page = 0;
// 设置页光标 (0=音效 1=开场 2=调试[仅 dev_mode]) 与滚动偏移
static uint8_t s_set_sel = 0;
static uint8_t s_set_off = 0;
// 开发模式解锁: 隐藏按键序列, s_dev_seq 记录已完成的步数
static uint8_t s_dev_seq = 0;

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
        case MSG_QUESTDONE: *bits = msg_questdone_bits; *w = MSG_QUESTDONE_W; *h = MSG_QUESTDONE_H; *bpr = MSG_QUESTDONE_BPR; break;
        // 拾取药水/护符: 复用物品栏中文名位图
        case MSG_POTION:  *bits = inv_potion_bits;  *w = INV_POTION_W;  *h = INV_POTION_H;  *bpr = INV_POTION_BPR;  break;
        case MSG_AMULET:  *bits = inv_amulet_bits;  *w = INV_AMULET_W;  *h = INV_AMULET_H;  *bpr = INV_AMULET_BPR;  break;
        // MC 挖掘/放置/锁定/弹药/成就: 无中文位图, toast 用英文回退
        case MSG_MINE: case MSG_PLACE:
        case MSG_LOCKED: case MSG_NOAMMO: case MSG_AMMO:
        case MSG_ACHIEVE: case MSG_TASKPROG:
            *bits = NULL; *w = *h = *bpr = 0; break;
        default: *bits = NULL; *w = *h = *bpr = 0;
    }
}

// 在 canvas 上直接绘制数字(Flipper 内置ASCII即可)
static void canvas_draw_num(Canvas* c, int x, int y, int n) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", n);
    canvas_draw_str(c, x, y, buf);
}

// v6.1: 画一个小心形 (7x6) — 用实心像素拼, 比字符更清晰
static void canvas_draw_heart(Canvas* c, int x, int y, bool filled) {
    // 心形位图 (7x6), 1=亮
    static const uint8_t heart[6] = {
        0x66,  // .##.##.
        0xFF,  // #######
        0xFF,  // #######
        0x7E,  // .#####.
        0x3C,  // ..###..
        0x18,  // ...#...
    };
    for(int r = 0; r < 6; r++) {
        for(int cc = 0; cc < 7; cc++) {
            bool on = (heart[r] >> (6 - cc)) & 1;
            if(filled ? on : on) {  // 心形只画 on 像素 (filled 参数保留以备空心)
                canvas_draw_dot(c, x + cc, y + r);
            }
        }
    }
}

// v6.1: 画一个小星星 (7x7) — 五角星近似
static void canvas_draw_star(Canvas* c, int x, int y) {
    // 星形位图 (7x7)
    static const uint8_t star[7] = {
        0x10,  // ...#...
        0x10,  // ...#...
        0xFE,  // #######
        0x38,  // ..###..
        0x7C,  // .#####.
        0x28,  // .#.#.#.
        0x44,  // #.....#
    };
    for(int r = 0; r < 7; r++) {
        for(int cc = 0; cc < 7; cc++) {
            if((star[r] >> (6 - cc)) & 1) {
                canvas_draw_dot(c, x + cc, y + r);
            }
        }
    }
}

// v6.1: 画一个骷髅/敌人图标 (8x7) — 表示剩余敌人
static void canvas_draw_skull(Canvas* c, int x, int y) {
    static const uint8_t skull[7] = {
        0x3C,  // ..####..
        0x7E,  // .######.
        0xDB,  // ##.##.##
        0xFF,  // ########
        0xFF,  // ########
        0x18,  // ...##...
        0x3C,  // ..####..
    };
    for(int r = 0; r < 7; r++) {
        for(int cc = 0; cc < 8; cc++) {
            if((skull[r] >> (7 - cc)) & 1) {
                canvas_draw_dot(c, x + cc, y + r);
            }
        }
    }
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
#define LS_SEL_IDX  3         // 选中项永远在可见行的中间 (第 4 行, 0 基准 3)
#define LS_ROW_H   7
#define LS_Y_START 15
#define LS_Y_END   52

static void draw_level_select(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    if(g.lang == LANG_ZH) {
        // 标题
        const uint8_t* tb; int tw, th;
        if(g.ls_for_campaign) { tb = ls_title_s_bits; tw = LS_TITLE_S_W; th = LS_TITLE_S_H; }
        else                  { tb = ls_title_e_bits; tw = LS_TITLE_E_W; th = LS_TITLE_E_H; }
        canvas_draw_xbm(c, (128 - tw) / 2, 1, tw, th, tb);
        // ---- 真正居中: 选中项永远在可见区正中央 ----
        int y_top = LS_Y_START, y_bot = LS_Y_END;
        int row_h = LS_ROW_H;
        int center_y = (y_top + y_bot) / 2;
        int sel_y = center_y - row_h / 2;
        for(int lvl = 1; lvl <= g.ls_max; lvl++) {
            int rel = lvl - (int)g.ls_sel;
            int y = sel_y + rel * row_h;
            if(y + row_h <= y_top || y >= y_bot) continue;
            bool locked = g.ls_for_campaign && (lvl > g.campaign_cleared + 1) && !g.dev_mode;
            bool cleared = g.ls_for_campaign && (lvl <= g.campaign_cleared);
            if(lvl == g.ls_sel && !locked) {
                canvas_draw_box(c, 0, y - 1, 128, row_h + 1);
                canvas_set_color(c, ColorWhite);
            }
            char lv[8]; snprintf(lv, sizeof(lv), "L%d", lvl);
            canvas_draw_str(c, 4, y + 6, lv);
            int tag_w, tag_h; const uint8_t* tag_b = NULL;
            if(locked)         { tag_b = ls_locked_bits;  tag_w = LS_LOCKED_W;  tag_h = LS_LOCKED_H; }
            else if(cleared)   { tag_b = ls_cleared_bits; tag_w = LS_CLEARED_W; tag_h = LS_CLEARED_H; }
            if(tag_b) canvas_draw_xbm(c, 113, y - 1, tag_w, tag_h, tag_b);
            canvas_set_color(c, ColorBlack);
        }
        // 进度条
        if(g.ls_max > 1) {
            int bar_x = 126, bar_y = y_top, bar_h = y_bot - y_top;
            float ratio = (float)(g.ls_sel - 1) / (float)(g.ls_max - 1);
            int thumb_h = bar_h / 4; if(thumb_h < 2) thumb_h = 2;
            int thumb_y = bar_y + (int)(ratio * (float)(bar_h - thumb_h));
            canvas_draw_frame(c, bar_x, bar_y, 2, bar_h);
            canvas_draw_box(c, bar_x, thumb_y, 2, thumb_h);
        }
        canvas_draw_xbm(c, 2, 62 - LS_HINT_H + 1, LS_HINT_W, LS_HINT_H, ls_hint_bits);
        return;
    }

    // 英文
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop,
        g.ls_for_campaign ? EN_LS_TITLE_STORY : EN_LS_TITLE_ENDLESS);
    canvas_set_font(c, FontSecondary);
    {
        int y_top = LS_Y_START, y_bot = LS_Y_END;
        int row_h = LS_ROW_H;
        int center_y = (y_top + y_bot) / 2;
        int sel_y = center_y - row_h / 2;
        for(int lvl = 1; lvl <= g.ls_max; lvl++) {
            int rel = lvl - (int)g.ls_sel;
            int y = sel_y + rel * row_h;
            if(y + row_h <= y_top || y >= y_bot) continue;
            bool locked = g.ls_for_campaign && (lvl > g.campaign_cleared + 1) && !g.dev_mode;
            bool cleared = g.ls_for_campaign && (lvl <= g.campaign_cleared);
            if(lvl == g.ls_sel && !locked) {
                canvas_draw_box(c, 0, y - 1, 128, row_h + 1);
                canvas_set_color(c, ColorWhite);
            }
            char line[24];
            const char* tag = locked ? "  LOCKED" : (cleared ? "  ok" : "");
            snprintf(line, sizeof(line), "Lv %d%s", lvl, tag);
            canvas_draw_str(c, 4, y + 6, line);
            canvas_set_color(c, ColorBlack);
        }
        if(g.ls_max > 1) {
            int bar_x = 126, bar_y = y_top, bar_h = y_bot - y_top;
            float ratio = (float)(g.ls_sel - 1) / (float)(g.ls_max - 1);
            int thumb_h = bar_h / 4; if(thumb_h < 2) thumb_h = 2;
            int thumb_y = bar_y + (int)(ratio * (float)(bar_h - thumb_h));
            canvas_draw_frame(c, bar_x, bar_y, 2, bar_h);
            canvas_draw_box(c, bar_x, thumb_y, 2, thumb_h);
        }
    }
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
    // ===== Stage 3: logo + 副标题静止, 等待开场自然结束进入菜单 =====
    else {
        // Logo + 副标题 静止
        canvas_draw_xbm(c, cx, cy_final, TITLE_W, TITLE_H, title_bits);
        if(g.lang == LANG_ZH)
            canvas_draw_str_aligned(c, 64, 50, AlignCenter, AlignTop, "k20120509");
        else
            canvas_draw_xbm(c, (128 - OPEN_BY_W)/2, 50, OPEN_BY_W, OPEN_BY_H, open_by_bits);
    }
}

// ---- 设置页 v6.9: 20+ 开发者设置 (选中项永远垂直居中)
// 设置项类型
typedef enum { SET_BOOL, SET_VAL8, SET_ACTION } SetType;
// 单个设置条目
typedef struct { SetType type; void* val_ptr; uint8_t min_val; uint8_t max_val;
    const char** labels_zh; const char** labels_en; const char* fmt_num;
    const char* label_zh; const char* label_en; } SetEntry;

// 档位表 (双语)
static const char* LBL_TURN_SENS[]   = { "1.0x","1.25x","1.5x","1.75x","2.0x","2.5x" };
static const char* LBL_SHORT_DEG[]   = { "5.7d","8.6d","11.5d","14.3d","17.2d" };
static const char* LBL_MOVE_SHT[]    = { "0.08","0.12","0.15","0.20","0.26" };
static const char* LBL_MOVE_MAX[]    = { "0.024","0.030","0.042","0.055","0.072" };
static const char* LBL_TURN_MAX[]    = { "0.030","0.038","0.050","0.065","0.085" };
static const char* LBL_JUMP_PX_ZH[]  = { "关","6px","9px","12px" };
static const char* LBL_JUMP_PX_EN[]  = { "Off","6px","9px","12px" };
static const char* LBL_BACK_RT[]     = { "0.55x","0.72x","0.88x","1.00x" };
static const char* LBL_DENSITY_ZH[]  = { "32列","48列","64列" };
static const char* LBL_DENSITY_EN[]  = { "32col","48col","64col" };
static const char* LBL_BRIGH[]       = { "0.6x","0.8x","1.0x","1.25x","1.5x" };
static const char* LBL_VOL_ZH[]      = { "低","中","高" };
static const char* LBL_VOL_EN[]      = { "Low","Mid","High" };
static const char* LBL_MAZE_SC[]     = { "0.6x","0.8x","1.0x","1.2x","1.5x" };
static const char* LBL_HP[]          = { "8HP","10HP","12HP","16HP","20HP" };
static const char* LBL_REGEN[]       = { "0.5x","1.0x","2.0x","3.0x" };
static const char* LBL_AMMO[]        = { "0.5x","1.0x","2.0x","3.0x" };
static const char* LBL_ENDLESS[]     = { "F1","F10","F25","F50","F99" };
static const char* LBL_MCSZ[]        = { "11x11","15x15","19x19","23x23" };
static const char* LBL_MCDAY[]       = { "1024","512","256","128" };
static const char* LBL_MCBLK_ZH[]    = { "砖","石","木","草","土","沙","原木","叶" };
static const char* LBL_MCBLK_EN[]    = { "Brk","Stn","Wd","Grs","Drt","Snd","Log","Lef" };
static const char* LBL_ONOFF_ZH[]    = { "关","开" };
static const char* LBL_ONOFF_EN[]    = { "Off","On" };

// --- 简单设置 (所有模式可见): 音效/开场 ---
#define SET_SIMPLE(type_,vp,mn,mx,lzh,len,fmt,lzh2,len2) \
    { type_, vp, mn, mx, lzh, len, fmt, lzh2, len2 }
static const SetEntry SIMPLE_SETS[] = {
  SET_SIMPLE(SET_BOOL, &g.sfx_enabled,     0,1, LBL_ONOFF_ZH,LBL_ONOFF_EN, NULL, "音效",     "Sound FX"),
  SET_SIMPLE(SET_BOOL, &g.opening_enabled, 0,1, LBL_ONOFF_ZH,LBL_ONOFF_EN, NULL, "开场动画", "Opening"),
};
#undef SIMPLE_SET_COUNT
#define SIMPLE_SET_COUNT (sizeof(SIMPLE_SETS)/sizeof(SIMPLE_SETS[0]))

// --- 开发者设置 (dev_mode 解锁后追加, 20+ 项) ---
static const SetEntry DEV_SETS[] = {
  // 控制类 (7)
  { SET_VAL8, &g.cfg_turn_sens,    0,5, LBL_TURN_SENS,LBL_TURN_SENS, NULL, "转向灵敏度", "Turn Sens"},
  { SET_VAL8, &g.cfg_turn_short,   0,4, LBL_SHORT_DEG,LBL_SHORT_DEG, NULL, "短按转角",   "Turn Short"},
  { SET_VAL8, &g.cfg_move_short,   0,4, LBL_MOVE_SHT, LBL_MOVE_SHT,  NULL, "短按步幅",   "Move Short"},
  { SET_VAL8, &g.cfg_move_max,     0,4, LBL_MOVE_MAX, LBL_MOVE_MAX,  NULL, "移动速度",   "Move Speed"},
  { SET_VAL8, &g.cfg_turn_max,     0,4, LBL_TURN_MAX, LBL_TURN_MAX,  NULL, "转向速度",   "Turn Speed"},
  { SET_VAL8, &g.cfg_jump_height,  0,3, LBL_JUMP_PX_ZH,LBL_JUMP_PX_EN,NULL,"跳跃高度",   "Jump Hgt"},
  { SET_VAL8, &g.cfg_back_ratio,   0,3, LBL_BACK_RT,  LBL_BACK_RT,   NULL, "后退速度",   "Back Ratio"},
  // 画面类 (5)
  { SET_VAL8, &g.cfg_density,      0,2, LBL_DENSITY_ZH,LBL_DENSITY_EN,NULL,"渲染密度",   "Density"},
  { SET_BOOL, &g.cfg_fog,          0,1, LBL_ONOFF_ZH, LBL_ONOFF_EN,  NULL, "距离雾效",   "Fog"},
  { SET_VAL8, &g.cfg_brightness,   0,4, LBL_BRIGH,    LBL_BRIGH,     NULL, "亮度",       "Brightness"},
  { SET_BOOL, &g.cfg_sky_ceil,     0,1, LBL_ONOFF_ZH, LBL_ONOFF_EN,  NULL, "天空天花板", "Sky/Ceiling"},
  { SET_BOOL, &g.cfg_floor_tex,    0,1, LBL_ONOFF_ZH, LBL_ONOFF_EN,  NULL, "地板纹理",   "Floor Tex"},
  // 音效类 (3)
  { SET_VAL8, &g.cfg_sfx_vol,      0,2, LBL_VOL_ZH,   LBL_VOL_EN,    NULL, "音量",       "Volume"},
  { SET_BOOL, &g.cfg_sfx_menu,     0,1, LBL_ONOFF_ZH, LBL_ONOFF_EN,  NULL, "菜单音效",   "Menu SFX"},
  { SET_BOOL, &g.cfg_sfx_combat,   0,1, LBL_ONOFF_ZH, LBL_ONOFF_EN,  NULL, "战斗音效",   "Combat SFX"},
  // 游戏参数 (5)
  { SET_VAL8, &g.cfg_maze_scale,   0,4, LBL_MAZE_SC,  LBL_MAZE_SC,   NULL, "迷宫缩放",   "Maze Scale"},
  { SET_VAL8, &g.cfg_hp_start,     0,4, LBL_HP,       LBL_HP,        NULL, "初始血量",   "Start HP"},
  { SET_VAL8, &g.cfg_regen_rate,   0,3, LBL_REGEN,    LBL_REGEN,     NULL, "回血速度",   "Regen Rate"},
  { SET_VAL8, &g.cfg_ammo_mul,     0,3, LBL_AMMO,     LBL_AMMO,      NULL, "弹药倍率",   "Ammo Mul"},
  { SET_VAL8, &g.cfg_endless_start,0,4, LBL_ENDLESS,  LBL_ENDLESS,   NULL, "无尽起始",   "Endless Strt"},
  // MC 沙盒 (4)
  { SET_VAL8, &g.cfg_mc_size,      0,3, LBL_MCSZ,     LBL_MCSZ,      NULL, "MC地图大小", "MC Size"},
  { SET_VAL8, &g.cfg_mc_day_len,   0,3, LBL_MCDAY,    LBL_MCDAY,     NULL, "MC日夜速度", "MC Day Len"},
  { SET_BOOL, &g.cfg_mc_jump,      0,1, LBL_ONOFF_ZH, LBL_ONOFF_EN,  NULL, "MC跳跃",     "MC Jump"},
  { SET_VAL8, &g.cfg_mc_start_sel, 0,7, LBL_MCBLK_ZH, LBL_MCBLK_EN,  NULL, "MC初始方块", "MC Block"},
  // 调试 (1)
  { SET_BOOL, &g.show_debug,       0,1, LBL_ONOFF_ZH, LBL_ONOFF_EN,  NULL, "调试信息",   "Debug Info"},
};
#define DEV_SET_COUNT (sizeof(DEV_SETS)/sizeof(DEV_SETS[0]))

// 总数
static int settings_count(void) {
    return (int)SIMPLE_SET_COUNT + (g.dev_mode ? (int)DEV_SET_COUNT : 0);
}

// 获取第 idx 个设置条目
static const SetEntry* settings_get(int idx) {
    if(idx < (int)SIMPLE_SET_COUNT) return &SIMPLE_SETS[idx];
    idx -= (int)SIMPLE_SET_COUNT;
    if(idx < (int)DEV_SET_COUNT)   return &DEV_SETS[idx];
    return NULL;
}

// 格式化设置值为字符串 (写到 buf) — 双语
static void settings_val_str(const SetEntry* e, char* buf, int bufsize) {
    uint8_t v = *(uint8_t*)(e->val_ptr);
    bool zh = (g.lang == LANG_ZH);
    if(e->type == SET_BOOL) {
        snprintf(buf, bufsize, "%s", zh ? LBL_ONOFF_ZH[v ? 1 : 0] : LBL_ONOFF_EN[v ? 1 : 0]);
        return;
    }
    // SET_VAL8: 优先用档位标签
    const char** lbl = zh ? e->labels_zh : e->labels_en;
    if(lbl) {
        int i = v;
        if(i < 0) i = 0;
        if(i > (int)e->max_val) i = (int)e->max_val;
        snprintf(buf, bufsize, "%s", lbl[i]);
        return;
    }
    if(e->fmt_num) {
        float f = 0.0f;
        if(e->val_ptr == (void*)&g.cfg_turn_sens) {
            static const float tv[] = {1.0f,1.25f,1.5f,1.75f,2.0f,2.5f};
            f = tv[v % 6];
        }
        snprintf(buf, bufsize, e->fmt_num, (double)f);
        return;
    }
    snprintf(buf, bufsize, "%d", (int)v);
}

// 开发模式解锁: 检测隐藏长按按键序列 (上/下/左/右).
// 在菜单和开场动画期间均可调用. 返回是否触发了音效 (供调用方判断).
static void dev_mode_try_unlock(InputKey key) {
    if(g.dev_mode) return;
    InputKey expect[4] = { InputKeyUp, InputKeyDown, InputKeyLeft, InputKeyRight };
    if(s_dev_seq < 4 && key == expect[s_dev_seq]) {
        s_dev_seq++;
        if(s_dev_seq >= 4) {
            g.dev_mode = true;
            storage_save();
            sfx_play(SFX_QUEST_DONE);
        } else {
            if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE);
        }
    } else {
        // 错误按键: 若是序列首键则重新开始, 否则清零
        s_dev_seq = (key == expect[0]) ? 1 : 0;
    }
}

static void draw_settings(Canvas* c) {
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    // 标题 (居中)
    if(g.dev_mode) {
        char tb[32];
        if(g.lang == LANG_ZH)
            snprintf(tb, sizeof(tb), "开发者设置 %d/%d", s_set_sel+1, settings_count());
        else
            snprintf(tb, sizeof(tb), "Dev Settings %d/%d", s_set_sel+1, settings_count());
        canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, tb);
    } else {
        if(g.lang == LANG_ZH)
            canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, "设置");
        else
            canvas_draw_str_aligned(c, 64, 1, AlignCenter, AlignTop, "SETTINGS");
    }
    canvas_draw_line(c, 0, 11, 127, 11);

    int n = settings_count();
    if(n == 0) return;
    if(s_set_sel >= n) s_set_sel = n - 1;

    // 右上角分类标签
    if(g.dev_mode) {
        static const struct { int from; int to; const char* zh; const char* en; } cat[] = {
            {0,1, "基础","Basic"}, {2,8, "操控","Control"}, {9,13,"画面","Video"},
            {14,16,"音效","Audio"}, {17,21,"游戏","Game"}, {22,25,"MC","MC"}, {26,26,"调试","Debug"},
        };
        const char* tag = (g.lang == LANG_ZH) ? "设置" : "Settings";
        for(unsigned k = 0; k < sizeof(cat)/sizeof(cat[0]); k++) {
            if(s_set_sel >= cat[k].from && s_set_sel <= cat[k].to) {
                tag = (g.lang == LANG_ZH) ? cat[k].zh : cat[k].en;
                break;
            }
        }
        canvas_draw_str_aligned(c, 127, 1, AlignRight, AlignTop, tag);
    }

    // ---- 真正居中: 选中项永远在可见区正中央, 列表围绕它滚动 ----
    // 可见区 y=12..51 (高40px), 行高8px → 5行, 选中行固定在第3行(y=28)
    int y_top = 12, y_bot = 51;
    int row_h = SET_ROW_H;
    int center_y = (y_top + y_bot) / 2;       // 31
    int sel_y = center_y - row_h / 2;          // 选中行顶部 = 27

    for(int i = 0; i < n; i++) {
        int rel = i - (int)s_set_sel;
        int y = sel_y + rel * row_h;
        // 超出可见区不绘制
        if(y + row_h <= y_top || y >= y_bot) continue;
        const SetEntry* e = settings_get(i);
        if(!e) continue;

        bool sel = (i == (int)s_set_sel);
        if(sel) {
            canvas_draw_box(c, 0, y, 128, row_h);
            canvas_set_color(c, ColorWhite);
            canvas_draw_str(c, 1, y + 7, "<");
            canvas_draw_str(c, 122, y + 7, ">");
        }
        // 标签 (左) — 双语
        const char* lbl = (g.lang == LANG_ZH) ? e->label_zh : e->label_en;
        canvas_draw_str(c, 8, y + 7, lbl);
        // 值 (右对齐)
        char vbuf[16];
        settings_val_str(e, vbuf, sizeof(vbuf));
        char wrap[24]; snprintf(wrap, sizeof(wrap), "[%s]", vbuf);
        canvas_draw_str_aligned(c, 119, y + 7, AlignRight, AlignBottom, wrap);
        canvas_set_color(c, ColorBlack);
    }

    // 滚动进度条 (右侧竖条)
    if(n > 1) {
        int bar_x = 126, bar_y = y_top, bar_h = y_bot - y_top;
        float ratio = (float)s_set_sel / (float)(n - 1);
        int thumb_h = bar_h / 4; if(thumb_h < 2) thumb_h = 2;
        int thumb_y = bar_y + (int)(ratio * (float)(bar_h - thumb_h));
        canvas_draw_frame(c, bar_x, bar_y, 2, bar_h);
        canvas_draw_box(c, bar_x, thumb_y, 2, thumb_h);
    }

    // 底部分隔 + 操作提示
    canvas_draw_line(c, 0, 52, 127, 52);
    if(g.lang == LANG_ZH) {
        canvas_draw_str(c, 2, 62, g.dev_mode ? "左右调 上下移 OK切换" : "上下选 OK切换");
        canvas_draw_str_aligned(c, 126, 62, AlignRight, AlignBottom, "返回");
    } else {
        canvas_draw_str(c, 2, 62, g.dev_mode ? "L/R:Adj U/D:Nav OK:Toggle" : "U/D:Sel OK:Toggle");
        canvas_draw_str_aligned(c, 126, 62, AlignRight, AlignBottom, "Back");
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

        // 菜单项: 1.闯关模式  2.无尽挑战  3.游客漫游  4.设置  5.MC模式(Beta)  6.积分商城
        const uint8_t* zh_items[M_COUNT] = { m1_bits, m2_bits, m3_bits, m4_bits, m5_bits, NULL };
        const char*    en_items[M_COUNT] = { EN_M1, EN_M2, EN_M3, "4. Settings", "5. MC Beta", "6. Shop" };
        const int ws[M_COUNT] = { M1_W, M2_W, M3_W, M4_W, M5_W, 0 };
        const int hs[M_COUNT] = { M1_H, M2_H, M3_H, M4_H, M5_H, 0 };
        // v6.11: 商城项无 XBM 位图, 中英文都用文字显示
        const char* zh_text_items[M_COUNT] = { NULL, NULL, NULL, NULL, NULL, "6. 商城" };
        // ---- 真正居中: 选中项永远在可见区正中央, 列表围绕它滚动 ----
        int y_top = 17, y_bot = 51;
        int row_h = MENU_ROW_H;
        int center_y = (y_top + y_bot) / 2;
        int sel_y = center_y - row_h / 2;  // 选中行顶部
        s_menu_off = 0; // 不再使用 offset
        for(int i = 0; i < M_COUNT; i++) {
            int rel = i - s_sel;
            int yy = sel_y + rel * row_h + 8; // 文字 baseline 偏移
            int box_y = yy - 9;
            // 超出可见区不绘制
            if(box_y + 10 <= y_top || box_y >= y_bot) continue;
            if(i == s_sel) {
                canvas_draw_box(canvas, 0, box_y, 128, 10);
                canvas_set_color(canvas, ColorWhite);
            }
            if(g.lang == LANG_ZH) {
                if(zh_items[i] != NULL) {
                    canvas_draw_xbm(canvas, 4, yy - 8, ws[i], hs[i], zh_items[i]);
                } else if(zh_text_items[i] != NULL) {
                    canvas_draw_str(canvas, 6, yy, zh_text_items[i]);
                }
            } else {
                canvas_draw_str(canvas, 6, yy, en_items[i]);
            }
            canvas_set_color(canvas, ColorBlack);
        }
        // 滚动进度条
        if(M_COUNT > 1) {
            int bar_x = 125, bar_y = y_top, bar_h = y_bot - y_top;
            float ratio = (float)s_sel / (float)(M_COUNT - 1);
            int thumb_h = bar_h / 4; if(thumb_h < 2) thumb_h = 2;
            int thumb_y = bar_y + (int)(ratio * (float)(bar_h - thumb_h));
            canvas_draw_frame(canvas, bar_x, bar_y, 2, bar_h);
            canvas_draw_box(canvas, bar_x, thumb_y, 2, thumb_h);
        }

        // 底部提示 (含语言切换说明)
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
    // v6.11: 商城/道具栏从主菜单进入时无 raycast 内容, 跳过 fb 拷贝
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(g.mode != MODE_SHOP && g.mode != MODE_SHOP_INV) {
        canvas_draw_xbm(canvas, 0, 0, SCREEN_W, SCREEN_H, g.fb);
    }

    // v6.1: 顶部状态条 — 关卡/层数 + 心形血量 + 骷髅剩余敌人 + 星星击杀 + 弹药
    // 半透明效果: 用白底黑字覆盖在 3D 画面顶部
    if(g.mode != MODE_MC) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 0, 128, 11);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        int lv = (g.mode == MODE_CAMPAIGN) ? g.level : g.endless_floor;
        const char* ltag = (g.mode == MODE_CAMPAIGN) ? "L" : "F";
        // 左侧: 关卡/层数 (文字)
        char sb[20];
        snprintf(sb, sizeof(sb), "%s%d", ltag, lv);
        canvas_draw_str(canvas, 1, 9, sb);
        // 心形血量 (最多画 5 颗心, 超出用数字)
        int hx = 22;
        int hp = g.player.health;
        int maxhp = g.player.max_health;
        if(maxhp > 5) {
            // 高血量: 画 1 颗心 + 数字
            canvas_draw_heart(canvas, hx, 2, true);
            char hpb[12]; snprintf(hpb, sizeof(hpb), "x%d", hp);
            canvas_draw_str(canvas, hx + 9, 9, hpb);
            hx += 24;
        } else {
            for(int i = 0; i < maxhp; i++) {
                canvas_draw_heart(canvas, hx + i * 8, 2, i < hp);
            }
            hx += maxhp * 8 + 2;
        }
        // 右侧: 战斗关显示 骷髅+剩余敌人 + 星星+击杀 + 弹药
        if(g.stage == STAGE_COMBAT) {
            int alive = 0;
            for(int i = 0; i < g.actor_count; i++)
                if(g.actors[i].active && g.actors[i].type == 0 && g.actors[i].hp > 0) alive++;
            // 弹药 (左中)
            char ab[12]; snprintf(ab, sizeof(ab), "A%d", g.ammo);
            canvas_draw_str(canvas, 70, 9, ab);
            // 星星击杀数
            canvas_draw_star(canvas, 84, 2);
            char kc[12]; snprintf(kc, sizeof(kc), "%d", g.task_kill_count);
            canvas_draw_str(canvas, 92, 9, kc);
            // 骷髅剩余敌人 (最右)
            canvas_draw_skull(canvas, 108, 2);
            char ec[12]; snprintf(ec, sizeof(ec), "%d", alive);
            canvas_draw_str(canvas, 117, 9, ec);
            // 没弹药时 AM 闪烁警告
            if(g.ammo == 0 && (g.tick & 7) < 4) {
                canvas_draw_str(canvas, 70, 9, "AM!");
            }
        } else if(g.stage == STAGE_PUZZLE) {
            // 解谜: 钥匙数 + 积分
            snprintf(sb, sizeof(sb), "K%d", g.player.keys);
            canvas_draw_str(canvas, 70, 9, sb);
            char sc[16]; snprintf(sc, sizeof(sc), "S%lu", (unsigned long)g.score);
            canvas_draw_str_aligned(canvas, 127, 9, AlignRight, AlignBottom, sc);
        } else if(g.stage == STAGE_MAZE_ONLY) {
            // v6.10: 纯迷宫关显示积分
            char sc[16]; snprintf(sc, sizeof(sc), "S%lu", (unsigned long)g.score);
            canvas_draw_str_aligned(canvas, 127, 9, AlignRight, AlignBottom, sc);
        }
    } else {
        // v6.4 MC 模式 HUD: 顶部简洁信息 + 底部物品栏 + 仿 MC 血条
        // 顶部: 已挖数 + 时间(日/夜)
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 0, 128, 10);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        // 太阳/月亮指示 + 已挖数
        // v6.9: cfg_mc_day_len → 周期掩码 1023/511/255/127, 白天半周期
        static const uint16_t mc_day_masks[] = { 1023u, 511u, 255u, 127u };
        uint16_t mc_mask = mc_day_masks[(g.cfg_mc_day_len < 4) ? g.cfg_mc_day_len : 0];
        bool is_day = ((g.tick & mc_mask) < ((mc_mask + 1) >> 1));
        char top[32];
        snprintf(top, sizeof(top), "%s M%d", is_day ? "DAY" : "NITE", g.mc_mined);
        canvas_draw_str(canvas, 1, 8, top);
        // 成就统计
        char ach[24];
        snprintf(ach, sizeof(ach), "K%lu C%lu", g.ach_total_kills, g.ach_total_clears);
        canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, ach);

        // 底部物品栏: 8 格方块图标 (16x16 每格, 选中格高亮框)
        const char* bn[8] = { "Brk", "Stn", "Wd", "Grs",
                              "Drt", "Snd", "Log", "Lef" };
        int slot_w = 15;
        int bar_x = (128 - 8 * slot_w) / 2;
        int bar_y = 49;
        // 背景
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, bar_x - 1, bar_y - 1, 8 * slot_w + 2, 15);
        canvas_set_color(canvas, ColorBlack);
        for(int i = 0; i < 8; i++) {
            int sx = bar_x + i * slot_w;
            // 选中格: 反色填充
            if(i == g.mc_block_type - 1) {
                canvas_draw_box(canvas, sx, bar_y, slot_w - 1, 13);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str_aligned(canvas, sx + slot_w/2, bar_y + 10,
                    AlignCenter, AlignBottom, bn[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_frame(canvas, sx, bar_y, slot_w - 1, 13);
                canvas_draw_str_aligned(canvas, sx + slot_w/2, bar_y + 10,
                    AlignCenter, AlignBottom, bn[i]);
            }
        }
        // 仿 MC 血条: 心形横排 (底部最下, 10 颗, 当前血量亮)
        int hp_y = 63;
        int hp_max = g.player.max_health;
        if(hp_max > 10) hp_max = 10;
        int hp_x = (128 - hp_max * 6) / 2;
        for(int i = 0; i < hp_max; i++) {
            canvas_draw_heart(canvas, hp_x + i * 6, hp_y, i < g.player.health);
        }
        // 操作提示 (顶部右侧闪烁)
        if((g.tick & 15) < 12) {
            const char* hint = (g.lang == LANG_ZH)
                ? "OK放 长OK挖 Back切"
                : "OK place OK-mine Back";
            canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignBottom, hint);
        }
    }

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

    // 提示信息 (toast): 始终显示, 不依赖 show_hud. 左下角带边框, 闪烁.
    } // end if(g.show_hud)
    if(g.msg_id >= 0 && g.msg_ttl > 0 && g.mode != MODE_PAUSED) {
        // 闪烁: 后半段每 4 tick 闪一次
        bool blink = (g.msg_ttl > 30) || ((g.tick & 3) < 2);
        if(blink) {
            const uint8_t* b = NULL; int w = 0, h = 0, bpr = 0;
            if(g.lang == LANG_ZH) get_msg_bmp(g.msg_id, &b, &w, &h, &bpr);
            const char* s = (b == NULL) ? en_msg_str(g.msg_id) : NULL;
            // 测量尺寸 -> 边框
            int bx = 1, by = 50, bw, bh;
            if(b) { bw = w + 4; bh = h + 2; }
            else if(s) {
                bw = (int)canvas_string_width(canvas, s) + 4;
                bh = 11;
            } else { bw = 0; bh = 0; }
            if(bw > 0) {
                if(by + bh > 63) by = 63 - bh;
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_box(canvas, bx, by, bw, bh);
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_frame(canvas, bx, by, bw, bh);
                if(b) canvas_draw_xbm(canvas, bx + 2, by + 1, w, h, b);
                else  canvas_draw_str(canvas, bx + 2, by + bh - 2, s);
            }
        }
    }

    // 调试信息覆盖层 (设置中开启 show_debug 后显示)
    if(g.show_debug) {
        // 顶部白底信息条
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 0, 128, 11);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        char db[32];
        int px = (int)(g.player.x * 100);
        int py = (int)(g.player.y * 100);
        int lv = (g.mode == MODE_CAMPAIGN) ? g.level : g.endless_floor;
        snprintf(db, sizeof(db), "L%d X%d Y%d T%d A%d",
                 lv, px, py, g.tick, g.actor_count);
        canvas_draw_str(canvas, 1, 9, db);
        // 朝向 (右下角小字)
        int dx = (int)(g.player.dir_x * 100);
        int dy = (int)(g.player.dir_y * 100);
        snprintf(db, sizeof(db), "D%d,%d", dx, dy);
        canvas_draw_str_aligned(canvas, 127, 9, AlignRight, AlignBottom, db);
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
        // v6.10: 积分显示 + 商城提示
        char sb[28];
        if(g.lang == LANG_ZH) snprintf(sb, sizeof(sb), "积分:%lu  左=商城", (unsigned long)g.score);
        else snprintf(sb, sizeof(sb), "Score:%lu  L=Shop", (unsigned long)g.score);
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignCenter, sb);
    } else if(g.mode == MODE_SHOP) {
        // v6.11: 积分商城 — 10 种道具, 居中滚动
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        char tb[28];
        if(g.lang == LANG_ZH) snprintf(tb, sizeof(tb), "商城 积分:%lu", (unsigned long)g.score);
        else snprintf(tb, sizeof(tb), "%s Score:%lu", EN_SHOP_TITLE, (unsigned long)g.score);
        canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, tb);
        canvas_draw_line(canvas, 0, 11, 127, 11);
        // 10 项商品 (索引 0..9)
        if(g.shop_sel >= 10) g.shop_sel = 0;
        int n = 10;
        int y_top = 12, y_bot = 51, row_h = 8;
        int center_y = (y_top + y_bot) / 2;
        int sel_y = center_y - row_h / 2;
        for(int i = 0; i < n; i++) {
            int rel = i - (int)g.shop_sel;
            int y = sel_y + rel * row_h;
            if(y + row_h <= y_top || y >= y_bot) continue;
            bool sel = (i == (int)g.shop_sel);
            if(sel) {
                canvas_draw_box(canvas, 0, y, 128, row_h);
                canvas_set_color(canvas, ColorWhite);
            }
            const char* name = (g.lang == LANG_ZH) ? shop_item_name_zh(i) : shop_item_name_en(i);
            const char* unit = (g.lang == LANG_ZH) ? "分" : "pts";
            char line[40];
            snprintf(line, sizeof(line), "%s  %d%s  x%d", name, shop_item_price(i), unit, g.items[i]);
            canvas_draw_str(canvas, 4, y + 7, line);
            if(sel) canvas_set_color(canvas, ColorBlack);
        }
        // 进度条
        if(n > 1) {
            int bar_x = 126, bar_y = y_top, bar_h = y_bot - y_top;
            float ratio = (float)g.shop_sel / (float)(n - 1);
            int thumb_h = bar_h / 4; if(thumb_h < 2) thumb_h = 2;
            int thumb_y = bar_y + (int)(ratio * (float)(bar_h - thumb_h));
            canvas_draw_frame(canvas, bar_x, bar_y, 2, bar_h);
            canvas_draw_box(canvas, bar_x, thumb_y, 2, thumb_h);
        }
        canvas_draw_line(canvas, 0, 52, 127, 52);
        // 底部: 当前选中项说明
        const char* desc = (g.lang == LANG_ZH) ? shop_item_desc_zh((int)g.shop_sel) : shop_item_desc_en((int)g.shop_sel);
        canvas_draw_str(canvas, 2, 62, desc);
        if(g.lang == LANG_ZH) canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "OK购 Back");
        else canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "OK:Buy Back");
    } else if(g.mode == MODE_SHOP_INV) {
        // v6.11: 新道具栏 — 长按 OK 在游戏中呼出, 显示库存 + 使用
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        char tb[32];
        if(g.lang == LANG_ZH) snprintf(tb, sizeof(tb), "道具栏  HP:%d/%d  弹:%d", g.player.health, g.player.max_health, g.ammo);
        else snprintf(tb, sizeof(tb), "%s  HP:%d/%d  Ammo:%d", EN_SHOP_INV_TITLE, g.player.health, g.player.max_health, g.ammo);
        canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, tb);
        canvas_draw_line(canvas, 0, 11, 127, 11);
        if(g.inv2_sel >= 10) g.inv2_sel = 0;
        int n = 10;
        int y_top = 12, y_bot = 51, row_h = 8;
        int center_y = (y_top + y_bot) / 2;
        int sel_y = center_y - row_h / 2;
        for(int i = 0; i < n; i++) {
            int rel = i - (int)g.inv2_sel;
            int y = sel_y + rel * row_h;
            if(y + row_h <= y_top || y >= y_bot) continue;
            bool sel = (i == (int)g.inv2_sel);
            if(sel) {
                canvas_draw_box(canvas, 0, y, 128, row_h);
                canvas_set_color(canvas, ColorWhite);
            }
            const char* name = (g.lang == LANG_ZH) ? shop_item_name_zh(i) : shop_item_name_en(i);
            const char* desc = (g.lang == LANG_ZH) ? shop_item_desc_zh(i) : shop_item_desc_en(i);
            char line[40];
            snprintf(line, sizeof(line), "%s  x%d  %s", name, g.items[i], desc);
            canvas_draw_str(canvas, 4, y + 7, line);
            if(sel) canvas_set_color(canvas, ColorBlack);
        }
        // 进度条
        if(n > 1) {
            int bar_x = 126, bar_y = y_top, bar_h = y_bot - y_top;
            float ratio = (float)g.inv2_sel / (float)(n - 1);
            int thumb_h = bar_h / 4; if(thumb_h < 2) thumb_h = 2;
            int thumb_y = bar_y + (int)(ratio * (float)(bar_h - thumb_h));
            canvas_draw_frame(canvas, bar_x, bar_y, 2, bar_h);
            canvas_draw_box(canvas, bar_x, thumb_y, 2, thumb_h);
        }
        canvas_draw_line(canvas, 0, 52, 127, 52);
        // buff 状态显示
        if(g.buff_shield > 0 || g.buff_doublefire > 0) {
            char bf[40];
            if(g.lang == LANG_ZH)
                snprintf(bf, sizeof(bf), "护盾:%ds 火力:%ds",
                    g.buff_shield / 60, g.buff_doublefire / 60);
            else
                snprintf(bf, sizeof(bf), "Shield:%ds 2xFire:%ds",
                    g.buff_shield / 60, g.buff_doublefire / 60);
            canvas_draw_str(canvas, 2, 62, bf);
        }
        if(g.lang == LANG_ZH) canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "OK用 Back");
        else canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "OK:Use Back");
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
        else if(key == InputKeyLeft) { g.mode = MODE_SHOP; g.shop_sel = 0; } // v6.10: 进入商城
    } else if(g.mode == MODE_SHOP) {
        // v6.11: 商城输入 — 10 项道具购买系统
        if(key == InputKeyUp) {
            g.shop_sel = (g.shop_sel + 9) % 10;
            if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE);
        } else if(key == InputKeyDown) {
            g.shop_sel = (g.shop_sel + 1) % 10;
            if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE);
        } else if(key == InputKeyOk) {
            // 购买: 库存上限 99, 积分足够才扣
            uint16_t price = shop_item_price(g.shop_sel);
            if(g.items[g.shop_sel] >= 99) {
                set_msg(MSG_SHOP_FULL);
                sfx_play(SFX_NO_AMMO);
            } else if(g.score >= price) {
                g.score -= price;
                g.items[g.shop_sel]++;
                set_msg(MSG_SHOP_BUY);
                sfx_play(SFX_MENU_OK);
                storage_save();
            } else {
                set_msg(MSG_SHOP_FAIL);
                sfx_play(SFX_NO_AMMO);
            }
        } else if(key == InputKeyBack) {
            // 通关画面进来的回通关画面, 主菜单进来的回主菜单
            g.mode = (s_resume_mode == MODE_LEVEL_CLEAR) ? MODE_LEVEL_CLEAR : MODE_MENU;
        }
    } else if(g.mode == MODE_SHOP_INV) {
        // v6.11: 新道具栏输入
        if(key == InputKeyUp) {
            g.inv2_sel = (g.inv2_sel + 9) % 10;
            if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE);
        } else if(key == InputKeyDown) {
            g.inv2_sel = (g.inv2_sel + 1) % 10;
            if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE);
        } else if(key == InputKeyOk) {
            shop_item_use(g.inv2_sel); // 使用道具 (内部已处理提示/音效)
        } else if(key == InputKeyBack) {
            g.mode = s_resume_mode; // 返回游戏
        }
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
    g.ls_max = 50;   // v6.0: 关卡上限提到 50
    // v6.9: cfg_endless_start 0..4 → F1/F10/F25/F50/F99
    static const int els[] = { 1, 10, 25, 50, 99 };
    int default_endless = els[(g.cfg_endless_start < 5) ? g.cfg_endless_start : 0];
    g.ls_sel = for_campaign ? (g.campaign_cleared + 1) : default_endless;
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
            bool locked = g.ls_for_campaign && (g.ls_sel > g.campaign_cleared + 1) && !g.dev_mode;
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
                // 开场动画不可通过按键跳过 (只能通过设置关闭), 让玩家完整听完 BGM.
                // 期间允许长按 上/下/左/右 输入开发者模式解锁序列.
                if(type == InputTypeLong) dev_mode_try_unlock(key);
                did_input = true;
            } else if(g.mode == MODE_SETTINGS) {
                int nset = settings_count();
                if(type == InputTypeShort) {
                    if(key == InputKeyUp) {
                        s_set_sel = (s_set_sel + nset - 1) % nset;
                        if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE);
                    } else if(key == InputKeyDown) {
                        s_set_sel = (s_set_sel + 1) % nset;
                        if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE);
                    } else if(key == InputKeyLeft || key == InputKeyRight) {
                        const SetEntry* e = settings_get(s_set_sel);
                        if(e) {
                            int dir = (key == InputKeyRight) ? +1 : -1;
                            // SET_BOOL: 统一按 uint8_t 指针访问 (bool 与 uint8_t 同尺寸 0/1 值一致)
                            uint8_t* vp = (uint8_t*)(e->val_ptr);
                            if(e->type == SET_BOOL) {
                                *vp = (*vp == 0) ? 1 : 0;
                            } else if(e->type == SET_VAL8) {
                                int v = (int)(*vp) + dir;
                                if(v < (int)e->min_val) v = (int)e->max_val;
                                if(v > (int)e->max_val) v = (int)e->min_val;
                                *vp = (uint8_t)v;
                            }
                            sfx_play(SFX_MENU_MOVE);
                            storage_save();
                        }
                    } else if(key == InputKeyOk) {
                        const SetEntry* e = settings_get(s_set_sel);
                        if(e) {
                            uint8_t* vp = (uint8_t*)(e->val_ptr);
                            if(e->type == SET_BOOL) {
                                *vp = (*vp == 0) ? 1 : 0;
                            } else if(e->type == SET_VAL8) {
                                // OK 键: 布尔切换/数值 也 +1 循环 (方便用户)
                                int v = (int)(*vp) + 1;
                                if(v > (int)e->max_val) v = (int)e->min_val;
                                *vp = (uint8_t)v;
                            }
                            sfx_play(SFX_MENU_OK);
                            storage_save();
                        }
                    } else if(key == InputKeyBack) {
                        g.mode = MODE_MENU;
                        sfx_play(SFX_MENU_OK);
                    }
                }
                did_input = true;
            } else if(g.mode == MODE_MENU) {
                if(type == InputTypeShort) {
                    if(key == InputKeyUp) { s_sel = (s_sel + M_COUNT - 1) % M_COUNT; if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE); }
                    else if(key == InputKeyDown) { s_sel = (s_sel + 1) % M_COUNT; if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE); }
                    else if(key == InputKeyLeft || key == InputKeyRight)
                        g.lang = (g.lang == LANG_ZH) ? LANG_EN : LANG_ZH;
                    else if(key == InputKeyOk) {
                        storage_load();
                        sfx_play(SFX_MENU_OK);
                        if(s_sel == M_CAMPAIGN) enter_level_select(true);
                        else if(s_sel == M_ENDLESS) enter_level_select(false);
                        else if(s_sel == M_VISITOR) game_init_endless(g.endless_floor, true);
                        else if(s_sel == M_SETTINGS) { g.mode = MODE_SETTINGS; s_set_sel = 0; s_set_off = 0; }
                        else if(s_sel == M_MC) game_init_mc();
                        else if(s_sel == M_SHOP) { g.mode = MODE_SHOP; g.shop_sel = 0; g.shop_page = 0; g.shop_page_sel = 0; }
                    } else if(key == InputKeyBack) { running = false; sfx_stop_all(); }
                } else if(type == InputTypeLong) {
                    // 开发模式解锁: 检测隐藏按键序列
                    dev_mode_try_unlock(key);
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
            } else if(g.mode == MODE_SHOP || g.mode == MODE_SHOP_INV) {
                // v6.11: 商城 + 道具栏 输入
                if(type == InputTypeShort) {
                    if((g.mode == MODE_SHOP && (key == InputKeyUp || key == InputKeyDown)) ||
                       (g.mode == MODE_SHOP_INV && (key == InputKeyUp || key == InputKeyDown))) {
                        if(g.cfg_sfx_menu) sfx_play(SFX_MENU_MOVE);
                    }
                    if(g.mode == MODE_SHOP && key == InputKeyOk) sfx_play(SFX_MENU_OK);
                    if(g.mode == MODE_SHOP_INV && key == InputKeyOk) sfx_play(SFX_PICK_ITEM);
                    if(key == InputKeyBack) sfx_play(SFX_MENU_OK);
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
            } else if(g.mode == MODE_MC) {
                // v6.7-beta MC 模式输入 (Beta 保护):
                //   OK 短按 = 放置方块
                //   OK 长按 = 挖掘方块
                //   Back 短按 = 切换手持物品
                //   Back 长按 = 跳跃 (Beta 新功能)
                //   连续 3 次 Back 长按 = 返回主菜单 (防误触退出)
                if(key == InputKeyOk && type == InputTypeShort) {
                    mc_place();
                } else if(key == InputKeyOk && type == InputTypeLong) {
                    mc_mine();
                } else if(key == InputKeyBack && type == InputTypeShort) {
                    mc_cycle_block();
                } else if(key == InputKeyBack && type == InputTypeLong) {
                    // v6.7-beta: 跳跃 + 三次连续长按退出保护
                    // v6.9: cfg_mc_jump=false 时禁止 MC 模式跳跃 (仅做退出计数)
                    if(g.cfg_mc_jump && g.jump_timer == 0) {
                        g.jump_timer = 1;    // 从 1 开始起跳 (game_update 里推进)
                        sfx_play(SFX_MENU_MOVE); // 跳跃轻微音效
                    }
                    // 累计连续长按计数 (beta 保护: 3 次才真退出)
                    if(g.exit_long_ttl > 0 && g.exit_long_cnt < 3) {
                        g.exit_long_cnt++;
                    } else {
                        g.exit_long_cnt = 1;
                    }
                    g.exit_long_ttl = 90; // 每次长按重置 1.5 秒有效窗口
                    if(g.exit_long_cnt >= 3) {
                        g.exit_long_cnt = 0;
                        g.exit_long_ttl = 0;
                        g.jump_timer = 0;   // 取消跳跃动画
                        g.jump_z = 0.0f;
                        g.mode = MODE_MENU;
                        sfx_stop_all();
                        sfx_play(SFX_MENU_OK);
                    }
                } else {
                    game_handle_input(key, type);
                }
                did_input = true;
            } else {
                // v6.7-beta: 所有游戏模式 (闯关/无尽/游客) — Beta 保护:
                //   OK 短按 = 射击
                //   OK 长按 = 打开物品栏
                //   Back 短按 = 暂停
                //   Back 长按 × 3 次连续 = 退出游戏 (防误触)
                if(key == InputKeyBack) {
                    if(type == InputTypeLong) {
                        if(g.exit_long_ttl > 0 && g.exit_long_cnt < 3) {
                            g.exit_long_cnt++;
                        } else {
                            g.exit_long_cnt = 1;
                        }
                        g.exit_long_ttl = 90; // 1.5 秒有效窗口
                        sfx_play(SFX_LOCKED); // 给一个确认音, 让玩家知道长按被记录了
                        if(g.exit_long_cnt >= 3) {
                            g.exit_long_cnt = 0;
                            g.exit_long_ttl = 0;
                            running = false;
                            sfx_stop_all();
                        }
                    } else if(type == InputTypeShort) {
                        s_resume_mode = g.mode;
                        g.mode = MODE_PAUSED;
                        sfx_play(SFX_MENU_OK);
                    }
                } else if(key == InputKeyOk) {
                    if(type == InputTypeShort) {
                        // v6.2: OK 短按=射击 (所有模式). 解谜/迷宫关无弹药时播"无弹药"
                        player_shoot();
                    } else if(type == InputTypeLong) {
                        // v6.11: OK 长按=打开新道具栏 (商城道具库存, 战斗/解谜/迷宫关均可用)
                        s_resume_mode = g.mode;
                        g.mode = MODE_SHOP_INV;
                        g.inv2_sel = 0;
                        sfx_play(SFX_MENU_OK);
                    }
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
            if(g.mode == MODE_CAMPAIGN || g.mode == MODE_ENDLESS_RUN ||
               g.mode == MODE_ENDLESS_VISITOR || g.mode == MODE_MC) {
                game_update();
                did_update_world = true;
            }
            last_update_tick = now;
        }

        // 按需渲染,避免 20Hz 的全速 raycasting(那是死机根源)
        bool need_render = did_input || did_update_world;
        bool in_game_view = (g.mode == MODE_CAMPAIGN || g.mode == MODE_ENDLESS_RUN ||
                             g.mode == MODE_ENDLESS_VISITOR || g.mode == MODE_PAUSED ||
                             g.mode == MODE_LEVEL_CLEAR || g.mode == MODE_GAME_OVER ||
                             g.mode == MODE_MC ||
                             g.mode == MODE_SHOP || g.mode == MODE_SHOP_INV);  // v6.11
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

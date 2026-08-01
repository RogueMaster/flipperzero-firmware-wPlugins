#include "maze3d.h"
#include "zh_chars.h"
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

// ---- 绘制回调 ----
static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    if(g.mode == MODE_MENU) {
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);
        // 标题(中文位图)
        canvas_draw_xbm(canvas, 46, 1, TITLE_W, TITLE_H, title_bits);
        // 菜单分隔线
        canvas_draw_line(canvas, 0, 16, 127, 16);

        // 菜单项: 1.闯关模式  2.无尽挑战  3.游客漫游
        const uint8_t* bitmaps[M_COUNT] = { m1_bits, m2_bits, m3_bits };
        const int ws[M_COUNT] = { M1_W, M2_W, M3_W };
        const int hs[M_COUNT] = { M1_H, M2_H, M3_H };
        for(int i = 0; i < M_COUNT; i++) {
            int yy = 22 + i * 12;
            if(i == s_sel) {
                // 反色条(白底黑字) - 先画白色矩形, 再画黑字 = 结果不对。
                // 简化: 画一个方框高亮
                canvas_draw_box(canvas, 0, yy - 9, 128, 11);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_xbm(canvas, 4, yy - 9, ws[i], hs[i], bitmaps[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_xbm(canvas, 4, yy - 9, ws[i], hs[i], bitmaps[i]);
            }
        }
        // 底部提示
        canvas_draw_xbm(canvas, 2, 62 - HINT_MENU_H + 1, HINT_MENU_W, HINT_MENU_H, hint_menu_bits);
        return;
    }

    // 游戏画面: 先把渲染好的 framebuffer 拷到 canvas
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, SCREEN_W, SCREEN_H, g.fb);

    // HUD 信息: 关卡/层数/钥匙/火把/血
    int x = 1;
    if(g.mode == MODE_CAMPAIGN) {
        // 关 X
        canvas_draw_num(canvas, x, 9, g.level);
        canvas_draw_xbm(canvas, x + 7, 1, HUD_LV_W, HUD_LV_H, hud_lv_bits);
        x += 16;
    } else {
        // 层 X
        canvas_draw_num(canvas, x, 9, g.endless_floor);
        canvas_draw_xbm(canvas, x + 7, 1, HUD_FLOOR_W, HUD_FLOOR_H, hud_floor_bits);
        x += 16;
    }
    if(g.stage == STAGE_PUZZLE || g.stage == STAGE_COMBAT) {
        canvas_draw_num(canvas, x, 9, g.player.keys);
        canvas_draw_xbm(canvas, x + 6, 1, HUD_KEY_W, HUD_KEY_H, hud_key_bits);
        x += 13;
        canvas_draw_num(canvas, x, 9, g.player.torches);
        canvas_draw_xbm(canvas, x + 6, 1, HUD_TORCH_W, HUD_TORCH_H, hud_torch_bits);
        x += 13;
    }
    if(g.stage == STAGE_COMBAT) {
        canvas_draw_num(canvas, x, 9, g.player.health);
        canvas_draw_xbm(canvas, x + 6, 1, HUD_HP_W, HUD_HP_H, hud_hp_bits);
    }

    // 中文提示
    if(g.msg_id >= 0 && g.msg_ttl > 0) {
        const uint8_t* b; int w, h, bpr;
        get_msg_bmp(g.msg_id, &b, &w, &h, &bpr);
        if(b) canvas_draw_xbm(canvas, 2, 62 - h + 1, w, h, b);
    }

    // 覆盖层
    int cx, cy;
    if(g.mode == MODE_LEVEL_CLEAR) {
        // 白底面板
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 16, 16, 96, 34);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 16, 16, 96, 34);
        cx = 128/2 - OV_CLEAR_W/2; cy = 24 - OV_CLEAR_H/2;
        canvas_draw_xbm(canvas, cx, cy, OV_CLEAR_W, OV_CLEAR_H, ov_clear_bits);
        cx = 128/2 - OV_BTNS_W/2; cy = 44;
        canvas_draw_xbm(canvas, cx, cy, OV_BTNS_W, OV_BTNS_H, ov_btns_bits);
    } else if(g.mode == MODE_GAME_OVER) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 20, 18, 88, 32);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 20, 18, 88, 32);
        cx = 128/2 - OV_OVER_W/2; cy = 26;
        canvas_draw_xbm(canvas, cx, cy, OV_OVER_W, OV_OVER_H, ov_over_bits);
        cx = 128/2 - OV_BTNS2_W/2; cy = 42;
        canvas_draw_xbm(canvas, cx, cy, OV_BTNS2_W, OV_BTNS2_H, ov_btns2_bits);
    } else if(g.mode == MODE_PAUSED) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 32, 18, 64, 28);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 32, 18, 64, 28);
        cx = 128/2 - OV_PAUSED_W/2; cy = 26;
        canvas_draw_xbm(canvas, cx, cy, OV_PAUSED_W, OV_PAUSED_H, ov_paused_bits);
        cx = 128/2 - OV_BTNS3_W/2; cy = 40;
        canvas_draw_xbm(canvas, cx, cy, OV_BTNS3_W, OV_BTNS3_H, ov_btns3_bits);
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
                    else if(key == InputKeyOk) {
                        storage_load();
                        if(s_sel == M_CAMPAIGN) game_init_campaign(g.campaign_cleared + 1);
                        else if(s_sel == M_ENDLESS) game_init_endless(g.endless_floor, false);
                        else game_init_endless(g.endless_floor, true);
                    } else if(key == InputKeyBack) running = false;
                }
                did_input = true;
            } else if(g.mode == MODE_LEVEL_CLEAR || g.mode == MODE_GAME_OVER || g.mode == MODE_PAUSED) {
                if(type == InputTypeShort) handle_overlay_input(key);
                did_input = true;
            } else {
                // 游戏中
                if(key == InputKeyBack) {
                    if(type == InputTypeLong) running = false;
                    else if(type == InputTypeShort) {
                        s_resume_mode = g.mode;
                        g.mode = MODE_PAUSED;
                    }
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
        if(g.mode != MODE_MENU && need_render) {
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

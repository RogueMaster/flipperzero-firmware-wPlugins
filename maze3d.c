#include "maze3d.h"
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>

#define EVENT_INPUT 1

typedef struct {
    uint8_t type;
    InputEvent input;
} AppEvent;

// 菜单项
typedef enum {
    MENU_CAMPAIGN = 0,
    MENU_ENDLESS_RUN,
    MENU_ENDLESS_VISITOR,
    MENU_COUNT,
} MenuItem;

static const char* MENU_LABELS[MENU_COUNT] = {
    "1. Campaign",
    "2. Endless Run",
    "3. Visitor",
};

static int s_menu_sel = 0;

static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    if(g.mode == MODE_MENU) {
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 11, "Maze 3D");
        canvas_set_font(canvas, FontSecondary);
        for(int i = 0; i < MENU_COUNT; i++) {
            int y = 22 + i * 11;
            if(i == s_menu_sel) {
                canvas_draw_box(canvas, 0, y - 8, 128, 10);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 4, y, MENU_LABELS[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 4, y, MENU_LABELS[i]);
            }
        }
        canvas_draw_str(canvas, 2, 62, "Up/Dn select  OK start");
        return;
    }

    // 游戏画面: 绘制 framebuffer
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, SCREEN_W, SCREEN_H, g.fb);

    // HUD
    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    if(g.mode == MODE_CAMPAIGN) {
        snprintf(buf, sizeof(buf), "Lv%d", g.level);
    } else {
        snprintf(buf, sizeof(buf), "F%d", g.endless_floor);
    }
    canvas_draw_str(canvas, 1, 7, buf);

    if(g.stage == STAGE_PUZZLE || g.stage == STAGE_COMBAT) {
        snprintf(buf, sizeof(buf), "K%d T%d", g.player.keys, g.player.torches);
        canvas_draw_str(canvas, 28, 7, buf);
    }
    if(g.stage == STAGE_COMBAT) {
        snprintf(buf, sizeof(buf), "HP%d", g.player.health);
        canvas_draw_str(canvas, 60, 7, buf);
    }

    // 提示文本
    if(g.message_ttl > 0 && g.message[0]) {
        canvas_draw_str(canvas, 1, 62, g.message);
    }

    // 状态覆盖层
    if(g.mode == MODE_LEVEL_CLEAR) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 20, 18, 88, 28);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 20, 18, 88, 28);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 36, 32, "Level Clear!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 30, 42, "OK: next  Back:exit");
    } else if(g.mode == MODE_GAME_OVER) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 20, 18, 88, 28);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 20, 18, 88, 28);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 40, 32, "Game Over");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 34, 42, "OK: retry  Back:exit");
    } else if(g.mode == MODE_PAUSED) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 30, 20, 68, 24);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 30, 20, 68, 24);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 48, 36, "Paused");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 34, 62, "OK:resume Back:exit");
    }
}

static void input_callback(InputEvent* ev, void* ctx) {
    FuriMessageQueue* q = ctx;
    AppEvent e = {.type = EVENT_INPUT, .input = *ev};
    furi_message_queue_put(q, &e, FuriWaitForever);
}

// 在覆盖层状态下处理 OK/Back
static void handle_overlay_input(InputKey key) {
    if(g.mode == MODE_LEVEL_CLEAR) {
        if(key == InputKeyOk) { game_next_level(); }
        else if(key == InputKeyBack) { g.mode = MODE_MENU; }
    } else if(g.mode == MODE_GAME_OVER) {
        if(key == InputKeyOk) {
            if(g.mode == MODE_CAMPAIGN) game_init_campaign(g.level);
            else game_init_endless(g.endless_floor, false);
        } else if(key == InputKeyBack) { g.mode = MODE_MENU; }
    } else if(g.mode == MODE_PAUSED) {
        if(key == InputKeyOk) {
            // 恢复到对应游戏模式
            if(g.level >= 100) g.mode = MODE_ENDLESS_RUN; // 简化判断
            else g.mode = MODE_CAMPAIGN;
        } else if(key == InputKeyBack) { g.mode = MODE_MENU; }
    }
}

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

    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);

    AppEvent ev;
    bool running = true;
    while(running) {
        // 事件驱动 + 定时刷新(用于动画/提示衰减)
        FuriStatus st = furi_message_queue_get(q, &ev, 50);
        if(st == FuriStatusOk && ev.type == EVENT_INPUT) {
            InputKey key = ev.input.key;
            InputType type = ev.input.type;

            if(g.mode == MODE_MENU) {
                if(type == InputTypeShort) {
                    if(key == InputKeyUp) { s_menu_sel = (s_menu_sel + MENU_COUNT - 1) % MENU_COUNT; }
                    else if(key == InputKeyDown) { s_menu_sel = (s_menu_sel + 1) % MENU_COUNT; }
                    else if(key == InputKeyOk) {
                        storage_load();
                        if(s_menu_sel == MENU_CAMPAIGN) game_init_campaign(g.campaign_cleared + 1);
                        else if(s_menu_sel == MENU_ENDLESS_RUN) game_init_endless(g.endless_floor, false);
                        else game_init_endless(g.endless_floor, true);
                    }
                    else if(key == InputKeyBack) { running = false; }
                }
            } else if(g.mode == MODE_LEVEL_CLEAR || g.mode == MODE_GAME_OVER || g.mode == MODE_PAUSED) {
                if(type == InputTypeShort) handle_overlay_input(key);
            } else {
                // 游戏中: Back 长按退出, 短按暂停
                if(key == InputKeyBack && type == InputTypeLong) {
                    running = false;
                } else if(key == InputKeyBack && type == InputTypeShort) {
                    g.mode = MODE_PAUSED;
                } else {
                    game_handle_input(key, type);
                }
            }
        }

        // 定时更新游戏(敌人移动等)
        if(g.mode == MODE_CAMPAIGN || g.mode == MODE_ENDLESS_RUN || g.mode == MODE_ENDLESS_VISITOR) {
            game_update();
            if(g.mode == MODE_GAME_OVER) {
                notification_message(notif, &sequence_error);
            }
        }

        // 渲染
        if(g.mode != MODE_MENU) {
            engine_render();
        }
        view_port_update(vp);
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_message_queue_free(q);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    return 0;
}

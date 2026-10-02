#include "maze3d.h"
#include "i18n.h"
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

#define EVENT_INPUT 1

typedef struct {
    uint8_t type;
    InputEvent input;
} AppEvent;

typedef enum {
    M_PLAY = 0,
    M_LANG = 1,
    M_ABOUT = 2,
    M_COUNT = 3,
} MenuItem;

static int s_sel = 0;

static void draw_menu(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 40, 12, EN_TITLE);

    const char* items[M_COUNT] = {EN_M_PLAY, EN_M_LANG, EN_M_ABOUT};
    for(int i = 0; i < M_COUNT; i++) {
        int y = 26 + i * 12;
        if(i == s_sel) {
            canvas_draw_box(canvas, 10, y - 9, 108, 11);
            canvas_invert_color(canvas);
            canvas_draw_str(canvas, 16, y, items[i]);
            canvas_invert_color(canvas);
        } else {
            canvas_draw_str(canvas, 16, y, items[i]);
        }
    }

    {
        int y = 26 + M_LANG * 12;
        const char* tag = (g.lang == LANG_EN) ? EN_LANG_TAG : EN_ZH_TAG;
        canvas_draw_str(canvas, 100, y, tag);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 62, EN_HINT_MENU);
}

static void draw_about(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 40, 14, EN_ABOUT_TITLE);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 20, 30, EN_ABOUT_LINE1);
    canvas_draw_str(canvas, 20, 42, EN_ABOUT_LINE2);
    canvas_draw_str(canvas, 20, 56, EN_ABOUT_BTNS);
}

static void draw_clear(Canvas* canvas) {
    canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 44, 26, EN_CLEAR);
    char lv[16];
    snprintf(lv, sizeof(lv), "Level %d", g.level);
    canvas_draw_str(canvas, 40, 40, lv);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 14, 58, EN_CLEAR_BTNS);
    canvas_invert_color(canvas);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);

    if(g.mode == MODE_MENU) {
        draw_menu(canvas);
    } else if(g.mode == MODE_ABOUT) {
        draw_about(canvas);
    } else if(g.mode == MODE_PLAY || g.mode == MODE_CLEAR) {
        // Draw the pre-computed raycast scene directly with canvas primitives.
        engine_draw(canvas);

        // HUD overlay
        if(g.show_hud || (g.tick & 63) < 20) {
            canvas_draw_box(canvas, 0, 0, SCREEN_W, 10);
            canvas_invert_color(canvas);
            canvas_set_font(canvas, FontSecondary);
            char buf[32];
            snprintf(buf, sizeof(buf), "%s %d", EN_HUD_LV, g.level);
            canvas_draw_str(canvas, 4, 8, buf);
            canvas_invert_color(canvas);
        }
        if(g.show_hud) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 62, EN_HUD_HINT);
        }
        if(g.mode == MODE_CLEAR) {
            draw_clear(canvas);
        }
    }
}

static void input_callback(InputEvent* ev, void* ctx) {
    FuriMessageQueue* q = ctx;
    AppEvent e = {.type = EVENT_INPUT, .input = *ev};
    furi_message_queue_put(q, &e, FuriWaitForever);
}

static void handle_menu_input(InputKey key, InputType type) {
    if(type != InputTypeShort) return;
    if(key == InputKeyUp) {
        s_sel = (s_sel + M_COUNT - 1) % M_COUNT;
        sfx_play(SFX_MENU_MOVE);
    } else if(key == InputKeyDown) {
        s_sel = (s_sel + 1) % M_COUNT;
        sfx_play(SFX_MENU_MOVE);
    } else if(key == InputKeyLeft || key == InputKeyRight) {
        g.lang = (g.lang == LANG_EN) ? LANG_ZH : LANG_EN;
        storage_save();
        sfx_play(SFX_MENU_MOVE);
    } else if(key == InputKeyOk) {
        sfx_play(SFX_MENU_OK);
        if(s_sel == M_PLAY) {
            game_init_level(1);
        } else if(s_sel == M_ABOUT) {
            g.mode = MODE_ABOUT;
        }
    }
    g.dirty = true;
}

static void handle_overlay_input(InputKey key, InputType type) {
    if(type != InputTypeShort) return;
    if(g.mode == MODE_ABOUT) {
        if(key == InputKeyBack || key == InputKeyOk) {
            g.mode = MODE_MENU;
            sfx_play(SFX_MENU_OK);
        }
    } else if(g.mode == MODE_CLEAR) {
        if(key == InputKeyOk) {
            game_next_level();
        } else if(key == InputKeyBack) {
            g.mode = MODE_MENU;
            sfx_play(SFX_MENU_OK);
        }
    }
    g.dirty = true;
}

int32_t maze3d_app(void* p) {
    UNUSED(p);
    memset(&g, 0, sizeof(g));
    g.lang = LANG_EN;
    storage_load();

    sfx_init();
    g.mode = MODE_MENU;

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(AppEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_callback, NULL);
    view_port_input_callback_set(vp, input_callback, q);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    AppEvent ev;
    bool running = true;
    const uint32_t UPDATE_MS = 100;

    while(running) {
        FuriStatus st = furi_message_queue_get(q, &ev, UPDATE_MS);
        if(st == FuriStatusOk && ev.type == EVENT_INPUT) {
            InputKey key = ev.input.key;
            InputType type = ev.input.type;

            if(key == InputKeyBack && type == InputTypeLong) {
                if(g.mode == MODE_MENU) {
                    running = false;
                } else {
                    g.mode = MODE_MENU;
                    g.dirty = true;
                    sfx_play(SFX_MENU_OK);
                }
                continue;
            }

            if(g.mode == MODE_MENU) {
                handle_menu_input(key, type);
            } else if(g.mode == MODE_PLAY) {
                game_handle_input(key, type);
            } else {
                handle_overlay_input(key, type);
            }
        }

        if(g.mode == MODE_PLAY) {
            game_update();
            engine_compute();
        }

        if(g.dirty || g.mode == MODE_PLAY) {
            view_port_update(vp);
            g.dirty = false;
        }
        sfx_tick_update();
    }

    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);
    furi_message_queue_free(q);
    sfx_deinit();
    return 0;
}

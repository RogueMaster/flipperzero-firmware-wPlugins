#include "include/ui/play_screen.h"
#include "include/domain/avocado_rules.h"
#include "include/platform/feedback_helper.h"
#include "include/platform/storage_helper.h"
#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>

/* 1 = dev shortcuts (long OK, etc.). Default 0 for release; override with -DAVOCADO_QA_MODE=1 */
#ifndef AVOCADO_QA_MODE
#define AVOCADO_QA_MODE 0
#endif

struct PlayScreen {
    View *view;
    AvocadoData *data;
    AvocadoFeedback *feedback;
#if AVOCADO_QA_MODE
    bool qa_mode;
#endif
};

typedef struct {
    PlayScreen *screen;
} PlayViewModel;

enum {
    CupCx = 64,
    CupY = 22,
    CupH = 32,
    CupTopY = CupY + 2,
    CupBotY = CupY + CupH - 2,
    CupTopHalfW = 11,
    CupBotHalfW = 9,
};

static void play_on_action(PlayScreen *screen);

static void glass_horizontal_at(int cx, int top_y, int bot_y, int top_half_w, int bot_half_w, int y,
                                int *out_l, int *out_r) {
    const int tl = cx - top_half_w;
    const int tr = cx + top_half_w;
    const int bl = cx - bot_half_w;
    const int br = cx + bot_half_w;
    if (y <= top_y) {
        *out_l = tl;
        *out_r = tr;
        return;
    }
    if (y >= bot_y) {
        *out_l = bl;
        *out_r = br;
        return;
    }
    const int dy = bot_y - top_y;
    const int n = y - top_y;
    *out_l = tl + (bl - tl) * n / dy;
    *out_r = tr + (br - tr) * n / dy;
}

static void cup_horizontal_at(int y, int *out_l, int *out_r) {
    glass_horizontal_at(CupCx, CupTopY, CupBotY, CupTopHalfW, CupBotHalfW, y, out_l, out_r);
}

static void draw_water_dither_full_cup(Canvas *canvas, uint8_t dirty_level) {
    canvas_set_color(canvas, ColorBlack);
    int l = 0;
    int r = 0;
    const unsigned murk = (unsigned)dirty_level * 128u / (unsigned)AVOCADO_GRIME_GAME_OVER;
    for (int y = CupTopY + 1; y < CupBotY; y++) {
        cup_horizontal_at(y, &l, &r);
        for (int x = l + 1; x < r; x++) {
            const int checker = (x + y) & 1;
            const unsigned h = (unsigned)((x * 31 + y * 17) & 127);
            if (checker == 0 || murk > h) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

static void draw_water_dither(Canvas *canvas, int y_surface) {
    if (y_surface >= CupBotY) {
        return;
    }
    canvas_set_color(canvas, ColorBlack);
    int l = 0;
    int r = 0;
    cup_horizontal_at(y_surface, &l, &r);
    canvas_draw_line(canvas, l, y_surface, r, y_surface);
    for (int y = y_surface + 1; y < CupBotY; y++) {
        cup_horizontal_at(y, &l, &r);
        for (int x = l + 1; x < r; x++) {
            if (((x + y) & 1) == 0) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

/** Half-width per scanline (dy = row - 9), pear-like pit; authored for base_r = 9. */
static const uint8_t k_pit_half_w[19] = {2, 3, 4, 5, 6, 7, 8, 8, 8, 9, 8, 8, 8, 7, 7, 6, 5, 4, 3};

static void draw_pit(Canvas *canvas, int cx, int cy, size_t radius, bool cracked) {
    const int scale = (int)radius;
    const int base_r = 9;

    canvas_set_color(canvas, ColorBlack);
    for (int i = 0; i < 19; i++) {
        const int dy = i - 9;
        int hw = (int)k_pit_half_w[i] * scale / base_r;
        if (hw < 1) {
            hw = 1;
        }
        const int y = cy + dy;
        canvas_draw_line(canvas, cx - hw, y, cx + hw, y);
    }

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, cx - 4, cy - 5);
    canvas_set_color(canvas, ColorBlack);

    if (cracked) {
        canvas_draw_line(canvas, cx - scale + 1, cy - 3, cx + scale - 1, cy + 4);
    }
}

static void draw_pit_toothpicks_on_rim(Canvas *canvas, int cx, int cy, size_t radius) {
    int rim_l = 0;
    int rim_r = 0;
    cup_horizontal_at(CupTopY, &rim_l, &rim_r);
    const int dy_rim = CupTopY - cy;
    if (dy_rim < -9 || dy_rim > 9) {
        return;
    }
    const int base_r = 9;
    const int scale = (int)radius;
    int hw_c = (int)k_pit_half_w[9] * scale / base_r;
    if (hw_c < 1) {
        hw_c = 1;
    }
    const int stick_past_rim = 15;
    canvas_set_color(canvas, ColorBlack);
    /* Pit to rim corner, then long horizontal stub outside the glass. */
    canvas_draw_line(canvas, cx - hw_c, cy, rim_l, CupTopY);
    canvas_draw_line(canvas, rim_l, CupTopY, rim_l - stick_past_rim, CupTopY);
    canvas_draw_line(canvas, cx + hw_c, cy, rim_r, CupTopY);
    canvas_draw_line(canvas, rim_r, CupTopY, rim_r + stick_past_rim, CupTopY);
}

static void draw_roots(Canvas *canvas, int cx, int y_start, uint8_t roots) {
    if (roots == 0) {
        return;
    }
    const int jar_bottom = CupBotY;
    canvas_set_color(canvas, ColorBlack);
    const int tap_len = 5 + (int)roots * 2;
    int y_tip = y_start + tap_len;
    if (y_tip > jar_bottom) {
        y_tip = jar_bottom;
    }
    canvas_draw_line(canvas, cx, y_start, cx, y_tip);

    if (roots == 1u) {
        canvas_draw_line(canvas, cx - 3, y_start + 2, cx, y_start + 5);
        canvas_draw_line(canvas, cx + 3, y_start + 2, cx, y_start + 5);
        return;
    }

    for (uint8_t n = 0; n < roots; n++) {
        const int dx = -10 + (20 * (int)n) / ((int)roots - 1);
        const int y_mid = y_start + 3 + (int)n * 2;
        if (y_mid > jar_bottom - 2) {
            break;
        }
        canvas_draw_line(canvas, cx, y_start + 1, cx + dx / 2, y_mid);
        canvas_draw_line(canvas, cx + dx / 2, y_mid, cx + dx, y_mid + 4);
    }
}

static void draw_grime(Canvas *canvas, uint8_t grime) {
    if (grime == 0u) {
        return;
    }
    canvas_set_color(canvas, ColorBlack);
    const int y_span = CupBotY - CupTopY - 10;
    if (y_span < 4) {
        return;
    }
    unsigned n = (unsigned)grime * 2u + 4u;
    if (n > 28u) {
        n = 28u;
    }
    for (unsigned i = 0; i < n; i++) {
        const int y = CupTopY + 6 + (int)((i * 13u + (unsigned)grime * 3u) % (unsigned)y_span);
        int l = 0;
        int r = 0;
        cup_horizontal_at(y, &l, &r);
        const int inner = r - l - 8;
        if (inner <= 1) {
            continue;
        }
        const int x = l + 4 + (int)((i * 17u + (unsigned)grime) % (unsigned)inner);
        canvas_draw_dot(canvas, x, y);
        if (i > 5u && grime > 5u) {
            canvas_draw_dot(canvas, x + 1, y);
        }
    }
}

static void draw_cup_outline(Canvas *canvas) {
    const int top_l = CupCx - CupTopHalfW;
    const int top_r = CupCx + CupTopHalfW;
    const int bot_l = CupCx - CupBotHalfW;
    const int bot_r = CupCx + CupBotHalfW;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, top_l, CupTopY, top_r, CupTopY);
    canvas_draw_line(canvas, top_l, CupTopY, bot_l, CupBotY);
    canvas_draw_line(canvas, top_r, CupTopY, bot_r, CupBotY);
    canvas_draw_line(canvas, bot_l, CupBotY, bot_r, CupBotY);
}

/**
 * Avocado half viewed from the front: the flat cut faces you (oval slice), dark flesh/skin with a
 * light oval pit cavity in the middle.
 */
static void draw_half_avocado_silhouette(Canvas *canvas, int cx, int cy) {
    static const uint8_t k_slice_hw[31] = {5,  7,  9,  10, 11, 12, 13, 14, 15, 15, 16,
                                           16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15,
                                           15, 14, 13, 12, 11, 10, 9,  7,  5};
    static const uint8_t k_pit_front_hw[21] = {3, 4, 5, 6, 7, 7, 8, 8, 9, 9, 9,
                                               9, 9, 8, 8, 7, 7, 6, 5, 4, 3};

    canvas_set_color(canvas, ColorBlack);
    for (int i = 0; i < 31; i++) {
        const int y = cy - 15 + i;
        const int hw = (int)k_slice_hw[i];
        canvas_draw_line(canvas, cx - hw, y, cx + hw, y);
    }

    canvas_set_color(canvas, ColorWhite);
    for (int i = 0; i < 21; i++) {
        const int y = cy - 10 + i;
        const int hw = (int)k_pit_front_hw[i];
        canvas_draw_line(canvas, cx - hw, y, cx + hw, y);
    }

    canvas_set_color(canvas, ColorBlack);
    for (int i = 0; i < 21; i++) {
        const int y = cy - 10 + i;
        const int hw = (int)k_pit_front_hw[i];
        canvas_draw_dot(canvas, cx - hw, y);
        canvas_draw_dot(canvas, cx + hw, y);
    }
}

static void draw_victory_screen(Canvas *canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "Victory!");
    draw_half_avocado_silhouette(canvas, 64, 36);
    canvas_set_font(canvas, FontSecondary);
    elements_button_right(canvas, "OK");
}

static void play_draw_callback(Canvas *canvas, void *model) {
    furi_assert(model);
    const PlayViewModel *vm = model;
    PlayScreen *screen = vm->screen;
    const AvocadoData *d = screen->data;

    canvas_clear(canvas);

#if AVOCADO_QA_MODE
    if (screen->qa_mode) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, 2, AlignRight, AlignTop, "QA");
    }
#endif

    if (avocado_rules_should_show_victory(d)) {
        draw_victory_screen(canvas);
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    const bool game_over = avocado_rules_is_game_over(d);
    const int cx = 64;
    const size_t pit_r = game_over ? 8u : 9u;
    const int pit_cy = game_over ? (CupY + 22) : (CupY - 1);
    const int y_drought_surface = CupY + CupH - 9;

    if (game_over) {
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "GAME OVER");
    } else {
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Avocado pit");
    }

    if (game_over) {
        draw_water_dither(canvas, y_drought_surface);
    } else {
        draw_water_dither_full_cup(canvas, d->dirty_level);
    }

    draw_pit(canvas, cx, pit_cy, pit_r, game_over);

    if (!game_over) {
        draw_roots(canvas, cx, pit_cy + (int)pit_r, d->roots_length);
        draw_grime(canvas, d->dirty_level);
    }

    draw_cup_outline(canvas);

    if (!game_over) {
        draw_pit_toothpicks_on_rim(canvas, cx, pit_cy, pit_r);
    }

    canvas_set_font(canvas, FontSecondary);
    if (game_over) {
        elements_button_right(canvas, "Start");
    } else {
        elements_button_right(canvas, "Clean");
    }
}

// cppcheck-suppress constParameterCallback
static bool play_input_callback(InputEvent *event, void *context) {
    PlayScreen *screen = context;

#if AVOCADO_QA_MODE
    if (event->type == InputTypeLong && event->key == InputKeyOk) {
        screen->qa_mode = !screen->qa_mode;
        view_commit_model(screen->view, true);
        return true;
    }

    if (screen->qa_mode) {
        if (event->type == InputTypeLong && event->key == InputKeyLeft) {
            avocado_rules_debug_preset_victory_pending(screen->data);
            avocado_data_save(screen->data);
            view_commit_model(screen->view, true);
            return true;
        }
        if (event->type == InputTypeLong && event->key == InputKeyRight) {
            avocado_rules_debug_preset_game_over(screen->data);
            avocado_data_save(screen->data);
            view_commit_model(screen->view, true);
            return true;
        }
        if (event->type == InputTypeShort) {
            if (event->key == InputKeyUp) {
                avocado_rules_debug_add_simulated_days(screen->data, 1);
                avocado_data_save(screen->data);
                view_commit_model(screen->view, true);
                return true;
            }
            if (event->key == InputKeyDown) {
                avocado_rules_debug_add_simulated_days(screen->data, 7);
                avocado_data_save(screen->data);
                view_commit_model(screen->view, true);
                return true;
            }
            if (event->key == InputKeyLeft) {
                avocado_rules_debug_bump_roots(screen->data);
                avocado_data_save(screen->data);
                view_commit_model(screen->view, true);
                return true;
            }
        }
    }
#endif

    if (event->type != InputTypeShort) {
        return false;
    }
    if (event->key == InputKeyOk || event->key == InputKeyRight) {
        play_on_action(screen);
        return true;
    }
    return false;
}

static void play_on_action(PlayScreen *screen) {
    if (avocado_rules_should_show_victory(screen->data)) {
        avocado_rules_acknowledge_victory(screen->data, furi_hal_rtc_get_timestamp());
        avocado_data_save(screen->data);
        if (screen->feedback) {
            avocado_feedback_play(screen->feedback, false);
        }
        view_commit_model(screen->view, true);
        return;
    }

    const bool was_game_over = avocado_rules_is_game_over(screen->data);
    avocado_rules_on_primary_action(screen->data);
    avocado_data_save(screen->data);

    if (screen->feedback) {
        avocado_feedback_play(screen->feedback, was_game_over);
    }
    view_commit_model(screen->view, true);
}

PlayScreen *play_screen_alloc(AvocadoData *data, AvocadoFeedback *feedback) {
    furi_check(data);

    PlayScreen *screen = malloc(sizeof(PlayScreen));
    if (!screen) {
        return NULL;
    }
    screen->view = view_alloc();
    if (!screen->view) {
        free(screen);
        return NULL;
    }
    screen->data = data;
    screen->feedback = feedback;
#if AVOCADO_QA_MODE
    screen->qa_mode = false;
#endif

    view_allocate_model(screen->view, ViewModelTypeLockFree, sizeof(PlayViewModel));
    PlayViewModel *vm = view_get_model(screen->view);
    vm->screen = screen;
    view_commit_model(screen->view, false);

    view_set_context(screen->view, screen);
    view_set_draw_callback(screen->view, play_draw_callback);
    view_set_input_callback(screen->view, play_input_callback);

    return screen;
}

void play_screen_free(PlayScreen *screen) {
    if (!screen) {
        return;
    }
    view_free(screen->view);
    free(screen);
}

View *play_screen_get_view(PlayScreen *screen) {
    furi_assert(screen);
    return screen->view;
}

#include "ui_i.h"

#include <furi_hal.h>
#include <gui/view.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FREQ_DIGITS   6
#define FREQ_CELL_W   18
#define FREQ_CELL_H   20
#define FREQ_CELL_GAP 1
#define FREQ_CELL_Y   11

typedef struct {
    FlipperHamApp* app;
} FreqInputModel;

static uint32_t freq_min_hz(void);
static uint32_t freq_max_hz(void);
static bool freq_band(uint32_t a, uint32_t* lo, uint32_t* hi);
static uint32_t freq_next_band(uint32_t a);
static void freq_input_draw(Canvas* canvas, void* model);
static bool freq_input_do(InputEvent* event, void* context);
static void freq_input_commit(FlipperHamApp* app);
static void freq_input_digit(Canvas* canvas, int32_t x, bool focus, char d);
static void freq_input_text(char* o, uint16_t n, uint32_t hz);
static void freq_input_bump_focus(FlipperHamApp* app, int8_t d);
static void freq_input_bump_digit(FlipperHamApp* app, int8_t d);
static uint32_t freq_input_place(uint8_t focus);

static uint32_t freq_input_old_hz;

uint32_t freq_step(uint32_t a, int8_t d) {
    uint32_t b;
    uint32_t c;
    uint32_t e;

    b = a;
    if(!freq_band(b, &c, &e)) {
        c = freq_min_hz();
        e = freq_max_hz();
        if(b < c) b = c;
        if(b > e) b = e;
        if(!freq_band(b, &c, &e)) return b;
    }

    if(d > 0) {
        if(b >= e) return e;
        b += 2500UL;
        if(b > e) b = e;
    } else {
        if(b <= c) return c;
        b -= 2500UL;
        if(b < c) b = c;
    }

    return b;
}

static uint32_t freq_min_hz(void) {
    uint32_t a;

    for(a = 0; a < 1000000000UL; a += 2500UL)
        if(freq_tx_allowed_hz(a)) return a;

    return CARRIER_HZ;
}

static uint32_t freq_max_hz(void) {
    uint32_t a;

    for(a = 1000000000UL - 1; a > 2500UL; a -= 2500UL)
        if(freq_tx_allowed_hz(a)) return a;

    return CARRIER_HZ;
}

static bool freq_band(uint32_t a, uint32_t* lo, uint32_t* hi) {
    uint32_t b;

    if(!freq_tx_allowed_hz(a)) return false;

    b = a;
    while(b >= 2500UL) {
        if(!freq_tx_allowed_hz(b - 2500UL)) break;
        b -= 2500UL;
    }
    *lo = b;

    b = a;
    while(b <= 1000000000UL - 2500UL) {
        if(!freq_tx_allowed_hz(b + 2500UL)) break;
        b += 2500UL;
    }
    *hi = b;

    return true;
}

static uint32_t freq_next_band(uint32_t a) {
    uint32_t c;
    uint32_t e;
    uint32_t b;
    uint32_t message_pick;

    message_pick = freq_max_hz();
    if(!freq_band(a, &c, &e)) return freq_min_hz();

    b = e;
    while(b <= message_pick - 2500UL) {
        b += 2500UL;
        if(freq_tx_allowed_hz(b)) return b;
    }

    return freq_min_hz();
}

void freq_show(char* o, uint16_t n, uint32_t a) {
    if(!o) return;
    if(!n) return;

    snprintf(
        o, n, "%06lu.%1lu", (unsigned long)(a / 1000UL), (unsigned long)((a % 1000UL) / 100UL));
}

void fsh2(char* o, uint16_t n, uint32_t a) {
    if(!o) return;
    if(!n) return;

    snprintf(o, n, "%06lu", (unsigned long)(a / 1000UL));
}

static bool freq_parse(const char* s, uint32_t* out) {
    char* e;
    uint32_t a;
    uint32_t b;

    if(!s) return false;
    if(!out) return false;
    if(!s[0]) return false;

    a = strtoul(s, &e, 10);
    if(e == s) return false;
    if(*e) return false;

    b = a;
    if(freq_tx_allowed_hz(b)) {
        *out = b;
        return true;
    }

    if(a <= 1000000UL) {
        b = a * 1000UL;
        if(freq_tx_allowed_hz(b)) {
            *out = b;
            return true;
        }
    }

    if(a <= 1000UL) {
        b = a * 1000000UL;
        if(freq_tx_allowed_hz(b)) {
            *out = b;
            return true;
        }
    }

    return false;
}

void freq_menu_build(FlipperHamApp* app) {
    uint8_t i;

    submenu_reset(app->freq_menu);

    if(app->freq_n < FREQ_N)
        submenu_add_item_ex(app->freq_menu, "Add new...", FlipperHamFreqIndexAdd, freq_pick, app);

    for(i = 0; i < FREQ_N; i++) {
        if(!app->freq_used[i]) continue;
        if(app->tx_freq_index == i)
            snprintf(
                app->freq_s[i],
                sizeof(app->freq_s[i]),
                "%06lu.%1lu *",
                (unsigned long)(app->freq[i] / 1000UL),
                (unsigned long)((app->freq[i] % 1000UL) / 100UL));
        else
            freq_show(app->freq_s[i], sizeof(app->freq_s[i]), app->freq[i]);
        submenu_add_item_ex(
            app->freq_menu, app->freq_s[i], FlipperHamFreqIndexBase + i, freq_pick, app);
    }

    if(app->freq_n < FREQ_N)
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexAdd);
    else
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexBase + app->tx_freq_index);

    if(app->freq_sel >= FlipperHamFreqIndexBase) {
        i = app->freq_sel - FlipperHamFreqIndexBase;
        if(i < FREQ_N)
            if(app->freq_used[i]) submenu_set_selected_item(app->freq_menu, app->freq_sel);
    }
}

void freq_edit_menu_build(FlipperHamApp* app) {
    VariableItem* it;

    variable_item_list_reset(app->freq_edit_menu);

    it = variable_item_list_add(app->freq_edit_menu, "Frequency", 201, freq_change, app);
    variable_item_set_current_value_index(it, 100);
    fsh2(app->f_edit, sizeof(app->f_edit), app->freq_edit_hz);
    variable_item_set_current_value_text(it, app->f_edit);

    it = variable_item_list_add(app->freq_edit_menu, "Enter frequency", 1, NULL, NULL);
    variable_item_set_current_value_index(it, 0);
    variable_item_set_current_value_text(it, app->f_bad ? "bad" : "");

    variable_item_list_add(app->freq_edit_menu, "Save", 1, NULL, NULL);
    variable_item_list_add(app->freq_edit_menu, "Use this for TX", 1, NULL, NULL);

    if(app->freq_n > 1)
        if(app->freq_index < FREQ_N)
            if(app->freq_used[app->freq_index])
                variable_item_list_add(app->freq_edit_menu, "Delete", 1, NULL, NULL);
    variable_item_list_set_selected_item(app->freq_edit_menu, 0);
}

void freq_edit_enter(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    bool a;

    a = false;
    if(app->freq_n > 1)
        if(app->freq_index < FREQ_N)
            if(app->freq_used[app->freq_index]) a = true;

    if(index == 0) {
        app->freq_sel = FlipperHamFreqIndexBase + app->freq_index;
        app->freq_edit_hz = freq_next_band(app->freq_edit_hz);
        app->f_bad = !freq_tx_allowed_hz(app->freq_edit_hz);
        freq_edit_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
        return;
    }

    if(index == 1) {
        app->freq_sel = FlipperHamFreqIndexBase + app->freq_index;
        freq_input_start(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqInput);
        return;
    }

    if(index == 2) {
        if(app->f_bad) return;
        if(app->freq_index >= FREQ_N) return;
        if(!freq_tx_allowed_hz(app->freq_edit_hz)) return;

        app->freq_sel = FlipperHamFreqIndexBase + app->freq_index;
        app->freq[app->freq_index] = app->freq_edit_hz;
        app->freq_used[app->freq_index] = 1;
        freq_fix(app);
        cfgsave(app);
        freq_menu_build(app);
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexBase + app->freq_index);
        settings_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreq);
        return;
    }

    if(index == 3) {
        if(app->f_bad) return;
        if(app->freq_index >= FREQ_N) return;
        if(!freq_tx_allowed_hz(app->freq_edit_hz)) return;

        app->freq_sel = FlipperHamFreqIndexBase + app->freq_index;
        app->freq[app->freq_index] = app->freq_edit_hz;
        app->freq_used[app->freq_index] = 1;
        app->tx_freq_index = app->freq_index;
        freq_fix(app);
        cfgsave(app);
        freq_menu_build(app);
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexBase + app->freq_index);
        settings_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreq);
        return;
    }

    if(a && index == 4) {
        app->freq_sel = FlipperHamFreqIndexAdd;
        app->freq[app->freq_index] = 0;
        app->freq_used[app->freq_index] = 0;
        freq_fix(app);
        cfgsave(app);
        freq_menu_build(app);
        settings_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreq);
        return;
    }
}

void freq_pick(void* context, InputType input_type, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t a;

    if(index == FlipperHamFreqIndexAdd) {
        app->freq_sel = FlipperHamFreqIndexAdd;
        app->freq_index = 0xff;
        app->f_bad = false;

        for(a = 0; a < FREQ_N; a++)
            if(!app->freq_used[a]) {
                app->freq_index = a;
                break;
            }

        if(app->freq_index == 0xff) return;

        app->freq_edit_hz = tx_freq_get(app);
        app->f_bad = !freq_tx_allowed_hz(app->freq_edit_hz);
        freq_edit_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
        return;
    }

    a = index - FlipperHamFreqIndexBase;
    if(a >= FREQ_N) return;
    if(!app->freq_used[a]) return;

    app->freq_sel = index;

    if(input_type == InputTypeLong) {
        app->tx_freq_index = a;
        app->f_bad = false;
        cfgsave(app);
        freq_menu_build(app);
        settings_menu_build(app);
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexBase + a);
        return;
    }

    if(input_type != InputTypeShort) return;

    app->freq_index = a;
    app->freq_edit_hz = app->freq[a];
    app->f_bad = false;
    freq_edit_menu_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
}

void freq_save(void* context) {
    FlipperHamApp* app = context;
    uint32_t a;

    if(freq_parse(app->f_edit, &a)) {
        app->freq_edit_hz = a;
        app->f_bad = false;
    } else
        app->f_bad = true;

    freq_edit_menu_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
}

static void freq_input_text(char* o, uint16_t n, uint32_t hz) {
    if(!o) return;
    if(!n) return;

    snprintf(o, n, "%06lu", (unsigned long)((hz / 1000UL) % 1000000UL));
}

static void freq_input_digit(Canvas* canvas, int32_t x, bool focus, char d) {
    char s[2];

    if(!canvas) return;

    s[0] = d;
    s[1] = 0;

    canvas_set_color(canvas, ColorBlack);
    if(focus) {
        canvas_draw_triangle(
            canvas, x + FREQ_CELL_W / 2, FREQ_CELL_Y - 2, 5, 3, CanvasDirectionBottomToTop);
        canvas_draw_triangle(
            canvas,
            x + FREQ_CELL_W / 2,
            FREQ_CELL_Y + FREQ_CELL_H + 2,
            5,
            3,
            CanvasDirectionTopToBottom);
        canvas_draw_rbox(canvas, x, FREQ_CELL_Y, FREQ_CELL_W, FREQ_CELL_H, 1);
        canvas_set_color(canvas, ColorWhite);
    } else
        canvas_draw_rframe(canvas, x, FREQ_CELL_Y, FREQ_CELL_W, FREQ_CELL_H, 1);

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas, x + FREQ_CELL_W / 2, FREQ_CELL_Y + FREQ_CELL_H / 2, AlignCenter, AlignCenter, s);
    canvas_set_color(canvas, ColorBlack);
}

static void freq_input_draw(Canvas* canvas, void* model) {
    FreqInputModel* m = model;
    FlipperHamApp* app;
    char s[FREQ_DIGITS + 1];
    const uint8_t total = (FREQ_CELL_W * FREQ_DIGITS) + (FREQ_CELL_GAP * (FREQ_DIGITS - 1));
    const uint8_t x0 = (128 - total) / 2;
    uint8_t i;

    if(!m) return;
    app = m->app;
    if(!app) return;

    canvas_clear(canvas);
    freq_input_text(s, sizeof(s), app->freq_edit_hz);
    for(i = 0; i < FREQ_DIGITS; i++)
        freq_input_digit(
            canvas,
            x0 + ((FREQ_CELL_W + FREQ_CELL_GAP) * i),
            i == (app->freq_focus % FREQ_DIGITS),
            s[i]);

    canvas_set_font(canvas, FontSecondary);
    if(!freq_vfo_valid_hz(app->freq_edit_hz)) {
        canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignCenter, "TX unavailable");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "PLL lock failed");
        return;
    }

    canvas_draw_str_aligned(
        canvas,
        64,
        52,
        AlignCenter,
        AlignCenter,
        freq_tx_allowed_hz(app->freq_edit_hz) ? "TX allowed" : "TX unavailable");
}

static uint32_t freq_input_place(uint8_t focus) {
    static const uint32_t p[FREQ_DIGITS] = {100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL};

    if(focus >= FREQ_DIGITS) return 1UL;

    return p[focus];
}

static void freq_input_bump_focus(FlipperHamApp* app, int8_t d) {
    uint8_t f;

    if(!app) return;

    f = app->freq_focus % FREQ_DIGITS;
    if(d < 0)
        app->freq_focus = f ? f - 1 : FREQ_DIGITS - 1;
    else
        app->freq_focus = (f + 1) % FREQ_DIGITS;
}

static void freq_input_bump_digit(FlipperHamApp* app, int8_t d) {
    uint32_t p;
    uint32_t khz;
    uint8_t old;
    uint8_t now;
    int32_t delta;

    if(!app) return;

    p = freq_input_place(app->freq_focus);
    khz = (app->freq_edit_hz / 1000UL) % 1000000UL;
    old = (uint8_t)((khz / p) % 10UL);
    now = d < 0 ? (uint8_t)((old + 9) % 10) : (uint8_t)((old + 1) % 10);
    delta = ((int32_t)now - (int32_t)old) * (int32_t)p;
    app->freq_edit_hz = (uint32_t)((int32_t)khz + delta) * 1000UL;
}

static void freq_input_commit(FlipperHamApp* app) {
    uint32_t hz;

    if(!app) return;

    hz = ((app->freq_edit_hz / 1000UL) % 1000000UL) * 1000UL;
    if(!freq_vfo_valid_hz(hz))
        app->freq_edit_hz = freq_input_old_hz;
    else if(!freq_tx_allowed_hz(hz))
        app->freq_edit_hz = freq_default_hz();
    else
        app->freq_edit_hz = hz;

    app->f_bad = !freq_tx_allowed_hz(app->freq_edit_hz);
    freq_edit_menu_build(app);
}

static bool freq_input_do(InputEvent* event, void* context) {
    FlipperHamApp* app = context;
    FreqInputModel* m;

    if(!event || !app) return false;

    if((event->key == InputKeyBack || event->key == InputKeyOk) &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        freq_input_commit(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
        return true;
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return true;

    if(event->key == InputKeyLeft)
        freq_input_bump_focus(app, -1);
    else if(event->key == InputKeyRight)
        freq_input_bump_focus(app, 1);
    else if(event->key == InputKeyUp)
        freq_input_bump_digit(app, 1);
    else if(event->key == InputKeyDown)
        freq_input_bump_digit(app, -1);

    m = view_get_model(app->freq_input_view);
    UNUSED(m);
    view_commit_model(app->freq_input_view, true);

    return true;
}

void freq_input_start(FlipperHamApp* app) {
    if(!app) return;

    app->freq_focus = 0;
    freq_input_old_hz = app->freq_edit_hz;
}

void freq_input_alloc(FlipperHamApp* app) {
    FreqInputModel* m;

    app->freq_input_view = view_alloc();
    view_set_context(app->freq_input_view, app);
    view_allocate_model(app->freq_input_view, ViewModelTypeLocking, sizeof(FreqInputModel));
    m = view_get_model(app->freq_input_view);
    m->app = app;
    view_commit_model(app->freq_input_view, false);
    view_set_draw_callback(app->freq_input_view, freq_input_draw);
    view_set_input_callback(app->freq_input_view, freq_input_do);
}

void freq_input_free(FlipperHamApp* app) {
    if(!app->freq_input_view) return;
    view_free(app->freq_input_view);
    app->freq_input_view = NULL;
}

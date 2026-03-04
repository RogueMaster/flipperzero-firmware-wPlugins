#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_i2c.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <input/input.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// LSM6DS3 on power I2C bus
#define IMU_ADDR          (0x6A << 1)
#define IMU_REG_CTRL1_XL  0x10
#define IMU_REG_OUTX_L_XL 0x28

#define MAX_GAP_MM        99.0f
#define MAX_SPINDLE_SIZES 11

static const float spindle_mm[]    = {12.0f,13.0f,25.0f,32.0f,35.0f,38.0f,41.0f,42.0f,44.0f,47.0f,50.0f};
static const char* spindle_label[] = {"12","13","25","32","35","38","41","42","44","47","50"};

typedef enum {
    ViewMenu,
    ViewAngle,       // IMU live angle + manual entry
    ViewStairInput,
    ViewStairResult,
    ViewStraightInput,
    ViewStraightResult,
} AppViewId;

typedef struct {
    // IMU
    float live_angle;
    bool  imu_ok;

    // Stair
    float stair_angle;      // tenths stored as int: 425 = 42.5 deg
    int   stair_angle_x10;  // integer tenths for manual input
    int   stair_run;
    int   stair_spindle;
    int   s_count;
    float s_horiz_gap;
    float s_rake_gap;
    float s_ctr_ctr;

    // Straight
    int   str_length;
    int   str_spindle;
    int   t_count;
    float t_gap;
    float t_ctr_ctr;

    // Result scroll
    int   result_scroll;

    Gui*            gui;
    ViewDispatcher* vd;
    Submenu*        menu;
    View*           v_angle;
    View*           v_stair_in;
    View*           v_stair_res;
    View*           v_straight_in;
    View*           v_straight_res;
    FuriTimer*      timer;
} App;

// ── IMU ──────────────────────────────────────────────────────────────────────

static bool imu_init(void) {
    uint8_t cfg = 0x40; // 104Hz, ±2g
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    bool ok = furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_power, IMU_ADDR,
        IMU_REG_CTRL1_XL, &cfg, 1, 20);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    return ok;
}

static float imu_pitch(bool* ok_out) {
    uint8_t buf[6] = {0};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
    bool ok = furi_hal_i2c_read_mem(
        &furi_hal_i2c_handle_power, IMU_ADDR,
        IMU_REG_OUTX_L_XL, buf, 6, 20);
    furi_hal_i2c_release(&furi_hal_i2c_handle_power);
    if(ok_out) *ok_out = ok;
    if(!ok) return 0.0f;
    float ax = (int16_t)((buf[1]<<8)|buf[0]) * 0.000061f;
    float ay = (int16_t)((buf[3]<<8)|buf[2]) * 0.000061f;
    float az = (int16_t)((buf[5]<<8)|buf[4]) * 0.000061f;
    // When Flipper laid flat on a surface, pitch from horizontal
    return fabsf(atan2f(az, sqrtf(ax*ax + ay*ay)) * (180.0f / (float)M_PI));
}

// ── Calculations ─────────────────────────────────────────────────────────────

static void calc_stair(App* app) {
    // stair_run is the DIAGONAL (rake) length entered by user
    // Convert to horizontal first: horiz = diagonal * cos(angle)
    // Gap limit (MAX_GAP_MM) is enforced on the horizontal gap
    // Rake gap is derived for display only
    float diag  = (float)app->stair_run;
    float thick = spindle_mm[app->stair_spindle];
    float angle = (float)app->stair_angle_x10 / 10.0f;
    float cos_a = cosf(angle * (float)M_PI / 180.0f);
    float run   = (cos_a > 0.001f) ? diag * cos_a : diag;
    int n;
    for(n = 1; n <= 300; n++) {
        if((float)(n + 1) * thick >= run) { n--; break; }
        if((run - n * thick) / (float)(n + 1) <= MAX_GAP_MM) break;
    }
    if(n < 1) n = 1;
    float hg = (run - (float)n * thick) / (float)(n + 1);
    app->s_count     = n;
    app->s_horiz_gap = hg;
    app->s_rake_gap  = (cos_a > 0.001f) ? hg / cos_a : hg;
    app->s_ctr_ctr   = hg + thick;
}

static void calc_straight(App* app) {
    float len   = (float)app->str_length;
    float thick = spindle_mm[app->str_spindle];
    int n;
    for(n = 1; n <= 300; n++) {
        // Stop before spindles alone would fill or exceed the length (avoids negative gap)
        if((float)(n + 1) * thick >= len) { n--; break; }
        if((len - n * thick) / (float)(n + 1) <= MAX_GAP_MM) break;
    }
    if(n < 1) n = 1;
    float g = (len - (float)n * thick) / (float)(n + 1);
    app->t_count   = n;
    app->t_gap     = g;
    app->t_ctr_ctr = g + thick;
}

// ── Draw helpers ─────────────────────────────────────────────────────────────

static void hdr(Canvas* c, const char* t) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 0, AlignCenter, AlignTop, t);
    canvas_draw_line(c, 0, 11, 128, 11);
}

static void ftr(Canvas* c, const char* l, const char* r) {
    canvas_draw_line(c, 0, 53, 128, 53);
    canvas_set_font(c, FontSecondary);
    if(l) canvas_draw_str(c, 2, 63, l);
    if(r) canvas_draw_str_aligned(c, 126, 63, AlignRight, AlignBottom, r);
}

// ── Angle View ───────────────────────────────────────────────────────────────
// Shows live IMU angle if working, plus manual +/- input
// Up/Down = +/- 0.1 deg    Left/Right = +/- 1.0 deg
// OK = lock angle and proceed    Back = menu

typedef struct { App* app; } AngleModel;

static void angle_draw(Canvas* c, void* m) {
    App* app = ((AngleModel*)m)->app;
    canvas_clear(c);
    hdr(c, "Stair Angle");

    char buf[32];
    float display_angle = (float)app->stair_angle_x10 / 10.0f;

    if(app->imu_ok) {
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 2, 22, "IMU live (lay on tread):");
        // Show live IMU reading smaller
        snprintf(buf, sizeof(buf), "IMU: %.1f", (double)app->live_angle);
        canvas_draw_str(c, 2, 32, buf);
    } else {
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 2, 22, "Manual entry:");
    }

    // Angle display - FontPrimary keeps it readable without pushing footer off
    canvas_set_font(c, FontPrimary);
    snprintf(buf, sizeof(buf), "%.1f deg", (double)display_angle);
    canvas_draw_str_aligned(c, 64, 28, AlignCenter, AlignTop, buf);

    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 43, AlignCenter, AlignTop, "U/D:0.1  L/R:1.0 deg");

    ftr(c, "Back", "OK=Use");
}

static bool angle_input(InputEvent* ev, void* ctx) {
    App* app = (App*)ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;

    if(ev->key == InputKeyOk && ev->type == InputTypeShort) {
        // If IMU is working, use live angle
        if(app->imu_ok && app->live_angle > 0.5f) {
            app->stair_angle_x10 = (int)(app->live_angle * 10.0f + 0.5f);
        }
        app->result_scroll = 0;
        view_dispatcher_switch_to_view(app->vd, ViewStairInput);
        return true;
    }
    if(ev->key == InputKeyBack && ev->type == InputTypeShort) {
        view_dispatcher_switch_to_view(app->vd, ViewMenu);
        return true;
    }
    // Manual angle adjust
    if(ev->key == InputKeyUp) {
        app->stair_angle_x10++;
        if(app->stair_angle_x10 > 600) app->stair_angle_x10 = 600; // max 60.0
    } else if(ev->key == InputKeyDown) {
        app->stair_angle_x10--;
        if(app->stair_angle_x10 < 10) app->stair_angle_x10 = 10; // min 1.0
    } else if(ev->key == InputKeyRight) {
        app->stair_angle_x10 += 10;
        if(app->stair_angle_x10 > 600) app->stair_angle_x10 = 600;
    } else if(ev->key == InputKeyLeft) {
        app->stair_angle_x10 -= 10;
        if(app->stair_angle_x10 < 10) app->stair_angle_x10 = 10;
    }
    // Trigger redraw
    with_view_model(app->v_angle, AngleModel* md, { UNUSED(md); }, true);
    return true;
}

// ── Stair Input View ─────────────────────────────────────────────────────────
// Field 0 = run,  Field 1 = spindle size
// Up/Down = ±10mm,  Left/Right = ±100mm  (or scroll spindle)

typedef struct { App* app; int field; } StairInModel;

static void stair_in_draw(Canvas* c, void* m) {
    StairInModel* md = (StairInModel*)m;
    App* app = md->app;
    canvas_clear(c);
    hdr(c, "Stair Inputs");
    canvas_set_font(c, FontSecondary);
    char buf[48];

    float angle = (float)app->stair_angle_x10 / 10.0f;
    snprintf(buf, sizeof(buf), "Angle: %.1f deg", (double)angle);
    canvas_draw_str(c, 2, 22, buf);

    bool s0 = (md->field == 0);
    if(s0) { canvas_draw_box(c, 0, 24, 128, 13); canvas_set_color(c, ColorWhite); }
    snprintf(buf, sizeof(buf), "Diag len:  %d mm", app->stair_run);
    canvas_draw_str(c, 2, 35, buf);
    canvas_set_color(c, ColorBlack);

    bool s1 = (md->field == 1);
    if(s1) { canvas_draw_box(c, 0, 37, 128, 13); canvas_set_color(c, ColorWhite); }
    snprintf(buf, sizeof(buf), "Spindle: %s mm  [L/R]", spindle_label[app->stair_spindle]);
    canvas_draw_str(c, 2, 48, buf);
    canvas_set_color(c, ColorBlack);

    ftr(c, "Back", s1 ? "OK=Calc" : "OK=Next");
}

static bool stair_in_input(InputEvent* ev, void* ctx) {
    App* app = (App*)ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat && ev->type != InputTypeLong) return false;
    bool consumed = false;
    with_view_model(app->v_stair_in, StairInModel* md, {
        if(ev->key == InputKeyUp) {
            if(md->field == 0) {
                int step = (ev->type == InputTypeRepeat) ? 10 : 1;
                app->stair_run += step; if(app->stair_run > 20000) app->stair_run = 20000;
            }
            consumed = true;
        } else if(ev->key == InputKeyDown) {
            if(md->field == 0) {
                int step = (ev->type == InputTypeRepeat) ? 10 : 1;
                app->stair_run -= step; if(app->stair_run < 1) app->stair_run = 1;
            }
            consumed = true;
        } else if(ev->key == InputKeyLeft) {
            if(md->field == 1) { if(app->stair_spindle > 0) app->stair_spindle--; }
            else {
                int step = (ev->type == InputTypeLong) ? 100 : 10;
                app->stair_run -= step; if(app->stair_run < 1) app->stair_run = 1;
            }
            consumed = true;
        } else if(ev->key == InputKeyRight) {
            if(md->field == 1) { if(app->stair_spindle < MAX_SPINDLE_SIZES - 1) app->stair_spindle++; }
            else {
                int step = (ev->type == InputTypeLong) ? 100 : 10;
                app->stair_run += step; if(app->stair_run > 20000) app->stair_run = 20000;
            }
            consumed = true;
        } else if(ev->key == InputKeyOk) {
            if(md->field == 0) { md->field = 1; }
            else {
                app->result_scroll = 0;
                calc_stair(app);
                view_dispatcher_switch_to_view(app->vd, ViewStairResult);
            }
            consumed = true;
        } else if(ev->key == InputKeyBack) {
            if(md->field == 1) { md->field = 0; consumed = true; }
            else { view_dispatcher_switch_to_view(app->vd, ViewAngle); consumed = true; }
        }
    }, true);
    return consumed;
}

// ── Stair Result View (scrollable) ───────────────────────────────────────────

// Each result line: label + value
// We draw a window of lines, scroll with Up/Down

static void stair_res_draw(Canvas* c, void* m) {
    App* app = *(App**)m;
    canvas_clear(c);
    hdr(c, "Stair Results");

    float angle = (float)app->stair_angle_x10 / 10.0f;

    // All result lines
    char lines[9][48];
    snprintf(lines[0], sizeof(lines[0]), "Angle:    %.1f deg", (double)angle);
    snprintf(lines[1], sizeof(lines[1]), "Diagonal length: %d mm", app->stair_run);
    snprintf(lines[2], sizeof(lines[2]), "Spindle:  %s mm", spindle_label[app->stair_spindle]);
    snprintf(lines[3], sizeof(lines[3]), "Count:    %d spindles", app->s_count);
    snprintf(lines[4], sizeof(lines[4]), "Gaps:     %d", app->s_count + 1);
    snprintf(lines[5], sizeof(lines[5]), "Gap:    %.1f mm", (double)app->s_horiz_gap);
    snprintf(lines[6], sizeof(lines[6]), "Rake gap: %.1f mm", (double)app->s_rake_gap);
    snprintf(lines[7], sizeof(lines[7]), "Ctr-ctr:  %.1f mm", (double)app->s_ctr_ctr);
    int total_lines = 8;

    // Visible area: y 13 to 52 = 39px, FontSecondary ~8px per line = 4 lines visible
    int visible = 4;
    int max_scroll = total_lines - visible;
    if(app->result_scroll < 0) app->result_scroll = 0;
    if(app->result_scroll > max_scroll) app->result_scroll = max_scroll;

    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < visible; i++) {
        int li = i + app->result_scroll;
        if(li >= total_lines) break;
        // Highlight count and gaps lines
        if(li == 3 || li == 4) {
            canvas_draw_box(c, 0, 13 + i * 10, 128, 10);
            canvas_set_color(c, ColorWhite);
            canvas_draw_str(c, 2, 13 + i * 10 + 8, lines[li]);
            canvas_set_color(c, ColorBlack);
        } else {
            canvas_draw_str(c, 2, 13 + i * 10 + 8, lines[li]);
        }
    }

    // Scroll indicators
    if(app->result_scroll > 0) {
        canvas_draw_str(c, 120, 20, "^");
    }
    if(app->result_scroll < max_scroll) {
        canvas_draw_str(c, 120, 50, "v");
    }

    ftr(c, "Back", "U/D:Scroll");
}

static bool stair_res_input(InputEvent* ev, void* ctx) {
    App* app = (App*)ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;
    if(ev->key == InputKeyUp) { app->result_scroll--; with_view_model(app->v_stair_res, App** m, { UNUSED(m); }, true); return true; }
    if(ev->key == InputKeyDown) { app->result_scroll++; with_view_model(app->v_stair_res, App** m, { UNUSED(m); }, true); return true; }
    if(ev->key == InputKeyBack) { view_dispatcher_switch_to_view(app->vd, ViewStairInput); return true; }
    return false;
}

// ── Straight Input View ───────────────────────────────────────────────────────

typedef struct { App* app; int field; } StraightInModel;

static void straight_in_draw(Canvas* c, void* m) {
    StraightInModel* md = (StraightInModel*)m;
    App* app = md->app;
    canvas_clear(c);
    hdr(c, "Straight Run");
    canvas_set_font(c, FontSecondary);
    char buf[48];

    bool s0 = (md->field == 0);
    if(s0) { canvas_draw_box(c, 0, 13, 128, 13); canvas_set_color(c, ColorWhite); }
    snprintf(buf, sizeof(buf), "Length: %d mm", app->str_length);
    canvas_draw_str(c, 2, 24, buf);
    canvas_set_color(c, ColorBlack);

    bool s1 = (md->field == 1);
    if(s1) { canvas_draw_box(c, 0, 26, 128, 13); canvas_set_color(c, ColorWhite); }
    snprintf(buf, sizeof(buf), "Spindle: %s mm  [L/R]", spindle_label[app->str_spindle]);
    canvas_draw_str(c, 2, 37, buf);
    canvas_set_color(c, ColorBlack);

    canvas_draw_str(c, 2, 50, "U/D:1mm  L/R:10 hold:100");
    ftr(c, "Back", s1 ? "OK=Calc" : "OK=Next");
}

static bool straight_in_input(InputEvent* ev, void* ctx) {
    App* app = (App*)ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat && ev->type != InputTypeLong) return false;
    bool consumed = false;
    with_view_model(app->v_straight_in, StraightInModel* md, {
        if(ev->key == InputKeyUp) {
            if(md->field == 0) {
                int step = (ev->type == InputTypeRepeat) ? 10 : 1;
                app->str_length += step; if(app->str_length > 20000) app->str_length = 20000;
            }
            consumed = true;
        } else if(ev->key == InputKeyDown) {
            if(md->field == 0) {
                int step = (ev->type == InputTypeRepeat) ? 10 : 1;
                app->str_length -= step; if(app->str_length < 1) app->str_length = 1;
            }
            consumed = true;
        } else if(ev->key == InputKeyLeft) {
            if(md->field == 1) { if(app->str_spindle > 0) app->str_spindle--; }
            else {
                int step = (ev->type == InputTypeLong) ? 100 : 10;
                app->str_length -= step; if(app->str_length < 1) app->str_length = 1;
            }
            consumed = true;
        } else if(ev->key == InputKeyRight) {
            if(md->field == 1) { if(app->str_spindle < MAX_SPINDLE_SIZES - 1) app->str_spindle++; }
            else {
                int step = (ev->type == InputTypeLong) ? 100 : 10;
                app->str_length += step; if(app->str_length > 20000) app->str_length = 20000;
            }
            consumed = true;
        } else if(ev->key == InputKeyOk) {
            if(md->field == 0) { md->field = 1; }
            else {
                app->result_scroll = 0;
                calc_straight(app);
                view_dispatcher_switch_to_view(app->vd, ViewStraightResult);
            }
            consumed = true;
        } else if(ev->key == InputKeyBack) {
            if(md->field == 1) { md->field = 0; consumed = true; }
            else { view_dispatcher_switch_to_view(app->vd, ViewMenu); consumed = true; }
        }
    }, true);
    return consumed;
}

// ── Straight Result View (scrollable) ────────────────────────────────────────

static void straight_res_draw(Canvas* c, void* m) {
    App* app = *(App**)m;
    canvas_clear(c);
    hdr(c, "Straight Results");

    char lines[7][48];
    snprintf(lines[0], sizeof(lines[0]), "Length:  %d mm", app->str_length);
    snprintf(lines[1], sizeof(lines[1]), "Spindle: %s mm", spindle_label[app->str_spindle]);
    snprintf(lines[2], sizeof(lines[2]), "Count:   %d spindles", app->t_count);
    snprintf(lines[3], sizeof(lines[3]), "Gaps:    %d", app->t_count + 1);
    snprintf(lines[4], sizeof(lines[4]), "Gap:     %.1f mm", (double)app->t_gap);
    snprintf(lines[5], sizeof(lines[5]), "Ctr-ctr: %.1f mm", (double)app->t_ctr_ctr);
    int total_lines = 6;

    int visible = 4;
    int max_scroll = total_lines - visible;
    if(app->result_scroll < 0) app->result_scroll = 0;
    if(app->result_scroll > max_scroll) app->result_scroll = max_scroll;

    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < visible; i++) {
        int li = i + app->result_scroll;
        if(li >= total_lines) break;
        if(li == 2 || li == 3) {
            canvas_draw_box(c, 0, 13 + i * 10, 128, 10);
            canvas_set_color(c, ColorWhite);
            canvas_draw_str(c, 2, 13 + i * 10 + 8, lines[li]);
            canvas_set_color(c, ColorBlack);
        } else {
            canvas_draw_str(c, 2, 13 + i * 10 + 8, lines[li]);
        }
    }

    if(app->result_scroll > 0) canvas_draw_str(c, 120, 20, "^");
    if(app->result_scroll < max_scroll) canvas_draw_str(c, 120, 50, "v");

    ftr(c, "Back", "U/D:Scroll");
}

static bool straight_res_input(InputEvent* ev, void* ctx) {
    App* app = (App*)ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;
    if(ev->key == InputKeyUp) { app->result_scroll--; with_view_model(app->v_straight_res, App** m, { UNUSED(m); }, true); return true; }
    if(ev->key == InputKeyDown) { app->result_scroll++; with_view_model(app->v_straight_res, App** m, { UNUSED(m); }, true); return true; }
    if(ev->key == InputKeyBack) { view_dispatcher_switch_to_view(app->vd, ViewStraightInput); return true; }
    return false;
}

// ── Menu ─────────────────────────────────────────────────────────────────────

static void menu_cb(void* ctx, uint32_t idx) {
    App* app = (App*)ctx;
    if(idx == 0) { app->result_scroll = 0; view_dispatcher_switch_to_view(app->vd, ViewAngle); }
    else         { app->result_scroll = 0; view_dispatcher_switch_to_view(app->vd, ViewStraightInput); }
}
static bool nav_cb(void* ctx) { App* app = (App*)ctx; view_dispatcher_stop(app->vd); return true; }
static bool custom_cb(void* ctx, uint32_t ev) { UNUSED(ctx); UNUSED(ev); return false; }

// ── Timer (IMU update) ────────────────────────────────────────────────────────

static void tick_cb(void* ctx) {
    App* app = (App*)ctx;
    bool ok = false;
    float angle = imu_pitch(&ok);
    app->imu_ok = ok;
    if(ok) app->live_angle = angle;
    with_view_model(app->v_angle, AngleModel* md, { UNUSED(md); }, true);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int32_t spindle_calc_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->stair_angle_x10 = 420; // 42.0 deg default
    app->stair_run       = 3600;
    app->stair_spindle   = 6;   // 41mm
    app->str_length      = 3600;
    app->str_spindle     = 6;   // 41mm
    app->result_scroll   = 0;
    app->imu_ok          = imu_init();

    app->gui = furi_record_open(RECORD_GUI);
    app->vd  = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_custom_event_callback(app->vd, custom_cb);
    view_dispatcher_set_navigation_event_callback(app->vd, nav_cb);
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    // Menu
    app->menu = submenu_alloc();
    submenu_add_item(app->menu, "Stair Spindles",    0, menu_cb, app);
    submenu_add_item(app->menu, "Straight Spindles", 1, menu_cb, app);
    view_dispatcher_add_view(app->vd, ViewMenu, submenu_get_view(app->menu));

    // Angle view
    app->v_angle = view_alloc();
    view_set_context(app->v_angle, app);
    view_allocate_model(app->v_angle, ViewModelTypeLocking, sizeof(AngleModel));
    with_view_model(app->v_angle, AngleModel* md, { md->app = app; }, false);
    view_set_draw_callback(app->v_angle, angle_draw);
    view_set_input_callback(app->v_angle, angle_input);
    view_dispatcher_add_view(app->vd, ViewAngle, app->v_angle);

    // Stair input
    app->v_stair_in = view_alloc();
    view_set_context(app->v_stair_in, app);
    view_allocate_model(app->v_stair_in, ViewModelTypeLocking, sizeof(StairInModel));
    with_view_model(app->v_stair_in, StairInModel* md, { md->app = app; md->field = 0; }, false);
    view_set_draw_callback(app->v_stair_in, stair_in_draw);
    view_set_input_callback(app->v_stair_in, stair_in_input);
    view_dispatcher_add_view(app->vd, ViewStairInput, app->v_stair_in);

    // Stair result
    app->v_stair_res = view_alloc();
    view_set_context(app->v_stair_res, app);
    view_allocate_model(app->v_stair_res, ViewModelTypeLocking, sizeof(App*));
    with_view_model(app->v_stair_res, App** md, { *md = app; }, false);
    view_set_draw_callback(app->v_stair_res, (ViewDrawCallback)stair_res_draw);
    view_set_input_callback(app->v_stair_res, stair_res_input);
    view_dispatcher_add_view(app->vd, ViewStairResult, app->v_stair_res);

    // Straight input
    app->v_straight_in = view_alloc();
    view_set_context(app->v_straight_in, app);
    view_allocate_model(app->v_straight_in, ViewModelTypeLocking, sizeof(StraightInModel));
    with_view_model(app->v_straight_in, StraightInModel* md, { md->app = app; md->field = 0; }, false);
    view_set_draw_callback(app->v_straight_in, straight_in_draw);
    view_set_input_callback(app->v_straight_in, straight_in_input);
    view_dispatcher_add_view(app->vd, ViewStraightInput, app->v_straight_in);

    // Straight result
    app->v_straight_res = view_alloc();
    view_set_context(app->v_straight_res, app);
    view_allocate_model(app->v_straight_res, ViewModelTypeLocking, sizeof(App*));
    with_view_model(app->v_straight_res, App** md, { *md = app; }, false);
    view_set_draw_callback(app->v_straight_res, (ViewDrawCallback)straight_res_draw);
    view_set_input_callback(app->v_straight_res, straight_res_input);
    view_dispatcher_add_view(app->vd, ViewStraightResult, app->v_straight_res);

    // Timer 10Hz for IMU
    app->timer = furi_timer_alloc(tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100);

    view_dispatcher_switch_to_view(app->vd, ViewMenu);
    view_dispatcher_run(app->vd);

    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    view_dispatcher_remove_view(app->vd, ViewMenu);
    view_dispatcher_remove_view(app->vd, ViewAngle);
    view_dispatcher_remove_view(app->vd, ViewStairInput);
    view_dispatcher_remove_view(app->vd, ViewStairResult);
    view_dispatcher_remove_view(app->vd, ViewStraightInput);
    view_dispatcher_remove_view(app->vd, ViewStraightResult);
    view_free(app->v_angle);
    view_free(app->v_stair_in);
    view_free(app->v_stair_res);
    view_free(app->v_straight_in);
    view_free(app->v_straight_res);
    submenu_free(app->menu);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
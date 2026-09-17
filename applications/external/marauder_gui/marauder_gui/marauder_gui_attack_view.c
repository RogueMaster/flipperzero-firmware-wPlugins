#include "marauder_gui_app_i.h"

/* Shared custom View for every attack/spam status scene - see marauder_gui_app_i.h for why this
   exists instead of the Widget these scenes used to use, and why it's shared rather than
   duplicated per scene (two concrete consumers - wifi_attack.c and bt_spam.c - as of writing).
   Neither animation reflects real RF data: Marauder's serial output has no per-packet signal
   figures for a running attack, so this is purely "something is happening" feedback in place of
   what used to be Marauder's own (unlocalizable, English-only) raw confirmation line. */

#define ATTACK_VIEW_BAND_Y 20
#define ATTACK_VIEW_BAND_H 24
#define ATTACK_VIEW_CENTER_X 64
#define ATTACK_VIEW_CENTER_Y (ATTACK_VIEW_BAND_Y + ATTACK_VIEW_BAND_H / 2)

#define SPECTRUM_BAR_COUNT 20
#define SPECTRUM_BAR_WIDTH 3
#define SPECTRUM_BAR_GAP 2
#define SPECTRUM_PERIOD 24
#define SPECTRUM_PHASE_STEP 3

static void marauder_gui_attack_view_draw_spectrum(Canvas* canvas, MarauderGuiApp* app) {
    const int32_t step = SPECTRUM_BAR_WIDTH + SPECTRUM_BAR_GAP;
    const int32_t total_w = SPECTRUM_BAR_COUNT * step - SPECTRUM_BAR_GAP;
    const int32_t x0 = (canvas_width(canvas) - total_w) / 2;
    const int32_t half_period = SPECTRUM_PERIOD / 2;

    for(int32_t i = 0; i < SPECTRUM_BAR_COUNT; i++) {
        uint32_t phase = (app->attack_spectrum_phase + (uint32_t)i * SPECTRUM_PHASE_STEP) %
                          SPECTRUM_PERIOD;
        int32_t triangle = (phase < (uint32_t)half_period) ? (int32_t)phase :
                                                              (SPECTRUM_PERIOD - (int32_t)phase);
        int32_t height = 2 + (triangle * (ATTACK_VIEW_BAND_H - 2)) / half_period;

        canvas_draw_box(
            canvas,
            x0 + i * step,
            ATTACK_VIEW_BAND_Y + ATTACK_VIEW_BAND_H - height,
            SPECTRUM_BAR_WIDTH,
            height);
    }
}

/* Rotating radar sweep over a ring of "device" blips that flash as the sweep passes them -
   reads as active scanning/discovery, which fits BLE spam better than a plain static pulse (the
   user's own verdict on the first attempt: "logic is nice but looks too plain in practice").
   No sin()/cos(): those aren't in this SDK's exposed API symbol set (same class of gotcha as
   strtok - silently fails at link time, "MissingImports"), so direction vectors for 16 discrete
   angles are precomputed here as a small integer table (cos/sin * 100, i.e. percent) instead of
   computed on-device. */
#define RADAR_ANGLE_STEPS 16
static const int8_t marauder_radar_cos16[RADAR_ANGLE_STEPS] = {
    100, 92, 71, 38, 0, -38, -71, -92, -100, -92, -71, -38, 0, 38, 71, 92};
static const int8_t marauder_radar_sin16[RADAR_ANGLE_STEPS] = {
    0, 38, 71, 92, 100, 92, 71, 38, 0, -38, -71, -92, -100, -92, -71, -38};

#define RADAR_RADIUS 22
#define RADAR_INNER_RADIUS 11
#define RADAR_SWEEP_TICKS_PER_STEP 2 /* one angle step every ~200ms - a full turn in ~3.2s */

/* Fixed "nearby device" positions the sweep passes over each rotation - angle index into the
   table above, plus how far out from center (percent of RADAR_RADIUS). */
static const struct {
    uint8_t angle_idx;
    uint8_t radius_pct;
} marauder_radar_blips[] = {
    {2, 65},
    {6, 85},
    {9, 50},
    {13, 75},
};
#define RADAR_BLIP_COUNT (sizeof(marauder_radar_blips) / sizeof(marauder_radar_blips[0]))

static void marauder_gui_attack_view_draw_radar(Canvas* canvas, MarauderGuiApp* app) {
    canvas_draw_circle(canvas, ATTACK_VIEW_CENTER_X, ATTACK_VIEW_CENTER_Y, RADAR_RADIUS);
    canvas_draw_circle(canvas, ATTACK_VIEW_CENTER_X, ATTACK_VIEW_CENTER_Y, RADAR_INNER_RADIUS);

    uint32_t sweep_idx =
        (app->attack_spectrum_phase / RADAR_SWEEP_TICKS_PER_STEP) % RADAR_ANGLE_STEPS;

    canvas_draw_line(
        canvas,
        ATTACK_VIEW_CENTER_X,
        ATTACK_VIEW_CENTER_Y,
        ATTACK_VIEW_CENTER_X + (marauder_radar_cos16[sweep_idx] * RADAR_RADIUS) / 100,
        ATTACK_VIEW_CENTER_Y + (marauder_radar_sin16[sweep_idx] * RADAR_RADIUS) / 100);

    for(size_t b = 0; b < RADAR_BLIP_COUNT; b++) {
        uint8_t idx = marauder_radar_blips[b].angle_idx;
        int32_t bx = ATTACK_VIEW_CENTER_X +
                     (marauder_radar_cos16[idx] * RADAR_RADIUS * marauder_radar_blips[b].radius_pct) /
                         10000;
        int32_t by = ATTACK_VIEW_CENTER_Y +
                     (marauder_radar_sin16[idx] * RADAR_RADIUS * marauder_radar_blips[b].radius_pct) /
                         10000;

        /* "Just passed" (not "about to be passed") so the flash trails the sweep line, like a
           real radar blip lighting up after the beam sweeps over it. */
        uint32_t behind = (sweep_idx >= idx) ? (sweep_idx - idx) : (RADAR_ANGLE_STEPS + sweep_idx - idx);
        if(behind <= 1) {
            canvas_draw_circle(canvas, bx, by, 3);
        } else {
            canvas_draw_dot(canvas, bx, by);
        }
    }
}

static void marauder_gui_attack_view_draw_callback(Canvas* canvas, void* model) {
    MarauderGuiApp* app = *(MarauderGuiApp**)model;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, app->attack_view_title);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignTop, app->attack_view_target);

    if(app->attack_view_style == MarauderAttackViewStyleRadar) {
        marauder_gui_attack_view_draw_radar(canvas, app);
    } else {
        marauder_gui_attack_view_draw_spectrum(canvas, app);
    }

    canvas_draw_str_aligned(
        canvas, 64, 56, AlignCenter, AlignTop, marauder_gui_text(app, "Durdur: Ok", "Stop: Ok"));
}

static bool marauder_gui_attack_view_input_callback(InputEvent* event, void* context) {
    MarauderGuiApp* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MARAUDER_ATTACK_STOP_CUSTOM_EVENT);
        return true;
    }
    return false;
}

void marauder_gui_attack_view_redraw(MarauderGuiApp* app) {
    with_view_model(app->attack_status_view, MarauderGuiApp * *model, { UNUSED(model); }, true);
}

View* marauder_gui_attack_view_alloc(MarauderGuiApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(MarauderGuiApp*));
    with_view_model(view, MarauderGuiApp * *model, { *model = app; }, false);
    view_set_draw_callback(view, marauder_gui_attack_view_draw_callback);
    view_set_input_callback(view, marauder_gui_attack_view_input_callback);
    view_set_context(view, app);
    return view;
}

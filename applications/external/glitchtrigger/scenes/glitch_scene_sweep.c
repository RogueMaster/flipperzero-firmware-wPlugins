#include "../glitch_trigger_i.h"

/* Width sweep: fire one shot per tick, stepping the pulse width from `from` to
 * `to`, looping. When the target faults, OK captures the width in play. */

typedef enum {
    SweepEvtMark = 20,
    SweepEvtPause,
    SweepEvtRestart,
} SweepEvt;

static uint32_t s_from, s_to, s_step;
static uint32_t s_current;
static uint32_t s_index, s_total;
static uint32_t s_hits, s_last_hit;
static bool s_running;

static void glitch_sweep_view_cb(SweepViewEvent event, void* ctx) {
    GlitchApp* app = ctx;
    uint32_t e = SweepEvtMark;
    if(event == SweepViewEventPause) e = SweepEvtPause;
    if(event == SweepViewEventRestart) e = SweepEvtRestart;
    view_dispatcher_send_custom_event(app->view_dispatcher, e);
}

static void glitch_sweep_reset_range(GlitchApp* app) {
    s_from = app->params.sweep_from_ns;
    s_to = app->params.sweep_to_ns;
    s_step = app->params.sweep_step_ns;
    if(s_step == 0) s_step = 1;
    if(s_to < s_from) {
        uint32_t t = s_from;
        s_from = s_to;
        s_to = t;
    }
    s_total = (s_to - s_from) / s_step + 1;
    s_current = s_from;
    s_index = 0;
}

static void glitch_sweep_push(GlitchApp* app) {
    sweep_view_set_range(app->sweep_view, s_from, s_to, s_step);
    sweep_view_set_progress(app->sweep_view, s_current, s_index, s_total);
    sweep_view_set_counts(app->sweep_view, glitch_engine_shots(app->engine), s_hits);
    sweep_view_set_running(app->sweep_view, s_running);
    sweep_view_set_last_hit(app->sweep_view, s_last_hit);
}

void glitch_scene_sweep_on_enter(void* context) {
    GlitchApp* app = context;

    glitch_engine_configure(app->engine, &app->params);
    glitch_engine_reset_shots(app->engine);
    glitch_sweep_reset_range(app);
    s_hits = 0;
    s_last_hit = 0;
    s_running = true;

    sweep_view_set_callback(app->sweep_view, glitch_sweep_view_cb, app);
    glitch_sweep_push(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewSweep);
}

bool glitch_scene_sweep_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SweepEvtMark:
            s_hits++;
            s_last_hit = s_current;
            glitch_notify_hit(app);
            glitch_sweep_push(app);
            consumed = true;
            break;
        case SweepEvtPause:
            s_running = !s_running;
            sweep_view_set_running(app->sweep_view, s_running);
            consumed = true;
            break;
        case SweepEvtRestart:
            glitch_sweep_reset_range(app);
            glitch_sweep_push(app);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(s_running) {
            GlitchParams shot = app->params;
            shot.width_ns = s_current;
            shot.trigger = GlitchTriggerManual; // one deterministic shot
            shot.sweep_enabled = false;
            glitch_engine_fire(app->engine, &shot);

            /* advance, loop at the top */
            s_index++;
            s_current += s_step;
            if(s_current > s_to || s_index >= s_total) {
                s_current = s_from;
                s_index = 0;
            }
            glitch_sweep_push(app);
        }
        consumed = true;
    }
    return consumed;
}

void glitch_scene_sweep_on_exit(void* context) {
    GlitchApp* app = context;
    glitch_engine_release(app->engine);
    s_running = false;
}

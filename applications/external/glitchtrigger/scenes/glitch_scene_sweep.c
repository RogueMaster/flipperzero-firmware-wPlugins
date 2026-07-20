#include "../glitch_trigger_i.h"

/* Sweep runner. Fires `dwell` shots at each point of a width range - or, in 2D,
 * a delay x width grid - then advances, looping at the end. A hit is recorded
 * when the user presses OK, or automatically when Auto-hit sees the feedback
 * pin reach its success level. Hits can be logged to CSV. */

typedef enum {
    SweepEvtMark = 20,
    SweepEvtPause,
    SweepEvtRestart,
} SweepEvt;

static uint32_t s_wfrom, s_wto, s_wstep; // width axis
static uint32_t s_dfrom, s_dto, s_dstep; // delay axis (2D)
static bool s_2d, s_auto, s_log;
static uint16_t s_dwell, s_dwell_i;
static uint32_t s_cur_w, s_cur_d;
static uint32_t s_wsteps, s_dsteps;
static uint32_t s_wi, s_di;
static uint32_t s_index, s_total;
static uint32_t s_hits, s_last_hit_w, s_last_hit_d;
static bool s_point_hit; // one auto-mark per point
static bool s_running;

static void glitch_sweep_view_cb(SweepViewEvent event, void* ctx) {
    GlitchApp* app = ctx;
    uint32_t e = SweepEvtMark;
    if(event == SweepViewEventPause) e = SweepEvtPause;
    if(event == SweepViewEventRestart) e = SweepEvtRestart;
    view_dispatcher_send_custom_event(app->view_dispatcher, e);
}

static uint32_t range_steps(uint32_t from, uint32_t to, uint32_t step) {
    if(step == 0) step = 1;
    if(to < from) {
        uint32_t t = from;
        from = to;
        to = t;
    }
    return (to - from) / step + 1;
}

static void glitch_sweep_reset(GlitchApp* app) {
    GlitchParams* p = &app->params;
    s_2d = p->sweep_2d;
    s_auto = p->auto_hit;
    s_log = p->log_hits;
    s_dwell = p->sweep_dwell ? p->sweep_dwell : 1;

    s_wfrom = p->sweep_from_ns;
    s_wto = p->sweep_to_ns;
    s_wstep = p->sweep_step_ns ? p->sweep_step_ns : 1;
    if(s_wto < s_wfrom) {
        uint32_t t = s_wfrom;
        s_wfrom = s_wto;
        s_wto = t;
    }
    s_wsteps = range_steps(s_wfrom, s_wto, s_wstep);

    s_dfrom = p->sweep_delay_from_us;
    s_dto = p->sweep_delay_to_us;
    s_dstep = p->sweep_delay_step_us ? p->sweep_delay_step_us : 1;
    if(s_dto < s_dfrom) {
        uint32_t t = s_dfrom;
        s_dfrom = s_dto;
        s_dto = t;
    }
    s_dsteps = s_2d ? range_steps(s_dfrom, s_dto, s_dstep) : 1;

    s_total = s_wsteps * s_dsteps;
    s_wi = s_di = s_index = 0;
    s_cur_w = s_wfrom;
    s_cur_d = s_2d ? s_dfrom : p->delay_us;
    s_dwell_i = 0;
    s_point_hit = false;
}

static void glitch_sweep_advance(void) {
    s_dwell_i = 0;
    s_point_hit = false;
    s_index++;
    if(s_index >= s_total) { // wrap to the start of the grid
        s_index = 0;
        s_wi = s_di = 0;
        s_cur_w = s_wfrom;
        if(s_2d) s_cur_d = s_dfrom;
        return;
    }
    s_wi++;
    s_cur_w += s_wstep;
    if(s_wi >= s_wsteps) {
        s_wi = 0;
        s_cur_w = s_wfrom;
        if(s_2d) {
            s_di++;
            s_cur_d += s_dstep;
        }
    }
}

static void glitch_sweep_push(GlitchApp* app) {
    sweep_view_set_width_range(app->sweep_view, s_wfrom, s_wto);
    sweep_view_set_point(app->sweep_view, s_cur_w, s_cur_d, s_2d);
    sweep_view_set_progress(app->sweep_view, s_index, s_total);
    sweep_view_set_counts(app->sweep_view, glitch_engine_shots(app->engine), s_hits);
    sweep_view_set_running(app->sweep_view, s_running);
    sweep_view_set_auto(app->sweep_view, s_auto);
    sweep_view_set_last_hit(app->sweep_view, s_last_hit_w, s_last_hit_d);
}

static void glitch_sweep_record_hit(GlitchApp* app, const char* source) {
    s_hits++;
    s_last_hit_w = s_cur_w;
    s_last_hit_d = s_cur_d;
    glitch_notify_hit(app);
    if(s_log) {
        GlitchParams hit = app->params;
        hit.width_ns = s_cur_w;
        if(s_2d) hit.delay_us = s_cur_d;
        glitch_storage_log_hit(&hit, glitch_engine_shots(app->engine), source);
    }
}

void glitch_scene_sweep_on_enter(void* context) {
    GlitchApp* app = context;

    glitch_engine_configure(app->engine, &app->params);
    glitch_engine_reset_shots(app->engine);
    glitch_sweep_reset(app);
    s_hits = 0;
    s_last_hit_w = 0;
    s_last_hit_d = 0;
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
            glitch_sweep_record_hit(app, "mark");
            glitch_sweep_push(app);
            consumed = true;
            break;
        case SweepEvtPause:
            s_running = !s_running;
            sweep_view_set_running(app->sweep_view, s_running);
            consumed = true;
            break;
        case SweepEvtRestart:
            glitch_sweep_reset(app);
            glitch_sweep_push(app);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(s_running) {
            GlitchParams shot = app->params;
            shot.width_ns = s_cur_w;
            if(s_2d) shot.delay_us = s_cur_d;
            shot.trigger = GlitchTriggerManual; // one deterministic shot
            shot.sweep_enabled = false;
            glitch_engine_fire(app->engine, &shot);

            if(s_auto && !s_point_hit &&
               glitch_engine_feedback_hit(app->engine, app->params.fb_active_high)) {
                s_point_hit = true;
                glitch_sweep_record_hit(app, "auto");
            }

            if(++s_dwell_i >= s_dwell) glitch_sweep_advance();
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

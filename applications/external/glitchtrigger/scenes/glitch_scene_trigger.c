#include "../glitch_trigger_i.h"

/* Fire-control state machine over the trigger view.
 *
 *   Manual   : OK arms; OK again fires one shot (repeatable).
 *   External : OK arms the GPIO interrupt; each edge fires a shot; OK disarms.
 *   Repeat   : OK starts free-running at the repeat interval; OK stops.
 *
 * Every shot - whoever fired it (button, ISR or timer) - is detected uniformly
 * by watching the engine's shot counter on tick, so feedback is consistent. */

typedef enum {
    TrigEvtOk = 10,
    TrigEvtLeft,
    TrigEvtRight,
} TrigEvt;

static TriggerState s_state;
static uint32_t s_last_shots;
static uint32_t s_last_repeat_tick;

static void glitch_trigger_view_cb(TriggerViewEvent event, void* ctx) {
    GlitchApp* app = ctx;
    uint32_t e = TrigEvtOk;
    if(event == TriggerViewEventLeft) e = TrigEvtLeft;
    if(event == TriggerViewEventRight) e = TrigEvtRight;
    view_dispatcher_send_custom_event(app->view_dispatcher, e);
}

static void glitch_trigger_push(GlitchApp* app) {
    trigger_view_set_params(app->trigger_view, &app->params);
    trigger_view_set_state(app->trigger_view, s_state);
    trigger_view_set_shots(app->trigger_view, glitch_engine_shots(app->engine));
}

void glitch_scene_trigger_on_enter(void* context) {
    GlitchApp* app = context;

    glitch_engine_configure(app->engine, &app->params);
    glitch_engine_reset_shots(app->engine);
    s_state = TriggerStateIdle;
    s_last_shots = 0;
    s_last_repeat_tick = furi_get_tick();

    trigger_view_set_callback(app->trigger_view, glitch_trigger_view_cb, app);
    glitch_trigger_push(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewTrigger);
}

static void glitch_trigger_toggle_arm(GlitchApp* app) {
    switch(app->params.trigger) {
    case GlitchTriggerManual:
        if(s_state == TriggerStateIdle) {
            s_state = TriggerStateArmed;
            glitch_notify_arm(app);
        } else {
            /* Armed + OK => fire; tick will pick up the increment for feedback. */
            glitch_engine_fire(app->engine, &app->params);
        }
        break;

    case GlitchTriggerExternal:
        if(s_state == TriggerStateIdle) {
            glitch_engine_arm_external(app->engine, &app->params);
            s_state = TriggerStateWaiting;
            glitch_notify_arm(app);
        } else {
            glitch_engine_disarm(app->engine);
            s_state = TriggerStateIdle;
            glitch_notify_disarm(app);
        }
        break;

    case GlitchTriggerRepeat:
        if(s_state == TriggerStateIdle) {
            s_state = TriggerStateRunning;
            s_last_repeat_tick = furi_get_tick();
            glitch_notify_arm(app);
        } else {
            s_state = TriggerStateIdle;
            glitch_notify_disarm(app);
        }
        break;

    default:
        break;
    }
    trigger_view_set_state(app->trigger_view, s_state);
}

bool glitch_scene_trigger_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TrigEvtOk) {
            glitch_trigger_toggle_arm(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        trigger_view_tick(app->trigger_view);

        /* Repeat mode: fire when the interval elapses. */
        if(s_state == TriggerStateRunning) {
            uint32_t now = furi_get_tick();
            uint32_t period = furi_ms_to_ticks(app->params.repeat_ms);
            if(period < 1) period = 1;
            if(now - s_last_repeat_tick >= period) {
                s_last_repeat_tick = now;
                glitch_engine_fire(app->engine, &app->params);
            }
        }

        /* Uniform shot feedback: any new shots since last tick. */
        uint32_t shots = glitch_engine_shots(app->engine);
        if(shots != s_last_shots) {
            s_last_shots = shots;
            trigger_view_set_shots(app->trigger_view, shots);
            trigger_view_flash(app->trigger_view);
            glitch_notify_fire(app);
        }
        consumed = true;
    }
    return consumed;
}

void glitch_scene_trigger_on_exit(void* context) {
    GlitchApp* app = context;
    glitch_engine_disarm(app->engine);
    glitch_engine_release(app->engine);
    s_state = TriggerStateIdle;
}

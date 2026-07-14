#include <stdio.h>

#include "../helpers/ir_helper.h"
#include "../helpers/time_helper.h"
#include "../timed_remote.h"
#include "timed_remote_scene.h"

static uint32_t countdown_seconds(const TimedRemoteApp*);
static void render_timer(TimedRemoteApp*);

#define REPEAT_UNLIMITED 255U

static void on_timer_tick(void* context) {
    TimedRemoteApp* app;

    app = context;
    view_dispatcher_send_custom_event(app->vd, EVENT_TIMER_TICK);
}

static uint32_t countdown_seconds(const TimedRemoteApp* app) {
    return time_hms_sec(app->h, app->m, app->s);
}

static void render_timer(TimedRemoteApp* app) {
    char timer[16];
    uint8_t h;
    uint8_t m;
    uint8_t s;

    time_sec_hms(app->left, &h, &m, &s);
    snprintf(timer, sizeof(timer), "%02d:%02d:%02d", h, m, s);

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontSecondary, app->sig);
    widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignTop, FontBigNumbers, timer);

    if(app->repeat > 0 && app->repeat < REPEAT_UNLIMITED) {
        char repeat[24];

        snprintf(
            repeat,
            sizeof(repeat),
            "Repeat: %d/%d",
            app->repeat - app->repeat_left + 1,
            app->repeat);
        widget_add_string_element(
            app->widget, 64, 52, AlignCenter, AlignTop, FontSecondary, repeat);
    } else if(app->repeat == REPEAT_UNLIMITED) {
        widget_add_string_element(
            app->widget, 64, 52, AlignCenter, AlignTop, FontSecondary, "Repeat: Unlimited");
    }
}

void scene_run_enter(void* context) {
    TimedRemoteApp* app;

    app = context;
    if(app->mode == MODE_COUNTDOWN) {
        app->left = countdown_seconds(app);
    } else {
        app->left = time_until(app->h, app->m, app->s);
        if(app->left == 0) {
            view_dispatcher_send_custom_event(app->vd, EVENT_FIRE_SIGNAL);
            return;
        }
    }

    if(app->repeat == 0) app->repeat_left = 1;

    render_timer(app);
    view_dispatcher_switch_to_view(app->vd, VIEW_RUN);

    app->timer = furi_timer_alloc(on_timer_tick, FuriTimerTypePeriodic, app);
    if(app->timer == NULL) return;

    furi_timer_start(app->timer, furi_kernel_get_tick_frequency());
}

bool scene_run_event(void* context, SceneManagerEvent event) {
    TimedRemoteApp* app;

    app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->sm, SCENE_BROWSE);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == EVENT_TIMER_TICK) {
        if(app->left > 0) {
            app->left--;
            render_timer(app);
        }
        if(app->left == 0) view_dispatcher_send_custom_event(app->vd, EVENT_FIRE_SIGNAL);
        return true;
    }
    if(event.event != EVENT_FIRE_SIGNAL) return false;

    if(app->ir != NULL) ir_tx(app->ir);

    if(app->repeat == REPEAT_UNLIMITED) {
        app->left = countdown_seconds(app);
        render_timer(app);
        return true;
    }

    if(app->repeat_left > 0) app->repeat_left--;

    if(app->repeat != 0 && app->repeat_left > 0) {
        app->left = countdown_seconds(app);
        render_timer(app);
        return true;
    }

    scene_manager_next_scene(app->sm, SCENE_DONE);
    return true;
}

void scene_run_exit(void* context) {
    TimedRemoteApp* app;

    app = context;
    if(app->timer != NULL) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    widget_reset(app->widget);
}

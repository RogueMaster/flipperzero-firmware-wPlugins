#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include "flipp_pomodoro_scene.h"
#include "../flipp_pomodoro_app.h"
#include "../views/flipp_pomodoro_timer_view.h"
#include "../modules/flipp_pomodoro_settings.h"
#include "../modules/flipp_pomodoro.h"
#include "../helpers/notification_manager.h"
#include "../helpers/time.h"
#include "../helpers/hints.h"

enum
{
    SceneEventConusmed = true,
    SceneEventNotConusmed = false
};

static char *random_string_of_list(char **hints, size_t num_hints)
{
    int random_index = rand() % num_hints;
    return hints[random_index];
}

void flipp_pomodoro_scene_timer_sync_view_state(void *ctx)
{
    furi_assert(ctx);

    FlippPomodoroApp *app = ctx;

    flipp_pomodoro_view_timer_set_state(
        flipp_pomodoro_view_timer_get_view(app->timer_view),
        app->state);
};

void flipp_pomodoro_scene_timer_on_next_stage(void* ctx) {
    furi_assert(ctx);
    FlippPomodoroApp* app = ctx;
    notification_manager_stop(app->notification_manager);
    const bool expired = flipp_pomodoro__is_stage_expired(app->state);
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        expired ? FlippPomodoroAppCustomEventStageComplete
                : FlippPomodoroAppCustomEventStageSkip);
}

void flipp_pomodoro_scene_timer_on_left(void* ctx) {
    FlippPomodoroApp* app = ctx;
    notification_manager_stop(app->notification_manager);
    scene_manager_next_scene(app->scene_manager, FlippPomodoroSceneConfig);
}

void flipp_pomodoro_scene_timer_on_ask_hint(void *ctx)
{
    FlippPomodoroApp *app = ctx;
    notification_manager_stop(app->notification_manager);
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        FlippPomodoroAppCustomEventTimerAskHint);
}

void flipp_pomodoro_scene_timer_on_enter(void *ctx)
{
    furi_assert(ctx);

    FlippPomodoroApp *app = ctx;
    notification_manager_reset(app->notification_manager);
    if (flipp_pomodoro__is_stage_expired(app->state))
    {
        FlippPomodoroSettings s;
        flipp_pomodoro_settings_load(&s);
        if(s.buzz_mode == FlippPomodoroBuzzSlide) {
            flipp_pomodoro__destroy(app->state);
            app->state = flipp_pomodoro__new();
        } else {
            notification_manager_stop(app->notification_manager);
            PomodoroStage next_stage = flipp_pomodoro__stage_by_index(app->state->current_stage_index + 1);
            notification_manager_handle_expired_stage(
                app->notification_manager,
                next_stage,
                s.buzz_mode);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, FlippPomodoroAppViewTimer);
    
    flipp_pomodoro_scene_timer_sync_view_state(app);

    flipp_pomodoro_view_timer_set_callback_context(app->timer_view, app);

    flipp_pomodoro_view_timer_set_on_ok_cb(
        app->timer_view,
        flipp_pomodoro_scene_timer_on_ask_hint);

    flipp_pomodoro_view_timer_set_on_right_cb(
         app->timer_view,
         flipp_pomodoro_scene_timer_on_next_stage);

     flipp_pomodoro_view_timer_set_on_left_cb(
          app->timer_view,
          flipp_pomodoro_scene_timer_on_left);
};

char *flipp_pomodoro_scene_timer_get_contextual_hint(FlippPomodoroApp *app)
{
    switch (flipp_pomodoro__get_stage(app->state))
    {
    case FlippPomodoroStageFocus:
        return random_string_of_list(work_hints, sizeof(work_hints) / sizeof(work_hints[0]));
    case FlippPomodoroStageRest:
    case FlippPomodoroStageLongBreak:
        return random_string_of_list(break_hints, BREAK_HINTS_COUNT);
    default:
        return "What's up?";
    }
}

void flipp_pomodoro_scene_timer_handle_custom_event(FlippPomodoroApp *app, FlippPomodoroAppCustomEvent custom_event)
{
    switch (custom_event)
    {
    case FlippPomodoroAppCustomEventTimerTick: {
        if (!flipp_pomodoro__is_stage_expired(app->state)) {
            notification_manager_reset_flags(app->notification_manager);
            break;
        }
        FlippPomodoroSettings s;
        flipp_pomodoro_settings_load(&s);

        PomodoroStage next_stage = flipp_pomodoro__stage_by_index(app->state->current_stage_index + 1);
        
        bool should_send_complete = notification_manager_handle_expired_stage(
            app->notification_manager,
            next_stage,
            s.buzz_mode);
        if(should_send_complete) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher,
                FlippPomodoroAppCustomEventStageComplete);
        }
        break;
    }
    case FlippPomodoroAppCustomEventStateUpdated:
        flipp_pomodoro_scene_timer_sync_view_state(app);
        flipp_pomodoro_view_timer_set_on_ok_cb(
            app->timer_view,
            flipp_pomodoro_scene_timer_on_ask_hint);
        notification_manager_reset_flags(app->notification_manager);
        notification_manager_stop(app->notification_manager);
        break;
    case FlippPomodoroAppCustomEventTimerAskHint:
        flipp_pomodoro_view_timer_display_hint(
            flipp_pomodoro_view_timer_get_view(app->timer_view),
            flipp_pomodoro_scene_timer_get_contextual_hint(app));
        break;
    default:
        break;
    }
};

bool flipp_pomodoro_scene_timer_on_event(void *ctx, SceneManagerEvent event)
{
    furi_assert(ctx);
    FlippPomodoroApp *app = ctx;

    switch (event.type)
    {
    case SceneManagerEventTypeCustom:
        flipp_pomodoro_scene_timer_handle_custom_event(
            app,
            event.event);
        return SceneEventConusmed;
    case SceneManagerEventTypeBack:
        notification_manager_stop(app->notification_manager);
        scene_manager_next_scene(app->scene_manager, FlippPomodoroSceneInfo);
        return SceneEventConusmed;
    default:
        break;
    };
    return SceneEventNotConusmed;
};

void flipp_pomodoro_scene_timer_on_exit(void *ctx)
{
    furi_assert(ctx);
    FlippPomodoroApp *app = ctx;
    notification_manager_stop(app->notification_manager);
};

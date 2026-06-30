#include <gui/scene_manager.h>
#include "../flipp_pomodoro_app.h"
#include "flipp_pomodoro_scene.h"
#include "../helpers/time.h"
#include "../modules/flipp_pomodoro_settings.h"
#include "../views/flipp_pomodoro_config_view.h"
#include "../modules/flipp_pomodoro.h"
#include <string.h>

static void flipp_pomodoro_scene_config_on_save(void* ctx) {
    FlippPomodoroApp* app = ctx;

    FlippPomodoroSettings s_now;
    flipp_pomodoro_view_config_get_settings(app->config_view, &s_now);
    flipp_pomodoro_settings_save(&s_now);
    // immediately apply to timers
    flipp_pomodoro__apply_settings(&s_now);

    scene_manager_next_scene(app->scene_manager, FlippPomodoroSceneTimer);
}

void flipp_pomodoro_scene_config_on_enter(void* ctx) {
    FlippPomodoroApp* app = ctx;
    app->paused_at_timestamp = time_now();

    FlippPomodoroSettings s;
    flipp_pomodoro_settings_load(&s);
    app->settings_before = s;

    // ram data to view
    flipp_pomodoro_view_config_set_settings(app->config_view, &s);

    // Save at center
    flipp_pomodoro_view_config_set_on_save_cb(app->config_view, flipp_pomodoro_scene_config_on_save, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlippPomodoroAppViewConfig);
}

bool flipp_pomodoro_scene_config_on_event(void* ctx, SceneManagerEvent event) {
    FlippPomodoroApp* app = ctx;
    if(event.type == SceneManagerEventTypeBack) {
        // when Back -> nothing to save
        scene_manager_next_scene(app->scene_manager, FlippPomodoroSceneTimer);
        return true;
    }
    return false;
}

void flipp_pomodoro_scene_config_on_exit(void* ctx) {
    FlippPomodoroApp* app = ctx;

    FlippPomodoroSettings now;
    flipp_pomodoro_settings_load(&now);

    bool changed = memcmp(&now, &app->settings_before, sizeof(FlippPomodoroSettings)) != 0;

    // settings from file
    flipp_pomodoro__apply_settings(&now);

    if(changed) {
        flipp_pomodoro__destroy(app->state);
        app->state = flipp_pomodoro__new();
    } else {
        uint32_t paused = time_now() - app->paused_at_timestamp;
        app->state->started_at_timestamp += paused;
    }
}

#include "ac_remote_app_i.h"

#include <furi.h>
#include <furi_hal.h>

static bool ac_remote_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    AC_RemoteApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ac_remote_app_back_event_callback(void* context) {
    furi_assert(context);
    AC_RemoteApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void ac_remote_app_tick_event_callback(void* context) {
    furi_assert(context);
    AC_RemoteApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static bool ac_remote_load_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* header = furi_string_alloc();

    uint32_t version = 0;
    bool success = false;
    do {
        if(!flipper_format_buffered_file_open_existing(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_read_header(ff, header, &version)) break;
        if(!furi_string_equal(header, "AC Remote") || (version != 1)) break;
        if(!flipper_format_read_uint32(ff, "Power", &app_state->power, 1)) break;
        if(!flipper_format_read_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(app_state->mode >= MODE_BUTTON_STATE_MAX) break;
        if(!flipper_format_read_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(app_state->temperature > HVAC_HITACHI_TEMPERATURE_MAX ||
           app_state->temperature < HVAC_HITACHI_TEMPERATURE_MIN)
            break;
        if(!flipper_format_read_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(app_state->fan >= FAN_SPEED_BUTTON_STATE_MAX) break;
        if(!flipper_format_read_uint32(ff, "Vane", &app_state->vane, 1)) break;
        if(app_state->vane > VANE_BUTTON_STATE_MAX) break;
        if(!flipper_format_read_uint32(ff, "TimerState", &app_state->timer_state, 1)) break;
        if(app_state->timer_state >= TIMER_STATE_COUNT) break;
        if(!flipper_format_read_uint32(ff, "TimerPresetOn", &app_state->timer_preset.on, 1)) break;
        if(app_state->timer_preset.on > 0xfff) break;
        if(!flipper_format_read_uint32(ff, "TimerPresetOff", &app_state->timer_preset.off, 1))
            break;
        if(app_state->timer_preset.off > 0xfff) break;
        if(!flipper_format_read_uint32(ff, "TimerPauseOn", &app_state->timer_pause.on, 1)) break;
        if(app_state->timer_pause.on > 0xfff) break;
        if(!flipper_format_read_uint32(ff, "TimerPauseOff", &app_state->timer_pause.off, 1)) break;
        if(app_state->timer_pause.off > 0xfff) break;
        if(!flipper_format_read_uint32(ff, "TimerOnExpiresAt", &app_state->timer_on_expires_at, 1))
            break;
        if(!flipper_format_read_uint32(
               ff, "TimerOffExpiresAt", &app_state->timer_off_expires_at, 1))
            break;
        if(!flipper_format_read_uint32(ff, "Side", &app_state->side, 1)) break;
        if(app_state->side > SETTINGS_SIDE_COUNT) break;
        if(!flipper_format_read_uint32(ff, "TimerStep", &app_state->timer_step, 1)) break;
        if(app_state->side > SETTINGS_TIMER_STEP_COUNT) break;
        if(!flipper_format_read_bool(ff, "AllowAuto", &app_state->allow_auto, 1)) break;
        success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(header);
    flipper_format_free(ff);
    return success;
}

static bool ac_remote_store_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool success = false;
    do {
        if(!flipper_format_file_open_always(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_write_header_cstr(ff, "AC Remote", 1)) break;
        if(!flipper_format_write_comment_cstr(ff, "")) break;
        if(!flipper_format_write_uint32(ff, "Power", &app_state->power, 1)) break;
        if(!flipper_format_write_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(!flipper_format_write_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(!flipper_format_write_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(!flipper_format_write_uint32(ff, "Vane", &app_state->vane, 1)) break;
        if(!flipper_format_write_uint32(ff, "TimerState", &app_state->timer_state, 1)) break;
        if(!flipper_format_write_uint32(ff, "TimerPresetOn", &app_state->timer_preset.on, 1))
            break;
        if(!flipper_format_write_uint32(ff, "TimerPresetOff", &app_state->timer_preset.off, 1))
            break;
        if(!flipper_format_write_uint32(ff, "TimerPauseOn", &app_state->timer_pause.on, 1)) break;
        if(!flipper_format_write_uint32(ff, "TimerPauseOff", &app_state->timer_pause.off, 1))
            break;
        if(!flipper_format_write_uint32(ff, "TimerOnExpiresAt", &app_state->timer_on_expires_at, 1))
            break;
        if(!flipper_format_write_uint32(
               ff, "TimerOffExpiresAt", &app_state->timer_off_expires_at, 1))
            break;
        if(!flipper_format_write_comment_cstr(ff, "")) break;
        if(!flipper_format_write_uint32(ff, "Side", &app_state->side, 1)) break;
        if(!flipper_format_write_uint32(ff, "TimerStep", &app_state->timer_step, 1)) break;
        if(!flipper_format_write_bool(ff, "AllowAuto", &app_state->allow_auto, 1)) break;
        success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    flipper_format_free(ff);
    return success;
}

void ac_remote_reset_settings(AC_RemoteApp* const app) {
    furi_assert(app);

    memset(&app->app_state, 0, sizeof(app->app_state));
    app->app_state.power = PowerButtonOff;
    app->app_state.mode = ModeButtonCooling;
    app->app_state.fan = FanSpeedButtonLow;
    app->app_state.vane = VaneButtonPos0;
    app->app_state.temperature = 23;
    app->app_state.timer_step = SettingsTimerStep30min;
}

AC_RemoteApp* ac_remote_app_alloc() {
    AC_RemoteApp* app = malloc(sizeof(AC_RemoteApp));

    app->gui = furi_record_open(RECORD_GUI);

    app->scene_manager = scene_manager_alloc(&ac_remote_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ac_remote_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ac_remote_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, ac_remote_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->panel_main = ac_remote_panel_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AC_RemoteAppViewMain, ac_remote_panel_get_view(app->panel_main));

    app->panel_sub = ac_remote_panel_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AC_RemoteAppViewSub, ac_remote_panel_get_view(app->panel_sub));

    app->vil_settings = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        AC_RemoteAppViewSettings,
        variable_item_list_get_view(app->vil_settings));

    app->dex_reset_confirm = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        AC_RemoteAppViewResetConfirm,
        dialog_ex_get_view(app->dex_reset_confirm));

    if(!ac_remote_load_settings(&app->app_state)) {
        ac_remote_reset_settings(app);
    }

    scene_manager_next_scene(app->scene_manager, AC_RemoteSceneHitachi);
    return app;
}

void ac_remote_app_free(AC_RemoteApp* app) {
    furi_assert(app);

    ac_remote_store_settings(&app->app_state);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, AC_RemoteAppViewResetConfirm);
    view_dispatcher_remove_view(app->view_dispatcher, AC_RemoteAppViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, AC_RemoteAppViewSub);
    view_dispatcher_remove_view(app->view_dispatcher, AC_RemoteAppViewMain);

    // View dispatcher
    dialog_ex_free(app->dex_reset_confirm);
    variable_item_list_free(app->vil_settings);
    ac_remote_panel_free(app->panel_sub);
    ac_remote_panel_free(app->panel_main);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t ac_remote_app(void* p) {
    UNUSED(p);
    AC_RemoteApp* ac_remote_app = ac_remote_app_alloc();
    view_dispatcher_run(ac_remote_app->view_dispatcher);
    ac_remote_app_free(ac_remote_app);
    return 0;
}

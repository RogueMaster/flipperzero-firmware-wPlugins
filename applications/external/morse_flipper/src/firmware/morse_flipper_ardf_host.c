#ifdef MF_ARDF_HOST_TEST
#include "morse_flipper_ardf_host_test.h"
#else
#include "morse_flipper_app_i.h"
#endif

#define MORSE_FLIPPER_ARDF_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_ardf.fal")

static bool morse_flipper_ardf_command(
    MorseFlipperApp* app,
    MfArdfCommand command,
    const void* input,
    uint32_t now_ms,
    void* output,
    MorseFlipperMappedFalResult* result) {
    return morse_flipper_plugin_runtime_call(
        app, MorseFlipperPluginOwnerArdf, command, input, output, now_ms, result);
}

static void morse_flipper_ardf_gpio_takeover(MorseFlipperApp* app) {
    if(app->ardf_gpio_owned) return;
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_ext_pc1, false);
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_ext_pc0, false);
    app->ardf_gpio_owned = true;
}

static void morse_flipper_ardf_gpio_release(MorseFlipperApp* app) {
    if(!app->ardf_gpio_owned) return;
    furi_hal_gpio_write(&gpio_ext_pc1, false);
    furi_hal_gpio_write(&gpio_ext_pc0, false);
    app->ardf_gpio_owned = false;
    app->signal_led_on = false;
    app->signal_led_red = false;
    app->signal_led_green = false;
    morse_flipper_gpio_apply(app);
}

static void morse_flipper_ardf_host_apply_after_unlock(
    MorseFlipperApp* app,
    MorseFlipperMappedFalResult result) {
    if(result.backlight_wake && app->notifications != NULL) {
        notification_message(app->notifications, &sequence_display_backlight_on);
        app->ardf_backlight_wake_active = true;
    }
    if(result.backlight_off && app->ardf_backlight_wake_active && app->notifications != NULL) {
        notification_message(app->notifications, &sequence_display_backlight_off);
        app->ardf_backlight_wake_active = false;
    }
    morse_flipper_update_sidetone(app);
    if(result.redraw) morse_flipper_view_dirty(app);
}

static void morse_flipper_ardf_text_callback(void* context) {
    MorseFlipperApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MorseFlipperCustomArdfTextDone);
}

bool morse_flipper_ardf_host_open(MorseFlipperApp* app, uint32_t now_ms) {
    MfArdfEnterArgs args;
    MorseFlipperMappedFalResult initial = {0};
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    args = (MfArdfEnterArgs){
        .struct_size = sizeof(args), .now_ms = now_ms, .frequency_hz = app->rf_frequency_hz};
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    bool opened = morse_flipper_plugin_runtime_open_mapped_locked(
        app,
        MorseFlipperPluginOwnerArdf,
        0U,
        MORSE_FLIPPER_ARDF_PLUGIN_PATH,
        MF_ARDF_API_VERSION,
        MF_ARDF_API_MAGIC,
        sizeof(MfArdfApi),
        &args,
        &initial);
    if(opened)
        morse_flipper_plugin_runtime_apply_result_locked(app, initial, now_ms);
    else
        morse_flipper_plugin_runtime_release_claim_locked(app, MorseFlipperPluginOwnerArdf);
    furi_mutex_release(app->plugin_slot.mutex);
    if(opened) morse_flipper_ardf_host_apply_after_unlock(app, initial);
    return opened;
}

static bool morse_flipper_ardf_host_populate_settings(MorseFlipperApp* app) {
    MorseFlipperMappedFalResult result;
    return morse_flipper_plugin_runtime_call(
               app,
               MorseFlipperPluginOwnerArdf,
               MfArdfCommandPopulateSettings,
               app->settings_list,
               NULL,
               furi_get_tick(),
               &result) &&
           result.handled;
}

static void morse_flipper_ardf_route(
    MorseFlipperApp* app,
    MorseFlipperMappedFalResult* state,
    uint32_t now_ms) {
    MorseFlipperMappedFalResult result;
    morse_flipper_ardf_host_apply_after_unlock(app, *state);
    if(state->feedback >= MfArdfHostActionOpenTextInput &&
       state->feedback <= MfArdfHostActionShowError) {
        MfArdfHostActionInfo info = {0};
        if(!morse_flipper_ardf_command(
               app, MfArdfCommandHostActionInfo, NULL, now_ms, &info, &result) ||
           !result.handled)
            return;
        if(state->feedback == MfArdfHostActionOpenTextInput) {
            if(app->ardf_view == MORSE_FLIPPER_ARDF_VIEW_TEXT) return;
            morse_flipper_ensure_view(app, MorseFlipperViewTextInput);
            text_input_reset(app->text_input);
            strlcpy(app->ardf_text, info.text, sizeof(app->ardf_text));
            text_input_set_header_text(app->text_input, info.header);
            text_input_set_result_callback(
                app->text_input,
                morse_flipper_ardf_text_callback,
                app,
                app->ardf_text,
                sizeof(app->ardf_text),
                false);
            app->ardf_view = MORSE_FLIPPER_ARDF_VIEW_TEXT;
            view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewTextInput);
            return;
        }
        MfArdfHostActionResultCommand action = {
            .action = state->feedback,
            .accepted = morse_flipper_host_dialog(app, &info),
        };
        morse_flipper_ardf_command(
            app, MfArdfCommandHostActionResult, &action, now_ms, NULL, &result);
        morse_flipper_ardf_host_apply_after_unlock(app, result);
        state = &result;
    } else if(state->feedback == MfArdfHostActionCloseToRadio) {
        morse_flipper_ardf_host_close(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuRf);
        return;
    }
    if(state->phase == MfArdfViewSettings) morse_flipper_ardf_gpio_release(app);
    if(app->ardf_view == state->phase) return;
    app->ardf_view = state->phase;
    if(state->phase == MfArdfViewSettings) {
        morse_flipper_ensure_view(app, MorseFlipperViewSettings);
        (void)morse_flipper_ardf_host_populate_settings(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewSettings);
    } else {
        morse_flipper_ensure_view(app, MorseFlipperViewLive);
        view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewLive);
        morse_flipper_view_dirty(app);
    }
}

static bool morse_flipper_ardf_host_step(
    MorseFlipperApp* app,
    uint32_t operation,
    const InputEvent* event,
    uint32_t now_ms) {
    MorseFlipperMappedFalResult result = {0};
    bool called = morse_flipper_plugin_runtime_call(
        app, MorseFlipperPluginOwnerArdf, operation, event, NULL, now_ms, &result);
    if(called && result.transition) {
        morse_flipper_ardf_gpio_takeover(app);
        called =
            morse_flipper_ardf_command(app, MfArdfCommandActivateRun, NULL, now_ms, NULL, &result);
    }
    if(called) morse_flipper_ardf_route(app, &result, now_ms);
    return called ? result.handled : false;
}

bool morse_flipper_ardf_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    if(event == NULL) return false;
    return morse_flipper_ardf_host_step(app, MORSE_FLIPPER_MAPPED_INPUT, event, now_ms);
}

void morse_flipper_ardf_host_tick(MorseFlipperApp* app, uint32_t now_ms) {
    if(app->ardf_view == MORSE_FLIPPER_ARDF_VIEW_TEXT) return;
    (void)morse_flipper_ardf_host_step(app, MORSE_FLIPPER_MAPPED_TICK, NULL, now_ms);
}

bool morse_flipper_ardf_host_text_result(
    MorseFlipperApp* app,
    const char* text,
    bool accepted,
    uint32_t now_ms) {
    MorseFlipperMappedFalResult result = {0};
    MfArdfTextResultCommand command = {.text = text, .accepted = accepted};
    bool called =
        morse_flipper_ardf_command(app, MfArdfCommandTextResult, &command, now_ms, NULL, &result);
    if(called) {
        app->ardf_view = MORSE_FLIPPER_ARDF_VIEW_NONE;
        morse_flipper_ardf_route(app, &result, now_ms);
    }
    return called;
}

void morse_flipper_ardf_host_close(void* context) {
    MorseFlipperApp* app = context;
    if(app == NULL || app->plugin_slot.mutex == NULL) return;
    morse_flipper_plugin_runtime_unload_current(app);
    morse_flipper_audio_pwm_stop(&app->audio_pwm);
    furi_hal_vibro_on(false);
    morse_flipper_ardf_gpio_release(app);
    if(app->ardf_backlight_wake_active && app->notifications != NULL) {
        notification_message(app->notifications, &sequence_display_backlight_off);
        app->ardf_backlight_wake_active = false;
    }
    app->ardf_view = MORSE_FLIPPER_ARDF_VIEW_NONE;
}

#include "morse_flipper_app_i.h"

#define MORSE_FLIPPER_RX_PRACTICE_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_rx_practice.fal")
#define MF_RX_START_EXTERNAL \
    (MF_RX_START_STRAIGHT | MF_RX_START_DIT | MF_RX_START_DAH)

static char mf_rx_answer_preview(void* context) {
    MorseFlipperApp* app = context;
    char preview;

    if(app == NULL) return '\0';
    preview =
        (char)morse_flipper_live_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));
    return preview == ' ' || preview == '|' ? '\0' : preview;
}

static void mf_rx_apply_locked(
    MorseFlipperApp* app,
    MfRxPracticeResult result,
    uint32_t now_ms) {
    result.playback_active = result.phase == MfRxPracticePhasePlayback;
    morse_flipper_plugin_runtime_apply_result_locked(app, result, now_ms);
    if(result.redraw) morse_flipper_view_dirty(app);
}

static void mf_rx_apply_after_unlock(
    MorseFlipperApp* app,
    MfRxPracticeResult result,
    uint32_t now_ms) {
    if(!result.decoder_reset) return;
    morse_flipper_reset_answer_decoder(app);
    if(result.phase != MfRxPracticePhaseAnswer) {
        morse_flipper_drop_live_keying_for_playback(app, now_ms);
        morse_flipper_release_all_notes(app);
    }
    morse_flipper_update_sidetone(app);
}

bool morse_flipper_rx_practice_host_enter(MorseFlipperApp* app, uint32_t now_ms) {
    MfRxPracticeResult initial = {0};
    MfRxPracticeEnterArgs args;
    bool entered;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    args = (MfRxPracticeEnterArgs){
        .struct_size = sizeof(args),
        .now_ms = now_ms,
        .rng_seed = furi_hal_random_get(),
        .answer_timeout_ms =
            (uint32_t)(app->trainer_answer_timeout_s == 0U ?
                           MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S :
                           app->trainer_answer_timeout_s) *
            1000U,
        .result_hold_ms = 3000U,
        .dit_ms = morse_flipper_current_dit_ms(app),
        .char_gap_ms = morse_flipper_training_char_gap_ms(
            morse_flipper_current_dit_ms(app),
            morse_flipper_local_wpm(app),
            app->trainer_farnsworth_wpm),
        .physical_key_can_start =
            app->input_source != MorseFlipperInputSourceButtons,
        .draw_services = {
            .context = app,
            .answer_preview = mf_rx_answer_preview,
        },
    };
    entered = morse_flipper_plugin_runtime_open_mapped_locked(
        app,
        MorseFlipperPluginOwnerRxPractice,
        0U,
        MORSE_FLIPPER_RX_PRACTICE_PLUGIN_PATH,
        MORSE_FLIPPER_RX_PRACTICE_API_VERSION,
        MORSE_FLIPPER_RX_PRACTICE_API_MAGIC,
        sizeof(MfRxPracticeApi),
        &args,
        &initial);
    if(entered) {
        app->session_result_tone = false;
        app->session_result_good = false;
        app->session_result_until = 0U;
        mf_rx_apply_locked(app, initial, now_ms);
    }
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    morse_flipper_release_all_notes(app);
    morse_flipper_reset_answer_decoder(app);
    morse_flipper_update_sidetone(app);
    return entered;
}

bool morse_flipper_rx_practice_host_input(
    MorseFlipperApp* app,
    const InputEvent* event,
    uint32_t now_ms) {
    MfRxPracticeResult result = {0};
    uint8_t hold_bit = 0U;
    bool button_paddle;
    if(app == NULL || event == NULL || app->plugin_slot.mutex == NULL) return false;
    button_paddle = app->input_source == MorseFlipperInputSourceButtons &&
                    !morse_flipper_straight_like_mode(app);
    if(event->key == InputKeyOk)
        hold_bit = MF_RX_START_OK;
    else if(event->key == InputKeyBack)
        hold_bit = MF_RX_START_BACK;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRxPractice) {
        if(event->type == InputTypePress)
            app->plugin_slot.start_hold_mask |= hold_bit;
        else if(event->type == InputTypeRelease)
            app->plugin_slot.start_hold_mask &= (uint8_t)~hold_bit;
    }
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRxPractice &&
       app->plugin_slot.error == MorseFlipperPluginErrorNone &&
       app->plugin_slot.api != NULL && app->plugin_slot.state != NULL) {
        result = ((const MfRxPracticeApi*)app->plugin_slot.api)
                     ->input(app->plugin_slot.state, event, button_paddle, now_ms);
        mf_rx_apply_locked(app, result, now_ms);
    }
    furi_mutex_release(app->plugin_slot.mutex);
    mf_rx_apply_after_unlock(app, result, now_ms);
    if(result.request_exit)
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuTraining);
    else if(!result.handled && event->key == InputKeyBack &&
            (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_plugin_runtime_unload_current(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuTraining);
    }
    return true;
}

bool morse_flipper_rx_practice_host_feed(
    MorseFlipperApp* app,
    const char* text,
    size_t length,
    uint32_t now_ms) {
    MfRxPracticeResult result = {0};
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRxPractice &&
       app->plugin_slot.error == MorseFlipperPluginErrorNone &&
       app->plugin_slot.api != NULL && app->plugin_slot.state != NULL) {
        result = ((const MfRxPracticeApi*)app->plugin_slot.api)
                     ->feed_text(app->plugin_slot.state, text, length, now_ms);
        mf_rx_apply_locked(app, result, now_ms);
    }
    furi_mutex_release(app->plugin_slot.mutex);
    mf_rx_apply_after_unlock(app, result, now_ms);
    return result.decoder_reset;
}

bool morse_flipper_rx_practice_host_tick(
    MorseFlipperApp* app,
    uint32_t now_ms,
    uint8_t down_mask) {
    MfRxPracticeResult result = {0};
    MfRxPracticeCommand command = MfRxPracticeCommandNone;
    uint8_t old_mask;
    uint8_t new_down;
    bool live = false;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    down_mask &= MF_RX_START_EXTERNAL;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner != MorseFlipperPluginOwnerRxPractice ||
       app->plugin_slot.error != MorseFlipperPluginErrorNone ||
       app->plugin_slot.api == NULL || app->plugin_slot.state == NULL)
        goto done;
    morse_flipper_plugin_feedback_expire_locked(app, now_ms);
    old_mask = app->plugin_slot.start_hold_mask;
    new_down = down_mask & (uint8_t)~old_mask;
    app->plugin_slot.start_hold_mask &=
        (uint8_t)~(MF_RX_START_EXTERNAL & (uint8_t)~down_mask);
    if(app->plugin_slot.phase == MfRxPracticePhaseIdle && new_down != 0U) {
        app->plugin_slot.start_hold_mask |= down_mask;
        command = MfRxPracticeCommandStart;
    } else if(app->plugin_slot.phase == MfRxPracticePhasePlayback) {
        app->plugin_slot.start_hold_mask |= down_mask;
    } else if(app->plugin_slot.phase == MfRxPracticePhaseAnswer &&
              app->plugin_slot.start_hold_mask != 0U) {
        app->plugin_slot.start_hold_mask |= down_mask;
    } else if(app->plugin_slot.phase == MfRxPracticePhaseResult &&
              new_down != 0U) {
        app->plugin_slot.start_hold_mask |= down_mask;
        command = MfRxPracticeCommandHurry;
    }
    if(command == MfRxPracticeCommandNone)
        morse_flipper_plugin_runtime_tick_locked(
            app, MorseFlipperPluginOwnerRxPractice, now_ms, &result);
    else
        result = ((const MfRxPracticeApi*)app->plugin_slot.api)
                     ->command(app->plugin_slot.state, command, now_ms);
    mf_rx_apply_locked(app, result, now_ms);
    if(app->plugin_slot.phase == MfRxPracticePhaseAnswer &&
       app->plugin_slot.start_hold_mask == 0U)
        live = true;
done:
    furi_mutex_release(app->plugin_slot.mutex);
    mf_rx_apply_after_unlock(app, result, now_ms);
    return live;
}

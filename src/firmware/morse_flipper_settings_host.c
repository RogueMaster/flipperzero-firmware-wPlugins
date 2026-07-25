#ifdef MF_SETTINGS_HOST_TEST
#include "morse_flipper_settings_host_test.h"
#else
#include "morse_flipper_app_i.h"
#endif

#define MORSE_FLIPPER_SETTINGS_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_settings.fal")

static void mf_settings_snapshot(MorseFlipperApp* app, MfSettingsSnapshot* snapshot) {
    *snapshot = (MfSettingsSnapshot){
        .local_wpm = morse_flipper_current_wpm(app),
        .input_source = app->input_source,
        .keyer_mode = app->keyer_mode,
        .handedness = app->handedness == MorseFlipperHandednessSwapped,
        .audio_path = app->audio_path == MorseFlipperAudioPathSoftBuzz ? 0U : app->audio_path,
        .tone_index = app->tone_idx,
        .p2_volume = morse_flipper_p2_volume_pct(app),
        .audio_waveform = app->audio_path == MorseFlipperAudioPathBuzzer ? 0U : 1U,
        .lesson = app->listening_settings.lesson,
        .farnsworth_wpm = app->listening_settings.farnsworth_wpm,
        .answer_timeout_s = app->listening_settings.answer_timeout_s,
        .group_pause_s = app->listening_settings.group_pause_s,
        .group_size = app->listening_settings.group_size,
        .group_count = app->listening_settings.session_groups,
        .custom_set_idx = app->listening_settings.custom_set_idx,
        .straight_wpm = morse_flipper_straight_wpm(app),
        .straight_answer_timeout_s = app->straight_answer_timeout_s,
        .straight_next_delay_s = app->straight_next_delay_s,
        .tx_groups_difficulty = app->txg_difficulty,
        .gpio_dit_pin = app->gpio_dit_idx,
        .gpio_dah_pin = app->gpio_dah_idx,
        .gpio_ground_pin = app->gpio_ground_idx,
        .gpio_ptt_pin = app->gpio_ptt_idx,
        .usb_mode = app->pc_mode_pref,
        .usb_paddle_preset = app->pc_paddle_preset,
        .usb_straight_preset = app->pc_straight_preset,
        .usb_mouse_invert = app->mouse_invert,
    };
}

static bool mf_settings_apply(void* context, const MfSettingsRequest* request, MfSettingsResponse* response) {
    MorseFlipperApp* app = context;
    uint32_t now_ms = furi_get_tick();
    bool changed = false;

    if(response != NULL) *response = (MfSettingsResponse){0};
    if(app == NULL || request == NULL || response == NULL) return false;
    switch(request->kind) {
    case MfSettingsSetLocalWpm:
        if(request->value < 10U || request->value > 30U) return true;
        morse_flipper_set_local_wpm(app, (uint8_t)request->value);
        changed = true;
        break;
    case MfSettingsSetInputSource:
        if(request->value > MorseFlipperInputSourcePaddle) return true;
        app->input_source = (uint8_t)request->value;
        morse_flipper_clear_button_keying(app, now_ms);
        morse_flipper_refresh_keyer(app, now_ms);
        morse_flipper_poll(app);
        changed = true;
        break;
    case MfSettingsSetKeyerMode:
        if(request->value > MorseKeyerModeKeyahead) return true;
        app->keyer_mode = (uint8_t)request->value;
        morse_flipper_refresh_keyer(app, now_ms);
        morse_flipper_poll(app);
        changed = true;
        break;
    case MfSettingsSetHandedness:
        if(request->value > 1U) return true;
        app->handedness = request->value ? MorseFlipperHandednessSwapped : MorseFlipperHandednessNormal;
        morse_flipper_resync_button_paddles(app, now_ms);
        morse_flipper_refresh_keyer(app, now_ms);
        morse_flipper_poll(app);
        changed = true;
        break;
    case MfSettingsSetAudioPath:
        if(request->value > MorseFlipperAudioPathVibration) return true;
        if(request->value == MorseFlipperAudioPathBuzzer)
            app->audio_path = app->audio_path == MorseFlipperAudioPathBuzzer ?
                                  MorseFlipperAudioPathBuzzer : MorseFlipperAudioPathSoftBuzz;
        else
            app->audio_path = (uint8_t)request->value;
        morse_flipper_sync_audio_output(app);
        changed = true;
        break;
    case MfSettingsSetTone:
        if(request->value >= COUNT_OF(morse_flipper_tones) || app->audio_path == MorseFlipperAudioPathVibration)
            return true;
        app->tone_idx = (uint8_t)request->value;
        app->preview_ticks = MORSE_FLIPPER_PREVIEW_TICKS;
        morse_flipper_update_sidetone(app);
        changed = true;
        break;
    case MfSettingsSetP2Volume:
        if(request->value < 10U || request->value > 100U || request->value % 5U != 0U) return true;
        app->p2_volume_pct = (uint8_t)request->value;
        if(morse_flipper_audio_output_is_pwm(app)) morse_flipper_sync_audio_output(app);
        changed = true;
        break;
    case MfSettingsSetAudioWaveform:
        if(request->value > 1U) return true;
        app->audio_path = request->value ? MorseFlipperAudioPathSoftBuzz : MorseFlipperAudioPathBuzzer;
        morse_flipper_sync_audio_output(app);
        changed = true;
        break;
    case MfSettingsSetListeningLesson:
        if(request->value == 0U || request->value > morse_trainer_lesson_count()) return true;
        app->listening_settings.lesson = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetListeningFarnsworth:
        if(request->value == 0U || request->value > morse_flipper_local_wpm(app)) return true;
        app->listening_settings.farnsworth_wpm = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetListeningAnswerTimeout:
        if(request->value < MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S || request->value > MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S) return true;
        app->listening_settings.answer_timeout_s = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetListeningGroupPause:
        if(request->value < MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S || request->value > MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S) return true;
        app->listening_settings.group_pause_s = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetListeningGroupSize:
        if(request->value < 1U || request->value > 9U) return true;
        app->listening_settings.group_size = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetListeningGroupCount:
        if(request->value < 3U || request->value > 30U) return true;
        app->listening_settings.session_groups = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetListeningCustomSet:
        if(request->value > MORSE_TRAINER_CUSTOM_SET_CAP) return true;
        app->listening_settings.custom_set_idx = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetStraightWpm:
        if(request->value < 10U || request->value > 30U) return true;
        morse_flipper_set_straight_wpm(app, (uint8_t)request->value);
        changed = true;
        break;
    case MfSettingsSetStraightAnswerTimeout:
        if(request->value < MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S || request->value > MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S) return true;
        app->straight_answer_timeout_s = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetStraightNextDelay:
        if(request->value < MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S || request->value > MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S) return true;
        app->straight_next_delay_s = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetTxGroupsDifficulty:
        if(request->value >= MorseFlipperTxgDifficultyCount) return true;
        app->txg_difficulty = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsApplyGpioDraft: {
        MorseFlipperGpioRule rule;
        if(!morse_flipper_gpio_try_apply(app, request->gpio_dit_pin, request->gpio_dah_pin, request->gpio_ground_pin, request->gpio_ptt_pin, &rule)) {
            response->error = (uint8_t)rule;
            return true;
        }
        mf_settings_snapshot(app, &response->snapshot);
        response->accepted = true;
        return true;
    }
    case MfSettingsSetUsbMode:
        if(request->value > MorseFlipperPcModeMidi) return true;
        app->pc_mode_pref = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetUsbPaddlePreset:
        if(request->value >= morse_pc_paddle_preset_count()) return true;
        app->pc_paddle_preset = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetUsbStraightPreset:
        if(request->value >= morse_pc_straight_preset_count()) return true;
        app->pc_straight_preset = (uint8_t)request->value;
        changed = true;
        break;
    case MfSettingsSetUsbMouseInvert:
        if(request->value > 1U) return true;
        app->mouse_invert = request->value != 0U;
        changed = true;
        break;
    default: return true;
    }
    if(changed) morse_flipper_save_config(app);
    mf_settings_snapshot(app, &response->snapshot);
    response->accepted = changed;
    return true;
}

static void mf_settings_post_navigate(void* context, uint32_t event) {
    MorseFlipperApp* app = context;
    if(app == NULL) return;
    if(event == MfSettingsNavigateAudio) view_dispatcher_send_custom_event(app->view_dispatcher, MorseFlipperSceneAudioCfg);
    if(event == MfSettingsNavigateGpio) view_dispatcher_send_custom_event(app->view_dispatcher, MorseFlipperSceneGpio);
}

bool morse_flipper_settings_host_enter(MorseFlipperApp* app, uint8_t entry, uint32_t selected_state) {
    MfSettingsSnapshot snapshot;
    MfSettingsHostServices services = {.struct_size = sizeof(services), .apply = mf_settings_apply, .post_navigate = mf_settings_post_navigate};
    MfSettingsEnterArgs args;
    MorseFlipperMappedFalResult initial = {0};
    bool entered;

    if(app == NULL || app->plugin_slot.mutex == NULL || entry > MfSettingsEntryUsb) return false;
    mf_settings_snapshot(app, &snapshot);
    args = (MfSettingsEnterArgs){.struct_size = sizeof(args), .entry = entry, .selected_state = selected_state,
        .list = app->settings_list, .snapshot = snapshot, .services = &services, .service_context = app};
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    entered = morse_flipper_plugin_runtime_open_mapped_locked(app, MorseFlipperPluginOwnerSettings, entry,
        MORSE_FLIPPER_SETTINGS_PLUGIN_PATH, MF_SETTINGS_API_VERSION, MF_SETTINGS_API_MAGIC,
        sizeof(MfSettingsApi), &args, &initial);
    furi_mutex_release(app->plugin_slot.mutex);
    return entered;
}

bool morse_flipper_settings_host_close(MorseFlipperApp* app, uint32_t scene) {
    MorseFlipperMappedFalResult result = {0};
    const MfSettingsApi* api;
    bool close = false;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerSettings && app->plugin_slot.api != NULL && app->plugin_slot.state != NULL) {
        api = app->plugin_slot.api;
        close = api->request_close(app->plugin_slot.state, &result);
        if(close) scene_manager_set_scene_state(app->scene_manager, scene, api->selected_state(app->plugin_slot.state));
    }
    furi_mutex_release(app->plugin_slot.mutex);
    if(!close && result.feedback != MorseFlipperGpioRuleOk)
        morse_flipper_gpio_alert(app, (MorseFlipperGpioRule)result.feedback);
    return close;
}

void morse_flipper_settings_host_leave(MorseFlipperApp* app, uint32_t scene) {
    if(app == NULL) return;
    if(app->preview_ticks != 0U) {
        app->preview_ticks = 0U;
        morse_flipper_update_sidetone(app);
    }
    if(app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerSettings)
        morse_flipper_plugin_runtime_detach_locked(app, MorseFlipperPluginOwnerSettings);
    furi_mutex_release(app->plugin_slot.mutex);
    UNUSED(scene);
}

static void mf_settings_scene_enter(MorseFlipperApp* app, uint8_t entry, uint32_t scene) {
    uint32_t selected = scene_manager_get_scene_state(app->scene_manager, scene);

    morse_flipper_ensure_view(app, MorseFlipperViewSettings);
    if(!morse_flipper_settings_host_enter(app, entry, selected)) {
        variable_item_list_reset(app->settings_list);
        variable_item_list_add(app->settings_list, "Settings unavailable", 0U, NULL, app);
    }
    morse_flipper_scene_enter_now(app, scene);
}

static bool mf_settings_scene_event(MorseFlipperApp* app, SceneManagerEvent event, uint32_t scene) {
    if(event.type != SceneManagerEventTypeBack) return false;
    if(app->plugin_slot.owner != MorseFlipperPluginOwnerSettings ||
       morse_flipper_settings_host_close(app, scene))
        morse_flipper_scene_back(app);
    return true;
}

static void mf_settings_scene_exit(MorseFlipperApp* app, uint32_t scene) {
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerSettings)
        morse_flipper_settings_host_leave(app, scene);
    else
        variable_item_list_reset(app->settings_list);
}

void morse_flipper_scene_home_on_enter(void* context) {
    mf_settings_scene_enter(context, MfSettingsEntryKeying, MorseFlipperSceneHome);
}


void morse_flipper_scene_audio_cfg_on_enter(void* context) {
    mf_settings_scene_enter(context, MfSettingsEntryAudio, MorseFlipperSceneAudioCfg);
}


void morse_flipper_scene_pc_on_enter(void* context) {
    mf_settings_scene_enter(context, MfSettingsEntryUsb, MorseFlipperScenePc);
}

void morse_flipper_scene_settings_listening_on_enter(void* context) { mf_settings_scene_enter(context, MfSettingsEntryListening, MorseFlipperSceneTrainer); }
void morse_flipper_scene_settings_straight_on_enter(void* context) { mf_settings_scene_enter(context, MfSettingsEntryStraight, MorseFlipperSceneStraightCfg); }
void morse_flipper_scene_settings_tx_groups_on_enter(void* context) { mf_settings_scene_enter(context, MfSettingsEntryTxGroups, MorseFlipperSceneTxGroupsCfg); }
void morse_flipper_scene_settings_gpio_on_enter(void* context) { mf_settings_scene_enter(context, MfSettingsEntryGpio, MorseFlipperSceneGpio); }

bool morse_flipper_scene_settings_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;
    return app != NULL && mf_settings_scene_event(app, event, app->scene);
}

void morse_flipper_scene_settings_on_exit(void* context) {
    MorseFlipperApp* app = context;
    if(app != NULL) mf_settings_scene_exit(app, app->scene);
}

#ifdef MF_SETTINGS_HOST_TEST
bool mf_settings_host_test_apply(
    MorseFlipperApp* app,
    const MfSettingsRequest* request,
    MfSettingsResponse* response) {
    return mf_settings_apply(app, request, response);
}

void mf_settings_host_test_navigate(MorseFlipperApp* app, uint32_t event) {
    mf_settings_post_navigate(app, event);
}
#endif

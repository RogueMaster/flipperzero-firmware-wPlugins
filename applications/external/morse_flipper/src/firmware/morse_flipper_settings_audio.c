/*
 * Purpose: Handle tone, output path, PWM volume, and buzzer waveform settings rows.
 * Owns: audio setting callbacks and preview/persistence side effects.
 * Depends on: morse_flipper_app_i.h and audio route/PWM state.
 * Tests: firmware build; settings UI is hardware-only.
 */

#include "morse_flipper_app_i.h"

#define MORSE_FLIPPER_AUDIO_PATH_VISIBLE_COUNT 3U
#define MORSE_FLIPPER_AUDIO_WAVEFORM_COUNT     2U
#define MORSE_FLIPPER_AUDIO_WAVEFORM_SQUARE    0U
#define MORSE_FLIPPER_AUDIO_WAVEFORM_SINE      1U

static const char* const morse_flipper_audio_waveform_names[] = {
    "Square",
    "Sine",
};

static void morse_flipper_audio_menu_refresh(MorseFlipperApp* app);
static void morse_flipper_audio_menu_rebuild(MorseFlipperApp* app, uint8_t selected);
static void morse_flipper_settings_audio_path_changed(VariableItem* item);
static void morse_flipper_settings_p2_volume_changed(VariableItem* item);
static void morse_flipper_settings_waveform_changed(VariableItem* item);

static bool morse_flipper_audio_path_uses_waveform(uint8_t audio_path) {
    return audio_path == MorseFlipperAudioPathBuzzer ||
           audio_path == MorseFlipperAudioPathSoftBuzz;
}

static uint8_t morse_flipper_audio_path_visible_idx(uint8_t audio_path) {
    if(audio_path == MorseFlipperAudioPathSoftBuzz) return MorseFlipperAudioPathBuzzer;
    if(audio_path < MORSE_FLIPPER_AUDIO_PATH_VISIBLE_COUNT) return audio_path;
    return MorseFlipperAudioPathBuzzer;
}

static uint8_t morse_flipper_audio_waveform_idx(uint8_t audio_path) {
    return audio_path == MorseFlipperAudioPathBuzzer ? MORSE_FLIPPER_AUDIO_WAVEFORM_SQUARE :
                                                       MORSE_FLIPPER_AUDIO_WAVEFORM_SINE;
}

void morse_flipper_settings_tone_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(app->audio_path == MorseFlipperAudioPathVibration) {
        variable_item_set_current_value_index(item, 0U);
        variable_item_set_current_value_text(item, "n/a");
        return;
    }

    app->tone_idx = idx < COUNT_OF(morse_flipper_tones) ? idx : MORSE_FLIPPER_DEFAULT_TONE_IDX;
    variable_item_set_current_value_text(item, morse_flipper_current_tone_name(app));
    app->preview_ticks = MORSE_FLIPPER_PREVIEW_TICKS;

    morse_flipper_update_sidetone(app);
    morse_flipper_save_config(app);
}

static uint8_t morse_flipper_p2_volume_idx(uint8_t pct) {
    if(pct < 10U) pct = 10U;
    if(pct > 100U) pct = 100U;
    return (uint8_t)((pct - 10U) / 5U);
}

static uint8_t morse_flipper_p2_volume_from_idx(uint8_t idx) {
    return (uint8_t)(10U + (idx * 5U));
}

static void morse_flipper_audio_menu_refresh(MorseFlipperApp* app) {
    VariableItem* item;
    char txt[8];

    if(app == NULL) return;

    item = app->audio_cfg_items[MorseFlipperAudioSettingPath];
    if(item) {
        if(app->audio_path >= MorseFlipperAudioPathCount)
            app->audio_path = MorseFlipperAudioPathSoftBuzz;
        uint8_t idx = morse_flipper_audio_path_visible_idx(app->audio_path);
        variable_item_set_current_value_index(item, idx);
        variable_item_set_current_value_text(item, morse_flipper_audio_path_names[idx]);
    }

    item = app->audio_cfg_items[MorseFlipperAudioSettingTone];
    if(item) {
        if(app->audio_path == MorseFlipperAudioPathVibration) {
            variable_item_set_values_count(item, 1U);
            variable_item_set_current_value_index(item, 0U);
            variable_item_set_current_value_text(item, "n/a");
        } else {
            variable_item_set_values_count(item, COUNT_OF(morse_flipper_tones));
            variable_item_set_current_value_index(
                item,
                app->tone_idx < COUNT_OF(morse_flipper_tones) ? app->tone_idx :
                                                                MORSE_FLIPPER_DEFAULT_TONE_IDX);
            variable_item_set_current_value_text(item, morse_flipper_current_tone_name(app));
        }
    }

    item = app->audio_cfg_items[MorseFlipperAudioSettingP2Volume];
    if(item) {
        variable_item_set_current_value_index(
            item, morse_flipper_p2_volume_idx(app->p2_volume_pct));
        snprintf(txt, sizeof(txt), "%u%%", (unsigned)morse_flipper_p2_volume_pct(app));
        variable_item_set_current_value_text(item, txt);
    }

    item = app->audio_cfg_items[MorseFlipperAudioSettingWaveform];
    if(item) {
        uint8_t idx = morse_flipper_audio_waveform_idx(app->audio_path);
        variable_item_set_current_value_index(item, idx);
        variable_item_set_current_value_text(item, morse_flipper_audio_waveform_names[idx]);
    }
}

static void morse_flipper_audio_menu_rebuild(MorseFlipperApp* app, uint8_t selected) {
    VariableItem* item;
    uint8_t max_selected = MorseFlipperAudioSettingP2Volume;

    if(app == NULL) return;

    variable_item_list_reset(app->settings_list);
    memset(app->audio_cfg_items, 0, sizeof(app->audio_cfg_items));
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);

    item = variable_item_list_add(
        app->settings_list,
        "Audio path",
        MORSE_FLIPPER_AUDIO_PATH_VISIBLE_COUNT,
        morse_flipper_settings_audio_path_changed,
        app);
    app->audio_cfg_items[MorseFlipperAudioSettingPath] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Frequency",
        COUNT_OF(morse_flipper_tones),
        morse_flipper_settings_tone_changed,
        app);
    app->audio_cfg_items[MorseFlipperAudioSettingTone] = item;

    item = variable_item_list_add(
        app->settings_list, "PWM Volume", 19U, morse_flipper_settings_p2_volume_changed, app);
    app->audio_cfg_items[MorseFlipperAudioSettingP2Volume] = item;

    if(morse_flipper_audio_path_uses_waveform(app->audio_path)) {
        item = variable_item_list_add(
            app->settings_list,
            "Waveform",
            MORSE_FLIPPER_AUDIO_WAVEFORM_COUNT,
            morse_flipper_settings_waveform_changed,
            app);
        app->audio_cfg_items[MorseFlipperAudioSettingWaveform] = item;
        max_selected = MorseFlipperAudioSettingWaveform;
    }

    if(selected > max_selected) selected = max_selected;
    morse_flipper_audio_menu_refresh(app);
    variable_item_list_set_selected_item(app->settings_list, selected);
}

static void morse_flipper_settings_audio_path_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx == MorseFlipperAudioPathBuzzer) {
        app->audio_path = morse_flipper_audio_path_uses_waveform(app->audio_path) ?
                              app->audio_path :
                              MorseFlipperAudioPathSoftBuzz;
    } else if(idx < MORSE_FLIPPER_AUDIO_PATH_VISIBLE_COUNT) {
        app->audio_path = idx;
    } else {
        app->audio_path = MorseFlipperAudioPathSoftBuzz;
    }

    morse_flipper_audio_menu_rebuild(app, MorseFlipperAudioSettingPath);
    morse_flipper_sync_audio_output(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_p2_volume_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    char txt[8];

    app->p2_volume_pct =
        morse_flipper_p2_volume_from_idx(variable_item_get_current_value_index(item));
    snprintf(txt, sizeof(txt), "%u%%", (unsigned)app->p2_volume_pct);
    variable_item_set_current_value_text(item, txt);
    if(morse_flipper_audio_output_is_pwm(app)) morse_flipper_sync_audio_output(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_waveform_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->audio_path = idx == MORSE_FLIPPER_AUDIO_WAVEFORM_SQUARE ? MorseFlipperAudioPathBuzzer :
                                                                   MorseFlipperAudioPathSoftBuzz;
    variable_item_set_current_value_text(
        item,
        morse_flipper_audio_waveform_names[morse_flipper_audio_waveform_idx(app->audio_path)]);
    morse_flipper_sync_audio_output(app);
    morse_flipper_save_config(app);
}

void morse_flipper_scene_audio_cfg_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneAudioCfg);

    morse_flipper_ensure_view(app, MorseFlipperViewSettings);
    morse_flipper_audio_menu_rebuild(app, (uint8_t)(sel & 0xffU));
    morse_flipper_scene_enter_now(app, MorseFlipperSceneAudioCfg);
}

bool morse_flipper_scene_audio_cfg_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

void morse_flipper_scene_audio_cfg_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneAudioCfg,
        morse_flipper_settings_list_state(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

#include "morse_flipper_settings_host_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned saves, refreshes, polls, audio_syncs, sidetones, events, opens, detaches;
static MfSettingsEnterArgs captured_args;
const int morse_flipper_tones[31];

uint32_t furi_get_tick(void) { return 1U; }
void furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout) { (void)mutex; (void)timeout; }
void furi_mutex_release(FuriMutex* mutex) { (void)mutex; }
uint8_t morse_flipper_current_wpm(const MorseFlipperApp* app) { return app->listening_settings.local_dit_ms ? 20U : 10U; }
uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp* app) { return app->p2_volume_pct; }
uint8_t morse_flipper_straight_wpm(const MorseFlipperApp* app) { (void)app; return 10U; }
uint8_t morse_trainer_group_size(const MorseTrainer* trainer) { return trainer->group_size; }
uint8_t morse_trainer_session_groups(const MorseTrainer* trainer) { return trainer->session_groups; }
void morse_flipper_set_local_wpm(MorseFlipperApp* app, uint8_t wpm) { app->listening_settings.local_dit_ms = wpm; }
void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now) { (void)app; (void)now; }
void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now) { (void)app; (void)now; refreshes++; }
void morse_flipper_poll(MorseFlipperApp* app) { (void)app; polls++; }
void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now) { (void)app; (void)now; }
void morse_flipper_sync_audio_output(MorseFlipperApp* app) { (void)app; audio_syncs++; }
void morse_flipper_update_sidetone(MorseFlipperApp* app) { (void)app; sidetones++; }
bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app) { return app->audio_path == MorseFlipperAudioPathP2; }
void morse_flipper_save_config(const MorseFlipperApp* app) { (void)app; saves++; }
uint8_t morse_pc_paddle_preset_count(void) { return 9U; }
uint8_t morse_pc_straight_preset_count(void) { return 8U; }
void view_dispatcher_send_custom_event(ViewDispatcher* dispatcher, uint32_t event) { (void)dispatcher; events = event; }
bool morse_flipper_plugin_runtime_open_mapped_locked(MorseFlipperApp* app, uint8_t owner, uint8_t mode, const char* path, uint32_t version, uint32_t magic, uint32_t size, const void* args, MorseFlipperMappedFalResult* initial) {
    (void)mode; (void)path; (void)version; (void)magic; (void)size; captured_args = *(const MfSettingsEnterArgs*)args; app->plugin_slot.owner = owner; opens++; *initial = (MorseFlipperMappedFalResult){0}; return true;
}
void morse_flipper_plugin_runtime_detach_locked(MorseFlipperApp* app, uint8_t owner) { assert(app->plugin_slot.owner == owner); app->plugin_slot.owner = 0U; detaches++; }
void scene_manager_set_scene_state(SceneManager* manager, uint32_t scene, uint32_t state) { manager->state[scene] = state; }
uint32_t scene_manager_get_scene_state(SceneManager* manager, uint32_t scene) { return manager->state[scene]; }
void morse_flipper_ensure_view(MorseFlipperApp* app, uint8_t view) { (void)app; (void)view; }
void morse_flipper_scene_enter_now(MorseFlipperApp* app, uint32_t scene) { (void)app; (void)scene; }
void morse_flipper_scene_back(MorseFlipperApp* app) { (void)app; }
VariableItem* variable_item_list_add(VariableItemList* list, const char* label, uint8_t count, VariableItemChangeCallback changed, void* context) { (void)label; (void)count; (void)changed; (void)context; return &list->items[list->count++]; }
void variable_item_list_reset(VariableItemList* list) { list->count = 0U; }

bool mf_settings_host_test_apply(MorseFlipperApp*, const MfSettingsRequest*, MfSettingsResponse*);
void mf_settings_host_test_navigate(MorseFlipperApp*, uint32_t);
bool morse_flipper_settings_host_enter(MorseFlipperApp*, uint8_t, uint32_t);
void morse_flipper_settings_host_leave(MorseFlipperApp*, uint32_t);

static void apply(MorseFlipperApp* app, uint8_t kind, uint32_t value, bool accepted) {
    MfSettingsResponse response;
    assert(mf_settings_host_test_apply(app, &(MfSettingsRequest){.kind = kind, .value = value}, &response));
    if(response.accepted != accepted) {
        fprintf(stderr, "apply mismatch kind=%u value=%lu got=%u expected=%u\n", kind, (unsigned long)value, response.accepted, accepted);
        assert(false);
    }
}

int main(void) {
    FuriMutex mutex = {0}; ViewDispatcher dispatcher = {0}; SceneManager manager = {0}; VariableItemList list = {0};
    MorseFlipperApp app = {.view_dispatcher = &dispatcher, .scene_manager = &manager, .settings_list = &list,
        .plugin_slot.mutex = &mutex, .listening_settings = {.local_dit_ms = 20U, .lesson = 2U, .group_size = 3U, .session_groups = 3U, .farnsworth_wpm = 20U, .answer_timeout_s = 5U, .group_pause_s = 5U}, .p2_volume_pct = 50U};
    apply(&app, MfSettingsSetLocalWpm, 25U, true);
    apply(&app, MfSettingsSetInputSource, MorseFlipperInputSourcePaddle, true);
    apply(&app, MfSettingsSetKeyerMode, MorseKeyerModeKeyahead, true);
    apply(&app, MfSettingsSetHandedness, 1U, true);
    apply(&app, MfSettingsSetAudioPath, MorseFlipperAudioPathP2, true);
    apply(&app, MfSettingsSetTone, 30U, true);
    assert(app.preview_ticks == MORSE_FLIPPER_PREVIEW_TICKS && sidetones == 1U);
    apply(&app, MfSettingsSetP2Volume, 100U, true);
    apply(&app, MfSettingsSetAudioWaveform, 1U, true);
    apply(&app, MfSettingsSetUsbMode, MorseFlipperPcModeMidi, true);
    apply(&app, MfSettingsSetUsbPaddlePreset, 8U, true);
    apply(&app, MfSettingsSetUsbStraightPreset, 7U, true);
    apply(&app, MfSettingsSetUsbMouseInvert, 1U, true);
    assert(saves == 12U && refreshes == 3U && polls == 3U && audio_syncs == 3U);
    apply(&app, MfSettingsSetTone, 31U, false);
    apply(&app, MfSettingsSetP2Volume, 11U, false);
    apply(&app, MfSettingsSetUsbMode, MorseFlipperPcModeMidi + 1U, false);
    assert(saves == 12U);
    mf_settings_host_test_navigate(&app, MfSettingsNavigateAudio);
    assert(events == MorseFlipperSceneAudioCfg);
    mf_settings_host_test_navigate(&app, MfSettingsNavigateGpio);
    assert(events == MorseFlipperSceneGpio);
    assert(morse_flipper_settings_host_enter(&app, MfSettingsEntryKeying, 2U));
    assert(captured_args.entry == MfSettingsEntryKeying && captured_args.selected_state == 2U);
    morse_flipper_settings_host_leave(&app, MorseFlipperSceneHome);
    assert(detaches == 1U);
    assert(morse_flipper_settings_host_enter(&app, MfSettingsEntryUsb, 1U));
    morse_flipper_settings_host_leave(&app, MorseFlipperScenePc);
    assert(opens == 2U && detaches == 2U && app.preview_ticks == 0U && sidetones == 2U);
    return 0;
}

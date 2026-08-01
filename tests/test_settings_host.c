#include "morse_flipper_settings_host_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned saves, clears, resyncs, refreshes, polls, audio_syncs, sidetones, opens, detaches;
static unsigned gpio_applies, gpio_alerts;
static unsigned mutex_depth;
static bool gpio_accept = true;
static MfSettingsEnterArgs captured_args;
static MfSettingsRequest close_request;
static uint32_t close_selected;
const int morse_flipper_tones[31];

uint32_t furi_get_tick(void) {
    return 1U;
}
void furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout) {
    (void)mutex;
    (void)timeout;
    assert(mutex_depth == 0U);
    mutex_depth++;
}
void furi_mutex_release(FuriMutex* mutex) {
    (void)mutex;
    assert(mutex_depth == 1U);
    mutex_depth--;
}
uint8_t morse_flipper_current_wpm(const MorseFlipperApp* app) {
    return (uint8_t)app->listening_settings.local_dit_ms;
}
uint8_t morse_flipper_local_wpm(const MorseFlipperApp* app) {
    return morse_flipper_current_wpm(app);
}
size_t morse_trainer_lesson_count(void) {
    return 40U;
}
uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp* app) {
    return app->p2_volume_pct;
}
uint8_t morse_flipper_straight_wpm(const MorseFlipperApp* app) {
    return app->straight_wpm;
}
uint8_t morse_trainer_group_size(const MorseTrainer* trainer) {
    return trainer->group_size;
}
uint8_t morse_trainer_session_groups(const MorseTrainer* trainer) {
    return trainer->session_groups;
}
void morse_flipper_set_local_wpm(MorseFlipperApp* app, uint8_t wpm) {
    app->listening_settings.local_dit_ms = wpm;
}
void morse_flipper_set_straight_wpm(MorseFlipperApp* app, uint8_t wpm) {
    app->straight_wpm = wpm;
}
bool morse_flipper_gpio_try_apply(
    MorseFlipperApp* app,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    uint8_t ptt,
    MorseFlipperGpioRule* rule) {
    assert(mutex_depth == 0U);
    gpio_applies++;
    if(rule) *rule = gpio_accept ? MorseFlipperGpioRuleOk : MorseFlipperGpioRuleBadIndex;
    if(!gpio_accept) return false;
    app->gpio_dit_idx = dit;
    app->gpio_dah_idx = dah;
    app->gpio_ground_idx = ground;
    app->gpio_ptt_idx = ptt;
    return true;
}
bool morse_flipper_host_dialog(MorseFlipperApp* app, const MorseFlipperHostDialog* info) {
    (void)app;
    (void)info;
    gpio_alerts++;
    return true;
}
const char* morse_flipper_gpio_rule_text(MorseFlipperGpioRule rule) {
    (void)rule;
    return "bad";
}
void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now) {
    (void)app;
    (void)now;
    clears++;
}
void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now) {
    (void)app;
    (void)now;
    refreshes++;
}
void morse_flipper_poll(MorseFlipperApp* app) {
    (void)app;
    polls++;
}
void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now) {
    (void)app;
    (void)now;
    resyncs++;
}
void morse_flipper_sync_audio_output(MorseFlipperApp* app) {
    (void)app;
    audio_syncs++;
}
void morse_flipper_update_sidetone(MorseFlipperApp* app) {
    (void)app;
    sidetones++;
}
bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app) {
    return app->audio_path == MorseFlipperAudioPathP2;
}
void morse_flipper_save_config(const MorseFlipperApp* app) {
    (void)app;
    saves++;
}
uint8_t morse_pc_paddle_preset_count(void) {
    return 9U;
}
uint8_t morse_pc_straight_preset_count(void) {
    return 8U;
}
bool morse_flipper_plugin_runtime_open_mapped_locked(
    MorseFlipperApp* app,
    uint8_t owner,
    uint8_t mode,
    const char* path,
    uint32_t version,
    uint32_t magic,
    uint32_t size,
    const void* args,
    MorseFlipperMappedFalResult* initial) {
    (void)mode;
    (void)path;
    (void)version;
    (void)magic;
    (void)size;
    captured_args = *(const MfSettingsEnterArgs*)args;
    app->plugin_slot.owner = owner;
    opens++;
    *initial = (MorseFlipperMappedFalResult){0};
    return true;
}
void morse_flipper_plugin_runtime_detach_locked(MorseFlipperApp* app, uint8_t owner) {
    assert(app->plugin_slot.owner == owner);
    app->plugin_slot.owner = 0U;
    detaches++;
}
void scene_manager_set_scene_state(SceneManager* manager, uint32_t scene, uint32_t state) {
    manager->state[scene] = state;
}
uint32_t scene_manager_get_scene_state(SceneManager* manager, uint32_t scene) {
    return manager->state[scene];
}
void morse_flipper_ensure_view(MorseFlipperApp* app, uint8_t view) {
    (void)app;
    (void)view;
}
void morse_flipper_scene_enter_now(MorseFlipperApp* app, uint32_t scene) {
    (void)app;
    (void)scene;
}
void morse_flipper_scene_back(MorseFlipperApp* app) {
    (void)app;
}
VariableItem* variable_item_list_add(
    VariableItemList* list,
    const char* label,
    uint8_t count,
    VariableItemChangeCallback changed,
    void* context) {
    (void)label;
    (void)count;
    (void)changed;
    (void)context;
    return &list->items[list->count++];
}
void variable_item_list_reset(VariableItemList* list) {
    list->count = 0U;
}

bool mf_settings_host_test_apply(MorseFlipperApp*, const MfSettingsRequest*, MfSettingsResponse*);
bool morse_flipper_settings_host_enter(MorseFlipperApp*, uint8_t, uint32_t);
bool morse_flipper_settings_host_close(MorseFlipperApp*, uint32_t);
void morse_flipper_settings_host_leave(MorseFlipperApp*, uint32_t);

static bool
    request_close(void* state, MfSettingsRequest* pending, MorseFlipperMappedFalResult* result) {
    (void)state;
    assert(mutex_depth == 1U);
    *pending = close_request;
    if(result != NULL)
        *result = (MorseFlipperMappedFalResult){.handled = true, .request_exit = true};
    return true;
}

static uint32_t selected_state(const void* state) {
    (void)state;
    assert(mutex_depth == 1U);
    return close_selected;
}

static void apply(MorseFlipperApp* app, uint8_t kind, uint32_t value, bool accepted) {
    MfSettingsResponse response;
    assert(mf_settings_host_test_apply(
        app, &(MfSettingsRequest){.kind = kind, .value = value}, &response));
    if(response.accepted != accepted) {
        fprintf(
            stderr,
            "apply mismatch kind=%u value=%lu got=%u expected=%u\n",
            kind,
            (unsigned long)value,
            response.accepted,
            accepted);
        assert(false);
    }
}

int main(void) {
    FuriMutex mutex = {0};
    ViewDispatcher dispatcher = {0};
    SceneManager manager = {0};
    VariableItemList list = {0};
    MorseFlipperApp app = {
        .view_dispatcher = &dispatcher,
        .scene_manager = &manager,
        .settings_list = &list,
        .plugin_slot.mutex = &mutex,
        .straight_wpm = 10U,
        .listening_settings =
            {.local_dit_ms = 20U,
             .lesson = 2U,
             .group_size = 3U,
             .session_groups = 3U,
             .farnsworth_wpm = 20U,
             .answer_timeout_s = 5U,
             .group_pause_s = 5U},
        .p2_volume_pct = 50U};
    apply(&app, MfSettingsSetLocalWpm, 25U, true);
    apply(&app, MfSettingsSetInputSource, MorseFlipperInputSourceButtons, true);
    assert(app.input_source == MorseFlipperInputSourceButtons);
    apply(&app, MfSettingsSetInputSource, MorseFlipperInputSourceStraight, true);
    assert(app.input_source == MorseFlipperInputSourceStraight);
    apply(&app, MfSettingsSetInputSource, MorseFlipperInputSourcePaddle, true);
    assert(app.input_source == MorseFlipperInputSourcePaddle);
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
    assert(saves == 14U && clears == 3U && refreshes == 5U && polls == 5U && audio_syncs == 3U);
    {
        unsigned no_op_saves = saves;
        unsigned no_op_clears = clears;
        unsigned no_op_resyncs = resyncs;
        unsigned no_op_refreshes = refreshes;
        unsigned no_op_polls = polls;
        unsigned no_op_audio_syncs = audio_syncs;
        unsigned no_op_sidetones = sidetones;

        apply(&app, MfSettingsSetLocalWpm, 25U, true);
        apply(&app, MfSettingsSetInputSource, MorseFlipperInputSourcePaddle, true);
        apply(&app, MfSettingsSetKeyerMode, MorseKeyerModeKeyahead, true);
        apply(&app, MfSettingsSetHandedness, 1U, true);
        apply(&app, MfSettingsSetAudioPath, MorseFlipperAudioPathBuzzer, true);
        assert(app.audio_path == MorseFlipperAudioPathSoftBuzz);
        apply(&app, MfSettingsSetTone, 30U, true);
        apply(&app, MfSettingsSetP2Volume, 100U, true);
        apply(&app, MfSettingsSetAudioWaveform, 1U, true);
        apply(&app, MfSettingsSetUsbMode, MorseFlipperPcModeMidi, true);
        apply(&app, MfSettingsSetUsbPaddlePreset, 8U, true);
        apply(&app, MfSettingsSetUsbStraightPreset, 7U, true);
        apply(&app, MfSettingsSetUsbMouseInvert, 1U, true);
        assert(
            saves == no_op_saves && clears == no_op_clears && resyncs == no_op_resyncs &&
            refreshes == no_op_refreshes && polls == no_op_polls &&
            audio_syncs == no_op_audio_syncs && sidetones == no_op_sidetones);
    }
    apply(&app, MfSettingsSetTone, 31U, false);
    apply(&app, MfSettingsSetP2Volume, 11U, false);
    apply(&app, MfSettingsSetUsbMode, MorseFlipperPcModeMidi + 1U, false);
    apply(&app, MfSettingsSetInputSource, 3U, false);
    assert(saves == 14U && app.input_source == MorseFlipperInputSourcePaddle);
    apply(&app, MfSettingsSetListeningLesson, 7U, true);
    apply(&app, MfSettingsSetListeningFarnsworth, 12U, true);
    apply(&app, MfSettingsSetListeningAnswerTimeout, 8U, true);
    apply(&app, MfSettingsSetListeningGroupPause, 9U, true);
    apply(&app, MfSettingsSetListeningGroupSize, 5U, true);
    apply(&app, MfSettingsSetListeningGroupCount, 20U, true);
    apply(&app, MfSettingsSetListeningCustomSet, 3U, true);
    assert(app.listening_settings.lesson == 7U && app.listening_settings.custom_set_idx == 3U);
    apply(&app, MfSettingsSetStraightWpm, 15U, true);
    apply(&app, MfSettingsSetStraightAnswerTimeout, 5U, true);
    apply(&app, MfSettingsSetStraightNextDelay, 4U, true);
    apply(&app, MfSettingsSetTxGroupsDifficulty, 1U, true);
    apply(&app, MfSettingsSetRxCallsignsLength, 3U, true);
    apply(&app, MfSettingsSetRxCallsignsWpm, 24U, true);
    apply(&app, MfSettingsSetRxCallsignsFarnsworth, 18U, true);
    {
        unsigned no_op_saves = saves;
        unsigned no_op_clears = clears;
        unsigned no_op_resyncs = resyncs;
        unsigned no_op_refreshes = refreshes;
        unsigned no_op_polls = polls;
        unsigned no_op_audio_syncs = audio_syncs;
        unsigned no_op_sidetones = sidetones;

        apply(&app, MfSettingsSetListeningLesson, 7U, true);
        apply(&app, MfSettingsSetListeningFarnsworth, 12U, true);
        apply(&app, MfSettingsSetListeningAnswerTimeout, 8U, true);
        apply(&app, MfSettingsSetListeningGroupPause, 9U, true);
        apply(&app, MfSettingsSetListeningGroupSize, 5U, true);
        apply(&app, MfSettingsSetListeningGroupCount, 20U, true);
        apply(&app, MfSettingsSetListeningCustomSet, 3U, true);
        apply(&app, MfSettingsSetStraightWpm, 15U, true);
        apply(&app, MfSettingsSetStraightAnswerTimeout, 5U, true);
        apply(&app, MfSettingsSetStraightNextDelay, 4U, true);
        apply(&app, MfSettingsSetTxGroupsDifficulty, 1U, true);
        apply(&app, MfSettingsSetRxCallsignsLength, 3U, true);
        apply(&app, MfSettingsSetRxCallsignsWpm, 24U, true);
        apply(&app, MfSettingsSetRxCallsignsFarnsworth, 18U, true);
        assert(
            saves == no_op_saves && clears == no_op_clears && resyncs == no_op_resyncs &&
            refreshes == no_op_refreshes && polls == no_op_polls &&
            audio_syncs == no_op_audio_syncs && sidetones == no_op_sidetones);
    }
    assert(morse_flipper_settings_host_enter(&app, MfSettingsEntryKeying, 2U));
    assert(captured_args.entry == MfSettingsEntryKeying && captured_args.selected_state == 2U);
    assert(captured_args.services != NULL && captured_args.services->apply != NULL);
    {
        MfSettingsResponse response;
        unsigned no_op_saves = saves;

        assert(captured_args.services->apply(
            captured_args.service_context,
            &(MfSettingsRequest){.kind = MfSettingsSetLocalWpm, .value = 25U},
            &response));
        assert(response.accepted && saves == no_op_saves);
    }
    morse_flipper_settings_host_leave(&app, MorseFlipperSceneHome);
    assert(detaches == 1U);
    assert(morse_flipper_settings_host_enter(&app, MfSettingsEntryUsb, 1U));
    morse_flipper_settings_host_leave(&app, MorseFlipperScenePc);
    assert(opens == 2U && detaches == 2U && app.preview_ticks == 0U && sidetones == 2U);
    {
        const MfSettingsApi api = {
            .request_close = request_close,
            .selected_state = selected_state,
        };
        unsigned before_applies = gpio_applies;

        close_request = (MfSettingsRequest){
            .kind = MfSettingsApplyGpioDraft,
            .gpio_dit_pin = 5U,
            .gpio_dah_pin = 6U,
            .gpio_ground_pin = 7U,
            .gpio_ptt_pin = 16U,
        };
        close_selected = 3U;
        app.plugin_slot.owner = MorseFlipperPluginOwnerSettings;
        app.plugin_slot.api = &api;
        app.plugin_slot.state = &app;
        assert(morse_flipper_settings_host_close(&app, MorseFlipperSceneGpio));
        assert(mutex_depth == 0U && gpio_applies == before_applies + 1U);
        assert(manager.state[MorseFlipperSceneGpio] == close_selected);
        assert(app.gpio_dit_idx == 5U && app.gpio_dah_idx == 6U);

        close_request.kind = MfSettingsRequestNone;
        close_selected = 2U;
        assert(morse_flipper_settings_host_close(&app, MorseFlipperSceneGpio));
        assert(gpio_applies == before_applies + 1U);
        assert(manager.state[MorseFlipperSceneGpio] == close_selected);

        close_request.kind = MfSettingsApplyGpioDraft;
        gpio_accept = false;
        close_selected = 1U;
        manager.state[MorseFlipperSceneGpio] = 9U;
        assert(!morse_flipper_settings_host_close(&app, MorseFlipperSceneGpio));
        assert(mutex_depth == 0U && gpio_applies == before_applies + 2U);
        assert(gpio_alerts == 1U && manager.state[MorseFlipperSceneGpio] == 9U);
        gpio_accept = true;
    }
    return 0;
}

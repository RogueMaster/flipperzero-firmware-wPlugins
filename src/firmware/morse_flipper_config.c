/*
 * Purpose: Load and save persistent Morse Flipper settings.
 * Owns: config record layout, legacy path cleanup, and default clamping.
 * Depends on: morse_flipper_app_i.h, storage paths, and Flipper Storage.
 * Tests: firmware build; persistence is hardware-only.
 */

#ifdef MF_CONFIG_HOST_TEST
#include "morse_flipper_config_test.h"

typedef struct {
    uint8_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t input_source;
    uint16_t local_dit_ms;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t gpio_ptt_idx;
    uint8_t trainer_custom_set_idx;
    uint8_t usb_mode;
    uint8_t usb_paddle_preset;
    uint8_t usb_straight_preset;
    uint8_t usb_mouse_invert;
    uint8_t trainer_farnsworth_wpm;
    uint8_t trainer_answer_timeout_s;
    uint8_t trainer_group_pause_s;
    uint8_t reserved[609];
} MorseFlipperConfig;

_Static_assert(sizeof(MorseFlipperConfig) == 632U, "main config must remain version-1 compatible");

static uint8_t mf_config_test_wpm(uint16_t dit_ms) {
    uint8_t wpm = (uint8_t)((1200U + (dit_ms / 2U)) / (dit_ms ? dit_ms : 100U));
    return wpm < 10U ? 10U : (wpm > 30U ? 30U : wpm);
}

static void mf_config_test_normalize(MorseFlipperListeningSettings* settings) {
    uint8_t wpm = mf_config_test_wpm(settings->local_dit_ms);
    if(settings->local_dit_ms == 0U) settings->local_dit_ms = 100U;
    if(settings->lesson == 0U || settings->lesson > 40U) settings->lesson = 1U;
    if(settings->group_size == 0U || settings->group_size > 9U) settings->group_size = 1U;
    if(settings->session_groups < 3U || settings->session_groups > 30U) settings->session_groups = 3U;
    if(settings->custom_set_idx > 8U) settings->custom_set_idx = 0U;
    if(settings->farnsworth_wpm == 0U || settings->farnsworth_wpm > wpm) settings->farnsworth_wpm = wpm;
    if(settings->answer_timeout_s < 3U || settings->answer_timeout_s > 10U) settings->answer_timeout_s = 6U;
    if(settings->group_pause_s < 3U || settings->group_pause_s > 15U) settings->group_pause_s = 3U;
}

void mf_config_test_save(const MorseFlipperListeningSettings* settings, uint8_t out[632]) {
    MorseFlipperConfig config = {.version = 1U};
    config.local_dit_ms = settings->local_dit_ms;
    config.trainer_lesson = settings->lesson;
    config.trainer_group_size = settings->group_size;
    config.trainer_session_groups = settings->session_groups;
    config.input_source = settings->input_source;
    config.trainer_custom_set_idx = settings->custom_set_idx;
    config.trainer_farnsworth_wpm = settings->farnsworth_wpm;
    config.trainer_answer_timeout_s = settings->answer_timeout_s;
    config.trainer_group_pause_s = settings->group_pause_s;
    memcpy(out, &config, sizeof(config));
}

bool mf_config_test_load(const uint8_t in[632], MorseFlipperListeningSettings* settings) {
    MorseFlipperConfig config;
    memcpy(&config, in, sizeof(config));
    if(config.version != 1U) return false;
    *settings = (MorseFlipperListeningSettings){.local_dit_ms = config.local_dit_ms,
        .lesson = config.trainer_lesson, .group_size = config.trainer_group_size,
        .session_groups = config.trainer_session_groups, .custom_set_idx = config.trainer_custom_set_idx,
        .input_source = config.input_source <= 2U ? config.input_source : 0U,
        .farnsworth_wpm = config.trainer_farnsworth_wpm,
        .answer_timeout_s = config.trainer_answer_timeout_s, .group_pause_s = config.trainer_group_pause_s};
    mf_config_test_normalize(settings);
    return true;
}
#else
#include "morse_flipper_app_i.h"
#include "morse_flipper_radio_config.h"
#endif

#ifndef MF_CONFIG_HOST_TEST
#define MORSE_FLIPPER_OLD_RF_CONFIG_PATH  APP_DATA_PATH("rf.bin")
#define MORSE_FLIPPER_OLD_TXG_CONFIG_PATH APP_DATA_PATH("tx_groups.bin")

typedef struct {
    uint8_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t input_source;
    uint16_t local_dit_ms;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t gpio_ptt_idx;
    uint8_t trainer_custom_set_idx;
    uint8_t usb_mode;
    uint8_t usb_paddle_preset;
    uint8_t usb_straight_preset;
    uint8_t usb_mouse_invert;
    uint8_t trainer_farnsworth_wpm;
    uint8_t trainer_answer_timeout_s;
    uint8_t trainer_group_pause_s;
    uint16_t straight_dit_ms;
    uint8_t straight_answer_timeout_s;
    uint8_t straight_next_delay_s;
    uint8_t audio_path;
    uint8_t p2_volume_pct;
    uint8_t txg_difficulty;
    uint8_t reserved0;
    uint32_t rf_frequency_hz;
    uint8_t ham_logging_enabled;
    uint8_t ham_message_count;
    uint8_t ham_assignments[MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS];
    char ham_messages[MORSE_FLIPPER_HAM_KEYER_MAX_MESSAGES]
                     [MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN + 1U];
} MorseFlipperConfig;

_Static_assert(sizeof(MorseFlipperConfig) == 632U, "main config must remain version-1 compatible");

uint8_t morse_flipper_local_wpm(const MorseFlipperApp* app) {
    uint16_t dit;
    uint8_t wpm;

    if(app == NULL) return 0U;
    dit = app->listening_settings.local_dit_ms ? app->listening_settings.local_dit_ms :
                                                MORSE_FLIPPER_DEFAULT_DIT_MS;
    wpm = (uint8_t)((1200U + (dit / 2U)) / dit);
    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    return wpm;
}

void morse_flipper_clamp_trainer_settings(MorseFlipperApp* app) {
    uint8_t w;

    if(app == NULL) return;

    w = morse_flipper_local_wpm(app);
    if(app->listening_settings.lesson == 0U ||
       app->listening_settings.lesson > morse_trainer_lesson_count())
        app->listening_settings.lesson = 1U;
    if(app->listening_settings.group_size == 0U) app->listening_settings.group_size = 1U;
    if(app->listening_settings.group_size > 9U) app->listening_settings.group_size = 9U;
    if(app->listening_settings.session_groups < 3U) app->listening_settings.session_groups = 3U;
    if(app->listening_settings.session_groups > 30U) app->listening_settings.session_groups = 30U;
    if(app->listening_settings.custom_set_idx > MORSE_TRAINER_CUSTOM_SET_CAP)
        app->listening_settings.custom_set_idx = 0U;
    if(app->listening_settings.farnsworth_wpm == 0U) app->listening_settings.farnsworth_wpm = w;
    if(app->listening_settings.farnsworth_wpm > w) app->listening_settings.farnsworth_wpm = w;

    if(app->listening_settings.answer_timeout_s == 0U)
        app->listening_settings.answer_timeout_s = MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S;
    if(app->listening_settings.answer_timeout_s < MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S)
        app->listening_settings.answer_timeout_s = MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S;
    if(app->listening_settings.answer_timeout_s > MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S)
        app->listening_settings.answer_timeout_s = MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S;

    if(app->listening_settings.group_pause_s == 0U)
        app->listening_settings.group_pause_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_DEFAULT_S;
    if(app->listening_settings.group_pause_s < MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S)
        app->listening_settings.group_pause_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S;
    if(app->listening_settings.group_pause_s > MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S)
        app->listening_settings.group_pause_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S;
}

void morse_flipper_clamp_straight_settings(MorseFlipperApp* app) {
    uint8_t w;

    if(app == NULL) return;

    w = morse_flipper_straight_wpm(app);
    if(w == 0U) {
        app->straight_dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS;
        w = morse_flipper_straight_wpm(app);
    }

    if(app->straight_answer_timeout_s == 0U)
        app->straight_answer_timeout_s = MORSE_FLIPPER_STRAIGHT_TIMEOUT_DEFAULT_S;
    if(app->straight_answer_timeout_s < MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S)
        app->straight_answer_timeout_s = MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S;
    if(app->straight_answer_timeout_s > MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S)
        app->straight_answer_timeout_s = MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S;

    if(app->straight_next_delay_s == 0U)
        app->straight_next_delay_s = MORSE_FLIPPER_STRAIGHT_NEXT_DEFAULT_S;
    if(app->straight_next_delay_s < MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S)
        app->straight_next_delay_s = MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S;
    if(app->straight_next_delay_s > MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S)
        app->straight_next_delay_s = MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S;

    UNUSED(w);
}

static uint8_t morse_flipper_config_load_ptt_idx(uint8_t stored_ptt_idx) {
    return stored_ptt_idx == MorseFlipperGpioPinP16 ? MorseFlipperGpioPinP16 :
                                                      MORSE_FLIPPER_GPIO_PIN_NONE;
}

static void morse_flipper_config_apply_gpio(
    MorseFlipperApp* app,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    uint8_t ptt) {
    if(app == NULL) return;

    if(morse_flipper_gpio_validate(dit, dah, ground) != MorseFlipperGpioRuleOk) {
        return;
    }

    app->gpio_dit_idx = dit;
    app->gpio_dah_idx = dah;
    app->gpio_ground_idx = ground;
    ptt = morse_flipper_config_load_ptt_idx(ptt);
    if(morse_flipper_gpio_validate_with_ptt(dit, dah, ground, ptt) != MorseFlipperGpioRuleOk) {
        ptt = MORSE_FLIPPER_GPIO_PIN_NONE;
    }

    app->gpio_ptt_idx = ptt;
    morse_flipper_gpio_sync_straight_idx(app);
}

static uint8_t morse_flipper_config_load_tone_idx(uint8_t stored_tone_idx) {
    if(stored_tone_idx < COUNT_OF(morse_flipper_tones)) return stored_tone_idx;
    return MORSE_FLIPPER_DEFAULT_TONE_IDX;
}

static uint8_t morse_flipper_config_load_audio_path(uint8_t stored_audio_path) {
    if(stored_audio_path < MorseFlipperAudioPathCount) return stored_audio_path;
    return MorseFlipperAudioPathSoftBuzz;
}

static uint8_t morse_flipper_config_load_p2_volume(uint8_t stored_p2_volume_pct) {
    if(stored_p2_volume_pct < 10U) return 10U;
    if(stored_p2_volume_pct > 100U) return 100U;
    return (uint8_t)(10U + ((((uint16_t)stored_p2_volume_pct - 10U) / 5U) * 5U));
}

static uint8_t morse_flipper_config_load_txg_difficulty(uint8_t stored_difficulty) {
    if(stored_difficulty < MorseFlipperTxgDifficultyCount) return stored_difficulty;
    return MorseFlipperTxgDifficultyCompetition;
}

static void morse_flipper_config_apply(MorseFlipperApp* app, const MorseFlipperConfig* config) {
    if(app == NULL || config == NULL) return;

    app->tone_idx = morse_flipper_config_load_tone_idx(config->tone_idx);

    if(config->keyer_mode >= MorseKeyerModeStraight &&
       config->keyer_mode <= MorseKeyerModeKeyahead)
        app->keyer_mode = config->keyer_mode;

    if(config->handedness <= MorseFlipperHandednessSwapped) app->handedness = config->handedness;
    if(config->input_source <= MorseFlipperInputSourceButtons)
        app->input_source = config->input_source;

    app->listening_settings.lesson = config->trainer_lesson;
    app->listening_settings.group_size = config->trainer_group_size;
    app->listening_settings.session_groups = config->trainer_session_groups;
    app->listening_settings.local_dit_ms = config->local_dit_ms;

    morse_flipper_config_apply_gpio(
        app,
        config->gpio_dit_idx,
        config->gpio_dah_idx,
        config->gpio_ground_idx,
        config->gpio_ptt_idx);

    if(config->trainer_custom_set_idx <= MORSE_TRAINER_CUSTOM_SET_CAP)
        app->listening_settings.custom_set_idx = config->trainer_custom_set_idx;
    if(config->usb_mode <= MorseFlipperPcModeMidi) app->pc_mode_pref = config->usb_mode;
    if(config->usb_paddle_preset < morse_pc_paddle_preset_count())
        app->pc_paddle_preset = config->usb_paddle_preset;
    if(config->usb_straight_preset < morse_pc_straight_preset_count())
        app->pc_straight_preset = config->usb_straight_preset;

    app->mouse_invert = config->usb_mouse_invert != 0U;
    app->listening_settings.farnsworth_wpm = config->trainer_farnsworth_wpm;
    app->listening_settings.answer_timeout_s = config->trainer_answer_timeout_s;
    app->listening_settings.group_pause_s = config->trainer_group_pause_s;
    app->straight_dit_ms = config->straight_dit_ms;
    app->straight_answer_timeout_s = config->straight_answer_timeout_s;
    app->straight_next_delay_s = config->straight_next_delay_s;
    app->audio_path = morse_flipper_config_load_audio_path(config->audio_path);
    app->p2_volume_pct = morse_flipper_config_load_p2_volume(config->p2_volume_pct);
    app->txg_difficulty = morse_flipper_config_load_txg_difficulty(config->txg_difficulty);
    app->rf_frequency_hz =
        morse_flipper_radio_config_candidate(config->rf_frequency_hz);

    app->ham_keyer.logging_enabled = config->ham_logging_enabled != 0U;
    app->ham_keyer.message_count = config->ham_message_count;
    memcpy(
        app->ham_keyer.assignments, config->ham_assignments, sizeof(app->ham_keyer.assignments));
    memcpy(app->ham_keyer.messages, config->ham_messages, sizeof(app->ham_keyer.messages));
}

static void morse_flipper_config_apply_runtime_limits(MorseFlipperApp* app) {
    if(app == NULL) return;

    morse_flipper_clamp_trainer_settings(app);
    morse_flipper_clamp_straight_settings(app);
    morse_flipper_ham_keyer_normalize(&app->ham_keyer);
}

static void morse_flipper_config_delete_settings(Storage* storage) {
    if(storage == NULL) return;

    storage_common_remove(storage, MORSE_FLIPPER_CONFIG_PATH);
    storage_common_remove(storage, MORSE_FLIPPER_OLD_RF_CONFIG_PATH);
    storage_common_remove(storage, MORSE_FLIPPER_OLD_TXG_CONFIG_PATH);
}

void morse_flipper_load_config(MorseFlipperApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    MorseFlipperConfig config;
    uint16_t got = 0U;
    bool reset_settings = false;

    if(storage_file_open(file, MORSE_FLIPPER_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        got = storage_file_read(file, &config, sizeof(config));
        if(got == sizeof(config) && config.version == MORSE_FLIPPER_SETTINGS_VERSION)
            morse_flipper_config_apply(app, &config);
        else
            reset_settings = true;
    }

    storage_file_close(file);
    if(reset_settings) morse_flipper_config_delete_settings(storage);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    morse_flipper_config_apply_runtime_limits(app);
}

void morse_flipper_save_config(const MorseFlipperApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    MorseFlipperConfig config = {
        .version = MORSE_FLIPPER_SETTINGS_VERSION,
        .tone_idx = app->tone_idx,
        .keyer_mode = app->keyer_mode,
        .handedness = app->handedness,
        .trainer_lesson = app->listening_settings.lesson,
        .trainer_group_size = app->listening_settings.group_size,
        .trainer_session_groups = app->listening_settings.session_groups,
        .input_source = app->input_source,
        .local_dit_ms = app->listening_settings.local_dit_ms,
        .gpio_straight_idx = morse_flipper_gpio_straight_idx(app),
        .gpio_dit_idx = app->gpio_dit_idx,
        .gpio_dah_idx = app->gpio_dah_idx,
        .gpio_ground_idx = app->gpio_ground_idx,
        .gpio_ptt_idx = app->gpio_ptt_idx,
        .trainer_custom_set_idx = app->listening_settings.custom_set_idx,
        .usb_mode = app->pc_mode_pref,
        .usb_paddle_preset = app->pc_paddle_preset,
        .usb_straight_preset = app->pc_straight_preset,
        .usb_mouse_invert = app->mouse_invert ? 1U : 0U,
        .trainer_farnsworth_wpm = app->listening_settings.farnsworth_wpm,
        .trainer_answer_timeout_s = app->listening_settings.answer_timeout_s,
        .trainer_group_pause_s = app->listening_settings.group_pause_s,
        .straight_dit_ms = app->straight_dit_ms,
        .straight_answer_timeout_s = app->straight_answer_timeout_s,
        .straight_next_delay_s = app->straight_next_delay_s,
        .audio_path = app->audio_path,
        .p2_volume_pct = app->p2_volume_pct,
        .txg_difficulty = morse_flipper_config_load_txg_difficulty(app->txg_difficulty),
        .reserved0 = 0U,
        .rf_frequency_hz = app->rf_frequency_hz,
        .ham_logging_enabled = app->ham_keyer.logging_enabled ? 1U : 0U,
        .ham_message_count = app->ham_keyer.message_count,
    };

    memcpy(config.ham_assignments, app->ham_keyer.assignments, sizeof(config.ham_assignments));
    memcpy(config.ham_messages, app->ham_keyer.messages, sizeof(config.ham_messages));

    if(storage_file_open(file, MORSE_FLIPPER_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(file, &config, sizeof(config));

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
#endif

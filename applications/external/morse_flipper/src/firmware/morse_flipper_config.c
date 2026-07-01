/*
 * Purpose: Load and save persistent Morse Flipper settings.
 * Owns: config record layout, legacy path cleanup, and default clamping.
 * Depends on: morse_flipper_app_i.h, storage paths, and Flipper Storage.
 * Tests: firmware build; persistence is hardware-only.
 */

#include "morse_flipper_app_i.h"

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

uint8_t morse_flipper_local_wpm(const MorseFlipperApp* app) {
    uint16_t dit;
    uint8_t wpm;

    if(app == NULL) return 0U;
    dit = app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    wpm = (uint8_t)((1200U + (dit / 2U)) / dit);
    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    return wpm;
}

void morse_flipper_clamp_trainer_settings(MorseFlipperApp* app) {
    uint8_t w;

    if(app == NULL) return;

    w = morse_flipper_local_wpm(app);
    if(app->trainer_farnsworth_wpm == 0U) app->trainer_farnsworth_wpm = w;
    if(app->trainer_farnsworth_wpm > w) app->trainer_farnsworth_wpm = w;

    if(app->trainer_answer_timeout_s == 0U)
        app->trainer_answer_timeout_s = MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S;
    if(app->trainer_answer_timeout_s < MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S)
        app->trainer_answer_timeout_s = MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S;
    if(app->trainer_answer_timeout_s > MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S)
        app->trainer_answer_timeout_s = MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S;

    if(app->trainer_group_pause_s == 0U)
        app->trainer_group_pause_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_DEFAULT_S;
    if(app->trainer_group_pause_s < MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S)
        app->trainer_group_pause_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S;
    if(app->trainer_group_pause_s > MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S)
        app->trainer_group_pause_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S;
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
    return stored_ptt_idx == MorseFlipperGpioPinP15 ? MorseFlipperGpioPinP15 :
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
    app->gpio_ptt_idx = morse_flipper_config_load_ptt_idx(ptt);
    morse_flipper_gpio_sync_straight_idx(app);
}

static uint8_t morse_flipper_config_load_tone_idx(uint8_t stored_tone_idx) {
    if(stored_tone_idx < COUNT_OF(morse_flipper_tones)) return stored_tone_idx;
    return MORSE_FLIPPER_DEFAULT_TONE_IDX;
}

static uint8_t morse_flipper_config_load_audio_path(uint8_t stored_audio_path) {
    if(stored_audio_path <= MorseFlipperAudioPathVibration) return stored_audio_path;
    return MorseFlipperAudioPathBuzzer;
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

    morse_trainer_set_lesson(&app->trainer, config->trainer_lesson);
    morse_trainer_set_group_size(&app->trainer, config->trainer_group_size);
    morse_trainer_set_session_groups(&app->trainer, config->trainer_session_groups);
    if(config->local_dit_ms != 0U) app->trainer.local_dit_ms = config->local_dit_ms;

    morse_flipper_config_apply_gpio(
        app,
        config->gpio_dit_idx,
        config->gpio_dah_idx,
        config->gpio_ground_idx,
        config->gpio_ptt_idx);

    if(config->trainer_custom_set_idx <= app->custom_sets.count)
        app->trainer.custom_set_idx = config->trainer_custom_set_idx;
    if(config->usb_mode <= MorseFlipperPcModeMidi) app->pc_mode_pref = config->usb_mode;
    if(config->usb_paddle_preset < morse_pc_paddle_preset_count())
        app->pc_paddle_preset = config->usb_paddle_preset;
    if(config->usb_straight_preset < morse_pc_straight_preset_count())
        app->pc_straight_preset = config->usb_straight_preset;

    app->mouse_invert = config->usb_mouse_invert != 0U;
    app->trainer_farnsworth_wpm = config->trainer_farnsworth_wpm;
    app->trainer_answer_timeout_s = config->trainer_answer_timeout_s;
    app->trainer_group_pause_s = config->trainer_group_pause_s;
    app->straight_dit_ms = config->straight_dit_ms;
    app->straight_answer_timeout_s = config->straight_answer_timeout_s;
    app->straight_next_delay_s = config->straight_next_delay_s;
    app->audio_path = morse_flipper_config_load_audio_path(config->audio_path);
    app->p2_volume_pct = morse_flipper_config_load_p2_volume(config->p2_volume_pct);
    app->txg_difficulty = morse_flipper_config_load_txg_difficulty(config->txg_difficulty);
    if(morse_flipper_rf_frequency_valid_hz(config->rf_frequency_hz))
        morse_flipper_rf_set_frequency_hz(&app->rf, config->rf_frequency_hz);
    else
        morse_flipper_rf_set_frequency_hz(&app->rf, morse_flipper_rf_default_frequency_hz());

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
    morse_flipper_tx_group_set_range(
        &app->tx_group,
        morse_flipper_txg_range_low(app->txg_difficulty),
        morse_flipper_txg_range_high(app->txg_difficulty));
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
        if(got == sizeof(config) && config.version == MORSE_FLIPPER_SETTINGS_VERSION) {
            morse_flipper_config_apply(app, &config);
        } else {
            reset_settings = true;
        }
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
        .trainer_lesson = morse_trainer_lesson(&app->trainer),
        .trainer_group_size = morse_trainer_group_size(&app->trainer),
        .trainer_session_groups = morse_trainer_session_groups(&app->trainer),
        .input_source = app->input_source,
        .local_dit_ms = app->trainer.local_dit_ms,
        .gpio_straight_idx = morse_flipper_gpio_straight_idx(app),
        .gpio_dit_idx = app->gpio_dit_idx,
        .gpio_dah_idx = app->gpio_dah_idx,
        .gpio_ground_idx = app->gpio_ground_idx,
        .gpio_ptt_idx = app->gpio_ptt_idx,
        .trainer_custom_set_idx = app->trainer.custom_set_idx,
        .usb_mode = app->pc_mode_pref,
        .usb_paddle_preset = app->pc_paddle_preset,
        .usb_straight_preset = app->pc_straight_preset,
        .usb_mouse_invert = app->mouse_invert ? 1U : 0U,
        .trainer_farnsworth_wpm = app->trainer_farnsworth_wpm,
        .trainer_answer_timeout_s = app->trainer_answer_timeout_s,
        .trainer_group_pause_s = app->trainer_group_pause_s,
        .straight_dit_ms = app->straight_dit_ms,
        .straight_answer_timeout_s = app->straight_answer_timeout_s,
        .straight_next_delay_s = app->straight_next_delay_s,
        .audio_path = app->audio_path,
        .p2_volume_pct = app->p2_volume_pct,
        .txg_difficulty = morse_flipper_config_load_txg_difficulty(app->txg_difficulty),
        .reserved0 = 0U,
        .rf_frequency_hz = morse_flipper_rf_frequency_hz(&app->rf),
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

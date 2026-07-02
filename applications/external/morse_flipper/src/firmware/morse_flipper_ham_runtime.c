/*
 * Purpose: Bridge ham keyer macros to GPIO, audio, logging, and UI state.
 * Owns: ham run startup, macro ticking, assignment flow, and text input glue.
 * Depends on: morse_flipper_app_i.h, cw.h, and morse_flipper_ham_keyer.h.
 * Tests: tests/test_ham_keyer.c plus firmware build.
 */

#include "morse_flipper_app_i.h"
#include "cw.h"
#include "morse_flipper_cw_token.h"

static char morse_flipper_ham_upper(char ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

void morse_flipper_ham_gpio_apply(MorseFlipperApp* app) {
    const GpioPin* key_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP15];
    const GpioPin* ptt_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP16];

    if(app == NULL) return;

    furi_hal_gpio_init(key_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(ptt_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(key_pin, false);
    furi_hal_gpio_write(ptt_pin, false);
    app->ham.key_level = false;
    app->ptt_level = false;
    app->ptt_tail_until = 0U;
}

void morse_flipper_ham_gpio_release(MorseFlipperApp* app) {
    if(app == NULL) return;

    furi_hal_gpio_write(morse_flipper_gpio_pins[MorseFlipperGpioPinP15], false);
    furi_hal_gpio_write(morse_flipper_gpio_pins[MorseFlipperGpioPinP16], false);
    app->ham.key_level = false;
    app->ptt_level = false;
    app->ptt_tail_until = 0U;
    morse_flipper_gpio_apply(app);
}

void morse_flipper_ham_stop_macro(MorseFlipperApp* app) {
    if(app == NULL) return;

    app->ham.macro_active = false;
    app->ham.macro_mark = false;
    app->ham.macro_char_idx = 0U;
    app->ham.macro_mark_idx = 0U;
    app->ham.macro_dir = MORSE_FLIPPER_HAM_KEYER_UNASSIGNED;
    app->ham.macro_next_at = 0U;
    app->ham.macro_text[0] = '\0';
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_HAM_MACRO, false);
}

void morse_flipper_ham_start_macro(MorseFlipperApp* app, const char* text, uint32_t now_ms) {
    if(app == NULL || text == NULL || text[0] == '\0') return;

    morse_flipper_ham_stop_macro(app);
    strncpy(app->ham.macro_text, text, sizeof(app->ham.macro_text) - 1U);
    app->ham.macro_text[sizeof(app->ham.macro_text) - 1U] = '\0';
    app->ham.macro_active = true;
    app->ham.macro_next_at = now_ms;
    morse_flipper_view_dirty(app);
}

void morse_flipper_tick_ham_macro(MorseFlipperApp* app, uint32_t now_ms) {
    uint16_t cw_code;
    uint8_t ch;
    uint8_t token = 0U;
    size_t consumed = 1U;
    uint32_t dit_ms;
    uint8_t marks;

    if(app == NULL || !app->ham.macro_active) return;

    if(app->screen != MorseFlipperScreenHamRun) {
        morse_flipper_ham_stop_macro(app);
        return;
    }

    if(now_ms < app->ham.macro_next_at) return;

    dit_ms = morse_flipper_current_dit_ms(app);
    if(morse_flipper_cw_token_parse(
           &app->ham.macro_text[app->ham.macro_char_idx], &token, &consumed)) {
        ch = token;
    } else {
        ch = (uint8_t)morse_flipper_ham_upper(app->ham.macro_text[app->ham.macro_char_idx]);
        consumed = 1U;
    }

    if(ch == '\0') {
        morse_flipper_ham_stop_macro(app);
        morse_flipper_view_dirty(app);
        return;
    }

    if(!app->ham.macro_mark && ch == ' ') {
        app->ham.macro_char_idx = (uint8_t)(app->ham.macro_char_idx + consumed);
        app->ham.macro_mark_idx = 0U;
        app->ham.macro_next_at = now_ms + (dit_ms * 7U);
        return;
    }

    cw_code = token ? morse_flipper_cw_token_code(token) : cw((char)ch);
    marks = cw_symbol_count(cw_code);
    if(cw_code == CW_INVALID || marks == 0U) {
        app->ham.macro_char_idx = (uint8_t)(app->ham.macro_char_idx + consumed);
        app->ham.macro_mark_idx = 0U;
        app->ham.macro_next_at = now_ms + (dit_ms * 3U);
        return;
    }

    if(!app->ham.macro_mark) {
        app->ham.macro_mark = true;
        morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_HAM_MACRO, true);
        app->ham.macro_next_at =
            now_ms + (dit_ms * cw_symbol_units(cw_code, app->ham.macro_mark_idx));
        morse_flipper_view_dirty(app);
        return;
    }

    app->ham.macro_mark = false;
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_HAM_MACRO, false);
    if(app->ham.macro_mark_idx + 1U < marks) {
        app->ham.macro_mark_idx++;
        app->ham.macro_next_at = now_ms + dit_ms;
    } else {
        app->ham.macro_char_idx = (uint8_t)(app->ham.macro_char_idx + consumed);
        app->ham.macro_mark_idx = 0U;
        app->ham.macro_next_at = now_ms + (dit_ms * 3U);
    }
    morse_flipper_view_dirty(app);
}

static void
    morse_flipper_ham_log_now(char* date_key, size_t date_sz, char* stamp, size_t stamp_sz) {
    DateTime dt = {0};

    furi_hal_rtc_get_datetime(&dt);
    if(date_key != NULL && date_sz != 0U) {
        snprintf(
            date_key,
            date_sz,
            "%04u-%02u-%02u",
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day);
    }
    if(stamp != NULL && stamp_sz != 0U) {
        snprintf(
            stamp,
            stamp_sz,
            "%04u-%02u-%02u %02u:%02u",
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day,
            (unsigned)dt.hour,
            (unsigned)dt.minute);
    }
}

void morse_flipper_ham_log_append_text(MorseFlipperApp* app, const char* text, uint32_t now_ms) {
    char date_key[16];
    char stamp[24];
    char expanded[96];

    if(app == NULL || text == NULL || text[0] == '\0') return;
    if(app->screen != MorseFlipperScreenHamRun || !app->ham_keyer.logging_enabled) return;

    morse_flipper_cw_token_expand_text(expanded, sizeof(expanded), text);
    morse_flipper_ham_log_now(date_key, sizeof(date_key), stamp, sizeof(stamp));
    morse_flipper_ham_keyer_append_activity(&app->ham_keyer, expanded, now_ms, date_key, stamp);
}

void morse_flipper_ham_log_append_marker(MorseFlipperApp* app, const char* marker, uint32_t now_ms) {
    char date_key[16];
    char stamp[24];

    if(app == NULL || marker == NULL || marker[0] == '\0') return;
    if(app->screen != MorseFlipperScreenHamRun || !app->ham_keyer.logging_enabled) return;

    morse_flipper_ham_log_now(date_key, sizeof(date_key), stamp, sizeof(stamp));
    morse_flipper_ham_keyer_append_marker(&app->ham_keyer, marker, now_ms, date_key, stamp);
}

void morse_flipper_ham_log_flush(MorseFlipperApp* app) {
    Storage* storage;
    File* file;
    char path[96];
    char header[MORSE_FLIPPER_HAM_KEYER_STAMP_LEN + 3U];
    bool ok;

    if(app == NULL || app->ham_keyer.pending_len == 0U) return;

    if(!app->ham_keyer.logging_enabled) {
        morse_flipper_ham_keyer_clear_pending(&app->ham_keyer);
        return;
    }

    snprintf(
        path,
        sizeof(path),
        "%s%s.txt",
        MORSE_FLIPPER_HAM_KEYER_LOG_PREFIX,
        app->ham_keyer.current_log_date[0] ? app->ham_keyer.current_log_date : "unknown");
    snprintf(header, sizeof(header), "%s\n", app->ham_keyer.pending_stamp);

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    storage_common_mkdir(storage, MORSE_FLIPPER_HAM_DIR);
    ok = storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(ok) {
        storage_file_write(file, header, strlen(header));
        storage_file_write(file, app->ham_keyer.pending, app->ham_keyer.pending_len);
        if(app->ham_keyer.pending[app->ham_keyer.pending_len - 1U] != '\n')
            storage_file_write(file, "\n", 1U);
        storage_file_write(file, "\n", 1U);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    morse_flipper_ham_keyer_clear_pending(&app->ham_keyer);
}

void morse_flipper_ham_log_flush_if_idle(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || app->screen != MorseFlipperScreenHamRun) return;
    if(morse_flipper_any_active_notes(app) || app->ham.macro_active) return;
    if(morse_flipper_ham_keyer_should_flush(&app->ham_keyer, now_ms))
        morse_flipper_ham_log_flush(app);
}

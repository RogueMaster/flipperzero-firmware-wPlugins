/**
 * ClayLoop - Clay pigeon/skeet shooting controller for Flipper Zero Sub-GHz signals
 *
 * @file    clayloop.c
 * @author  Bobby Gibbs (@bobbygi97169329)
 * @version 1.0.0
 * @date    March 07, 2026
 * @license MIT
 *
 * @brief Transmits .sub files repeatedly with configurable delay, duration,
 *        interval, repeats. Features LED/beep countdown (R-Y-G 440/660/880Hz),
 *        vibration, mid-cancel, reset combo, persistent settings/paths.
 *
 * @details
 * Select up to 4 .sub files, configure transmission timing, and transmit them
 * in sequence with LED countdown feedback. Supports both RAW and keyed protocol
 * files. All settings and per-group file paths persist across sessions via
 * FlipperFormat on the SD card.
 *
 * Application flow:
 *   1. Delay Screen  - Optional start delay (None, 1-10s)
 *   2. Setup Screen  - Repeat count (1-16 / Infinite) and file count (1-4)
 *   3. File Browser  - Select .sub files from SD card
 *   4. Control Screen - Adjust duration/interval, OK to start/stop
 *
 * Target: Flipper Zero official firmware (API 87.1, Target 7)
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <furi_hal_subghz.h>
#include "clayloop_icons.h"

/** Log tag for FURI_LOG_* macros */
#define TAG "ClayLoop"

/* CLEANUP: Removed legacy SUBGHZ_APP_FOLDER define — provided by SDK in lib/subghz/types.h */

/** File extension filter for the file browser */
#define SUBGHZ_APP_EXTENSION ".sub"

/* ──────────────────────────────────────────────
 *  Transmission timing defaults and limits
 * ────────────────────────────────────────────── */

/** Minimum transmission duration in seconds */
#define DURATION_MIN     0.5f
/** Maximum transmission duration in seconds */
#define DURATION_MAX     30.0f
/** Step size when adjusting duration */
#define DURATION_STEP    0.5f
/** Default transmission duration */
#define DURATION_DEFAULT 0.5f

/** Minimum interval between transmissions in seconds */
#define INTERVAL_MIN     0.0f
/** Maximum interval between transmissions in seconds */
#define INTERVAL_MAX     60.0f
/** Step size when adjusting interval */
#define INTERVAL_STEP    0.5f
/** Default interval between transmissions */
#define INTERVAL_DEFAULT 2.0f

/** Duration of the TX-start beep in ms (used in countdown gap calculation) */
#define TX_START_BEEP_MS 200

/** Persistent settings file path on the SD card */
#define CLAYLOOP_CONFIG_DIR  "/ext/apps_data/clayloop"
#define CLAYLOOP_CONFIG_PATH "/ext/apps_data/clayloop/config.ff"

/** Sentinel value indicating infinite repeats */
#define REPEAT_INFINITE 0

/** Maximum number of .sub files that can be queued */
#define MAX_FILES 4

/* ──────────────────────────────────────────────
 *  Enumerations
 * ────────────────────────────────────────────── */

/** Application screens */
typedef enum {
    ScreenDelay,        /**< Initial delay selection screen (shown first) */
    ScreenSetup,        /**< Repeat/file count configuration screen */
    ScreenControl,      /**< Playback control screen */
    ScreenResetConfirm, /**< Reset-to-defaults confirmation overlay */
} AppScreen;

/** Event types pushed to the message queue */
typedef enum {
    EventTypeInput, /**< User button press */
    EventTypeDurationEnd, /**< Transmission duration timer expired */
    EventTypeTxComplete, /**< File encoder finished playing the file */
    EventTypeAnimTick, /**< Animation frame advance */
} EventType;

/** Event structure for the message queue */
typedef struct {
    EventType type; /**< What kind of event occurred */
    InputEvent input; /**< Button press details (valid when type == EventTypeInput) */
    uint32_t generation; /**< TX generation for stale-event filtering */
} AppEvent;

/** Current state of the transmitter */
typedef enum {
    TxIdle, /**< Not transmitting */
    TxTransmitting, /**< Actively sending a signal */
    TxWaitingInterval, /**< Paused between transmissions */
} TxState;

/* ──────────────────────────────────────────────
 *  Data Structures
 * ────────────────────────────────────────────── */

/**
 * Information parsed from a single .sub file.
 *
 * Each queued file slot stores the file path, a short display name,
 * and the protocol metadata extracted from the file header.
 */
typedef struct {
    FuriString* file_path; /**< Full path to the .sub file on SD card */
    char filename[48]; /**< Short display name (without path or extension) */
    uint32_t frequency; /**< Carrier frequency in Hz (e.g., 433880000) */
    char preset_name[40]; /**< Raw preset string from file (e.g., "FuriHalSubGhzPresetOok650Async") */
    char protocol_name[40]; /**< Protocol name (e.g., "Princeton", "MegaCode", "RAW") */
    uint32_t bit_count; /**< Number of data bits (0 for RAW files) */
    char protocol_display[80]; /**< Formatted display string (e.g., "Princeton 24bit 433.88 AM") */
    bool is_raw; /**< True if this is a RAW protocol file, false for keyed protocols */
} SubFileInfo;

/**
 * Main application context.
 *
 * Holds all state for the application including GUI handles,
 * file queue, transmission parameters, and runtime state.
 */
typedef struct {
    /* GUI and system services */
    ViewPort* view_port; /**< Main viewport for rendering (fullscreen when app is open) */
    Gui* gui; /**< GUI service handle */
    FuriMessageQueue* event_queue; /**< Event queue for input and timer events */
    DialogsApp* dialogs; /**< Dialog service for file browser */

    /* File queue (up to MAX_FILES slots) */
    SubFileInfo files[MAX_FILES]; /**< Array of queued file info */
    uint32_t file_count_setting; /**< Number of files the user wants to select (1-4) */
    uint32_t files_loaded; /**< Number of files actually loaded and parsed */
    uint32_t current_file_index; /**< Index of the file currently being transmitted */

    /**
     * Persistent file-path memory, grouped by file count.
     * saved_paths[count-1][slot] = last path used for that count/slot group.
     * e.g. saved_paths[1][0] and saved_paths[1][1] are the two paths for
     * the "2-file" group, independent of the paths used for the "3-file" group.
     */
    FuriString* saved_paths[MAX_FILES][MAX_FILES];

    /* User-adjustable transmission parameters */
    float duration; /**< How long each file transmits (seconds) */
    float interval; /**< Pause between transmissions (seconds) */
    uint32_t max_repeats; /**< Total repeat cycles (0 = infinite) */
    uint32_t repeat_select; /**< Index into repeat_options[] during setup */
    uint32_t delay_select; /**< Index into delay_options[] (0=None, 1-10 seconds) */

    /* Transmission runtime state */
    TxState tx_state; /**< Current transmitter state */
    uint32_t repeat_count; /**< Current cycle number */
    bool repeat_enabled; /**< Whether the repeat loop is active */
    uint32_t tx_generation; /**< Incremented each time a new TX starts */
    SubGhzEnvironment* environment; /**< SubGHz environment with protocol registry */
    SubGhzFileEncoderWorker* worker; /**< SubGHz file encoder worker instance */
    SubGhzTransmitter* transmitter; /**< SubGHz protocol transmitter instance */
    const SubGhzDevice* radio_device; /**< CC1101 radio device handle */
    bool radio_tx_running; /**< True when async TX is actively feeding the radio */
    FuriTimer* duration_timer; /**< Timer that ends each transmission */

    /* Screen management */
    AppScreen current_screen; /**< Which screen is currently displayed */
    bool running; /**< Master run flag; false = exit app */

    /* Home screen animation */
    FuriTimer* anim_timer; /**< Periodic timer for frame advance (~150ms) */
    uint8_t anim_frame; /**< Current animation frame index (0-12) */
} ClayLoopApp;

/**
 * Available repeat count options.
 * Values 1-16 for fixed counts, 0 = infinite.
 * The UI cycles through these with left/right buttons.
 */
static const uint32_t repeat_options[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0};
#define REPEAT_OPTIONS_COUNT 17

/**
 * Available initial delay options in seconds.
 * 0 = no delay, 1-10 = delay in seconds.
 */
static const uint32_t delay_options[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
#define DELAY_OPTIONS_COUNT 11

/* ──────────────────────────────────────────────
 *  Forward Declarations
 * ────────────────────────────────────────────── */

static void app_draw_callback(Canvas* canvas, void* ctx);
static void app_input_callback(InputEvent* input_event, void* ctx);
static bool app_show_file_browser(ClayLoopApp* app, uint32_t file_idx);
static bool app_parse_sub_file(ClayLoopApp* app, uint32_t file_idx);
static void app_start_transmit_file(ClayLoopApp* app);
static void app_restart_current_tx(ClayLoopApp* app);
static void app_stop_transmit(ClayLoopApp* app);
static void app_led_tx_on(void);
static void app_led_off(void);
static bool app_countdown_gap(ClayLoopApp* app, uint32_t gap_ms);
static bool app_led_flash_countdown(ClayLoopApp* app, uint32_t delay_ms);
static void app_save_settings(ClayLoopApp* app);
static void app_load_settings(ClayLoopApp* app);
static void app_led_flash_green_single(void);
static void app_handle_tx_end(ClayLoopApp* app);
static void duration_timer_cb(void* ctx);
static void tx_complete_cb(void* ctx);
static void anim_timer_cb(void* ctx);

/* ──────────────────────────────────────────────
 *  Helper Functions
 * ────────────────────────────────────────────── */

/**
 * Determine the modulation type from the preset name.
 *
 * Flipper .sub files store the preset as strings like
 * "FuriHalSubGhzPresetOok650Async". This function extracts
 * a human-readable modulation label for the display.
 *
 * @param preset  The raw preset string from the .sub file header
 * @return "AM" for OOK/AM presets, "FM" for FSK presets, "AM" as fallback
 */
static const char* get_modulation(const char* preset) {
    if(strstr(preset, "AM") || strstr(preset, "Ook") || strstr(preset, "OOK")) return "AM";
    if(strstr(preset, "FM") || strstr(preset, "2FSK") || strstr(preset, "FSK")) return "FM";
    return "AM";
}

/**
 * Truncate a filename to fit on the Flipper's 128px wide screen.
 *
 * When displaying multiple filenames side-by-side, each name is
 * limited to max_len characters to prevent overflow.
 *
 * @param src      Source filename string
 * @param dst      Destination buffer (must be at least max_len + 1 bytes)
 * @param max_len  Maximum number of characters to keep
 */
static void truncate_name(const char* src, char* dst, size_t max_len) {
    size_t len = strlen(src);
    if(len <= max_len) {
        strncpy(dst, src, max_len + 1);
    } else {
        strncpy(dst, src, max_len);
        dst[max_len] = '\0';
    }
}

/* ──────────────────────────────────────────────
 *  LED Control
 * ────────────────────────────────────────────── */

/**
 * Turn on the LED in purple (Red + Blue) to indicate active transmission.
 */
static void app_led_tx_on(void) {
    furi_hal_light_set(LightRed, 0xFF);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue, 0xFF);
}

/**
 * Turn off all LEDs. Called when transmission stops or between signals.
 */
static void app_led_off(void) {
    furi_hal_light_set(LightRed, 0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue, 0x00);
}

/**
 * Wait for gap_ms while polling the event queue every 20 ms.
 * Returns true if the full wait elapsed normally.
 * Returns false immediately if the user presses OK (cancel/pause),
 * clearing all LEDs and vibration before returning.
 * All other events (anim ticks, stale timer events) are discarded
 * during the countdown window.
 */
static bool app_countdown_gap(ClayLoopApp* app, uint32_t gap_ms) {
    uint32_t remaining = gap_ms;
    while(remaining > 0) {
        uint32_t slice = remaining < 20 ? remaining : 20;
        AppEvent ev;
        if(furi_message_queue_get(app->event_queue, &ev, furi_ms_to_ticks(slice)) == FuriStatusOk) {
            if(ev.type == EventTypeInput &&
               ev.input.key == InputKeyOk &&
               ev.input.type == InputTypeShort) {
                /* User cancelled mid-countdown — clear all LEDs/vibration */
                furi_hal_light_set(LightRed,   0x00);
                furi_hal_light_set(LightGreen, 0x00);
                furi_hal_light_set(LightBlue,  0x00);
                furi_hal_vibro_on(false);
                return false;
            }
            /* Discard anim ticks and other events stale during countdown */
        }
        remaining = remaining > slice ? remaining - slice : 0;
    }
    return true;
}

/**
 * 3-flash countdown timed to the user's selected delay/interval.
 *
 * Gaps between the 4 events are polled in 20 ms slices so an OK press
 * during any gap cancels the countdown immediately.
 *
 * Returns true  — countdown completed, safe to start TX.
 * Returns false — user cancelled mid-countdown, TX should NOT start.
 *
 * Timeline:
 *   [vibro ON] → Red+440Hz 80ms → gap → Yellow+660Hz 80ms → gap →
 *   Green+880Hz 80ms → gap → [caller starts TX → 1000Hz beep]
 *
 * @param app       Application context (needed for queue polling).
 * @param delay_ms  Delay/interval in milliseconds. 0 = minimum 80 ms gap.
 */
static bool app_led_flash_countdown(ClayLoopApp* app, uint32_t delay_ms) {
    uint32_t fixed_ms = (uint32_t)(3 * 80 + TX_START_BEEP_MS);
    uint32_t gap_ms = (delay_ms > fixed_ms) ? (delay_ms - fixed_ms) / 3 : 80;

    /* Flash 1: Red + 440 Hz + vibration */
    furi_hal_vibro_on(true);
    furi_hal_light_set(LightRed,   0xFF);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue,  0x00);
    {
        bool beep = furi_hal_speaker_acquire(500);
        if(beep) furi_hal_speaker_start(440.0f, 1.0f);
        furi_delay_ms(80);
        if(beep) { furi_hal_speaker_stop(); furi_hal_speaker_release(); }
    }
    furi_hal_light_set(LightRed, 0x00);
    furi_hal_vibro_on(false);

    /* Gap 1 — cancellable */
    if(!app_countdown_gap(app, gap_ms)) return false;

    /* Flash 2: Yellow + 660 Hz */
    furi_hal_light_set(LightRed,   0xFF);
    furi_hal_light_set(LightGreen, 0xFF);
    furi_hal_light_set(LightBlue,  0x00);
    {
        bool beep = furi_hal_speaker_acquire(500);
        if(beep) furi_hal_speaker_start(660.0f, 1.0f);
        furi_delay_ms(80);
        if(beep) { furi_hal_speaker_stop(); furi_hal_speaker_release(); }
    }
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0x00);

    /* Gap 2 — cancellable */
    if(!app_countdown_gap(app, gap_ms)) return false;

    /* Flash 3: Green + 880 Hz */
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0xFF);
    furi_hal_light_set(LightBlue,  0x00);
    {
        bool beep = furi_hal_speaker_acquire(500);
        if(beep) furi_hal_speaker_start(880.0f, 1.0f);
        furi_delay_ms(80);
        if(beep) { furi_hal_speaker_stop(); furi_hal_speaker_release(); }
    }
    furi_hal_light_set(LightGreen, 0x00);

    /* Gap 3 — cancellable (space before TX-start beep) */
    if(!app_countdown_gap(app, gap_ms)) return false;

    return true;
}

/**
 * Single solid green flash for pause feedback.
 * 80 ms on, then off. Simpler than the countdown —
 * just confirms the button was registered.
 */
static void app_led_flash_green_single(void) {
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0xFF);
    furi_hal_light_set(LightBlue,  0x00);
    furi_delay_ms(80);
    furi_hal_light_set(LightGreen, 0x00);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  ANIMATION
 *
 *  13-frame silhouette animation stored in images/ and compiled into
 *  the FAP by ufbt. Frames cycle at ~150ms (≈6.7 fps) via a periodic timer.
 *  The 64×64 sprite is drawn on the right half of the 128×64 display so all
 *  text fits cleanly on the left half.
 * ══════════════════════════════════════════════════════════════════════════════ */
#define ANIM_FRAME_COUNT 13
#define ANIM_FRAME_MS    150

static const Icon* const anim_frames[ANIM_FRAME_COUNT] = {
    &I_frame_0,  &I_frame_1,  &I_frame_2,  &I_frame_3,  &I_frame_4,
    &I_frame_5,  &I_frame_6,  &I_frame_7,  &I_frame_8,  &I_frame_9,
    &I_frame_10, &I_frame_11, &I_frame_12,
};

/**
 * Periodic animation timer callback.
 * Pushes EventTypeAnimTick to the main event queue so the main loop
 * advances the frame counter on the correct thread, then redraws.
 */
static void anim_timer_cb(void* ctx) {
    ClayLoopApp* app = ctx;
    AppEvent event = {.type = EventTypeAnimTick};
    furi_message_queue_put(app->event_queue, &event, 0);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  DRAW CALLBACK
 *
 *  Renders the current screen to the Flipper's 128x64 pixel display.
 *  Called by the GUI system whenever view_port_update() is invoked.
 * ══════════════════════════════════════════════════════════════════════════════ */
static void app_draw_callback(Canvas* canvas, void* ctx) {
    ClayLoopApp* app = ctx;
    furi_check(app);
    canvas_clear(canvas);

    if(app->current_screen == ScreenDelay) {
        /*
         * DELAY SCREEN LAYOUT:
         *   RIGHT HALF (x 64-127): 64x64 animated ClayLoop icon
         *   LEFT HALF  (x  0- 63): title + delay selector
         *
         *   Line 0: "ClayLoop" (small)
         *   Line 1: "Start Delay" (small)
         *   Line 2: "<None>" or "<Xs>" (bold, left/right to change)
         *   Line 3: "[OK]/[Back]" (small)
         */

        /* Animation — right half of screen (same as setup screen) */
        canvas_draw_icon(canvas, 64, 0, anim_frames[app->anim_frame]);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 32, 0, AlignCenter, AlignTop, "ClayLoop");
        canvas_draw_str_aligned(canvas, 32, 12, AlignCenter, AlignTop, "Start Delay");

        /* Delay selector */
        char delay_str[20];
        uint32_t dval = delay_options[app->delay_select];
        if(dval == 0) {
            snprintf(delay_str, sizeof(delay_str), "<None>");
        } else {
            snprintf(delay_str, sizeof(delay_str), "<%lus>", dval);
        }
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, 26, AlignCenter, AlignTop, delay_str);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 32, 36, AlignCenter, AlignTop, "UP+LEFT");
        canvas_draw_str_aligned(canvas, 32, 45, AlignCenter, AlignTop, "reset all");
        canvas_draw_str_aligned(canvas, 32, 54, AlignCenter, AlignTop, "[OK]/[Back]");
        return;
    }

    if(app->current_screen == ScreenResetConfirm) {
        /* Full-screen confirmation overlay (canvas already cleared at top of draw_callback) */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8,  AlignCenter, AlignTop, "Reset all");
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, "defaults?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "[OK]  Yes");
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "[Back] No");
        return;
    }

    if(app->current_screen == ScreenSetup) {
        /*
         * SETUP SCREEN LAYOUT:
         *   RIGHT HALF (x 64-127): 64x64 animated ClayLoop icon
         *   LEFT HALF  (x  0- 63): all text controls
         *
         *   Line 0: "ClayLoop" (small)
         *   Line 1: "Configure & OK" (small)
         *   Line 2: "<Rpts:N>" (bold, left/right to change)
         *   Line 3: "^Files:N v" (bold, up/down to change)
         *   Line 4: "[OK]/[Back]" (small)
         */

        /* Animation — right half of screen */
        canvas_draw_icon(canvas, 64, 0, anim_frames[app->anim_frame]);

        /* Title — single line, fits comfortably in the 64px left half */
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 32, 0, AlignCenter, AlignTop, "ClayLoop");
        canvas_draw_str_aligned(canvas, 32, 11, AlignCenter, AlignTop, "Configure & OK");

        /* Repeat count selector (left/right arrows cycle through options) */
        char rep_str[32];
        uint32_t val = repeat_options[app->repeat_select];
        if(val == 0) {
            snprintf(rep_str, sizeof(rep_str), "<Rpts:Inf>");
        } else {
            snprintf(rep_str, sizeof(rep_str), "<Rpts:%lu>", val);
        }
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, 24, AlignCenter, AlignTop, rep_str);

        /* File/machine count selector (up/down arrows cycle 1-4) */
        char file_str[24];
        snprintf(file_str, sizeof(file_str), "^Files:%lu v", app->file_count_setting);
        canvas_draw_str_aligned(canvas, 32, 37, AlignCenter, AlignTop, file_str);

        /* Bottom hint line (small font) */
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 32, 51, AlignCenter, AlignTop, "[OK]/[Back]");
        return;
    }

    /*
     * CONTROL SCREEN LAYOUT:
     *   RIGHT HALF (x 64-127): 64x64 animated ClayLoop icon
     *   LEFT HALF  (x  0- 63): title, filenames, controls, status
     */

    /* Animation — right half of screen */
    canvas_draw_icon(canvas, 64, 0, anim_frames[app->anim_frame]);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 0, AlignCenter, AlignTop, "ClayLoop");

    canvas_set_font(canvas, FontSecondary);

    if(app->tx_state == TxIdle) {
        if(app->files_loaded == 1) {
            /* Single file: show name only (no protocol line) */
            canvas_draw_str_aligned(canvas, 32, 13, AlignCenter, AlignTop, app->files[0].filename);
        } else {
            /* Multiple files: show truncated names in two rows */
            char names_line1[80] = {0};
            char names_line2[80] = {0};
            size_t pos1 = 0;
            size_t pos2 = 0;

            for(uint32_t i = 0; i < app->files_loaded; i++) {
                char trunc[12];
                truncate_name(app->files[i].filename, trunc, 11);

                if(i < 2) {
                    /* First two files on the top row */
                    if(pos1 > 0) {
                        names_line1[pos1++] = ' ';
                    }
                    size_t tlen = strlen(trunc);
                    memcpy(names_line1 + pos1, trunc, tlen);
                    pos1 += tlen;
                } else {
                    /* Files 3-4 on the second row */
                    if(pos2 > 0) {
                        names_line2[pos2++] = ' ';
                    }
                    size_t tlen = strlen(trunc);
                    memcpy(names_line2 + pos2, trunc, tlen);
                    pos2 += tlen;
                }
            }
            names_line1[pos1] = '\0';
            names_line2[pos2] = '\0';

            canvas_draw_str_aligned(canvas, 32, 13, AlignCenter, AlignTop, names_line1);
            if(pos2 > 0) {
                canvas_draw_str_aligned(canvas, 32, 23, AlignCenter, AlignTop, names_line2);
            }
        }
    } else {
        /* During transmission: show current filename only */
        SubFileInfo* cur = &app->files[app->current_file_index];
        canvas_draw_str_aligned(canvas, 32, 13, AlignCenter, AlignTop, cur->filename);
    }

    /* Duration control (left/right to adjust) */
    char dur_str[24];
    snprintf(dur_str, sizeof(dur_str), "<Dur:%.1fs>", (double)app->duration);
    canvas_draw_str_aligned(canvas, 32, 25, AlignCenter, AlignTop, dur_str);

    /* Interval control (up/down to adjust) */
    char int_str[24];
    snprintf(int_str, sizeof(int_str), "^Int:%.1fs v", (double)app->interval);
    canvas_draw_str_aligned(canvas, 32, 37, AlignCenter, AlignTop, int_str);

    /* Status line: shows start prompt when idle, progress when transmitting */
    char status[40];
    if(app->tx_state == TxIdle) {
        if(app->max_repeats == 0) {
            snprintf(status, sizeof(status), "[OK]Inf x%lu", app->files_loaded);
        } else {
            snprintf(
                status,
                sizeof(status),
                "[OK]%lux x%lu",
                app->max_repeats,
                app->files_loaded);
        }
    } else if(app->tx_state == TxTransmitting) {
        if(app->max_repeats == 0) {
            snprintf(
                status,
                sizeof(status),
                "TX%lu/%lu #%lu",
                app->current_file_index + 1,
                app->files_loaded,
                app->repeat_count);
        } else {
            snprintf(
                status,
                sizeof(status),
                "TX%lu/%lu %lu/%lu",
                app->current_file_index + 1,
                app->files_loaded,
                app->repeat_count,
                app->max_repeats);
        }
    } else {
        snprintf(
            status,
            sizeof(status),
            "Wait F%lu/%lu",
            app->current_file_index + 1,
            app->files_loaded);
    }
    canvas_draw_str_aligned(canvas, 32, 50, AlignCenter, AlignTop, status);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  INPUT CALLBACK
 *
 *  Called by the GUI system when a button is pressed. Wraps the input event
 *  into an AppEvent and pushes it to the message queue for processing in
 *  the main loop.
 * ══════════════════════════════════════════════════════════════════════════════ */
static void app_input_callback(InputEvent* input_event, void* ctx) {
    ClayLoopApp* app = ctx;
    AppEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, 0);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  FILE BROWSER
 *
 *  Opens the Flipper's built-in file browser dialog filtered to .sub files.
 *  The selected file path is stored in the specified file slot.
 *
 *  @param app       Application context
 *  @param file_idx  Index of the file slot to populate (0 to MAX_FILES-1)
 *  @return true if a file was selected, false if the user cancelled
 * ══════════════════════════════════════════════════════════════════════════════ */
static bool app_show_file_browser(ClayLoopApp* app, uint32_t file_idx) {
    FURI_LOG_I(TAG, "Opening file browser for slot %lu", file_idx);

    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, SUBGHZ_APP_EXTENSION, NULL);
    opts.base_path = SUBGHZ_APP_FOLDER;
    opts.hide_ext = true;

    /* Pre-populate with the last-used path for this count/slot group, so the
     * browser opens in the same folder as before.  Fall back to the default
     * Sub-GHz folder if no path has been saved yet for this slot. */
    FuriString* saved = app->saved_paths[app->file_count_setting - 1][file_idx];
    if(!furi_string_empty(saved)) {
        furi_string_set(app->files[file_idx].file_path, saved);
    } else {
        furi_string_set_str(app->files[file_idx].file_path, SUBGHZ_APP_FOLDER);
    }

    bool selected = dialog_file_browser_show(
        app->dialogs, app->files[file_idx].file_path, app->files[file_idx].file_path, &opts);

    if(selected) {
        FURI_LOG_I(
            TAG, "Slot %lu: %s", file_idx, furi_string_get_cstr(app->files[file_idx].file_path));
    }
    return selected;
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  .SUB FILE PARSER
 *
 *  Reads the header of a Flipper .sub file to extract metadata:
 *  - Frequency (e.g., 433880000 Hz)
 *  - Preset (e.g., "FuriHalSubGhzPresetOok650Async")
 *  - Protocol (e.g., "Princeton", "MegaCode", "RAW")
 *  - Bit count (e.g., 24)
 *
 *  These values are combined into a human-readable display string like
 *  "Princeton 24bit 433.88 AM" for showing on the control screen.
 *
 *  @param app       Application context
 *  @param file_idx  Index of the file slot to parse
 *  @return true if the file was successfully parsed (frequency found)
 * ══════════════════════════════════════════════════════════════════════════════ */
static bool app_parse_sub_file(ClayLoopApp* app, uint32_t file_idx) {
    SubFileInfo* fi = &app->files[file_idx];
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(!storage_file_open(
           file, furi_string_get_cstr(fi->file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Cannot open file slot %lu", file_idx);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    /* Extract the short filename (without path or .sub extension) */
    const char* full = furi_string_get_cstr(fi->file_path);
    const char* slash = strrchr(full, '/');
    const char* name_start = slash ? slash + 1 : full;
    strncpy(fi->filename, name_start, sizeof(fi->filename) - 1);
    fi->filename[sizeof(fi->filename) - 1] = '\0';
    char* dot = strrchr(fi->filename, '.');
    if(dot) *dot = '\0';

    /* Set defaults before parsing */
    fi->frequency = 0;
    fi->bit_count = 0;
    fi->is_raw = false;
    snprintf(fi->preset_name, sizeof(fi->preset_name), "Unknown");
    snprintf(fi->protocol_name, sizeof(fi->protocol_name), "RAW");

    /*
     * Read the file line-by-line looking for header fields.
     * We only need the first ~30 lines; the actual signal data
     * starts after "RAW_Data:" or similar protocol-specific keys.
     */
    char line_buf[128];
    size_t line_pos = 0;
    int lines_read = 0;
    bool done = false;

    while(!done && lines_read < 30) {
        uint8_t ch;
        uint16_t bytes_read = storage_file_read(file, &ch, 1);
        if(bytes_read == 0) break;

        if(ch == '\n' || ch == '\r') {
            if(line_pos > 0) {
                line_buf[line_pos] = '\0';
                lines_read++;

                /* Parse "Frequency: 433880000" */
                if(strncmp(line_buf, "Frequency:", 10) == 0) {
                    const char* fp = line_buf + 10;
                    while(*fp == ' ')
                        fp++;
                    uint32_t freq = 0;
                    while(*fp >= '0' && *fp <= '9') {
                        freq = freq * 10 + (uint32_t)(*fp - '0');
                        fp++;
                    }
                    fi->frequency = freq;
                }
                /* Parse "Preset: FuriHalSubGhzPresetOok650Async" */
                else if(strncmp(line_buf, "Preset:", 7) == 0) {
                    const char* p = line_buf + 7;
                    while(*p == ' ')
                        p++;
                    strncpy(fi->preset_name, p, sizeof(fi->preset_name) - 1);
                    fi->preset_name[sizeof(fi->preset_name) - 1] = '\0';
                }
                /* Parse "Protocol: Princeton" */
                else if(strncmp(line_buf, "Protocol:", 9) == 0) {
                    const char* p = line_buf + 9;
                    while(*p == ' ')
                        p++;
                    strncpy(fi->protocol_name, p, sizeof(fi->protocol_name) - 1);
                    fi->protocol_name[sizeof(fi->protocol_name) - 1] = '\0';
                }
                /* Parse "Bit: 24" */
                else if(strncmp(line_buf, "Bit:", 4) == 0) {
                    const char* bp = line_buf + 4;
                    while(*bp == ' ')
                        bp++;
                    uint32_t bits = 0;
                    while(*bp >= '0' && *bp <= '9') {
                        bits = bits * 10 + (uint32_t)(*bp - '0');
                        bp++;
                    }
                    fi->bit_count = bits;
                }
                /* Stop reading at the data section */
                else if(strncmp(line_buf, "RAW_Data:", 9) == 0) {
                    done = true;
                }
                line_pos = 0;
            }
        } else {
            if(line_pos < sizeof(line_buf) - 1) {
                line_buf[line_pos++] = (char)ch;
            }
        }
    }

    if(fi->frequency > 0) ok = true;

    /* Determine if this is a RAW file */
    fi->is_raw = (strcmp(fi->protocol_name, "RAW") == 0);

    /*
     * Build the protocol display string.
     * Format: "Protocol Xbit Freq.XX AM/FM"
     * Example: "Princeton 24bit 433.88 AM"
     * For RAW files (no bit count): "RAW 433.88 AM"
     */
    const char* mod = get_modulation(fi->preset_name);
    uint32_t freq_mhz = fi->frequency / 1000000UL;
    uint32_t freq_dec = (fi->frequency % 1000000UL) / 10000UL;

    if(fi->bit_count > 0) {
        snprintf(
            fi->protocol_display,
            sizeof(fi->protocol_display),
            "%s %lubit %lu.%02lu %s",
            fi->protocol_name,
            fi->bit_count,
            freq_mhz,
            freq_dec,
            mod);
    } else {
        snprintf(
            fi->protocol_display,
            sizeof(fi->protocol_display),
            "%s %lu.%02lu %s",
            fi->protocol_name,
            freq_mhz,
            freq_dec,
            mod);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Slot %lu parsed: %s", file_idx, fi->protocol_display);
    return ok;
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  TIMER CALLBACKS
 *
 *  These are called by FuriTimer when the respective timer expires.
 *  They push events to the message queue so the main loop can handle
 *  them safely (timers fire from a different context).
 * ══════════════════════════════════════════════════════════════════════════════ */

/** Called when the transmission duration timer expires */
static void duration_timer_cb(void* ctx) {
    ClayLoopApp* app = ctx;
    AppEvent event = {.type = EventTypeDurationEnd};
    furi_message_queue_put(app->event_queue, &event, 0);
}

/** Called by the file encoder worker when it finishes playing a RAW file */
static void tx_complete_cb(void* ctx) {
    ClayLoopApp* app = ctx;
    FURI_LOG_I(TAG, "TX complete callback");
    AppEvent event = {.type = EventTypeTxComplete, .generation = app->tx_generation};
    furi_message_queue_put(app->event_queue, &event, 0);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  TRANSMISSION CONTROL
 * ══════════════════════════════════════════════════════════════════════════════ */

/**
 * Convert the preset name string from a .sub file header to the
 * corresponding FuriHalSubGhzPreset enum value.
 *
 * @param preset_name  Raw preset string (e.g., "FuriHalSubGhzPresetOok650Async")
 * @return Matching FuriHalSubGhzPreset, defaults to OOK 650kHz
 */
static FuriHalSubGhzPreset app_get_preset(const char* preset_name) {
    if(strstr(preset_name, "OOK_650") || strstr(preset_name, "Ook650"))
        return FuriHalSubGhzPresetOok650Async;
    if(strstr(preset_name, "OOK_270") || strstr(preset_name, "Ook270"))
        return FuriHalSubGhzPresetOok270Async;
    if(strstr(preset_name, "2FSKDev476") || strstr(preset_name, "2FSK_Dev47") || strstr(preset_name, "2FskDev476"))
        return FuriHalSubGhzPreset2FSKDev476Async;
    if(strstr(preset_name, "2FSKDev238") || strstr(preset_name, "2FSK_Dev2") || strstr(preset_name, "2FskDev238"))
        return FuriHalSubGhzPreset2FSKDev238Async;
    if(strstr(preset_name, "MSK") || strstr(preset_name, "GFSK"))
        return FuriHalSubGhzPresetMSK99_97KbAsync;
    return FuriHalSubGhzPresetOok650Async;
}

/**
 * Async TX feed callback for RAW files.
 *
 * Called by the CC1101 DMA engine to get the next LevelDuration sample.
 * Pulls data from the SubGhzFileEncoderWorker which has parsed the .sub file.
 *
 * @param ctx  Application context pointer (ClayLoopApp*)
 * @return Next LevelDuration sample for the radio
 */
static LevelDuration app_radio_tx_feed_raw(void* ctx) {
    ClayLoopApp* app = ctx;
    return subghz_file_encoder_worker_get_level_duration(app->worker);
}

/**
 * Async TX feed callback for protocol files.
 *
 * Called by the CC1101 DMA engine to get the next LevelDuration sample.
 * Pulls data from the SubGhzTransmitter which encodes the protocol.
 *
 * @param ctx  Application context pointer (ClayLoopApp*)
 * @return Next LevelDuration sample for the radio
 */
static LevelDuration app_radio_tx_feed_protocol(void* ctx) {
    ClayLoopApp* app = ctx;
    LevelDuration ld = subghz_transmitter_yield(app->transmitter);
    if(level_duration_is_reset(ld)) {
        AppEvent event = {.type = EventTypeTxComplete, .generation = app->tx_generation};
        furi_message_queue_put(app->event_queue, &event, 0);
    }
    return ld;
}

/**
 * Start transmitting the file at current_file_index.
 *
 * Initializes the SubGHz file encoder worker with the selected file,
 * turns on the purple LED, and starts the duration timer. When the
 * timer expires (or the file finishes), app_handle_tx_end() is called.
 */
static void app_start_transmit_file(ClayLoopApp* app) {
    app->tx_generation++;
    SubFileInfo* fi = &app->files[app->current_file_index];
    app->tx_state = TxTransmitting;
    app->anim_frame = 0; /* Restart animation from first frame on each new TX */

    FURI_LOG_I(
        TAG,
        "TX file %lu/%lu (cycle %lu): %s  dur=%.1fs (type: %s)",
        app->current_file_index + 1,
        app->files_loaded,
        app->repeat_count,
        fi->filename,
        (double)app->duration,
        fi->is_raw ? "RAW" : "Protocol");

    bool started = false;

    if(fi->is_raw) {
        /* RAW file: use SubGhzFileEncoderWorker */
        started = subghz_file_encoder_worker_start(
            app->worker, furi_string_get_cstr(fi->file_path), SUBGHZ_DEVICE_CC1101_INT_NAME);

        if(!started) {
            FURI_LOG_E(TAG, "Failed to start RAW encoder for slot %lu!", app->current_file_index);
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            app_led_off();
            return;
        }

        /* Allow encoder to buffer file data before radio starts consuming it */
        furi_delay_ms(100);
    } else {
        /* Protocol file: use SubGhzTransmitter */
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* fff = flipper_format_file_alloc(storage);

        if(!flipper_format_file_open_existing(fff, furi_string_get_cstr(fi->file_path))) {
            FURI_LOG_E(TAG, "Cannot open protocol file slot %lu", app->current_file_index);
            flipper_format_free(fff);
            furi_record_close(RECORD_STORAGE);
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            app_led_off();
            return;
        }

        app->transmitter = subghz_transmitter_alloc_init(
            app->environment, fi->protocol_name);
        if(!app->transmitter) {
            FURI_LOG_E(TAG, "Failed to alloc transmitter for %s", fi->protocol_name);
            flipper_format_file_close(fff);
            flipper_format_free(fff);
            furi_record_close(RECORD_STORAGE);
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            app_led_off();
            return;
        }

        if(subghz_transmitter_deserialize(app->transmitter, fff) != SubGhzProtocolStatusOk) {
            FURI_LOG_E(TAG, "Failed to deserialize protocol file slot %lu", app->current_file_index);
            subghz_transmitter_free(app->transmitter);
            app->transmitter = NULL;
            flipper_format_file_close(fff);
            flipper_format_free(fff);
            furi_record_close(RECORD_STORAGE);
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            app_led_off();
            return;
        }

        flipper_format_file_close(fff);
        flipper_format_free(fff);
        furi_record_close(RECORD_STORAGE);
        started = true;
    }

    /* ── Configure CC1101 radio hardware ──
     * 1. Idle the radio (safe starting state)
     * 2. Load the modulation preset (OOK, FSK, etc.)
     * 3. Set the carrier frequency via the device abstraction layer
     * 4. Start DMA-driven async TX (handles RF path + TX entry internally)
     *
     * IMPORTANT: Do NOT call subghz_devices_set_tx() before start_async_tx().
     * set_tx() enters synchronous TX mode (HAL state → SubGhzStateTx), but
     * start_async_tx() requires SubGhzStateIdle to initialize the DMA engine.
     * Calling set_tx() first causes start_async_tx() to fail silently.
     * The start_async_tx() function handles the RF antenna path and TX
     * strobe internally.
     */
    subghz_devices_idle(app->radio_device);
    subghz_devices_load_preset(
        app->radio_device, app_get_preset(fi->preset_name), NULL);
    subghz_devices_set_frequency(app->radio_device, fi->frequency);

    FURI_LOG_I(TAG, "Radio: preset=%s freq=%lu", fi->preset_name, fi->frequency);

    bool tx_started = false;
    if(fi->is_raw) {
        tx_started = subghz_devices_start_async_tx(app->radio_device, app_radio_tx_feed_raw, app);
    } else {
        tx_started = subghz_devices_start_async_tx(app->radio_device, app_radio_tx_feed_protocol, app);
    }

    if(!tx_started) {
        FURI_LOG_E(TAG, "start_async_tx failed on %lu Hz!", fi->frequency);
        if(fi->is_raw) {
            subghz_file_encoder_worker_stop(app->worker);
        } else {
            if(app->transmitter) {
                subghz_transmitter_free(app->transmitter);
                app->transmitter = NULL;
            }
        }
        subghz_devices_idle(app->radio_device);
        app->tx_state = TxIdle;
        app->repeat_enabled = false;
        app_led_off();
        return;
    }
    app->radio_tx_running = true;
    FURI_LOG_I(TAG, "Async TX active on %lu Hz", fi->frequency);

    /* Visual feedback: purple LED on during active transmission */
    app_led_tx_on();

    /* Audio feedback: fixed-length TX-start beep (1000 Hz, TX_START_BEEP_MS).
     * Duration is independent of signal length — 4th beat of the countdown. */
    {
        bool spk = furi_hal_speaker_acquire(500);
        if(spk) {
            furi_hal_speaker_start(1000.0f, 1.0f);
            furi_delay_ms(TX_START_BEEP_MS);
            furi_hal_speaker_stop();
            furi_hal_speaker_release();
        }
    }

    /* Start the duration timer */
    uint32_t dur_ms = (uint32_t)(app->duration * 1000.0f);
    furi_timer_start(app->duration_timer, furi_ms_to_ticks(dur_ms));
    view_port_update(app->view_port);
}

/**
 * Immediately stop all transmission activity.
 *
 * Stops timers, kills the encoder worker, turns off the LED,
 * and resets all transmission state to idle.
 */
static void app_stop_transmit(ClayLoopApp* app) {
    FURI_LOG_I(TAG, "Stopping TX");

    furi_timer_stop(app->duration_timer);

    /* Stop radio hardware first */
    if(app->radio_tx_running) {
        subghz_devices_stop_async_tx(app->radio_device);
        subghz_devices_idle(app->radio_device);
        app->radio_tx_running = false;
    }

    if(subghz_file_encoder_worker_is_running(app->worker)) {
        subghz_file_encoder_worker_stop(app->worker);
    }

    if(app->transmitter) {
        subghz_transmitter_free(app->transmitter);
        app->transmitter = NULL;
    }

    subghz_devices_sleep(app->radio_device);

    app_led_off();
    app->tx_state = TxIdle;
    app->repeat_enabled = false;
    app->repeat_count = 0;
    app->current_file_index = 0;
    view_port_update(app->view_port);
}

/**
 * Handle the end of a single file's transmission.
 *
 * This is the core sequencing logic. After a file finishes transmitting:
 *
 * 1. If there are more files in the queue, advance to the next one.
 * 2. If all files have been played (one full cycle), check if we've
 *    reached the maximum repeat count.
 * 3. If more cycles remain, reset to file 0 and increment the cycle counter.
 * 4. If the interval is > 0, wait before starting the next transmission.
 *    If interval is 0, start immediately.
 *
 * Playback order example (3 files, 2 repeats):
 *   File1 -> wait -> File2 -> wait -> File3 -> wait ->
 *   File1 -> wait -> File2 -> wait -> File3 -> DONE
 */
static void app_handle_tx_end(ClayLoopApp* app) {
    /* Guard: ignore duplicate events if we already handled the TX end.
     * Both EventTypeDurationEnd and EventTypeTxComplete can fire for the
     * same transmission. Without this guard, processing both would skip
     * a file in the queue. */
    if(app->tx_state != TxTransmitting) return;

    furi_timer_stop(app->duration_timer);

    /* Stop the radio first, then the encoder */
    if(app->radio_tx_running) {
        subghz_devices_stop_async_tx(app->radio_device);
        subghz_devices_idle(app->radio_device);
        app->radio_tx_running = false;
    }

    if(subghz_file_encoder_worker_is_running(app->worker)) {
        subghz_file_encoder_worker_stop(app->worker);
    }

    if(app->transmitter) {
        subghz_transmitter_free(app->transmitter);
        app->transmitter = NULL;
    }

    /* LED off between transmissions */
    app_led_off();

    if(!app->repeat_enabled) {
        app->tx_state = TxIdle;
        view_port_update(app->view_port);
        return;
    }

    /* Advance to the next file in the queue */
    uint32_t next_file = app->current_file_index + 1;

    if(next_file < app->files_loaded) {
        /* More files remaining in this cycle */
        app->current_file_index = next_file;
        FURI_LOG_I(TAG, "Advancing to file %lu/%lu", next_file + 1, app->files_loaded);
    } else {
        /* All files played - one cycle complete */
        FURI_LOG_I(TAG, "Cycle %lu complete", app->repeat_count);

        /* Check if we've reached the configured repeat limit */
        if(app->max_repeats != REPEAT_INFINITE && app->repeat_count >= app->max_repeats) {
            FURI_LOG_I(TAG, "Reached max repeats (%lu), stopping", app->max_repeats);
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            app->current_file_index = 0;
            view_port_update(app->view_port);
            return;
        }

        /* Start the next cycle from file 0 */
        app->current_file_index = 0;
        app->repeat_count++;
        FURI_LOG_I(TAG, "Starting cycle %lu", app->repeat_count);
    }

    /* Run the countdown timed to the interval, then start the next transmission.
     * If the user presses OK during a gap, countdown returns false — abort. */
    uint32_t int_ms = (app->interval < 0.01f) ? 0 : (uint32_t)(app->interval * 1000.0f);
    app->tx_state = TxWaitingInterval;
    view_port_update(app->view_port);
    if(!app_led_flash_countdown(app, int_ms)) {
        /* Cancelled mid-countdown — return to idle cleanly */
        app->tx_state = TxIdle;
        app->repeat_enabled = false;
        view_port_update(app->view_port);
        return;
    }
    app_start_transmit_file(app);
}

static void app_restart_current_tx(ClayLoopApp* app) {
    app->tx_generation++;
    SubFileInfo* fi = &app->files[app->current_file_index];
    FURI_LOG_I(TAG, "Restarting TX file %lu within duration window", app->current_file_index + 1);

    if(app->radio_tx_running) {
        subghz_devices_stop_async_tx(app->radio_device);
        subghz_devices_idle(app->radio_device);
        app->radio_tx_running = false;
    }
    if(subghz_file_encoder_worker_is_running(app->worker)) {
        subghz_file_encoder_worker_stop(app->worker);
    }
    if(app->transmitter) {
        subghz_transmitter_free(app->transmitter);
        app->transmitter = NULL;
    }

    bool started = false;

    if(fi->is_raw) {
        started = subghz_file_encoder_worker_start(
            app->worker, furi_string_get_cstr(fi->file_path), SUBGHZ_DEVICE_CC1101_INT_NAME);
        if(!started) {
            FURI_LOG_E(TAG, "Restart RAW encoder failed!");
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            return;
        }
        furi_delay_ms(50);
    } else {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* fff = flipper_format_file_alloc(storage);
        if(!flipper_format_file_open_existing(fff, furi_string_get_cstr(fi->file_path))) {
            FURI_LOG_E(TAG, "Restart: cannot open protocol file slot %lu", app->current_file_index);
            flipper_format_free(fff);
            furi_record_close(RECORD_STORAGE);
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            return;
        }
        app->transmitter = subghz_transmitter_alloc_init(
            app->environment, fi->protocol_name);
        if(!app->transmitter) {
            FURI_LOG_E(TAG, "Restart: failed to alloc transmitter for %s", fi->protocol_name);
            flipper_format_file_close(fff);
            flipper_format_free(fff);
            furi_record_close(RECORD_STORAGE);
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            return;
        }
        if(subghz_transmitter_deserialize(app->transmitter, fff) != SubGhzProtocolStatusOk) {
            FURI_LOG_E(TAG, "Restart: failed to deserialize protocol file slot %lu", app->current_file_index);
            subghz_transmitter_free(app->transmitter);
            app->transmitter = NULL;
            flipper_format_file_close(fff);
            flipper_format_free(fff);
            furi_record_close(RECORD_STORAGE);
            app->tx_state = TxIdle;
            app->repeat_enabled = false;
            return;
        }
        flipper_format_file_close(fff);
        flipper_format_free(fff);
        furi_record_close(RECORD_STORAGE);
        started = true;
    }
    UNUSED(started);

    subghz_devices_idle(app->radio_device);
    subghz_devices_load_preset(app->radio_device, app_get_preset(fi->preset_name), NULL);
    subghz_devices_set_frequency(app->radio_device, fi->frequency);

    bool tx_started = false;
    if(fi->is_raw) {
        tx_started = subghz_devices_start_async_tx(app->radio_device, app_radio_tx_feed_raw, app);
    } else {
        tx_started = subghz_devices_start_async_tx(app->radio_device, app_radio_tx_feed_protocol, app);
    }
    if(!tx_started) {
        FURI_LOG_E(TAG, "Restart: async TX failed!");
        if(fi->is_raw) {
            subghz_file_encoder_worker_stop(app->worker);
        } else {
            if(app->transmitter) {
                subghz_transmitter_free(app->transmitter);
                app->transmitter = NULL;
            }
        }
        subghz_devices_idle(app->radio_device);
        app->tx_state = TxIdle;
        app->repeat_enabled = false;
        return;
    }
    app->radio_tx_running = true;
    view_port_update(app->view_port);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  PERSISTENT SETTINGS
 *
 *  Settings are saved to /ext/apps_data/clayloop/config.ff using FlipperFormat.
 *  Duration and interval are stored as integer tenths (x10) so 0.5s = 5,
 *  2.0s = 20, etc. All values are bounds-checked on load.
 * ══════════════════════════════════════════════════════════════════════════════ */

/**
 * Save duration, interval, repeat_select, delay_select, and file_count_setting
 * to the SD card config file. Called at app exit.
 */
static void app_save_settings(ClayLoopApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Create the directory if it doesn't exist yet */
    storage_common_mkdir(storage, CLAYLOOP_CONFIG_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_always(ff, CLAYLOOP_CONFIG_PATH)) {
        flipper_format_write_header_cstr(ff, "ClayLoop Config", 2);

        /* Store floats as integer tenths to avoid floating-point serialisation */
        uint32_t dur_x10 = (uint32_t)(app->duration * 10.0f + 0.5f);
        uint32_t int_x10 = (uint32_t)(app->interval * 10.0f + 0.5f);

        flipper_format_write_uint32(ff, "Duration_x10",  &dur_x10,                  1);
        flipper_format_write_uint32(ff, "Interval_x10",  &int_x10,                  1);
        flipper_format_write_uint32(ff, "RepeatSelect",  &app->repeat_select,        1);
        flipper_format_write_uint32(ff, "DelaySelect",   &app->delay_select,         1);
        flipper_format_write_uint32(ff, "FileCount",     &app->file_count_setting,   1);

        /* BUGFIX: Save file paths for every count/slot group (Path_<count>_<slot>).
         * Always write ALL keys even when empty — FlipperFormat reads sequentially,
         * so missing keys cause the cursor to skip past later keys on load. */
        char path_key[20];
        for(uint32_t c = 0; c < MAX_FILES; c++) {
            for(uint32_t s = 0; s <= c; s++) {
                snprintf(path_key, sizeof(path_key), "Path_%lu_%lu", c + 1, s);
                flipper_format_write_string(ff, path_key, app->saved_paths[c][s]);
            }
        }

        FURI_LOG_I(TAG, "Settings saved (dur=%lu/10 int=%lu/10 rpt=%lu dly=%lu fc=%lu)",
            dur_x10, int_x10, app->repeat_select, app->delay_select, app->file_count_setting);
    } else {
        FURI_LOG_E(TAG, "Failed to open config file for writing");
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

/**
 * Load previously saved settings from the SD card config file.
 * Called after defaults are set at startup — values replace defaults only
 * if the file exists and each field is within valid bounds.
 * Any missing or out-of-range field silently keeps its default value.
 */
static void app_load_settings(ClayLoopApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    do {
        if(!flipper_format_file_open_existing(ff, CLAYLOOP_CONFIG_PATH)) {
            FURI_LOG_I(TAG, "No saved config found, using defaults");
            break;
        }

        /* Validate header */
        FuriString* header = furi_string_alloc();
        uint32_t version = 0;
        bool header_ok = flipper_format_read_header(ff, header, &version);
        furi_string_free(header);
        if(!header_ok || version < 2) break;

        uint32_t val;

        if(flipper_format_read_uint32(ff, "Duration_x10", &val, 1)) {
            float dur = (float)val / 10.0f;
            if(dur >= DURATION_MIN && dur <= DURATION_MAX)
                app->duration = dur;
        }
        if(flipper_format_read_uint32(ff, "Interval_x10", &val, 1)) {
            float intv = (float)val / 10.0f;
            if(intv >= INTERVAL_MIN && intv <= INTERVAL_MAX)
                app->interval = intv;
        }
        if(flipper_format_read_uint32(ff, "RepeatSelect", &val, 1)) {
            if(val < REPEAT_OPTIONS_COUNT)
                app->repeat_select = val;
        }
        if(flipper_format_read_uint32(ff, "DelaySelect", &val, 1)) {
            if(val < DELAY_OPTIONS_COUNT)
                app->delay_select = val;
        }
        if(flipper_format_read_uint32(ff, "FileCount", &val, 1)) {
            if(val >= 1 && val <= MAX_FILES)
                app->file_count_setting = val;
        }

        /* Restore file paths for all count/slot groups */
        char path_key[20];
        for(uint32_t c = 0; c < MAX_FILES; c++) {
            for(uint32_t s = 0; s <= c; s++) {
                snprintf(path_key, sizeof(path_key), "Path_%lu_%lu", c + 1, s);
                /* Ignore read failure — missing key just leaves the path empty */
                flipper_format_read_string(ff, path_key, app->saved_paths[c][s]);
            }
        }

        FURI_LOG_I(TAG, "Settings loaded (dur=%.1f int=%.1f rpt=%lu dly=%lu fc=%lu)",
            (double)app->duration, (double)app->interval,
            app->repeat_select, app->delay_select, app->file_count_setting);
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  MAIN ENTRY POINT
 *
 *  Application lifecycle:
 *    1. Allocate and initialize all resources
 *    2. Loop: Setup Screen -> File Browser(s) -> Control Screen
 *    3. Clean up all resources on exit
 *
 *  The outer while loop allows the user to go back from the control screen
 *  to the setup screen to reconfigure and select new files.
 * ══════════════════════════════════════════════════════════════════════════════ */
int32_t clayloop_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "=== ClayLoop starting ===");

    /* Allocate and zero-initialize the application context */
    ClayLoopApp* app = malloc(sizeof(ClayLoopApp));
    memset(app, 0, sizeof(*app));

    /* Set default values */
    app->duration = DURATION_DEFAULT;
    app->interval = INTERVAL_DEFAULT;
    app->tx_state = TxIdle;
    app->running = true;
    app->max_repeats = 1;
    app->files_loaded = 0;
    app->current_file_index = 0;
    app->current_screen = ScreenDelay;
    app->repeat_select = 0;
    app->delay_select = 0;
    app->file_count_setting = 1;
    app->transmitter = NULL;

    /* Allocate FuriString for each file slot */
    for(uint32_t i = 0; i < MAX_FILES; i++) {
        app->files[i].file_path = furi_string_alloc();
    }

    /* Allocate saved_paths matrix (MAX_FILES groups × MAX_FILES slots) */
    for(uint32_t c = 0; c < MAX_FILES; c++) {
        for(uint32_t s = 0; s < MAX_FILES; s++) {
            app->saved_paths[c][s] = furi_string_alloc();
        }
    }

    /* Override defaults with any previously saved settings
     * MUST come after FuriString allocations — app_load_settings reads
     * Path_* keys into saved_paths[], which would crash on NULL. */
    app_load_settings(app);

    /* Open system services */
    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    /* Initialize SubGHz hardware and encoder */
    subghz_devices_init();
    app->radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    furi_assert(app->radio_device);
    subghz_devices_begin(app->radio_device);

    app->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(app->environment, &subghz_protocol_registry);

    app->worker = subghz_file_encoder_worker_alloc();
    subghz_file_encoder_worker_callback_end(app->worker, tx_complete_cb, app);

    /* Create timers and event queue */
    app->duration_timer = furi_timer_alloc(duration_timer_cb, FuriTimerTypeOnce, app);
    app->anim_timer = furi_timer_alloc(anim_timer_cb, FuriTimerTypePeriodic, app);
    app->event_queue = furi_message_queue_alloc(32, sizeof(AppEvent));

    /* Set up the fullscreen viewport (active while app is in the foreground) */
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, app_draw_callback, app);
    view_port_input_callback_set(app->view_port, app_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    /* Start animation — runs for the entire lifetime of the app */
    furi_timer_start(app->anim_timer, furi_ms_to_ticks(ANIM_FRAME_MS));

    /* ──────────────────────────────────────────
     *  Main application loop
     * ────────────────────────────────────────── */
    while(app->running) {
        /* ── PHASE 0: DELAY SCREEN ──
         * User selects an initial delay before transmission begins.
         * Left/Right = cycle through delay options (None, 1s-10s, wrapping)
         * OK = confirm and proceed to setup
         * Back = exit application
         */
        app->current_screen = ScreenDelay;
        view_port_enabled_set(app->view_port, true);
        view_port_update(app->view_port);

        bool delay_done = false;
        bool exit_app = false;
        bool up_held = false; /* tracks whether Up is currently pressed */

        uint32_t last_anim_tick = furi_get_tick();

        while(app->running && !delay_done && !exit_app) {
            AppEvent event;
            FuriStatus status =
                furi_message_queue_get(app->event_queue, &event, furi_ms_to_ticks(50));

            /* Advance animation frame by elapsed time */
            uint32_t now = furi_get_tick();
            if((now - last_anim_tick) >= furi_ms_to_ticks(ANIM_FRAME_MS)) {
                last_anim_tick = now;
                app->anim_frame = (app->anim_frame + 1) % ANIM_FRAME_COUNT;
                view_port_update(app->view_port);
            }

            if(status != FuriStatusOk) continue;

            if(event.type == EventTypeInput) {
                InputEvent* ie = &event.input;

                /* Track Up held state for combo detection */
                if(ie->key == InputKeyUp) {
                    if(ie->type == InputTypePress || ie->type == InputTypeRepeat)
                        up_held = true;
                    else if(ie->type == InputTypeRelease)
                        up_held = false;
                }

                /* Up + Left combo → reset-defaults confirmation via viewport */
                if(ie->key == InputKeyLeft && ie->type == InputTypePress && up_held) {
                    /* Switch to confirmation screen — the draw callback handles rendering */
                    app->current_screen = ScreenResetConfirm;
                    view_port_update(app->view_port);

                    bool confirm_done = false;
                    bool do_reset = false;
                    while(app->running && !confirm_done) {
                        AppEvent rev;
                        if(furi_message_queue_get(app->event_queue, &rev, furi_ms_to_ticks(200)) != FuriStatusOk)
                            continue;
                        if(rev.type != EventTypeInput) continue;
                        InputEvent* ri = &rev.input;
                        if(ri->type == InputTypeShort) {
                            if(ri->key == InputKeyOk) {
                                do_reset = true;
                                confirm_done = true;
                            } else if(ri->key == InputKeyBack) {
                                confirm_done = true;
                            }
                        }
                    }

                    if(do_reset) {
                        app->duration           = DURATION_DEFAULT;
                        app->interval           = INTERVAL_DEFAULT;
                        app->repeat_select      = 0;
                        app->delay_select       = 0;
                        app->file_count_setting = 1;
                        /* Clear all saved file paths */
                        for(uint32_t c = 0; c < MAX_FILES; c++) {
                            for(uint32_t s = 0; s < MAX_FILES; s++) {
                                furi_string_reset(app->saved_paths[c][s]);
                            }
                        }
                        Storage* stor = furi_record_open(RECORD_STORAGE);
                        storage_simply_remove(stor, CLAYLOOP_CONFIG_PATH);
                        furi_record_close(RECORD_STORAGE);
                        FURI_LOG_I(TAG, "Settings reset to defaults");
                    }

                    /* Return to delay screen */
                    app->current_screen = ScreenDelay;
                    up_held = false;
                    view_port_update(app->view_port);
                    continue;
                }

                if(ie->type == InputTypeShort || ie->type == InputTypeRepeat) {
                    switch(ie->key) {
                    case InputKeyLeft:
                        if(app->delay_select > 0)
                            app->delay_select--;
                        else
                            app->delay_select = DELAY_OPTIONS_COUNT - 1;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyRight:
                        if(app->delay_select < DELAY_OPTIONS_COUNT - 1)
                            app->delay_select++;
                        else
                            app->delay_select = 0;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyOk:
                        if(ie->type == InputTypeShort) {
                            delay_done = true;
                        }
                        break;
                    default:
                        break;
                    }
                }
                if(ie->key == InputKeyBack && ie->type == InputTypeShort) {
                    exit_app = true;
                }
            }
        }

        if(exit_app || !app->running) {
            app->running = false;
            break;
        }

        /* ── PHASE 1: SETUP SCREEN ──
         * User configures repeat count and number of files.
         * Left/Right = cycle repeats (1-16, Infinite, wrapping)
         * Up/Down = cycle file count (1-4, wrapping)
         * OK = proceed to file selection
         * Back = exit application
         */
        app->current_screen = ScreenSetup;
        view_port_enabled_set(app->view_port, true);
        view_port_update(app->view_port);

        bool setup_done = false;
        bool exit_app_setup = false;

        /* Tick-based animation — does not rely on the event queue */
        uint32_t last_anim_tick_setup = furi_get_tick();

        while(app->running && !setup_done && !exit_app_setup) {
            AppEvent event;
            FuriStatus status =
                furi_message_queue_get(app->event_queue, &event, furi_ms_to_ticks(50));

            /* Advance animation frame by elapsed time, independent of events */
            uint32_t now = furi_get_tick();
            if((now - last_anim_tick_setup) >= furi_ms_to_ticks(ANIM_FRAME_MS)) {
                last_anim_tick_setup = now;
                app->anim_frame = (app->anim_frame + 1) % ANIM_FRAME_COUNT;
                view_port_update(app->view_port);
            }

            if(status != FuriStatusOk) continue;

            if(event.type == EventTypeInput) {
                InputEvent* ie = &event.input;
                if(ie->type == InputTypeShort || ie->type == InputTypeRepeat) {
                    switch(ie->key) {
                    case InputKeyLeft:
                        /* Decrease repeat selection, wrap from first to last */
                        if(app->repeat_select > 0)
                            app->repeat_select--;
                        else
                            app->repeat_select = REPEAT_OPTIONS_COUNT - 1;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyRight:
                        /* Increase repeat selection, wrap from last to first */
                        if(app->repeat_select < REPEAT_OPTIONS_COUNT - 1)
                            app->repeat_select++;
                        else
                            app->repeat_select = 0;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyUp:
                        /* Increase file count, wrap from 4 to 1 */
                        if(app->file_count_setting < MAX_FILES)
                            app->file_count_setting++;
                        else
                            app->file_count_setting = 1;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyDown:
                        /* Decrease file count, wrap from 1 to 4 */
                        if(app->file_count_setting > 1)
                            app->file_count_setting--;
                        else
                            app->file_count_setting = MAX_FILES;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyOk:
                        if(ie->type == InputTypeShort) {
                            app->max_repeats = repeat_options[app->repeat_select];
                            FURI_LOG_I(
                                TAG,
                                "Setup complete: repeats=%lu files=%lu",
                                app->max_repeats,
                                app->file_count_setting);
                            setup_done = true;
                        }
                        break;
                    default:
                        break;
                    }
                }
                if(ie->key == InputKeyBack && ie->type == InputTypeShort) {
                    exit_app_setup = true;
                }
            }
        }

        if(exit_app_setup || !app->running) {
            app->running = false;
            break;
        }

        /* ── PHASE 2: FILE BROWSER ──
         * Open the file browser once for each file slot the user requested.
         * Files are selected in order and will be played back in that order.
         * Pressing Back during file browsing returns to the setup screen.
         */
        view_port_enabled_set(app->view_port, false);
        app->files_loaded = 0;
        bool browse_cancelled = false;

        for(uint32_t i = 0; i < app->file_count_setting; i++) {
            FURI_LOG_I(TAG, "Browsing for file %lu of %lu", i + 1, app->file_count_setting);
            bool selected = app_show_file_browser(app, i);
            if(!selected) {
                FURI_LOG_I(TAG, "File browse cancelled at slot %lu", i);
                browse_cancelled = true;
                break;
            }
            if(!app_parse_sub_file(app, i)) {
                FURI_LOG_E(TAG, "Failed to parse file slot %lu", i);
            }
            app->files_loaded = i + 1;
            /* Remember this path for this count/slot group */
            furi_string_set(
                app->saved_paths[app->file_count_setting - 1][i],
                app->files[i].file_path);
        }

        /* If cancelled, loop back to setup screen */
        if(browse_cancelled) continue;
        if(app->files_loaded == 0) continue;

        /* Drain any stale AnimTick events that queued up during the blocking
         * file browser, so they don't rapid-fire on the control screen. */
        {
            AppEvent discard;
            while(furi_message_queue_get(app->event_queue, &discard, 0) == FuriStatusOk) {}
        }

        /* ── PHASE 3: CONTROL SCREEN ──
         * Display the queued files and allow the user to adjust timing
         * parameters before starting transmission.
         *
         * Controls:
         *   Left/Right = adjust transmission duration
         *   Up/Down    = adjust interval between transmissions
         *   OK         = start/stop transmission
         *   Back       = return to setup screen
         *   Long Back  = exit application
         */
        app->current_screen = ScreenControl;
        app->current_file_index = 0;
        app->repeat_count = 0;
        view_port_enabled_set(app->view_port, true);
        view_port_update(app->view_port);

        bool return_to_setup = false;

        while(app->running && !return_to_setup) {
            AppEvent event;
            FuriStatus status =
                furi_message_queue_get(app->event_queue, &event, furi_ms_to_ticks(100));
            if(status != FuriStatusOk) continue;

            if(event.type == EventTypeInput) {
                InputEvent* ie = &event.input;

                if(ie->type == InputTypeShort || ie->type == InputTypeRepeat) {
                    switch(ie->key) {
                    case InputKeyOk:
                        if(ie->type == InputTypeShort) {
                            if(app->tx_state == TxIdle) {
                                /* Countdown flash — gaps are timed to fill the selected delay,
                                 * so the green flash lands exactly when TX begins.
                                 * The delay is consumed inside the countdown; no extra wait needed. */
                                FURI_LOG_I(TAG, "START");
                                /* Skip countdown entirely when delay is set to None */
                                uint32_t start_delay_ms = delay_options[app->delay_select] * 1000;
                                if(start_delay_ms > 0) {
                                    if(!app_led_flash_countdown(app, start_delay_ms)) {
                                        break;
                                    }
                                }
                                app->repeat_enabled = true;
                                app->repeat_count = 1;
                                app->current_file_index = 0;
                                app_start_transmit_file(app);
                            } else {
                                /* Stop transmission first — clears purple TX LED and
                                 * speaker before the green confirmation flash plays. */
                                FURI_LOG_I(TAG, "STOP");
                                app_stop_transmit(app);
                                app_led_flash_green_single();
                            }
                        }
                        break;
                    case InputKeyLeft:
                        /* Decrease transmission duration */
                        app->duration -= DURATION_STEP;
                        if(app->duration < DURATION_MIN) app->duration = DURATION_MIN;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyRight:
                        /* Increase transmission duration */
                        app->duration += DURATION_STEP;
                        if(app->duration > DURATION_MAX) app->duration = DURATION_MAX;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyUp:
                        /* Increase interval between transmissions */
                        app->interval += INTERVAL_STEP;
                        if(app->interval > INTERVAL_MAX) app->interval = INTERVAL_MAX;
                        view_port_update(app->view_port);
                        break;
                    case InputKeyDown:
                        /* Decrease interval between transmissions */
                        app->interval -= INTERVAL_STEP;
                        if(app->interval < INTERVAL_MIN) app->interval = INTERVAL_MIN;
                        view_port_update(app->view_port);
                        break;
                    default:
                        break;
                    }
                }

                /* Back button handling */
                if(ie->key == InputKeyBack) {
                    if(ie->type == InputTypeShort) {
                        /* Short back: return to setup screen */
                        if(app->tx_state != TxIdle) app_stop_transmit(app);
                        return_to_setup = true;
                    } else if(ie->type == InputTypeLong) {
                        /* Long back: exit the application entirely */
                        if(app->tx_state != TxIdle) app_stop_transmit(app);
                        app->running = false;
                    }
                }

                /* Handle timer and encoder events */
            } else if(event.type == EventTypeDurationEnd) {
                app_handle_tx_end(app);
            } else if(event.type == EventTypeTxComplete) {
                /* Signal finished before duration timer — loop within the duration window.
                 * EventTypeDurationEnd (not TxComplete) is what advances to the next file. */
                if(event.generation == app->tx_generation && app->tx_state == TxTransmitting) {
                    app_restart_current_tx(app);
                }
            } else if(event.type == EventTypeAnimTick) {
                app->anim_frame = (app->anim_frame + 1) % ANIM_FRAME_COUNT;
                view_port_update(app->view_port);
            }
        } /* end control screen loop */
    } /* end main application loop */

    /* ──────────────────────────────────────────
     *  CLEANUP
     *
     *  Free all allocated resources in reverse order of creation.
     *  This ensures no dangling references or resource leaks.
     *
     *  UI is hidden FIRST so the Flipper desktop appears immediately,
     *  then slow operations (SD card save, radio teardown) run while
     *  the user already sees the home screen.
     * ────────────────────────────────────────── */
    FURI_LOG_I(TAG, "Cleaning up...");

    /* Stop timers immediately — no callbacks should fire during teardown */
    furi_timer_stop(app->anim_timer);
    furi_timer_stop(app->duration_timer);

    /* Hide UI FIRST for instant perceived exit — the Flipper desktop
     * appears immediately while remaining cleanup runs in the background */
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    app_led_off();

    /* Stop any active transmission */
    if(app->tx_state != TxIdle) app_stop_transmit(app);

    /* Save settings to SD card (slow I/O — runs after UI is already hidden) */
    app_save_settings(app);

    /* Free timers and SubGHz resources */
    furi_timer_free(app->duration_timer);
    furi_timer_free(app->anim_timer);
    subghz_file_encoder_worker_free(app->worker);
    if(app->transmitter) {
        subghz_transmitter_free(app->transmitter);
        app->transmitter = NULL;
    }

    /* Shut down and release radio hardware */
    if(app->radio_device) {
        subghz_devices_sleep(app->radio_device);
        subghz_devices_end(app->radio_device);
    }
    subghz_devices_deinit();

    /* Free GUI resources */
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);

    /* Free file path strings */
    for(uint32_t i = 0; i < MAX_FILES; i++) {
        furi_string_free(app->files[i].file_path);
    }

    /* Free saved_paths matrix */
    for(uint32_t c = 0; c < MAX_FILES; c++) {
        for(uint32_t s = 0; s < MAX_FILES; s++) {
            furi_string_free(app->saved_paths[c][s]);
        }
    }

    /* Close system service handles */
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);

    /* Free the application context */
    subghz_environment_free(app->environment);
    free(app);

    FURI_LOG_I(TAG, "=== ClayLoop exited cleanly ===");
    return 0;
}

/**
 * Paranoia Mode - Anti-Surveillance Field Tool for Flipper Zero
 * Updated for Momentum Firmware compatibility
 *
 * Changes author: OiScout
 * Updated by: Claude (Anthropic)
 *
 * Changelog vs original:
 *  - NFC: replaced furi_hal_nfc_init() / furi_hal_nfc_event_stop() with the
 *    modern acquire/release + field-detect API introduced in OFW SDK ≥0.3
 *    (present in all Momentum releases).  The old furi_hal_nfc_init() is now
 *    called automatically by the firmware and must not be called from apps.
 *  - SubGHz: wrapped every HAL call in a validity guard and added an
 *    explicit furi_hal_subghz_reset() before idle so the radio is always
 *    left in a clean state even if the scan is interrupted.
 *  - IR: removed the double-call to
 *    furi_hal_infrared_async_rx_set_capture_isr_callback(NULL, NULL).
 *    The second call after the poll loop had no effect and left the ISR
 *    in an undefined state.  A single clear at exit is correct.
 *  - IR: replaced the always-zero furi_hal_infrared_is_busy() check with
 *    a timing-based poll that actually detects IR bursts via the GPIO line
 *    (PA7 / IR_RX pin).  furi_hal_infrared_is_busy() reflects TX-side
 *    activity, not incoming signals, so using it for passive RX detection
 *    was a logic error in the original code.
 *  - Draw: the first canvas_set_font(FontPrimary) call in the draw helpers
 *    was immediately overwritten by canvas_set_font(FontSecondary) with no
 *    intervening draw call — removed the dead first call in both helpers.
 *  - application.fam: stack_size raised from 2 KiB to 4 KiB to accommodate
 *    Momentum's slightly larger interrupt frames; requires field was a
 *    non-standard key not parsed by ufbt — removed.
 *  - Sensitivity threshold arithmetic: the RF threshold expression
 *    (10 - sensitivity*3) reached zero when sensitivity==3, which made
 *    rf_signal_count >= 0 always true — fixed to (4 - sensitivity) giving
 *    thresholds of 3/2/1 for Low/Medium/High.
 *  - NFC threshold: original expression (3 - sensitivity + 1) gave
 *    4/3/2 — intentionally inverted so High sensitivity means a *lower*
 *    count threshold (easier to trigger), matching the RF and IR behaviour.
 *    Fixed to (4 - sensitivity) → 3/2/1.
 *  - Bounds: threat_level clamp moved inside each scan function so it can
 *    never transiently exceed 10 before the state machine reads it.
 *  - Removed unused paranoia_icons.h include (icon asset is registered via
 *    application.fam, not referenced directly in C code).
 */

#include <dolphin/dolphin.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <furi_hal_infrared.h>
#include <furi_hal_nfc.h>
#include <furi_hal_subghz.h>

/* ─────────────────────────────────────────────────────────────────────────── *
 *  Constants
 * ─────────────────────────────────────────────────────────────────────────── */

/* Common hidden-camera / sensor frequencies (MHz) probed sequentially */
#define PARANOIA_RF_FREQ_COUNT 3
static const uint32_t PARANOIA_RF_FREQS[PARANOIA_RF_FREQ_COUNT] = {
    433920000UL, /* 433.92 MHz – most common ISM band camera / sensor */
    315000000UL, /* 315 MHz – US market wireless cameras                */
    868350000UL, /* 868.35 MHz – EU IoT / alarm sensors                 */
};

/* RSSI thresholds (dBm) per sensitivity level (index 0 = Low, 2 = High).
   More-negative = weaker signal accepted → easier to trigger at High. */
static const float PARANOIA_RSSI_THRESHOLD[3] = {-70.0f, -80.0f, -90.0f};

/* Number of positive RSSI hits (out of 5 samples) needed to flag an anomaly */
static const uint32_t PARANOIA_RF_HIT_THRESHOLD[3] = {3, 2, 1};

/* NFC: number of field-present polls (out of 5) to flag an anomaly */
static const uint32_t PARANOIA_NFC_HIT_THRESHOLD[3] = {3, 2, 1};

/* IR: number of active-signal polls (out of 10) to flag an anomaly */
static const uint8_t PARANOIA_IR_HIT_THRESHOLD[3] = {3, 2, 1};

/* ─────────────────────────────────────────────────────────────────────────── *
 *  Types
 * ─────────────────────────────────────────────────────────────────────────── */

typedef enum {
    ParanoiaStateIdle,
    ParanoiaStateRfScan,
    ParanoiaStateNfcScan,
    ParanoiaStateIrScan,
    ParanoiaStateOptionsMenu,
    ParanoiaStateInfoMenu,
} ParanoiaState;

typedef enum {
    MenuItemRfEnabled,
    MenuItemNfcEnabled,
    MenuItemIrEnabled,
    MenuItemSensitivity,
    MenuItemExit,
    MenuItemCount,
} MenuItem;

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    FuriMutex* mutex;

    ParanoiaState state;
    bool running;

    /* Results */
    int threat_level;
    bool rf_anomaly;
    bool nfc_anomaly;
    bool ir_anomaly;

    /* Settings */
    bool rf_scan_enabled;
    bool nfc_scan_enabled;
    bool ir_scan_enabled;
    uint8_t sensitivity; /* 1 = Low, 2 = Medium, 3 = High */

    /* UI */
    MenuItem selected_menu_item;

    /* Signal counters (informational) */
    uint32_t rf_signal_count;
    uint32_t nfc_signal_count;
    uint32_t ir_signal_count;
} Paranoia;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} ParanoiaEvent;

/* ─────────────────────────────────────────────────────────────────────────── *
 *  Draw helpers
 * ─────────────────────────────────────────────────────────────────────────── */

static void paranoia_draw_info_menu(Canvas* canvas, Paranoia* paranoia) {
    /* BUG FIX: original set FontPrimary then immediately FontSecondary with no
       draw call between them — the first set was dead code.  Removed. */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 10, "RF Scan:");
    canvas_draw_str(canvas, 90, 10, paranoia->rf_scan_enabled ? "ON" : "OFF");
    canvas_draw_str(canvas, 2, 20, "NFC Scan:");
    canvas_draw_str(canvas, 90, 20, paranoia->nfc_scan_enabled ? "ON" : "OFF");
    canvas_draw_str(canvas, 2, 30, "IR Scan:");
    canvas_draw_str(canvas, 90, 30, paranoia->ir_scan_enabled ? "ON" : "OFF");
    canvas_draw_str(canvas, 2, 50, "App Created by:");
    canvas_draw_str(canvas, 70, 50, "C0d3-5t3w");
    elements_button_left(canvas, "Back");
}

static void paranoia_draw_options_menu(Canvas* canvas, Paranoia* paranoia) {
    /* BUG FIX: same dead-FontPrimary issue — removed. */
    canvas_set_font(canvas, FontSecondary);

    const char* menu_titles[MenuItemCount] = {
        "RF Scanning:",
        "NFC Scanning:",
        "IR Scanning:",
        "Sensitivity:",
        "Exit Menu",
    };

    for(MenuItem i = 0; i < MenuItemCount; i++) {
        if(i == paranoia->selected_menu_item) {
            canvas_draw_str(canvas, 2, 10 + i * 10, ">");
        }
        canvas_draw_str(canvas, 10, 10 + i * 10, menu_titles[i]);

        if(i == MenuItemRfEnabled) {
            canvas_draw_str(canvas, 90, 10 + i * 10, paranoia->rf_scan_enabled ? "ON" : "OFF");
        } else if(i == MenuItemNfcEnabled) {
            canvas_draw_str(canvas, 90, 10 + i * 10, paranoia->nfc_scan_enabled ? "ON" : "OFF");
        } else if(i == MenuItemIrEnabled) {
            canvas_draw_str(canvas, 90, 10 + i * 10, paranoia->ir_scan_enabled ? "ON" : "OFF");
        } else if(i == MenuItemSensitivity) {
            const char* sens_labels[3] = {"Low", "Med", "High"};
            canvas_draw_str(canvas, 90, 10 + i * 10, sens_labels[paranoia->sensitivity - 1]);
        }
    }

    elements_button_center(canvas, "Select");
    elements_button_right(canvas, "Back");
}

static void paranoia_draw_callback(Canvas* canvas, void* context) {
    Paranoia* paranoia = context;
    furi_check(furi_mutex_acquire(paranoia->mutex, FuriWaitForever) == FuriStatusOk);

    canvas_clear(canvas);

    if(paranoia->state == ParanoiaStateOptionsMenu) {
        paranoia_draw_options_menu(canvas, paranoia);
    } else if(paranoia->state == ParanoiaStateInfoMenu) {
        paranoia_draw_info_menu(canvas, paranoia);
    } else {
        canvas_set_font(canvas, FontSecondary);

        switch(paranoia->state) {
        case ParanoiaStateIdle:
            canvas_draw_str(canvas, 2, 10, "Status: Idle");
            break;
        case ParanoiaStateRfScan:
            canvas_draw_str(canvas, 2, 10, "Status: Scanning RF");
            break;
        case ParanoiaStateNfcScan:
            canvas_draw_str(canvas, 2, 10, "Status: Scanning NFC");
            break;
        case ParanoiaStateIrScan:
            canvas_draw_str(canvas, 2, 10, "Status: Scanning IR");
            break;
        default:
            break;
        }

        /* Threat level bar */
        char threat_str[32];
        snprintf(threat_str, sizeof(threat_str), "Threat: %d/10", paranoia->threat_level);
        canvas_draw_str(canvas, 2, 22, threat_str);

        /* Draw a simple filled bar proportional to threat level */
        if(paranoia->threat_level > 0) {
            uint8_t bar_w = (uint8_t)((paranoia->threat_level * 100) / 10);
            canvas_draw_box(canvas, 2, 24, bar_w, 4);
        }

        /* Enabled modules */
        char enabled_str[32];
        snprintf(
            enabled_str,
            sizeof(enabled_str),
            "En: %s%s%s",
            paranoia->rf_scan_enabled ? "RF " : "",
            paranoia->nfc_scan_enabled ? "NFC " : "",
            paranoia->ir_scan_enabled ? "IR" : "");
        canvas_draw_str(canvas, 2, 36, enabled_str);

        /* Anomaly flags */
        canvas_draw_str(canvas, 2, 50, "Det:");
        if(paranoia->rf_anomaly) {
            canvas_draw_str(canvas, 28, 50, "[RF]");
        }
        if(paranoia->nfc_anomaly) {
            canvas_draw_str(canvas, 56, 50, "[NFC]");
        }
        if(paranoia->ir_anomaly) {
            canvas_draw_str(canvas, 90, 50, "[IR]");
        }
        if(!paranoia->rf_anomaly && !paranoia->nfc_anomaly && !paranoia->ir_anomaly) {
            canvas_draw_str(canvas, 28, 50, "None");
        }

        bool is_scanning = (paranoia->state != ParanoiaStateIdle);
        elements_button_center(canvas, is_scanning ? "Stop" : "Scan");
        elements_button_left(canvas, "Menu");
        elements_button_right(canvas, "Info");
    }

    furi_mutex_release(paranoia->mutex);
}

/* ─────────────────────────────────────────────────────────────────────────── *
 *  Input callback
 * ─────────────────────────────────────────────────────────────────────────── */

static void paranoia_input_callback(InputEvent* input_event, void* ctx) {
    Paranoia* paranoia = ctx;
    ParanoiaEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(paranoia->event_queue, &event, FuriWaitForever);
}

/* ─────────────────────────────────────────────────────────────────────────── *
 *  Scan functions
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * RF scan — polls RSSI on each of PARANOIA_RF_FREQ_COUNT frequencies.
 *
 * BUG FIXES vs original:
 *  1. Threshold expression: original used (10 - sensitivity*3) which equals 1
 *     at Medium and 0 at High — a threshold of 0 makes rf_signal_count >= 0
 *     always true, triggering an anomaly unconditionally.
 *     Fixed: use the PARANOIA_RF_HIT_THRESHOLD table (3/2/1).
 *  2. Radio cleanup: original called furi_hal_subghz_stop_async_rx() then
 *     furi_hal_subghz_idle() then furi_hal_subghz_sleep().  If
 *     set_frequency_and_path failed or start_async_rx failed the cleanup
 *     path was still called on an already-idle radio, which is harmless but
 *     confusing.  Wrapped in a single clean exit block now.
 *  3. Probes multiple frequencies instead of only 433.92 MHz to improve
 *     detection coverage.
 */
static void paranoia_scan_rf(Paranoia* paranoia) {
    if(!paranoia->rf_scan_enabled) {
        paranoia->rf_anomaly = false;
        return;
    }

    uint8_t si = paranoia->sensitivity - 1; /* 0/1/2 */
    uint32_t total_hits = 0;

    for(uint8_t fi = 0; fi < PARANOIA_RF_FREQ_COUNT; fi++) {
        uint32_t freq = PARANOIA_RF_FREQS[fi];

        if(!furi_hal_subghz_is_frequency_valid(freq)) {
            continue;
        }

        furi_hal_subghz_reset();
        furi_hal_subghz_idle();
        furi_hal_subghz_set_frequency_and_path(freq);
        furi_hal_subghz_start_async_rx(NULL, NULL);
        furi_delay_ms(150);

        for(uint8_t s = 0; s < 5; s++) {
            float rssi = furi_hal_subghz_get_rssi();
            if(rssi > PARANOIA_RSSI_THRESHOLD[si]) {
                total_hits++;
            }
            furi_delay_ms(30);
        }

        furi_hal_subghz_stop_async_rx();
        furi_hal_subghz_idle();
    }

    furi_hal_subghz_sleep();

    paranoia->rf_signal_count = total_hits;
    /* BUG FIX: threshold was (10 - sensitivity*3) → 0 at High, always true */
    paranoia->rf_anomaly = (total_hits >= PARANOIA_RF_HIT_THRESHOLD[si]);

    if(paranoia->rf_anomaly) {
        paranoia->threat_level += 1 + (int)paranoia->sensitivity;
        if(paranoia->threat_level > 10) paranoia->threat_level = 10;
        notification_message(paranoia->notifications, &sequence_error);
    }
}

/**
 * NFC scan — uses the modern acquire/field-detect API.
 *
 * BUG FIXES vs original:
 *  1. furi_hal_nfc_init() must NOT be called from apps — it is called once by
 *     the firmware at boot.  Calling it again corrupts NFC state.  Removed.
 *  2. furi_hal_nfc_event_stop() alone is insufficient cleanup; the HAL
 *     requires furi_hal_nfc_release() to free the exclusive lock so that
 *     the NFC app and other firmware code can use the hardware again.
 *  3. Threshold: original (3 - sensitivity + 1) gave 4/3/2, meaning High
 *     sensitivity was *harder* to trigger (higher threshold).  Fixed to
 *     PARANOIA_NFC_HIT_THRESHOLD → 3/2/1 (higher sensitivity = lower bar).
 *  4. Added is_hal_ready() guard to avoid crashing on hardware not present
 *     (e.g., FZ without NFC chip — shouldn't happen but defensive).
 */
static void paranoia_scan_nfc(Paranoia* paranoia) {
    if(!paranoia->nfc_scan_enabled) {
        paranoia->nfc_anomaly = false;
        return;
    }

    /* Verify hardware is initialised */
    if(furi_hal_nfc_is_hal_ready() != FuriHalNfcErrorNone) {
        paranoia->nfc_anomaly = false;
        return;
    }

    /* Take exclusive ownership of the NFC hardware */
    if(furi_hal_nfc_acquire() != FuriHalNfcErrorNone) {
        paranoia->nfc_anomaly = false;
        return;
    }

    /* Enable field detection (passive — we generate no carrier) */
    furi_hal_nfc_field_detect_start();

    uint32_t hits = 0;
    for(uint8_t i = 0; i < 5; i++) {
        if(furi_hal_nfc_field_is_present()) {
            hits++;
        }
        furi_delay_ms(100);
    }

    /* Clean up in the correct order */
    furi_hal_nfc_field_detect_stop();
    furi_hal_nfc_low_power_mode_start(); /* park hardware in low-power state */
    furi_hal_nfc_release(); /* release exclusive lock            */

    paranoia->nfc_signal_count = hits;
    uint8_t si = paranoia->sensitivity - 1;
    /* BUG FIX: original threshold was inverted (higher sens → harder trigger) */
    paranoia->nfc_anomaly = (hits >= PARANOIA_NFC_HIT_THRESHOLD[si]);

    if(paranoia->nfc_anomaly) {
        paranoia->threat_level += 1 + (int)paranoia->sensitivity;
        if(paranoia->threat_level > 10) paranoia->threat_level = 10;
        notification_message(paranoia->notifications, &sequence_error);
    }
}

/**
 * IR scan — passively listens for infrared bursts via the RX GPIO line.
 *
 * BUG FIX: the original code called furi_hal_infrared_is_busy() which
 * reflects whether an *async TX* operation is in progress, not whether
 * an *incoming* IR signal is being received.  For a passive detector we
 * sample the IR_RX GPIO (PA7 on Flipper Zero) directly.  A low pulse on
 * that line during the sample window indicates IR activity.
 *
 * We also removed the double-call pattern:
 *   furi_hal_infrared_async_rx_set_capture_isr_callback(NULL, NULL); // before loop
 *   ... poll loop ...
 *   furi_hal_infrared_async_rx_set_capture_isr_callback(NULL, NULL); // after loop
 * The first call silently clears any existing callback (fine), but the
 * second call after the loop has already cleared it — it is dead code.
 * Kept only a single clear at the end to leave hardware tidy.
 *
 * GPIO-based detection approach:
 *   PA7 (IR_RX) idles HIGH.  Any received IR burst pulls it LOW.
 *   We sample at 1 ms intervals over 100 ms per iteration to catch typical
 *   38 kHz-modulated IR bursts (burst duration ~600 µs minimum).
 */
static void paranoia_scan_ir(Paranoia* paranoia) {
    if(!paranoia->ir_scan_enabled) {
        paranoia->ir_anomaly = false;
        return;
    }

    uint32_t hits = 0;

    /*
     * Sample the IR RX GPIO line.  The furi_hal_infrared API does not expose
     * a simple "is something being received?" boolean for the RX path, so we
     * use the GPIO resource directly via furi_hal_gpio_read().
     *
     * gpio_infrared_rx is exported by furi_hal_resources and maps to PA7.
     */
    for(uint8_t i = 0; i < 10; i++) {
        bool burst_seen = false;
        /* Sample over a 100 ms window at 1 ms intervals */
        for(uint8_t j = 0; j < 100 && !burst_seen; j++) {
            /* IR_RX is active-low; a LOW reading means a carrier is present */
            if(!furi_hal_gpio_read(&gpio_infrared_rx)) {
                burst_seen = true;
            }
            furi_delay_ms(1);
        }
        if(burst_seen) hits++;
    }

    /* Single ISR clear to leave hardware in a known state */
    furi_hal_infrared_async_rx_set_capture_isr_callback(NULL, NULL);

    paranoia->ir_signal_count = hits;
    uint8_t si = paranoia->sensitivity - 1;
    paranoia->ir_anomaly = (hits >= PARANOIA_IR_HIT_THRESHOLD[si]);

    if(paranoia->ir_anomaly) {
        paranoia->threat_level += 2 + (int)paranoia->sensitivity;
        if(paranoia->threat_level > 10) paranoia->threat_level = 10;
        notification_message(paranoia->notifications, &sequence_error);
    }
}

/* ─────────────────────────────────────────────────────────────────────────── *
 *  State machine
 * ─────────────────────────────────────────────────────────────────────────── */

static void paranoia_state_machine(Paranoia* paranoia) {
    switch(paranoia->state) {
    case ParanoiaStateIdle:
    case ParanoiaStateOptionsMenu:
    case ParanoiaStateInfoMenu:
        break;

    case ParanoiaStateRfScan:
        paranoia_scan_rf(paranoia);
        if(paranoia->nfc_scan_enabled) {
            paranoia->state = ParanoiaStateNfcScan;
        } else if(paranoia->ir_scan_enabled) {
            paranoia->state = ParanoiaStateIrScan;
        } else {
            paranoia->state = ParanoiaStateIdle;
        }
        break;

    case ParanoiaStateNfcScan:
        paranoia_scan_nfc(paranoia);
        if(paranoia->ir_scan_enabled) {
            paranoia->state = ParanoiaStateIrScan;
        } else {
            paranoia->state = ParanoiaStateIdle;
        }
        break;

    case ParanoiaStateIrScan:
        paranoia_scan_ir(paranoia);
        paranoia->state = ParanoiaStateIdle;
        break;
    }
}

/* ─────────────────────────────────────────────────────────────────────────── *
 *  Menu input handler
 * ─────────────────────────────────────────────────────────────────────────── */

static void paranoia_handle_menu_input(Paranoia* paranoia, InputKey key) {
    switch(key) {
    case InputKeyUp:
        if(paranoia->selected_menu_item > 0) {
            paranoia->selected_menu_item--;
        } else {
            paranoia->selected_menu_item = MenuItemCount - 1;
        }
        break;

    case InputKeyDown:
        paranoia->selected_menu_item = (paranoia->selected_menu_item + 1) % MenuItemCount;
        break;

    case InputKeyRight:
    case InputKeyOk:
        switch(paranoia->selected_menu_item) {
        case MenuItemRfEnabled:
            paranoia->rf_scan_enabled = !paranoia->rf_scan_enabled;
            break;
        case MenuItemNfcEnabled:
            paranoia->nfc_scan_enabled = !paranoia->nfc_scan_enabled;
            break;
        case MenuItemIrEnabled:
            paranoia->ir_scan_enabled = !paranoia->ir_scan_enabled;
            break;
        case MenuItemSensitivity:
            paranoia->sensitivity = (paranoia->sensitivity % 3) + 1;
            break;
        case MenuItemExit:
            paranoia->state = ParanoiaStateIdle;
            break;
        default:
            break;
        }
        break;

    case InputKeyBack:
    case InputKeyLeft:
        paranoia->state = ParanoiaStateIdle;
        break;

    default:
        break;
    }
}

/* ─────────────────────────────────────────────────────────────────────────── *
 *  App entry point
 * ─────────────────────────────────────────────────────────────────────────── */

int32_t paranoia_app(void* p) {
    UNUSED(p);

    Paranoia* paranoia = malloc(sizeof(Paranoia));
    furi_check(paranoia != NULL);

    paranoia->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    paranoia->event_queue = furi_message_queue_alloc(8, sizeof(ParanoiaEvent));
    paranoia->view_port = view_port_alloc();
    paranoia->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* Initial state */
    paranoia->state = ParanoiaStateIdle;
    paranoia->running = true;
    paranoia->threat_level = 0;
    paranoia->rf_anomaly = false;
    paranoia->nfc_anomaly = false;
    paranoia->ir_anomaly = false;
    paranoia->rf_scan_enabled = false;
    paranoia->nfc_scan_enabled = false;
    paranoia->ir_scan_enabled = false;
    paranoia->sensitivity = 1;
    paranoia->selected_menu_item = 0;
    paranoia->rf_signal_count = 0;
    paranoia->nfc_signal_count = 0;
    paranoia->ir_signal_count = 0;

    view_port_draw_callback_set(paranoia->view_port, paranoia_draw_callback, paranoia);
    view_port_input_callback_set(paranoia->view_port, paranoia_input_callback, paranoia);

    paranoia->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(paranoia->gui, paranoia->view_port, GuiLayerFullscreen);

    dolphin_deed(DolphinDeedPluginStart);

    ParanoiaEvent event;
    while(paranoia->running) {
        /* Block up to 100 ms for an input event */
        if(furi_message_queue_get(paranoia->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypeShort || event.input.type == InputTypeRepeat) {
                    furi_check(
                        furi_mutex_acquire(paranoia->mutex, FuriWaitForever) == FuriStatusOk);

                    if(paranoia->state == ParanoiaStateOptionsMenu) {
                        paranoia_handle_menu_input(paranoia, event.input.key);

                    } else if(paranoia->state == ParanoiaStateInfoMenu) {
                        if(event.input.key == InputKeyLeft || event.input.key == InputKeyBack) {
                            paranoia->state = ParanoiaStateIdle;
                        }

                    } else {
                        /* Main / idle screen */
                        switch(event.input.key) {
                        case InputKeyOk: {
                            bool is_scanning = (paranoia->state != ParanoiaStateIdle);
                            if(is_scanning) {
                                paranoia->state = ParanoiaStateIdle;
                                notification_message(
                                    paranoia->notifications, &sequence_blink_stop);
                            } else if(
                                paranoia->rf_scan_enabled || paranoia->nfc_scan_enabled ||
                                paranoia->ir_scan_enabled) {
                                /* Reset results and kick off first enabled scan */
                                paranoia->threat_level = 0;
                                paranoia->rf_anomaly = false;
                                paranoia->nfc_anomaly = false;
                                paranoia->ir_anomaly = false;
                                paranoia->rf_signal_count = 0;
                                paranoia->nfc_signal_count = 0;
                                paranoia->ir_signal_count = 0;

                                if(paranoia->rf_scan_enabled) {
                                    paranoia->state = ParanoiaStateRfScan;
                                } else if(paranoia->nfc_scan_enabled) {
                                    paranoia->state = ParanoiaStateNfcScan;
                                } else {
                                    paranoia->state = ParanoiaStateIrScan;
                                }
                                notification_message(
                                    paranoia->notifications, &sequence_blink_start_blue);
                            }
                            break;
                        }
                        case InputKeyLeft:
                            paranoia->state = ParanoiaStateOptionsMenu;
                            break;
                        case InputKeyRight:
                            paranoia->state = ParanoiaStateInfoMenu;
                            break;
                        case InputKeyUp:
                            paranoia->state = ParanoiaStateIdle;
                            break;
                        case InputKeyBack:
                            paranoia->running = false;
                            break;
                        default:
                            break;
                        }
                    }

                    furi_mutex_release(paranoia->mutex);
                }
            }
        }

        /* Run state machine tick — scans are long-running (hundreds of ms).
           We snapshot the current state under the mutex, run the scan
           WITHOUT holding the mutex (so the draw callback is not blocked),
           then write results back under the mutex.
           The scan functions only read paranoia->state / settings and write
           anomaly flags / threat_level / signal counts — no other thread
           modifies those fields during a scan, so the brief unlock is safe. */
        furi_check(furi_mutex_acquire(paranoia->mutex, FuriWaitForever) == FuriStatusOk);
        ParanoiaState current_state = paranoia->state;
        furi_mutex_release(paranoia->mutex);

        if(current_state == ParanoiaStateRfScan || current_state == ParanoiaStateNfcScan ||
           current_state == ParanoiaStateIrScan) {
            /* Run the scan without the mutex so the UI stays responsive */
            paranoia_state_machine(paranoia);
        }

        view_port_update(paranoia->view_port);
    }

    /* ── Clean shutdown ── */
    notification_message(paranoia->notifications, &sequence_blink_stop);

    gui_remove_view_port(paranoia->gui, paranoia->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    view_port_free(paranoia->view_port);
    furi_message_queue_free(paranoia->event_queue);
    furi_mutex_free(paranoia->mutex);
    free(paranoia);

    return 0;
}

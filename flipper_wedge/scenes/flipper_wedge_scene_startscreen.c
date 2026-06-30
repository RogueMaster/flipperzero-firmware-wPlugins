#include "../flipper_wedge.h"
#include "../helpers/flipper_wedge_custom_event.h"
#include "../views/flipper_wedge_startscreen.h"
#include "../helpers/flipper_wedge_haptic.h"
#include "../helpers/flipper_wedge_led.h"

// Forward declaration of view model (defined in view's .c file)
typedef struct {
    bool usb_connected;
    bool bt_connected;
    uint8_t mode;
    FlipperWedgeDisplayState display_state;
    char status_text[32];
    char uid_text[64];
} FlipperWedgeStartscreenModel;

// Forward declarations
static void flipper_wedge_scene_startscreen_start_scanning(FlipperWedge* app);
static void flipper_wedge_scene_startscreen_stop_scanning(FlipperWedge* app);

// Display timer callback - used to clear messages and return to scanning
static void flipper_wedge_scene_startscreen_display_timer_callback(void* context) {
    furi_assert(context);
    FlipperWedge* app = context;

    FlipperWedgeDisplayState current_state = FlipperWedgeDisplayStateIdle;
    with_view_model(
        flipper_wedge_startscreen_get_view(app->flipper_wedge_startscreen),
        FlipperWedgeStartscreenModel * model,
        { current_state = model->display_state; },
        false);

    FURI_LOG_D("FlipperWedgeScene", "Display timer fired - current display state: %d", current_state);

    // State machine for display sequence
    if(current_state == FlipperWedgeDisplayStateResult) {
        // First timer: showed result, check if this is success or error
        bool is_error = false;
        with_view_model(
            flipper_wedge_startscreen_get_view(app->flipper_wedge_startscreen),
            FlipperWedgeStartscreenModel * model,
            {
                // Error messages don't need "Sent" confirmation
                is_error = (strstr(model->status_text, "Not NFC Forum Compliant") != NULL) ||
                          (strstr(model->status_text, "Unsupported NFC Forum Type") != NULL) ||
                          (strstr(model->status_text, "NDEF Not Found") != NULL);
            },
            false);

        if(is_error) {
            // For errors, skip "Sent" and go directly to cooldown
            flipper_wedge_led_reset(app);
            flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateIdle);
            flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, "");
            furi_timer_start(app->display_timer, furi_ms_to_ticks(300));
        } else {
            // For success, show "Sent" with vibration feedback
            flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateSent);
            flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, "Sent");
            flipper_wedge_play_happy_bump(app);  // Vibrate when "Sent" is displayed
            furi_timer_start(app->display_timer, furi_ms_to_ticks(200));
        }
    } else if(current_state == FlipperWedgeDisplayStateSent) {
        // Second timer: showed "Sent", now cooldown and prepare to restart
        flipper_wedge_led_reset(app);
        flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateIdle);
        flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, "");
        furi_timer_start(app->display_timer, furi_ms_to_ticks(300));
    } else {
        // Third timer: cooldown done, return to Idle state for scanning to restart
        flipper_wedge_led_reset(app);
        flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateIdle);
        flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, "");
        app->scan_state = FlipperWedgeScanStateIdle;
        // Tick handler will restart scanning automatically
    }
}

// Timeout callback - used for combo mode timeout when waiting for second tag
static void flipper_wedge_scene_startscreen_timeout_callback(void* context) {
    furi_assert(context);
    FlipperWedge* app = context;

    // Only send timeout event if we're still waiting for second tag
    if(app->scan_state == FlipperWedgeScanStateWaitingSecond) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher,
            FlipperWedgeCustomEventScanTimeout);
    }
}

void flipper_wedge_scene_startscreen_callback(FlipperWedgeCustomEvent event, void* context) {
    furi_assert(context);
    FlipperWedge* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

// NFC callback - called when an NFC tag is detected
static void flipper_wedge_scene_startscreen_nfc_callback(FlipperWedgeNfcData* data, void* context) {
    furi_assert(context);
    FlipperWedge* app = context;

    FURI_LOG_I("FlipperWedgeScene", "NFC callback: uid_len=%d, has_ndef=%d, error=%d", data->uid_len, data->has_ndef, data->error);

    // Store the NFC data
    app->nfc_uid_len = data->uid_len;
    memcpy(app->nfc_uid, data->uid, data->uid_len);
    app->nfc_error = data->error;

    // In NDEF mode, only store NDEF text; in other NFC modes, store UID
    if(data->has_ndef) {
        snprintf(app->ndef_text, sizeof(app->ndef_text), "%s", data->ndef_text);
    } else {
        app->ndef_text[0] = '\0';
    }

    // Send event to main thread
    FURI_LOG_D("FlipperWedgeScene", "NFC callback: sending custom event");
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipperWedgeCustomEventNfcDetected);
}

// RFID callback - called when an RFID tag is detected
static void flipper_wedge_scene_startscreen_rfid_callback(FlipperWedgeRfidData* data, void* context) {
    furi_assert(context);
    FlipperWedge* app = context;

    // Store the RFID data
    app->rfid_uid_len = data->uid_len;
    memcpy(app->rfid_uid, data->uid, data->uid_len);

    // Send event to main thread
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipperWedgeCustomEventRfidDetected);
}

static void flipper_wedge_scene_startscreen_update_status(FlipperWedge* app) {
    bool usb_connected = flipper_wedge_hid_is_usb_connected(flipper_wedge_get_hid(app));
    bool bt_connected = flipper_wedge_hid_is_bt_connected(flipper_wedge_get_hid(app));
    flipper_wedge_startscreen_set_connected_status(
        app->flipper_wedge_startscreen, usb_connected, bt_connected);
}

static void flipper_wedge_scene_startscreen_output_and_reset(FlipperWedge* app) {
    FURI_LOG_I("FlipperWedgeScene", "output_and_reset: nfc_uid_len=%d, rfid_uid_len=%d", app->nfc_uid_len, app->rfid_uid_len);

    // Determine max NDEF length from settings
    size_t max_ndef_len = 0;
    switch(app->ndef_max_len) {
        case FlipperWedgeNdefMaxLen250:
            max_ndef_len = 250;
            break;
        case FlipperWedgeNdefMaxLen500:
            max_ndef_len = 500;
            break;
        case FlipperWedgeNdefMaxLen1000:
            max_ndef_len = 1000;
            break;
        default:
            max_ndef_len = 250;
            break;
    }

    // Sanitize NDEF text if present (remove non-printable chars, apply length limit)
    char sanitized_ndef[FLIPPER_WEDGE_NDEF_MAX_LEN];
    if(app->ndef_text[0] != '\0') {
        size_t original_len = strlen(app->ndef_text);
        size_t sanitized_len = flipper_wedge_sanitize_text(
            app->ndef_text,
            sanitized_ndef,
            sizeof(sanitized_ndef),
            max_ndef_len);

        FURI_LOG_I("FlipperWedgeScene", "NDEF text: original=%zu, sanitized=%zu, limit=%zu",
                   original_len, sanitized_len, max_ndef_len);

        // Warn if text was truncated
        if(max_ndef_len > 0 && original_len > max_ndef_len) {
            FURI_LOG_W("FlipperWedgeScene", "NDEF text truncated from %zu to %zu chars",
                       original_len, sanitized_len);
        }
    } else {
        sanitized_ndef[0] = '\0';
    }

    // Format the output based on mode
    if(app->mode == FlipperWedgeModeNdef) {
        // NDEF mode: output only NDEF text (no UID)
        snprintf(app->output_buffer, sizeof(app->output_buffer), "%s", sanitized_ndef);
    } else {
        // Other modes: format UIDs (and NDEF if present)
        bool nfc_first = (app->mode == FlipperWedgeModeNfc ||
                          app->mode == FlipperWedgeModeNfcThenRfid);

        flipper_wedge_format_output(
            app->nfc_uid_len > 0 ? app->nfc_uid : NULL,
            app->nfc_uid_len,
            app->rfid_uid_len > 0 ? app->rfid_uid : NULL,
            app->rfid_uid_len,
            sanitized_ndef,  // Use sanitized text
            app->delimiter,
            nfc_first,
            app->output_buffer,
            sizeof(app->output_buffer));
    }

    // Show the output briefly
    flipper_wedge_startscreen_set_uid_text(app->flipper_wedge_startscreen, app->output_buffer);
    flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateResult);

    // Type the output via HID (with chunking for long text)
    if(flipper_wedge_hid_is_connected(flipper_wedge_get_hid(app))) {
        size_t text_len = strlen(app->output_buffer);

        // If text is long (>100 chars), show progress and type in chunks
        if(text_len > 100) {
            const size_t chunk_size = 100;
            size_t chunks = (text_len + chunk_size - 1) / chunk_size;

            for(size_t i = 0; i < chunks; i++) {
                // Update progress
                char progress_text[32];
                snprintf(progress_text, sizeof(progress_text), "Typing %zu/%zu...", i + 1, chunks);
                flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, progress_text);

                // Type chunk
                size_t chunk_start = i * chunk_size;
                size_t chunk_len = (chunk_start + chunk_size > text_len) ?
                                   (text_len - chunk_start) : chunk_size;

                char chunk[101];  // 100 + null terminator
                memcpy(chunk, app->output_buffer + chunk_start, chunk_len);
                chunk[chunk_len] = '\0';

                flipper_wedge_hid_type_string(flipper_wedge_get_hid(app), app->keyboard_layout, chunk);

                // Small delay between chunks (let HID catch up)
                furi_delay_ms(50);
            }
        } else {
            // Short text, type normally
            flipper_wedge_hid_type_string(flipper_wedge_get_hid(app), app->keyboard_layout, app->output_buffer);
        }

        if(app->append_enter) {
            flipper_wedge_hid_press_enter(flipper_wedge_get_hid(app));
        }

        // Log to SD card if enabled
        if(app->log_to_sd) {
            flipper_wedge_log_scan(app->output_buffer);
        }
    }

    // LED feedback (haptic happens later when "Sent" is displayed)
    flipper_wedge_led_set_rgb(app, 0, 255, 0);  // Green flash

    // Start display timer to show result, then "Sent", then cooldown (non-blocking)
    if(app->display_timer) {
        furi_timer_stop(app->display_timer);
    } else {
        app->display_timer = furi_timer_alloc(
            flipper_wedge_scene_startscreen_display_timer_callback,
            FuriTimerTypeOnce,
            app);
    }

    // Clear scanned data
    app->nfc_uid_len = 0;
    app->rfid_uid_len = 0;
    app->ndef_text[0] = '\0';

    // Set state to cooldown to prevent immediate re-scan
    app->scan_state = FlipperWedgeScanStateCooldown;

    // Timer will handle three stages: 200ms (show result) -> 200ms (show "Sent") -> 300ms (cooldown)
    furi_timer_start(app->display_timer, furi_ms_to_ticks(200));
}

static void flipper_wedge_scene_startscreen_start_scanning(FlipperWedge* app) {
    // Don't scan if no HID connection
    if(!flipper_wedge_hid_is_connected(flipper_wedge_get_hid(app))) {
        FURI_LOG_D("FlipperWedgeScene", "start_scanning: no HID connection, skipping");
        return;
    }

    FURI_LOG_I("FlipperWedgeScene", "start_scanning: mode=%d, current scan_state=%d", app->mode, app->scan_state);

    // Clear previous scan state to ensure fresh start
    app->nfc_error = FlipperWedgeNfcErrorNone;
    app->nfc_uid_len = 0;
    app->ndef_text[0] = '\0';

    app->scan_state = FlipperWedgeScanStateScanning;
    // Keep display in Idle state to show mode selector while scanning

    // Start appropriate reader(s) based on mode
    switch(app->mode) {
    case FlipperWedgeModeNfc:
        // NFC mode: read UID only (no NDEF parsing)
        flipper_wedge_nfc_set_callback(app->nfc, flipper_wedge_scene_startscreen_nfc_callback, app);
        flipper_wedge_nfc_start(app->nfc, false);
        break;
    case FlipperWedgeModeRfid:
        flipper_wedge_rfid_set_callback(app->rfid, flipper_wedge_scene_startscreen_rfid_callback, app);
        flipper_wedge_rfid_start(app->rfid);
        break;
    case FlipperWedgeModeNdef:
        // NDEF mode: read and parse NDEF text records only
        flipper_wedge_nfc_set_callback(app->nfc, flipper_wedge_scene_startscreen_nfc_callback, app);
        flipper_wedge_nfc_start(app->nfc, true);
        break;
    case FlipperWedgeModeNfcThenRfid:
        // Start with NFC (UID only for combo mode)
        flipper_wedge_nfc_set_callback(app->nfc, flipper_wedge_scene_startscreen_nfc_callback, app);
        flipper_wedge_nfc_start(app->nfc, false);
        break;
    case FlipperWedgeModeRfidThenNfc:
        // Start with RFID
        flipper_wedge_rfid_set_callback(app->rfid, flipper_wedge_scene_startscreen_rfid_callback, app);
        flipper_wedge_rfid_start(app->rfid);
        break;
    default:
        break;
    }
}

static void flipper_wedge_scene_startscreen_stop_scanning(FlipperWedge* app) {
    FURI_LOG_I("FlipperWedgeScene", "stop_scanning: current scan_state=%d", app->scan_state);

    flipper_wedge_nfc_stop(app->nfc);
    flipper_wedge_rfid_stop(app->rfid);
    app->scan_state = FlipperWedgeScanStateIdle;
    flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateIdle);
    FURI_LOG_D("FlipperWedgeScene", "stop_scanning: done, scan_state now Idle");
}

void flipper_wedge_scene_startscreen_on_enter(void* context) {
    furi_assert(context);
    FlipperWedge* app = context;
    flipper_wedge_startscreen_set_callback(
        app->flipper_wedge_startscreen, flipper_wedge_scene_startscreen_callback, app);

    // Set initial mode on the view
    flipper_wedge_startscreen_set_mode(app->flipper_wedge_startscreen, app->mode);

    // Update HID connection status
    flipper_wedge_scene_startscreen_update_status(app);

    // Turn on backlight (but don't enforce - let system timeout work)
    // The app_alloc already turned it on initially, so this is just a wake-up
    notification_message(app->notification, &sequence_display_backlight_on);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperWedgeViewIdStartscreen);

    // Start scanning if HID is connected
    flipper_wedge_scene_startscreen_start_scanning(app);
}

bool flipper_wedge_scene_startscreen_on_event(void* context, SceneManagerEvent event) {
    FlipperWedge* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FlipperWedgeCustomEventModeChange:
            // Stop current scanning and restart with new mode
            flipper_wedge_scene_startscreen_stop_scanning(app);

            // Get the new mode from the view (the view already updated it)
            app->mode = flipper_wedge_startscreen_get_mode(app->flipper_wedge_startscreen);
            FURI_LOG_I("FlipperWedgeScene", "Mode changed to: %d", app->mode);

            // Save mode to persistent storage
            flipper_wedge_save_settings(app);

            flipper_wedge_scene_startscreen_start_scanning(app);
            consumed = true;
            break;

        case FlipperWedgeCustomEventNfcDetected:
            // NFC tag detected
            FURI_LOG_I("FlipperWedgeScene", "Event NfcDetected: mode=%d, scan_state=%d", app->mode, app->scan_state);
            if(app->mode == FlipperWedgeModeNfc) {
                // Single tag mode - output UID immediately
                FURI_LOG_D("FlipperWedgeScene", "NFC single mode - stopping and outputting");
                flipper_wedge_scene_startscreen_stop_scanning(app);
                flipper_wedge_scene_startscreen_output_and_reset(app);
            } else if(app->mode == FlipperWedgeModeNdef) {
                // NDEF mode - check the error status to distinguish between cases
                // We need to retrieve the error from the NFC data that was stored in the callback
                // Since we're in the custom event handler, we need to check what happened

                if(app->ndef_text[0] != '\0') {
                    // NDEF text found - output it
                    FURI_LOG_D("FlipperWedgeScene", "NDEF mode - NDEF text found, outputting");
                    flipper_wedge_scene_startscreen_stop_scanning(app);
                    flipper_wedge_scene_startscreen_output_and_reset(app);
                } else {
                    // No NDEF text - determine error message based on nfc_error field
                    const char* error_msg;
                    if(app->nfc_error == FlipperWedgeNfcErrorNotForumCompliant) {
                        error_msg = "Not NFC Forum Compliant";
                        FURI_LOG_D("FlipperWedgeScene", "NDEF mode - Not NFC Forum compliant (e.g., MIFARE Classic)");
                    } else if(app->nfc_error == FlipperWedgeNfcErrorUnsupportedType) {
                        error_msg = "Unsupported NFC Forum Type";
                        FURI_LOG_D("FlipperWedgeScene", "NDEF mode - Unsupported NFC Forum Type");
                    } else if(app->nfc_error == FlipperWedgeNfcErrorNoTextRecord) {
                        error_msg = "NDEF Not Found";
                        FURI_LOG_D("FlipperWedgeScene", "NDEF mode - NDEF not found");
                    } else {
                        // Fallback for any other case
                        error_msg = "NDEF Not Found";
                        FURI_LOG_D("FlipperWedgeScene", "NDEF mode - Unknown error");
                    }

                    // IMPORTANT: Stop the scanner before showing error to prevent conflicts
                    flipper_wedge_scene_startscreen_stop_scanning(app);

                    flipper_wedge_led_set_rgb(app, 255, 0, 0);  // Red flash

                    // Start display timer to show error for 500ms, then clear and continue scanning
                    if(app->display_timer) {
                        furi_timer_stop(app->display_timer);
                    } else {
                        app->display_timer = furi_timer_alloc(
                            flipper_wedge_scene_startscreen_display_timer_callback,
                            FuriTimerTypeOnce,
                            app);
                    }

                    // Show error message
                    flipper_wedge_startscreen_set_uid_text(app->flipper_wedge_startscreen, "");
                    flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, error_msg);
                    flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateResult);

                    // Clear data
                    app->nfc_uid_len = 0;
                    app->ndef_text[0] = '\0';

                    // Set state to cooldown to prevent immediate re-scan
                    app->scan_state = FlipperWedgeScanStateCooldown;

                    // Timer will detect this is an error (via status_text) and skip "Sent" state
                    furi_timer_start(app->display_timer, furi_ms_to_ticks(500));
                }
            } else if(app->mode == FlipperWedgeModeNfcThenRfid) {
                // Combo mode - now wait for RFID
                flipper_wedge_nfc_stop(app->nfc);
                app->scan_state = FlipperWedgeScanStateWaitingSecond;
                flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, "Waiting for RFID...");
                flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateWaiting);

                // Start RFID scanning
                flipper_wedge_rfid_set_callback(app->rfid, flipper_wedge_scene_startscreen_rfid_callback, app);
                flipper_wedge_rfid_start(app->rfid);

                // Start timeout timer (5 seconds to scan second tag)
                if(!app->timeout_timer) {
                    app->timeout_timer = furi_timer_alloc(
                        flipper_wedge_scene_startscreen_timeout_callback,
                        FuriTimerTypeOnce,
                        app);
                }
                furi_timer_start(app->timeout_timer, furi_ms_to_ticks(5000));
            } else if(app->mode == FlipperWedgeModeRfidThenNfc && app->scan_state == FlipperWedgeScanStateWaitingSecond) {
                // Got the second tag in combo mode - stop timeout timer
                if(app->timeout_timer) {
                    furi_timer_stop(app->timeout_timer);
                }
                flipper_wedge_nfc_stop(app->nfc);
                flipper_wedge_scene_startscreen_output_and_reset(app);
            }
            consumed = true;
            break;

        case FlipperWedgeCustomEventRfidDetected:
            // RFID tag detected
            FURI_LOG_I("FlipperWedgeScene", "Event RfidDetected: mode=%d, scan_state=%d", app->mode, app->scan_state);
            if(app->mode == FlipperWedgeModeRfid) {
                // Single tag mode - output immediately
                FURI_LOG_D("FlipperWedgeScene", "RFID single/any mode - stopping and outputting");
                flipper_wedge_scene_startscreen_stop_scanning(app);
                flipper_wedge_scene_startscreen_output_and_reset(app);
            } else if(app->mode == FlipperWedgeModeRfidThenNfc) {
                // Combo mode - now wait for NFC
                flipper_wedge_rfid_stop(app->rfid);
                app->scan_state = FlipperWedgeScanStateWaitingSecond;
                flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, "Waiting for NFC...");
                flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateWaiting);

                // Start NFC scanning (UID only for combo mode)
                flipper_wedge_nfc_set_callback(app->nfc, flipper_wedge_scene_startscreen_nfc_callback, app);
                flipper_wedge_nfc_start(app->nfc, false);

                // Start timeout timer (5 seconds to scan second tag)
                if(!app->timeout_timer) {
                    app->timeout_timer = furi_timer_alloc(
                        flipper_wedge_scene_startscreen_timeout_callback,
                        FuriTimerTypeOnce,
                        app);
                }
                furi_timer_start(app->timeout_timer, furi_ms_to_ticks(5000));
            } else if(app->mode == FlipperWedgeModeNfcThenRfid && app->scan_state == FlipperWedgeScanStateWaitingSecond) {
                // Got the second tag in combo mode - stop timeout timer
                if(app->timeout_timer) {
                    furi_timer_stop(app->timeout_timer);
                }
                flipper_wedge_rfid_stop(app->rfid);
                flipper_wedge_scene_startscreen_output_and_reset(app);
            }
            consumed = true;
            break;

        case FlipperWedgeCustomEventStartscreenBack:
            flipper_wedge_scene_startscreen_stop_scanning(app);
            notification_message(app->notification, &sequence_reset_red);
            notification_message(app->notification, &sequence_reset_green);
            notification_message(app->notification, &sequence_reset_blue);
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, FlipperWedgeSceneStartscreen)) {
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;

        case FlipperWedgeCustomEventOpenSettings:
            // Stop scanning and open Settings
            flipper_wedge_scene_startscreen_stop_scanning(app);
            scene_manager_next_scene(app->scene_manager, FlipperWedgeSceneSettings);
            consumed = true;
            break;

        case FlipperWedgeCustomEventScanTimeout:
            // Combo mode timeout - second tag wasn't scanned in time
            FURI_LOG_I("FlipperWedgeScene", "Scan timeout - resetting to scan first tag");

            // Stop any running scanners
            flipper_wedge_nfc_stop(app->nfc);
            flipper_wedge_rfid_stop(app->rfid);

            // Clear stored data from first tag
            app->nfc_uid_len = 0;
            app->rfid_uid_len = 0;
            app->ndef_text[0] = '\0';

            // Show timeout message briefly
            flipper_wedge_startscreen_set_status_text(app->flipper_wedge_startscreen, "Scan timed out");
            flipper_wedge_startscreen_set_display_state(app->flipper_wedge_startscreen, FlipperWedgeDisplayStateIdle);

            // Reset to scanning state and restart for first tag
            app->scan_state = FlipperWedgeScanStateIdle;
            // Tick handler will restart scanning automatically
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Update HID connection status periodically
        flipper_wedge_scene_startscreen_update_status(app);

        // Process NFC state machine (handles scanner->poller transitions and callbacks)
        flipper_wedge_nfc_tick(app->nfc);

        // Check if we should start/stop scanning based on HID connection
        bool connected = flipper_wedge_hid_is_connected(flipper_wedge_get_hid(app));
        if(connected && app->scan_state == FlipperWedgeScanStateIdle) {
            flipper_wedge_scene_startscreen_start_scanning(app);
        } else if(!connected && app->scan_state != FlipperWedgeScanStateIdle) {
            flipper_wedge_scene_startscreen_stop_scanning(app);
        }
    }

    return consumed;
}

void flipper_wedge_scene_startscreen_on_exit(void* context) {
    FlipperWedge* app = context;
    flipper_wedge_scene_startscreen_stop_scanning(app);

    // Stop display timer if running
    if(app->display_timer) {
        furi_timer_stop(app->display_timer);
    }

    // Stop timeout timer if running
    if(app->timeout_timer) {
        furi_timer_stop(app->timeout_timer);
    }

    // Return backlight to auto mode
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
}

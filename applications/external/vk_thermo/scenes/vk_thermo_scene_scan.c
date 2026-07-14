#include "../vk_thermo.h"
#include "../helpers/vk_thermo_custom_event.h"
#include "../views/vk_thermo_scan_view.h"
#include "../helpers/vk_thermo_haptic.h"
#include "../helpers/vk_thermo_led.h"
#include "../helpers/vk_thermo_speaker.h"
#include "../helpers/vk_thermo_nfc.h"
#include "../helpers/vk_thermo_storage.h"

// Convert EH timeout setting index to seconds (0 = no timeout / indefinite)
static const uint32_t eh_timeout_seconds[] = {1, 2, 5, 10, 30, 0};

// Restart delay in ticks (250ms per tick)
#define RESTART_DELAY_ERROR 4 // 1000ms after error

// Tick-based restart state (avoids blocking the GUI thread)
static uint32_t restart_countdown = 0;

// Whether a successful result is being displayed (waiting for user dismiss)
static bool showing_result = false;

// NFC callback - called when temperature is read or error occurs
// IMPORTANT: This runs on the GUI thread (from tick handler).
// NEVER block here - no furi_delay_ms, no long operations.
static void vk_thermo_scene_scan_nfc_callback(
    VkThermoNfcEvent event,
    VkThermoNfcData* data,
    void* context) {
    furi_assert(context);
    VkThermo* app = context;

    if(event == VkThermoNfcEventTagDetected) {
        // Tag first detected - show "Reading..." state
        vk_thermo_scan_view_set_state(app->scan_view, VkThermoScanStateReading);
        view_dispatcher_send_custom_event(app->view_dispatcher, VkThermoCustomEventNfcTagDetected);
        return;
    }

    if(data->valid) {
        // We have a valid temperature — this is ALWAYS a success, period.
        // The NFC event type doesn't matter; what matters is that we got data.
        showing_result = true;
        restart_countdown = 0; // Cancel any pending error restart

        // Update the scan view with temperature
        vk_thermo_scan_view_set_temperature(app->scan_view, data->temperature_celsius, data->uid);

        // Log the reading
        vk_thermo_log_add_entry(&app->log, data->uid, data->temperature_celsius);

        // Play success feedback
        vk_thermo_play_happy_bump(app);
        vk_thermo_led_success(app);
        vk_thermo_play_success_sound(app);

    } else if(!showing_result) {
        // No valid data and no result on screen — genuine error
        vk_thermo_scan_view_set_state(app->scan_view, VkThermoScanStateError);

        // Play error feedback
        vk_thermo_play_bad_bump(app);
        vk_thermo_led_error(app);
        vk_thermo_play_error_sound(app);

        // Schedule restart via tick countdown (non-blocking)
        restart_countdown = RESTART_DELAY_ERROR;
    }
}

void vk_thermo_scene_scan_callback(VkThermoCustomEvent event, void* context) {
    furi_assert(context);
    VkThermo* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void vk_thermo_scene_scan_on_enter(void* context) {
    furi_assert(context);
    VkThermo* app = context;

    // Reset restart state
    restart_countdown = 0;
    showing_result = false;

    // Set callback for scan view
    vk_thermo_scan_view_set_callback(app->scan_view, vk_thermo_scene_scan_callback, app);

    // Set temperature unit preference
    vk_thermo_scan_view_set_temp_unit(app->scan_view, app->temp_unit);

    // Set NFC callback, EH timeout, and start scanning
    vk_thermo_nfc_set_callback(app->nfc, vk_thermo_scene_scan_nfc_callback, app);
    uint32_t timeout_idx = app->eh_timeout < VkThermoEhTimeoutCount ? app->eh_timeout :
                                                                      VkThermoEhTimeout5s;
    vk_thermo_nfc_set_eh_timeout(app->nfc, eh_timeout_seconds[timeout_idx]);
    vk_thermo_nfc_set_debug(app->nfc, app->debug == VkThermoDebugOn);
    vk_thermo_nfc_start(app->nfc);

    // Set initial state
    vk_thermo_scan_view_set_state(app->scan_view, VkThermoScanStateScanning);

    // Switch to scan view
    view_dispatcher_switch_to_view(app->view_dispatcher, VkThermoViewIdScan);
}

bool vk_thermo_scene_scan_on_event(void* context, SceneManagerEvent event) {
    VkThermo* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case VkThermoCustomEventScanOk:
            if(showing_result) {
                // Dismiss result and resume scanning
                showing_result = false;
                vk_thermo_scan_view_set_state(app->scan_view, VkThermoScanStateScanning);
                vk_thermo_nfc_start(app->nfc);
            } else {
                // Navigate to log view
                scene_manager_next_scene(app->scene_manager, VkThermoSceneLog);
            }
            consumed = true;
            break;

        case VkThermoCustomEventScanLeft:
            // Navigate to graph view
            scene_manager_next_scene(app->scene_manager, VkThermoSceneGraph);
            consumed = true;
            break;

        case VkThermoCustomEventScanRight:
            // Navigate to settings
            scene_manager_next_scene(app->scene_manager, VkThermoSceneSettings);
            consumed = true;
            break;

        case VkThermoCustomEventNfcTagDetected:
            // Tag detected - heartbeat feedback (LED + vibro, no sound)
            vk_thermo_led_detect(app);
            vk_thermo_play_heartbeat(app);
            consumed = true;
            break;

        case VkThermoCustomEventNfcReadSuccess:
        case VkThermoCustomEventNfcReadError:
            // Feedback already played immediately in NFC callback
            consumed = true;
            break;

        case VkThermoCustomEventScanBack:
            if(showing_result) {
                // Dismiss result and resume scanning
                showing_result = false;
                vk_thermo_scan_view_set_state(app->scan_view, VkThermoScanStateScanning);
                vk_thermo_nfc_start(app->nfc);
            } else {
                // Stop NFC and exit
                vk_thermo_nfc_stop(app->nfc);
                notification_message(app->notification, &sequence_reset_red);
                notification_message(app->notification, &sequence_reset_green);
                notification_message(app->notification, &sequence_reset_blue);
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Update scan view animation
        vk_thermo_scan_view_tick(app->scan_view);

        // Handle restart countdown (non-blocking scan restart)
        if(restart_countdown > 0) {
            restart_countdown--;
            if(restart_countdown == 0) {
                vk_thermo_nfc_start(app->nfc);
                vk_thermo_scan_view_set_state(app->scan_view, VkThermoScanStateScanning);
            }
        }

        // Process NFC state machine on tick
        vk_thermo_nfc_tick(app->nfc);
    }

    return consumed;
}

void vk_thermo_scene_scan_on_exit(void* context) {
    VkThermo* app = context;
    restart_countdown = 0;
    showing_result = false;
    // Stop NFC scanning
    vk_thermo_nfc_stop(app->nfc);
}

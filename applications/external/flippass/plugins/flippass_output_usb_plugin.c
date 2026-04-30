/**
 * @file flippass_output_usb_plugin.c
 * @brief Late-loaded USB HID transport provider for FlipPass.
 */

#include "flippass_output_plugin.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>

#define FLIPPASS_USB_ENUMERATION_TIMEOUT_MS 15000U
#define FLIPPASS_USB_ENUMERATION_GRACE_MS   5000U
#define FLIPPASS_USB_PREPARE_RETRY_COUNT    3U
#define FLIPPASS_USB_POLL_DELAY_MS          100U
#define FLIPPASS_USB_PRESS_DELAY_MS         12U
#define FLIPPASS_USB_RELEASE_DELAY_MS       18U
#define FLIPPASS_USB_SWITCH_DELAY_MS        150U
#define FLIPPASS_USB_SETTLE_DELAY_MS        300U

enum {
    FlipPassUsbPrepareFlagConnected = 1U << 0,
};

typedef struct {
    volatile uint8_t flags;
    volatile uint8_t activity_serial;
} FlipPassUsbPrepareState;

typedef struct {
    FuriHalUsbInterface* previous_interface;
    bool was_locked;
} FlipPassUsbPluginState;

static FlipPassUsbPluginState flippass_output_usb_state = {0};

static void flippass_output_usb_progress(
    const FlipPassOutputPluginHostApiV1* host_api,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(host_api != NULL && host_api->progress != NULL) {
        host_api->progress(host_api->host_context, stage, detail, percent);
    }
}

static void
    flippass_output_usb_log(const FlipPassOutputPluginHostApiV1* host_api, const char* message) {
    if(host_api != NULL && host_api->log != NULL) {
        host_api->log(host_api->host_context, "flippass_output_usb", message);
    }
}

static bool flippass_output_usb_should_cancel(const FlipPassOutputPluginHostApiV1* host_api) {
    return host_api != NULL && host_api->should_cancel != NULL &&
           host_api->should_cancel(host_api->host_context);
}

static bool flippass_output_usb_delay_or_cancel(
    const FlipPassOutputPluginHostApiV1* host_api,
    uint32_t delay_ms) {
    while(delay_ms > 0U) {
        const uint32_t step_ms = (delay_ms > 25U) ? 25U : delay_ms;

        if(flippass_output_usb_should_cancel(host_api)) {
            return false;
        }

        furi_delay_ms(step_ms);
        delay_ms -= step_ms;
    }

    return !flippass_output_usb_should_cancel(host_api);
}

static void
    flippass_usb_prepare_mark_activity(FlipPassUsbPrepareState* prepare_state, bool connected) {
    if(prepare_state != NULL) {
        uint8_t flags = prepare_state->flags;
        if(connected) {
            flags |= FlipPassUsbPrepareFlagConnected;
        }
        prepare_state->flags = flags;
        prepare_state->activity_serial++;
    }
}

static void flippass_usb_prepare_usb_state_callback(FuriHalUsbStateEvent state, void* context) {
    switch(state) {
    case FuriHalUsbStateEventReset:
    case FuriHalUsbStateEventDescriptorRequest:
    case FuriHalUsbStateEventWakeup:
    case FuriHalUsbStateEventSuspend:
        flippass_usb_prepare_mark_activity(context, false);
        break;
    }
}

static void flippass_usb_prepare_state_callback(bool state, void* context) {
    FlipPassUsbPrepareState* prepare_state = context;
    if(prepare_state == NULL) {
        return;
    }

    if(state) {
        flippass_usb_prepare_mark_activity(prepare_state, true);
    } else {
        prepare_state->flags &= (uint8_t)~FlipPassUsbPrepareFlagConnected;
    }
}

static uint8_t flippass_usb_prepare_progress_percent(
    uint8_t attempt_index,
    uint32_t elapsed_ms,
    uint32_t attempt_timeout_ms) {
    const uint32_t total_timeout_ms = attempt_timeout_ms * FLIPPASS_USB_PREPARE_RETRY_COUNT;
    const uint32_t completed_before_attempt =
        (attempt_index > 0U) ? ((uint32_t)(attempt_index - 1U) * attempt_timeout_ms) : 0U;
    uint32_t aggregate_elapsed = completed_before_attempt + elapsed_ms;

    if(total_timeout_ms == 0U) {
        return 5U;
    }

    if(aggregate_elapsed > total_timeout_ms) {
        aggregate_elapsed = total_timeout_ms;
    }

    return (uint8_t)(5U + ((aggregate_elapsed * 35U) / total_timeout_ms));
}

static void flippass_usb_prepare_progress(
    const FlipPassOutputPluginHostApiV1* host_api,
    uint8_t attempt_index,
    uint32_t elapsed_ms,
    uint32_t attempt_timeout_ms) {
    char detail[48];

    snprintf(
        detail,
        sizeof(detail),
        "Waiting for USB HID host (%u/%u).",
        (unsigned)attempt_index,
        (unsigned)FLIPPASS_USB_PREPARE_RETRY_COUNT);
    flippass_output_usb_progress(
        host_api,
        "Connecting",
        detail,
        flippass_usb_prepare_progress_percent(attempt_index, elapsed_ms, attempt_timeout_ms));
}

static bool flippass_usb_prepare_attach_hid(
    const FlipPassOutputPluginHostApiV1* host_api,
    uint8_t attempt_index) {
    char detail[40];

    snprintf(
        detail,
        sizeof(detail),
        "Re-enumerating USB HID (%u/%u).",
        (unsigned)attempt_index,
        (unsigned)FLIPPASS_USB_PREPARE_RETRY_COUNT);
    flippass_output_usb_progress(
        host_api,
        "Cleaning",
        detail,
        (uint8_t)(3U +
                  (((uint32_t)(attempt_index - 1U) * 2U) / FLIPPASS_USB_PREPARE_RETRY_COUNT)));

    furi_hal_hid_kb_release_all();
    if(!flippass_output_usb_delay_or_cancel(host_api, FLIPPASS_USB_RELEASE_DELAY_MS)) {
        return false;
    }
    if(!furi_hal_usb_set_config(NULL, NULL)) {
        flippass_output_usb_log(host_api, "prepare detach failed");
        return false;
    }

    if(!flippass_output_usb_delay_or_cancel(host_api, FLIPPASS_USB_SWITCH_DELAY_MS)) {
        return false;
    }
    furi_hal_hid_kb_release_all();
    if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
        flippass_output_usb_log(host_api, "prepare attach failed");
        return false;
    }

    if(!flippass_output_usb_delay_or_cancel(host_api, FLIPPASS_USB_SWITCH_DELAY_MS)) {
        return false;
    }
    if(attempt_index > 1U) {
        furi_hal_usb_reinit();
        if(!flippass_output_usb_delay_or_cancel(host_api, FLIPPASS_USB_SWITCH_DELAY_MS)) {
            return false;
        }
    }

    return true;
}

static bool flippass_usb_prepare_wait_connected(
    const FlipPassOutputPluginHostApiV1* host_api,
    FlipPassUsbPrepareState* prepare_state,
    uint8_t attempt_index,
    uint32_t attempt_timeout_ms,
    uint32_t* waited_ms) {
    uint32_t elapsed_ms = 0U;
    uint32_t deadline_ms = FLIPPASS_USB_ENUMERATION_TIMEOUT_MS;
    uint8_t observed_activity_serial = 0U;

    observed_activity_serial = prepare_state->activity_serial;

    while(((prepare_state->flags & FlipPassUsbPrepareFlagConnected) == 0U) &&
          !furi_hal_hid_is_connected() && elapsed_ms < attempt_timeout_ms &&
          !flippass_output_usb_should_cancel(host_api)) {
        if(elapsed_ms >= deadline_ms) {
            break;
        }

        if(!flippass_output_usb_delay_or_cancel(host_api, FLIPPASS_USB_POLL_DELAY_MS)) {
            return false;
        }
        elapsed_ms += FLIPPASS_USB_POLL_DELAY_MS;

        if(observed_activity_serial != prepare_state->activity_serial) {
            observed_activity_serial = prepare_state->activity_serial;
            deadline_ms = elapsed_ms + FLIPPASS_USB_ENUMERATION_GRACE_MS;
            if(deadline_ms > attempt_timeout_ms) {
                deadline_ms = attempt_timeout_ms;
            }
        }

        flippass_usb_prepare_progress(host_api, attempt_index, elapsed_ms, attempt_timeout_ms);
    }

    *waited_ms = elapsed_ms;
    if(flippass_output_usb_should_cancel(host_api)) {
        return false;
    }
    return ((prepare_state->flags & FlipPassUsbPrepareFlagConnected) != 0U) ||
           furi_hal_hid_is_connected();
}

static bool flippass_output_usb_begin(const FlipPassOutputPluginHostApiV1* host_api) {
    const bool was_locked = furi_hal_usb_is_locked();
    const uint32_t attempt_timeout_ms =
        FLIPPASS_USB_ENUMERATION_TIMEOUT_MS + FLIPPASS_USB_ENUMERATION_GRACE_MS;

    if(was_locked) {
        furi_hal_usb_unlock();
    }

    if(flippass_output_usb_state.previous_interface == NULL) {
        flippass_output_usb_state.was_locked = was_locked;
        flippass_output_usb_state.previous_interface = furi_hal_usb_get_config();
    }

    for(uint8_t attempt = 1U; attempt <= FLIPPASS_USB_PREPARE_RETRY_COUNT; attempt++) {
        FlipPassUsbPrepareState prepare_state = {0};
        uint32_t waited_ms = 0U;

        if(flippass_output_usb_should_cancel(host_api)) {
            return false;
        }

        furi_hal_usb_set_state_callback(flippass_usb_prepare_usb_state_callback, &prepare_state);
        furi_hal_hid_set_state_callback(flippass_usb_prepare_state_callback, &prepare_state);
        if(!flippass_usb_prepare_attach_hid(host_api, attempt)) {
            furi_hal_hid_set_state_callback(NULL, NULL);
            furi_hal_usb_set_state_callback(NULL, NULL);
            continue;
        }

        if(flippass_usb_prepare_wait_connected(
               host_api, &prepare_state, attempt, attempt_timeout_ms, &waited_ms)) {
            furi_hal_hid_set_state_callback(NULL, NULL);
            furi_hal_usb_set_state_callback(NULL, NULL);
            flippass_output_usb_progress(host_api, "Typing", "USB HID connected.", 40U);
            return flippass_output_usb_delay_or_cancel(host_api, FLIPPASS_USB_SETTLE_DELAY_MS);
        }

        UNUSED(waited_ms);
        furi_hal_hid_set_state_callback(NULL, NULL);
        furi_hal_usb_set_state_callback(NULL, NULL);
        furi_hal_hid_kb_release_all();
        furi_hal_usb_set_config(NULL, NULL);
        furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
    }

    return false;
}

static bool
    flippass_output_usb_press_key(const FlipPassOutputPluginHostApiV1* host_api, uint16_t hid_key) {
    UNUSED(host_api);
    return (hid_key != HID_KEYBOARD_NONE) && furi_hal_hid_kb_press(hid_key);
}

static bool flippass_output_usb_release_key(
    const FlipPassOutputPluginHostApiV1* host_api,
    uint16_t hid_key) {
    UNUSED(host_api);
    return (hid_key != HID_KEYBOARD_NONE) && furi_hal_hid_kb_release(hid_key);
}

static void flippass_output_usb_release_all(const FlipPassOutputPluginHostApiV1* host_api) {
    UNUSED(host_api);
    furi_hal_hid_kb_release_all();
}

static void flippass_output_usb_end(const FlipPassOutputPluginHostApiV1* host_api) {
    UNUSED(host_api);

    furi_hal_hid_kb_release_all();
    furi_delay_ms(FLIPPASS_USB_RELEASE_DELAY_MS);
    if(flippass_output_usb_state.previous_interface != NULL) {
        furi_hal_usb_set_config(NULL, NULL);
        furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
        furi_hal_usb_set_config(flippass_output_usb_state.previous_interface, NULL);
        furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
        flippass_output_usb_state.previous_interface = NULL;
    }

    if(flippass_output_usb_state.was_locked) {
        furi_hal_usb_lock();
        flippass_output_usb_state.was_locked = false;
    }
}

static bool flippass_output_usb_is_connected(const FlipPassOutputPluginHostApiV1* host_api) {
    UNUSED(host_api);
    return furi_hal_hid_is_connected();
}

static bool flippass_output_usb_is_advertising(const FlipPassOutputPluginHostApiV1* host_api) {
    UNUSED(host_api);
    return false;
}

static bool flippass_output_usb_advertise(const FlipPassOutputPluginHostApiV1* host_api) {
    const bool was_locked = furi_hal_usb_is_locked();

    if(was_locked) {
        furi_hal_usb_unlock();
    }

    if(flippass_output_usb_state.previous_interface == NULL) {
        flippass_output_usb_state.was_locked = was_locked;
        flippass_output_usb_state.previous_interface = furi_hal_usb_get_config();
    }

    flippass_output_usb_progress(host_api, "Preparing", "USB HID pre-warm.", 15U);
    return flippass_usb_prepare_attach_hid(host_api, 1U);
}

static void flippass_output_usb_get_name(char* buffer, size_t size) {
    if(buffer != NULL && size > 0U) {
        snprintf(buffer, size, "USB HID");
    }
}

static void flippass_output_usb_cleanup(const FlipPassOutputPluginHostApiV1* host_api) {
    flippass_output_usb_end(host_api);
}

static const FlipPassOutputPluginV1 flippass_output_usb_plugin = {
    .api_version = FLIPPASS_OUTPUT_PLUGIN_API_VERSION,
    .module_name = "flippass_output_usb",
    .transport = FlipPassOutputPluginTransportUsb,
    .begin = flippass_output_usb_begin,
    .press_key = flippass_output_usb_press_key,
    .release_key = flippass_output_usb_release_key,
    .release_all = flippass_output_usb_release_all,
    .end = flippass_output_usb_end,
    .is_connected = flippass_output_usb_is_connected,
    .is_advertising = flippass_output_usb_is_advertising,
    .advertise = flippass_output_usb_advertise,
    .get_name = flippass_output_usb_get_name,
    .cleanup = flippass_output_usb_cleanup,
};

static const FlipperAppPluginDescriptor flippass_output_usb_descriptor = {
    .appid = FLIPPASS_OUTPUT_USB_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OUTPUT_PLUGIN_API_VERSION,
    .entry_point = &flippass_output_usb_plugin,
};

const FlipPassOutputPluginV1* flippass_output_usb_plugin_table(void) {
    return &flippass_output_usb_plugin;
}

const FlipperAppPluginDescriptor* flippass_output_usb_plugin_ep(void) {
    return &flippass_output_usb_descriptor;
}

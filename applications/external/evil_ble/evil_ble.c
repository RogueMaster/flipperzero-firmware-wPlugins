/**
 * evil_ble.c — Main application, scene management, and BLE cloning engine.
 *
 * View hierarchy
 * ~~~~~~~~~~~~~~
 *   EvilBleViewMainMenu    Submenu  — "Scan", "Clone Selected", "Stop Clone"
 *   EvilBleViewDeviceList  Submenu  — scrollable list of scanned BLE devices
 *   EvilBleViewCloneStatus TextBox  — live broadcast status with MAC + name
 *
 * Clone construction
 * ~~~~~~~~~~~~~~~~~~
 * When the user selects a device from the list, we configure the Flipper's
 * BLE extra-beacon with the cloned MAC and a synthesised advertisement
 * payload built from the device name.  The extra_beacon API runs as a
 * separate BLE advertising stream — normal Flipper BLE operation continues.
 *
 * Cleanup guarantee
 * ~~~~~~~~~~~~~~~~~
 * evil_ble_stop_clone() is called both on explicit "Stop Clone" user action
 * and in evil_ble_app_free() so the extra_beacon is always stopped before
 * the process exits.
 */

#include "evil_ble.h"

#include <extra_beacon.h>
#include <furi_hal_bt.h>
#include <notification/notification_messages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "EvilBLE"

/* --------------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------------- */
static bool evil_ble_navigation_callback(void* ctx);
static bool evil_ble_custom_event_callback(void* ctx, uint32_t event);
static void evil_ble_main_menu_callback(void* ctx, uint32_t index);
static void evil_ble_device_list_callback(void* ctx, uint32_t index);
static void evil_ble_rebuild_device_list(EvilBleApp* app);
static void evil_ble_start_clone(EvilBleApp* app, uint32_t device_idx);
static void evil_ble_stop_clone(EvilBleApp* app);
static void evil_ble_update_clone_status(EvilBleApp* app);

/* --------------------------------------------------------------------------
 * Scanner device-found callback
 *
 * Called on the UART worker thread.  We only send a custom event to the
 * GUI thread — no GUI operations here.
 * -------------------------------------------------------------------------- */
static void evil_ble_on_device_found(void* ctx) {
    EvilBleApp* app = (EvilBleApp*)ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, EvilBleCustomEventDeviceFound);
}

/* --------------------------------------------------------------------------
 * Navigation callback — Back key not consumed by the active view
 * -------------------------------------------------------------------------- */
static bool evil_ble_navigation_callback(void* ctx) {
    EvilBleApp* app = (EvilBleApp*)ctx;

    switch(app->current_view) {
    case EvilBleViewDeviceList:
        /* Stop scan before returning to main menu. */
        evil_ble_scanner_stop(app->scanner);
        app->current_view = EvilBleViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, EvilBleViewMainMenu);
        return true;

    case EvilBleViewCloneStatus:
        /* User pressed Back from the status screen — stop the beacon. */
        evil_ble_stop_clone(app);
        app->current_view = EvilBleViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, EvilBleViewMainMenu);
        return true;

    case EvilBleViewMainMenu:
    default:
        /* Back from root menu exits. */
        view_dispatcher_stop(app->view_dispatcher);
        return false;
    }
}

/* --------------------------------------------------------------------------
 * Custom event callback — runs on the GUI thread
 * -------------------------------------------------------------------------- */
static bool evil_ble_custom_event_callback(void* ctx, uint32_t event) {
    EvilBleApp* app = (EvilBleApp*)ctx;

    switch((EvilBleCustomEvent)event) {
    case EvilBleCustomEventDeviceFound:
        /* Refresh the device list view if it is currently visible. */
        if(app->current_view == EvilBleViewDeviceList) {
            evil_ble_rebuild_device_list(app);
        }
        return true;

    case EvilBleCustomEventCloneStart:
        evil_ble_start_clone(app, app->selected_device_idx);
        return true;

    case EvilBleCustomEventCloneStop:
        evil_ble_stop_clone(app);
        app->current_view = EvilBleViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, EvilBleViewMainMenu);
        return true;
    }

    return false;
}

/* --------------------------------------------------------------------------
 * Main menu
 * -------------------------------------------------------------------------- */
typedef enum {
    EvilBleMainMenuScan = 0,
    EvilBleMainMenuClone = 1,
    EvilBleMainMenuStop = 2,
} EvilBleMainMenuItem;

static void evil_ble_rebuild_main_menu(EvilBleApp* app) {
    submenu_reset(app->main_menu);
    submenu_set_header(app->main_menu, "Evil BLE");
    submenu_add_item(
        app->main_menu, "Scan for Devices", EvilBleMainMenuScan, evil_ble_main_menu_callback, app);

    /* Label "Clone Selected" only if we have at least one scanned device.
     * submenu_add_item does NOT copy the string, so use persistent storage. */
    uint32_t count = evil_ble_scanner_get_count(app->scanner);
    if(count > 0) {
        snprintf(
            app->clone_menu_label,
            sizeof(app->clone_menu_label),
            "Clone Selected (%lu found)",
            (unsigned long)count);
    } else {
        strncpy(
            app->clone_menu_label,
            "Clone Selected (scan first)",
            sizeof(app->clone_menu_label) - 1);
        app->clone_menu_label[sizeof(app->clone_menu_label) - 1] = '\0';
    }
    submenu_add_item(
        app->main_menu,
        app->clone_menu_label,
        EvilBleMainMenuClone,
        evil_ble_main_menu_callback,
        app);

    /* "Stop Clone" always visible; shows beacon active state. */
    const char* stop_label = app->cloning ? "Stop Clone [ACTIVE]" : "Stop Clone";
    submenu_add_item(
        app->main_menu, stop_label, EvilBleMainMenuStop, evil_ble_main_menu_callback, app);
}

static void evil_ble_main_menu_callback(void* ctx, uint32_t index) {
    EvilBleApp* app = (EvilBleApp*)ctx;

    switch((EvilBleMainMenuItem)index) {
    case EvilBleMainMenuScan:
        /* Start scan and switch to the device list view. */
        evil_ble_scanner_start(app->scanner);
        evil_ble_rebuild_device_list(app);
        app->current_view = EvilBleViewDeviceList;
        view_dispatcher_switch_to_view(app->view_dispatcher, EvilBleViewDeviceList);
        break;

    case EvilBleMainMenuClone:
        /* Show device list. Shows "Scanning..." when empty, hinting user to scan first. */
        evil_ble_rebuild_device_list(app);
        app->current_view = EvilBleViewDeviceList;
        view_dispatcher_switch_to_view(app->view_dispatcher, EvilBleViewDeviceList);
        break;

    case EvilBleMainMenuStop:
        if(app->cloning) {
            view_dispatcher_send_custom_event(app->view_dispatcher, EvilBleCustomEventCloneStop);
        }
        break;
    }
}

/* --------------------------------------------------------------------------
 * Device list view
 * -------------------------------------------------------------------------- */
static void evil_ble_rebuild_device_list(EvilBleApp* app) {
    submenu_reset(app->device_list);
    submenu_set_header(app->device_list, "Select Device");

    uint32_t count = evil_ble_scanner_get_count(app->scanner);

    if(count == 0) {
        /* Placeholder — NULL callback keeps it inert. */
        submenu_add_item(app->device_list, "Scanning...", UINT32_MAX, NULL, NULL);
        return;
    }

    for(uint32_t i = 0; i < count; i++) {
        EvilBleDevice dev;
        if(!evil_ble_scanner_get_device(app->scanner, i, &dev)) continue;

        /* Format: "[name] (dBm)" — fits within the 128-px Flipper display.
         * submenu_add_item does NOT copy the string, so use persistent storage. */
        snprintf(
            app->device_labels[i],
            sizeof(app->device_labels[i]),
            "%s (%d)",
            dev.name,
            (int)dev.rssi);
        submenu_add_item(
            app->device_list, app->device_labels[i], i, evil_ble_device_list_callback, app);
    }
}

static void evil_ble_device_list_callback(void* ctx, uint32_t index) {
    EvilBleApp* app = (EvilBleApp*)ctx;
    app->selected_device_idx = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, EvilBleCustomEventCloneStart);
}

/* --------------------------------------------------------------------------
 * Clone engine
 * -------------------------------------------------------------------------- */

static void evil_ble_start_clone(EvilBleApp* app, uint32_t device_idx) {
    /* Stop any currently running beacon first. */
    if(app->cloning) {
        furi_hal_bt_extra_beacon_stop();
        app->cloning = false;
    }

    EvilBleDevice dev;
    if(!evil_ble_scanner_get_device(app->scanner, device_idx, &dev)) {
        FURI_LOG_W(TAG, "Clone requested for out-of-range index %lu", (unsigned long)device_idx);
        return;
    }

    /* --- Configure the extra beacon MAC and channel parameters --- */
    GapExtraBeaconConfig config = {
        .min_adv_interval_ms = 100,
        .max_adv_interval_ms = 100,
        .adv_channel_map = GapAdvChannelMapAll,
        .adv_power_level = GapAdvPowerLevel_0dBm,
        .address_type = GapAddressTypeRandom,
    };
    /* Clone the target MAC verbatim. */
    memcpy(config.address, dev.mac_bytes, EXTRA_BEACON_MAC_ADDR_SIZE);

    if(!furi_hal_bt_extra_beacon_set_config(&config)) {
        FURI_LOG_E(TAG, "extra_beacon_set_config failed");
        return;
    }

    /* --- Build advertisement payload --- */
    uint8_t adv[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t adv_len;

    if(dev.adv_data_len > 0) {
        /* Use the synthesised payload from the scanner. */
        memcpy(adv, dev.adv_data, dev.adv_data_len);
        adv_len = dev.adv_data_len;
    } else {
        /* Fallback: bare Complete Local Name from raw name string. */
        uint8_t name_len = (uint8_t)strlen(dev.name);
        if(name_len > EXTRA_BEACON_MAX_DATA_SIZE - 2) {
            name_len = EXTRA_BEACON_MAX_DATA_SIZE - 2;
        }
        adv[0] = name_len + 1; /* length: type byte + data */
        adv[1] = 0x09; /* AD type: Complete Local Name */
        memcpy(&adv[2], dev.name, name_len);
        adv_len = (uint8_t)(2 + name_len);
    }

    if(!furi_hal_bt_extra_beacon_set_data(adv, adv_len)) {
        FURI_LOG_E(TAG, "extra_beacon_set_data failed");
        return;
    }

    if(!furi_hal_bt_extra_beacon_start()) {
        FURI_LOG_E(TAG, "extra_beacon_start failed");
        return;
    }

    app->cloning = true;
    app->selected_device_idx = device_idx;
    FURI_LOG_I(TAG, "Cloning %s \"%s\"", dev.mac, dev.name);

    /* Build status text and switch to the status view. */
    evil_ble_update_clone_status(app);
    app->current_view = EvilBleViewCloneStatus;
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBleViewCloneStatus);
}

static void evil_ble_stop_clone(EvilBleApp* app) {
    if(app->cloning) {
        furi_hal_bt_extra_beacon_stop();
        app->cloning = false;
        FURI_LOG_I(TAG, "Clone stopped");
    }
    /* Refresh main menu so "ACTIVE" badge disappears. */
    evil_ble_rebuild_main_menu(app);
}

static void evil_ble_update_clone_status(EvilBleApp* app) {
    EvilBleDevice dev;
    bool ok = evil_ble_scanner_get_device(app->scanner, app->selected_device_idx, &dev);

    text_box_reset(app->clone_status);
    text_box_set_font(app->clone_status, TextBoxFontText);

    if(ok) {
        snprintf(
            app->status_buf,
            sizeof(app->status_buf),
            "Broadcasting as:\n"
            "%s\n"
            "MAC: %s\n"
            "RSSI was: %d dBm\n"
            "\n"
            "Press BACK to stop.",
            dev.name,
            dev.mac,
            (int)dev.rssi);
    } else {
        strncpy(
            app->status_buf, "Clone active.\n\nPress BACK to stop.", sizeof(app->status_buf) - 1);
        app->status_buf[sizeof(app->status_buf) - 1] = '\0';
    }

    text_box_set_text(app->clone_status, app->status_buf);
}

/* --------------------------------------------------------------------------
 * App alloc
 * -------------------------------------------------------------------------- */
static EvilBleApp* evil_ble_app_alloc(void) {
    EvilBleApp* app = malloc(sizeof(EvilBleApp));
    furi_assert(app);
    memset(app, 0, sizeof(EvilBleApp));

    app->current_view = EvilBleViewMainMenu;

    /* ---- Service records ---- */
    app->gui = furi_record_open(RECORD_GUI);

    /* ---- UART + scanner ---- */
    app->uart = evil_ble_uart_alloc();
    furi_assert(app->uart);

    app->scanner = evil_ble_scanner_alloc(app->uart, evil_ble_on_device_found, app);
    furi_assert(app->scanner);

    /* ---- View dispatcher ---- */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, evil_ble_navigation_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, evil_ble_custom_event_callback);

    /* ---- Main menu ---- */
    app->main_menu = submenu_alloc();
    evil_ble_rebuild_main_menu(app);
    view_dispatcher_add_view(
        app->view_dispatcher, EvilBleViewMainMenu, submenu_get_view(app->main_menu));

    /* ---- Device list ---- */
    app->device_list = submenu_alloc();
    submenu_set_header(app->device_list, "Select Device");
    view_dispatcher_add_view(
        app->view_dispatcher, EvilBleViewDeviceList, submenu_get_view(app->device_list));

    /* ---- Clone status TextBox ---- */
    app->clone_status = text_box_alloc();
    text_box_set_font(app->clone_status, TextBoxFontText);
    view_dispatcher_add_view(
        app->view_dispatcher, EvilBleViewCloneStatus, text_box_get_view(app->clone_status));

    return app;
}

/* --------------------------------------------------------------------------
 * App free
 * -------------------------------------------------------------------------- */
static void evil_ble_app_free(EvilBleApp* app) {
    furi_assert(app);

    /* Always stop the beacon before tearing down — order is critical. */
    if(app->cloning) {
        furi_hal_bt_extra_beacon_stop();
        app->cloning = false;
    }

    /* Scanner holds the UART RX callback; free it before freeing uart. */
    if(app->scanner) {
        evil_ble_scanner_stop(app->scanner);
        evil_ble_scanner_free(app->scanner);
        app->scanner = NULL;
    }

    if(app->uart) {
        evil_ble_uart_free(app->uart);
        app->uart = NULL;
    }

    /* Remove views before freeing their underlying objects. */
    view_dispatcher_remove_view(app->view_dispatcher, EvilBleViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, EvilBleViewDeviceList);
    view_dispatcher_remove_view(app->view_dispatcher, EvilBleViewCloneStatus);

    view_dispatcher_free(app->view_dispatcher);

    submenu_free(app->main_menu);
    submenu_free(app->device_list);
    text_box_free(app->clone_status);

    furi_record_close(RECORD_GUI);

    free(app);
}

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */
int32_t evil_ble_app(void* p) {
    UNUSED(p);

    EvilBleApp* app = evil_ble_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, EvilBleViewMainMenu);

    /* Blocks until view_dispatcher_stop() is called (Back from root menu). */
    view_dispatcher_run(app->view_dispatcher);

    evil_ble_app_free(app);

    return 0;
}

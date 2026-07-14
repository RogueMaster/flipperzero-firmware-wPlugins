#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "ble_scanner_worker.h"
#include "ble_uart.h"

/* =========================================================================
 * View IDs
 * ========================================================================= */
typedef enum {
    BleScanViewMainMenu = 0,
    BleScanViewScan,
    BleScanViewSettings,
} BleScanView;

/* =========================================================================
 * Settings — stored in app, applied on each scan start
 * ========================================================================= */
typedef enum {
    BleSortByRssi = 0,
    BleSortByTime,
    BleSortByMac,
    BleSortCount,
} BleSortMode;

typedef struct {
    int8_t min_rssi; /* filter: only show devices with rssi >= min_rssi */
    BleSortMode sort; /* display sort order */
    bool log_sd; /* write log file to SD */
} BleScanSettings;

/* =========================================================================
 * App state
 * ========================================================================= */
typedef struct {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* main_menu;
    View* scan_view;
    VariableItemList* settings_list;
    BleScanView current_view;

    /* Services */
    Storage* storage;
    NotificationApp* notifications;

    /* BLE layer */
    BleUart* uart;
    BleScanWorker* worker;
    BleScanResults* results; /* heap-allocated, shared with worker */

    /* Refresh timer — fires every 500 ms to push results into the view model */
    FuriTimer* refresh_timer;

    /* Settings */
    BleScanSettings settings;

    /* Scan state */
    bool scanning;
} BleScanApp;

/* =========================================================================
 * Entry point
 * ========================================================================= */
int32_t ble_scanner_app(void* p);

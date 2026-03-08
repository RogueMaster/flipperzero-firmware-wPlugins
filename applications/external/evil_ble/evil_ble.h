#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/view_dispatcher.h>

#include "evil_ble_scanner.h"
#include "evil_ble_uart.h"

/* ------------------------------------------------------------------
 * View IDs registered with the ViewDispatcher
 * ------------------------------------------------------------------ */
typedef enum {
    EvilBleViewMainMenu = 0,
    EvilBleViewDeviceList,
    EvilBleViewCloneStatus,
    EvilBleViewCount, /* sentinel — not a real view */
} EvilBleView;

/* ------------------------------------------------------------------
 * Custom events sent via view_dispatcher_send_custom_event()
 * ------------------------------------------------------------------ */
typedef enum {
    EvilBleCustomEventDeviceFound = 0, /* scanner found a new device       */
    EvilBleCustomEventCloneStart = 1, /* user confirmed clone selection   */
    EvilBleCustomEventCloneStop = 2, /* user stopped clone from status   */
} EvilBleCustomEvent;

/* ------------------------------------------------------------------
 * Application state (heap-allocated — stack is only ~4 KB)
 * ------------------------------------------------------------------ */
typedef struct EvilBleApp {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    /* Views */
    Submenu* main_menu;
    Submenu* device_list;
    TextBox* clone_status;

    /* Active view tracker for navigation_callback */
    EvilBleView current_view;

    /* UART + scanner */
    EvilBleUart* uart;
    EvilBleScanner* scanner;

    /* Scanned device list (scanner manages its own internal mutex) */
    EvilBleDevice devices[EVIL_BLE_MAX_DEVICES];
    uint32_t device_count;

    /* Clone state */
    uint32_t selected_device_idx;
    bool cloning;

    /* Scratch buffer for clone status TextBox text */
    char status_buf[256];
} EvilBleApp;

/* Entry point declared here so application.fam can resolve it. */
int32_t evil_ble_app(void* p);

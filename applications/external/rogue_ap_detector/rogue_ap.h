#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>

#include "rogue_ap_worker.h"
#include "rogue_uart.h"

/* =========================================================================
 * View IDs
 * ========================================================================= */

typedef enum {
    RogueViewMainMenu = 0,
    RogueViewScan,
    RogueViewResults,
    RogueViewSettings,
} RogueView;

/* =========================================================================
 * Scan view model — updated every 500 ms from a FuriTimer.
 *
 * Kept small: the raw AP table lives in RogueApResults (heap).  The model
 * only carries what the draw callback needs to render one frame.
 * ========================================================================= */

typedef struct {
    RogueStatus status;
    char flagged_ssid[ROGUE_SSID_LEN];
    uint32_t flagged_bssid_count;
    uint32_t ap_count;
    bool scanning;
} RogueScanModel;

/* =========================================================================
 * Settings — persisted across the session only (no SD storage needed).
 * ========================================================================= */

typedef struct {
    /* Minimum RSSI (absolute dBm) to include an AP — weaker APs filtered. */
    int8_t min_rssi; /* default: -90 */
} RogueSettings;

/* =========================================================================
 * Main app state — all large objects heap-allocated.
 * ========================================================================= */

typedef struct {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* main_menu;
    View* scan_view;
    TextBox* results_box;
    FuriString* results_text;
    VariableItemList* settings_list;

    /* Services */
    NotificationApp* notifications;

    /* UART + detection layer */
    RogueUart* uart;
    RogueApResults* ap_results; /* heap-allocated, mutex inside */
    RogueApWorker* worker;

    /* Refresh timer — fires every 500 ms to push results into the view model. */
    FuriTimer* refresh_timer;

    /* Settings */
    RogueSettings settings;

    /* Navigation state */
    RogueView current_view;

    /* True once we've sent at least one notification for the current threat. */
    bool notified_this_threat;
} RogueApp;

/* =========================================================================
 * Entry point
 * ========================================================================= */

int32_t rogue_ap_app(void* p);

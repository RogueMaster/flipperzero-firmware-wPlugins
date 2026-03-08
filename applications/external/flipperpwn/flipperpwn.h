#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include "wifi_uart.h"
#include "marauder.h"

/* =========================================================================
 * Constants
 * ========================================================================= */

#define FPWN_MODULES_DIR   EXT_PATH("flipperpwn/modules")
#define FPWN_MAX_MODULES   16
#define FPWN_MAX_OPTIONS   4
#define FPWN_MAX_LINE_LEN  512
#define FPWN_OPT_NAME_LEN  32
#define FPWN_OPT_VALUE_LEN 64
#define FPWN_OPT_DESC_LEN  32
#define FPWN_NAME_LEN      64
#define FPWN_DESC_LEN      128
#define FPWN_PATH_LEN      128

/* =========================================================================
 * Enums
 * ========================================================================= */

typedef enum {
    FPwnViewMainMenu,
    FPwnViewCategoryMenu,
    FPwnViewModuleList,
    FPwnViewModuleInfo,
    FPwnViewOptions,
    FPwnViewOptionEdit,
    FPwnViewExecute,
    FPwnViewWifiMenu,
    FPwnViewWifiScan,
    FPwnViewWifiPassword,
    FPwnViewPingScan,
    FPwnViewPortScan,
    FPwnViewWifiStatus,
} FPwnView;

typedef enum {
    FPwnCategoryRecon,
    FPwnCategoryCredential,
    FPwnCategoryExploit,
    FPwnCategoryPost,
    FPwnCategoryCount,
} FPwnCategory;

typedef enum {
    FPwnPlatformWindows = (1 << 0),
    FPwnPlatformMac = (1 << 1),
    FPwnPlatformLinux = (1 << 2),
    FPwnPlatformAll = 0x07,
} FPwnPlatform;

typedef enum {
    FPwnOSUnknown,
    FPwnOSWindows,
    FPwnOSMac,
    FPwnOSLinux,
} FPwnOS;

/* =========================================================================
 * Data types
 * ========================================================================= */

typedef struct {
    char name[FPWN_OPT_NAME_LEN];
    char value[FPWN_OPT_VALUE_LEN];
    char description[FPWN_OPT_DESC_LEN];
} FPwnOption;

/* Lightweight module metadata — loaded from scanning .fpwn headers */
typedef struct {
    char name[FPWN_NAME_LEN];
    char description[FPWN_DESC_LEN];
    FPwnCategory category;
    uint8_t platforms; /* bitmask of FPwnPlatform */
    char file_path[FPWN_PATH_LEN];

    /* Options (populated when module is selected) */
    FPwnOption options[FPWN_MAX_OPTIONS];
    uint8_t option_count;
    bool options_loaded;
} FPwnModule;

/* Execution status passed to the execute view */
typedef struct {
    char status[128];
    uint32_t lines_done;
    uint32_t lines_total;
    bool finished;
    bool error;
} FPwnExecModel;

/* Main app state */
typedef struct {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* main_menu;
    Submenu* category_menu;
    Submenu* module_list;
    Widget* module_info;
    VariableItemList* options_list;
    TextInput* option_edit_input;
    char option_edit_buf[FPWN_OPT_VALUE_LEN];
    uint8_t editing_option_index;
    View* execute_view;

    /* Services */
    Storage* storage;
    NotificationApp* notifications;

    /* Module catalog — heap-allocated to avoid ~44 KB in the main struct */
    FPwnModule* modules;
    uint32_t module_count;

    /* State */
    FPwnCategory current_category;
    int32_t selected_module_index;
    FPwnOS detected_os;
    FPwnOS manual_os; /* 0 = auto-detect */

    /* Execution */
    FuriThread* exec_thread;
    bool abort_requested;
    FuriMutex* mutex;

    /* WiFi Dev Board */
    FPwnWifiUart* wifi_uart;
    FPwnMarauder* marauder;
    FuriTimer* wifi_scan_timer; /* polls marauder → refreshes scan view models */
    Submenu* wifi_menu;
    View* wifi_scan_view;
    TextInput* wifi_text_input;
    char wifi_text_buf[128];
    TextBox* wifi_status;
    FuriString* wifi_status_text;
    View* ping_scan_view;
    View* port_scan_view;
    uint8_t wifi_selected_ap;
    uint8_t wifi_selected_host;
} FPwnApp;

/* =========================================================================
 * Payload engine (payload_engine.c)
 * ========================================================================= */

/* Write built-in sample .fpwn files to FPWN_MODULES_DIR if none exist yet.
 * Call once at startup before fpwn_modules_scan(). */
void fpwn_modules_write_samples(FPwnApp* app);

/* Scan FPWN_MODULES_DIR for .fpwn files, populate app->modules[].
 * Only reads metadata headers (NAME, CATEGORY, etc.) — not full payloads. */
void fpwn_modules_scan(FPwnApp* app);

/* Load full module details (options, payload sections) for the given index.
 * Populates module->options[] and sets module->options_loaded. */
bool fpwn_module_load_full(FPwnApp* app, uint32_t index);

/* Execute the selected module's payload on a background thread.
 * Detects OS (or uses manual_os), selects platform section,
 * substitutes {{OPTION}} values, types HID keystrokes. */
int32_t fpwn_payload_execute_thread(void* ctx);

/* =========================================================================
 * OS detection (os_detect.c)
 * ========================================================================= */

/* Attempt to detect the host OS via USB HID timing heuristics.
 * Must be called when USB HID is active. */
FPwnOS fpwn_os_detect(void);

/* Return a human-readable name for the OS. */
const char* fpwn_os_name(FPwnOS os);

/* =========================================================================
 * App helpers (flipperpwn.c) — used across translation units
 * ========================================================================= */

/* Resolve the effective OS: manual_os wins over detected_os.
 * Used by payload_engine.c to select the correct payload section. */
FPwnOS fpwn_effective_os(const FPwnApp* app);

/* Set the active view for navigation tracking.
 * Used by wifi_views.c to update g_current_view before switching views. */
void fpwn_set_current_view(FPwnView view);

/* =========================================================================
 * WiFi views (wifi_views.c)
 * ========================================================================= */

/* Allocate and register all WiFi-related views with the view dispatcher. */
void fpwn_wifi_views_alloc(FPwnApp* app);

/* Remove WiFi views and free all WiFi resources. */
void fpwn_wifi_views_free(FPwnApp* app);

/* Populate the WiFi submenu items. */
void fpwn_wifi_menu_setup(FPwnApp* app);

/* =========================================================================
 * Custom event IDs (sent via view_dispatcher_send_custom_event)
 * Matches FPwnCustomEvent enum in flipperpwn.c — keep in sync.
 * ========================================================================= */
#define FPWN_CUSTOM_EVENT_WIFI_CONNECTED 2

/* =========================================================================
 * Entry point
 * ========================================================================= */

int32_t flipperpwn_app(void* p);

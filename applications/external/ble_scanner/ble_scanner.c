/**
 * ble_scanner.c — Main app for BLE Scanner FAP.
 *
 * View hierarchy
 * ~~~~~~~~~~~~~~
 *   BleScanViewMainMenu  Submenu           — root: Scan / Saved List / Settings
 *   BleScanViewScan      View (custom)     — live scrollable BLE device list
 *   BleScanViewSettings  VariableItemList  — RSSI filter, sort, SD log toggle
 *
 * Navigation
 * ~~~~~~~~~~
 *   Back from main menu → exit app.
 *   Back from scan view → stop scan, return to main menu.
 *   Back from settings  → return to main menu.
 *
 * Scan view layout (128 x 64 px):
 *   Row 0-11:  header "BLE Scanner" + status indicator
 *   Row 12:    separator
 *   Row 13-63: 5 device rows (10 px each)
 *              col 0-24:  RSSI (right-aligned, 5 chars)
 *              col 26-77: MAC  (17 chars)
 *              col 79-127: Name (truncated, 8 chars) — or "[AirTag!]"
 */

#include "ble_scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "BleScan"

/* =========================================================================
 * Scan view model
 *
 * A snapshot of the sorted, filtered device list is copied here every 500 ms
 * by the refresh timer so the draw callback never touches the worker mutex.
 * ========================================================================= */
#define SCAN_VIEW_MAX_DEVICES 64
#define SCAN_VIEW_VISIBLE     5

typedef struct {
    /* Snapshot of (filtered, sorted) device rows for the draw callback. */
    struct {
        char mac[BLE_SCANNER_MAC_LEN];
        int8_t rssi;
        char name[BLE_SCANNER_NAME_LEN];
        bool is_airtag;
    } rows[SCAN_VIEW_MAX_DEVICES];
    uint32_t count;

    uint8_t scroll_offset;
    bool scanning;
    bool no_esp32; /* UART failed to acquire — show warning */
} BleScanViewModel;

/* =========================================================================
 * Sort helpers — insertion sort (qsort not available in Flipper SDK).
 * Dataset is at most 64 devices; insertion sort is fast enough.
 * ========================================================================= */

static void sort_devices(BleScanDevice* arr, uint32_t n, BleSortMode mode) {
    if(n < 2) return;
    for(uint32_t i = 1; i < n; i++) {
        BleScanDevice key = arr[i];
        int32_t j = (int32_t)i - 1;
        while(j >= 0) {
            bool should_swap = false;
            switch(mode) {
            case BleSortByRssi:
                /* Descending: stronger (less negative) first */
                should_swap = arr[j].rssi < key.rssi;
                break;
            case BleSortByTime:
                /* Descending: most recent first */
                should_swap = arr[j].last_seen_tick < key.last_seen_tick;
                break;
            case BleSortByMac:
            default:
                /* Ascending alphabetical */
                should_swap = strcmp(arr[j].mac, key.mac) > 0;
                break;
            }
            if(!should_swap) break;
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

/* =========================================================================
 * Refresh timer callback — runs on the timer service thread.
 *
 * Takes a snapshot of the result set (under mutex), applies filter/sort,
 * then pushes it into the view model.  with_view_model is thread-safe.
 * ========================================================================= */
static void ble_refresh_timer_cb(void* ctx) {
    BleScanApp* app = (BleScanApp*)ctx;
    if(!app->results) return;

    /* --- Snapshot under mutex --- */
    /* Stack-allocate a work buffer — each BleScanDevice is ~56 bytes,
   * 64 devices = 3584 bytes.  Worker stack is 4KB; this fires on the
   * timer thread (default 1KB), so we heap-allocate the work buffer. */
    BleScanDevice* buf = malloc(BLE_SCANNER_MAX_DEVICES * sizeof(BleScanDevice));
    if(!buf) return;

    uint32_t count = 0;

    furi_mutex_acquire(app->results->mutex, FuriWaitForever);
    count = app->results->count;
    if(count > BLE_SCANNER_MAX_DEVICES) count = BLE_SCANNER_MAX_DEVICES;
    memcpy(buf, app->results->devices, count * sizeof(BleScanDevice));
    furi_mutex_release(app->results->mutex);

    /* --- Filter --- */
    uint32_t filtered = 0;
    for(uint32_t i = 0; i < count; i++) {
        if(buf[i].rssi >= app->settings.min_rssi) {
            if(filtered != i) {
                buf[filtered] = buf[i];
            }
            filtered++;
        }
    }

    /* --- Sort --- */
    sort_devices(buf, filtered, app->settings.sort);

    bool no_esp32 = !ble_uart_is_connected(app->uart);

    /* --- Push into view model --- */
    with_view_model(
        app->scan_view,
        BleScanViewModel * m,
        {
            m->count = filtered < SCAN_VIEW_MAX_DEVICES ? filtered : SCAN_VIEW_MAX_DEVICES;
            for(uint32_t i = 0; i < m->count; i++) {
                strncpy(m->rows[i].mac, buf[i].mac, BLE_SCANNER_MAC_LEN - 1);
                m->rows[i].mac[BLE_SCANNER_MAC_LEN - 1] = '\0';
                strncpy(m->rows[i].name, buf[i].name, BLE_SCANNER_NAME_LEN - 1);
                m->rows[i].name[BLE_SCANNER_NAME_LEN - 1] = '\0';
                m->rows[i].rssi = buf[i].rssi;
                m->rows[i].is_airtag = buf[i].is_airtag;
            }
            m->scanning = app->scanning;
            m->no_esp32 = no_esp32;
            /* Clamp scroll offset if device list shrunk */
            if(m->count > SCAN_VIEW_VISIBLE && m->scroll_offset > m->count - SCAN_VIEW_VISIBLE) {
                m->scroll_offset = (uint8_t)(m->count - SCAN_VIEW_VISIBLE);
            } else if(m->count <= SCAN_VIEW_VISIBLE) {
                m->scroll_offset = 0;
            }
        },
        true);

    free(buf);
}

/* =========================================================================
 * Scan view — draw callback
 * ========================================================================= */
static void ble_scan_draw(Canvas* canvas, void* model_ptr) {
    BleScanViewModel* m = (BleScanViewModel*)model_ptr;

    canvas_clear(canvas);

    /* Header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "BLE Scanner");

    canvas_set_font(canvas, FontSecondary);
    if(m->no_esp32) {
        canvas_draw_str(canvas, 74, 10, "[No ESP32]");
    } else if(m->scanning) {
        canvas_draw_str(canvas, 74, 10, "[scanning]");
    } else {
        canvas_draw_str(canvas, 80, 10, "[stopped]");
    }

    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->count == 0) {
        canvas_draw_str(canvas, 6, 38, m->scanning ? "Scanning..." : "No devices");
        return;
    }

    /* Column header — tiny but readable guide */
    canvas_draw_str(canvas, 0, 21, "RSSI MAC               Name");
    canvas_draw_line(canvas, 0, 22, 127, 22);

    /* Device rows: 5 visible, 8 px row height, starting y=31 */
    const uint8_t row_h = 8;
    const uint8_t list_y = 23;

    for(uint8_t row = 0; row < SCAN_VIEW_VISIBLE; row++) {
        uint8_t idx = m->scroll_offset + row;
        if(idx >= m->count) break;

        int16_t ry = (int16_t)(list_y + row * row_h);

        /* RSSI — right-aligned in 4-char field at x=0..24 */
        char rssi_str[6];
        snprintf(rssi_str, sizeof(rssi_str), "%4d", (int)m->rows[idx].rssi);
        canvas_draw_str(canvas, 0, ry + 7, rssi_str);

        /* MAC — fixed 17 chars at x=26 */
        canvas_draw_str(canvas, 26, ry + 7, m->rows[idx].mac);

        /* Name / AirTag flag — right side, x=96 */
        if(m->rows[idx].is_airtag) {
            canvas_draw_str(canvas, 96, ry + 7, "!TAG!");
        } else if(m->rows[idx].name[0] != '\0') {
            /* Truncate name to 5 chars so it fits the narrow column */
            char name_buf[6];
            strncpy(name_buf, m->rows[idx].name, 5);
            name_buf[5] = '\0';
            canvas_draw_str(canvas, 96, ry + 7, name_buf);
        }
    }

    /* Scroll indicator (2px wide strip on right edge) */
    if(m->count > SCAN_VIEW_VISIBLE) {
        uint8_t total_h = (uint8_t)(64 - list_y);
        uint8_t bar_h = (uint8_t)((SCAN_VIEW_VISIBLE * (uint32_t)total_h) / m->count);
        if(bar_h < 3) bar_h = 3;
        uint8_t bar_y = (uint8_t)(list_y + (m->scroll_offset * (uint32_t)total_h) / m->count);
        canvas_draw_box(canvas, 126, bar_y, 2, bar_h);
    }
}

/* =========================================================================
 * Scan view — input callback
 * ========================================================================= */
static bool ble_scan_input(InputEvent* event, void* ctx) {
    BleScanApp* app = (BleScanApp*)ctx;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        with_view_model(
            app->scan_view,
            BleScanViewModel * m,
            {
                if(event->key == InputKeyUp) {
                    if(m->scroll_offset > 0) m->scroll_offset--;
                } else {
                    uint8_t max_scroll =
                        m->count > SCAN_VIEW_VISIBLE ? (uint8_t)(m->count - SCAN_VIEW_VISIBLE) : 0;
                    if(m->scroll_offset < max_scroll) m->scroll_offset++;
                }
            },
            true);
        consumed = true;
    }

    /* Back key is NOT consumed here — the navigation callback handles it. */
    return consumed;
}

/* =========================================================================
 * Navigation callback
 * ========================================================================= */
static bool ble_navigation_callback(void* ctx) {
    BleScanApp* app = (BleScanApp*)ctx;

    switch(app->current_view) {
    case BleScanViewScan:
        /* Stop scan, return to main menu */
        if(app->scanning && app->worker) {
            ble_scan_worker_stop(app->worker);
            app->scanning = false;
        }
        app->current_view = BleScanViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, BleScanViewMainMenu);
        return true;

    case BleScanViewSettings:
        app->current_view = BleScanViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, BleScanViewMainMenu);
        return true;

    case BleScanViewMainMenu:
    default:
        view_dispatcher_stop(app->view_dispatcher);
        return false;
    }
}

/* =========================================================================
 * Main menu
 * ========================================================================= */
typedef enum {
    MainMenuScan = 0,
    MainMenuSettings,
} BleScanMainMenuItem;

static void ble_main_menu_callback(void* ctx, uint32_t index) {
    BleScanApp* app = (BleScanApp*)ctx;

    switch((BleScanMainMenuItem)index) {
    case MainMenuScan: {
        /* Reset scroll and start scan */
        with_view_model(
            app->scan_view,
            BleScanViewModel * m,
            {
                m->scroll_offset = 0;
                m->scanning = true;
                m->no_esp32 = false;
            },
            false);

        /* Clear stale results */
        furi_mutex_acquire(app->results->mutex, FuriWaitForever);
        app->results->count = 0;
        memset(app->results->devices, 0, sizeof(app->results->devices));
        furi_mutex_release(app->results->mutex);

        /* Recreate worker with current settings (log_sd may have changed) */
        if(app->worker) {
            ble_scan_worker_free(app->worker);
        }
        app->worker = ble_scan_worker_alloc(app->uart, app->results, app->settings.log_sd);

        ble_scan_worker_start(app->worker);
        app->scanning = true;

        app->current_view = BleScanViewScan;
        view_dispatcher_switch_to_view(app->view_dispatcher, BleScanViewScan);
        break;
    }

    case MainMenuSettings:
        app->current_view = BleScanViewSettings;
        view_dispatcher_switch_to_view(app->view_dispatcher, BleScanViewSettings);
        break;
    }
}

/* =========================================================================
 * Settings — VariableItemList
 *
 * Item 0: Min RSSI filter  — values: Any / -80 / -60 / -40
 * Item 1: Sort             — values: RSSI / Time / MAC
 * Item 2: Log to SD        — values: Off / On
 * ========================================================================= */

static const int8_t RSSI_OPTIONS[] = {-100, -80, -60, -40};
static const char* const RSSI_LABELS[] = {"Any", "-80", "-60", "-40"};
#define RSSI_OPTIONS_COUNT 4

static const char* const SORT_LABELS[] = {"RSSI", "Time", "MAC"};
static const char* const LOG_LABELS[] = {"Off", "On"};

static void settings_rssi_changed(VariableItem* item) {
    BleScanApp* app = (BleScanApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= RSSI_OPTIONS_COUNT) idx = 0;
    app->settings.min_rssi = RSSI_OPTIONS[idx];
    variable_item_set_current_value_text(item, RSSI_LABELS[idx]);
}

static void settings_sort_changed(VariableItem* item) {
    BleScanApp* app = (BleScanApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= (uint8_t)BleSortCount) idx = 0;
    app->settings.sort = (BleSortMode)idx;
    variable_item_set_current_value_text(item, SORT_LABELS[idx]);
}

static void settings_log_changed(VariableItem* item) {
    BleScanApp* app = (BleScanApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.log_sd = (idx == 1);
    variable_item_set_current_value_text(item, LOG_LABELS[idx]);
}

static void ble_settings_build(BleScanApp* app) {
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_header(app->settings_list, "BLE Scanner Settings");

    /* Min RSSI */
    VariableItem* item;
    uint8_t rssi_idx = 0;
    for(uint8_t i = 0; i < RSSI_OPTIONS_COUNT; i++) {
        if(RSSI_OPTIONS[i] == app->settings.min_rssi) {
            rssi_idx = i;
            break;
        }
    }
    item = variable_item_list_add(
        app->settings_list, "Min RSSI", RSSI_OPTIONS_COUNT, settings_rssi_changed, app);
    variable_item_set_current_value_index(item, rssi_idx);
    variable_item_set_current_value_text(item, RSSI_LABELS[rssi_idx]);

    /* Sort */
    uint8_t sort_idx = (uint8_t)app->settings.sort;
    item = variable_item_list_add(
        app->settings_list, "Sort by", BleSortCount, settings_sort_changed, app);
    variable_item_set_current_value_index(item, sort_idx);
    variable_item_set_current_value_text(item, SORT_LABELS[sort_idx]);

    /* Log to SD */
    uint8_t log_idx = app->settings.log_sd ? 1 : 0;
    item = variable_item_list_add(app->settings_list, "Log to SD", 2, settings_log_changed, app);
    variable_item_set_current_value_index(item, log_idx);
    variable_item_set_current_value_text(item, LOG_LABELS[log_idx]);
}

/* =========================================================================
 * App alloc / free
 * ========================================================================= */
static BleScanApp* ble_scanner_alloc(void) {
    BleScanApp* app = malloc(sizeof(BleScanApp));
    furi_assert(app);
    memset(app, 0, sizeof(BleScanApp));

    /* Default settings */
    app->settings.min_rssi = -100; /* show all */
    app->settings.sort = BleSortByRssi;
    app->settings.log_sd = false;

    /* ---- Service records ---- */
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* ---- Shared result set (heap) ---- */
    app->results = malloc(sizeof(BleScanResults));
    furi_assert(app->results);
    memset(app->results, 0, sizeof(BleScanResults));
    app->results->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(app->results->mutex);

    /* ---- UART layer ---- */
    app->uart = ble_uart_alloc();

    /* ---- View dispatcher ---- */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, ble_navigation_callback);

    /* ---- Main menu ---- */
    app->main_menu = submenu_alloc();
    submenu_set_header(app->main_menu, "BLE Scanner");
    submenu_add_item(app->main_menu, "Scan", MainMenuScan, ble_main_menu_callback, app);
    submenu_add_item(app->main_menu, "Settings", MainMenuSettings, ble_main_menu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, BleScanViewMainMenu, submenu_get_view(app->main_menu));

    /* ---- Scan view ---- */
    app->scan_view = view_alloc();
    view_set_context(app->scan_view, app);
    view_set_draw_callback(app->scan_view, ble_scan_draw);
    view_set_input_callback(app->scan_view, ble_scan_input);
    view_allocate_model(app->scan_view, ViewModelTypeLocking, sizeof(BleScanViewModel));
    with_view_model(
        app->scan_view, BleScanViewModel * m, { memset(m, 0, sizeof(BleScanViewModel)); }, false);
    view_dispatcher_add_view(app->view_dispatcher, BleScanViewScan, app->scan_view);

    /* ---- Settings view ---- */
    app->settings_list = variable_item_list_alloc();
    ble_settings_build(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        BleScanViewSettings,
        variable_item_list_get_view(app->settings_list));

    /* ---- Refresh timer (500 ms) ---- */
    app->refresh_timer = furi_timer_alloc(ble_refresh_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->refresh_timer, 500);

    app->current_view = BleScanViewMainMenu;
    return app;
}

static void ble_scanner_free(BleScanApp* app) {
    furi_assert(app);

    /* Stop timer before anything else to prevent callbacks on freed data */
    if(app->refresh_timer) {
        furi_timer_stop(app->refresh_timer);
        furi_timer_free(app->refresh_timer);
        app->refresh_timer = NULL;
    }

    /* Stop worker (sends stopscan, closes log) */
    if(app->worker) {
        ble_scan_worker_free(app->worker);
        app->worker = NULL;
    }

    /* Release UART */
    if(app->uart) {
        ble_uart_free(app->uart);
        app->uart = NULL;
    }

    /* Free result set */
    if(app->results) {
        furi_mutex_free(app->results->mutex);
        free(app->results);
        app->results = NULL;
    }

    /* Remove views before freeing their backing objects */
    view_dispatcher_remove_view(app->view_dispatcher, BleScanViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BleScanViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, BleScanViewSettings);

    view_dispatcher_free(app->view_dispatcher);

    submenu_free(app->main_menu);
    view_free(app->scan_view);
    variable_item_list_free(app->settings_list);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* =========================================================================
 * Entry point
 * ========================================================================= */
int32_t ble_scanner_app(void* p) {
    UNUSED(p);

    BleScanApp* app = ble_scanner_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, BleScanViewMainMenu);

    /* Blocks until view_dispatcher_stop() is called (user exits root menu). */
    view_dispatcher_run(app->view_dispatcher);

    ble_scanner_free(app);
    return 0;
}

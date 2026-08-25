#include "scene_wifi_scan.h"

#include "../api_caller.h"
#include "../wifi/wifi_manager.h"

#define WIFI_SCAN_REFRESH_MS 8000

static void api_caller_scene_wifi_scan_item_callback(void* context, uint32_t index) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/** No-op callback used for placeholder items ("Ricerca reti...", errors). */
static void api_caller_scene_wifi_scan_dummy_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

/** Copy the n-th newline-separated entry into `out`. */
static bool ssid_list_get_nth(const FuriString* list, uint32_t n, char* out, size_t out_size) {
    const char* cursor = furi_string_get_cstr(list);
    uint32_t current = 0;
    const char* line_start = cursor;

    while(*cursor != '\0') {
        if(*cursor == '\n') {
            if(current == n) {
                size_t len = cursor - line_start;
                if(len >= out_size) {
                    len = out_size - 1;
                }
                memcpy(out, line_start, len);
                out[len] = '\0';
                return true;
            }
            current++;
            line_start = cursor + 1;
        }
        cursor++;
    }
    return false;
}

/** Kick off a scan (keeps the current list on screen during a refresh). */
static void api_caller_scene_wifi_scan_start(AppContext* app) {
    bool had_results = !furi_string_empty(app->wifi_ssid_list);

    app->wifi_scan_in_progress = true;
    wifi_manager_scan_start(app); // Note: resets wifi_ssid_list

    if(!had_results) {
        // First scan or no results yet: show the progress placeholder
        submenu_reset(app->submenu);
        submenu_add_item(
            app->submenu,
            locale_get(app, LocKeyScanInProgress),
            0,
            api_caller_scene_wifi_scan_dummy_callback,
            app);
    } else {
        // Refresh in background: keep the list and notify via the header
        submenu_set_header(app->submenu, locale_get(app, LocKeyScanRefresh));
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewWifiScan);
}

/** Rebuild the submenu from the scan results. */
static void api_caller_scene_wifi_scan_populate(AppContext* app) {
    submenu_reset(app->submenu);

    if(furi_string_empty(app->wifi_ssid_list)) {
        submenu_set_header(app->submenu, locale_get(app, LocKeyScanNoResults));
        submenu_add_item(
            app->submenu,
            locale_get(app, LocKeyScanRetry),
            0,
            api_caller_scene_wifi_scan_dummy_callback,
            app);
        return;
    }

    submenu_set_header(app->submenu, locale_get(app, LocKeyScanFoundHeader));

    uint32_t index = 0;
    char line[64];
    while(ssid_list_get_nth(app->wifi_ssid_list, index, line, sizeof(line))) {
        submenu_add_item(app->submenu, line, index, api_caller_scene_wifi_scan_item_callback, app);
        index++;
    }
}

void api_caller_scene_wifi_scan_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    submenu_reset(app->submenu);
    api_caller_scene_wifi_scan_start(app);
}

bool api_caller_scene_wifi_scan_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    AppContext* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        uint32_t now = furi_get_tick();

        if(app->wifi_scan_in_progress) {
            if(wifi_manager_scan_poll(app, app->wifi_ssid_list)) {
                app->wifi_scan_in_progress = false;
                app->wifi_scan_done_tick = now;
                api_caller_scene_wifi_scan_populate(app);
            } else if(furi_string_empty(app->wifi_ssid_list)) {
                // Update the progress placeholder with the elapsed time
                uint32_t elapsed_s = (now - app->wifi_scan_start_tick) / 1000;
                FuriString* label = furi_string_alloc_printf(
                    "%s %lus", locale_get(app, LocKeyScanInProgress), (unsigned long)elapsed_s);
                submenu_change_item_label(app->submenu, 0, furi_string_get_cstr(label));
                furi_string_free(label);
            }
        } else if(now - app->wifi_scan_done_tick >= WIFI_SCAN_REFRESH_MS) {
            // Live refresh: scan again in background
            api_caller_scene_wifi_scan_start(app);
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(ssid_list_get_nth(app->wifi_ssid_list, event.event, app->ssid, sizeof(app->ssid))) {
            scene_manager_next_scene(app->scene_manager, ApiCallerSceneWifiConnect);
            consumed = true;
        }
    }

    return consumed;
}

void api_caller_scene_wifi_scan_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;

    app->wifi_scan_in_progress = false;
    submenu_reset(app->submenu);
}

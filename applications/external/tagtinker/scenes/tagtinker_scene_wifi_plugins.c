/*
 * WiFi Plugins
 * ============
 *
 * Top-level scene that:
 *   1. Lazy-allocates the TagTinkerWifi link and opens the UART.
 *   2. Asks the ESP for its plugin list (LIST_PLUGINS).
 *   3. Renders a submenu of plugins, plus persistent header rows for
 *      "WiFi Setup" and "Forget WiFi" so the user can manage credentials
 *      without leaving the page.
 *   4. Routes to the Run scene when a plugin is picked.
 *
 * Events from TagTinkerWifi land on the FAP main thread via
 * view_dispatcher custom events (we marshal via the message queue rather
 * than touching submenu* directly from the worker thread, which is not
 * thread-safe).
 */
#include "../tagtinker_app.h"
#include "../wifi/tagtinker_wifi.h"

#include <string.h>
#include <stdio.h>

#define EVT_PLUGIN_BASE  0x100u
#define EVT_WIFI_SETUP   0x001u
#define EVT_WIFI_FORGET  0x002u
#define EVT_WIFI_REFRESH 0x003u

#define EVT_LINK_LIST_DONE 0x200u
#define EVT_LINK_STATUS    0x201u
#define EVT_LINK_LOST      0x202u
#define EVT_LINK_HELLO     0x203u

static TagTinkerWifiPlugin* plugin_array(TagTinkerApp* app) {
    return (TagTinkerWifiPlugin*)app->wifi_plugins;
}

static void wifi_plugins_event_cb(const TtWifiEvent* e, void* user) {
    TagTinkerApp* app = user;
    switch(e->type) {
    case TtWifiEvtHello:
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_LINK_HELLO);
        break;
    case TtWifiEvtWifiStatus:
        app->wifi_link_state = (uint8_t)e->u0;
        app->wifi_rssi = (int8_t)e->i1;
        strncpy(app->wifi_ssid, e->str0 ? e->str0 : "", sizeof(app->wifi_ssid) - 1);
        strncpy(app->wifi_ip, e->str1 ? e->str1 : "", sizeof(app->wifi_ip) - 1);
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_LINK_STATUS);
        break;
    case TtWifiEvtPlugin:
        if(app->wifi_plugin_count < TT_WIFI_MAX_FAP_PLUGINS && e->plugin) {
            plugin_array(app)[app->wifi_plugin_count++] = *e->plugin;
        }
        break;
    case TtWifiEvtPluginsEnd:
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_LINK_LIST_DONE);
        break;
    case TtWifiEvtLinkLost:
        view_dispatcher_send_custom_event(app->view_dispatcher, EVT_LINK_LOST);
        break;
    default:
        break; /* progress/result/error are handled by the run scene */
    }
}

static void wifi_plugins_submenu_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/* Update only the header (status badge) without rebuilding the submenu,
 * so the periodic 2s WIFI_STATUS push doesn't kick the cursor back to
 * the top entry every time. */
static void refresh_header(TagTinkerApp* app) {
    char hdr[40];
    const char* badge = "...";
    switch(app->wifi_link_state) {
    case TT_WIFI_DISCONNECTED:
        badge = "off";
        break;
    case TT_WIFI_CONNECTING:
        badge = "...";
        break;
    case TT_WIFI_CONNECTED:
        badge = "OK";
        break;
    case TT_WIFI_AUTH_FAILED:
        badge = "auth!";
        break;
    case TT_WIFI_NO_AP:
        badge = "no AP";
        break;
    }
    snprintf(hdr, sizeof(hdr), "WiFi Plugins [%s]", badge);
    submenu_set_header(app->submenu, hdr);
}

static void rebuild_submenu(TagTinkerApp* app) {
    /* Preserve the current cursor position across rebuilds. */
    uint32_t saved = submenu_get_selected_item(app->submenu);

    submenu_reset(app->submenu);
    refresh_header(app);

    if(app->wifi_plugin_count == 0) {
        const char* placeholder = app->wifi_plugins_loading ? "Loading plugins..." :
                                                              "(no plugins yet)";
        submenu_add_item(
            app->submenu, placeholder, EVT_WIFI_REFRESH, wifi_plugins_submenu_cb, app);
    } else {
        for(uint8_t i = 0; i < app->wifi_plugin_count; i++) {
            const TagTinkerWifiPlugin* p = &plugin_array(app)[i];
            submenu_add_item(
                app->submenu, p->name, EVT_PLUGIN_BASE + i, wifi_plugins_submenu_cb, app);
        }
    }
    submenu_add_item(app->submenu, "WiFi Setup", EVT_WIFI_SETUP, wifi_plugins_submenu_cb, app);
    submenu_add_item(app->submenu, "Forget WiFi", EVT_WIFI_FORGET, wifi_plugins_submenu_cb, app);
    submenu_add_item(
        app->submenu, "Refresh Plugins", EVT_WIFI_REFRESH, wifi_plugins_submenu_cb, app);

    submenu_set_selected_item(app->submenu, saved);
}

void tagtinker_scene_wifi_plugins_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    /* Lazy-allocate the link + plugin cache the first time we enter.
     * The cache is the dominant heap cost of the WiFi flow (~1.9 KB per
     * slot), so capping at TT_WIFI_MAX_FAP_PLUGINS keeps the IR TX
     * pipeline that follows a plugin run well-fed on heap. */
    if(!app->wifi) {
        const size_t bytes = sizeof(TagTinkerWifiPlugin) * TT_WIFI_MAX_FAP_PLUGINS;
        app->wifi_plugins = malloc(bytes);
        memset(app->wifi_plugins, 0, bytes);
        app->wifi = tagtinker_wifi_alloc(wifi_plugins_event_cb, app);
    }
    if(!tagtinker_wifi_open((TagTinkerWifi*)app->wifi)) {
        /* UART couldn't be acquired - rare unless another app holds it. */
        app->wifi_link_state = TT_WIFI_DISCONNECTED;
    }

    /* Always re-query plugins on entry; the ESP may have been re-flashed. */
    app->wifi_plugin_count = 0;
    app->wifi_plugins_loading = true;
    rebuild_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewSubmenu);

    tagtinker_wifi_query_status((TagTinkerWifi*)app->wifi);
    tagtinker_wifi_list_plugins((TagTinkerWifi*)app->wifi);
}

bool tagtinker_scene_wifi_plugins_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == EVT_WIFI_SETUP) {
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneWifiSetup);
        return true;
    }
    if(event.event == EVT_WIFI_FORGET) {
        tagtinker_wifi_forget((TagTinkerWifi*)app->wifi);
        app->wifi_link_state = TT_WIFI_DISCONNECTED;
        rebuild_submenu(app);
        return true;
    }
    if(event.event == EVT_WIFI_REFRESH) {
        app->wifi_plugin_count = 0;
        app->wifi_plugins_loading = true;
        rebuild_submenu(app);
        tagtinker_wifi_list_plugins((TagTinkerWifi*)app->wifi);
        return true;
    }
    if(event.event == EVT_LINK_STATUS) {
        /* Lightweight: only refresh the status badge, keep the cursor put. */
        refresh_header(app);
        return true;
    }
    if(event.event == EVT_LINK_LIST_DONE || event.event == EVT_LINK_HELLO ||
       event.event == EVT_LINK_LOST) {
        app->wifi_plugins_loading = false;
        rebuild_submenu(app);
        return true;
    }
    if(event.event >= EVT_PLUGIN_BASE && event.event < EVT_PLUGIN_BASE + TT_WIFI_MAX_FAP_PLUGINS) {
        uint8_t idx = (uint8_t)(event.event - EVT_PLUGIN_BASE);
        if(idx < app->wifi_plugin_count) {
            app->wifi_selected_plugin = (int8_t)idx;
            scene_manager_next_scene(app->scene_manager, TagTinkerSceneWifiRun);
            return true;
        }
    }
    return false;
}

void tagtinker_scene_wifi_plugins_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    submenu_reset(app->submenu);
    /* Keep the UART open while we stay inside the WiFi flow. The link is
     * closed in the app's free path or when leaving the WiFi area entirely
     * (the run scene calls back into us, so don't close on every exit). */
}

#include "../hotspot_arcade_i.h"
#include "../helpers/ha_storage.h"
#include "../helpers/ha_session.h"

typedef enum {
    MenuStartOrDash,
    MenuSelectGame,
    MenuLeaderboard,
    MenuConsole,
    MenuStop,
    MenuSsid,
    MenuFlashFirmware,
    MenuSettings,
    MenuAbout,
} MainMenuIndex;

static void ha_menu_cb(void* context, uint32_t index) {
    HotspotArcadeApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void ha_show_message(HotspotArcadeApp* app, const char* header, const char* text) {
    DialogMessage* m = dialog_message_alloc();
    if(header) dialog_message_set_header(m, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(m, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, m);
    dialog_message_free(m);
}

static void ha_menu_build(HotspotArcadeApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->session_active ? "Arcade  [ON]" : "Hotspot Arcade");

    if(app->session_active) {
        submenu_add_item(app->submenu, "Session Dashboard", MenuStartOrDash, ha_menu_cb, app);
        submenu_add_item(app->submenu, "Select Game", MenuSelectGame, ha_menu_cb, app);
        submenu_add_item(app->submenu, "Leaderboard", MenuLeaderboard, ha_menu_cb, app);
        submenu_add_item(app->submenu, "Console", MenuConsole, ha_menu_cb, app);
        submenu_add_item(app->submenu, "Stop Session", MenuStop, ha_menu_cb, app);
    } else {
        submenu_add_item(app->submenu, "Start Session", MenuStartOrDash, ha_menu_cb, app);
    }

    FuriString* label = furi_string_alloc();
    furi_string_printf(label, "SSID: %s", furi_string_get_cstr(app->ssid));
    submenu_add_item(app->submenu, furi_string_get_cstr(label), MenuSsid, ha_menu_cb, app);
    furi_string_free(label);

    if(!app->session_active)
        submenu_add_item(app->submenu, "Install Firmware", MenuFlashFirmware, ha_menu_cb, app);
    submenu_add_item(app->submenu, "Settings", MenuSettings, ha_menu_cb, app);
    submenu_add_item(app->submenu, "About", MenuAbout, ha_menu_cb, app);

    app->menu_shows_active = app->session_active;
}

void hotspot_arcade_scene_main_menu_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    ha_menu_build(app);
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, HaSceneMainMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, HaViewSubmenu);
}

bool hotspot_arcade_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    HotspotArcadeApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == HaEventRefreshView) {
        if(app->session_active != app->menu_shows_active) {
            ha_menu_build(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HaViewSubmenu);
        }
        return true;
    }

    scene_manager_set_scene_state(app->scene_manager, HaSceneMainMenu, event.event);

    switch(event.event) {
    case MenuStartOrDash:
        if(!app->session_active && app->asset_count == 0 && !ha_storage_load_manifest(app)) {
            ha_show_message(app, "No web bundle", "Deploy web/dist to\nthe SD card first.");
            view_dispatcher_switch_to_view(app->view_dispatcher, HaViewSubmenu);
        } else {
            scene_manager_next_scene(app->scene_manager, HaSceneLobby);
        }
        return true;
    case MenuSelectGame:
        scene_manager_next_scene(app->scene_manager, HaSceneGameSelect);
        return true;
    case MenuLeaderboard:
        scene_manager_next_scene(app->scene_manager, HaSceneLeaderboard);
        return true;
    case MenuConsole:
        scene_manager_next_scene(app->scene_manager, HaSceneTextView);
        return true;
    case MenuStop:
        ha_session_stop(app);
        ha_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, HaViewSubmenu);
        return true;
    case MenuSsid:
        scene_manager_next_scene(app->scene_manager, HaSceneSsidInput);
        return true;
    case MenuFlashFirmware:
        // Pick which board to flash; the picker sets the manifest and pushes the
        // flasher, then pops back here. Firmware ships bundled per board, no file picker.
        scene_manager_next_scene(app->scene_manager, HaSceneBoardSelect);
        return true;
    case MenuSettings:
        scene_manager_next_scene(app->scene_manager, HaSceneSettings);
        return true;
    case MenuAbout:
        ha_show_message(
            app,
            "Hotspot Arcade",
            "Offline party games\nover an ESP32 hotspot.\nJoin, then play.");
        view_dispatcher_switch_to_view(app->view_dispatcher, HaViewSubmenu);
        return true;
    default:
        return false;
    }
}

void hotspot_arcade_scene_main_menu_on_exit(void* context) {
    HotspotArcadeApp* app = context;
    submenu_reset(app->submenu);
}

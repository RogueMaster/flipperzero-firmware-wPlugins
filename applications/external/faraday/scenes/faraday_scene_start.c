#include "../faraday_i.h"

/* The intro plays for ~1.7 s (17 ticks at the 100 ms tick) then hands off to
 * the menu; any key skips it. It lives inside the root scene rather than on the
 * scene stack, so returning to the menu from a test never replays it and Back
 * from the menu still exits the app cleanly. */
#define FDY_SPLASH_TICKS 17

typedef enum {
    StartIndexSubGhz,
    StartIndexNfc,
    StartIndexHunt,
    StartIndexResults,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void faraday_scene_start_submenu_cb(void* context, uint32_t index) {
    FaradayApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void faraday_scene_start_show_menu(FaradayApp* app) {
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Faraday");
    submenu_add_item(
        submenu, "Test Sub-GHz (key fob)", StartIndexSubGhz, faraday_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Test NFC (card)", StartIndexNfc, faraday_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Leak hunt (Sub-GHz)", StartIndexHunt, faraday_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Saved results", StartIndexResults, faraday_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, faraday_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, faraday_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, FaradaySceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewSubmenu);
}

static void faraday_scene_start_skip_splash(void* context) {
    FaradayApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FaradayCustomEventOk);
}

void faraday_scene_start_on_enter(void* context) {
    FaradayApp* app = context;

    if(!app->splash_done) {
        app->splash_ticks = 0;
        splash_view_set_progress(app->splash_view, 0);
        splash_view_set_skip_callback(app->splash_view, faraday_scene_start_skip_splash, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewSplash);
    } else {
        faraday_scene_start_show_menu(app);
    }
}

bool faraday_scene_start_on_event(void* context, SceneManagerEvent event) {
    FaradayApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        /* Drive the intro while it is up. */
        if(!app->splash_done) {
            app->splash_ticks++;
            uint8_t progress = (uint8_t)((app->splash_ticks * 100u) / FDY_SPLASH_TICKS);
            splash_view_set_progress(app->splash_view, progress);
            splash_view_tick(app->splash_view);
            if(app->splash_ticks >= FDY_SPLASH_TICKS) {
                app->splash_done = true;
                faraday_scene_start_show_menu(app);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        /* A key press during the intro (FaradayCustomEventOk) skips to the menu;
         * the same event id never collides with a submenu index because the
         * submenu is not on screen while the splash is. */
        if(!app->splash_done && event.event == FaradayCustomEventOk) {
            app->splash_done = true;
            faraday_scene_start_show_menu(app);
            return true;
        }

        scene_manager_set_scene_state(app->scene_manager, FaradaySceneStart, event.event);
        switch(event.event) {
        case StartIndexSubGhz:
            scene_manager_next_scene(app->scene_manager, FaradaySceneSubGhz);
            consumed = true;
            break;
        case StartIndexNfc:
            scene_manager_next_scene(app->scene_manager, FaradaySceneNfc);
            consumed = true;
            break;
        case StartIndexHunt:
            scene_manager_next_scene(app->scene_manager, FaradaySceneHunt);
            consumed = true;
            break;
        case StartIndexResults:
            scene_manager_next_scene(app->scene_manager, FaradaySceneResults);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, FaradaySceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, FaradaySceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void faraday_scene_start_on_exit(void* context) {
    FaradayApp* app = context;
    submenu_reset(app->submenu);
}

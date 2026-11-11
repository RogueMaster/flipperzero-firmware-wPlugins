#include "../hamcram.h"

enum {
    StartIndexSettings = HAMCRAM_MAX_POOLS,
    StartIndexAbout,
};

void hamcram_scene_start_on_enter(void* context) {
    HamCram* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Ham Cram");
    for(uint8_t i = 0; i < app->pool_count; i++) {
        const HamCramPoolEntry* entry = &app->pools[i];
        bool expired = hamcram_pool_expired(app, entry);
        furi_string_printf(app->text, "%s %s", entry->name, expired ? "(expired)" : entry->years);
        submenu_add_item(
            submenu, furi_string_get_cstr(app->text), i, hamcram_submenu_callback, app);
    }
    submenu_add_item(submenu, "Settings", StartIndexSettings, hamcram_submenu_callback, app);
    submenu_add_item(submenu, "About", StartIndexAbout, hamcram_submenu_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, HamCramSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewSubmenu);

    /* Say so, once, if the question pools did not make it to the SD card */
    if(app->pool_count == 0 && !app->warned_no_pools) {
        app->warned_no_pools = true;
        hamcram_show_message(
            app, "No question pools", "Reinstall Ham Cram to\nrestore its pool files.");
    }
}

bool hamcram_scene_start_on_event(void* context, SceneManagerEvent event) {
    HamCram* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, HamCramSceneStart, event.event);
    if(event.event == StartIndexSettings) {
        scene_manager_next_scene(app->scene_manager, HamCramSceneSettings);
    } else if(event.event == StartIndexAbout) {
        scene_manager_next_scene(app->scene_manager, HamCramSceneAbout);
    } else if(event.event < app->pool_count) {
        if(hamcram_open_pool(app, event.event)) {
            scene_manager_set_scene_state(app->scene_manager, HamCramScenePool, 0);
            scene_manager_next_scene(app->scene_manager, HamCramScenePool);
        } else {
            hamcram_show_message(app, "Can't open pool", "The pool file is missing\nor damaged.");
        }
    }
    return true;
}

void hamcram_scene_start_on_exit(void* context) {
    HamCram* app = context;
    submenu_reset(app->submenu);
}

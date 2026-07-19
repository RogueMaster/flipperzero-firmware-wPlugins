#include "../gauntlet_i.h"

static void gauntlet_scene_browse_submenu_cb(void* context, uint32_t index) {
    GauntletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void gauntlet_scene_browse_on_enter(void* context) {
    GauntletApp* app = context;
    Submenu* submenu = app->submenu;
    CtfPack* pack = app->pack;

    submenu_reset(submenu);
    submenu_set_header(submenu, pack->title);

    for(uint8_t i = 0; i < pack->count; i++) {
        CtfChallenge* c = &pack->challenges[i];
        bool solved = ctf_progress_is_solved(app->progress, c->id);
        /* sized for the worst case: mark + full title + tag + 5-digit points */
        char label[80];
        /* "* Title  RFID 120" - leading star marks a solved challenge */
        snprintf(
            label,
            sizeof(label),
            "%s%s  %s %u",
            solved ? "* " : "",
            c->title,
            ctf_category_tag(c->category),
            c->points);
        submenu_add_item(submenu, label, i, gauntlet_scene_browse_submenu_cb, app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, GauntletSceneBrowse));
    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewSubmenu);
}

bool gauntlet_scene_browse_on_event(void* context, SceneManagerEvent event) {
    GauntletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t idx = event.event;
        if(idx < app->pack->count) {
            scene_manager_set_scene_state(app->scene_manager, GauntletSceneBrowse, idx);
            app->sel_challenge = (uint8_t)idx;
            app->hint_shown = false;
            app->hint_used = false;
            scene_manager_next_scene(app->scene_manager, GauntletSceneChallenge);
            consumed = true;
        }
    }
    return consumed;
}

void gauntlet_scene_browse_on_exit(void* context) {
    GauntletApp* app = context;
    submenu_reset(app->submenu);
}

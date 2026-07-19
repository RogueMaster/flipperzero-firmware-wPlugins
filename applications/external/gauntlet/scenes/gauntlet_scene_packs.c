#include "../gauntlet_i.h"

static void gauntlet_scene_packs_submenu_cb(void* context, uint32_t index) {
    GauntletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void show_empty_state(GauntletApp* app) {
    Widget* widget = app->widget;
    widget_reset(widget);
    FuriString* s = furi_string_alloc();
    furi_string_cat_str(s, "\e#No challenge packs\n\n");
    furi_string_cat_str(
        s,
        "Drop .pack files here on the\n"
        "SD card:\n\n"
        "  apps_data/gauntlet/packs/\n\n"
        "A starter pack is normally\n"
        "written for you on first run.\n"
        "See the README for the pack\n"
        "format and sample challenges.\n");
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    furi_string_free(s);
    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewWidget);
}

void gauntlet_scene_packs_on_enter(void* context) {
    GauntletApp* app = context;

    ctf_store_ensure_starter(app->storage);
    app->pack_count = ctf_store_scan(app->storage, app->packs, CTF_MAX_PACKS);

    if(app->pack_count == 0) {
        show_empty_state(app);
        return;
    }

    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Challenge Packs");
    for(uint8_t i = 0; i < app->pack_count; i++) {
        char label[64]; /* full pack title + " (NNN)" */
        snprintf(
            label, sizeof(label), "%s  (%u)", app->packs[i].title, app->packs[i].count);
        submenu_add_item(submenu, label, i, gauntlet_scene_packs_submenu_cb, app);
    }
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, GauntletScenePacks));
    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewSubmenu);
}

bool gauntlet_scene_packs_on_event(void* context, SceneManagerEvent event) {
    GauntletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t idx = event.event;
        if(idx < app->pack_count) {
            scene_manager_set_scene_state(app->scene_manager, GauntletScenePacks, idx);
            app->sel_pack = (uint8_t)idx;
            if(ctf_store_load_pack(app->storage, app->packs[idx].filename, app->pack)) {
                scene_manager_next_scene(app->scene_manager, GauntletSceneBrowse);
            } else {
                gauntlet_notify_wrong(app);
            }
            consumed = true;
        }
    }
    return consumed;
}

void gauntlet_scene_packs_on_exit(void* context) {
    GauntletApp* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}

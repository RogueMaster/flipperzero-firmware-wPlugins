#include "src/core/cfs_app_i.h"

// The color list shows the first N table entries (N per the settings palette),
// then a trailing "Custom..." item at index == count that opens byte-input.

void cfs_scene_write_color_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Color");
    size_t count = cfs_palette_count(app->settings.color_palette);
    for(uint32_t i = 0; i < count; i++) {
        submenu_add_item(submenu, cfs_color_table[i].name, i, cfs_submenu_send_index, app);
    }
    submenu_add_item(submenu, "Custom...", count, cfs_submenu_send_index, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_write_color_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    size_t count = cfs_palette_count(app->settings.color_palette);
    if(event.event > count) return false;

    if(event.event == count) {
        // "Custom..." — seed byte-input with the current pending color.
        app->color_bytes[0] = app->pending.color_r;
        app->color_bytes[1] = app->pending.color_g;
        app->color_bytes[2] = app->pending.color_b;
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteColorCustom);
    } else {
        const CfsColorPreset* c = &cfs_color_table[event.event];
        app->pending.color_r = c->r;
        app->pending.color_g = c->g;
        app->pending.color_b = c->b;
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteWeight);
    }
    return true;
}

void cfs_scene_write_color_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}

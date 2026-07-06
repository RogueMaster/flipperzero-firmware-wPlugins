#include "src/core/cfs_app_i.h"

// The material picker lists the authoritative catalog filtered by the selected
// brand (from app->pending.vendor). The submenu item id is the catalog index, so
// selecting an entry sets filament_id = "1" + base_id — the correct brand-aware
// id. (The old flat PLA/PETG/... list was brand-blind and collapsed every PLA
// to Creality Hyper PLA regardless of the actual brand.)

void cfs_scene_write_material_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Material");

    CfsBrand brand = cfs_vendor_to_brand(app->pending.vendor);
    // The catalog entry the current filament_id resolves to, to preselect it.
    const CfsFilamentInfo* current = cfs_filament_lookup(app->pending.filament_id);

    uint32_t position = 0;
    for(size_t i = 0; i < cfs_catalog_count(); i++) {
        const CfsFilamentInfo* e = cfs_catalog_entry(i);
        if(e->brand != brand) continue;
        submenu_add_item(submenu, e->name, i, cfs_submenu_send_index, app);
        if(e == current) submenu_set_selected_item(submenu, position);
        position++;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_write_material_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event < cfs_catalog_count()) {
        const CfsFilamentInfo* e = cfs_catalog_entry(event.event);
        // Tag filament_id is a 1-char prefix + the 5-char catalog id.
        snprintf(
            app->pending.filament_id, sizeof(app->pending.filament_id), "1%s", e->base_id);
        app->pending.filament_type = cfs_filament_id_to_type(app->pending.filament_id);
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteColor);
        return true;
    }
    return false;
}

void cfs_scene_write_material_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}

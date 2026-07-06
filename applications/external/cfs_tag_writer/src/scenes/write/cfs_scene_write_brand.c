#include "src/core/cfs_app_i.h"

void cfs_scene_write_brand_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Brand");
    // Only writable brands (Generic/Creality); eSUN is read/catalog-only.
    for(uint32_t i = 0; i < CFS_BRAND_WRITABLE_COUNT; i++) {
        submenu_add_item(submenu, cfs_brand_to_name(i), i, cfs_submenu_send_index, app);
    }
    submenu_set_selected_item(submenu, cfs_vendor_to_brand(app->pending.vendor));
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_write_brand_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event < CfsBrandCount) {
        strncpy(
            app->pending.vendor,
            cfs_brand_to_vendor((CfsBrand)event.event),
            sizeof(app->pending.vendor));
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteMaterial);
        return true;
    }
    return false;
}

void cfs_scene_write_brand_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}

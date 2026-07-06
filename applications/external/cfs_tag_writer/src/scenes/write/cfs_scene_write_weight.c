#include "src/core/cfs_app_i.h"

void cfs_scene_write_weight_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Spool Weight");
    for(uint32_t i = 0; i < CFS_WEIGHT_COUNT; i++) {
        submenu_add_item(submenu, cfs_weight_label(i), i, cfs_submenu_send_index, app);
    }
    submenu_set_selected_item(submenu, app->pending_weight_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_write_weight_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= CFS_WEIGHT_COUNT) return false;

    app->pending_weight_index = event.event;
    app->pending.length_m = app->settings.weight_len[event.event];

    // Finalize the serial per the configured mode (applies to new AND clone
    // writes). Keep uses the source serial when one is available (clone/edit),
    // otherwise falls back to random. Generated once here, so a 2x dual write
    // puts the SAME serial on both tags.
    switch(app->settings.serial_mode) {
    case CfsSerialPrompt:
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteSerial);
        return true;
    case CfsSerialSequential:
        snprintf(
            app->pending.serial,
            sizeof(app->pending.serial),
            "%06lu",
            (unsigned long)(app->settings.serial_counter % 1000000UL));
        break;
    case CfsSerialRandom:
        cfs_serial_random(app->pending.serial);
        break;
    case CfsSerialKeepSource:
    default:
        // Keep the source serial on clone/edit; random when writing from scratch.
        if(!app->has_source_serial) {
            cfs_serial_random(app->pending.serial);
        }
        break;
    }

    scene_manager_next_scene(app->scene_manager, CfsSceneWriteConfirm);
    return true;
}

void cfs_scene_write_weight_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}

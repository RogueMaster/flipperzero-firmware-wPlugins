#include "src/core/cfs_app_i.h"

// Templates: spools you write again and again. Picking one opens the confirm
// card with a fresh serial (per the serial setting); after a successful write
// the app comes back here, ready for the next tag.

#define TEMPLATES_ITEM_DELETE (CfsCustomEventMenuBase + CFS_TEMPLATES_MAX)

void cfs_scene_templates_on_enter(void* context) {
    CfsApp* app = context;
    app->template_count =
        cfs_storage_list(app->storage, CFS_TEMPLATE_DIR, app->template_names, CFS_TEMPLATES_MAX);

    if(app->template_count == 0) {
        Widget* widget = app->widget;
        widget_reset(widget);
        widget_add_string_element(
            widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "No templates yet");
        widget_add_text_box_element(
            widget,
            2,
            16,
            124,
            46,
            AlignCenter,
            AlignTop,
            "Write a tag, and on the confirm screen choose Save, then Template.",
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
        return;
    }

    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Templates");
    for(size_t i = 0; i < app->template_count; i++) {
        submenu_add_item(
            submenu,
            app->template_names[i],
            CfsCustomEventMenuBase + i,
            cfs_submenu_send_index,
            app);
    }
    submenu_add_item(
        submenu, "Delete a template...", TEMPLATES_ITEM_DELETE, cfs_submenu_send_index, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, CfsSceneTemplates));
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_templates_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == TEMPLATES_ITEM_DELETE) {
        scene_manager_set_scene_state(app->scene_manager, CfsSceneTemplates, event.event);
        scene_manager_next_scene(app->scene_manager, CfsSceneTemplateDelete);
        return true;
    }
    if(event.event < CfsCustomEventMenuBase ||
       event.event >= CfsCustomEventMenuBase + app->template_count) {
        return false;
    }
    scene_manager_set_scene_state(app->scene_manager, CfsSceneTemplates, event.event);

    char path[CFS_STORAGE_NAME_MAX + 48];
    cfs_storage_path_in(
        CFS_TEMPLATE_DIR,
        app->template_names[event.event - CfsCustomEventMenuBase],
        path,
        sizeof(path));
    CfsLoadedTag loaded;
    if(!cfs_storage_load(app->storage, path, &loaded)) {
        notification_message(app->notifications, &sequence_error);
        return true;
    }

    // A new write from the template: stage every write-wizard field.
    app->pending = loaded.data;
    app->has_source_serial = false;
    app->dual_tag = false;
    app->write_exact = false;
    app->pending_weight_index =
        cfs_nearest_weight_index(app->settings.weight_len, app->pending.length_m);
    if(cfs_app_assign_serial(app)) {
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteConfirm);
    } else {
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteSerial);
    }
    return true;
}

void cfs_scene_templates_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}

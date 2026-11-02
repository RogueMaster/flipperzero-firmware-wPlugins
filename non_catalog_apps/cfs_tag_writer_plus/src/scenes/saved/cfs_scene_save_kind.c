#include "src/core/cfs_app_i.h"
#include "src/data/cfs_shades.h"

// Save step 1: a saved tag (a record of this spool, with its serial) or a
// template (a spool you write again and again; each write gets a new serial).
// The invoker sets app->save_from_write before opening this scene.

enum {
    SaveKindTag = CfsCustomEventMenuBase,
    SaveKindTemplate,
};

// Filenames can't hold these on the SD card.
static void cfs_save_kind_clean_name(char* s) {
    for(; *s; s++) {
        if(strchr("\\/:*?\"<>|", *s)) *s = '_';
    }
}

static void cfs_save_kind_template_name(CfsApp* app, const CfsTagData* d) {
    char label[48];
    cfs_brand_material_label(d, label, sizeof(label));
    snprintf(
        app->save_name,
        sizeof(app->save_name),
        "%s %s %s",
        label,
        cfs_color_nearest_name(d->color_r, d->color_g, d->color_b),
        cfs_weight_label(cfs_nearest_weight_index(app->settings.weight_len, d->length_m)));
}

void cfs_scene_save_kind_on_enter(void* context) {
    CfsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Save as");
    submenu_add_item(submenu, "Saved tag", SaveKindTag, cfs_submenu_send_index, app);
    submenu_add_item(submenu, "Template", SaveKindTemplate, cfs_submenu_send_index, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewSubmenu);
}

bool cfs_scene_save_kind_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != SaveKindTag && event.event != SaveKindTemplate) return false;

    const CfsTagData* d = app->save_from_write ? &app->pending : &app->tag_data;
    app->save_as_template = event.event == SaveKindTemplate;
    if(app->save_as_template) {
        cfs_save_kind_template_name(app, d);
    } else {
        cfs_build_save_name(d, app->save_name, sizeof(app->save_name));
    }
    cfs_save_kind_clean_name(app->save_name);
    scene_manager_next_scene(app->scene_manager, CfsSceneSaveName);
    return true;
}

void cfs_scene_save_kind_on_exit(void* context) {
    CfsApp* app = context;
    submenu_reset(app->submenu);
}

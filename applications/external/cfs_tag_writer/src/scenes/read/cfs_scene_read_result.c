#include "src/core/cfs_app_i.h"

static void cfs_result_button_callback(GuiButtonType result, InputType type, void* context) {
    CfsApp* app = context;
    if(type != InputTypeShort) return;
    switch(result) {
    case GuiButtonTypeLeft:
        view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonSave);
        break;
    case GuiButtonTypeCenter:
        view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonWrite);
        break;
    case GuiButtonTypeRight:
        view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonDetails);
        break;
    default:
        break;
    }
}

// Blank-tag screen: the single Right button re-runs a live read.
static void cfs_result_blank_callback(GuiButtonType result, InputType type, void* context) {
    CfsApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeRight)
        view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonReread);
}

void cfs_scene_read_result_on_enter(void* context) {
    CfsApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    if(app->is_blank) {
        widget_add_string_element(widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Blank Tag");
        widget_add_string_element(
            widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "No CFS data on this tag");
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Read", cfs_result_blank_callback, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
        return;
    }

    const CfsTagData* d = &app->tag_data;
    char line[40];

    // Authoritative catalog is the source of truth for name + brand; fall back
    // to the raw id / vendor code when the filament id isn't in the catalog.
    const CfsFilamentInfo* info = cfs_filament_lookup(d->filament_id);

    // Headline: catalog product name (or the legacy type name, or raw id).
    if(info) {
        widget_add_string_element(widget, 0, 1, AlignLeft, AlignTop, FontPrimary, info->name);
    } else if(d->filament_type != CfsFilamentUnknown) {
        widget_add_string_element(
            widget, 0, 1, AlignLeft, AlignTop, FontPrimary, cfs_filament_type_to_name(d->filament_type));
    } else {
        snprintf(line, sizeof(line), "ID %s", d->filament_id);
        widget_add_string_element(widget, 0, 1, AlignLeft, AlignTop, FontPrimary, line);
    }

    // Left column.
    const char* brand = cfs_resolve_brand_name(d);
    snprintf(line, sizeof(line), "Brand:%s", brand);
    widget_add_string_element(widget, 0, 15, AlignLeft, AlignTop, FontSecondary, line);
    size_t wi = cfs_nearest_weight_index(app->settings.weight_len, d->length_m);
    snprintf(line, sizeof(line), "Wt: %s", cfs_weight_label(wi));
    widget_add_string_element(widget, 0, 26, AlignLeft, AlignTop, FontSecondary, line);
    snprintf(line, sizeof(line), "Ser:%s", d->serial);
    widget_add_string_element(widget, 0, 37, AlignLeft, AlignTop, FontSecondary, line);

    // Right column.
    snprintf(line, sizeof(line), "Len:%um", d->length_m);
    widget_add_string_element(widget, 66, 15, AlignLeft, AlignTop, FontSecondary, line);
    snprintf(line, sizeof(line), "#%02X%02X%02X", d->color_r, d->color_g, d->color_b);
    widget_add_string_element(widget, 66, 26, AlignLeft, AlignTop, FontSecondary, line);
    // Abbreviated "U:" (not "UID:") -- this column only has ~62px before the
    // screen edge, and a full-width UID string would clip on longer hex runs.
    char uidstr[24] = "U:";
    cfs_format_uid_hex(app->uid, app->uid_len, uidstr + 2, sizeof(uidstr) - 2);
    widget_add_string_element(widget, 66, 37, AlignLeft, AlignTop, FontSecondary, uidstr);

    // Actions. Right "Raw" opens the details/diagnostic view (filament id,
    // vendor, date bytes, UID). Re-read a live tag with Back -> Read.
    widget_add_button_element(widget, GuiButtonTypeLeft, "Save", cfs_result_button_callback, app);
    widget_add_button_element(widget, GuiButtonTypeCenter, "Write", cfs_result_button_callback, app);
    widget_add_button_element(widget, GuiButtonTypeRight, "Raw", cfs_result_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
}

bool cfs_scene_read_result_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        if(app->result_read_only) {
            // Saved-record view: step back to the saved entry, not the menu.
            scene_manager_previous_scene(app->scene_manager);
        } else {
            // Live read: skip the intermediate Read scene, which would
            // otherwise re-start a read on re-entry.
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, CfsSceneStart);
        }
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case CfsCustomEventButtonSave:
        app->save_from_write = false;
        cfs_build_save_name(&app->tag_data, app->save_name, sizeof(app->save_name));
        scene_manager_next_scene(app->scene_manager, CfsSceneSaveName);
        return true;
    case CfsCustomEventButtonWrite:
        scene_manager_next_scene(app->scene_manager, CfsSceneTagActions);
        return true;
    case CfsCustomEventButtonDetails:
        scene_manager_next_scene(app->scene_manager, CfsSceneReadRaw);
        return true;
    case CfsCustomEventButtonReread:
        if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, CfsSceneRead)) {
            scene_manager_next_scene(app->scene_manager, CfsSceneRead);
        }
        return true;
    default:
        return false;
    }
}

void cfs_scene_read_result_on_exit(void* context) {
    CfsApp* app = context;
    widget_reset(app->widget);
}

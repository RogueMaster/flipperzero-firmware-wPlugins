#include "src/core/cfs_app_i.h"

static void cfs_confirm_button_callback(GuiButtonType result, InputType type, void* context) {
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
        view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonWriteDual);
        break;
    default:
        break;
    }
}

void cfs_scene_write_confirm_on_enter(void* context) {
    CfsApp* app = context;
    Widget* widget = app->widget;
    const CfsTagData* d = &app->pending;
    widget_reset(widget);

    char line[48];
    widget_add_string_element(widget, 0, 1, AlignLeft, AlignTop, FontPrimary, "Confirm Write");

    // Brand + material label (catalog-driven, brand-deduped). filament_type is
    // often Unknown for catalog ids, so it must not drive the display directly.
    cfs_brand_material_label(d, line, sizeof(line));
    widget_add_string_element(widget, 0, 15, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(
        line,
        sizeof(line),
        "%s (%um)  SN %s",
        cfs_weight_label(app->pending_weight_index),
        d->length_m,
        d->serial);
    widget_add_string_element(widget, 0, 26, AlignLeft, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Color #%02X%02X%02X", d->color_r, d->color_g, d->color_b);
    widget_add_string_element(widget, 0, 37, AlignLeft, AlignTop, FontSecondary, line);

    widget_add_button_element(widget, GuiButtonTypeLeft, "Save", cfs_confirm_button_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Write", cfs_confirm_button_callback, app);
    widget_add_button_element(widget, GuiButtonTypeRight, "x2", cfs_confirm_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
}

bool cfs_scene_write_confirm_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case CfsCustomEventButtonWrite:
    case CfsCustomEventButtonWriteDual:
        app->dual_tag = (event.event == CfsCustomEventButtonWriteDual);
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteProgress);
        return true;
    case CfsCustomEventButtonSave:
        app->save_from_write = true;
        cfs_build_save_name(&app->pending, app->save_name, sizeof(app->save_name));
        scene_manager_next_scene(app->scene_manager, CfsSceneSaveName);
        return true;
    default:
        return false;
    }
}

void cfs_scene_write_confirm_on_exit(void* context) {
    CfsApp* app = context;
    widget_reset(app->widget);
}

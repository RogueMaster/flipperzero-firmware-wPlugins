#include "src/core/cfs_app_i.h"
#include "src/core/cfs_ui.h"

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

    // Spool card. Right "Raw" opens the details/diagnostic view (filament id,
    // vendor, date bytes, UID). Re-read a live tag with Back -> Read.
    CfsCardModel card;
    cfs_ui_fill_card(
        &app->tag_data,
        cfs_nearest_weight_index(app->settings.weight_len, app->tag_data.length_m),
        &card);
    card.button_left = "Save";
    card.button_center = "Write";
    card.button_right = "Raw";
    cfs_screen_show_card(
        app->screen,
        &card,
        CfsCustomEventButtonSave,
        CfsCustomEventButtonWrite,
        CfsCustomEventButtonDetails);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewScreen);
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
        scene_manager_next_scene(app->scene_manager, CfsSceneSaveKind);
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
    cfs_screen_reset(app->screen);
}

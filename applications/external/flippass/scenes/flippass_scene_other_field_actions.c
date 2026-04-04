#include "flippass_scene_other_field_actions.h"

#include "../flippass.h"
#include "../flippass_db.h"
#include "flippass_scene.h"
#include "flippass_scene_send_confirm.h"
#include "flippass_scene_status.h"
#include "flippass_scene_text_view.h"

#include <string.h>

enum {
    FlipPassSceneOtherFieldActionsEventRunAction = 0x200U,
};

typedef enum {
    FlipPassOtherFieldActionTypeBluetooth = 0,
    FlipPassOtherFieldActionShow,
    FlipPassOtherFieldActionTypeUsb,
} FlipPassOtherFieldAction;

static const char* flippass_other_field_safe_text(const char* value, const char* fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static bool flippass_other_field_get_value(App* app, const char** out_value, FuriString* error) {
    return flippass_db_get_other_field_value(
        app,
        app->active_entry,
        app->pending_other_field_mask,
        app->pending_other_custom_field,
        out_value,
        error);
}

static void flippass_scene_other_field_actions_dialog_callback(
    DialogExResult result,
    void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static FlipPassEntryAction flippass_scene_other_field_actions_map_type_action(
    FlipPassOtherFieldAction action) {
    return (action == FlipPassOtherFieldActionTypeBluetooth) ? FlipPassEntryActionTypeOtherBluetooth :
                                                               FlipPassEntryActionTypeOtherUsb;
}

static FlipPassOutputTransport flippass_scene_other_field_actions_transport(
    const App* app) {
    return (app->pending_entry_action == FlipPassEntryActionTypeOtherBluetooth) ?
               FlipPassOutputTransportBluetooth :
               FlipPassOutputTransportUsb;
}

static const char* flippass_scene_other_field_actions_typing_status_title(const App* app) {
    return flippass_scene_other_field_actions_transport(app) == FlipPassOutputTransportBluetooth ?
               "Bluetooth Typing Failed" :
               "USB Typing Failed";
}

void flippass_other_field_begin_type_action(App* app, bool bluetooth, uint32_t run_event) {
    const FlipPassOtherFieldAction action =
        bluetooth ? FlipPassOtherFieldActionTypeBluetooth : FlipPassOtherFieldActionTypeUsb;

    app->other_field_action_selected_index = (uint32_t)action;
    app->pending_entry_action = flippass_scene_other_field_actions_map_type_action(action);
    flippass_entry_action_prepare_pending(app);
    flippass_progress_begin(app, "Typing Field", "Connecting", 5U);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_send_custom_event(app->view_dispatcher, run_event);
}

void flippass_other_field_show_selected_value(App* app, uint32_t return_scene) {
    const char* value = NULL;
    size_t value_len = 0U;
    FuriString* error = furi_string_alloc();

    if(!flippass_other_field_get_value(app, &value, error)) {
        flippass_scene_status_show(
            app,
            "Field Load Failed",
            furi_string_get_cstr(error),
            return_scene);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        furi_string_free(error);
        return;
    }

    value_len = (value != NULL) ? strlen(value) : 0U;
    if(app->pending_other_field_mask == KDBXEntryFieldNotes || value_len > 80U) {
        flippass_scene_text_view_show(
            app,
            app->pending_other_field_name,
            flippass_other_field_safe_text(value, "Not set"),
            return_scene);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_TextView);
    } else {
        flippass_scene_status_show(
            app,
            app->pending_other_field_name,
            flippass_other_field_safe_text(value, "Not set"),
            return_scene);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
}

void flippass_other_field_run_pending_type_action(App* app, uint32_t failure_return_scene) {
    FuriString* error = furi_string_alloc();

    if(flippass_entry_action_execute_pending(app, error)) {
        flippass_progress_update(app, "Done", "Field sent.", 100U);
        flippass_progress_reset(app);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FlipPassScene_DbEntries);
    } else {
        flippass_progress_reset(app);
        flippass_scene_status_show(
            app,
            flippass_scene_other_field_actions_typing_status_title(app),
            furi_string_get_cstr(error),
            failure_return_scene);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
}

void flippass_scene_other_field_actions_on_enter(void* context) {
    App* app = context;

    if(app->active_entry == NULL ||
       (app->pending_other_field_mask == 0U && app->pending_other_custom_field == NULL)) {
        flippass_scene_status_show(
            app,
            "No Field Selected",
            "Open an alternate field from the entry browser first.",
            FlipPassScene_DbEntries);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        return;
    }

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(
        app->dialog_ex,
        flippass_other_field_safe_text(app->pending_other_field_name, "Other Field"),
        64,
        3,
        AlignCenter,
        AlignTop);
    dialog_ex_set_text(
        app->dialog_ex,
        "BT = type, Show = view, USB = type.",
        64,
        24,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "BT");
    dialog_ex_set_center_button_text(app->dialog_ex, "Show");
    dialog_ex_set_right_button_text(app->dialog_ex, "USB");
    dialog_ex_set_result_callback(app->dialog_ex, flippass_scene_other_field_actions_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialogEx);
}

bool flippass_scene_other_field_actions_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft) {
        flippass_other_field_begin_type_action(
            app, true, FlipPassSceneOtherFieldActionsEventRunAction);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == DialogExResultCenter) {
        app->other_field_action_selected_index = FlipPassOtherFieldActionShow;
        flippass_other_field_show_selected_value(app, FlipPassScene_OtherFieldActions);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == DialogExResultRight) {
        flippass_other_field_begin_type_action(
            app, false, FlipPassSceneOtherFieldActionsEventRunAction);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == FlipPassSceneOtherFieldActionsEventRunAction) {
        flippass_other_field_run_pending_type_action(app, FlipPassScene_OtherFieldActions);
        return true;
    }

    return false;
}

void flippass_scene_other_field_actions_on_exit(void* context) {
    App* app = context;
    dialog_ex_reset(app->dialog_ex);
}

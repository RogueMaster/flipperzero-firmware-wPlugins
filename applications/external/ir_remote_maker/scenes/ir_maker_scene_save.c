#include "../ir_maker.h"

#define IR_MAKER_SAVE_EVENT_DONE 0U

static void ir_maker_scene_save_popup_callback(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, IR_MAKER_SAVE_EVENT_DONE);
}

void ir_maker_scene_save_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    /* Whole draft written in one pass (<= 32 signals), no worker thread. */
    const InfraredErrorCode error =
        ir_maker_draft_save(app->draft, app->storage, furi_string_get_cstr(app->file_path));

    popup_reset(app->popup);
    popup_set_context(app->popup, app);

    if(!INFRARED_ERROR_PRESENT(error)) {
        popup_set_header(app->popup, "Saved", 64, 20, AlignCenter, AlignTop);
        popup_set_text(
            app->popup,
            furi_string_get_cstr(app->draft->remote_name),
            64,
            38,
            AlignCenter,
            AlignTop);
        notification_message(app->notifications, &sequence_success);
        popup_set_callback(app->popup, ir_maker_scene_save_popup_callback);
        popup_set_timeout(app->popup, 1500);
        popup_enable_timeout(app->popup);
    } else {
        popup_set_header(app->popup, "Save failed", 64, 20, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Could not write file", 64, 38, AlignCenter, AlignTop);
        notification_message(app->notifications, &sequence_error);
        /* No timeout: user presses Back to return to the remote and retry. */
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewPopup);
}

bool ir_maker_scene_save_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == IR_MAKER_SAVE_EVENT_DONE) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, IrMakerSceneButtonList);
            consumed = true;
        }
    }

    return consumed;
}

void ir_maker_scene_save_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    popup_reset(app->popup);
}

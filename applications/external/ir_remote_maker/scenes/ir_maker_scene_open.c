#include "../ir_maker.h"

#define IR_MAKER_OPEN_EVENT_ERROR 0U

static char ir_maker_open_error_text[48];

static void ir_maker_scene_open_popup_callback(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, IR_MAKER_OPEN_EVENT_ERROR);
}

void ir_maker_scene_open_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, IR_MAKER_EXT, NULL);
    options.base_path = IR_MAKER_FOLDER;

    if(!dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &options)) {
        /* Cancelled: back to the start menu. */
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    ir_maker_draft_reset(app->draft);
    const InfraredErrorCode error =
        ir_maker_draft_load(app->draft, app->storage, furi_string_get_cstr(app->file_path));

    if(!INFRARED_ERROR_PRESENT(error)) {
        path_extract_filename_no_ext(
            furi_string_get_cstr(app->file_path), app->draft->remote_name);
        scene_manager_next_scene(app->scene_manager, IrMakerSceneButtonList);
        return;
    }

    /* Load failed: explain and bounce back to the start menu. */
    if(INFRARED_ERROR_CHECK(error, InfraredErrorCodeWrongFileType)) {
        snprintf(
            ir_maker_open_error_text, sizeof(ir_maker_open_error_text), "Not an IR remote file");
    } else {
        snprintf(
            ir_maker_open_error_text, sizeof(ir_maker_open_error_text), "Could not open file");
    }

    popup_reset(app->popup);
    popup_set_header(app->popup, "Open failed", 64, 20, AlignCenter, AlignTop);
    popup_set_text(app->popup, ir_maker_open_error_text, 64, 38, AlignCenter, AlignTop);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, ir_maker_scene_open_popup_callback);
    popup_set_timeout(app->popup, 1500);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewPopup);
}

bool ir_maker_scene_open_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == IR_MAKER_OPEN_EVENT_ERROR) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void ir_maker_scene_open_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    popup_reset(app->popup);
}

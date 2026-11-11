#include "../ir_maker.h"

/* Custom events distinguish "name accepted" from "error popup dismissed". */
#define IR_MAKER_NAME_EVENT_DONE       0U
#define IR_MAKER_NAME_EVENT_ERROR_BACK 100U

static void ir_maker_scene_button_name_text_callback(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, IR_MAKER_NAME_EVENT_DONE);
}

static void ir_maker_scene_button_name_popup_callback(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, IR_MAKER_NAME_EVENT_ERROR_BACK);
}

void ir_maker_scene_button_name_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    if(app->edit_mode == IrMakerEditModeNew) {
        /* Suggest a name from the staged signal, mirroring the stock IR app. */
        if(infrared_signal_is_raw(app->pending)) {
            const InfraredRawSignal* raw = infrared_signal_get_raw_signal(app->pending);
            snprintf(app->name_buf, sizeof(app->name_buf), "RAW_%zu", raw->timings_size);
        } else {
            const InfraredMessage* msg = infrared_signal_get_message(app->pending);
            snprintf(
                app->name_buf,
                sizeof(app->name_buf),
                "%.4s_%0*lX",
                infrared_get_protocol_name(msg->protocol),
                (int)ROUND_UP_TO(infrared_get_protocol_command_length(msg->protocol), 4),
                (unsigned long)msg->command);
        }
    } else {
        /* Rename/replace: start from the button's current name. */
        strncpy(
            app->name_buf,
            furi_string_get_cstr(app->draft->items[app->selected_button].name),
            sizeof(app->name_buf) - 1);
        app->name_buf[sizeof(app->name_buf) - 1] = '\0';
    }

    text_input_set_header_text(app->text_input, "Name the button");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        ir_maker_scene_button_name_text_callback,
        app,
        app->name_buf,
        sizeof(app->name_buf),
        app->edit_mode == IrMakerEditModeNew);

    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewTextInput);
}

bool ir_maker_scene_button_name_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == IR_MAKER_NAME_EVENT_DONE) {
        bool ok = true;
        const char* error_text = NULL;

        if(app->edit_mode == IrMakerEditModeRename) {
            ir_maker_draft_set_name(app->draft, app->selected_button, app->name_buf);
        } else if(app->edit_mode == IrMakerEditModeSignal) {
            if(ir_maker_draft_set_signal(app->draft, app->selected_button, app->pending)) {
                ir_maker_draft_set_name(app->draft, app->selected_button, app->name_buf);
            } else {
                ok = false;
                error_text = "Too much raw data";
            }
        } else { // IrMakerEditModeNew
            if(!ir_maker_draft_add(app->draft, app->name_buf, app->pending)) {
                ok = false;
                error_text = (app->draft->count >= IR_MAKER_MAX_BUTTONS) ? "Remote full" :
                                                                           "Too much raw data";
            }
        }

        if(ok) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, IrMakerSceneButtonList);
        } else {
            popup_reset(app->popup);
            popup_set_header(app->popup, error_text, 64, 16, AlignCenter, AlignTop);
            popup_set_text(app->popup, "Button not added", 64, 36, AlignCenter, AlignTop);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, ir_maker_scene_button_name_popup_callback);
            popup_set_timeout(app->popup, 1500);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewPopup);
        }
        consumed = true;
    } else if(event.event == IR_MAKER_NAME_EVENT_ERROR_BACK) {
        popup_reset(app->popup);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, IrMakerSceneButtonList);
        consumed = true;
    }

    return consumed;
}

void ir_maker_scene_button_name_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    text_input_reset(app->text_input);
    popup_reset(app->popup);
}

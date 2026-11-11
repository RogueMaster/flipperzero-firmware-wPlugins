#include "../ir_maker.h"

static char ir_maker_learn_result_text[64];

static void ir_maker_scene_learn_result_dialog_callback(DialogExResult result, void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void ir_maker_scene_learn_result_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    if(infrared_signal_is_raw(app->pending)) {
        const InfraredRawSignal* raw = infrared_signal_get_raw_signal(app->pending);
        snprintf(
            ir_maker_learn_result_text,
            sizeof(ir_maker_learn_result_text),
            "Raw signal\n%zu samples",
            raw->timings_size);
    } else {
        const InfraredMessage* msg = infrared_signal_get_message(app->pending);
        snprintf(
            ir_maker_learn_result_text,
            sizeof(ir_maker_learn_result_text),
            "%s\nA: %0*lX  C: %0*lX",
            infrared_get_protocol_name(msg->protocol),
            (int)ROUND_UP_TO(infrared_get_protocol_address_length(msg->protocol), 4),
            (unsigned long)msg->address,
            (int)ROUND_UP_TO(infrared_get_protocol_command_length(msg->protocol), 4),
            (unsigned long)msg->command);
    }

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Learned", 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex, ir_maker_learn_result_text, 64, 26, AlignCenter, AlignTop);
    dialog_ex_set_left_button_text(app->dialog_ex, "Retry");
    dialog_ex_set_center_button_text(app->dialog_ex, "Send");
    dialog_ex_set_right_button_text(app->dialog_ex, "Save");
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, ir_maker_scene_learn_result_dialog_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewDialogEx);
}

bool ir_maker_scene_learn_result_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DialogExResultLeft:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        case DialogExResultCenter:
            ir_maker_send_test(app->pending);
            notification_message(app->notifications, &sequence_blink_cyan_10);
            consumed = true;
            break;
        case DialogExResultRight:
            scene_manager_next_scene(app->scene_manager, IrMakerSceneButtonName);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void ir_maker_scene_learn_result_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}

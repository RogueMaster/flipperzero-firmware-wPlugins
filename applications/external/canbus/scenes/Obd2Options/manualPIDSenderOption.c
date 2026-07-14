#include "../../app_user.h"

// These arrays are used in the manual sender PID
static uint32_t can_id = 0x7DF;
static uint8_t byte_values[4] = {0, 0, 0, 0};
static uint8_t service_to_send = 0x1;
static uint8_t lenght_pid = 0x01;
static uint8_t code_to_send = 0x00;
static uint8_t count_of_bytes = 2;

// Thread
static int32_t obdii_thread_response_manual_sender_on_work(void* context);

/*
    Manual Sender Scene
*/

// Callback for the input options
void callback_manual_input_pid_options(void* context, uint32_t index) {
    App* app = context;
    app->sender_selected_item = index;

    switch(index) {
    case 0:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_input_manual_pid_option, index);
        scene_manager_next_scene(app->scene_manager, app_scene_input_manual_pid_option);

        break;

    case 1:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_input_manual_pid_option, index);
        scene_manager_next_scene(app->scene_manager, app_scene_input_manual_pid_option);

        break;

    case 3:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_input_manual_pid_option, index);
        scene_manager_next_scene(app->scene_manager, app_scene_input_manual_pid_option);

        break;

    case 5:
        scene_manager_next_scene(app->scene_manager, app_scene_response_pid_option);

    default:
        break;
    }
}

// Callback for the options
void callback_manual_pid_sender_options(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t selected_index = variable_item_list_get_selected_item_index(app->varList);
    FuriString* text = app->text;

    furi_string_reset(text);

    switch(selected_index) {
    // Service
    case 1:
        service_to_send = index;
        furi_string_cat_printf(text, "0x%x", service_to_send);
        variable_item_set_current_value_text(item, furi_string_get_cstr(text));

        break;

    // count of bytes
    case 2:
        count_of_bytes = index;
        furi_string_cat_printf(text, "%u", count_of_bytes);
        variable_item_set_current_value_text(item, furi_string_get_cstr(text));

        break;

    // Lenght
    case 4:
        lenght_pid = index;
        furi_string_cat_printf(text, "%u", lenght_pid);
        variable_item_set_current_value_text(item, furi_string_get_cstr(text));
        break;

    default:
        break;
    }
}

// Scene on enter
void app_scene_manual_sender_pid_on_enter(void* context) {
    App* app = context;
    FuriString* text = app->text;
    VariableItem* item;

    variable_item_list_reset(app->varList);

    // First item to set (CAN ID) [0]
    furi_string_reset(text);
    furi_string_cat_printf(text, "0x%lx", can_id);

    item =
        variable_item_list_add(app->varList, "CAN ID", 0, callback_manual_pid_sender_options, app);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));

    // Second Item to set (Service) [1]
    furi_string_reset(text);
    furi_string_cat_printf(text, "0x%x", service_to_send);
    item = variable_item_list_add(
        app->varList, "Service", 11, callback_manual_pid_sender_options, app);
    variable_item_set_current_value_index(item, service_to_send);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));

    // Third item COUNT OF BYTES [2]
    furi_string_reset(text);
    furi_string_cat_printf(text, "%u", count_of_bytes);
    item = variable_item_list_add(
        app->varList, "Count Bytes", 3, callback_manual_pid_sender_options, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));
    variable_item_set_current_value_index(item, count_of_bytes);

    // Fourth item [3]
    furi_string_reset(text);
    furi_string_cat_printf(text, "0x%x", code_to_send);
    item = variable_item_list_add(app->varList, "Pid", 0, callback_manual_pid_sender_options, app);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));

    // Fifth Element to set (message lenght) [4]
    furi_string_reset(text);
    furi_string_cat_printf(text, "%u", lenght_pid);

    item = variable_item_list_add(
        app->varList, "Lenght", 255, callback_manual_pid_sender_options, app);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));
    variable_item_set_current_value_index(item, lenght_pid);

    // Item to request the data [5]
    item = variable_item_list_add(
        app->varList, "Send Request", 0, callback_manual_pid_sender_options, app);
    variable_item_list_set_enter_callback(app->varList, callback_manual_input_pid_options, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

bool app_scene_manual_sender_pid_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void app_scene_manual_sender_pid_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->varList);
}

/*
    Scene to set the pid code
*/

void input_manual_pid(void* context) {
    App* app = context;
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_input_manual_pid_option);

    switch(state) {
    case 0: // can id

        can_id = byte_values[3] | (byte_values[2] << 8) | (byte_values[1] << 16) |
                 (byte_values[0] << 24);
        break;

    case 1: // service
        service_to_send = byte_values[0];
        break;

    case 3: // pid
        code_to_send = byte_values[0];
        break;

    default:
        break;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, ReturnEvent);
}

void app_scene_input_manual_set_pid_on_enter(void* context) {
    App* app = context;
    ByteInput* scene = app->input_byte_value;
    uint8_t count_bytes = 0;

    uint32_t scene_state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_input_manual_pid_option);

    switch(scene_state) {
    case 0:
        count_bytes = 4;

        byte_values[3] = can_id;
        byte_values[2] = can_id >> 8;
        byte_values[1] = can_id >> 16;
        byte_values[0] = can_id >> 24;
        break;

    case 1:
        count_bytes = 1;
        byte_values[0] = service_to_send;
        break;

    case 3:
        count_bytes = 1;
        byte_values[0] = code_to_send;

        break;

    default:
        break;
    }

    byte_input_set_result_callback(scene, input_manual_pid, NULL, app, byte_values, count_bytes);
    byte_input_set_header_text(scene, "SET VALUE");

    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

bool app_scene_input_manual_set_pid_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ReturnEvent:
            scene_manager_previous_scene(app->scene_manager);
            break;

        default:
            break;
        }
    }
    return consumed;
}

void app_scene_input_manual_set_pid_on_exit(void* context) {
    UNUSED(context);
}

/*
    Scene to show the response in the scene
*/

void app_scene_response_manual_pid_on_enter(void* context) {
    App* app = context;
    text_box_reset(app->textBox);
    app->thread =
        furi_thread_alloc_ex("ManualPID", 1024, obdii_thread_response_manual_sender_on_work, app);
    furi_thread_start(app->thread);
    text_box_set_focus(app->textBox, TextBoxFocusEnd);

    view_dispatcher_switch_to_view(app->view_dispatcher, TextBoxView);
}

bool app_scene_response_manual_pid_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    UNUSED(context);
    UNUSED(event);
    return consumed;
}

void app_scene_response_manual_pid_on_exit(void* context) {
    App* app = context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
    text_box_reset(app->textBox);
}

/*
    Scene to set if the device wasnt sent okay or the device is not connected
*/

void app_scene_obdii_warnings_on_enter(void* context) {
    App* app = context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_obdii_warning_scenes);

    if(state == 0) draw_device_no_connected(app);
    if(state == 1) draw_transmition_failure(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool app_scene_obdii_warnings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void app_scene_obdii_warnings_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

/*
    Thread to Send and Received the PID message
*/

static int32_t obdii_thread_response_manual_sender_on_work(void* context) {
    App* app = context;

    OBDII scanner;

    CANFRAME canframes[lenght_pid];

    memset(canframes, 0, sizeof(canframes));

    FuriString* text = app->text;

    furi_string_reset(text);

    scanner.bitrate = app->mcp_can->bitRate;

    bool run = pid_init(&scanner);

    if(run) {
        // Time delay added to initialize
        furi_delay_ms(500);

        furi_string_printf(text, "DEVICE CONNECTED!...\n");
        furi_string_cat_printf(
            text,
            "-> %lx %u %x %x 0 0 0 0 0\n",
            can_id,
            count_of_bytes,
            service_to_send,
            code_to_send);

        text_box_set_text(app->textBox, furi_string_get_cstr(text));

        if(pid_manual_request(
               &scanner,
               can_id,
               service_to_send,
               code_to_send,
               canframes,
               lenght_pid,
               count_of_bytes)) {
            for(uint8_t i = 0; i < lenght_pid; i++) {
                CANFRAME frame_received = canframes[i];
                if(frame_received.canId == 0x00) break;
                furi_string_cat_printf(
                    text,
                    "<-(%u) %lx %x %x %x %x %x %x %x %x\n",
                    i,
                    frame_received.canId,
                    frame_received.buffer[0],
                    frame_received.buffer[1],
                    frame_received.buffer[2],
                    frame_received.buffer[3],
                    frame_received.buffer[4],
                    frame_received.buffer[5],
                    frame_received.buffer[6],
                    frame_received.buffer[7]);
            }

            text_box_reset(app->textBox);
            text_box_set_text(app->textBox, furi_string_get_cstr(text));
        } else {
            text_box_reset(app->textBox);
            furi_string_cat_printf(text, "FAILURE TRANSMITION!!\n");
            text_box_set_text(app->textBox, furi_string_get_cstr(text));
        }

    } else {
        furi_string_printf(text, "DEVICE NOT CONNECTED!");
        text_box_set_text(app->textBox, furi_string_get_cstr(text));
    }

    pid_deinit(&scanner);

    return 0;
}

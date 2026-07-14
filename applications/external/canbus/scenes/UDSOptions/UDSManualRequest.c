#include "../../app_user.h"

static uint32_t id_request;
static uint32_t id_response;

static uint8_t id_request_array[4] = {0, 0, 0x7, 0xe0};
static uint8_t id_response_array[4] = {0, 0, 0x7, 0xe8};

static uint8_t count_of_frames = 1;
static uint8_t count_of_bytes = 1;

static uint8_t data_to_send[100] = {0};

static uint32_t selected_item = 0;

// Thread to work with
static int32_t uds_multiframe_request_thread(void* context);

/**
 * Scene for UDS MENU
 */

void callback_input_manual_sender_uds(void* context, uint32_t index) {
    App* app = context;

    selected_item = index;

    switch(index) {
    case 0:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_uds_single_frame_data_option, 0);
        scene_manager_next_scene(app->scene_manager, app_scene_uds_single_frame_data_option);
        break;

    case 1:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_uds_single_frame_data_option, 1);
        scene_manager_next_scene(app->scene_manager, app_scene_uds_single_frame_data_option);
        break;

    case 4:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_uds_single_frame_data_option, 2);
        scene_manager_next_scene(app->scene_manager, app_scene_uds_single_frame_data_option);
        break;

    case 5:
        scene_manager_next_scene(
            app->scene_manager, app_scene_uds_single_frame_request_response_option);
        break;

    default:
        break;
    }
}

void callback_single_frame_request_menu(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t selected_index = variable_item_list_get_selected_item_index(app->varList);
    FuriString* text = app->text;

    furi_string_reset(text);

    switch(selected_index) {
    case 2:
        count_of_frames = index + 1;
        variable_item_set_current_value_index(item, index);
        furi_string_cat_printf(text, "%u", count_of_frames);
        variable_item_set_current_value_text(item, furi_string_get_cstr(text));
        break;

    case 3:
        count_of_bytes = index + 1;
        variable_item_set_current_value_index(item, index);
        furi_string_cat_printf(text, "%u", count_of_bytes);
        variable_item_set_current_value_text(item, furi_string_get_cstr(text));
        break;

    default:
        break;
    }
}

void app_scene_uds_single_frame_request_sender_on_enter(void* context) {
    App* app = context;
    FuriString* text = app->text;
    VariableItem* item;

    id_request = (id_request_array[0] << 24) | (id_request_array[1] << 16) |
                 (id_request_array[2] << 8) | (id_request_array[3]);

    id_response = (id_response_array[0] << 24) | (id_response_array[1] << 16) |
                  (id_response_array[2] << 8) | (id_response_array[3]);

    // ID REQUEST  0
    item = variable_item_list_add(app->varList, "ID REQUEST", 0, NULL, app);
    variable_item_set_current_value_index(item, 0);
    furi_string_reset(text);
    furi_string_cat_printf(text, "%lx", id_request);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));

    // ID TO RESPONSE    1
    item = variable_item_list_add(app->varList, "ID RESPONSE", 0, NULL, app);
    variable_item_set_current_value_index(item, 0);
    furi_string_reset(text);
    furi_string_cat_printf(text, "%lx", id_response);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));

    // COUNT OF FRAMES TO GET   2
    item = variable_item_list_add(
        app->varList, "Frames to get", 100, callback_single_frame_request_menu, app);
    variable_item_set_current_value_index(item, count_of_frames - 1);
    furi_string_reset(text);
    furi_string_cat_printf(text, "%u", count_of_frames);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));

    // SET COUNT OF BYTES   3
    item = variable_item_list_add(
        app->varList, "Bytes", 100, callback_single_frame_request_menu, app);
    variable_item_set_current_value_index(item, count_of_bytes - 1);
    furi_string_reset(text);
    furi_string_cat_printf(text, "%u", count_of_bytes);
    variable_item_set_current_value_text(item, furi_string_get_cstr(text));

    // Set DATA     4
    item = variable_item_list_add(app->varList, "Set Data", 0, NULL, app);

    // Send de message     5
    item = variable_item_list_add(app->varList, "Send Message", 0, NULL, app);

    // Set the callback to the View
    variable_item_list_set_enter_callback(app->varList, callback_input_manual_sender_uds, app);

    variable_item_list_set_selected_item(app->varList, selected_item);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

bool app_scene_uds_single_frame_request_sender_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void app_scene_uds_single_frame_request_sender_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->varList);
}

/**
 * Scene to set the Data
 */

void input_manual_uds(void* context) {
    App* app = context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_uds_single_frame_data_option);

    switch(state) {
    case 0:

        id_request = (id_request_array[0] << 24) | (id_request_array[1] << 16) |
                     (id_request_array[2] << 8) | (id_request_array[3]);
        break;

    case 1:

        id_response = (id_response_array[0] << 24) | (id_response_array[1] << 16) |
                      (id_response_array[2] << 8) | (id_response_array[3]);
        break;

    default:
        break;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, ReturnEvent);
}

void app_scene_uds_single_frame_data_on_enter(void* context) {
    App* app = context;
    ByteInput* scene = app->input_byte_value;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_uds_single_frame_data_option);

    switch(state) {
    case 0:

        id_request_array[3] = id_request;
        id_request_array[2] = id_request >> 8;
        id_request_array[1] = id_request >> 16;
        id_request_array[0] = id_request >> 24;

        byte_input_set_result_callback(scene, input_manual_uds, NULL, app, id_request_array, 4);
        byte_input_set_header_text(scene, "SET DATA");
        break;

    case 1:

        id_response_array[3] = id_response;
        id_response_array[2] = id_response >> 8;
        id_response_array[1] = id_response >> 16;
        id_response_array[0] = id_response >> 24;

        byte_input_set_result_callback(scene, input_manual_uds, NULL, app, id_response_array, 4);
        byte_input_set_header_text(scene, "SET DATA");
        break;

    case 2:
        for(int i = count_of_bytes; i < 7; i++) {
            data_to_send[i] = 0x00;
        }
        byte_input_set_result_callback(
            scene, input_manual_uds, NULL, app, data_to_send, count_of_bytes);
        byte_input_set_header_text(scene, "SET DATA");
        break;

    default:
        break;
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

bool app_scene_uds_single_frame_data_on_event(void* context, SceneManagerEvent event) {
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

void app_scene_uds_single_frame_data_on_exit(void* context) {
    UNUSED(context);
}

/**
 * Scene for the response of the uds services
 */

void app_scene_uds_single_frame_request_response_on_enter(void* context) {
    App* app = context;
    text_box_reset(app->textBox);
    text_box_set_focus(app->textBox, TextBoxFocusEnd);

    app->thread = furi_thread_alloc_ex("ManualUDS", 10 * 1024, uds_multiframe_request_thread, app);
    furi_thread_start(app->thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, TextBoxView);
}

bool app_scene_uds_single_frame_request_response_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    UNUSED(context);
    UNUSED(event);
    return consumed;
}

void app_scene_uds_single_frame_request_response_on_exit(void* context) {
    App* app = context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
    text_box_reset(app->textBox);
}

/**
 * Thread for the manual request
 */

static int32_t uds_multiframe_request_thread(void* context) {
    App* app = context;
    FuriString* text = app->text;
    furi_string_reset(text);
    text_box_set_text(app->textBox, furi_string_get_cstr(text));

    MCP2515* CAN = app->mcp_can;

    CANFRAME* canframes_to_received = (CANFRAME*)calloc(count_of_frames, sizeof(CANFRAME));

    CANFRAME* canframes_to_send = (CANFRAME*)calloc(15, sizeof(CANFRAME));

    UDS_SERVICE* uds_service =
        uds_service_alloc(id_request, id_response, CAN->mode, CAN->clck, CAN->bitRate);

    bool run = uds_init(uds_service);

    furi_delay_ms(500);

    if(run) {
        if(uds_multi_frame_request(
               uds_service,
               data_to_send,
               count_of_bytes,
               canframes_to_send,
               count_of_frames,
               canframes_to_received)) {
            for(uint8_t i = 0; i < 15; i++) {
                if(canframes_to_send[i].canId != uds_service->id_to_send) break;

                furi_string_cat_printf(text, "->%lx  ", canframes_to_send[i].canId);
                for(uint8_t j = 0; j < canframes_to_send[i].data_lenght; j++) {
                    furi_string_cat_printf(text, "%x ", canframes_to_send[i].buffer[j]);
                }

                furi_string_cat_printf(text, "\n");
            }

            for(uint8_t i = 0; i < count_of_frames; i++) {
                if(canframes_to_received[i].canId != uds_service->id_to_received) break;
                furi_string_cat_printf(text, "<-%lx  ", canframes_to_received[i].canId);
                for(uint8_t j = 0; j < canframes_to_received[i].data_lenght; j++) {
                    furi_string_cat_printf(text, "%x ", canframes_to_received[i].buffer[j]);
                }

                furi_string_cat_printf(text, "\n");
            }

            text_box_set_text(app->textBox, furi_string_get_cstr(text));
        } else {
            text_box_set_text(app->textBox, "Transmition Failure");
        }

    } else {
        text_box_set_text(app->textBox, "Device not connected");
    }

    free(canframes_to_received);
    free(canframes_to_send);
    free_uds(uds_service);

    return 0;
}

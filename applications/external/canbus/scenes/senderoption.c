#include "../app_user.h"

typedef enum {
    SEND_OK,
    SEND_ERROR,
} sender_status;

// This part is for the time
enum {
    ONCE,
    PERIODIC
} sender_periods;

static uint8_t timing = 0;

static const char* timing_texts[] = {"ONCE", "PERIODIC", "REPEAT"};
static const char* timing_multiply[] = {"x1", "x10", "x100", "x1000", "x10000", "x100000"};

static uint8_t time = 1;
static uint8_t multiply = 3;

static uint8_t quantity_to_repeat = 10;
static uint8_t multiply_quantity = 0;

// Only solution I think for the text with
// the dynamic menu in set data to get the bytes
static const char* text_bytes[] = {
    "Byte [0]",
    "Byte [1]",
    "Byte [2]",
    "Byte [3]",
    "Byte [4]",
    "Byte [5]",
    "Byte [6]",
    "Byte [7]",
};

// Data for the can Id
uint8_t can_id[4];

// Empty Callback
void empty_input_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

/**
 * Threads
 */

static int32_t thread_to_send_once(void* context);
static int32_t thread_to_send_periodic(void* context);
static int32_t thread_to_send_repeat(void* context);

/**
 *  Scene for the Menu Sender
 */

// Option callback using button OK
void callback_input_sender_options(void* context, uint32_t index) {
    App* app = context;
    app->sender_selected_item = index;

    switch(index) {
    case 0:
        scene_manager_next_scene(app->scene_manager, app_scene_send_message_option);
        break;
    case 1:
        scene_manager_next_scene(app->scene_manager, app_scene_set_timing_option);
        break;

    case 2:
        scene_manager_next_scene(app->scene_manager, app_scene_set_data_sender_option);
        break;

    case 3:
        scene_manager_set_scene_state(app->scene_manager, app_scene_input_data_option, 0xff);
        scene_manager_next_scene(app->scene_manager, app_scene_input_data_option);

    default:
        break;
    }
}

void set_timing_menu_callback(VariableItem* item) {
    timing = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, timing_texts[timing]);
}

// To display the variable list
void default_list_for_sender_menu(App* app) {
    VariableItem* item;
    variable_item_list_reset(app->varList);

    // First Item [0]
    item = variable_item_list_add(app->varList, "SEND MESSAGE", 0, NULL, app);
    variable_item_set_current_value_index(item, 0);

    // Second Item [1]
    item = variable_item_list_add(app->varList, "Timing", 3, set_timing_menu_callback, app);
    variable_item_set_current_value_index(item, timing);
    variable_item_set_current_value_text(item, timing_texts[timing]);

    // Third Item [2]
    item = variable_item_list_add(app->varList, "DATA", 0, NULL, app);
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "0x%lx", app->frame_to_send->canId);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    // This is for the
    furi_string_reset(app->text);

    for(uint8_t i = 0; i < app->frame_to_send->data_lenght; i++) {
        if(app->frame_to_send->buffer[i] < 0x10) {
            furi_string_cat_printf(app->text, "0%x ", app->frame_to_send->buffer[i]);
        } else {
            furi_string_cat_printf(app->text, "%x ", app->frame_to_send->buffer[i]);
        }
    }

    if(app->frame_to_send->data_lenght == 0 || app->frame_to_send->req == 1) {
        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, "---");
    }

    // Fourth Item [3]
    item = variable_item_list_add(app->varList, furi_string_get_cstr(app->text), 0, NULL, app);

    variable_item_list_set_enter_callback(app->varList, callback_input_sender_options, app);

    variable_item_list_set_selected_item(app->varList, app->sender_selected_item);
}

// Menu sender Scene On enter
void app_scene_sender_on_enter(void* context) {
    App* app = context;

    default_list_for_sender_menu(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

// Menu Sender On event
bool app_scene_sender_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

// Menu Sender On exit
void app_scene_sender_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->varList);
}

/**
 * Scene to set the timming
 */

void set_timing_view(App* app);

void set_timings_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    timing = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, timing_texts[timing]);

    set_timing_view(app);
}

void set_time_sender(VariableItem* item) {
    App* app = variable_item_get_context(item);
    time = variable_item_get_current_value_index(item) + 1;

    furi_string_reset(app->text);

    furi_string_cat_printf(app->text, "%u ms", time);

    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));
}

void set_multiply_time(VariableItem* item) {
    multiply = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, timing_multiply[multiply]);
}

void set_multiply_count(VariableItem* item) {
    multiply_quantity = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, timing_multiply[multiply_quantity]);
}

void set_count_to_send(VariableItem* item) {
    App* app = variable_item_get_context(item);
    quantity_to_repeat = variable_item_get_current_value_index(item) + 1;

    furi_string_reset(app->text);

    furi_string_cat_printf(app->text, "%u", quantity_to_repeat);

    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));
}

// View to display
void set_timing_view(App* app) {
    VariableItem* item;
    variable_item_list_reset(app->varList);
    variable_item_list_set_enter_callback(app->varList, empty_input_callback, app);

    // First Item
    item = variable_item_list_add(app->varList, "Timing", 3, set_timings_callback, app);
    variable_item_set_current_value_index(item, timing);
    variable_item_set_current_value_text(item, timing_texts[timing]);

    // Second Item
    if(timing) {
        item = variable_item_list_add(app->varList, "Time", 255, set_time_sender, app);
        variable_item_set_current_value_index(item, time - 1);
        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, "%u ms", time);
        variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));
    } else {
        item = variable_item_list_add(app->varList, "After", 255, set_time_sender, app);
        variable_item_set_current_value_index(item, time - 1);
        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, "%u ms", time);
        variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));
    }

    // Third Item

    item = variable_item_list_add(app->varList, "Multiply by", 4, set_multiply_time, app);
    variable_item_set_current_value_index(item, multiply);
    variable_item_set_current_value_text(item, timing_multiply[multiply]);

    if(timing == 2) {
        item = variable_item_list_add(app->varList, "Count to send", 100, set_count_to_send, app);
        variable_item_set_current_value_index(item, quantity_to_repeat - 1);
        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, "%u", quantity_to_repeat);
        variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

        item = variable_item_list_add(app->varList, "Count Multiply", 6, set_multiply_count, app);
        variable_item_set_current_value_index(item, multiply_quantity);
        variable_item_set_current_value_text(item, timing_multiply[multiply_quantity]);
    }
}

// Menu sender Scene On enter
void app_scene_set_timing_on_enter(void* context) {
    App* app = context;

    set_timing_view(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

// Menu Sender On event
bool app_scene_set_timing_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

// Menu Sender On exit
void app_scene_set_timing_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->varList);
}

/**
 * Scene to set the timing
 */

void set_data_view(App* app);

// Go to set the option
void input_set_data(void* context, uint32_t index) {
    App* app = context;

    if(index == 0) {
        if(app->num_of_devices > 0) {
            scene_manager_next_scene(app->scene_manager, app_scene_id_list_option);
            return;
        } else {
            scene_manager_next_scene(app->scene_manager, app_scene_warning_log_sender);
            return;
        }
    }

    if(index == 1 || index > 3) {
        scene_manager_set_scene_state(app->scene_manager, app_scene_input_data_option, index);
        scene_manager_next_scene(app->scene_manager, app_scene_input_data_option);
    }
}

// Callback for the frame
void set_frame_request_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);

    app->frame_to_send->req = variable_item_get_current_value_index(item);

    set_data_view(app);
}

// Callback to set the data length
void set_data_length_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);

    app->frame_to_send->data_lenght = variable_item_get_current_value_index(item);

    set_data_view(app);
}

// View to set the Data
void set_data_view(App* app) {
    VariableItem* item;
    variable_item_list_reset(app->varList);

    variable_item_list_set_enter_callback(app->varList, input_set_data, app);

    // first item [0]
    item = variable_item_list_add(app->varList, "Choose last ID", 0, NULL, app);

    // second item [1]
    item = variable_item_list_add(app->varList, "Set Id", 0, NULL, app);
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "0x%lx", app->frame_to_send->canId);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    // third item [2]
    item =
        variable_item_list_add(app->varList, "Frame request", 2, set_frame_request_callback, app);
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%u", app->frame_to_send->req);
    variable_item_set_current_value_index(item, app->frame_to_send->req);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    if(app->frame_to_send->req) return;

    // fourth item [4]
    item = variable_item_list_add(app->varList, "Data Length", 9, set_data_length_callback, app);
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%u", app->frame_to_send->data_lenght);
    variable_item_set_current_value_index(item, app->frame_to_send->data_lenght);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    for(uint8_t i = 0; i < app->frame_to_send->data_lenght; i++) {
        item = variable_item_list_add(app->varList, text_bytes[i], 0, NULL, app);

        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, "0x");
        if(app->frame_to_send->buffer[i] < 0x10) {
            furi_string_cat_printf(app->text, "0");
        }
        furi_string_cat_printf(app->text, "%x", app->frame_to_send->buffer[i]);
        variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));
    }
}

// Menu sender Scene On enter
void app_scene_set_data_sender_on_enter(void* context) {
    App* app = context;

    set_data_view(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

// Menu Sender On event
bool app_scene_set_data_sender_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

// Menu Sender On exit
void app_scene_set_data_sender_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->varList);
}

/**
 * Scene to display the when there are not ids recorded
 */

void app_scene_warning_log_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 65, 20, AlignCenter, AlignBottom, FontPrimary, "W A R N I N G !");

    widget_add_string_multiline_element(
        app->widget,
        65,
        35,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "First go to the Sniffing option\nin the menu and get the ID's");

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool app_scene_warning_log_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void app_scene_warning_log_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

/**
 *  Scene for the id list
 */

// Callback for menus id
void menu_ids_callback(void* context, uint32_t index) {
    App* app = context;
    app->frame_to_send->canId = app->frameArray[index].canId;
    app->frame_to_send->data_lenght = app->frameArray[index].data_lenght;
    app->frame_to_send->ext = app->frameArray[index].ext;
    app->frame_to_send->req = app->frameArray[index].req;

    for(uint8_t i = 0; i < (app->frame_to_send->data_lenght); i++) {
        app->frame_to_send->buffer[i] = app->frameArray[index].buffer[i];
    }

    scene_manager_handle_custom_event(app->scene_manager, ReturnEvent);
}

void app_scene_id_list_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "CAN ADDRESS TO SEND");

    for(uint8_t i = 0; i < app->num_of_devices; i++) {
        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, "0x%lx", app->frameArray[i].canId);
        submenu_add_item(app->submenu, furi_string_get_cstr(app->text), i, menu_ids_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

bool app_scene_id_list_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}

void app_scene_id_list_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

/**
 * Scene used for the input on the menu sender
 */

void input_byte_sender_callback(void* context) {
    App* app = context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_input_data_option);

    switch(state) {
    case 1:
        app->frame_to_send->canId = (can_id[0] << 24) | (can_id[1] << 16) | (can_id[2] << 8) |
                                    (can_id[3]);
        break;

    default:
        break;
    }

    scene_manager_previous_scene(app->scene_manager);
}

void app_scene_input_data_on_enter(void* context) {
    App* app = context;
    ByteInput* scene = app->input_byte_value;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_input_data_option);

    if(state == 1) {
        can_id[3] = app->frame_to_send->canId;
        can_id[2] = app->frame_to_send->canId >> 8;
        can_id[1] = app->frame_to_send->canId >> 16;
        can_id[0] = app->frame_to_send->canId >> 24;

        byte_input_set_header_text(scene, "SET ID");
        byte_input_set_result_callback(scene, input_byte_sender_callback, NULL, app, can_id, 4);
    }
    if((state > 1) && (state < 0xff)) {
        state = state - 4;

        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, "Set Byte [%lu]", state);
        byte_input_set_header_text(scene, furi_string_get_cstr(app->text));
        byte_input_set_result_callback(
            scene, input_byte_sender_callback, NULL, app, &(app->frame_to_send->buffer[state]), 1);
    }

    if(state == 0xff) {
        byte_input_set_header_text(scene, "Set Data");
        byte_input_set_result_callback(
            scene,
            input_byte_sender_callback,
            NULL,
            app,
            app->frame_to_send->buffer,
            app->frame_to_send->data_lenght);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

bool app_scene_input_data_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    UNUSED(event);
    UNUSED(app);

    return consumed;
}

void app_scene_input_data_on_exit(void* context) {
    UNUSED(context);
}

/**
 *  Scene for display the sender
 */

// Views for the sender
void draw_timer_to_send(App* app, double time) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget,
        64,
        20,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "Message will be\nsent in...");

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%.3f ms", time);

    widget_add_string_element(
        app->widget,
        64,
        40,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        furi_string_get_cstr(app->text));
}

// Views to know if it was send it
void draw_data_send(App* app, bool was_send_it, uint32_t count) {
    widget_reset(app->widget);

    if(was_send_it) {
        widget_add_string_element(
            app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Successfully");
    } else {
        widget_add_string_element(
            app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Failure");
    }

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%lx\n", app->frame_to_send->canId);

    if(app->frame_to_send->req) {
        furi_string_cat_printf(app->text, "Request");
        widget_add_string_element(
            app->widget,
            64,
            32,
            AlignCenter,
            AlignCenter,
            FontPrimary,
            furi_string_get_cstr(app->text));

        return;
    }

    for(uint8_t i = 0; i < app->frame_to_send->data_lenght; i++) {
        if(app->frame_to_send->buffer[i] > 0xf) {
            furi_string_cat_printf(app->text, "%x ", app->frame_to_send->buffer[i]);
            continue;
        }

        furi_string_cat_printf(app->text, "0%x ", app->frame_to_send->buffer[i]);
    }

    widget_add_string_multiline_element(
        app->widget,
        64,
        30,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Message count: %lu", count);

    widget_add_string_element(
        app->widget,
        64,
        50,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

void draw_data_send_repeat(App* app, bool was_send_it, uint32_t count, uint32_t total_count) {
    widget_reset(app->widget);

    furi_string_reset(app->text);

    if(was_send_it) {
        widget_add_string_element(
            app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Successfully");
    } else {
        widget_add_string_element(
            app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Failure");
    }

    furi_string_cat_printf(app->text, "%lx ", app->frame_to_send->canId);

    if(app->frame_to_send->req) {
        furi_string_cat_printf(app->text, "Request");
        widget_add_string_element(
            app->widget,
            64,
            32,
            AlignCenter,
            AlignCenter,
            FontPrimary,
            furi_string_get_cstr(app->text));

        return;
    }

    for(uint8_t i = 0; i < app->frame_to_send->data_lenght; i++) {
        if(app->frame_to_send->buffer[i] > 0xf) {
            furi_string_cat_printf(app->text, "%x ", app->frame_to_send->buffer[i]);
            continue;
        }

        furi_string_cat_printf(app->text, "0%x ", app->frame_to_send->buffer[i]);
    }

    widget_add_string_element(
        app->widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%lu of %lu", count, total_count);

    widget_add_string_element(
        app->widget,
        64,
        45,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

void draw_waiting_time_to_send(App* app) {
    widget_reset(app->widget);

    widget_add_string_multiline_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "WAITING TIME\n TO SEND");
}

void draw_finished_to_send(App* app) {
    widget_reset(app->widget);

    widget_add_string_multiline_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "FINISHED\n TO SEND");
}

// Sender on enter
void app_scene_send_message_on_enter(void* context) {
    App* app = context;

    widget_reset(app->widget);

    switch(timing) {
    case 0:
        app->thread = furi_thread_alloc_ex("Sender Thread", 1024, thread_to_send_once, app);
        break;

    case 1:
        app->thread = furi_thread_alloc_ex("Sender Thread", 1024, thread_to_send_periodic, app);
        break;

    default:
        app->thread = furi_thread_alloc_ex("Sender Thread", 1024, thread_to_send_repeat, app);
        break;
    }

    furi_thread_start(app->thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

// Sender on event
bool app_scene_send_message_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SEND_OK:
            draw_send_ok(app);
            break;

        case SEND_ERROR:
            draw_send_wrong(app);
            break;

        case DEVICE_NO_CONNECTED:
            draw_device_no_connected(app);
            break;

        default:
            break;
        }
    }

    return consumed;
}

// Sender on exit
void app_scene_send_message_on_exit(void* context) {
    App* app = context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);

    widget_reset(app->widget);
}

/**
 * Thread to send Once
 */

static int32_t thread_to_send_once(void* context) {
    App* app = context;
    MCP2515* mcp_can = app->mcp_can;
    mcp_can->mode = MCP_NORMAL;

    bool debug = (mcp2515_init(mcp_can) == ERROR_OK) ? true : false;

    if(!debug) draw_device_no_connected(app);

    uint32_t timer_send = time * (pow(10, multiply));

    uint32_t last_time = furi_get_tick();

    uint32_t timer = 0;

    while(debug && furi_hal_gpio_read(&gpio_button_back)) {
        timer = timer_send - (furi_get_tick() - last_time);

        if(timer_send < (furi_get_tick() - last_time)) {
            draw_data_send(app, (send_can_frame(mcp_can, app->frame_to_send) == ERROR_OK), 1);
            break;
        } else
            furi_delay_ms(1);

        draw_timer_to_send(app, (double)timer / 1000);
    }

    deinit_mcp2515(mcp_can);

    return 0;
}

/**
 *  Thread to send Periodic
 */

static int32_t thread_to_send_periodic(void* context) {
    App* app = context;
    MCP2515* mcp_can = app->mcp_can;
    mcp_can->mode = MCP_NORMAL;

    bool debug = (mcp2515_init(mcp_can) == ERROR_OK) ? true : false;

    if(!debug) draw_device_no_connected(app);

    draw_waiting_time_to_send(app);

    uint32_t timer_send = time * (pow(10, multiply));

    uint32_t last_time = furi_get_tick();

    uint32_t count = 1;

    while(debug && furi_hal_gpio_read(&gpio_button_back)) {
        if(timer_send < (furi_get_tick() - last_time)) {
            draw_data_send(app, (send_can_frame(mcp_can, app->frame_to_send) == ERROR_OK), count);
            count++;
            last_time = furi_get_tick();
        } else
            furi_delay_ms(1);
    }

    deinit_mcp2515(mcp_can);

    return 0;
}

/**
 * Thread to send multiple times
 */

static int32_t thread_to_send_repeat(void* context) {
    App* app = context;
    MCP2515* mcp_can = app->mcp_can;
    mcp_can->mode = MCP_NORMAL;

    bool debug = (mcp2515_init(mcp_can) == ERROR_OK) ? true : false;

    if(!debug) draw_device_no_connected(app);

    draw_waiting_time_to_send(app);

    uint32_t timer_send = time * (pow(10, multiply));

    uint32_t last_time = furi_get_tick();

    uint8_t total_count = quantity_to_repeat * (pow(10, multiply_quantity));

    uint32_t count = 1;

    while(debug && furi_hal_gpio_read(&gpio_button_back)) {
        if(timer_send < (furi_get_tick() - last_time)) {
            draw_data_send(app, (send_can_frame(mcp_can, app->frame_to_send) == ERROR_OK), count);
            count++;
            last_time = furi_get_tick();
        } else
            furi_delay_ms(1);

        if(count > total_count) {
            furi_delay_ms(500);
            draw_finished_to_send(app);
            break;
        }
    }

    deinit_mcp2515(mcp_can);

    return 0;
}

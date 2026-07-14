#include "../app_user.h"
#define TAG            "PLAY "
#define MAX_UNIQUE_IDS 100

typedef struct {
    uint32_t id;
    bool enabled;
} UniqueId;

typedef enum {
    PLAY_OK,
    PLAY_ERROR,
} player_status;

typedef enum {
    TIMING_DEFAULT,
    TIMING_CUSTOM,
    TIMING_TIMESTAMP,
} timing;

// Timing configuration
const uint8_t config_timing_values[] = {
    0x00,
    0x01,
    0x02,
};
const char* const config_timing_names[] = {
    "DEFAULT",
    "CUSTOM",
    "TIMESTAMP",
};

const char* multiply_timing[] = {"x1", "x10", "x100", "x1000"};

// Costum time
uint32_t costum_timing = 1;
uint32_t multiply = 0;

uint32_t hex_to_int(const char* hex_str) {
    unsigned int result = 0;
    sscanf(hex_str, "%x", &result);
    return (uint32_t)result;
}

char* custom_strtok_r(char* str, const char* delim, char** saveptr) {
    if(str) {
        *saveptr = str;
    }
    if(!*saveptr) {
        return NULL;
    }

    char* start = *saveptr;
    while(*start && strchr(delim, *start)) {
        ++start;
    }
    if(*start == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    char* end = start;
    while(*end && !strchr(delim, *end)) {
        ++end;
    }

    if(*end) {
        *end = '\0';
        *saveptr = end + 1;
    } else {
        *saveptr = NULL;
    }

    return start;
}

void path_file_name(const char* path, FuriString* file_name) {
    uint8_t last_pos = 0;
    for(uint8_t i = 0; path[i] != '\0'; i++) {
        if(path[i] == '/') last_pos = i + 1;
    }

    furi_string_reset(file_name);

    for(uint8_t i = last_pos; path[i] != '\0'; i++) {
        furi_string_cat_printf(file_name, "%c", path[i]);
    }
}

// For the message
void draw_message_send(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 62, 25, AlignCenter, AlignBottom, FontPrimary, "Sending...");

    widget_add_string_element(
        app->widget,
        62,
        40,
        AlignCenter,
        AlignBottom,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

// Message sent fail
void draw_message_fail(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 62, 30, AlignCenter, AlignBottom, FontPrimary, "Sending Failed...");

    widget_add_string_element(
        app->widget,
        62,
        50,
        AlignCenter,
        AlignBottom,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

void draw_finished(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 39, 0, &I_RPLYOK49x38);
    widget_add_string_element(
        app->widget, 62, 47, AlignCenter, AlignCenter, FontPrimary, "REPLAY FINISHED");
}

void draw_file_no_opened(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 50, 0, &I_FILERROR27x38);
    widget_add_string_element(
        app->widget, 62, 47, AlignCenter, AlignCenter, FontPrimary, "File cannot be opened");
}

void draw_starting_transmition(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 62, 32, AlignCenter, AlignBottom, FontPrimary, "Starting Transmition");
}

void play_data_frames_bk(void* context, int frame_interval) {
    App* app = context;

    app->mcp_can->mode = MCP_NORMAL;
    ERROR_CAN debug = ERROR_OK;
    debug = mcp2515_init(app->mcp_can);

    if(debug != ERROR_OK) {
        draw_device_no_connected(app);
        return;
    }

    if(!storage_file_open(
           app->log_file, furi_string_get_cstr(app->path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        draw_file_no_opened(app);
        return;
    }

    draw_starting_transmition(app);

    char buffer[256];
    size_t buffer_index = 0;
    size_t bytes_read;
    char c;

    // [Code for collecting unique IDs remains the same...]
    buffer_index = 0;

    // Read first character for next frame
    bytes_read = storage_file_read(app->log_file, &c, 1);
    storage_file_seek(app->log_file, 0, true);

    // To have the time
    uint32_t delay = 0;
    uint32_t offset_time = 0;
    bool first_frame = true;

    while(bytes_read > 0) {
        // Read current frame into buffer
        while((bytes_read = storage_file_read(app->log_file, &c, 1)) > 0) {
            if(c == '\n' || buffer_index >= 255) {
                buffer[buffer_index] = '\0';
                break;
            }
            buffer[buffer_index++] = c;
        }
        buffer[buffer_index] = '\0';

        // Process current frame
        CANFRAME frame_to_send = {0};
        char* saveptr;
        char* endptr;
        char* token;

        // Get current timestamp
        token = custom_strtok_r(buffer, ":", &saveptr);
        if(!token) break;
        token = custom_strtok_r(NULL, ":", &saveptr);
        if(!token) break;
        token = custom_strtok_r(NULL, ":", &saveptr);
        if(!token) break;

        // Get next timestamp if available
        uint32_t delay_ms = strtod(token, &endptr);

        // CAN bus ID
        token = custom_strtok_r(NULL, ":", &saveptr);
        if(!token) break;
        frame_to_send.canId = hex_to_int(token);

        // Data length
        token = custom_strtok_r(NULL, ":", &saveptr);
        if(!token) break;
        frame_to_send.data_lenght = (uint8_t)atoi(token);

        // Fill the data buffer
        for(int i = 0; i < frame_to_send.data_lenght && i < MAX_LEN; i++) {
            token = custom_strtok_r(NULL, " ", &saveptr);
            if(!token) break;
            frame_to_send.buffer[i] = (uint8_t)hex_to_int(token);
        }

        // To get the offset time
        if(first_frame) {
            offset_time = delay_ms;
            first_frame = false;
        }

        // Apply timing based on frame_interval mode
        switch(frame_interval) {
        case TIMING_CUSTOM:
            if(costum_timing == 0) {
                costum_timing = 1;
            }

            delay = costum_timing * pow(10, multiply);

            if(first_frame) {
                delay = 0;
                first_frame = false;
            }

            break;

        case TIMING_TIMESTAMP:

            delay = (delay_ms - offset_time);

            break;
        case TIMING_DEFAULT:
            delay = 1000;
            break;
        }

        uint32_t current_time = furi_get_tick();

        // Delay with real time it works to exit from the while
        while((furi_get_tick() - current_time) < delay) {
            if(!furi_hal_gpio_read(&(gpio_button_back))) {
                storage_file_close(app->log_file);
                deinit_mcp2515(app->mcp_can);
                return;
            }
        }

        furi_string_reset(app->text);
        furi_string_cat_printf(
            app->text, "%lx %u", frame_to_send.canId, frame_to_send.data_lenght);

        for(uint8_t i = 0; i < frame_to_send.data_lenght; i++) {
            furi_string_cat_printf(app->text, " %x", frame_to_send.buffer[i]);
        }

        if(send_can_frame(app->mcp_can, &frame_to_send) != ERROR_OK) {
            draw_message_fail(app);
        } else {
            draw_message_send(app);
        }

        // Set the last time
        offset_time = delay_ms;
        buffer_index = 0;
    }

    furi_delay_ms(1000);
    draw_finished(app);

    storage_file_close(app->log_file);
    deinit_mcp2515(app->mcp_can);
}

// Thread work
int32_t thread_play_logs(void* context) {
    App* app = context;
    play_data_frames_bk(app, app->config_timing_index);

    return 0;
}

void draw_list(App* app);
void draw_list_costum(App* app);

// Option callback using button OK
void callback_input_player_options(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case 0:
        scene_manager_next_scene(app->scene_manager, app_scene_play_logs_widget);
        break;

    case 1:
        scene_manager_next_scene(app->scene_manager, app_scene_file_browser_option);
        break;

    default:
        break;
    }
}

// Options Callback
void callback_player_timing_options(VariableItem* item) {
    App* app = variable_item_get_context(item);

    uint8_t selected_index = variable_item_list_get_selected_item_index(app->varList);

    switch(selected_index) {
    case 2:
        uint8_t index = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(item, config_timing_names[index]);

        app->config_timing_index = index;

        if(app->config_timing_index == TIMING_CUSTOM) {
            draw_list_costum(app);
        } else {
            draw_list(app);
        }

        break;

    case 3:
        costum_timing = variable_item_get_current_value_index(item);

        variable_item_set_current_value_index(item, costum_timing);

        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, "%lu", costum_timing);

        variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

        break;

    case 4:
        multiply = variable_item_get_current_value_index(item);

        variable_item_set_current_value_index(item, multiply);

        variable_item_set_current_value_text(item, multiply_timing[multiply]);
        break;

    default:
        break;
    }
}

void draw_list(App* app) {
    VariableItem* item;

    // reset list
    variable_item_list_reset(app->varList);

    // Play the logs
    item = variable_item_list_add(app->varList, "Play", 0, NULL, app);

    // Choose File
    item = variable_item_list_add(app->varList, "File", 0, NULL, app);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->data));

    // Timing options
    item = variable_item_list_add(
        app->varList,
        "Timing",
        COUNT_OF(config_timing_values),
        callback_player_timing_options,
        app);

    variable_item_set_current_value_index(item, app->config_timing_index);
    variable_item_set_current_value_text(item, config_timing_names[app->config_timing_index]);

    // Set the enter callback
    variable_item_list_set_enter_callback(app->varList, callback_input_player_options, app);
}

void draw_list_costum(App* app) {
    VariableItem* item;

    // reset list
    variable_item_list_reset(app->varList);

    // Play the logs
    item = variable_item_list_add(app->varList, "Play", 0, NULL, app);

    // Choose File
    item = variable_item_list_add(app->varList, "File", 0, NULL, app);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->data));

    // Timing options
    item = variable_item_list_add(
        app->varList,
        "Timing",
        COUNT_OF(config_timing_values),
        callback_player_timing_options,
        app);

    variable_item_set_current_value_index(item, app->config_timing_index);
    variable_item_set_current_value_text(item, config_timing_names[app->config_timing_index]);

    // Timing options
    item =
        variable_item_list_add(app->varList, "Time(ms)", 99, callback_player_timing_options, app);
    variable_item_set_current_value_index(item, costum_timing);

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%lu", costum_timing);

    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    // Set the enter callback
    variable_item_list_set_enter_callback(app->varList, callback_input_player_options, app);

    item = variable_item_list_add(
        app->varList, "Time(ms)", COUNT_OF(multiply_timing), callback_player_timing_options, app);
    variable_item_set_current_value_index(item, multiply);
    variable_item_set_current_value_text(item, multiply_timing[multiply]);
}

// The function to enter a Scene
void app_scene_play_logs_on_enter(void* context) {
    App* app = context;

    if(app->config_timing_index == TIMING_CUSTOM) {
        draw_list_costum(app);
    } else {
        draw_list(app);
    }

    variable_item_list_set_selected_item(app->varList, 0);

    // Switch View
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

// The function to exit the scene
void app_scene_play_logs_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->varList);
}

// The function for the events
bool app_scene_play_logs_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    UNUSED(event);
    UNUSED(app);
    return consumed;
}

// File browser callback
void file_browser_callback(void* context) {
    App* app = context;
    path_file_name(furi_string_get_cstr(app->path), app->data);
    scene_manager_previous_scene(app->scene_manager);
}

// File Browser scene on enter
void app_scene_file_browser_on_enter(void* context) {
    App* app = context;
    file_browser_configure(app->file_browser, ".log", PATHLOGS, true, true, NULL, true);
    file_browser_set_callback(app->file_browser, file_browser_callback, app);

    furi_string_reset(app->text);
    furi_string_cat(app->text, PATHLOGS);

    file_browser_start(app->file_browser, app->text);
    view_dispatcher_switch_to_view(app->view_dispatcher, FileBrowserView);
}

// File browser scene on event
bool app_scene_file_browser_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    UNUSED(event);
    UNUSED(app);
    return consumed;
}

// File browser on exit
void app_scene_file_browser_on_exit(void* context) {
    App* app = context;
    file_browser_stop(app->file_browser);
}

// Test widget
void app_scene_play_logs_widget_on_enter(void* context) {
    App* app = context;
    app->thread = furi_thread_alloc_ex("PlayLogs", 10 * 1024, thread_play_logs, app);
    furi_thread_start(app->thread);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool app_scene_play_logs_widget_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    UNUSED(event);
    UNUSED(app);
    return consumed;
}

void app_scene_play_logs_widget_on_exit(void* context) {
    App* app = context;

    furi_thread_join(app->thread);
    furi_thread_free(app->thread);

    widget_reset(app->widget);
}

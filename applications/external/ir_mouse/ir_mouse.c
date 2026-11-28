#include <furi.h>
#include <furi/core/event_flag.h>
#include <furi_hal_infrared.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <infrared_worker.h>
#include <input/input.h>
#include <storage/storage.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include <hid_usage_keyboard.h>

#include <stdio.h>
#include <string.h>

#define IR_MOUSE_STEP          5
#define IR_MOUSE_REPEAT_STEP   10
#define IR_BEACON_BURST_MS     15U
#define IR_BEACON_BURST_US     (IR_BEACON_BURST_MS * 1000U)
#define IR_BEACON_PERIOD_MS    60U
#define IR_MOUSE_HID_BTN_LEFT  0x01U
#define IR_MOUSE_HID_BTN_RIGHT 0x02U
#define IR_MOUSE_EXIT_FLAG     1U
#define KEYMAP_DIRECTORY       "/ext/apps_data/ir_mouse"
#define KEYMAP_PATH            KEYMAP_DIRECTORY "/keymap.txt"
#define KEYMAP_BUFFER_SIZE     512U

typedef enum {
    IrMouseModeRun,
    IrMouseModeLearn,
    IrMouseModeBeacon,
    IrMouseModeCount,
} IrMouseMode;

typedef enum {
    IrMouseActionUp,
    IrMouseActionDown,
    IrMouseActionLeft,
    IrMouseActionRight,
    IrMouseActionLeftClick,
    IrMouseActionRightClick,
    IrMouseActionSpacebar,
    IrMouseActionCount,
} IrMouseAction;

typedef struct {
    bool valid;
    InfraredProtocol protocol;
    uint32_t address;
    uint32_t command;
} IrMouseBinding;

typedef enum {
    IrMouseEventInput,
    IrMouseEventSignal,
} IrMouseEventType;

typedef struct {
    IrMouseEventType type;
    union {
        InputEvent input;
        struct {
            InfraredProtocol protocol;
            uint32_t address;
            uint32_t command;
            uint32_t generation;
            bool repeat;
        } signal;
    } data;
} IrMouseEvent;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    FuriEventFlag* exit_event;
    ViewPort* view_port;
    InfraredWorker* worker;
    Bt* bt;
    FuriHalBleProfileBase* ble_profile;
    IrMouseMode mode;
    IrMouseBinding bindings[IrMouseActionCount];
    uint8_t selected_action;
    int8_t active_action;
    char protocol[16];
    char status[20];
    uint32_t address;
    uint32_t command;
    uint32_t status_until;
    uint32_t rx_generation;
    InfraredProtocol last_protocol;
    uint32_t last_address;
    uint32_t last_command;
    bool has_signal;
    bool repeating;
    bool has_last_signal;
    bool rx_active;
    bool beacon_active;
    bool beacon_left_pressed;
    bool beacon_right_pressed;
    uint32_t beacon_next_pulse_tick;
    bool bt_connected;
} IrMouseApp;

static const char* const ir_mouse_action_names[IrMouseActionCount] = {
    "Mouse Up",
    "Mouse Down",
    "Mouse Left",
    "Mouse Right",
    "Left Click",
    "Right Click",
    "Spacebar",
};

static const char* const ir_mouse_mode_names[IrMouseModeCount] = {
    "Run Mode",
    "Learn Mode",
    "Beacon Mode",
};

static void ir_mouse_set_status(IrMouseApp* app, const char* status) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(app->status, sizeof(app->status), "%s", status);
    app->status_until = furi_get_tick() + furi_ms_to_ticks(1200);
    furi_mutex_release(app->mutex);
}

static void ir_mouse_bt_status_changed(BtStatus status, void* context) {
    IrMouseApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->bt_connected = status == BtStatusConnected;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void ir_mouse_draw_str_fit(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    const char* text,
    uint16_t max_width) {
    char fitted[64];
    snprintf(fitted, sizeof(fitted), "%s", text);
    size_t length = strlen(fitted);
    while(length > 0 && canvas_string_width(canvas, fitted) > max_width) {
        fitted[--length] = '\0';
    }
    canvas_draw_str(canvas, x, y, fitted);
}

static void ir_mouse_draw_footer(Canvas* canvas, const char* text) {
    char fitted[64];
    snprintf(fitted, sizeof(fitted), "%s", text);
    size_t length = strlen(fitted);
    while(length > 0 && canvas_string_width(canvas, fitted) > canvas_width(canvas)) {
        fitted[--length] = '\0';
    }
    canvas_draw_str_aligned(
        canvas, (int32_t)(canvas_width(canvas) / 2U), 62, AlignCenter, AlignBottom, fitted);
}

static void ir_mouse_draw_callback(Canvas* canvas, void* context) {
    IrMouseApp* app = context;
    IrMouseMode mode;
    uint8_t selected_action;
    int8_t active_action;
    char protocol[sizeof(app->protocol)];
    char status[sizeof(app->status)];
    uint32_t address;
    uint32_t command;
    uint32_t status_until;
    bool has_signal;
    bool repeating;
    bool bt_connected;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    mode = app->mode;
    selected_action = app->selected_action;
    active_action = app->active_action;
    snprintf(protocol, sizeof(protocol), "%s", app->protocol);
    snprintf(status, sizeof(status), "%s", app->status);
    address = app->address;
    command = app->command;
    status_until = app->status_until;
    has_signal = app->has_signal;
    repeating = app->repeating;
    bt_connected = app->bt_connected;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    char line[40];
    const char* bt_badge = bt_connected ? "BT: Connected" : "BT: Ready";
    canvas_set_font(canvas, FontSecondary);
    uint16_t badge_width = canvas_string_width(canvas, bt_badge);
    size_t width = canvas_width(canvas);
    uint16_t title_width =
        width > (size_t)badge_width + 6U ? (uint16_t)(width - badge_width - 6U) : 0U;
    canvas_set_font(canvas, FontPrimary);
    ir_mouse_draw_str_fit(canvas, 0, 11, ir_mouse_mode_names[mode], title_width);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, (int32_t)width - 1, 11, AlignRight, AlignBottom, bt_badge);
    canvas_draw_line(canvas, 0, 13, 128, 13);

    const bool status_visible = status[0] != '\0' && (int32_t)(status_until - furi_get_tick()) > 0;
    if(mode == IrMouseModeLearn) {
        canvas_set_font(canvas, FontPrimary);
        snprintf(line, sizeof(line), "> %s", ir_mouse_action_names[selected_action]);
        ir_mouse_draw_str_fit(canvas, 0, 26, line, (uint16_t)width);

        canvas_set_font(canvas, FontSecondary);
        ir_mouse_draw_str_fit(
            canvas, 0, 39, status_visible ? status : "Press remote key to bind", (uint16_t)width);
        if(has_signal) {
            snprintf(
                line,
                sizeof(line),
                "%.10s A:%lX C:%lX%s",
                protocol,
                (unsigned long)address,
                (unsigned long)command,
                repeating ? " R" : "");
            ir_mouse_draw_str_fit(canvas, 0, 52, line, (uint16_t)width);
        } else {
            ir_mouse_draw_str_fit(canvas, 0, 52, "Waiting for IR signal", (uint16_t)width);
        }
        ir_mouse_draw_footer(canvas, "U/D Action  L/R Mode  Back");
    } else if(mode == IrMouseModeBeacon) {
        canvas_set_font(canvas, FontSecondary);
        ir_mouse_draw_str_fit(
            canvas, 0, 26, status_visible ? status : "IR marker active", (uint16_t)width);
        ir_mouse_draw_str_fit(canvas, 0, 39, "OK: Left click", (uint16_t)width);
        ir_mouse_draw_str_fit(canvas, 0, 52, "Down: Right click", (uint16_t)width);
        ir_mouse_draw_footer(canvas, "L/R: Mode  Back: Exit");
    } else {
        if(status_visible) {
            canvas_set_font(canvas, FontSecondary);
            ir_mouse_draw_str_fit(canvas, 0, 26, status, (uint16_t)width);
        } else if(active_action >= 0) {
            canvas_set_font(canvas, FontPrimary);
            snprintf(line, sizeof(line), "Last: %s", ir_mouse_action_names[active_action]);
            ir_mouse_draw_str_fit(canvas, 0, 26, line, (uint16_t)width);
        } else {
            canvas_set_font(canvas, FontSecondary);
            ir_mouse_draw_str_fit(canvas, 0, 26, "Waiting for mapped key", (uint16_t)width);
        }

        canvas_set_font(canvas, FontSecondary);
        if(has_signal) {
            snprintf(line, sizeof(line), "Protocol: %.12s", protocol);
            ir_mouse_draw_str_fit(canvas, 0, 39, line, (uint16_t)width);
            snprintf(
                line,
                sizeof(line),
                "A:%08lX C:%08lX%s",
                (unsigned long)address,
                (unsigned long)command,
                repeating ? " R" : "");
            ir_mouse_draw_str_fit(canvas, 0, 52, line, (uint16_t)width);
        } else {
            ir_mouse_draw_str_fit(canvas, 0, 39, "Aim remote at Flipper", (uint16_t)width);
            ir_mouse_draw_str_fit(canvas, 0, 52, "No IR signal received", (uint16_t)width);
        }
        ir_mouse_draw_footer(canvas, "L/R: Mode  Back: Exit");
    }
}

static void ir_mouse_input_callback(InputEvent* input_event, void* context) {
    IrMouseApp* app = context;
    if(input_event->key == InputKeyBack && input_event->type == InputTypePress) {
        uint32_t flags = furi_event_flag_set(app->exit_event, IR_MOUSE_EXIT_FLAG);
        if(flags >= FuriFlagError) {
            FURI_LOG_E("IR Mouse", "Could not signal application exit");
        }
        return;
    }

    IrMouseEvent event = {
        .type = IrMouseEventInput,
        .data.input = *input_event,
    };
    if(furi_message_queue_put(app->event_queue, &event, 0) != FuriStatusOk) {
        FURI_LOG_W("IR Mouse", "Input event queue full; dropping input");
    }
}

static void ir_mouse_received_signal_callback(void* context, InfraredWorkerSignal* signal) {
    IrMouseApp* app = context;
    if(!infrared_worker_signal_is_decoded(signal)) return;

    const InfraredMessage* message = infrared_worker_get_decoded_signal(signal);
    if(!message) return;

    IrMouseEvent event = {
        .type = IrMouseEventSignal,
        .data.signal =
            {
                .protocol = message->protocol,
                .address = message->address,
                .command = message->command,
                .repeat = message->repeat,
            },
    };
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    event.data.signal.generation = app->rx_generation;
    furi_mutex_release(app->mutex);
    if(furi_message_queue_put(app->event_queue, &event, 0) != FuriStatusOk) {
        FURI_LOG_D("IR Mouse", "IR event queue full; dropping signal");
    }
}

static FuriHalInfraredTxGetDataState
    ir_mouse_beacon_data_callback(void* context, uint32_t* duration, bool* level) {
    UNUSED(context);
    *duration = IR_BEACON_BURST_US;
    *level = true;
    return FuriHalInfraredTxGetDataStateLastDone;
}

static void ir_mouse_load_keymap(IrMouseApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    char contents[KEYMAP_BUFFER_SIZE] = {0};
    FS_Error stat_result = storage_common_stat(storage, KEYMAP_PATH, NULL);

    if(stat_result == FSE_NOT_EXIST) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    if(stat_result != FSE_OK) {
        FURI_LOG_E(
            "IR Mouse",
            "Could not check keymap file: %s",
            filesystem_api_error_get_desc(stat_result));
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    if(!storage_file_open(file, KEYMAP_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("IR Mouse", "Could not open keymap file");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    size_t bytes_read = storage_file_read(file, contents, sizeof(contents) - 1U);
    contents[bytes_read] = '\0';
    if(bytes_read == sizeof(contents) - 1U) {
        FURI_LOG_W("IR Mouse", "Keymap file is larger than expected");
    }

    char* line = contents;
    while(line && *line) {
        char* next_line = strchr(line, '\n');
        if(next_line) {
            *next_line = '\0';
            next_line++;
        }

        unsigned int action_index;
        unsigned long address;
        unsigned long command;
        char protocol_name[16];
        if(sscanf(line, "%u %15s %lx %lx", &action_index, protocol_name, &address, &command) ==
               4 &&
           action_index < (unsigned int)IrMouseActionCount) {
            InfraredProtocol protocol = infrared_get_protocol_by_name(protocol_name);
            if(infrared_is_protocol_valid(protocol)) {
                app->bindings[action_index] = (IrMouseBinding){
                    .valid = true,
                    .protocol = protocol,
                    .address = (uint32_t)address,
                    .command = (uint32_t)command,
                };
            } else {
                FURI_LOG_W("IR Mouse", "Ignoring keymap entry with unknown protocol");
            }
        }

        line = next_line;
    }

    if(!storage_file_close(file)) {
        FURI_LOG_E("IR Mouse", "Could not close keymap file after reading");
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static bool ir_mouse_save_keymap(const IrMouseApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error directory_result = storage_common_mkdir(storage, "/ext/apps_data");
    if(directory_result != FSE_OK && directory_result != FSE_EXIST) {
        FURI_LOG_E(
            "IR Mouse",
            "Could not create apps_data directory: %s",
            filesystem_api_error_get_desc(directory_result));
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    directory_result = storage_common_mkdir(storage, KEYMAP_DIRECTORY);
    if(directory_result != FSE_OK && directory_result != FSE_EXIST) {
        FURI_LOG_E(
            "IR Mouse",
            "Could not create keymap directory: %s",
            filesystem_api_error_get_desc(directory_result));
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, KEYMAP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E("IR Mouse", "Could not open keymap file for writing");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool success = true;
    char line[64];
    for(unsigned int i = 0; i < (unsigned int)IrMouseActionCount; i++) {
        const IrMouseBinding* binding = &app->bindings[i];
        if(!binding->valid) continue;

        int length = snprintf(
            line,
            sizeof(line),
            "%u %s 0x%08lX 0x%08lX\n",
            i,
            infrared_get_protocol_name(binding->protocol),
            (unsigned long)binding->address,
            (unsigned long)binding->command);
        if(length < 0 || (size_t)length >= sizeof(line) ||
           storage_file_write(file, line, (size_t)length) != (size_t)length) {
            success = false;
            break;
        }
    }

    if(success && !storage_file_sync(file)) success = false;
    if(!storage_file_close(file)) success = false;
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!success) FURI_LOG_E("IR Mouse", "Could not persist keymap");
    return success;
}

static bool ir_mouse_hid_mouse_move(IrMouseApp* app, int8_t dx, int8_t dy) {
    if(app->ble_profile && ble_profile_hid_mouse_move(app->ble_profile, dx, dy)) return true;
    FURI_LOG_W("IR Mouse", "BLE mouse movement failed");
    ir_mouse_set_status(app, "BLE HID send failed");
    return false;
}

static bool ir_mouse_hid_mouse_button(IrMouseApp* app, uint8_t button, bool pressed) {
    if(!app->ble_profile) return false;
    const bool success = pressed ? ble_profile_hid_mouse_press(app->ble_profile, button) :
                                   ble_profile_hid_mouse_release(app->ble_profile, button);
    if(!success) {
        FURI_LOG_W("IR Mouse", "BLE mouse button report failed");
        ir_mouse_set_status(app, "BLE HID send failed");
    }
    return success;
}

static void ir_mouse_execute_action(IrMouseApp* app, IrMouseAction action, bool repeat) {
    const int8_t step = repeat ? IR_MOUSE_REPEAT_STEP : IR_MOUSE_STEP;
    switch(action) {
    case IrMouseActionUp:
        ir_mouse_hid_mouse_move(app, 0, -step);
        break;
    case IrMouseActionDown:
        ir_mouse_hid_mouse_move(app, 0, step);
        break;
    case IrMouseActionLeft:
        ir_mouse_hid_mouse_move(app, -step, 0);
        break;
    case IrMouseActionRight:
        ir_mouse_hid_mouse_move(app, step, 0);
        break;
    case IrMouseActionLeftClick:
        ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_LEFT, true);
        ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_LEFT, false);
        break;
    case IrMouseActionRightClick:
        ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_RIGHT, true);
        ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_RIGHT, false);
        break;
    case IrMouseActionSpacebar:
        if(!app->ble_profile) {
            FURI_LOG_W("IR Mouse", "BLE keyboard report failed");
            ir_mouse_set_status(app, "BLE HID send failed");
        } else {
            const bool pressed = ble_profile_hid_kb_press(app->ble_profile, HID_KEYBOARD_SPACEBAR);
            const bool released =
                ble_profile_hid_kb_release(app->ble_profile, HID_KEYBOARD_SPACEBAR);
            if(!pressed || !released) {
                FURI_LOG_W("IR Mouse", "BLE keyboard report failed");
                ir_mouse_set_status(app, "BLE HID send failed");
            }
        }
        break;
    case IrMouseActionCount:
        break;
    }
}

static void ir_mouse_process_signal(IrMouseApp* app, const IrMouseEvent* event) {
    const InfraredProtocol protocol = event->data.signal.protocol;
    const uint32_t address = event->data.signal.address;
    const uint32_t command = event->data.signal.command;
    const bool repeat = event->data.signal.repeat;

    if(!repeat) {
        app->last_protocol = protocol;
        app->last_address = address;
        app->last_command = command;
        app->has_last_signal = true;

        const char* protocol_name = infrared_is_protocol_valid(protocol) ?
                                        infrared_get_protocol_name(protocol) :
                                        "Unknown";
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        snprintf(app->protocol, sizeof(app->protocol), "%s", protocol_name);
        app->address = address;
        app->command = command;
        app->has_signal = true;
        furi_mutex_release(app->mutex);
    }
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->repeating = repeat;
    furi_mutex_release(app->mutex);

    if(app->mode == IrMouseModeLearn) {
        if(!repeat && infrared_is_protocol_valid(protocol)) {
            app->bindings[app->selected_action] = (IrMouseBinding){
                .valid = true,
                .protocol = protocol,
                .address = address,
                .command = command,
            };
            if(ir_mouse_save_keymap(app)) {
                ir_mouse_set_status(app, "Saved!");
            } else {
                ir_mouse_set_status(app, "Save failed");
            }
        }
        view_port_update(app->view_port);
        return;
    }

    if(app->mode != IrMouseModeRun) return;

    InfraredProtocol match_protocol = protocol;
    uint32_t match_address = address;
    uint32_t match_command = command;
    if(repeat) {
        if(!app->has_last_signal) return;
        match_protocol = app->last_protocol;
        match_address = app->last_address;
        match_command = app->last_command;
    }

    for(unsigned int i = 0; i < (unsigned int)IrMouseActionCount; i++) {
        const IrMouseBinding* binding = &app->bindings[i];
        if(binding->valid && binding->protocol == match_protocol &&
           binding->address == match_address && binding->command == match_command) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->active_action = (int8_t)i;
            furi_mutex_release(app->mutex);
            ir_mouse_execute_action(app, (IrMouseAction)i, repeat);
            break;
        }
    }
    view_port_update(app->view_port);
}

static void ir_mouse_start_beacon(IrMouseApp* app) {
    furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);
    app->beacon_next_pulse_tick = furi_get_tick();
}

static void ir_mouse_send_beacon_burst(IrMouseApp* app) {
    if(app->mode != IrMouseModeBeacon || app->beacon_active) return;

    furi_hal_infrared_async_tx_set_data_isr_callback(ir_mouse_beacon_data_callback, app);
    furi_hal_infrared_async_tx_start(38000U, 0.33f);
    app->beacon_active = true;
    furi_delay_ms(IR_BEACON_BURST_MS);
    furi_hal_infrared_async_tx_stop();
    app->beacon_active = false;
    app->beacon_next_pulse_tick =
        furi_get_tick() + furi_ms_to_ticks(IR_BEACON_PERIOD_MS - IR_BEACON_BURST_MS);
}

static void ir_mouse_release_beacon_buttons(IrMouseApp* app) {
    if(app->beacon_left_pressed) {
        ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_LEFT, false);
        app->beacon_left_pressed = false;
    }
    if(app->beacon_right_pressed) {
        ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_RIGHT, false);
        app->beacon_right_pressed = false;
    }
}

static void ir_mouse_stop_beacon(IrMouseApp* app) {
    if(app->beacon_active) {
        furi_hal_infrared_async_tx_stop();
        app->beacon_active = false;
    }
    ir_mouse_release_beacon_buttons(app);
}

static void ir_mouse_set_mode(IrMouseApp* app, IrMouseMode mode) {
    if(mode == app->mode) return;

    IrMouseMode old_mode = app->mode;
    if(old_mode == IrMouseModeBeacon) {
        ir_mouse_stop_beacon(app);
    } else if(mode == IrMouseModeBeacon && app->rx_active) {
        infrared_worker_rx_set_received_signal_callback(app->worker, NULL, NULL);
        infrared_worker_rx_stop(app->worker);
        app->rx_active = false;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->mode = mode;
    app->rx_generation++;
    furi_mutex_release(app->mutex);

    if(mode == IrMouseModeBeacon) {
        ir_mouse_start_beacon(app);
        ir_mouse_set_status(app, "Beacon active");
    } else {
        if(old_mode == IrMouseModeBeacon) {
            infrared_worker_rx_set_received_signal_callback(
                app->worker, ir_mouse_received_signal_callback, app);
            infrared_worker_rx_start(app->worker);
            app->rx_active = true;
        }
        ir_mouse_set_status(app, mode == IrMouseModeLearn ? "Learn mode" : "Run mode");
    }

    view_port_update(app->view_port);
}

static void ir_mouse_process_input(IrMouseApp* app, const InputEvent* input) {
    if(input->type == InputTypePress &&
       (input->key == InputKeyLeft || input->key == InputKeyRight)) {
        int mode_delta = input->key == InputKeyRight ? 1 : (int)IrMouseModeCount - 1;
        IrMouseMode mode = (IrMouseMode)((app->mode + mode_delta) % IrMouseModeCount);
        ir_mouse_set_mode(app, mode);
        return;
    }

    if(app->mode == IrMouseModeBeacon) {
        if(input->key == InputKeyOk) {
            if(input->type == InputTypePress && !app->beacon_left_pressed) {
                app->beacon_left_pressed =
                    ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_LEFT, true);
            } else if(input->type == InputTypeRelease && app->beacon_left_pressed) {
                ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_LEFT, false);
                app->beacon_left_pressed = false;
            }
            return;
        }

        if(input->key == InputKeyDown) {
            if(input->type == InputTypePress && !app->beacon_right_pressed) {
                app->beacon_right_pressed =
                    ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_RIGHT, true);
            } else if(input->type == InputTypeRelease && app->beacon_right_pressed) {
                ir_mouse_hid_mouse_button(app, IR_MOUSE_HID_BTN_RIGHT, false);
                app->beacon_right_pressed = false;
            }
            return;
        }
    }

    if(app->mode == IrMouseModeLearn &&
       (input->type == InputTypePress || input->type == InputTypeRepeat)) {
        if(input->key == InputKeyUp) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->selected_action =
                (uint8_t)((app->selected_action + (uint8_t)IrMouseActionCount - 1U) %
                          (uint8_t)IrMouseActionCount);
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        } else if(input->key == InputKeyDown) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->selected_action =
                (uint8_t)((app->selected_action + 1U) % (uint8_t)IrMouseActionCount);
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        }
    }
}

int32_t ir_mouse_app(void* context) {
    UNUSED(context);

    IrMouseApp app = {
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .event_queue = furi_message_queue_alloc(16, sizeof(IrMouseEvent)),
        .exit_event = furi_event_flag_alloc(),
        .view_port = view_port_alloc(),
        .mode = IrMouseModeRun,
        .active_action = -1,
        .protocol = "Waiting",
        .rx_generation = 1,
    };
    Gui* gui = NULL;
    bool view_port_added = false;
    int32_t result = -1;

    if(!app.mutex || !app.event_queue || !app.exit_event || !app.view_port) {
        FURI_LOG_E("IR Mouse", "Failed to allocate application resources");
        goto cleanup;
    }

    view_port_draw_callback_set(app.view_port, ir_mouse_draw_callback, &app);
    view_port_input_callback_set(app.view_port, ir_mouse_input_callback, &app);
    gui = furi_record_open(RECORD_GUI);
    if(!gui) {
        FURI_LOG_E("IR Mouse", "Could not open GUI service");
        goto cleanup;
    }
    gui_add_view_port(gui, app.view_port, GuiLayerFullscreen);
    view_port_added = true;

    app.bt = furi_record_open(RECORD_BT);
    if(!app.bt) {
        FURI_LOG_E("IR Mouse", "Could not open Bluetooth service");
        goto cleanup;
    }
    bt_set_status_changed_callback(app.bt, ir_mouse_bt_status_changed, &app);
    app.ble_profile = bt_profile_start(app.bt, ble_profile_hid, NULL);
    if(!app.ble_profile) {
        FURI_LOG_E("IR Mouse", "Could not start BLE HID profile");
        goto cleanup;
    }

    ir_mouse_load_keymap(&app);
    app.worker = infrared_worker_alloc();
    if(!app.worker) {
        FURI_LOG_E("IR Mouse", "Failed to allocate IR worker");
        goto cleanup;
    }
    infrared_worker_rx_enable_signal_decoding(app.worker, true);
    infrared_worker_rx_set_received_signal_callback(
        app.worker, ir_mouse_received_signal_callback, &app);
    infrared_worker_rx_start(app.worker);
    app.rx_active = true;

    result = 0;
    view_port_update(app.view_port);
    while((furi_event_flag_get(app.exit_event) & IR_MOUSE_EXIT_FLAG) == 0U) {
        if(app.mode == IrMouseModeBeacon &&
           (int32_t)(furi_get_tick() - app.beacon_next_pulse_tick) >= 0) {
            ir_mouse_send_beacon_burst(&app);
        }

        IrMouseEvent event;
        uint32_t queue_timeout = furi_ms_to_ticks(50U);
        if(app.mode == IrMouseModeBeacon) {
            int32_t ticks_until_pulse = (int32_t)(app.beacon_next_pulse_tick - furi_get_tick());
            if(ticks_until_pulse <= 0) {
                queue_timeout = 0;
            } else if((uint32_t)ticks_until_pulse < queue_timeout) {
                queue_timeout = (uint32_t)ticks_until_pulse;
            }
        }
        FuriStatus queue_status = furi_message_queue_get(app.event_queue, &event, queue_timeout);
        if(queue_status == FuriStatusErrorTimeout) continue;
        if(queue_status != FuriStatusOk) {
            FURI_LOG_E("IR Mouse", "Could not read application event queue");
            result = -1;
            break;
        }

        if(event.type == IrMouseEventInput) {
            ir_mouse_process_input(&app, &event.data.input);
        } else if(
            event.type == IrMouseEventSignal &&
            event.data.signal.generation == app.rx_generation) {
            ir_mouse_process_signal(&app, &event);
        }
    }

cleanup:
    if(app.beacon_active) {
        furi_hal_infrared_async_tx_stop();
        app.beacon_active = false;
    }
    if(app.worker) {
        if(app.rx_active) {
            infrared_worker_rx_set_received_signal_callback(app.worker, NULL, NULL);
            infrared_worker_rx_stop(app.worker);
            app.rx_active = false;
        }
        infrared_worker_free(app.worker);
    }
    ir_mouse_release_beacon_buttons(&app);

    if(app.ble_profile) {
        if(!ble_profile_hid_mouse_release_all(app.ble_profile)) {
            FURI_LOG_W("IR Mouse", "Could not release BLE mouse buttons");
        }
        if(!ble_profile_hid_kb_release_all(app.ble_profile)) {
            FURI_LOG_W("IR Mouse", "Could not release BLE keyboard keys");
        }
    }
    if(app.bt) {
        bt_set_status_changed_callback(app.bt, NULL, NULL);
        if(app.ble_profile) {
            furi_delay_ms(50);
            if(!bt_profile_restore_default(app.bt)) {
                FURI_LOG_E("IR Mouse", "Failed to restore default Bluetooth profile");
                result = -1;
            }
        }
        furi_record_close(RECORD_BT);
    }

    if(view_port_added) gui_remove_view_port(gui, app.view_port);
    if(gui) furi_record_close(RECORD_GUI);
    if(app.view_port) view_port_free(app.view_port);

    if(app.event_queue) furi_message_queue_free(app.event_queue);
    if(app.exit_event) furi_event_flag_free(app.exit_event);
    if(app.mutex) furi_mutex_free(app.mutex);

    return result;
}

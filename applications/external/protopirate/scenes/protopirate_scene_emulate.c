// scenes/protopirate_scene_emulate.c
#include "../protopirate_app_i.h"
#ifdef ENABLE_EMULATE_FEATURE
#include "../helpers/protopirate_storage.h"
#include "../protocols/protocol_items.h"

#define TAG "ProtoPirateEmulate"

#define MIN_TX_TIME 666
typedef struct {
    uint32_t original_counter;
    uint32_t current_counter;
    uint32_t serial;
    uint8_t original_button;
    FuriString* protocol_name;
    const char* preset;
    FuriString* preset_from_file;
    uint32_t freq;
    FlipperFormat* flipper_format;
    SubGhzTransmitter* transmitter;
    bool is_transmitting;
    bool flag_stop_called;
    Storage* storage;
} EmulateContext;

static EmulateContext* emulate_context = NULL;

#define TX_PRESET_VALUES_AM    8 //Gets 1 added, so is 1 less than actual value.
#define TX_PRESET_VALUES_COUNT 17

//I had to skip the +10dBM and -6dBm Values, use only ones AM/FM have in common.
//Highest Value is 12dBm for AM, 10 for FM. So Menu needs to reflect that.
const uint8_t tx_power_value[TX_PRESET_VALUES_COUNT] = {
    //FM Power Values for 1st PA Table Byte.
    0,
    0xC0, // 10dBm
    0xC8, //7dBm
    0x84, //5dBm
    0x60, //0dBm
    0x34, //-10dBm
    0x1D, //-15dBm
    0x0E, // -20dBm
    0x12, //-30dBm

    //AM Power Values for 1st PA Table Byte.
    0xC0, //12dBm
    0xCD, //7dBm
    0x86, //5dBm
    0x50, //0dBm
    0x26, // -10dBm
    0x1D, // -15dBm
    0x17, //-20dBm
    0x03 //-30dBm
};

static void emulate_context_reset_transmitter(void) {
    if(emulate_context && emulate_context->transmitter) {
        subghz_transmitter_free(emulate_context->transmitter);
        emulate_context->transmitter = NULL;
    }
}

void stop_tx(ProtoPirateApp* app) {
    FURI_LOG_I(TAG, "Stopping transmission");

    // Stop async TX first
    subghz_devices_stop_async_tx(app->txrx->radio_device);

    // Stop the encoder
    if(emulate_context && emulate_context->transmitter) {
        subghz_transmitter_stop(emulate_context->transmitter);
    }

    furi_delay_ms(10);

    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    app->start_tx_time = 0;
    emulate_context_reset_transmitter();

    FURI_LOG_I(TAG, "Transmission stopped, state set to IDLE");
    notification_message(app->notifications, &sequence_blink_stop);
}

static void emulate_context_free(void) {
    if(emulate_context == NULL) return;

    if(emulate_context->transmitter) {
        subghz_transmitter_free(emulate_context->transmitter);
        emulate_context->transmitter = NULL;
    }

    if(emulate_context->flipper_format) {
        flipper_format_free(emulate_context->flipper_format);
        emulate_context->flipper_format = NULL;
    }

    if(emulate_context->protocol_name) {
        furi_string_free(emulate_context->protocol_name);
        emulate_context->protocol_name = NULL;
    }

    if(emulate_context->preset_from_file) {
        furi_string_free(emulate_context->preset_from_file);
        emulate_context->preset_from_file = NULL;
    }

    if(emulate_context->storage) {
        furi_record_close(RECORD_STORAGE);
        emulate_context->storage = NULL;
    }

    free(emulate_context);
    emulate_context = NULL;
}

static bool emulate_context_try_init_transmitter(ProtoPirateApp* app, EmulateContext* ctx) {
    if(ctx->transmitter) {
        return true;
    }
    if(!ctx->flipper_format || !ctx->protocol_name) {
        return false;
    }

    const char* proto_name = furi_string_get_cstr(ctx->protocol_name);
    const char* registry_name = proto_name;
    if(strcmp(proto_name, "Kia V3") == 0) {
        registry_name = "Kia V3/V4";
        FURI_LOG_I(TAG, "Protocol name KiaV3 fixed to Kia V3/V4 for registry");
    } else if(strcmp(proto_name, "Kia V4") == 0) {
        registry_name = "Kia V3/V4";
        FURI_LOG_I(TAG, "Protocol name KiaV4 fixed to Kia V3/V4 for registry");
    }

    const SubGhzProtocol* protocol = NULL;
    for(size_t i = 0; i < protopirate_protocol_registry.size; i++) {
        if(strcmp(protopirate_protocol_registry.items[i]->name, registry_name) == 0) {
            protocol = protopirate_protocol_registry.items[i];
            FURI_LOG_I(TAG, "Found protocol %s in registry at index %zu", registry_name, i);
            break;
        }
    }

    if(!protocol || !protocol->encoder || !protocol->encoder->alloc) {
        FURI_LOG_E(TAG, "Protocol %s has no encoder or not in registry", registry_name);
        return false;
    }

    ctx->transmitter = subghz_transmitter_alloc_init(app->txrx->environment, registry_name);
    if(!ctx->transmitter) {
        FURI_LOG_E(TAG, "Failed to allocate transmitter for %s", registry_name);
        return false;
    }

    flipper_format_rewind(ctx->flipper_format);
    SubGhzProtocolStatus status =
        subghz_transmitter_deserialize(ctx->transmitter, ctx->flipper_format);
    if(status != SubGhzProtocolStatusOk) {
        FURI_LOG_E(TAG, "Failed to deserialize transmitter, status: %d", status);
        subghz_transmitter_free(ctx->transmitter);
        ctx->transmitter = NULL;
        return false;
    }

    FURI_LOG_I(TAG, "Transmitter ready (lazy init)");
    return true;
}

static uint8_t protopirate_get_button_for_protocol(
    const char* protocol,
    InputKey key,
    uint8_t original,
    FlipperFormat* ff) {
    // Kia V7
    if(strcmp(protocol, KIA_PROTOCOL_V7_NAME) == 0) {
        switch(key) {
        case InputKeyUp:
            return 0x01; // Lock
        case InputKeyOk:
            return 0x02; // Unlock
        case InputKeyDown:
            return 0x03; // Trunk
        case InputKeyRight:
            return 0x08; // Panic
        default:
            return original;
        }
    }
    // Kia V0 (Type 1=Kia, 2=Suzuki, 3=Honda V0)
    if(strstr(protocol, "Kia")) {
        uint32_t kia_v0_type = 1;
        if(ff) {
            flipper_format_rewind(ff);
            flipper_format_read_uint32(ff, "Type", &kia_v0_type, 1);
        }
        if(kia_v0_type == 2) {
            switch(key) {
            case InputKeyUp:
                return 0x3; // Lock
            case InputKeyOk:
                return 0x4; // Unlock
            case InputKeyDown:
                return 0x2; // Boot
            case InputKeyLeft:
                return 0x1; // Panic
            case InputKeyRight:
                return original;
            default:
                return original;
            }
        }
        if(kia_v0_type == 3) {
            switch(key) {
            case InputKeyUp:
                return 1;
            case InputKeyOk:
                return 2;
            case InputKeyDown:
                return 3;
            case InputKeyLeft:
                return 4;
            case InputKeyRight:
                return 5;
            default:
                return original;
            }
        }
        switch(key) {
        case InputKeyUp:
            return 0x1; // Lock
        case InputKeyOk:
            return 0x2; // Unlock
        case InputKeyDown:
            return 0x3; // Boot
        case InputKeyLeft:
            return 0x4; // Panic
        case InputKeyRight:
            return 0x8; // Horn/Lights?
        default:
            return original;
        }
    }
    // VAG
    else if(strstr(protocol, "VAG")) {
        if(original == 0x10 || original == 0x20 || original == 0x40) {
            switch(key) {
            case InputKeyUp:
                return 0x20; // Lock
            case InputKeyOk:
                return 0x10; // Unlock
            case InputKeyDown:
                return 0x40; // Boot
            default:
                return original;
            }
        }
        switch(key) {
        case InputKeyUp:
            return 0x2; // Lock
        case InputKeyOk:
            return 0x1; // Unlock
        case InputKeyDown:
            return 0x4; // Boot
        case InputKeyLeft:
            return 0x8; // Panic
        case InputKeyRight:
            return 0x3; // Un+Lk combo
        default:
            return original;
        }
    }
    // Honda Static
    else if(strstr(protocol, "Honda Static")) {
        switch(key) {
        case InputKeyUp:
            return 0x1; // Lock
        case InputKeyOk:
            return 0x2; // Unlock
        case InputKeyDown:
            return 0x4; // Trunk
        case InputKeyRight:
            return 0x5; // Remote Start
        case InputKeyLeft:
            return 0x8; // Panic
        default:
            return original;
        }
    }
    // Mazda V0
    else if(strstr(protocol, "Mazda")) {
        switch(key) {
        case InputKeyUp:
            return 0x01; // Lock
        case InputKeyOk:
            return 0x02; // Unlock
        case InputKeyDown:
            return 0x04; // Trunk
        case InputKeyRight:
            return 0x08; // Remote
        default:
            return original;
        }
    }
    // Ford - (needs testing)
    else if(strstr(protocol, "Ford")) {
        if(strstr(protocol, FORD_PROTOCOL_V1_NAME)) {
            switch(key) {
            case InputKeyUp:
                return 0x1; // Lock
            case InputKeyOk:
                return 0x2; // Unlock
            case InputKeyDown:
                return 0x4; // Trunk
            case InputKeyLeft:
                return 0x8; // Panic
            case InputKeyRight:
            default:
                return original;
            }
        }
        switch(key) {
        case InputKeyLeft:
            return 0x1; // Panic
        case InputKeyUp:
            return 0x2; // Lock
        case InputKeyOk:
            return 0x4; // Unlock
        case InputKeyDown:
            return 0x8; // Boot
        case InputKeyRight:
            return 0x10; // There is no 10 (Unless other vehicles?)
        default:
            return original;
        }
    }
    // Subaru - (needs testing)
    else if(strstr(protocol, "Subaru")) {
        switch(key) {
        case InputKeyUp:
            return 0x1; // Lock?
        case InputKeyOk:
            return 0x2; // Unlock?
        case InputKeyDown:
            return 0x3; // Boot?
        case InputKeyLeft:
            return 0x4; // Panic?
        case InputKeyRight:
            return 0x8; // ?
        default:
            return original;
        }
    }

    return original;
}

static bool protopirate_emulate_update_data(EmulateContext* ctx, uint8_t button) {
    if(!ctx || !ctx->flipper_format) return false;

    // Update button and counter in the flipper format
    flipper_format_rewind(ctx->flipper_format);

    // Update button
    uint32_t btn_value = button;
    flipper_format_insert_or_update_uint32(ctx->flipper_format, "Btn", &btn_value, 1);
    FURI_LOG_I(TAG, "Updated flipper format - Btn: 0x%02X", button);

    flipper_format_insert_or_update_uint32(ctx->flipper_format, "Cnt", &ctx->current_counter, 1);
    FURI_LOG_I(TAG, "Updated flipper format - Cnt: 0x%03lX", (unsigned long)ctx->current_counter);

    return true;
}

static void protopirate_emulate_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    if(!emulate_context) return;

    static uint8_t animation_frame = 0;
    animation_frame = (animation_frame + 1) % 8;

    canvas_clear(canvas);

    // Header bar
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontSecondary);
    const char* proto_name = furi_string_get_cstr(emulate_context->protocol_name);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, proto_name);
    canvas_invert_color(canvas);

    // Info section
    canvas_set_font(canvas, FontSecondary);

    // Serial - left aligned
    char info_str[32];
    snprintf(info_str, sizeof(info_str), "SN:%08lX", (unsigned long)emulate_context->serial);
    canvas_draw_str(canvas, 2, 20, info_str);

    snprintf(
        info_str,
        sizeof(info_str),
        "F:%lu.%02lu",
        emulate_context->freq / 1000000,
        (emulate_context->freq % 1000000) / 10000);
    canvas_draw_str(canvas, 2, 30, info_str);

    // Counter - left aligned
    snprintf(
        info_str, sizeof(info_str), "CNT:%04lX", (unsigned long)emulate_context->current_counter);
    canvas_draw_str(canvas, 68, 20, info_str);

    // Increment on right if changed
    if(emulate_context->current_counter > emulate_context->original_counter) {
        snprintf(
            info_str,
            sizeof(info_str),
            "+%ld",
            (long)(emulate_context->current_counter - emulate_context->original_counter));
        canvas_draw_str(canvas, 112, 20, info_str);
    }

    snprintf(info_str, sizeof(info_str), "%s", emulate_context->preset);
    canvas_draw_str(canvas, 95, 30, info_str);

    // Divider
    //canvas_draw_line(canvas, 0, 34, 127, 34);

    // Button mapping - adjusted positioning
    canvas_set_font(canvas, FontSecondary);

    // OK in Centre
    char* unlock_text = "UNLOCK";
    uint16_t width_button = canvas_string_width(canvas, unlock_text) + 8;
    uint16_t height_button = canvas_current_font_height(canvas);
    canvas_draw_rbox(
        canvas, 64 - (width_button / 2), 45 - (height_button / 2), width_button, height_button, 3);
    canvas_invert_color(canvas); //Switch to white
    canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, unlock_text);
    canvas_invert_color(canvas); // Back to Black

    // Row 1
    char* panic_text = "PANIC";
    width_button = canvas_string_width(canvas, panic_text) + 8;
    canvas_draw_rbox(
        canvas, 64 - (width_button / 2), 33 - (height_button / 2), width_button, height_button, 3);
    canvas_invert_color(canvas); //Switch to white
    canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignBottom, "LOCK");
    canvas_invert_color(canvas); // Back to Black

    // Left Centre Row
    canvas_draw_rbox(canvas, 0, 46 - (height_button / 2), width_button, height_button, 3);
    canvas_invert_color(canvas); //Switch to white
    canvas_draw_str_aligned(canvas, (width_button / 2), 50, AlignCenter, AlignBottom, panic_text);
    canvas_invert_color(canvas); // Back to Black

    // Right Centre Row
    canvas_draw_rbox(
        canvas, 127 - width_button, 46 - (height_button / 2), width_button, height_button, 3);
    canvas_invert_color(canvas); //Switch to white
    canvas_draw_str_aligned(canvas, 127 - (width_button / 2), 50, AlignCenter, AlignBottom, "XXX");
    canvas_invert_color(canvas); // Back to Black

    // Row 3
    canvas_draw_rbox(
        canvas, 64 - (width_button / 2), 57 - (height_button / 2), width_button, height_button, 3);
    canvas_invert_color(canvas); //Switch to white
    canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, "BOOT");
    canvas_invert_color(canvas); // Back to Black

    // Transmitting overlay
    if(emulate_context->is_transmitting) {
        // TX box
        canvas_draw_rbox(canvas, 24, 18, 80, 18, 3);
        canvas_invert_color(canvas);

        // Waves
        int wave = animation_frame % 3;
        canvas_draw_str(canvas, 28 + wave * 2, 25, ")))");

        // Text
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "TX");

        canvas_invert_color(canvas);
    }
}

static bool protopirate_emulate_input_callback(InputEvent* event, void* context) {
    ProtoPirateApp* app = context;
    EmulateContext* ctx = emulate_context;

    if(!ctx) return false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyBack) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventEmulateExit);
            return true;
        }

        // Get button mapping for this key
        uint8_t button = protopirate_get_button_for_protocol(
            furi_string_get_cstr(ctx->protocol_name),
            event->key,
            ctx->original_button,
            ctx->flipper_format);

        // Update data with new button and counter
        ctx->current_counter++;
        protopirate_emulate_update_data(ctx, button);

        // Start transmission - user can hold as long as they want
        ctx->is_transmitting = true;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventEmulateTransmit);

        return true;
    } else if(event->type == InputTypeRelease) {
        // Stop transmission immediately on release
        if(ctx && ctx->is_transmitting) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventEmulateStop);
            return true;
        }
        return false;
    }

    return false;
}

void protopirate_scene_emulate_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(emulate_context != NULL) {
        FURI_LOG_W(TAG, "Previous emulate context not freed, cleaning up");
        emulate_context_free();
    }

    if(app->txrx && app->txrx->history) {
        protopirate_history_release_scratch(app->txrx->history);
    }

    protopirate_rx_stack_suspend_for_tx(app);

    // Create emulate context
    emulate_context = malloc(sizeof(EmulateContext));
    if(!emulate_context) {
        FURI_LOG_E(TAG, "Failed to allocate emulate context");
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    memset(emulate_context, 0, sizeof(EmulateContext));

    emulate_context->protocol_name = furi_string_alloc();
    if(!emulate_context->protocol_name) {
        FURI_LOG_E(TAG, "Failed to allocate protocol name string");
        emulate_context_free();
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    // Load the file
    if(app->loaded_file_path) {
        // Open storage once and keep track of it
        emulate_context->storage = furi_record_open(RECORD_STORAGE);
        if(!emulate_context->storage) {
            FURI_LOG_E(TAG, "Failed to open storage");
            emulate_context_free();
            notification_message(app->notifications, &sequence_error);
            scene_manager_previous_scene(app->scene_manager);
            return;
        }

        emulate_context->flipper_format = flipper_format_file_alloc(emulate_context->storage);
        if(!emulate_context->flipper_format) {
            FURI_LOG_E(TAG, "Failed to allocate FlipperFormat");
            emulate_context_free();
            notification_message(app->notifications, &sequence_error);
            scene_manager_previous_scene(app->scene_manager);
            return;
        }

        if(!flipper_format_file_open_existing(
               emulate_context->flipper_format, furi_string_get_cstr(app->loaded_file_path))) {
            FURI_LOG_E(
                TAG, "Failed to open file: %s", furi_string_get_cstr(app->loaded_file_path));
            emulate_context_free();
            notification_message(app->notifications, &sequence_error);
            scene_manager_previous_scene(app->scene_manager);
            return;
        }

        // Read frequency and preset from the saved file
        uint32_t frequency = 433920000;
        FuriString* preset_str = furi_string_alloc();

        flipper_format_rewind(emulate_context->flipper_format);
        if(!flipper_format_read_uint32(
               emulate_context->flipper_format, "Frequency", &frequency, 1)) {
            FURI_LOG_W(TAG, "Failed to read frequency, using default 433.92MHz");
        }

        flipper_format_rewind(emulate_context->flipper_format);
        if(!flipper_format_read_string(emulate_context->flipper_format, "Preset", preset_str)) {
            FURI_LOG_W(TAG, "Failed to read preset, using AM650");
            furi_string_set(preset_str, "AM650");
        }

        emulate_context->preset_from_file = furi_string_alloc();
        furi_string_set(emulate_context->preset_from_file, preset_str);

        // Convert full preset name to short name
        emulate_context->preset = preset_name_to_short(furi_string_get_cstr(preset_str));
        FURI_LOG_I(
            TAG,
            "Using frequency %lu Hz, preset %s (from %s)",
            (unsigned long)frequency,
            furi_string_get_cstr(emulate_context->preset_from_file),
            emulate_context->preset);
        emulate_context->freq = frequency;
        furi_string_free(preset_str);

        // Read protocol name
        flipper_format_rewind(emulate_context->flipper_format);
        if(!flipper_format_read_string(
               emulate_context->flipper_format, "Protocol", emulate_context->protocol_name)) {
            FURI_LOG_E(TAG, "Failed to read protocol name");
            furi_string_set(emulate_context->protocol_name, "Unknown");
        }

        // Standalone Suzuki captures: merged into Kia V0 Type 2
        if(furi_string_equal(emulate_context->protocol_name, "Suzuki")) {
            uint32_t type_suzuki = 2;
            furi_string_set(emulate_context->protocol_name, KIA_PROTOCOL_V0_NAME);
            flipper_format_rewind(emulate_context->flipper_format);
            flipper_format_insert_or_update_string_cstr(
                emulate_context->flipper_format, "Protocol", KIA_PROTOCOL_V0_NAME);
            flipper_format_insert_or_update_uint32(
                emulate_context->flipper_format, "Type", &type_suzuki, 1);
        }

        // Read serial
        flipper_format_rewind(emulate_context->flipper_format);
        if(!flipper_format_read_uint32(
               emulate_context->flipper_format, "Serial", &emulate_context->serial, 1)) {
            FURI_LOG_W(TAG, "Failed to read serial");
            emulate_context->serial = 0;
        }

        // Read original button
        flipper_format_rewind(emulate_context->flipper_format);
        uint32_t btn_temp = 0;
        if(flipper_format_read_uint32(emulate_context->flipper_format, "Btn", &btn_temp, 1)) {
            emulate_context->original_button = (uint8_t)btn_temp;
        }

        // Read counter
        flipper_format_rewind(emulate_context->flipper_format);
        if(flipper_format_read_uint32(
               emulate_context->flipper_format, "Cnt", &emulate_context->original_counter, 1)) {
            emulate_context->current_counter = emulate_context->original_counter;
        }

    } else {
        FURI_LOG_E(TAG, "No file path set");
        emulate_context_free();
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    // Set up view
    view_set_draw_callback(app->view_about, protopirate_emulate_draw_callback);
    view_set_input_callback(app->view_about, protopirate_emulate_input_callback);
    view_set_context(app->view_about, app);
    view_set_previous_callback(app->view_about, NULL);

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewAbout);
}

uint8_t get_tx_preset_byte(uint8_t* preset_data) {
#define MAX_PRESET_SIZE 128
    uint8_t offset = 0;
    while(preset_data[offset] && (offset < MAX_PRESET_SIZE)) {
        offset += 2;
    }
    return (!preset_data[offset] ? offset + 2 : 0);
}

bool protopirate_scene_emulate_on_event(void* context, SceneManagerEvent event) {
#define INVALID_PRESET         "Cannot set TX power on this preset."
#define CUSTOM_PRESET_DATA_KEY "Custom_preset_data"
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ProtoPirateCustomEventEmulateTransmit:
            if(emulate_context && emulate_context->flipper_format) {
                // Stop any ongoing transmission FIRST
                if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
                    FURI_LOG_W(TAG, "Previous transmission still active, stopping it");
                    subghz_devices_stop_async_tx(app->txrx->radio_device);
                    subghz_transmitter_stop(emulate_context->transmitter);
                    furi_delay_ms(10);
                    subghz_devices_idle(app->txrx->radio_device);
                    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
                }

                emulate_context_reset_transmitter();

                if(!emulate_context_try_init_transmitter(app, emulate_context)) {
                    FURI_LOG_E(TAG, "No transmitter available");
                    emulate_context->is_transmitting = false;
                    notification_message(app->notifications, &sequence_error);
                    consumed = true;
                    break;
                }

                // Re-deserialize with updated values
                flipper_format_rewind(emulate_context->flipper_format);
                SubGhzProtocolStatus status = subghz_transmitter_deserialize(
                    emulate_context->transmitter, emulate_context->flipper_format);

                if(status != SubGhzProtocolStatusOk) {
                    FURI_LOG_E(TAG, "Failed to re-deserialize transmitter: %d", status);
                    emulate_context_reset_transmitter();
                    emulate_context->is_transmitting = false;
                    notification_message(app->notifications, &sequence_error);
                    consumed = true;
                    break;
                }

                //Preset Loading
                uint8_t* preset_data = NULL;
                bool free_custom_data = false;

                //Use the Custom Preset data from the file, if we have it.
                uint32_t uint32_array_size;
                if(strcmp(emulate_context->preset, "Custom") == 0) {
                    flipper_format_rewind(emulate_context->flipper_format);
                    if(flipper_format_get_value_count(
                           emulate_context->flipper_format,
                           CUSTOM_PRESET_DATA_KEY,
                           &uint32_array_size) &&
                       uint32_array_size > 0 && uint32_array_size < 1024) {
                        preset_data = malloc(uint32_array_size);
                        free_custom_data = true;
                        if(!flipper_format_read_hex(
                               emulate_context->flipper_format,
                               CUSTOM_PRESET_DATA_KEY,
                               preset_data,
                               uint32_array_size)) {
                            FURI_LOG_W(TAG, "Custom Preset not Loaded, trying AM650");
                            free(preset_data);
                            free_custom_data = false;
                            preset_data =
                                subghz_setting_get_preset_data_by_name(app->setting, "AM650");
                            emulate_context->preset = "AM650";
                        }
                    }
                } else {
                    //NOT A CUStoM PRESET
                    // Get preset data with fallback chain
                    preset_data = subghz_setting_get_preset_data_by_name(
                        app->setting, emulate_context->preset);
                }

                if(!preset_data) {
                    FURI_LOG_W(TAG, "Preset %s not found, trying AM650", emulate_context->preset);
                    preset_data = subghz_setting_get_preset_data_by_name(app->setting, "AM650");
                    emulate_context->preset = "AM650";
                }
                if(!preset_data) {
                    FURI_LOG_W(TAG, "AM650 not found, trying FM476");
                    preset_data = subghz_setting_get_preset_data_by_name(app->setting, "FM476");
                    emulate_context->preset = "FM476";
                }

                if(preset_data) {
                    if(app->tx_power) {
                        //Grab the start of the PA table for this Preset.
                        uint8_t preset_offset = 0;
                        preset_offset = get_tx_preset_byte(preset_data);

                        //Grab the AM and FM byte now, so we can do proper checks.
                        uint8_t fm_byte = preset_data[preset_offset];
                        uint8_t am_byte = preset_data[preset_offset + 1];

                        if(fm_byte && am_byte) {
                            FURI_LOG_I(TAG, INVALID_PRESET);
                        } else if(fm_byte) {
                            FURI_LOG_I(TAG, "FM PA table found.");
                            preset_data[preset_offset] = tx_power_value[app->tx_power];
                        } else if(am_byte) {
                            FURI_LOG_I(TAG, "AM PA table found.");
                            preset_data[preset_offset + 1] =
                                tx_power_value[TX_PRESET_VALUES_AM + app->tx_power];
                        } else {
                            FURI_LOG_I(TAG, INVALID_PRESET);
                        }
                    }

                    // Configure radio for TX
                    subghz_devices_reset(app->txrx->radio_device);
                    subghz_devices_idle(app->txrx->radio_device);
                    subghz_devices_load_preset(
                        app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
                    subghz_devices_set_frequency(app->txrx->radio_device, emulate_context->freq);

                    // Start transmission
                    subghz_devices_set_tx(app->txrx->radio_device);
                    app->start_tx_time = furi_get_tick();

                    if(subghz_devices_start_async_tx(
                           app->txrx->radio_device,
                           subghz_transmitter_yield,
                           emulate_context->transmitter)) {
                        app->txrx->txrx_state = ProtoPirateTxRxStateTx;
                        notification_message(app->notifications, &sequence_tx);
                        notification_message(app->notifications, &sequence_blink_magenta_10);
                        FURI_LOG_I(
                            TAG,
                            "Started transmission: freq=%lu file_Preset=\"%s\" short=\"%s\"",
                            (unsigned long)emulate_context->freq,
                            emulate_context->preset_from_file ?
                                furi_string_get_cstr(emulate_context->preset_from_file) :
                                "?",
                            emulate_context->preset);
                    } else {
                        FURI_LOG_E(TAG, "Failed to start async TX");
                        emulate_context_reset_transmitter();
                        emulate_context->is_transmitting = false;
                        subghz_devices_idle(app->txrx->radio_device);
                        notification_message(app->notifications, &sequence_error);
                    }
                } else {
                    FURI_LOG_E(TAG, "No preset data available - cannot transmit");
                    emulate_context->is_transmitting = false;
                    notification_message(app->notifications, &sequence_error);
                }

                if(free_custom_data)
                    free(preset_data); //We have used the preset, I alloced it I have to free.
            }
            consumed = true;
            break;

        case ProtoPirateCustomEventEmulateStop:
            FURI_LOG_I(TAG, "Stop event received, txrx_state=%d", app->txrx->txrx_state);

            if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
                if((furi_get_tick() - app->start_tx_time) > MIN_TX_TIME) {
                    stop_tx(app);
                    emulate_context->is_transmitting = false;
                } else {
                    emulate_context->flag_stop_called = true;
                }
            }
            consumed = true;
            break;

        case ProtoPirateCustomEventEmulateExit:
            if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
                stop_tx(app);
                emulate_context->is_transmitting = false;
                emulate_context->flag_stop_called = false;
            }

            //If we came in from emulate, exit the app, otherwise go back to Saved or Received Info
            if(scene_manager_has_previous_scene(app->scene_manager, ProtoPirateSceneStart))
                scene_manager_previous_scene(app->scene_manager);
            else {
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Update display (causes ViewPort lockup warning but works)
        view_commit_model(app->view_about, true);

        if(emulate_context && emulate_context->is_transmitting) {
            if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
                //Are we supposed to be stopping the TX from the MIN_TX
                if((app->start_tx_time &&
                    ((furi_get_tick() - app->start_tx_time) > MIN_TX_TIME)) &&
                   emulate_context->flag_stop_called) {
                    stop_tx(app);
                    emulate_context->is_transmitting = false;
                    emulate_context->flag_stop_called = false;
                } else {
                    notification_message(app->notifications, &sequence_blink_magenta_10);
                }
            }
        }

        consumed = true;
    }

    return consumed;
}

void protopirate_scene_emulate_on_exit(void* context) {
    ProtoPirateApp* app = context;

    // Stop any active transmission
    if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
        FURI_LOG_I(TAG, "Stopping transmission on exit");

        subghz_devices_stop_async_tx(app->txrx->radio_device);

        if(emulate_context && emulate_context->transmitter) {
            subghz_transmitter_stop(emulate_context->transmitter);
        }

        furi_delay_ms(10);

        subghz_devices_idle(app->txrx->radio_device);
        app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    } else if(app->txrx->txrx_state != ProtoPirateTxRxStateIDLE) {
        protopirate_idle(app);
    }

    // Free emulate context and all its resources
    emulate_context_free();

    // Delete temp file if we were using one
    protopirate_storage_delete_temp();

    notification_message(app->notifications, &sequence_blink_stop);

    // Clear view callbacks
    view_set_draw_callback(app->view_about, NULL);
    view_set_input_callback(app->view_about, NULL);
    view_set_context(app->view_about, NULL);
}
#endif

#include "../weather_station_app_i.h"
#include <dialogs/dialogs.h>
#include <flipper_format/flipper_format_i.h>
#include <toolbox/path.h>
#include <toolbox/stream/stream.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#define WS_RAW_MAX_LINE_BYTES 8192U
#define WS_RAW_PULSE_SPACES   " \t\r\n"

static void ws_scene_read_raw_clear(WeatherStationApp* app) {
    ws_history_reset(app->txrx->history);
    free(app->raw_preset_data);
    app->raw_preset_data = NULL;
}

// Preflight with a fixed buffer before any FlipperFormat string or line reads.
static bool ws_scene_read_raw_lines_fit(Stream* stream) {
    uint8_t chunk[128];
    size_t line_bytes = 0;
    size_t count;
    while((count = stream_read(stream, chunk, sizeof(chunk))) != 0) {
        for(size_t i = 0; i < count; i++) {
            if(chunk[i] == '\n') {
                line_bytes = 0;
            } else if(++line_bytes > WS_RAW_MAX_LINE_BYTES) {
                return false;
            }
        }
    }
    return stream_rewind(stream);
}

static bool
    ws_scene_read_raw_parse_pulse(const char* start, char** end, uint32_t* duration, bool* level) {
    errno = 0;
    const long value = strtol(start, end, 10);
    if(*end == start || !strchr(WS_RAW_PULSE_SPACES, **end) || errno == ERANGE ||
       value < -INT32_MAX || value > INT32_MAX) {
        return false;
    }
    *level = value > 0;
    *duration = (uint32_t)(value < 0 ? -value : value);
    return true;
}

typedef struct {
    WeatherStationApp* app;
    SubGhzRadioPreset preset;
} WSReadRawContext;

static void ws_scene_read_raw_rx_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    WSReadRawContext* ctx = context;
    ws_history_add_to_history(ctx->app->txrx->history, decoder_base, &ctx->preset);
    subghz_receiver_reset(receiver);
}

static void ws_scene_read_raw_show_message(const char* text) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, "Read RAW", 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(dialogs, message);
    dialog_message_free(message);
    furi_record_close(RECORD_DIALOGS);
}

// Feeds every RAW_Data sample through the app's decoders. Returns NULL on success, or a message
// to show when the file cannot be read as a RAW recording.
static const char* ws_scene_read_raw_decode(WeatherStationApp* app, const char* path) {
    ws_scene_read_raw_clear(app);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* str = furi_string_alloc();
    WSReadRawContext ctx = {.app = app, .preset = {.name = furi_string_alloc()}};
    const char* error = NULL;

    do {
        uint32_t version = 0;
        if(!flipper_format_file_open_existing(ff, path)) {
            error = "Cannot read\nthis file";
            break;
        }
        Stream* stream = flipper_format_get_raw_stream(ff);
        if(!ws_scene_read_raw_lines_fit(stream)) {
            error = "RAW line too long\nor unreadable";
            break;
        }
        if(!flipper_format_read_header(ff, str, &version)) {
            error = "Cannot read\nthis file";
            break;
        }
        if(furi_string_cmp_str(str, SUBGHZ_RAW_FILE_TYPE) != 0 ||
           version != SUBGHZ_RAW_FILE_VERSION) {
            error = "Not a supported\nRAW recording";
            break;
        }
        if(!flipper_format_read_uint32(ff, "Frequency", &ctx.preset.frequency, 1) ||
           !flipper_format_read_string(ff, "Preset", ctx.preset.name)) {
            error = "Cannot read\nthis file";
            break;
        }
        if(furi_string_cmp_str(ctx.preset.name, "FuriHalSubGhzPresetCustom") == 0) {
            uint32_t size = 0;
            if(!flipper_format_read_string(ff, "Custom_preset_module", str) ||
               furi_string_cmp_str(str, "CC1101") != 0 ||
               !flipper_format_get_value_count(ff, "Custom_preset_data", &size) || size == 0 ||
               size > UINT16_MAX) {
                error = "Invalid custom\nradio preset";
                break;
            }
            app->raw_preset_data = malloc(size);
            if(!app->raw_preset_data ||
               !flipper_format_read_hex(ff, "Custom_preset_data", app->raw_preset_data, size)) {
                error = "Cannot read\ncustom preset";
                break;
            }
            ctx.preset.data = app->raw_preset_data;
            ctx.preset.data_size = size;
        }
        if(!flipper_format_read_string(ff, "Protocol", str) ||
           furi_string_cmp_str(str, "RAW") != 0) {
            error = "Not a RAW\nrecording";
            break;
        }

        subghz_receiver_reset(app->txrx->receiver);
        subghz_receiver_set_rx_callback(app->txrx->receiver, ws_scene_read_raw_rx_callback, &ctx);

        while(stream_read_line(stream, str)) {
            if(!furi_string_start_with_str(str, "RAW_Data:")) continue;
            const char* p = furi_string_get_cstr(str) + strlen("RAW_Data:");
            p += strspn(p, WS_RAW_PULSE_SPACES);
            while(*p) {
                char* end;
                uint32_t duration;
                bool level;
                if(!ws_scene_read_raw_parse_pulse(p, &end, &duration, &level)) {
                    error = "Invalid RAW\npulse duration";
                    break;
                }
                if(duration != 0) subghz_receiver_decode(app->txrx->receiver, level, duration);
                p = end + strspn(end, WS_RAW_PULSE_SPACES);
            }
            if(error) break;
        }
        // A long silence lets decoders that wait for the end of a frame finish the last one
        if(!error) subghz_receiver_decode(app->txrx->receiver, false, 100000);
        subghz_receiver_set_rx_callback(app->txrx->receiver, NULL, NULL);

        // Show the age of each reading from the recording time, not from this decode
        uint32_t ts = 0;
        if(!error && storage_common_timestamp(storage, path, &ts) == FSE_OK) {
            for(uint16_t i = 0; i < ws_history_get_item(app->txrx->history); i++) {
                FlipperFormat* data = ws_history_get_raw_data(app->txrx->history, i);
                flipper_format_rewind(data);
                flipper_format_update_uint32(data, "Ts", &ts, 1);
            }
        }
    } while(false);

    furi_string_free(ctx.preset.name);
    furi_string_free(str);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return error;
}

static void ws_scene_read_raw_submenu_callback(void* context, uint32_t index) {
    WeatherStationApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void weather_station_scene_read_raw_on_enter(void* context) {
    WeatherStationApp* app = context;

    if(!scene_manager_get_scene_state(app->scene_manager, WeatherStationSceneReadRaw)) {
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Decoding...");
        view_dispatcher_switch_to_view(app->view_dispatcher, WeatherStationViewWidget);

        const char* error = ws_scene_read_raw_decode(app, furi_string_get_cstr(app->file_path));
        widget_reset(app->widget);
        if(!error && ws_history_get_item(app->txrx->history) == 0) {
            error = "No weather data\nfound in this file";
        }
        if(error) {
            ws_scene_read_raw_clear(app);
            ws_scene_read_raw_show_message(error);
            scene_manager_previous_scene(app->scene_manager);
            return;
        }
        app->txrx->idx_menu_chosen = 0;
        scene_manager_set_scene_state(app->scene_manager, WeatherStationSceneReadRaw, 1);
    }

    Submenu* submenu = app->submenu;
    FuriString* str = furi_string_alloc();
    path_extract_filename(app->file_path, str, true);
    submenu_set_header(submenu, furi_string_get_cstr(str));
    for(uint16_t i = 0; i < ws_history_get_item(app->txrx->history); i++) {
        ws_history_get_text_item_menu(app->txrx->history, str, i);
        submenu_add_item(
            submenu, furi_string_get_cstr(str), i, ws_scene_read_raw_submenu_callback, app);
    }
    furi_string_free(str);
    submenu_set_selected_item(submenu, app->txrx->idx_menu_chosen);

    view_dispatcher_switch_to_view(app->view_dispatcher, WeatherStationViewSubmenu);
}

bool weather_station_scene_read_raw_on_event(void* context, SceneManagerEvent event) {
    WeatherStationApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        app->txrx->idx_menu_chosen = event.event;
        scene_manager_next_scene(app->scene_manager, WeatherStationSceneReceiverInfo);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_set_scene_state(app->scene_manager, WeatherStationSceneReadRaw, 0);
        ws_scene_read_raw_clear(app);
    }
    return false;
}

void weather_station_scene_read_raw_on_exit(void* context) {
    WeatherStationApp* app = context;
    submenu_reset(app->submenu);
}

/**
 * TV Off - sends every TV "Power" code from the universal remote dictionary
 * as soon as the app is launched.
 *
 * Codes come from the stock dictionary on the SD card
 * (/ext/infrared/assets/tv.ir). A custom list can be placed at
 * /ext/apps_data/tv_off/codes.ir and takes priority when present.
 */
#include <furi.h>
#include <furi_hal_infrared.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <notification/notification_messages.h>
#include <infrared.h>
#include <infrared_transmit.h>

#define TAG "TvOff"

#define TV_OFF_STOCK_DB    EXT_PATH("infrared/assets/tv.ir")
#define TV_OFF_CUSTOM_DB   APP_DATA_PATH("codes.ir")
#define TV_OFF_BUTTON_NAME "Power"
#define TV_OFF_MAX_TIMINGS 1024U
#define TV_OFF_GAP_MS      50U

typedef enum {
    TvOffStateLoading,
    TvOffStateSending,
    TvOffStatePaused,
    TvOffStateDone,
    TvOffStateError,
} TvOffState;

typedef struct {
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    Storage* storage;
    FlipperFormat* ff;
    FuriString* name;
    FuriString* tmp;
    uint32_t* timings;

    // Shared with the draw callback, guarded by mutex
    TvOffState state;
    uint32_t total;
    uint32_t sent;
} TvOff;

static void tv_off_set_state(TvOff* app, TvOffState state) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = state;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void tv_off_draw_callback(Canvas* canvas, void* context) {
    TvOff* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const TvOffState state = app->state;
    const uint32_t total = app->total;
    const uint32_t sent = app->sent;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "TV Off");
    canvas_set_font(canvas, FontSecondary);

    if(state == TvOffStateLoading) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Loading codes...");
        return;
    }

    if(state == TvOffStateError) {
        canvas_draw_str_aligned(
            canvas, 64, 28, AlignCenter, AlignCenter, "No TV power codes found");
        canvas_draw_str_aligned(
            canvas, 64, 42, AlignCenter, AlignCenter, "Update SD card databases");
        return;
    }

    const float progress = total ? (float)sent / (float)total : 0.0f;
    elements_progress_bar(canvas, 4, 17, 120, progress);

    char line[32];
    const char* status = "Sending";
    const char* button = "Pause";
    if(state == TvOffStatePaused) {
        status = "Paused";
        button = "Resume";
    } else if(state == TvOffStateDone) {
        status = "Done";
        button = "Again";
    }
    snprintf(line, sizeof(line), "%s %lu/%lu", status, sent, total);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, line);
    elements_button_center(canvas, button);
}

static void tv_off_input_callback(InputEvent* event, void* context) {
    TvOff* app = context;
    furi_message_queue_put(app->queue, event, FuriWaitForever);
}

static bool tv_off_open_db(TvOff* app) {
    const char* path = storage_file_exists(app->storage, TV_OFF_CUSTOM_DB) ? TV_OFF_CUSTOM_DB :
                                                                             TV_OFF_STOCK_DB;
    FURI_LOG_I(TAG, "Using %s", path);

    if(!flipper_format_buffered_file_open_existing(app->ff, path)) return false;

    uint32_t version = 0;
    if(!flipper_format_read_header(app->ff, app->tmp, &version)) return false;

    return furi_string_equal(app->tmp, "IR library file") ||
           furi_string_equal(app->tmp, "IR signals file");
}

static bool tv_off_is_power(const FuriString* name) {
    return furi_string_cmpi_str(name, TV_OFF_BUTTON_NAME) == 0;
}

static uint32_t tv_off_count(TvOff* app) {
    uint32_t count = 0;
    while(flipper_format_read_string(app->ff, "name", app->name)) {
        if(tv_off_is_power(app->name)) count++;
    }
    flipper_format_rewind(app->ff);
    return count;
}

static void tv_off_send_parsed(TvOff* app) {
    InfraredMessage message = {0};

    if(!flipper_format_read_string(app->ff, "protocol", app->tmp)) return;
    message.protocol = infrared_get_protocol_by_name(furi_string_get_cstr(app->tmp));
    if(!infrared_is_protocol_valid(message.protocol)) return;

    if(!flipper_format_read_hex(app->ff, "address", (uint8_t*)&message.address, 4)) return;
    if(!flipper_format_read_hex(app->ff, "command", (uint8_t*)&message.command, 4)) return;

    const uint32_t address_mask =
        (1UL << infrared_get_protocol_address_length(message.protocol)) - 1;
    const uint32_t command_mask =
        (1UL << infrared_get_protocol_command_length(message.protocol)) - 1;
    if(message.address != (message.address & address_mask)) return;
    if(message.command != (message.command & command_mask)) return;

    infrared_send(&message, 1);
}

static void tv_off_send_raw(TvOff* app) {
    uint32_t frequency = 0;
    float duty_cycle = 0;
    uint32_t count = 0;

    if(!flipper_format_read_uint32(app->ff, "frequency", &frequency, 1)) return;
    if(!flipper_format_read_float(app->ff, "duty_cycle", &duty_cycle, 1)) return;
    if(!flipper_format_get_value_count(app->ff, "data", &count)) return;

    if(frequency < INFRARED_MIN_FREQUENCY || frequency > INFRARED_MAX_FREQUENCY) return;
    if(duty_cycle <= 0.0f || duty_cycle > 1.0f) return;
    if(count == 0 || count > TV_OFF_MAX_TIMINGS) return;

    if(!flipper_format_read_uint32(app->ff, "data", app->timings, count)) return;

    infrared_send_raw_ext(app->timings, count, true, frequency, duty_cycle);
}

/** Send the next Power code. Returns false when the dictionary is exhausted. */
static bool tv_off_send_next(TvOff* app) {
    while(flipper_format_read_string(app->ff, "name", app->name)) {
        if(!tv_off_is_power(app->name)) continue;

        notification_message(app->notifications, &sequence_blink_magenta_10);

        if(flipper_format_read_string(app->ff, "type", app->tmp)) {
            if(furi_string_equal(app->tmp, "parsed")) {
                tv_off_send_parsed(app);
            } else if(furi_string_equal(app->tmp, "raw")) {
                tv_off_send_raw(app);
            }
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->sent++;
        furi_mutex_release(app->mutex);

        furi_delay_ms(TV_OFF_GAP_MS);
        return true;
    }
    return false;
}

static void tv_off_restart(TvOff* app) {
    flipper_format_rewind(app->ff);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->sent = 0;
    furi_mutex_release(app->mutex);
    tv_off_set_state(app, TvOffStateSending);
}

static TvOff* tv_off_alloc(void) {
    TvOff* app = malloc(sizeof(TvOff));

    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->name = furi_string_alloc();
    app->tmp = furi_string_alloc();
    app->timings = malloc(sizeof(uint32_t) * TV_OFF_MAX_TIMINGS);
    app->state = TvOffStateLoading;

    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->ff = flipper_format_buffered_file_alloc(app->storage);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, tv_off_draw_callback, app);
    view_port_input_callback_set(app->view_port, tv_off_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static void tv_off_free(TvOff* app) {
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    flipper_format_free(app->ff);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);

    free(app->timings);
    furi_string_free(app->tmp);
    furi_string_free(app->name);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->queue);
    free(app);
}

int32_t tv_off_app(void* p) {
    UNUSED(p);
    TvOff* app = tv_off_alloc();

    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    view_port_update(app->view_port);

    uint32_t total = 0;
    if(tv_off_open_db(app)) total = tv_off_count(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->total = total;
    furi_mutex_release(app->mutex);
    tv_off_set_state(app, total ? TvOffStateSending : TvOffStateError);

    bool running = true;
    while(running) {
        // While sending, only poll for input; otherwise sleep until a key is pressed
        const uint32_t timeout = (app->state == TvOffStateSending) ? 0 : FuriWaitForever;

        InputEvent event;
        if(furi_message_queue_get(app->queue, &event, timeout) == FuriStatusOk) {
            if(event.type != InputTypeShort && event.type != InputTypeLong) continue;

            if(event.key == InputKeyBack) {
                running = false;
            } else if(event.key == InputKeyOk) {
                if(app->state == TvOffStateSending) {
                    tv_off_set_state(app, TvOffStatePaused);
                } else if(app->state == TvOffStatePaused) {
                    tv_off_set_state(app, TvOffStateSending);
                } else if(app->state == TvOffStateDone) {
                    tv_off_restart(app);
                }
            }
            continue;
        }

        if(app->state == TvOffStateSending) {
            if(tv_off_send_next(app)) {
                view_port_update(app->view_port);
            } else {
                tv_off_set_state(app, TvOffStateDone);
            }
        }
    }

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    tv_off_free(app);
    return 0;
}

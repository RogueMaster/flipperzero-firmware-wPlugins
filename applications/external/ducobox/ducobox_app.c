/**
 * Ducobox remote emulator for Flipper Zero.
 *
 * On first start the app sweeps the Duco band to find the remote's channel,
 * reads one frame from the physical controller, stores it as a template and
 * derives every button command from it. Later starts go straight to the
 * remote screen.
 *
 * Left/Up/Right/Down map to Speed 1/2/3/Auto. A tap sends the temporary
 * command (15 minutes; repeated taps extend to 30/45), holding for 5 seconds
 * sends the permanent command. OK opens the config, Back exits.
 */

#include <dialogs/dialogs.h>
#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <furi_hal_subghz.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <string.h>

#include "duco_radio.h"

#define DUCO_DIR             "/ext/apps_data/ducobox"
#define DUCO_TEMPLATE_PATH   DUCO_DIR "/remote.duco"
#define DUCO_FILE_TYPE       "Ducobox RF"
#define DUCO_FILE_VERSION    1
#define DUCO_REFRESH_MS      200
#define DUCO_SCAN_DWELL_MS   400
#define DUCO_MIN_PACKET      10
#define DUCO_FREQUENCY_COUNT 5
#define DUCO_HOLD_MS         5000
#define DUCO_MULTI_PRESS_MS  10000
#define DUCO_STATUS_MS       2000

static const uint32_t ducobox_frequencies[DUCO_FREQUENCY_COUNT] = {
    868244000u,
    868294000u,
    868326000u,
    868350000u,
    868394000u,
};

typedef enum {
    DucoboxViewListen,
    DucoboxViewRemote,
    DucoboxViewConfig,
} DucoboxView;

typedef enum {
    DucoboxStateSetup,
    DucoboxStateRemote,
    DucoboxStateConfig,
} DucoboxState;

typedef enum {
    DucoboxEventTick = 100,
    DucoboxEventSetup,
} DucoboxEvent;

typedef enum {
    DucoboxConfigLearn,
    DucoboxConfigDelete,
    DucoboxConfigExit,
} DucoboxConfigIndex;

typedef struct DucoboxApp DucoboxApp;

typedef struct {
    char status[40];
    uint32_t status_until;
} DucoboxRemoteModel;

struct DucoboxApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    Submenu* config_menu;
    View* remote_view;
    DialogsApp* dialogs;
    Storage* storage;
    FuriTimer* timer;
    FuriThread* rx_thread;
    volatile bool rx_running;
    volatile int16_t rssi;
    volatile uint16_t frames_seen;
    volatile uint16_t crc_errors;
    DucoboxState state;
    uint32_t current_frequency;
    uint32_t packet_frequency;
    uint32_t scan_index;
    bool scan_mode;
    bool setup_mode;
    bool setup_locked;
    bool packet_crc_ok;
    uint8_t packet[DUCO_MAX_PACKET];
    uint8_t rx_packet[DUCO_MAX_PACKET];
    uint8_t packet_len;
    uint8_t remote_counter;
    bool pending;
    bool hold_sent;
    InputKey pending_key;
    uint8_t pending_mode;
    uint8_t pending_button;
    uint32_t press_tick;
    uint8_t last_press_mode;
    uint8_t last_presses;
    uint32_t last_press_tick;
};

static DucoboxApp* ducobox_app_alloc(void);
static void ducobox_app_free(DucoboxApp* app);
static bool ducobox_navigation_callback(void* context);
static bool ducobox_custom_event_callback(void* context, uint32_t event);
static void ducobox_timer_callback(void* context);

static bool ducobox_enter_remote(DucoboxApp* app);
static bool ducobox_load_template(DucoboxApp* app);
static bool ducobox_is_template(const uint8_t* data, uint8_t length);
static void ducobox_start_setup(DucoboxApp* app);
static int32_t ducobox_rx_thread(void* context);
static void ducobox_stop_rx(DucoboxApp* app);
static void ducobox_refresh_listen(DucoboxApp* app);

static void ducobox_show_config(DucoboxApp* app);
static void ducobox_config_callback(void* context, uint32_t index);
static bool ducobox_confirm_delete(DucoboxApp* app);

static void ducobox_remote_draw(Canvas* canvas, void* context);
static bool ducobox_remote_input(InputEvent* event, void* context);
static void ducobox_remote_tick(DucoboxApp* app);
static void ducobox_remote_status(DucoboxApp* app, const char* text);
static void ducobox_remote_send(DucoboxApp* app, uint8_t mode, uint8_t button, bool permanent);
static bool ducobox_build_remote_frame(
    DucoboxApp* app,
    uint8_t mode,
    uint8_t button,
    bool permanent,
    uint8_t presses,
    uint8_t* frame,
    uint8_t* length);
static bool ducobox_key_action(InputKey key, uint8_t* mode, uint8_t* button);
static const char* ducobox_mode_name(uint8_t mode);

static void ducobox_message(DucoboxApp* app, const char* header, const char* text);
static bool ducobox_save_packet(DucoboxApp* app, const char* path);
static bool ducobox_load_packet(DucoboxApp* app, const char* path);
static void ducobox_format_frequency(uint32_t frequency, char* buffer, size_t size);
static void ducobox_switch_to(DucoboxApp* app, DucoboxView view, DucoboxState state);

int32_t ducobox_app(void* p) {
    UNUSED(p);

    DucoboxApp* app = ducobox_app_alloc();
    if(!ducobox_enter_remote(app)) {
        ducobox_start_setup(app);
    }
    view_dispatcher_run(app->view_dispatcher);
    ducobox_app_free(app);
    return 0;
}

static DucoboxApp* ducobox_app_alloc(void) {
    DucoboxApp* app = malloc(sizeof(DucoboxApp));
    furi_assert(app);
    memset(app, 0, sizeof(DucoboxApp));
    app->rssi = -127;
    app->last_press_mode = 0xFF;

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->timer = furi_timer_alloc(ducobox_timer_callback, FuriTimerTypePeriodic, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, ducobox_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ducobox_navigation_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DucoboxViewListen, widget_get_view(app->widget));

    app->config_menu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DucoboxViewConfig, submenu_get_view(app->config_menu));

    app->remote_view = view_alloc();
    view_set_context(app->remote_view, app);
    view_set_draw_callback(app->remote_view, ducobox_remote_draw);
    view_set_input_callback(app->remote_view, ducobox_remote_input);
    view_allocate_model(app->remote_view, ViewModelTypeLocking, sizeof(DucoboxRemoteModel));
    DucoboxRemoteModel* model = view_get_model(app->remote_view);
    model->status[0] = '\0';
    model->status_until = 0;
    view_commit_model(app->remote_view, false);
    view_dispatcher_add_view(app->view_dispatcher, DucoboxViewRemote, app->remote_view);

    storage_common_mkdir(app->storage, DUCO_DIR);
    return app;
}

static void ducobox_app_free(DucoboxApp* app) {
    ducobox_stop_rx(app);
    duco_radio_stop();

    view_dispatcher_remove_view(app->view_dispatcher, DucoboxViewListen);
    widget_free(app->widget);
    view_dispatcher_remove_view(app->view_dispatcher, DucoboxViewConfig);
    submenu_free(app->config_menu);
    view_dispatcher_remove_view(app->view_dispatcher, DucoboxViewRemote);
    view_free(app->remote_view);
    view_dispatcher_free(app->view_dispatcher);

    furi_timer_free(app->timer);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

static bool ducobox_navigation_callback(void* context) {
    DucoboxApp* app = context;

    switch(app->state) {
    case DucoboxStateSetup:
        app->setup_mode = false;
        ducobox_stop_rx(app);
        if(!ducobox_enter_remote(app)) {
            return false;
        }
        return true;
    case DucoboxStateConfig:
        if(!ducobox_enter_remote(app)) {
            ducobox_start_setup(app);
        }
        return true;
    case DucoboxStateRemote:
    default:
        return false;
    }
}

static bool ducobox_custom_event_callback(void* context, uint32_t event) {
    DucoboxApp* app = context;

    if(event == DucoboxEventTick) {
        if(app->state == DucoboxStateSetup) {
            ducobox_refresh_listen(app);
        } else if(app->state == DucoboxStateRemote) {
            ducobox_remote_tick(app);
        }
        return true;
    }

    if(event == DucoboxEventSetup) {
        if(app->state != DucoboxStateSetup) {
            return true;
        }
        ducobox_stop_rx(app);
        app->setup_mode = false;
        if(ducobox_save_packet(app, DUCO_TEMPLATE_PATH)) {
            if(!ducobox_enter_remote(app)) {
                ducobox_message(app, "Error", "Cannot load\nsaved remote");
                ducobox_start_setup(app);
            }
        } else {
            dialog_message_show_storage_error(app->dialogs, "Cannot save remote");
            ducobox_start_setup(app);
        }
        return true;
    }

    return false;
}

static void ducobox_timer_callback(void* context) {
    DucoboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DucoboxEventTick);
}

static bool ducobox_enter_remote(DucoboxApp* app) {
    if(!ducobox_load_template(app)) {
        return false;
    }

    ducobox_stop_rx(app);
    app->setup_mode = false;
    app->setup_locked = false;
    app->scan_mode = false;
    app->pending = false;
    app->hold_sent = false;
    app->last_press_mode = 0xFF;
    app->last_presses = 0;
    app->remote_counter = app->packet[7] & 0x0F;

    DucoboxRemoteModel* model = view_get_model(app->remote_view);
    model->status[0] = '\0';
    model->status_until = 0;
    view_commit_model(app->remote_view, true);

    ducobox_switch_to(app, DucoboxViewRemote, DucoboxStateRemote);
    return true;
}

static void ducobox_show_config(DucoboxApp* app) {
    furi_timer_stop(app->timer);
    app->pending = false;
    app->hold_sent = false;

    submenu_reset(app->config_menu);
    submenu_set_header(app->config_menu, "Config");
    submenu_add_item(
        app->config_menu, "Re-learn remote", DucoboxConfigLearn, ducobox_config_callback, app);
    submenu_add_item(
        app->config_menu, "Delete remote", DucoboxConfigDelete, ducobox_config_callback, app);
    submenu_add_item(app->config_menu, "Exit", DucoboxConfigExit, ducobox_config_callback, app);
    ducobox_switch_to(app, DucoboxViewConfig, DucoboxStateConfig);
}

static void ducobox_config_callback(void* context, uint32_t index) {
    DucoboxApp* app = context;

    switch(index) {
    case DucoboxConfigLearn:
        ducobox_start_setup(app);
        break;
    case DucoboxConfigDelete:
        if(ducobox_confirm_delete(app)) {
            storage_common_remove(app->storage, DUCO_TEMPLATE_PATH);
            app->packet_len = 0;
            ducobox_start_setup(app);
        }
        break;
    case DucoboxConfigExit:
        view_dispatcher_stop(app->view_dispatcher);
        break;
    default:
        break;
    }
}

static bool ducobox_confirm_delete(DucoboxApp* app) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, "Delete remote?", 64, 3, AlignCenter, AlignTop);
    dialog_message_set_text(
        message, "Saved remote code\nwill be removed", 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "Cancel", NULL, "Delete");
    DialogMessageButton result = dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
    return result == DialogMessageButtonRight;
}

static void ducobox_remote_draw(Canvas* canvas, void* context) {
    DucoboxRemoteModel* model = context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "DUCOBOX REMOTE");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    canvas_draw_str_aligned(canvas, 2, 14, AlignLeft, AlignTop, "Left   Speed 1");
    canvas_draw_str_aligned(canvas, 2, 22, AlignLeft, AlignTop, "Up     Speed 2");
    canvas_draw_str_aligned(canvas, 2, 30, AlignLeft, AlignTop, "Right  Speed 3");
    canvas_draw_str_aligned(canvas, 2, 38, AlignLeft, AlignTop, "Down   Auto");

    if(model->status[0] != '\0') {
        canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignTop, model->status);
    } else {
        canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignTop, "tap 15m / hold 5s perm");
    }
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, "OK config  Back exit");
}

static bool ducobox_remote_input(InputEvent* event, void* context) {
    DucoboxApp* app = context;

    if(event->key == InputKeyBack) {
        return false;
    }

    if(event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            ducobox_show_config(app);
        }
        return true;
    }

    uint8_t mode = 0;
    uint8_t button = 0;
    if(!ducobox_key_action(event->key, &mode, &button)) {
        return false;
    }

    if(event->type == InputTypePress) {
        app->pending = true;
        app->hold_sent = false;
        app->pending_key = event->key;
        app->pending_mode = mode;
        app->pending_button = button;
        app->press_tick = furi_get_tick();
        furi_timer_start(app->timer, furi_ms_to_ticks(DUCO_REFRESH_MS));
        return true;
    }

    if(event->type == InputTypeRelease) {
        if(app->pending && event->key == app->pending_key) {
            bool permanent = (furi_get_tick() - app->press_tick) >= furi_ms_to_ticks(DUCO_HOLD_MS);
            if(!app->hold_sent) {
                ducobox_remote_send(app, app->pending_mode, app->pending_button, permanent);
            }
            app->pending = false;
            app->hold_sent = false;
            return true;
        }
    }

    return true;
}

static void ducobox_remote_tick(DucoboxApp* app) {
    // Fire the permanent command at the 5 second mark while the key is still
    // held, so the box reacts without waiting for the release.
    if(app->pending && !app->hold_sent &&
       (furi_get_tick() - app->press_tick) >= furi_ms_to_ticks(DUCO_HOLD_MS)) {
        app->hold_sent = true;
        ducobox_remote_send(app, app->pending_mode, app->pending_button, true);
    }

    DucoboxRemoteModel* model = view_get_model(app->remote_view);
    bool empty = model->status[0] == '\0';
    bool changed = false;

    if(!empty && (int32_t)(furi_get_tick() - model->status_until) >= 0) {
        model->status[0] = '\0';
        empty = true;
        changed = true;
    }
    view_commit_model(app->remote_view, changed);

    if(empty && !app->pending) {
        furi_timer_stop(app->timer);
    }
}

static void ducobox_remote_status(DucoboxApp* app, const char* text) {
    DucoboxRemoteModel* model = view_get_model(app->remote_view);
    strncpy(model->status, text, sizeof(model->status) - 1);
    model->status[sizeof(model->status) - 1] = '\0';
    model->status_until = furi_get_tick() + furi_ms_to_ticks(DUCO_STATUS_MS);
    view_commit_model(app->remote_view, true);
    furi_timer_start(app->timer, furi_ms_to_ticks(DUCO_REFRESH_MS));
}

static void ducobox_remote_send(DucoboxApp* app, uint8_t mode, uint8_t button, bool permanent) {
    uint32_t now = furi_get_tick();
    uint8_t presses = 1;

    if(mode == 0x00) {
        permanent = false;
        app->last_press_mode = 0xFF;
        app->last_presses = 0;
    } else if(!permanent) {
        if(app->last_press_mode == mode &&
           (now - app->last_press_tick) < furi_ms_to_ticks(DUCO_MULTI_PRESS_MS)) {
            presses = app->last_presses < 3 ? app->last_presses + 1 : 3;
        }
        app->last_press_mode = mode;
        app->last_presses = presses;
        app->last_press_tick = now;
    } else {
        app->last_press_mode = 0xFF;
        app->last_presses = 0;
    }

    uint8_t frame[32];
    uint8_t length = 0;
    char status[40];

    if(!ducobox_build_remote_frame(app, mode, button, permanent, presses, frame, &length)) {
        ducobox_remote_status(app, "No template");
        return;
    }

    if(!duco_radio_send_packet(app->packet_frequency, frame, length)) {
        FURI_LOG_I(
            "Ducobox",
            "Remote mode=%u perm=%u presses=%u send=fail",
            (unsigned)mode,
            permanent,
            (unsigned)presses);
        ducobox_remote_status(app, "Send failed");
        return;
    }

    FURI_LOG_I(
        "Ducobox",
        "Remote mode=%u perm=%u presses=%u send=ok",
        (unsigned)mode,
        permanent,
        (unsigned)presses);

    const char* name = ducobox_mode_name(mode);
    if(mode == 0x00) {
        snprintf(status, sizeof(status), "Auto");
    } else if(permanent) {
        snprintf(status, sizeof(status), "%s: permanent", name);
    } else {
        snprintf(status, sizeof(status), "%s: %u min", name, (unsigned)(presses * 15));
    }
    ducobox_remote_status(app, status);
}

static bool ducobox_build_remote_frame(
    DucoboxApp* app,
    uint8_t mode,
    uint8_t button,
    bool permanent,
    uint8_t presses,
    uint8_t* frame,
    uint8_t* length) {
    if(!ducobox_is_template(app->packet, app->packet_len)) {
        return false;
    }

    memcpy(frame, app->packet, 17);

    uint8_t counter = (app->remote_counter % 15) + 1;
    app->remote_counter = counter;

    frame[7] = (app->packet[7] & 0xF0) | counter;
    frame[9] = 0x60;
    frame[10] = 0x00;
    frame[11] = 0x22;
    frame[12] = app->packet[12];
    frame[13] = (permanent ? 0x20 : 0x00) | 0x08 | (mode & 0x07);
    frame[14] = app->packet[14];
    frame[15] = 0x00;
    frame[16] = ((presses & 0x03) << 3) | (permanent ? 0x04 : 0x00) | (button & 0x07);

    *length = 17;
    return true;
}

static bool ducobox_key_action(InputKey key, uint8_t* mode, uint8_t* button) {
    switch(key) {
    case InputKeyLeft:
        *mode = 0x04;
        *button = 1;
        return true;
    case InputKeyUp:
        *mode = 0x05;
        *button = 2;
        return true;
    case InputKeyRight:
        *mode = 0x06;
        *button = 3;
        return true;
    case InputKeyDown:
        *mode = 0x00;
        *button = 0;
        return true;
    default:
        return false;
    }
}

static const char* ducobox_mode_name(uint8_t mode) {
    switch(mode) {
    case 0x00:
        return "Auto";
    case 0x04:
        return "Speed 1";
    case 0x05:
        return "Speed 2";
    case 0x06:
        return "Speed 3";
    case 0x07:
        return "Away";
    default:
        return "Mode";
    }
}

static bool ducobox_is_template(const uint8_t* data, uint8_t length) {
    if(length < 17 || data[0] != 0x07 || data[11] != 0x22 || (data[9] >> 4) < 6) {
        return false;
    }

    // Only accept remote-to-box frames. The Ducobox itself answers with 0x22
    // confirmation frames (source 1, destination = remote address); using one
    // of those as a template would emulate the box instead of the remote.
    uint8_t source = data[5] >> 3;
    uint8_t destination = ((data[5] & 0x07) << 2) | (data[6] >> 6);
    return source != 1 && destination == 1;
}

static bool ducobox_load_template(DucoboxApp* app) {
    if(!ducobox_load_packet(app, DUCO_TEMPLATE_PATH)) {
        return false;
    }
    return ducobox_is_template(app->packet, app->packet_len);
}

static void ducobox_start_setup(DucoboxApp* app) {
    ducobox_stop_rx(app);
    app->setup_mode = true;
    app->setup_locked = false;
    app->scan_mode = true;
    app->pending = false;
    app->hold_sent = false;
    app->frames_seen = 0;
    app->crc_errors = 0;
    app->rssi = -127;
    // Start at the most likely Duco channel, then sweep the rest.
    app->scan_index = 1;
    app->current_frequency = ducobox_frequencies[1];

    ducobox_refresh_listen(app);
    ducobox_switch_to(app, DucoboxViewListen, DucoboxStateSetup);

    furi_timer_start(app->timer, furi_ms_to_ticks(DUCO_REFRESH_MS));
    app->rx_running = true;
    app->rx_thread = furi_thread_alloc_ex("DucoboxRx", 4096, ducobox_rx_thread, app);
    furi_thread_start(app->rx_thread);
}

static int32_t ducobox_rx_thread(void* context) {
    DucoboxApp* app = context;
    uint32_t dwell = furi_ms_to_ticks(DUCO_SCAN_DWELL_MS);

    duco_radio_start_rx(app->current_frequency);
    uint32_t scan_since = furi_get_tick();

    while(app->rx_running) {
        if(app->scan_mode && (furi_get_tick() - scan_since) > dwell) {
            app->scan_index = (app->scan_index + 1) % DUCO_FREQUENCY_COUNT;
            app->current_frequency = ducobox_frequencies[app->scan_index];
            duco_radio_retune(app->current_frequency);
            scan_since = furi_get_tick();
        }

        app->rssi = duco_radio_get_rssi();

        uint8_t size = 0;
        bool crc_ok = false;
        if(duco_radio_read_packet(app->rx_packet, &size, &crc_ok)) {
            app->frames_seen++;
            if(!crc_ok) {
                app->crc_errors++;
            }

            if(size >= DUCO_MIN_PACKET) {
                if(!app->setup_locked) {
                    // Any decoded frame identifies the channel. If it is also
                    // a valid remote frame the template is accepted below
                    // without asking for a second press.
                    app->setup_locked = true;
                    app->scan_mode = false;
                    FURI_LOG_I(
                        "Ducobox",
                        "Setup locked to %lu Hz",
                        (unsigned long)app->current_frequency);
                }

                if(ducobox_is_template(app->rx_packet, size)) {
                    memcpy(app->packet, app->rx_packet, size);
                    app->packet_len = size;
                    app->packet_crc_ok = crc_ok;
                    app->packet_frequency = app->current_frequency;
                    app->rx_running = false;
                    view_dispatcher_send_custom_event(app->view_dispatcher, DucoboxEventSetup);
                    break;
                }
            }
        }

        furi_delay_ms(10);
    }

    duco_radio_stop();
    return 0;
}

static void ducobox_stop_rx(DucoboxApp* app) {
    app->rx_running = false;
    if(app->rx_thread) {
        furi_thread_join(app->rx_thread);
        furi_thread_free(app->rx_thread);
        app->rx_thread = NULL;
    }
    furi_timer_stop(app->timer);
}

static void ducobox_refresh_listen(DucoboxApp* app) {
    char frequency[16];
    char line[40];
    ducobox_format_frequency(app->current_frequency, frequency, sizeof(frequency));

    const char* title = app->setup_locked ? "Remote found" : "Finding frequency";

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, title);
    snprintf(line, sizeof(line), "%s MHz", frequency);
    widget_add_string_element(app->widget, 64, 17, AlignCenter, AlignTop, FontSecondary, line);
    snprintf(line, sizeof(line), "RSSI %d dBm", (int)app->rssi);
    widget_add_string_element(app->widget, 64, 27, AlignCenter, AlignTop, FontSecondary, line);
    snprintf(
        line,
        sizeof(line),
        "Frames %u, bad %u",
        (unsigned)app->frames_seen,
        (unsigned)app->crc_errors);
    widget_add_string_element(app->widget, 64, 37, AlignCenter, AlignTop, FontSecondary, line);
    widget_add_string_multiline_element(
        app->widget,
        64,
        48,
        AlignCenter,
        AlignTop,
        FontSecondary,
        app->setup_locked ? "Press again to\nread the remote" : "Press the remote\nbutton");
}

static void ducobox_message(DucoboxApp* app, const char* header, const char* text) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 3, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
}

static bool ducobox_save_packet(DucoboxApp* app, const char* path) {
    FlipperFormat* format = flipper_format_file_alloc(app->storage);
    bool saved = false;

    do {
        if(!flipper_format_file_open_always(format, path)) {
            break;
        }
        if(!flipper_format_write_header_cstr(format, DUCO_FILE_TYPE, DUCO_FILE_VERSION)) {
            break;
        }
        uint32_t frequency = app->packet_frequency;
        if(!flipper_format_write_uint32(format, "Frequency", &frequency, 1)) {
            break;
        }
        uint32_t length = app->packet_len;
        if(!flipper_format_write_uint32(format, "Length", &length, 1)) {
            break;
        }
        uint32_t crc = app->packet_crc_ok ? 1 : 0;
        if(!flipper_format_write_uint32(format, "Crc", &crc, 1)) {
            break;
        }
        if(!flipper_format_write_hex(format, "Data", app->packet, (uint16_t)app->packet_len)) {
            break;
        }
        if(!flipper_format_write_comment_cstr(
               format, "Ducobox payload without length byte or CRC")) {
            break;
        }
        saved = true;
    } while(false);

    flipper_format_free(format);
    return saved;
}

static bool ducobox_load_packet(DucoboxApp* app, const char* path) {
    FlipperFormat* format = flipper_format_file_alloc(app->storage);
    FuriString* file_type = furi_string_alloc();
    bool loaded = false;

    do {
        if(!flipper_format_file_open_existing(format, path)) {
            break;
        }
        uint32_t version = 0;
        if(!flipper_format_read_header(format, file_type, &version)) {
            break;
        }
        if(furi_string_cmp_str(file_type, DUCO_FILE_TYPE) != 0) {
            break;
        }

        uint32_t frequency = 0;
        if(!flipper_format_read_uint32(format, "Frequency", &frequency, 1)) {
            break;
        }
        if(!furi_hal_subghz_is_frequency_valid(frequency)) {
            break;
        }
        uint32_t length = 0;
        if(!flipper_format_read_uint32(format, "Length", &length, 1)) {
            break;
        }
        if(length == 0 || length > DUCO_MAX_PACKET) {
            break;
        }
        if(!flipper_format_read_hex(format, "Data", app->packet, (uint16_t)length)) {
            break;
        }

        uint32_t crc = 1;
        // Older files have no Crc field; treat them as valid.
        flipper_format_read_uint32(format, "Crc", &crc, 1);

        app->packet_len = (uint8_t)length;
        app->packet_frequency = frequency;
        app->packet_crc_ok = crc != 0;
        loaded = true;
    } while(false);

    furi_string_free(file_type);
    flipper_format_free(format);
    return loaded;
}

static void ducobox_format_frequency(uint32_t frequency, char* buffer, size_t size) {
    snprintf(
        buffer,
        size,
        "%lu.%03lu",
        (unsigned long)(frequency / 1000000),
        (unsigned long)((frequency % 1000000) / 1000));
}

static void ducobox_switch_to(DucoboxApp* app, DucoboxView view, DucoboxState state) {
    app->state = state;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

#include <furi.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include <input/input.h>
#include <stdlib.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_spi.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_resources.h>
#include <nrf24.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>
#include "nrf24_jammer_icons.h"
#include "gui/elements.h"
#include <toolbox/stream/file_stream.h>
#include <dialogs/dialogs.h>

#include <stringp.h>

#define TAG "jammer"

#define MARGIN_LEFT  5
#define MARGIN_TOP   5
#define MARGIN_BOT   5
#define MARGIN_RIGHT 5
#define KEY_WIDTH    5
#define KEY_HEIGHT   10
#define KEY_PADDING  2

typedef struct {
    FuriMutex* mutex;
    bool is_thread_running;
    bool is_nrf24_connected;
    bool close_thread_please;
    bool select_file_on_next_render;
    uint8_t wifi_channel;
    uint8_t jam_type; // 0:narrow, 1:wide, 2:all, 3:WiFi, 4:custom
    FuriThread* jam_thread;
} PluginState;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

// various types of hopping I empirically found
uint8_t hopping_channels_2[128];
uint8_t hopping_channels_1[] = {32, 34, 46, 48, 50, 52, 0,  1,  2,  4,  6,  8,
                                22, 24, 26, 28, 30, 74, 76, 78, 80, 82, 84, 86};
uint8_t hopping_channels_0[] = {2, 26, 80};
uint8_t hopping_channels_3[] = {12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72};
uint8_t hopping_channels_len[] = {3, 24, 42, 12, 0};

int parse_custom_list(char* input, uint8_t** out_arr) {
    // Make a copy of the input string, because strtok modifies it
    char* temp = strdup(input);
    if(!temp) {
        *out_arr = NULL;
        return 0;
    }

    // First pass: count how many integers we have (commas + 1)
    int count = 0;
    for(int i = 0; temp[i] != '\0'; i++) {
        if(temp[i] == ',') {
            count++;
        }
    }
    count++; // one more than number of commas

    // Allocate the array
    *out_arr = (uint8_t*)malloc(count * sizeof(uint8_t));
    if(!(*out_arr)) {
        free(temp);
        return 0; // allocation failed
    }

    // Second pass: tokenize and parse
    int index = 0;
    char* token = strtok(temp, ",");
    while(token != NULL) {
        (*out_arr)[index++] = (uint8_t)atoi(token);
        token = strtok(NULL, ",");
    }

    free(temp);
    return count; // number of integers parsed
}

char* jam_types[] = {"narrow", "wide", "full", "WiFi", "custom"};
uint8_t* hopping_channels;
int hopping_channels_custom_len = 0;
uint8_t* hopping_channels_custom_arr = NULL;

static void render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    PluginState* plugin_state = ctx;
    //furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);

    // border around the edge of the screen
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontSecondary);

    char tmp[128];
    if(plugin_state->jam_type == 3)
        snprintf(
            tmp,
            128,
            "^ type:%s > channel:%d",
            jam_types[plugin_state->jam_type],
            plugin_state->wifi_channel + 1);
    else
        snprintf(tmp, 128, "^ type:%s", jam_types[plugin_state->jam_type]);
    canvas_draw_str_aligned(canvas, 10, 3, AlignLeft, AlignTop, tmp);

    if(!plugin_state->is_thread_running && plugin_state->jam_type <= 4) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 10, 20, AlignLeft, AlignBottom, "Press Ok button to start");

        if(plugin_state->jam_type == 4) {
            canvas_draw_str(canvas, 80, 30, "select file >");
        }

        if(!plugin_state->is_nrf24_connected) {
            canvas_draw_str_aligned(
                canvas, 10, 30, AlignLeft, AlignBottom, "Connect NRF24 to GPIO!");
        }
    } else if(plugin_state->is_thread_running && plugin_state->jam_type <= 4) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 3, 30, AlignLeft, AlignBottom, "Causing mayhem...");
        canvas_draw_str_aligned(canvas, 3, 40, AlignLeft, AlignBottom, "Please wait!");
        canvas_draw_str_aligned(canvas, 3, 50, AlignLeft, AlignBottom, "Press back to exit.");
    } else {
        canvas_draw_str_aligned(canvas, 3, 10, AlignLeft, AlignBottom, "Unknown Error");
        canvas_draw_str_aligned(canvas, 3, 20, AlignLeft, AlignBottom, "press back");
        canvas_draw_str_aligned(canvas, 3, 30, AlignLeft, AlignBottom, "to exit");
    }
    uint8_t limit = hopping_channels_len[plugin_state->jam_type];
    canvas_draw_frame(canvas, 0, 52, 128, 13);
    if(limit > 0) {
        for(int i = 0; i < limit; i++) {
            canvas_draw_line(canvas, hopping_channels[i] + 1, 53, hopping_channels[i] + 1, 64);
        }
    }
    //furi_mutex_release(plugin_state->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, 100);
}

static void jammer_state_init(PluginState* const plugin_state) {
    plugin_state->is_thread_running = false;
}

// entrypoint for worker
static int32_t mj_worker_thread(void* ctx) {
    PluginState* plugin_state = ctx;
    plugin_state->is_thread_running = true;

#define size 32
    uint8_t tx[size + 1] = {0};

    FURI_LOG_D(TAG, "Starting optimized carrier jamming");

    if(plugin_state->jam_type == 3) {
        char tmp[128];

        nrf24_set_idle(nrf24_HANDLE);

        uint8_t mac[] = {0xDE, 0xAD}; // DEAD BEEF FEED

        uint8_t ping_packet[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE,
                                 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
                                 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF};

        nrf24_configure(nrf24_HANDLE, 2, mac, mac, 2, 1, true, true);

        uint8_t setup;
        nrf24_read_reg(nrf24_HANDLE, REG_RF_SETUP, &setup, 1);
        setup = (setup & 0xF8) | 7;
        nrf24_write_reg(nrf24_HANDLE, REG_RF_SETUP, setup);

        uint8_t conf;
        nrf24_read_reg(nrf24_HANDLE, REG_CONFIG, &conf, 1);
        snprintf(tmp, sizeof(tmp), "NRF24 SETUP REGISTER: %d", setup);
        FURI_LOG_D(TAG, tmp);
        snprintf(tmp, sizeof(tmp), "NRF24 CONFIG REGISTER: %d", conf);
        FURI_LOG_D(TAG, tmp);

        tx[0] = W_TX_PAYLOAD_NOACK;
        memcpy(&tx[1], ping_packet, size);

#define nrf24_TIMEOUT 500
    }

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_blink_red_100);

    if(plugin_state->jam_type != 3) {
        nrf24_deinit();
        nrf24_init();
        nrf24_set_tx_mode(nrf24_HANDLE);
        nrf24_startConstCarrier(nrf24_HANDLE, 3, 45);
    }

    uint8_t current_channel = 0;
    uint8_t limit = hopping_channels_len[plugin_state->jam_type];

    nrf24_set_tx_mode(nrf24_HANDLE);

    while(!plugin_state->close_thread_please) {
        for(int ch = 0; ch < limit && !plugin_state->close_thread_please; ch++) {
            current_channel = hopping_channels[ch];
            if(plugin_state->jam_type == 3) {
                for(int wifi_ch = (plugin_state->wifi_channel * 5) + 1;
                    wifi_ch < (plugin_state->wifi_channel * 5) + 23 &&
                    !plugin_state->close_thread_please;
                    wifi_ch++) {
                    nrf24_write_reg(nrf24_HANDLE, REG_RF_CH, wifi_ch);
                    nrf24_spi_trx(nrf24_HANDLE, tx, 0, 3, nrf24_TIMEOUT);
                }
            } else
                nrf24_write_reg(nrf24_HANDLE, REG_RF_CH, current_channel);
        }
    }

    if(plugin_state->jam_type != 3) nrf24_stopConstCarrier(nrf24_HANDLE);

    furi_record_close(RECORD_NOTIFICATION);
    plugin_state->is_thread_running = false;
    return 0;
}

int32_t jammer_app(void* p) {
    UNUSED(p);
    if(!furi_hal_power_is_otg_enabled()) furi_hal_power_enable_otg();
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));
    dolphin_deed(DolphinDeedPluginStart);
    PluginState* plugin_state = malloc(sizeof(PluginState));
    jammer_state_init(plugin_state);
    plugin_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    plugin_state->wifi_channel = 0;
    if(!plugin_state->mutex) {
        FURI_LOG_E("jammer", "cannot create mutex\r\n");
        furi_message_queue_free(event_queue);
        free(plugin_state);
        return 255;
    }

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, plugin_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    plugin_state->jam_thread = furi_thread_alloc();
    furi_thread_set_name(plugin_state->jam_thread, "Jammer Worker");
    furi_thread_set_stack_size(plugin_state->jam_thread, 2048);
    furi_thread_set_context(plugin_state->jam_thread, plugin_state);
    furi_thread_set_callback(plugin_state->jam_thread, mj_worker_thread);
    FURI_LOG_D(TAG, "nrf24 init...");
    nrf24_init();
    FURI_LOG_D(TAG, "nrf24 init done!");
    PluginEvent event;
    for(int i = 0; i < 128; i++)
        hopping_channels_2[i] = i * 2;
    hopping_channels = hopping_channels_0;
    plugin_state->is_nrf24_connected = true;
    if(!nrf24_check_connected(nrf24_HANDLE)) {
        plugin_state->is_nrf24_connected = false;
    }

    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);
        if(event_status == FuriStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    //furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);

                    switch(event.input.key) {
                    case InputKeyUp:
                        if(!plugin_state->is_thread_running) {
                            plugin_state->jam_type = (plugin_state->jam_type + 1) % 5;

                            switch(plugin_state->jam_type) {
                            case 0:
                                hopping_channels = hopping_channels_0;
                                break;
                            case 1:
                                hopping_channels = hopping_channels_1;
                                break;
                            case 2:
                                hopping_channels = hopping_channels_2;
                                break;
                            case 3:
                                hopping_channels = hopping_channels_3;
                                break;
                            case 4:
                                hopping_channels = hopping_channels_custom_arr;
                            default:
                                break;
                            }

                            view_port_update(view_port);
                        }
                        break;
                    case InputKeyDown:
                        if(!plugin_state->is_thread_running) {
                            plugin_state->jam_type =
                                (plugin_state->jam_type == 0) ? 4 : (plugin_state->jam_type - 1);

                            switch(plugin_state->jam_type) {
                            case 0:
                                hopping_channels = hopping_channels_0;
                                break;
                            case 1:
                                hopping_channels = hopping_channels_1;
                                break;
                            case 2:
                                hopping_channels = hopping_channels_2;
                                break;
                            case 3:
                                hopping_channels = hopping_channels_3;
                                break;
                            case 4:
                                hopping_channels = hopping_channels_custom_arr;
                            default:
                                break;
                            }

                            view_port_update(view_port);
                        }
                        break;
                    case InputKeyRight:
                        if(plugin_state->jam_type == 4 && !plugin_state->is_thread_running) {
                            DialogsApp* dialogs = furi_record_open("dialogs");
                            Storage* storage = furi_record_open(RECORD_STORAGE);
                            plugin_state->select_file_on_next_render = true;
                            Stream* file_stream = file_stream_alloc(storage);
                            plugin_state->select_file_on_next_render = false;
                            FuriString* path;
                            path = furi_string_alloc();
                            furi_string_set(path, "/ext/");
                            DialogsFileBrowserOptions browser_options;
                            dialog_file_browser_set_basic_options(
                                &browser_options, ".txt", &I_badusb_10px);
                            browser_options.hide_ext = false;
                            browser_options.base_path = "/ext/";
                            furi_delay_ms(1000);
                            bool ret =
                                dialog_file_browser_show(dialogs, path, path, &browser_options);
                            furi_record_close("dialogs");
                            if(ret) {
                                if(!file_stream_open(
                                       file_stream,
                                       furi_string_get_cstr(path),
                                       FSAM_READ,
                                       FSOM_OPEN_EXISTING)) {
                                    FURI_LOG_D(
                                        TAG,
                                        "Cannot open file \"%s\"",
                                        furi_string_get_cstr(path));
                                } else {
                                    size_t file_size = 0;
                                    size_t bytes_read = 0;
                                    uint8_t* file_buf;
                                    FURI_LOG_I(TAG, "opening channel file");
                                    file_size = stream_size(file_stream);
                                    file_buf = malloc(file_size);
                                    memset(file_buf, 0, file_size);
                                    bytes_read = stream_read(file_stream, file_buf, file_size);
                                    if(bytes_read == file_size) {
                                        FURI_LOG_I(TAG, "read file");
                                        hopping_channels_custom_len = parse_custom_list(
                                            (char*)file_buf, &hopping_channels_custom_arr);

                                        FURI_LOG_D(
                                            TAG,
                                            "Length: %d\n",
                                            hopping_channels_custom_len); // e.g. 6
                                        for(int i = 0; i < hopping_channels_custom_len; i++) {
                                            FURI_LOG_D(
                                                TAG,
                                                "%d ",
                                                hopping_channels_custom_arr[i]); // 1 2 3 4 1 2
                                        }
                                        hopping_channels_len[4] = hopping_channels_custom_len;
                                        hopping_channels = hopping_channels_custom_arr;

                                        FURI_LOG_D(TAG, "%s", file_buf);
                                    } else {
                                        FURI_LOG_I(TAG, "load failed. file size: %d", file_size);
                                    }
                                    free(file_buf);
                                }
                            }
                            furi_string_free(path);

                            view_port_update(view_port);
                        } else if(plugin_state->jam_type == 3 && !plugin_state->is_thread_running) {
                            plugin_state->wifi_channel = (plugin_state->wifi_channel + 1) % 13;
                            view_port_update(view_port);
                        }
                        break;
                    case InputKeyLeft:
                        if(plugin_state->jam_type == 3 && !plugin_state->is_thread_running) {
                            plugin_state->wifi_channel = (plugin_state->wifi_channel == 0) ?
                                                             12 :
                                                             (plugin_state->wifi_channel - 1);
                            view_port_update(view_port);
                        }
                        break;
                    case InputKeyOk:
                        if(!plugin_state->is_thread_running) {
                            if(!nrf24_check_connected(nrf24_HANDLE) ||
                               !plugin_state->is_nrf24_connected) {
                                plugin_state->is_nrf24_connected = false;
                                notification_message(notification, &sequence_error);
                            } else {
                                furi_thread_start(plugin_state->jam_thread);
                            }
                            view_port_update(view_port);
                        }

                        break;
                    case InputKeyBack:
                        FURI_LOG_D(TAG, "terminating thread...");
                        if(!plugin_state->is_thread_running) processing = false;

                        plugin_state->close_thread_please = true;

                        if(plugin_state->is_thread_running && plugin_state->jam_thread) {
                            plugin_state->is_thread_running = false;
                            furi_thread_join(
                                plugin_state->jam_thread); // wait until thread is finished
                            view_port_update(view_port);
                        }
                        plugin_state->close_thread_please = false;

                        break;
                    default:
                        break;
                    }

                    //view_port_update(view_port);
                }
            }
        }

        // furi_mutex_release(plugin_state->mutex);
    }

    furi_thread_free(plugin_state->jam_thread);
    FURI_LOG_D(TAG, "nrf24 deinit...");
    nrf24_deinit();
    furi_hal_power_disable_otg();
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    // furi_mutex_free(plugin_state->mutex);
    free(plugin_state);

    return 0;
}

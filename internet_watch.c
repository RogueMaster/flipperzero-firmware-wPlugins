#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "WifiInternetWatch"

#define WIFI_MAX_NETWORKS 16
#define WIFI_SSID_SIZE 33
#define WIFI_PASSWORD_SIZE 65
#define AT_RESPONSE_SIZE 8192
#define MONITOR_INTERVAL_MS 60000

typedef enum {
    WifiScreenScanning,
    WifiScreenNetworkList,
    WifiScreenStatus,
} WifiScreen;

typedef enum {
    WifiViewMain,
    WifiViewPassword,
} WifiView;

typedef enum {
    WifiEventPasswordDone = 1,
} WifiEvent;

typedef enum {
    WifiWorkerFlagStop = (1U << 0),
    WifiWorkerFlagConnect = (1U << 1),
} WifiWorkerFlag;

typedef struct {
    WifiScreen screen;
    char networks[WIFI_MAX_NETWORKS][WIFI_SSID_SIZE];
    size_t network_count;
    size_t selected_network;
    char status[32];
    char detail[48];
} WifiViewModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    TextInput* password_input;
    NotificationApp* notification;
    Expansion* expansion;

    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* worker;

    char ssid[WIFI_SSID_SIZE];
    char password[WIFI_PASSWORD_SIZE];
    bool monitor_active;
    bool was_online;
    bool otg_was_enabled;
} WifiInternetWatch;

static void wifi_password_done_callback(void* context);

static void wifi_set_status(
    WifiInternetWatch* app,
    WifiScreen screen,
    const char* status,
    const char* detail) {
    with_view_model(
        app->main_view,
        WifiViewModel * model,
        {
            model->screen = screen;
            strlcpy(model->status, status ? status : "", sizeof(model->status));
            strlcpy(model->detail, detail ? detail : "", sizeof(model->detail));
        },
        true);
}

static void wifi_serial_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    WifiInternetWatch* app = context;

    if(event & FuriHalSerialRxEventData) {
        const uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(app->rx_stream, &byte, 1, 0);
    }
}

static void wifi_drain_uart(WifiInternetWatch* app) {
    uint8_t buffer[64];
    while(furi_stream_buffer_receive(app->rx_stream, buffer, sizeof(buffer), 0) > 0) {
    }
}

static bool wifi_at_command(
    WifiInternetWatch* app,
    const char* command,
    char* response,
    size_t response_size,
    uint32_t timeout_ms) {
    furi_assert(response_size > 1);
    wifi_drain_uart(app);
    response[0] = '\0';

    furi_hal_serial_tx(app->serial, (const uint8_t*)command, strlen(command));
    furi_hal_serial_tx(app->serial, (const uint8_t*)"\r\n", 2);
    furi_hal_serial_tx_wait_complete(app->serial);

    const uint32_t deadline = furi_get_tick() + timeout_ms;
    size_t length = 0;

    while((int32_t)(deadline - furi_get_tick()) > 0) {
        if(furi_thread_flags_get() & WifiWorkerFlagStop) return false;

        uint8_t byte;
        if(furi_stream_buffer_receive(app->rx_stream, &byte, 1, 50) == 1) {
            if(length + 1 < response_size) {
                response[length++] = (char)byte;
                response[length] = '\0';
            }

            if(strstr(response, "\r\nOK\r\n") || strstr(response, "\r\nERROR\r\n") ||
               strstr(response, "\r\nFAIL\r\n")) {
                break;
            }
        }
    }

    return strstr(response, "\r\nOK\r\n") != NULL;
}

static bool wifi_network_exists(
    char networks[WIFI_MAX_NETWORKS][WIFI_SSID_SIZE],
    size_t network_count,
    const char* ssid) {
    for(size_t i = 0; i < network_count; i++) {
        if(strcmp(networks[i], ssid) == 0) return true;
    }
    return false;
}

static size_t wifi_parse_scan(
    const char* response,
    char networks[WIFI_MAX_NETWORKS][WIFI_SSID_SIZE]) {
    size_t count = 0;
    const char* cursor = response;

    while(count < WIFI_MAX_NETWORKS) {
        const char* entry = strstr(cursor, "+CWLAP:");
        if(!entry) break;

        const char* start = strchr(entry, '"');
        if(!start) break;
        start++;

        const char* end = strchr(start, '"');
        if(!end) break;

        const size_t length = end - start;
        if(length > 0 && length < WIFI_SSID_SIZE) {
            char ssid[WIFI_SSID_SIZE];
            memcpy(ssid, start, length);
            ssid[length] = '\0';

            if(!wifi_network_exists(networks, count, ssid)) {
                strlcpy(networks[count], ssid, WIFI_SSID_SIZE);
                count++;
            }
        }

        cursor = end + 1;
    }

    return count;
}

static void wifi_escape_at_string(const char* input, char* output, size_t output_size) {
    size_t output_pos = 0;

    for(size_t i = 0; input[i] && output_pos + 1 < output_size; i++) {
        if((input[i] == '\\' || input[i] == '"') && output_pos + 2 < output_size) {
            output[output_pos++] = '\\';
        }
        output[output_pos++] = input[i];
    }

    output[output_pos] = '\0';
}

static void wifi_scan(WifiInternetWatch* app) {
    char* response = malloc(AT_RESPONSE_SIZE);
    char networks[WIFI_MAX_NETWORKS][WIFI_SSID_SIZE] = {0};

    if(!response) {
        wifi_set_status(app, WifiScreenStatus, "OUT OF MEMORY", "Cannot scan Wi-Fi");
        return;
    }

    wifi_set_status(app, WifiScreenScanning, "Scanning Wi-Fi...", "");
    wifi_at_command(app, "AT", response, AT_RESPONSE_SIZE, 1000);
    wifi_at_command(app, "ATE0", response, AT_RESPONSE_SIZE, 1000);
    wifi_at_command(app, "AT+CWMODE=1", response, AT_RESPONSE_SIZE, 2000);

    const bool scan_ok =
        wifi_at_command(app, "AT+CWLAP", response, AT_RESPONSE_SIZE, 20000);
    const size_t network_count = scan_ok ? wifi_parse_scan(response, networks) : 0;

    with_view_model(
        app->main_view,
        WifiViewModel * model,
        {
            model->network_count = network_count;
            model->selected_network = 0;
            for(size_t i = 0; i < network_count; i++) {
                strlcpy(model->networks[i], networks[i], WIFI_SSID_SIZE);
            }

            if(network_count > 0) {
                model->screen = WifiScreenNetworkList;
                strlcpy(model->status, "Select Wi-Fi", sizeof(model->status));
                model->detail[0] = '\0';
            } else {
                model->screen = WifiScreenStatus;
                strlcpy(model->status, "SCAN FAILED", sizeof(model->status));
                strlcpy(
                    model->detail,
                    scan_ok ? "No networks found" : "ESP-AT did not respond",
                    sizeof(model->detail));
            }
        },
        true);

    free(response);
}

static bool wifi_is_joined(WifiInternetWatch* app, char* response, size_t response_size) {
    return wifi_at_command(app, "AT+CWJAP?", response, response_size, 3000) &&
           strstr(response, "+CWJAP:") != NULL;
}

static bool wifi_join(WifiInternetWatch* app, char* response, size_t response_size) {
    if(wifi_is_joined(app, response, response_size) &&
       strstr(response, app->ssid) != NULL) {
        return true;
    }

    char escaped_ssid[WIFI_SSID_SIZE * 2];
    char escaped_password[WIFI_PASSWORD_SIZE * 2];
    char command[256];
    wifi_escape_at_string(app->ssid, escaped_ssid, sizeof(escaped_ssid));
    wifi_escape_at_string(app->password, escaped_password, sizeof(escaped_password));

    snprintf(
        command,
        sizeof(command),
        "AT+CWJAP=\"%s\",\"%s\"",
        escaped_ssid,
        escaped_password);

    return wifi_at_command(app, command, response, response_size, 25000);
}

static void wifi_monitor_once(WifiInternetWatch* app) {
    char response[1024];

    wifi_set_status(app, WifiScreenStatus, "Connecting Wi-Fi...", app->ssid);
    if(!wifi_join(app, response, sizeof(response))) {
        app->was_online = false;
        wifi_set_status(app, WifiScreenStatus, "NO WI-FI", app->ssid);
        notification_message(app->notification, &sequence_set_only_red_255);
        return;
    }

    wifi_set_status(app, WifiScreenStatus, "Checking internet...", "1.1.1.1");
    const bool online =
        wifi_at_command(app, "AT+PING=\"1.1.1.1\"", response, sizeof(response), 10000) &&
        strstr(response, "+PING:") != NULL;

    if(online) {
        wifi_set_status(app, WifiScreenStatus, "ONLINE", app->ssid);
        notification_message(app->notification, &sequence_set_only_green_255);
        if(!app->was_online) {
            notification_message(app->notification, &sequence_success);
        }
    } else {
        wifi_set_status(app, WifiScreenStatus, "OFFLINE", app->ssid);
        notification_message(app->notification, &sequence_set_only_red_255);
    }

    app->was_online = online;
}

static int32_t wifi_worker(void* context) {
    WifiInternetWatch* app = context;
    wifi_scan(app);

    while(true) {
        const uint32_t timeout = app->monitor_active ? MONITOR_INTERVAL_MS : FuriWaitForever;
        const uint32_t flags = furi_thread_flags_wait(
            WifiWorkerFlagStop | WifiWorkerFlagConnect, FuriFlagWaitAny, timeout);

        if(flags & WifiWorkerFlagStop) break;

        if(flags == (uint32_t)FuriFlagErrorTimeout) {
            if(app->monitor_active) wifi_monitor_once(app);
            continue;
        }

        if(flags & WifiWorkerFlagConnect) {
            app->monitor_active = true;
            app->was_online = false;
            wifi_monitor_once(app);
        }
    }

    return 0;
}

static void wifi_draw_callback(Canvas* canvas, void* context) {
    WifiViewModel* model = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 12, "WiFi Internet Watch");
    canvas_draw_line(canvas, 0, 15, 127, 15);

    if(model->screen == WifiScreenNetworkList) {
        canvas_set_font(canvas, FontSecondary);
        if(model->network_count == 0) {
            canvas_draw_str(canvas, 4, 35, "No networks");
            return;
        }

        size_t first = 0;
        if(model->selected_network > 1) first = model->selected_network - 1;
        if(first + 3 > model->network_count && model->network_count > 3) {
            first = model->network_count - 3;
        }

        for(size_t row = 0; row < 3 && first + row < model->network_count; row++) {
            const size_t index = first + row;
            const uint8_t y = 27 + row * 12;
            if(index == model->selected_network) {
                canvas_draw_box(canvas, 1, y - 8, 126, 10);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 4, y, model->networks[index]);
            if(index == model->selected_network) canvas_set_color(canvas, ColorBlack);
        }

        elements_button_center(canvas, "Select");
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 31, model->status);
        canvas_draw_str(canvas, 4, 47, model->detail);
        canvas_draw_str(canvas, 4, 62, "Back: exit");
    }
}

static bool wifi_input_callback(InputEvent* event, void* context) {
    WifiInternetWatch* app = context;
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;
    bool select_network = false;

    with_view_model(
        app->main_view,
        WifiViewModel * model,
        {
            if(model->screen == WifiScreenNetworkList && model->network_count > 0) {
                if(event->key == InputKeyUp) {
                    if(model->selected_network > 0) model->selected_network--;
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    if(model->selected_network + 1 < model->network_count) {
                        model->selected_network++;
                    }
                    consumed = true;
                } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                    strlcpy(
                        app->ssid,
                        model->networks[model->selected_network],
                        sizeof(app->ssid));
                    select_network = true;
                    consumed = true;
                }
            }
        },
        consumed);

    if(select_network) {
        app->password[0] = '\0';
        text_input_reset(app->password_input);
        text_input_set_header_text(app->password_input, app->ssid);
        text_input_set_result_callback(
            app->password_input,
            wifi_password_done_callback,
            app,
            app->password,
            sizeof(app->password),
            true);
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewPassword);
    }

    return consumed;
}

static uint32_t wifi_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t wifi_password_back_callback(void* context) {
    UNUSED(context);
    return WifiViewMain;
}

static void wifi_password_done_callback(void* context) {
    WifiInternetWatch* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WifiEventPasswordDone);
}

static bool wifi_custom_event_callback(void* context, uint32_t event) {
    WifiInternetWatch* app = context;

    if(event == WifiEventPasswordDone) {
        wifi_set_status(app, WifiScreenStatus, "Connecting Wi-Fi...", app->ssid);
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewMain);
        furi_thread_flags_set(furi_thread_get_id(app->worker), WifiWorkerFlagConnect);
        return true;
    }

    return false;
}

static WifiInternetWatch* wifi_app_alloc(void) {
    WifiInternetWatch* app = malloc(sizeof(WifiInternetWatch));
    memset(app, 0, sizeof(WifiInternetWatch));

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, wifi_custom_event_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(WifiViewModel));
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, wifi_draw_callback);
    view_set_input_callback(app->main_view, wifi_input_callback);
    view_set_previous_callback(app->main_view, wifi_exit_callback);
    with_view_model(
        app->main_view,
        WifiViewModel * model,
        {
            memset(model, 0, sizeof(WifiViewModel));
            model->screen = WifiScreenScanning;
            strlcpy(model->status, "Starting ESP-AT...", sizeof(model->status));
        },
        false);
    view_dispatcher_add_view(app->view_dispatcher, WifiViewMain, app->main_view);

    app->password_input = text_input_alloc();
    text_input_set_result_callback(
        app->password_input,
        wifi_password_done_callback,
        app,
        app->password,
        sizeof(app->password),
        true);
    view_set_previous_callback(text_input_get_view(app->password_input), wifi_password_back_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, WifiViewPassword, text_input_get_view(app->password_input));

    return app;
}

static bool wifi_uart_start(WifiInternetWatch* app) {
    app->rx_stream = furi_stream_buffer_alloc(AT_RESPONSE_SIZE, 1);
    app->serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    if(!app->serial) return false;

    furi_hal_serial_init(app->serial, 115200);
    furi_hal_serial_async_rx_start(
        app->serial, wifi_serial_rx_callback, app, false);
    return true;
}

static void wifi_app_free(WifiInternetWatch* app) {
    if(app->worker) {
        furi_thread_flags_set(furi_thread_get_id(app->worker), WifiWorkerFlagStop);
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
    }

    if(app->serial) {
        furi_hal_serial_async_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
    }
    if(app->rx_stream) furi_stream_buffer_free(app->rx_stream);

    notification_message(app->notification, &sequence_reset_rgb);

    view_dispatcher_remove_view(app->view_dispatcher, WifiViewPassword);
    text_input_free(app->password_input);
    view_dispatcher_remove_view(app->view_dispatcher, WifiViewMain);
    view_free(app->main_view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t wifi_internet_watch_app(void* argument) {
    UNUSED(argument);

    WifiInternetWatch* app = wifi_app_alloc();
    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    app->otg_was_enabled = furi_hal_power_is_otg_enabled();
    for(size_t attempt = 0; !furi_hal_power_is_otg_enabled() && attempt < 5; attempt++) {
        furi_hal_power_enable_otg();
        furi_delay_ms(20);
    }
    furi_delay_ms(300);

    if(wifi_uart_start(app)) {
        app->worker = furi_thread_alloc_ex("WifiAtWorker", 4096, wifi_worker, app);
        furi_thread_start(app->worker);
    } else {
        wifi_set_status(app, WifiScreenStatus, "UART BUSY", "Close other GPIO apps");
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewMain);
    view_dispatcher_run(app->view_dispatcher);

    const bool otg_was_enabled = app->otg_was_enabled;
    Expansion* expansion = app->expansion;
    wifi_app_free(app);

    if(furi_hal_power_is_otg_enabled() && !otg_was_enabled) {
        furi_hal_power_disable_otg();
    }
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    return 0;
}

#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#define TAG "WifiInternetWatch"

#define WIFI_MAX_NETWORKS 16
#define WIFI_SSID_SIZE 33
#define WIFI_PASSWORD_SIZE 65
#define WIFI_MAX_SAVED_NETWORKS 16
#define AT_RESPONSE_SIZE 8192
#define MONITOR_INTERVAL_MS 60000
#define WIFI_CREDENTIALS_PATH APP_DATA_PATH("credentials.bin")
#define WIFI_CREDENTIALS_MAGIC 0x57495743UL
#define WIFI_CREDENTIALS_VERSION 1

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
    WifiWorkerFlagForget = (1U << 2),
} WifiWorkerFlag;

typedef struct {
    WifiScreen screen;
    char networks[WIFI_MAX_NETWORKS][WIFI_SSID_SIZE];
    bool saved[WIFI_MAX_NETWORKS];
    size_t network_count;
    size_t selected_network;
    char status[32];
    char detail[48];
} WifiViewModel;

typedef struct {
    char password[WIFI_PASSWORD_SIZE];
    uint8_t mode;
    uint8_t row;
    uint8_t column;
} WifiPasswordModel;

typedef struct {
    char value;
    uint8_t x;
} WifiPasswordKey;

typedef struct {
    char ssid[WIFI_SSID_SIZE];
    char password[WIFI_PASSWORD_SIZE];
} WifiCredential;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
} WifiCredentialsHeader;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    View* password_view;
    NotificationApp* notification;
    Expansion* expansion;
    Storage* storage;

    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* worker;

    char ssid[WIFI_SSID_SIZE];
    char password[WIFI_PASSWORD_SIZE];
    char forget_ssid[WIFI_SSID_SIZE];
    WifiCredential credentials[WIFI_MAX_SAVED_NETWORKS];
    size_t credentials_count;
    bool monitor_active;
    bool was_online;
    bool credentials_save_failed;
    bool otg_was_enabled;
} WifiInternetWatch;

#define WIFI_PASSWORD_KEY_SWITCH    ((char)0x01)
#define WIFI_PASSWORD_KEY_BACKSPACE '\b'
#define WIFI_PASSWORD_KEY_ENTER     '\r'

static const WifiPasswordKey wifi_password_alpha_row_1[] = {
    {'q', 2},  {'w', 11}, {'e', 20}, {'r', 29}, {'t', 38}, {'y', 47}, {'u', 56},
    {'i', 65}, {'o', 74}, {'p', 83}, {'0', 93}, {'1', 103}, {'2', 113}, {'3', 123},
};

static const WifiPasswordKey wifi_password_alpha_row_2[] = {
    {'a', 2},
    {'s', 11},
    {'d', 20},
    {'f', 29},
    {'g', 38},
    {'h', 47},
    {'j', 56},
    {'k', 65},
    {'l', 74},
    {WIFI_PASSWORD_KEY_BACKSPACE, 84},
    {'4', 103},
    {'5', 113},
    {'6', 123},
};

static const WifiPasswordKey wifi_password_alpha_row_3[] = {
    {WIFI_PASSWORD_KEY_SWITCH, 2},
    {'z', 15},
    {'x', 23},
    {'c', 31},
    {'v', 39},
    {'b', 47},
    {'n', 55},
    {'m', 63},
    {' ', 71},
    {WIFI_PASSWORD_KEY_ENTER, 79},
    {'7', 103},
    {'8', 113},
    {'9', 123},
};

static const WifiPasswordKey wifi_password_symbol_row_1[] = {
    {'!', 2},  {'@', 12}, {'#', 22}, {'$', 32}, {'%', 42}, {'^', 52}, {'&', 62},
    {'*', 72}, {'(', 82}, {')', 92}, {'<', 103}, {'>', 113}, {'?', 123},
};

static const WifiPasswordKey wifi_password_symbol_row_2[] = {
    {'~', 2},
    {'+', 11},
    {'-', 20},
    {'_', 29},
    {'=', 38},
    {'[', 47},
    {']', 56},
    {'{', 65},
    {'}', 74},
    {WIFI_PASSWORD_KEY_BACKSPACE, 84},
    {'/', 103},
    {'\\', 113},
    {'|', 123},
};

static const WifiPasswordKey wifi_password_symbol_row_3[] = {
    {WIFI_PASSWORD_KEY_SWITCH, 2},
    {'.', 16},
    {',', 27},
    {';', 38},
    {':', 49},
    {'\'', 60},
    {'"', 70},
    {WIFI_PASSWORD_KEY_ENTER, 79},
    {'`', 103},
};

static const WifiPasswordKey* wifi_password_get_row(uint8_t mode, uint8_t row) {
    if(mode == 2) {
        if(row == 0) return wifi_password_symbol_row_1;
        if(row == 1) return wifi_password_symbol_row_2;
        return wifi_password_symbol_row_3;
    }

    if(row == 0) return wifi_password_alpha_row_1;
    if(row == 1) return wifi_password_alpha_row_2;
    return wifi_password_alpha_row_3;
}

static size_t wifi_password_get_row_size(uint8_t mode, uint8_t row) {
    if(mode == 2) {
        if(row == 0) return COUNT_OF(wifi_password_symbol_row_1);
        if(row == 1) return COUNT_OF(wifi_password_symbol_row_2);
        return COUNT_OF(wifi_password_symbol_row_3);
    }

    if(row == 0) return COUNT_OF(wifi_password_alpha_row_1);
    if(row == 1) return COUNT_OF(wifi_password_alpha_row_2);
    return COUNT_OF(wifi_password_alpha_row_3);
}

static int32_t wifi_credential_index(const WifiInternetWatch* app, const char* ssid) {
    for(size_t i = 0; i < app->credentials_count; i++) {
        if(strcmp(app->credentials[i].ssid, ssid) == 0) return (int32_t)i;
    }
    return -1;
}

static bool wifi_credentials_save(WifiInternetWatch* app) {
    File* file = storage_file_alloc(app->storage);
    const WifiCredentialsHeader header = {
        .magic = WIFI_CREDENTIALS_MAGIC,
        .version = WIFI_CREDENTIALS_VERSION,
        .count = app->credentials_count,
    };

    bool success = storage_file_open(
        file, WIFI_CREDENTIALS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        success =
            storage_file_write(file, &header, sizeof(header)) == sizeof(header);
    }
    if(success && app->credentials_count > 0) {
        const size_t credentials_size =
            app->credentials_count * sizeof(WifiCredential);
        success =
            storage_file_write(file, app->credentials, credentials_size) ==
            credentials_size;
    }
    if(success) success = storage_file_sync(file);

    storage_file_close(file);
    storage_file_free(file);

    if(!success) FURI_LOG_E(TAG, "Failed to save Wi-Fi credentials");
    return success;
}

static void wifi_credentials_load(WifiInternetWatch* app) {
    File* file = storage_file_alloc(app->storage);
    WifiCredentialsHeader header;

    bool success = storage_file_open(
        file, WIFI_CREDENTIALS_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        success = storage_file_read(file, &header, sizeof(header)) == sizeof(header) &&
                  header.magic == WIFI_CREDENTIALS_MAGIC &&
                  header.version == WIFI_CREDENTIALS_VERSION &&
                  header.count <= WIFI_MAX_SAVED_NETWORKS;
    }

    if(success && header.count > 0) {
        const size_t credentials_size = header.count * sizeof(WifiCredential);
        success =
            storage_file_read(file, app->credentials, credentials_size) ==
            credentials_size;
    }

    storage_file_close(file);
    storage_file_free(file);

    if(!success) {
        app->credentials_count = 0;
        memset(app->credentials, 0, sizeof(app->credentials));
        return;
    }

    app->credentials_count = header.count;
    for(size_t i = 0; i < app->credentials_count; i++) {
        app->credentials[i].ssid[WIFI_SSID_SIZE - 1] = '\0';
        app->credentials[i].password[WIFI_PASSWORD_SIZE - 1] = '\0';
    }
}

static bool wifi_credentials_upsert(
    WifiInternetWatch* app,
    const char* ssid,
    const char* password) {
    int32_t index = wifi_credential_index(app, ssid);
    const size_t old_count = app->credentials_count;
    WifiCredential old_credential = {0};

    if(index >= 0) {
        old_credential = app->credentials[index];
    } else {
        if(app->credentials_count >= WIFI_MAX_SAVED_NETWORKS) return false;
        index = app->credentials_count++;
    }

    strlcpy(app->credentials[index].ssid, ssid, WIFI_SSID_SIZE);
    strlcpy(app->credentials[index].password, password, WIFI_PASSWORD_SIZE);

    if(wifi_credentials_save(app)) return true;

    app->credentials_count = old_count;
    if((size_t)index < old_count) app->credentials[index] = old_credential;
    return false;
}

static bool wifi_credentials_delete(WifiInternetWatch* app, const char* ssid) {
    const int32_t index = wifi_credential_index(app, ssid);
    if(index < 0) return true;

    const WifiCredential deleted_credential = app->credentials[index];
    const size_t old_count = app->credentials_count;

    for(size_t i = index; i + 1 < app->credentials_count; i++) {
        app->credentials[i] = app->credentials[i + 1];
    }
    app->credentials_count--;
    memset(&app->credentials[app->credentials_count], 0, sizeof(WifiCredential));

    if(wifi_credentials_save(app)) return true;

    app->credentials_count = old_count;
    for(size_t i = old_count - 1; i > (size_t)index; i--) {
        app->credentials[i] = app->credentials[i - 1];
    }
    app->credentials[index] = deleted_credential;
    return false;
}

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

static void wifi_set_network_saved(
    WifiInternetWatch* app,
    const char* ssid,
    bool saved) {
    with_view_model(
        app->main_view,
        WifiViewModel * model,
        {
            for(size_t i = 0; i < model->network_count; i++) {
                if(strcmp(model->networks[i], ssid) == 0) {
                    model->saved[i] = saved;
                    break;
                }
            }
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
    wifi_at_command(app, "AT+SYSSTORE=1", response, AT_RESPONSE_SIZE, 2000);
    wifi_at_command(app, "AT+CWAUTOCONN=0", response, AT_RESPONSE_SIZE, 2000);
    wifi_at_command(app, "AT+SYSSTORE=0", response, AT_RESPONSE_SIZE, 2000);

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
                model->saved[i] =
                    wifi_credential_index(app, networks[i]) >= 0;
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
    const int32_t credential_index = wifi_credential_index(app, app->ssid);
    const bool password_is_saved =
        credential_index >= 0 &&
        strcmp(app->credentials[credential_index].password, app->password) == 0;
    const bool joined = wifi_is_joined(app, response, response_size);

    if(joined && strstr(response, app->ssid) != NULL && password_is_saved) {
        return true;
    }
    if(joined) {
        wifi_at_command(app, "AT+CWQAP", response, response_size, 3000);
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

    const int32_t credential_index = wifi_credential_index(app, app->ssid);
    if(credential_index < 0 ||
       strcmp(app->credentials[credential_index].password, app->password) != 0) {
        app->credentials_save_failed =
            !wifi_credentials_upsert(app, app->ssid, app->password);
        if(!app->credentials_save_failed) {
            wifi_set_network_saved(app, app->ssid, true);
        }
    } else {
        app->credentials_save_failed = false;
    }

    wifi_set_status(app, WifiScreenStatus, "Checking internet...", "1.1.1.1");
    const bool online =
        wifi_at_command(app, "AT+PING=\"1.1.1.1\"", response, sizeof(response), 10000) &&
        strstr(response, "+PING:") != NULL;

    if(online) {
        wifi_set_status(
            app,
            WifiScreenStatus,
            app->credentials_save_failed ? "ONLINE - NOT SAVED" : "ONLINE",
            app->ssid);
        notification_message(app->notification, &sequence_set_only_green_255);
        if(!app->was_online) {
            notification_message(app->notification, &sequence_success);
        }
    } else {
        wifi_set_status(
            app,
            WifiScreenStatus,
            app->credentials_save_failed ? "OFFLINE - NOT SAVED" : "OFFLINE",
            app->ssid);
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
            WifiWorkerFlagStop | WifiWorkerFlagConnect | WifiWorkerFlagForget,
            FuriFlagWaitAny,
            timeout);

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

        if(flags & WifiWorkerFlagForget) {
            char response[256];
            app->monitor_active = false;
            app->was_online = false;
            if(wifi_is_joined(app, response, sizeof(response)) &&
               strstr(response, app->forget_ssid) != NULL) {
                wifi_at_command(app, "AT+CWQAP", response, sizeof(response), 3000);
            }
            notification_message(app->notification, &sequence_reset_rgb);
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
            if(model->saved[index]) canvas_draw_str(canvas, 120, y, "*");
            if(index == model->selected_network) canvas_set_color(canvas, ColorBlack);
        }

        elements_button_center(canvas, "Join");
        if(model->saved[model->selected_network]) {
            elements_button_right(canvas, "Forget");
        }
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

    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong) {
        return false;
    }

    bool consumed = false;
    bool show_password = false;
    bool connect_saved = false;
    bool forget_network = false;

    with_view_model(
        app->main_view,
        WifiViewModel * model,
        {
            if(model->screen == WifiScreenNetworkList && model->network_count > 0) {
                if(event->key == InputKeyUp && event->type != InputTypeLong) {
                    if(model->selected_network > 0) model->selected_network--;
                    consumed = true;
                } else if(event->key == InputKeyDown && event->type != InputTypeLong) {
                    if(model->selected_network + 1 < model->network_count) {
                        model->selected_network++;
                    }
                    consumed = true;
                } else if(event->key == InputKeyOk &&
                          (event->type == InputTypeShort ||
                           event->type == InputTypeLong)) {
                    strlcpy(
                        app->ssid,
                        model->networks[model->selected_network],
                        sizeof(app->ssid));
                    if(event->type == InputTypeShort &&
                       model->saved[model->selected_network]) {
                        connect_saved = true;
                    } else {
                        show_password = true;
                    }
                    consumed = true;
                } else if(
                    event->key == InputKeyRight && event->type == InputTypeShort &&
                    model->saved[model->selected_network]) {
                    strlcpy(
                        app->forget_ssid,
                        model->networks[model->selected_network],
                        sizeof(app->forget_ssid));
                    forget_network = true;
                    consumed = true;
                }
            }
        },
        consumed);

    if(connect_saved) {
        const int32_t index = wifi_credential_index(app, app->ssid);
        if(index >= 0) {
            strlcpy(
                app->password,
                app->credentials[index].password,
                sizeof(app->password));
            wifi_set_status(app, WifiScreenStatus, "Connecting Wi-Fi...", app->ssid);
            furi_thread_flags_set(
                furi_thread_get_id(app->worker), WifiWorkerFlagConnect);
        } else {
            wifi_set_network_saved(app, app->ssid, false);
            show_password = true;
        }
    }

    if(show_password) {
        app->password[0] = '\0';
        const int32_t index = wifi_credential_index(app, app->ssid);
        with_view_model(
            app->password_view,
            WifiPasswordModel * model,
            {
                memset(model, 0, sizeof(WifiPasswordModel));
                if(index >= 0) {
                    strlcpy(
                        model->password,
                        app->credentials[index].password,
                        sizeof(model->password));
                }
            },
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewPassword);
    }

    if(forget_network) {
        if(wifi_credentials_delete(app, app->forget_ssid)) {
            wifi_set_network_saved(app, app->forget_ssid, false);
            furi_thread_flags_set(
                furi_thread_get_id(app->worker), WifiWorkerFlagForget);
        } else {
            wifi_set_status(
                app,
                WifiScreenStatus,
                "DELETE FAILED",
                "Check the SD card");
        }
    }

    return consumed;
}

static uint32_t wifi_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void wifi_password_draw_callback(Canvas* canvas, void* context) {
    WifiPasswordModel* model = context;
    char password_text[WIFI_PASSWORD_SIZE];
    const size_t password_size = strlen(model->password);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "Wi-Fi password");
    elements_slightly_rounded_frame(canvas, 1, 11, 126, 15);

    const size_t visible_size = MIN(password_size, sizeof(password_text) - 1);
    for(size_t i = 0; i < visible_size; i++) {
        password_text[i] = '*';
    }
    password_text[visible_size] = '\0';
    canvas_draw_str(canvas, 4, 21, password_text);

    canvas_set_font(canvas, FontKeyboard);
    for(uint8_t row = 0; row < 3; row++) {
        const WifiPasswordKey* keys = wifi_password_get_row(model->mode, row);
        const size_t row_size = wifi_password_get_row_size(model->mode, row);
        const uint8_t y = 36 + row * 12;

        for(size_t column = 0; column < row_size; column++) {
            const bool selected = model->row == row && model->column == column;
            const char value = keys[column].value;
            const uint8_t x = keys[column].x;

            if(value == WIFI_PASSWORD_KEY_BACKSPACE) {
                canvas_set_font(canvas, FontSecondary);
                if(selected) canvas_draw_box(canvas, x - 2, y - 9, 16, 10);
                canvas_set_color(canvas, selected ? ColorWhite : ColorBlack);
                canvas_draw_str(canvas, x, y, "<-");
            } else if(value == WIFI_PASSWORD_KEY_ENTER) {
                canvas_set_font(canvas, FontSecondary);
                if(selected) canvas_draw_box(canvas, x - 2, y - 9, 20, 10);
                canvas_set_color(canvas, selected ? ColorWhite : ColorBlack);
                canvas_draw_str(canvas, x, y, "OK");
            } else if(value == WIFI_PASSWORD_KEY_SWITCH) {
                canvas_set_font(canvas, FontSecondary);
                if(selected) canvas_draw_box(canvas, x - 1, y - 9, 12, 10);
                canvas_set_color(canvas, selected ? ColorWhite : ColorBlack);
                canvas_draw_str(canvas, x, y, model->mode == 0 ? "Aa" : model->mode == 1 ? "#?" : "ab");
            } else {
                canvas_set_font(canvas, FontKeyboard);
                if(selected) canvas_draw_box(canvas, x - 1, y - 8, 7, 10);
                canvas_set_color(canvas, selected ? ColorWhite : ColorBlack);
                char display = value;
                if(model->mode == 1 && display >= 'a' && display <= 'z') {
                    display = display - ('a' - 'A');
                }
                canvas_draw_glyph(canvas, x, y, display);
            }

            canvas_set_color(canvas, ColorBlack);
            canvas_set_font(canvas, FontKeyboard);
        }
    }
}

static bool wifi_password_input_callback(InputEvent* event, void* context) {
    WifiInternetWatch* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = true;
    bool connect = false;
    bool return_to_networks = false;

    with_view_model(
        app->password_view,
        WifiPasswordModel * model,
        {
            const WifiPasswordKey* row = wifi_password_get_row(model->mode, model->row);
            const size_t row_size = wifi_password_get_row_size(model->mode, model->row);
            const size_t password_size = strlen(model->password);

            if(event->key == InputKeyLeft) {
                model->column =
                    model->column == 0 ? (uint8_t)(row_size - 1) :
                                         (uint8_t)(model->column - 1);
            } else if(event->key == InputKeyRight) {
                model->column = (model->column + 1) % row_size;
            } else if(event->key == InputKeyUp) {
                model->row = model->row == 0 ? 2 : model->row - 1;
                const size_t new_row_size = wifi_password_get_row_size(model->mode, model->row);
                if(model->column >= new_row_size) model->column = new_row_size - 1;
            } else if(event->key == InputKeyDown) {
                model->row = (model->row + 1) % 3;
                const size_t new_row_size = wifi_password_get_row_size(model->mode, model->row);
                if(model->column >= new_row_size) model->column = new_row_size - 1;
            } else if(event->key == InputKeyOk) {
                char value = row[model->column].value;
                if(value == WIFI_PASSWORD_KEY_SWITCH) {
                    model->mode = (model->mode + 1) % 3;
                    const size_t new_row_size =
                        wifi_password_get_row_size(model->mode, model->row);
                    if(model->column >= new_row_size) model->column = new_row_size - 1;
                } else if(value == WIFI_PASSWORD_KEY_BACKSPACE) {
                    if(password_size > 0) model->password[password_size - 1] = '\0';
                } else if(value == WIFI_PASSWORD_KEY_ENTER) {
                    strlcpy(app->password, model->password, sizeof(app->password));
                    connect = true;
                } else if(password_size + 1 < sizeof(model->password)) {
                    if(model->mode == 1 && value >= 'a' && value <= 'z') {
                        value = value - ('a' - 'A');
                    }
                    model->password[password_size] = value;
                    model->password[password_size + 1] = '\0';
                }
            } else if(event->key == InputKeyBack &&
                      (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
                if(password_size > 0) {
                    model->password[password_size - 1] = '\0';
                } else {
                    return_to_networks = true;
                }
            } else {
                consumed = false;
            }
        },
        consumed);

    if(connect) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WifiEventPasswordDone);
    } else if(return_to_networks) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewMain);
    }

    return consumed;
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
    app->storage = furi_record_open(RECORD_STORAGE);
    wifi_credentials_load(app);

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

    app->password_view = view_alloc();
    view_allocate_model(
        app->password_view, ViewModelTypeLocking, sizeof(WifiPasswordModel));
    view_set_context(app->password_view, app);
    view_set_draw_callback(app->password_view, wifi_password_draw_callback);
    view_set_input_callback(app->password_view, wifi_password_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, WifiViewPassword, app->password_view);

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
    view_free(app->password_view);
    view_dispatcher_remove_view(app->view_dispatcher, WifiViewMain);
    view_free(app->main_view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
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

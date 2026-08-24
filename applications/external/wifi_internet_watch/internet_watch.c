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
#include <stdlib.h>

#define TAG "WifiInternetWatch"

#define WIFI_MAX_NETWORKS          16
#define WIFI_SSID_SIZE             33
#define WIFI_PASSWORD_SIZE         65
#define WIFI_MAX_SAVED_NETWORKS    16
#define AT_RESPONSE_SIZE           8192
#define MONITOR_INTERVAL_MS        60000
#define WIFI_CREDENTIALS_PATH      APP_DATA_PATH("credentials.bin")
#define WIFI_SETTINGS_PATH         APP_DATA_PATH("settings.bin")
#define WIFI_CREDENTIALS_MAGIC     0x57495743UL
#define WIFI_CREDENTIALS_VERSION   1
#define WIFI_SETTINGS_MAGIC        0x57495354UL
#define WIFI_SETTINGS_VERSION      2
#define WIFI_DEFAULT_PING_IP       "1.1.1.1"
#define WIFI_DEFAULT_TIMER_SECONDS 60

typedef enum {
    WifiScreenScanning,
    WifiScreenNetworkList,
    WifiScreenStatus,
} WifiScreen;

typedef enum {
    WifiViewMain,
    WifiViewSettings,
    WifiViewPassword,
    WifiViewIp,
    WifiViewTimer,
} WifiView;

typedef enum {
    WifiEventPasswordDone = 1,
    WifiEventIpDone = 2,
} WifiEvent;

typedef enum {
    WifiWorkerFlagStop = (1U << 0),
    WifiWorkerFlagConnect = (1U << 1),
    WifiWorkerFlagForget = (1U << 2),
    WifiWorkerFlagRescan = (1U << 3),
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
    char ip[16];
    uint8_t row;
    uint8_t column;
} WifiIpModel;

typedef struct {
    uint8_t row;
} WifiSettingsMenuModel;

typedef struct {
    char value[8];
    uint8_t row;
    uint8_t column;
} WifiTimerModel;

typedef struct {
    char value;
    uint8_t x;
} WifiIpKey;

typedef struct {
    uint32_t magic;
    uint16_t version;
    char ping_ip[16];
    uint16_t timer_seconds;
} WifiSettings;

typedef struct {
    uint32_t magic;
    uint16_t version;
    char ping_ip[16];
    uint16_t timer_seconds;
} WifiSettingsLegacy;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
} WifiCredentialsHeader;

static bool wifi_ip_is_valid(const char* ip);

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    View* settings_view;
    View* password_view;
    View* ip_view;
    View* timer_view;
    NotificationApp* notification;
    Expansion* expansion;
    Storage* storage;

    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriThread* worker;

    char ssid[WIFI_SSID_SIZE];
    char password[WIFI_PASSWORD_SIZE];
    char forget_ssid[WIFI_SSID_SIZE];
    char ping_ip[16];
    uint16_t timer_seconds;
    WifiCredential credentials[WIFI_MAX_SAVED_NETWORKS];
    size_t credentials_count;
    bool monitor_active;
    bool was_online;
    bool was_joined;
    bool blink_on;
    bool blink_is_green;
    bool credentials_save_failed;
    bool otg_was_enabled;
} WifiInternetWatch;

static bool wifi_timer_is_valid(uint32_t timer_seconds);
static void wifi_sync_settings_views(WifiInternetWatch* app);
static void wifi_reset_settings(WifiInternetWatch* app);

#define WIFI_PASSWORD_KEY_SWITCH    ((char)0x01)
#define WIFI_PASSWORD_KEY_BACKSPACE '\b'
#define WIFI_PASSWORD_KEY_ENTER     '\r'

static const WifiPasswordKey wifi_password_alpha_row_1[] = {
    {'q', 2},
    {'w', 11},
    {'e', 20},
    {'r', 29},
    {'t', 38},
    {'y', 47},
    {'u', 56},
    {'i', 65},
    {'o', 74},
    {'p', 83},
    {'0', 93},
    {'1', 103},
    {'2', 113},
    {'3', 123},
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
    {'!', 2},
    {'@', 12},
    {'#', 22},
    {'$', 32},
    {'%', 42},
    {'^', 52},
    {'&', 62},
    {'*', 72},
    {'(', 82},
    {')', 92},
    {'<', 103},
    {'>', 113},
    {'?', 123},
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

    bool success = storage_file_open(file, WIFI_CREDENTIALS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        success = storage_file_write(file, &header, sizeof(header)) == sizeof(header);
    }
    if(success && app->credentials_count > 0) {
        const size_t credentials_size = app->credentials_count * sizeof(WifiCredential);
        success = storage_file_write(file, app->credentials, credentials_size) == credentials_size;
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

    bool success = storage_file_open(file, WIFI_CREDENTIALS_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        success = storage_file_read(file, &header, sizeof(header)) == sizeof(header) &&
                  header.magic == WIFI_CREDENTIALS_MAGIC &&
                  header.version == WIFI_CREDENTIALS_VERSION &&
                  header.count <= WIFI_MAX_SAVED_NETWORKS;
    }

    if(success && header.count > 0) {
        const size_t credentials_size = header.count * sizeof(WifiCredential);
        success = storage_file_read(file, app->credentials, credentials_size) == credentials_size;
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

static bool
    wifi_credentials_upsert(WifiInternetWatch* app, const char* ssid, const char* password) {
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

static void wifi_settings_load(WifiInternetWatch* app) {
    File* file = storage_file_alloc(app->storage);
    WifiSettings settings = {
        .magic = WIFI_SETTINGS_MAGIC,
        .version = WIFI_SETTINGS_VERSION,
        .timer_seconds = WIFI_DEFAULT_TIMER_SECONDS,
    };
    strlcpy(settings.ping_ip, WIFI_DEFAULT_PING_IP, sizeof(settings.ping_ip));

    bool success = storage_file_open(file, WIFI_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        const size_t read_size = storage_file_read(file, &settings, sizeof(settings));
        success = settings.magic == WIFI_SETTINGS_MAGIC &&
                  ((read_size == sizeof(settings) && settings.version == WIFI_SETTINGS_VERSION) ||
                   (read_size == sizeof(WifiSettingsLegacy) && settings.version == 1));
    }

    storage_file_close(file);
    storage_file_free(file);

    if(!success) {
        strlcpy(app->ping_ip, WIFI_DEFAULT_PING_IP, sizeof(app->ping_ip));
        app->timer_seconds = WIFI_DEFAULT_TIMER_SECONDS;
        return;
    }

    strlcpy(app->ping_ip, settings.ping_ip, sizeof(app->ping_ip));
    if(app->ping_ip[0] == '\0' || !wifi_ip_is_valid(app->ping_ip)) {
        FURI_LOG_W(TAG, "Invalid saved Wi-Fi settings IP, falling back to default");
        strlcpy(app->ping_ip, WIFI_DEFAULT_PING_IP, sizeof(app->ping_ip));
    }

    if(settings.timer_seconds == 0 || settings.timer_seconds > 3600) {
        app->timer_seconds = WIFI_DEFAULT_TIMER_SECONDS;
    } else {
        app->timer_seconds = settings.timer_seconds;
    }
}

static bool wifi_settings_save(WifiInternetWatch* app) {
    File* file = storage_file_alloc(app->storage);
    WifiSettings settings = {
        .magic = WIFI_SETTINGS_MAGIC,
        .version = WIFI_SETTINGS_VERSION,
        .timer_seconds = app->timer_seconds,
    };
    strlcpy(settings.ping_ip, app->ping_ip, sizeof(settings.ping_ip));

    bool success = storage_file_open(file, WIFI_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        success = storage_file_write(file, &settings, sizeof(settings)) == sizeof(settings);
    }
    if(success) success = storage_file_sync(file);

    storage_file_close(file);
    storage_file_free(file);

    if(!success) FURI_LOG_E(TAG, "Failed to save Wi-Fi settings");
    return success;
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

static void wifi_stop_blink(WifiInternetWatch* app) {
    app->blink_on = false;
    app->blink_is_green = false;
    notification_message(app->notification, &sequence_reset_rgb);
}

static void wifi_set_online_led(WifiInternetWatch* app) {
    app->blink_on = true;
    app->blink_is_green = true;
    notification_message(app->notification, &sequence_set_only_green_255);
}

static void wifi_start_offline_blink(WifiInternetWatch* app) {
    app->blink_on = true;
    app->blink_is_green = false;
    notification_message(app->notification, &sequence_set_only_red_255);
}

static void wifi_tick_offline_blink(WifiInternetWatch* app) {
    if(app->blink_is_green) return;

    app->blink_on = !app->blink_on;
    if(app->blink_on) {
        notification_message(app->notification, &sequence_set_only_red_255);
    } else {
        notification_message(app->notification, &sequence_reset_rgb);
    }
}

static void wifi_settings_menu_draw_callback(Canvas* canvas, void* context) {
    WifiSettingsMenuModel* model = context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 12, "Settings");
    canvas_draw_line(canvas, 0, 15, 127, 15);

    canvas_set_font(canvas, FontSecondary);

    const char* items[] = {"IP Address", "Timer", "Reset All"};
    for(uint8_t row = 0; row < 3; row++) {
        const uint8_t y = 27 + row * 12;
        if(model->row == row) {
            canvas_draw_box(canvas, 3, y - 9, 122, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 6, y, items[row]);
        canvas_set_color(canvas, ColorBlack);
    }

    elements_button_center(canvas, "Open");
}

static bool wifi_settings_menu_input_callback(InputEvent* event, void* context) {
    WifiInternetWatch* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong) {
        return false;
    }

    bool consumed = true;
    bool open_ip = false;
    bool open_timer = false;
    bool reset_all = false;
    bool exit_menu = false;

    with_view_model(
        app->settings_view,
        WifiSettingsMenuModel * model,
        {
            if(event->key == InputKeyUp && event->type != InputTypeLong) {
                model->row = model->row == 0 ? 2 : model->row - 1;
            } else if(event->key == InputKeyDown && event->type != InputTypeLong) {
                model->row = (model->row + 1) % 3;
            } else if(event->key == InputKeyOk) {
                if(model->row == 0) {
                    open_ip = true;
                } else if(model->row == 1) {
                    open_timer = true;
                } else {
                    reset_all = true;
                }
            } else if(event->key == InputKeyBack) {
                exit_menu = true;
            } else {
                consumed = false;
            }
        },
        consumed);

    if(open_ip) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewIp);
    } else if(open_timer) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewTimer);
    } else if(reset_all) {
        wifi_reset_settings(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewMain);
    } else if(exit_menu) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewMain);
    }

    return consumed;
}

static void wifi_timer_draw_callback(Canvas* canvas, void* context) {
    WifiTimerModel* model = context;
    static const WifiIpKey row_1[] = {{'1', 8}, {'2', 22}, {'3', 36}};
    static const WifiIpKey row_2[] = {{'4', 8}, {'5', 22}, {'6', 36}};
    static const WifiIpKey row_3[] = {{'7', 8}, {'8', 22}, {'9', 36}};
    static const WifiIpKey row_4[] = {{WIFI_PASSWORD_KEY_BACKSPACE, 8}, {'0', 22}, {'S', 102}};

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "Timer (sec)");
    elements_slightly_rounded_frame(canvas, 1, 11, 126, 10);
    canvas_draw_str(canvas, 4, 19, model->value);
    canvas_set_font(canvas, FontKeyboard);

    const WifiIpKey* rows[] = {row_1, row_2, row_3, row_4};
    for(uint8_t row = 0; row < 4; row++) {
        const WifiIpKey* keys = rows[row];
        const uint8_t y = 31 + row * 8;
        for(uint8_t column = 0; column < 3; column++) {
            const bool selected = model->row == row && model->column == column;
            const char value = keys[column].value;
            if(selected) {
                canvas_draw_box(canvas, keys[column].x - 1, y - 7, value == 'S' ? 22 : 8, 9);
            }
            canvas_set_color(canvas, selected ? ColorWhite : ColorBlack);
            if(value == WIFI_PASSWORD_KEY_BACKSPACE) {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, keys[column].x, y, "<-");
                canvas_set_font(canvas, FontKeyboard);
            } else if(value == 'S') {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, keys[column].x, y, "Save");
                canvas_set_font(canvas, FontKeyboard);
            } else {
                canvas_draw_glyph(canvas, keys[column].x, y, value);
            }
            canvas_set_color(canvas, ColorBlack);
        }
    }
}

static bool wifi_timer_input_callback(InputEvent* event, void* context) {
    WifiInternetWatch* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong) {
        return false;
    }

    bool consumed = true;
    bool save = false;
    bool cancel = false;

    with_view_model(
        app->timer_view,
        WifiTimerModel * model,
        {
            const size_t len = strlen(model->value);
            if(event->key == InputKeyLeft) {
                model->column = model->column == 0 ? 2 : model->column - 1;
            } else if(event->key == InputKeyRight) {
                model->column = (model->column + 1) % 3;
            } else if(event->key == InputKeyUp) {
                model->row = model->row == 0 ? 3 : model->row - 1;
            } else if(event->key == InputKeyDown) {
                model->row = (model->row + 1) % 4;
            } else if(event->key == InputKeyOk) {
                if(model->row == 3 && model->column == 2) {
                    save = true;
                } else {
                    const char value =
                        (model->row == 3) ?
                            (model->column == 0 ? WIFI_PASSWORD_KEY_BACKSPACE : '0') :
                            (char)('1' + model->row * 3 + model->column);
                    if(value == WIFI_PASSWORD_KEY_BACKSPACE) {
                        if(len > 0)
                            model->value[len - 1] = '\0';
                        else
                            cancel = true;
                    } else if(len + 1 < sizeof(model->value)) {
                        model->value[len] = value;
                        model->value[len + 1] = '\0';
                    }
                }
            } else if(event->key == InputKeyBack) {
                cancel = true;
            } else {
                consumed = false;
            }
        },
        consumed);

    if(save) {
        with_view_model(
            app->timer_view,
            WifiTimerModel * model,
            {
                const uint32_t timer_seconds = (uint32_t)strtoul(model->value, NULL, 10);
                if(wifi_timer_is_valid(timer_seconds)) {
                    app->timer_seconds = (uint16_t)timer_seconds;
                    wifi_settings_save(app);
                    wifi_sync_settings_views(app);
                    view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewSettings);
                } else {
                    wifi_set_status(app, WifiScreenStatus, "INVALID TIMER", "Use 1..3600 sec");
                }
            },
            false);
    } else if(cancel) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewSettings);
    }

    return consumed;
}

static void wifi_notify_connection_lost(WifiInternetWatch* app) {
    notification_message(app->notification, &sequence_error);
}

static void wifi_set_network_saved(WifiInternetWatch* app, const char* ssid, bool saved) {
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

static bool wifi_ip_is_valid(const char* ip) {
    int parts = 0;
    const char* cursor = ip;

    while(*cursor) {
        if(parts >= 4) return false;
        int value = 0;
        int digits = 0;
        while(*cursor >= '0' && *cursor <= '9') {
            value = value * 10 + (*cursor - '0');
            if(value > 255) return false;
            cursor++;
            digits++;
        }
        if(digits == 0) return false;
        parts++;
        if(*cursor == '\0') break;
        if(*cursor != '.') return false;
        cursor++;
    }

    return parts == 4;
}

static bool wifi_timer_is_valid(uint32_t timer_seconds) {
    return timer_seconds >= 1 && timer_seconds <= 3600;
}

static void wifi_sync_settings_views(WifiInternetWatch* app) {
    with_view_model(
        app->ip_view,
        WifiIpModel * model,
        {
            memset(model, 0, sizeof(WifiIpModel));
            strlcpy(model->ip, app->ping_ip, sizeof(model->ip));
        },
        false);

    with_view_model(
        app->timer_view,
        WifiTimerModel * model,
        {
            memset(model, 0, sizeof(WifiTimerModel));
            snprintf(model->value, sizeof(model->value), "%u", (unsigned)app->timer_seconds);
        },
        false);
}

static void wifi_reset_settings(WifiInternetWatch* app) {
    strlcpy(app->ping_ip, WIFI_DEFAULT_PING_IP, sizeof(app->ping_ip));
    app->timer_seconds = WIFI_DEFAULT_TIMER_SECONDS;
    memset(app->credentials, 0, sizeof(app->credentials));
    app->credentials_count = 0;

    wifi_settings_save(app);
    wifi_credentials_save(app);
    wifi_sync_settings_views(app);

    with_view_model(
        app->main_view,
        WifiViewModel * model,
        {
            for(size_t i = 0; i < model->network_count; i++) {
                model->saved[i] = false;
            }
            strlcpy(model->status, "SETTINGS RESET", sizeof(model->status));
            strlcpy(model->detail, "IP, timer, networks cleared", sizeof(model->detail));
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

static size_t
    wifi_parse_scan(const char* response, char networks[WIFI_MAX_NETWORKS][WIFI_SSID_SIZE]) {
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

    const bool scan_ok = wifi_at_command(app, "AT+CWLAP", response, AT_RESPONSE_SIZE, 20000);
    const size_t network_count = scan_ok ? wifi_parse_scan(response, networks) : 0;

    with_view_model(
        app->main_view,
        WifiViewModel * model,
        {
            model->network_count = network_count;
            model->selected_network = 0;
            for(size_t i = 0; i < network_count; i++) {
                strlcpy(model->networks[i], networks[i], WIFI_SSID_SIZE);
                model->saved[i] = wifi_credential_index(app, networks[i]) >= 0;
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

    snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"", escaped_ssid, escaped_password);

    return wifi_at_command(app, command, response, response_size, 25000);
}

static void wifi_monitor_once(WifiInternetWatch* app) {
    char response[1024];
    char command[64];

    wifi_stop_blink(app);
    wifi_set_status(app, WifiScreenStatus, "Connecting Wi-Fi...", app->ssid);
    if(!wifi_join(app, response, sizeof(response))) {
        wifi_start_offline_blink(app);
        wifi_notify_connection_lost(app);
        wifi_set_status(app, WifiScreenStatus, "NO WI-FI", app->ssid);
        return;
    }

    app->was_joined = true;

    const int32_t credential_index = wifi_credential_index(app, app->ssid);
    if(credential_index < 0 ||
       strcmp(app->credentials[credential_index].password, app->password) != 0) {
        app->credentials_save_failed = !wifi_credentials_upsert(app, app->ssid, app->password);
        if(!app->credentials_save_failed) {
            wifi_set_network_saved(app, app->ssid, true);
        }
    } else {
        app->credentials_save_failed = false;
    }

    wifi_set_status(app, WifiScreenStatus, "Checking internet...", app->ping_ip);
    snprintf(command, sizeof(command), "AT+PING=\"%s\"", app->ping_ip);
    const bool online = wifi_at_command(app, command, response, sizeof(response), 10000) &&
                        strstr(response, "+PING:") != NULL;

    if(online) {
        wifi_set_status(
            app,
            WifiScreenStatus,
            app->credentials_save_failed ? "ONLINE - NOT SAVED" : "ONLINE",
            app->ssid);
        wifi_set_online_led(app);
        if(!app->was_online) {
            notification_message(app->notification, &sequence_success);
        }
    } else {
        wifi_start_offline_blink(app);
        wifi_set_status(
            app,
            WifiScreenStatus,
            app->credentials_save_failed ? "OFFLINE - NOT SAVED" : "OFFLINE",
            app->ssid);
        if(app->was_online) {
            wifi_notify_connection_lost(app);
        }
    }

    app->was_online = online;
}

static int32_t wifi_worker(void* context) {
    WifiInternetWatch* app = context;
    uint32_t last_monitor_tick = furi_get_tick();
    uint32_t last_blink_tick = last_monitor_tick;
    wifi_scan(app);

    while(true) {
        uint32_t timeout = FuriWaitForever;
        if(app->monitor_active) {
            const uint32_t now = furi_get_tick();
            const uint32_t monitor_interval_ms = (uint32_t)app->timer_seconds * 1000U;
            const uint32_t monitor_elapsed = now - last_monitor_tick;
            const uint32_t monitor_timeout =
                monitor_elapsed >= monitor_interval_ms ? 0 : monitor_interval_ms - monitor_elapsed;

            if(app->was_online) {
                timeout = monitor_timeout;
            } else {
                const uint32_t blink_elapsed = now - last_blink_tick;
                const uint32_t blink_timeout = blink_elapsed >= 1000U ? 0 : 1000U - blink_elapsed;
                timeout = monitor_timeout < blink_timeout ? monitor_timeout : blink_timeout;
            }
        }

        const uint32_t flags = furi_thread_flags_wait(
            WifiWorkerFlagStop | WifiWorkerFlagConnect | WifiWorkerFlagForget |
                WifiWorkerFlagRescan,
            FuriFlagWaitAny,
            timeout);

        if(flags & WifiWorkerFlagStop) break;

        if(flags == (uint32_t)FuriFlagErrorTimeout) {
            if(app->monitor_active) {
                const uint32_t now = furi_get_tick();
                const uint32_t monitor_interval_ms = (uint32_t)app->timer_seconds * 1000U;

                if(now - last_monitor_tick >= monitor_interval_ms) {
                    wifi_monitor_once(app);
                    last_monitor_tick = furi_get_tick();
                    if(!app->was_online) last_blink_tick = last_monitor_tick;
                } else if(!app->was_online && now - last_blink_tick >= 1000U) {
                    wifi_tick_offline_blink(app);
                    last_blink_tick = now;
                }
            }
            continue;
        }

        if(flags & WifiWorkerFlagConnect) {
            app->monitor_active = true;
            app->was_online = false;
            wifi_monitor_once(app);
            last_monitor_tick = furi_get_tick();
            last_blink_tick = last_monitor_tick;
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
            last_monitor_tick = furi_get_tick();
            last_blink_tick = last_monitor_tick;
        }

        if(flags & WifiWorkerFlagRescan) {
            app->monitor_active = false;
            app->was_online = false;
            wifi_stop_blink(app);
            wifi_scan(app);
            last_monitor_tick = furi_get_tick();
            last_blink_tick = last_monitor_tick;
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
        elements_button_left(canvas, "Set");
        if(model->saved[model->selected_network]) {
            elements_button_right(canvas, "Forget");
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 31, model->status);
        canvas_draw_str(canvas, 4, 47, model->detail);
        canvas_draw_str(canvas, 4, 62, "Back: rescan");
    }
}

static bool wifi_input_callback(InputEvent* event, void* context) {
    WifiInternetWatch* app = context;
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        bool rescan = false;
        with_view_model(
            app->main_view,
            WifiViewModel * model,
            { rescan = model->screen == WifiScreenStatus; },
            false);

        if(rescan) {
            wifi_set_status(app, WifiScreenScanning, "Scanning Wi-Fi...", "");
            furi_thread_flags_set(furi_thread_get_id(app->worker), WifiWorkerFlagRescan);
        } else {
            view_dispatcher_stop(app->view_dispatcher);
        }
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
    bool open_settings = false;

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
                } else if(
                    event->key == InputKeyOk &&
                    (event->type == InputTypeShort || event->type == InputTypeLong)) {
                    strlcpy(
                        app->ssid, model->networks[model->selected_network], sizeof(app->ssid));
                    if(event->type == InputTypeShort && model->saved[model->selected_network]) {
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
                } else if(event->key == InputKeyLeft && event->type == InputTypeLong) {
                    open_settings = true;
                    consumed = true;
                }
            }
        },
        consumed);

    if(connect_saved) {
        const int32_t index = wifi_credential_index(app, app->ssid);
        if(index >= 0) {
            strlcpy(app->password, app->credentials[index].password, sizeof(app->password));
            wifi_set_status(app, WifiScreenStatus, "Connecting Wi-Fi...", app->ssid);
            furi_thread_flags_set(furi_thread_get_id(app->worker), WifiWorkerFlagConnect);
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
            furi_thread_flags_set(furi_thread_get_id(app->worker), WifiWorkerFlagForget);
        } else {
            wifi_set_status(app, WifiScreenStatus, "DELETE FAILED", "Check the SD card");
        }
    }

    if(open_settings) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewSettings);
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
                canvas_draw_str(
                    canvas,
                    x,
                    y,
                    model->mode == 0 ? "Aa" :
                    model->mode == 1 ? "#?" :
                                       "ab");
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
                model->column = model->column == 0 ? (uint8_t)(row_size - 1) :
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
            } else if(
                event->key == InputKeyBack &&
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

static void wifi_ip_draw_callback(Canvas* canvas, void* context) {
    WifiIpModel* model = context;
    static const WifiIpKey row_1[] = {{'1', 8}, {'2', 22}, {'3', 36}};
    static const WifiIpKey row_2[] = {{'4', 8}, {'5', 22}, {'6', 36}};
    static const WifiIpKey row_3[] = {{'7', 8}, {'8', 22}, {'9', 36}};
    static const WifiIpKey row_4[] = {{'.', 8}, {'0', 22}, {WIFI_PASSWORD_KEY_BACKSPACE, 36}};
    static const WifiIpKey save_key = {'S', 102};

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "Internet IP");
    elements_slightly_rounded_frame(canvas, 1, 11, 126, 10);
    canvas_draw_str(canvas, 4, 19, model->ip);
    canvas_set_font(canvas, FontKeyboard);

    const WifiIpKey* rows[] = {row_1, row_2, row_3, row_4};
    for(uint8_t row = 0; row < 4; row++) {
        const WifiIpKey* keys = rows[row];
        const uint8_t y = 31 + row * 8;
        for(uint8_t column = 0; column < 3; column++) {
            const bool selected = model->row == row && model->column == column;
            const char value = keys[column].value;
            if(selected) canvas_draw_box(canvas, keys[column].x - 1, y - 7, 8, 9);
            canvas_set_color(canvas, selected ? ColorWhite : ColorBlack);
            if(value == WIFI_PASSWORD_KEY_BACKSPACE) {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, keys[column].x, y, "<-");
                canvas_set_font(canvas, FontKeyboard);
            } else {
                canvas_draw_glyph(canvas, keys[column].x, y, value);
            }
            canvas_set_color(canvas, ColorBlack);
        }
    }

    if(model->row == 4 && model->column == 0)
        canvas_draw_box(canvas, save_key.x - 1, 62 - 7, 22, 9);
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, model->row == 4 && model->column == 0 ? ColorWhite : ColorBlack);
    canvas_draw_str(canvas, save_key.x, 62, "Save");
}

static const char wifi_ip_keymap[] =
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '0', WIFI_PASSWORD_KEY_BACKSPACE};

static bool wifi_ip_input_callback(InputEvent* event, void* context) {
    WifiInternetWatch* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong) {
        return false;
    }

    bool consumed = true;
    bool save = false;
    bool cancel = false;

    with_view_model(
        app->ip_view,
        WifiIpModel * model,
        {
            const size_t len = strlen(model->ip);
            if(event->key == InputKeyLeft) {
                model->column = model->column == 0 ? 2 : model->column - 1;
            } else if(event->key == InputKeyRight) {
                model->column = (model->column + 1) % 3;
            } else if(event->key == InputKeyUp) {
                model->row = model->row == 0 ? 3 : model->row - 1;
            } else if(event->key == InputKeyDown) {
                model->row = (model->row + 1) % 5;
                if(model->row == 4) model->column = 0;
            } else if(event->key == InputKeyOk) {
                if(model->row == 4) {
                    save = true;
                } else {
                    const char value = wifi_ip_keymap[model->row * 3 + model->column];
                    if(value == WIFI_PASSWORD_KEY_BACKSPACE) {
                        if(len > 0)
                            model->ip[len - 1] = '\0';
                        else
                            cancel = true;
                    } else if(value == '.') {
                        if(len + 1 < sizeof(model->ip)) {
                            model->ip[len] = '.';
                            model->ip[len + 1] = '\0';
                        }
                    } else if(len + 1 < sizeof(model->ip)) {
                        model->ip[len] = value;
                        model->ip[len + 1] = '\0';
                    }
                }
            } else if(event->key == InputKeyBack) {
                cancel = true;
            } else {
                consumed = false;
            }
        },
        consumed);

    if(save) {
        with_view_model(
            app->ip_view,
            WifiIpModel * model,
            {
                if(wifi_ip_is_valid(model->ip)) {
                    strlcpy(app->ping_ip, model->ip, sizeof(app->ping_ip));
                    wifi_settings_save(app);
                    wifi_sync_settings_views(app);
                    view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewSettings);
                } else {
                    wifi_set_status(app, WifiScreenStatus, "INVALID IP", "Use x.x.x.x");
                }
            },
            false);
    } else if(cancel) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WifiViewSettings);
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
    wifi_settings_load(app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, wifi_custom_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

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
            model->detail[0] = '\0';
        },
        false);
    view_dispatcher_add_view(app->view_dispatcher, WifiViewMain, app->main_view);

    app->settings_view = view_alloc();
    view_allocate_model(app->settings_view, ViewModelTypeLocking, sizeof(WifiSettingsMenuModel));
    view_set_context(app->settings_view, app);
    view_set_draw_callback(app->settings_view, wifi_settings_menu_draw_callback);
    view_set_input_callback(app->settings_view, wifi_settings_menu_input_callback);
    with_view_model(app->settings_view, WifiSettingsMenuModel * model, { model->row = 0; }, false);
    view_dispatcher_add_view(app->view_dispatcher, WifiViewSettings, app->settings_view);

    app->password_view = view_alloc();
    view_allocate_model(app->password_view, ViewModelTypeLocking, sizeof(WifiPasswordModel));
    view_set_context(app->password_view, app);
    view_set_draw_callback(app->password_view, wifi_password_draw_callback);
    view_set_input_callback(app->password_view, wifi_password_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, WifiViewPassword, app->password_view);

    app->ip_view = view_alloc();
    view_allocate_model(app->ip_view, ViewModelTypeLocking, sizeof(WifiIpModel));
    view_set_context(app->ip_view, app);
    view_set_draw_callback(app->ip_view, wifi_ip_draw_callback);
    view_set_input_callback(app->ip_view, wifi_ip_input_callback);
    with_view_model(
        app->ip_view,
        WifiIpModel * model,
        {
            model->row = 0;
            model->column = 0;
        },
        false);
    view_dispatcher_add_view(app->view_dispatcher, WifiViewIp, app->ip_view);

    app->timer_view = view_alloc();
    view_allocate_model(app->timer_view, ViewModelTypeLocking, sizeof(WifiTimerModel));
    view_set_context(app->timer_view, app);
    view_set_draw_callback(app->timer_view, wifi_timer_draw_callback);
    view_set_input_callback(app->timer_view, wifi_timer_input_callback);
    with_view_model(
        app->timer_view,
        WifiTimerModel * model,
        {
            model->row = 0;
            model->column = 0;
        },
        false);
    view_dispatcher_add_view(app->view_dispatcher, WifiViewTimer, app->timer_view);

    wifi_sync_settings_views(app);

    return app;
}

static bool wifi_uart_start(WifiInternetWatch* app) {
    app->rx_stream = furi_stream_buffer_alloc(AT_RESPONSE_SIZE, 1);
    app->serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    if(!app->serial) return false;

    furi_hal_serial_init(app->serial, 115200);
    furi_hal_serial_async_rx_start(app->serial, wifi_serial_rx_callback, app, false);
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

    wifi_stop_blink(app);

    view_dispatcher_remove_view(app->view_dispatcher, WifiViewPassword);
    view_free(app->password_view);
    view_dispatcher_remove_view(app->view_dispatcher, WifiViewSettings);
    view_free(app->settings_view);
    view_dispatcher_remove_view(app->view_dispatcher, WifiViewIp);
    view_free(app->ip_view);
    view_dispatcher_remove_view(app->view_dispatcher, WifiViewTimer);
    view_free(app->timer_view);
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

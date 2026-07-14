#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/dialog_ex.h>
#include <datetime/datetime.h>
#include <locale/locale.h>
#include <expansion/expansion.h>

#define TAG                "WIFI_MAP"
#define MAX_AP_LIST        20
#define WORKER_EVENTS_MASK (WorkerEventStop | WorkerEventRx)
#define BAUD_RATE          115200
#define RX_BUFFER_SIZE     1024

// Screen is 128x64 px
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    View* view;
    FuriThread* worker_thread;
    FuriStreamBuffer* rx_stream;
    File* file;
    FuriHalSerialHandle* serial_handle;
} WiFiMapApp;

typedef struct WifiMapModel WifiMapModel;

typedef struct {
    FuriString* line;
} StringList;

struct WifiMapModel {
    StringList* lines[MAX_AP_LIST];
    int lnrdy;
    int rdy;
    int cntr;
};

typedef enum {
    WorkerEventReserved = (1 << 0),
    WorkerEventStop = (1 << 1),
    WorkerEventRx = (1 << 2),
} WorkerEventFlags;

const NotificationSequence sequence_notification = {
    &message_display_backlight_on,
    &message_green_255,
    &message_delay_10,
    NULL,
};

static File* open_file() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    DateTime rtc = {0};
    furi_hal_rtc_get_datetime(&rtc);
    FuriString* flnm = furi_string_alloc();
    furi_string_printf(
        flnm,
        "/data/wifi_map_%d-%d-%d_%d_%d_%d.csv",
        rtc.day,
        rtc.month,
        rtc.year,
        rtc.hour,
        rtc.minute,
        rtc.second);
    const char* filename = furi_string_get_cstr(flnm);

    if(!storage_file_open(file, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open file");
    }
    furi_string_free(flnm);
    return file;
}

static int32_t write_to_file(char data_line, File* file) {
    char* data = (char*)malloc(sizeof(char) + 1);
    data[0] = data_line;
    if(!storage_file_write(file, data, (uint16_t)strlen(data))) {
        FURI_LOG_E(TAG, "Failed to write to file");
    }
    free(data);
    return 0;
}

static int32_t close_file(File* file) {
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return 0;
}

static void retrieve_ap_ssid_distance(const char* data, char* apssid, char* dst) {
    for(size_t i = 0; i < 2; i++) {
        apssid[i] = data[i];
    }

    int cntr = 0;
    for(size_t i = 9; i < strlen(data); i++) {
        if(data[i] == '.')
            break;
        else
            dst[cntr] = data[i];
        cntr++;
    }
}

static void uart_echo_view_draw_callback(Canvas* canvas, void* _model) {
    WifiMapModel* model = _model;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    int cntr = 0;
    for(size_t i = 0; i < MAX_AP_LIST; i++) {
        const char* line = furi_string_get_cstr(model->lines[i]->line);
        FURI_LOG_D(TAG, "raw_data:> %s", line);
        char apssid[2], dst[6];
        retrieve_ap_ssid_distance(line, apssid, dst);
        FURI_LOG_D(TAG, "appssid:> %s", apssid);
        FURI_LOG_D(TAG, "dist:> %s", dst);
        if(strlen(line) > 0) {
            int d = atoi(dst);
            canvas_draw_circle(canvas, 0, SCREEN_HEIGHT / 2, d);
            if(i % 2) {
                canvas_draw_str(canvas, d, (SCREEN_HEIGHT / 2) + cntr, apssid);
            } else {
                canvas_draw_str(canvas, d, (SCREEN_HEIGHT / 2) - cntr, apssid);
                cntr = cntr + 8;
            }
            if(i > 8) {
                cntr = 8;
            }
        }
    }

    if(model->rdy) {
        canvas_clear(canvas);
        for(size_t i = 0; i < MAX_AP_LIST; i++) {
            furi_string_reset(model->lines[i]->line);
        }
        model->rdy = 0;
        model->cntr = 0;
    }
}

static bool uart_echo_view_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

static uint32_t uart_echo_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void
    uart_echo_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent data, void* context) {
    furi_assert(context);
    UNUSED(handle);
    WiFiMapApp* app = context;
    volatile FuriHalSerialRxEvent event_copy = data;
    UNUSED(event_copy);

    WorkerEventFlags flag = 0;

    if(data & FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(app->rx_stream, &data, 1, 0);
        flag |= WorkerEventRx;
    }

    furi_thread_flags_set(furi_thread_get_id(app->worker_thread), flag);
}

static void uart_push_to_list(WifiMapModel* model, const char data, WiFiMapApp* app) {
    write_to_file((char)data, app->file);
    if(!model->rdy) {
        if(data != '\n') {
            furi_string_push_back(model->lines[model->cntr]->line, data);
        } else {
            model->cntr++;
        }

        if(model->cntr == MAX_AP_LIST) {
            model->rdy = 1;
        }
    }
}

static int32_t wifi_map_worker(void* context) {
    furi_assert(context);
    WiFiMapApp* app = context;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_EVENTS_MASK, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEventStop) {
            break;
        }
        if(events & WorkerEventRx) {
            size_t length = 0;
            do {
                uint8_t data[64];
                length = furi_stream_buffer_receive(app->rx_stream, data, 64, 0);
                if(length > 0) {
                    with_view_model(
                        app->view,
                        WifiMapModel * model,
                        {
                            for(size_t i = 0; i < length; i++) {
                                uart_push_to_list(model, data[i], app);
                            }
                        },
                        false);
                }
            } while(length > 0);

            notification_message(app->notification, &sequence_notification);
            with_view_model(app->view, WifiMapModel * model, { UNUSED(model); }, true);
        }
    }

    return 0;
}

static WiFiMapApp* wifi_map_app_alloc() {
    WiFiMapApp* app = malloc(sizeof(WiFiMapApp));
    app->file = open_file();
    app->rx_stream = furi_stream_buffer_alloc(2048, 1);

    // Gui
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Views
    app->view = view_alloc();
    view_set_draw_callback(app->view, uart_echo_view_draw_callback);
    view_set_input_callback(app->view, uart_echo_view_input_callback);
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(WifiMapModel));

    with_view_model(
        app->view,
        WifiMapModel * model,
        {
            for(size_t i = 0; i < MAX_AP_LIST; i++) {
                model->lines[i] = malloc(sizeof(StringList));
                model->lines[i]->line = furi_string_alloc();
            }
            model->lnrdy = 0;
            model->rdy = 0;
            model->cntr = 0;
        },
        true);

    view_set_previous_callback(app->view, uart_echo_exit);
    view_dispatcher_add_view(app->view_dispatcher, 0, app->view);
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);

    app->rx_stream = furi_stream_buffer_alloc(RX_BUFFER_SIZE, 1);
    app->worker_thread = furi_thread_alloc();
    furi_thread_set_name(app->worker_thread, "UsbUartWorker");
    furi_thread_set_stack_size(app->worker_thread, 1024);
    furi_thread_set_context(app->worker_thread, app);
    furi_thread_set_callback(app->worker_thread, wifi_map_worker);

    furi_check(app->worker_thread);

    furi_thread_start(app->worker_thread);

    // Enable uart listener
    app->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_check(app->serial_handle);
    uint32_t bdrt = BAUD_RATE;
    furi_hal_serial_init(app->serial_handle, bdrt);
    furi_hal_serial_async_rx_start(app->serial_handle, uart_echo_on_irq_cb, app, true);

    return app;
}

static void wifi_map_app_free(WiFiMapApp* app) {
    furi_assert(app);

    // Serial free
    furi_hal_serial_deinit(app->serial_handle);
    furi_hal_serial_control_release(app->serial_handle);

    furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WorkerEventStop);
    furi_thread_join(app->worker_thread);
    furi_thread_free(app->worker_thread);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, 0);

    with_view_model(
        app->view,
        WifiMapModel * model,
        {
            for(size_t i = 0; i < MAX_AP_LIST; i++) {
                free(model->lines[i]);
            }
            free(model);
        },
        true);
    view_free(app->view);
    view_dispatcher_free(app->view_dispatcher);

    // Close gui record
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    app->gui = NULL;

    furi_stream_buffer_free(app->rx_stream);

    close_file(app->file);

    // Free rest
    free(app);
}

int32_t wifi_map_app(void* p) {
    UNUSED(p);
    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    FURI_LOG_I(TAG, "wifi_map_app starting...");
    WiFiMapApp* app = wifi_map_app_alloc();

    char* headers = "AP hash;Distance (meters);AP auth mode;Time from start (seconds)\n";

    if(!storage_file_write(app->file, headers, (uint16_t)strlen(headers))) {
        FURI_LOG_E(TAG, "Failed to write to file");
    }
    view_dispatcher_run(app->view_dispatcher);
    wifi_map_app_free(app);
    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    return 0;
}

// Screen is 128x64 px
// x -->
// y
// |
// |
//

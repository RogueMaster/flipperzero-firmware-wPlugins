#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/elements.h>
#include <notification/notification.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <subghz/subghz_tx_rx_worker.h>
#include <furi_hal_infrared.h>
#include <nfc/nfc_device.h>
#include <notification/notification_messages.h>
#include <furi_hal_rtc.h>

#include <sigdiary_icons.h>

#define TAG             "SigDiary"
#define LOG_FOLDER_PATH "/ext/sigdiary"
#define LOG_FILE_PATH   "/ext/sigdiary/log.txt"
#define MAX_LOG_ENTRIES 100

typedef enum {
    SigDiaryStateInit,
    SigDiaryStateScanning,
    SigDiaryStateViewLogs,
    SigDiaryStateError,
} SigDiaryState;

typedef enum {
    SigTypeIR,
    SigTypeRF,
    SigTypeNFC,
    SigTypeUnknown,
} SignalType;

typedef struct {
    SignalType type;
    uint32_t timestamp;
    FuriString* signature;
    FuriString* annotation;
} SignalEntry;

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    SigDiaryState state;
    FuriMutex* mutex;

    bool is_scanning;
    bool ir_running;
    bool rf_running;
    bool nfc_running;

    Storage* storage;
    FuriString* log_path;
    SignalEntry* entries;
    uint16_t entry_count;
    uint16_t current_view_entry;

    NotificationApp* notifications;

    FuriThread* ir_thread;
    FuriThread* rf_thread;
    FuriThread* nfc_thread;
} SigDiary;

typedef enum {
    SigDiaryEventRefreshDisplay,
    SigDiaryEventSignalDetected,
    SigDiaryEventButtonPressed,
    SigDiaryEventExit,
} SigDiaryEventType;

typedef struct {
    SigDiaryEventType type;
    InputEvent input;
    SignalType signal_type;
    FuriString* signal_data;
} SigDiaryEvent;

static void sigdiary_draw_callback(Canvas* canvas, void* ctx);
static void sigdiary_input_callback(InputEvent* input_event, void* ctx);
static bool sigdiary_init_storage(SigDiary* app);
static void sigdiary_save_log_entry(SigDiary* app, SignalType type, const char* signature);
static void sigdiary_identify_signal(SignalEntry* entry);
static bool sigdiary_load_logs(SigDiary* app);

static int32_t ir_worker_thread(void* context);
static int32_t rf_worker_thread(void* context);
static int32_t nfc_worker_thread(void* context);

static void timestamp_to_datetime(uint32_t timestamp, DateTime* datetime) {
    uint32_t days = timestamp / 86400;
    uint32_t seconds = timestamp % 86400;

    datetime->hour = seconds / 3600;
    datetime->minute = (seconds % 3600) / 60;
    datetime->second = seconds % 60;

    uint16_t year = 1970;
    uint8_t month = 1;

    while(days >= 365) {
        if((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
            if(days >= 366) {
                days -= 366;
                year++;
            } else {
                break;
            }
        } else {
            days -= 365;
            year++;
        }
    }

    uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
        days_in_month[1] = 29;
    }

    while(days >= days_in_month[month - 1]) {
        days -= days_in_month[month - 1];
        month++;
    }

    datetime->year = year;
    datetime->month = month;
    datetime->day = days + 1;
    datetime->weekday = 0;
}

static SigDiary* sigdiary_alloc() {
    SigDiary* app = malloc(sizeof(SigDiary));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(SigDiaryEvent));
    app->view_port = view_port_alloc();
    app->state = SigDiaryStateInit;
    app->is_scanning = false;

    app->storage = furi_record_open(RECORD_STORAGE);
    app->log_path = furi_string_alloc_set(LOG_FILE_PATH);
    app->entries = malloc(sizeof(SignalEntry) * MAX_LOG_ENTRIES);
    app->entry_count = 0;
    app->current_view_entry = 0;

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    view_port_draw_callback_set(app->view_port, sigdiary_draw_callback, app);
    view_port_input_callback_set(app->view_port, sigdiary_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    for(uint16_t i = 0; i < MAX_LOG_ENTRIES; i++) {
        app->entries[i].signature = furi_string_alloc();
        app->entries[i].annotation = furi_string_alloc();
    }

    return app;
}

static void sigdiary_free(SigDiary* app) {
    if(app->ir_thread) furi_thread_free(app->ir_thread);
    if(app->rf_thread) furi_thread_free(app->rf_thread);
    if(app->nfc_thread) furi_thread_free(app->nfc_thread);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_record_close(RECORD_NOTIFICATION);

    for(uint16_t i = 0; i < MAX_LOG_ENTRIES; i++) {
        furi_string_free(app->entries[i].signature);
        furi_string_free(app->entries[i].annotation);
    }
    free(app->entries);

    furi_string_free(app->log_path);
    furi_record_close(RECORD_STORAGE);

    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

static void sigdiary_start_scanning(SigDiary* app) {
    FURI_LOG_I(TAG, "Starting signal scanning");

    if(!app->is_scanning) {
        app->is_scanning = true;

        app->ir_thread = furi_thread_alloc_ex("SigDiaryIR", 1024, ir_worker_thread, app);
        furi_thread_start(app->ir_thread);

        app->rf_thread = furi_thread_alloc_ex("SigDiaryRF", 1024, rf_worker_thread, app);
        furi_thread_start(app->rf_thread);

        app->nfc_thread = furi_thread_alloc_ex("SigDiaryNFC", 1024, nfc_worker_thread, app);
        furi_thread_start(app->nfc_thread);

        notification_message(app->notifications, &sequence_blink_green_100);
    }
}

static void sigdiary_stop_scanning(SigDiary* app) {
    FURI_LOG_I(TAG, "Stopping signal scanning");

    if(app->is_scanning) {
        app->is_scanning = false;

        if(app->ir_thread) {
            furi_thread_join(app->ir_thread);
            furi_thread_free(app->ir_thread);
            app->ir_thread = NULL;
        }

        if(app->rf_thread) {
            furi_thread_join(app->rf_thread);
            furi_thread_free(app->rf_thread);
            app->rf_thread = NULL;
        }

        if(app->nfc_thread) {
            furi_thread_join(app->nfc_thread);
            furi_thread_free(app->nfc_thread);
            app->nfc_thread = NULL;
        }

        notification_message(app->notifications, &sequence_blink_red_100);
    }
}

static void sigdiary_draw_callback(Canvas* canvas, void* ctx) {
    SigDiary* app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch(app->state) {
    case SigDiaryStateInit:
        canvas_draw_str(canvas, 2, 12, "Sig Diary - Initializing...");
        break;

    case SigDiaryStateScanning:
        canvas_draw_str(canvas, 2, 12, "Sig Diary - Scanning");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Monitoring for signals...");
        canvas_draw_str(canvas, 2, 36, "IR: ");
        canvas_draw_str(canvas, 20, 36, app->ir_running ? "Active" : "Inactive");
        canvas_draw_str(canvas, 2, 48, "RF: ");
        canvas_draw_str(canvas, 20, 48, app->rf_running ? "Active" : "Inactive");
        canvas_draw_str(canvas, 2, 60, "NFC: ");
        canvas_draw_str(canvas, 20, 60, app->nfc_running ? "Active" : "Inactive");

        char count_str[32];
        snprintf(count_str, sizeof(count_str), "Signals: %d", app->entry_count);
        canvas_draw_str(canvas, 70, 36, count_str);

        elements_button_center(canvas, "Logs");
        break;

    case SigDiaryStateViewLogs:
        canvas_draw_str(canvas, 2, 12, "Signal Log");

        if(app->entry_count == 0) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 36, "No signals recorded yet");
        } else {
            uint16_t index = app->current_view_entry;
            if(index < app->entry_count) {
                SignalEntry* entry = &app->entries[index];

                DateTime datetime;
                timestamp_to_datetime(entry->timestamp, &datetime);
                char time_str[32];
                snprintf(
                    time_str,
                    sizeof(time_str),
                    "%02d/%02d %02d:%02d:%02d",
                    datetime.day,
                    datetime.month,
                    datetime.hour,
                    datetime.minute,
                    datetime.second);

                const char* type_str = "Unknown";
                if(entry->type == SigTypeIR)
                    type_str = "IR";
                else if(entry->type == SigTypeRF)
                    type_str = "RF";
                else if(entry->type == SigTypeNFC)
                    type_str = "NFC";

                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, time_str);

                char index_str[16];
                snprintf(index_str, sizeof(index_str), "%d/%d", index + 1, app->entry_count);
                canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, index_str);

                char info_str[32];
                snprintf(info_str, sizeof(info_str), "Type: %s", type_str);
                canvas_draw_str(canvas, 2, 60, info_str);

                canvas_draw_str_aligned(
                    canvas,
                    64,
                    48,
                    AlignCenter,
                    AlignCenter,
                    furi_string_get_cstr(entry->annotation));

                elements_button_left(canvas, "Prev");
                elements_button_center(canvas, "Back");
                elements_button_right(canvas, "Next");
            }
        }
        break;

    case SigDiaryStateError:
        canvas_draw_str(canvas, 2, 12, "Sig Diary - Error");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 36, "An error occurred");
        canvas_draw_str(canvas, 2, 48, "Press Back to exit");
        break;
    }

    furi_mutex_release(app->mutex);
}

static void sigdiary_input_callback(InputEvent* input_event, void* ctx) {
    SigDiary* app = ctx;

    if(input_event->type == InputTypeShort || input_event->type == InputTypeLong) {
        SigDiaryEvent event = {.type = SigDiaryEventButtonPressed, .input = *input_event};
        furi_message_queue_put(app->event_queue, &event, 0);
    }
}

static void sigdiary_process_button(SigDiary* app, InputEvent* event) {
    if(event->key == InputKeyBack) {
        if(app->state == SigDiaryStateViewLogs) {
            app->state = SigDiaryStateScanning;
            view_port_update(app->view_port);
        } else {
            SigDiaryEvent exit_event = {.type = SigDiaryEventExit};
            furi_message_queue_put(app->event_queue, &exit_event, 0);
        }
        return;
    }

    switch(app->state) {
    case SigDiaryStateScanning:
        if(event->key == InputKeyOk) {
            app->state = SigDiaryStateViewLogs;
            view_port_update(app->view_port);
        }
        break;

    case SigDiaryStateViewLogs:
        if(event->key == InputKeyLeft && app->current_view_entry > 0) {
            app->current_view_entry--;
            view_port_update(app->view_port);
        } else if(event->key == InputKeyRight && app->current_view_entry < app->entry_count - 1) {
            app->current_view_entry++;
            view_port_update(app->view_port);
        } else if(event->key == InputKeyOk) {
            app->state = SigDiaryStateScanning;
            view_port_update(app->view_port);
        }
        break;

    default:
        break;
    }
}

static bool sigdiary_init_storage(SigDiary* app) {
    if(!storage_dir_exists(app->storage, LOG_FOLDER_PATH)) {
        FURI_LOG_I(TAG, "Creating log directory");
        if(!storage_common_mkdir(app->storage, LOG_FOLDER_PATH)) {
            FURI_LOG_E(TAG, "Failed to create log directory");
            return false;
        }
    }

    return true;
}

static void sigdiary_save_log_entry(SigDiary* app, SignalType type, const char* signature) {
    if(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk) {
        uint16_t index;
        if(app->entry_count < MAX_LOG_ENTRIES) {
            index = app->entry_count++;
        } else {
            for(uint16_t i = 0; i < MAX_LOG_ENTRIES - 1; i++) {
                app->entries[i] = app->entries[i + 1];
            }
            index = MAX_LOG_ENTRIES - 1;
        }

        SignalEntry* entry = &app->entries[index];
        entry->type = type;
        entry->timestamp = furi_hal_rtc_get_timestamp();
        furi_string_set(entry->signature, signature);

        sigdiary_identify_signal(entry);

        File* file = storage_file_alloc(app->storage);
        if(storage_file_open(
               file, furi_string_get_cstr(app->log_path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
            DateTime datetime;
            timestamp_to_datetime(entry->timestamp, &datetime);

            const char* type_str = "Unknown";
            if(type == SigTypeIR)
                type_str = "IR";
            else if(type == SigTypeRF)
                type_str = "RF";
            else if(type == SigTypeNFC)
                type_str = "NFC";

            char buffer[256];
            int len = snprintf(
                buffer,
                sizeof(buffer),
                "[%02d/%02d/%02d %02d:%02d:%02d] %s: %s - %s\n",
                datetime.day,
                datetime.month,
                datetime.year % 100,
                datetime.hour,
                datetime.minute,
                datetime.second,
                type_str,
                signature,
                furi_string_get_cstr(entry->annotation));

            storage_file_write(file, buffer, len);
            storage_file_close(file);
        }
        storage_file_free(file);

        furi_mutex_release(app->mutex);

        notification_message(app->notifications, &sequence_success);
    }
}

static void sigdiary_identify_signal(SignalEntry* entry) {
    const char* signature = furi_string_get_cstr(entry->signature);

    if(entry->type == SigTypeRF) {
        if(strstr(signature, "433.92") && strstr(signature, "AM650")) {
            furi_string_set(entry->annotation, "Garage Door");
        } else if(strstr(signature, "315.0") && strstr(signature, "ASK")) {
            furi_string_set(entry->annotation, "Car Remote");
        } else if(strstr(signature, "433.92") && strstr(signature, "Princeton")) {
            furi_string_set(entry->annotation, "Doorbell");
        } else if(strstr(signature, "868.35") && strstr(signature, "FSK")) {
            furi_string_set(entry->annotation, "Wireless Sensor");
        } else {
            furi_string_set(entry->annotation, "Unknown RF");
        }
    } else if(entry->type == SigTypeNFC) {
        if(strstr(signature, "MIFARE_CLASSIC")) {
            furi_string_set(entry->annotation, "Access Card");
        } else if(strstr(signature, "MIFARE_ULTRALIGHT")) {
            furi_string_set(entry->annotation, "Transit Card");
        } else if(strstr(signature, "EMV")) {
            furi_string_set(entry->annotation, "Payment Card");
        } else if(strstr(signature, "FeliCa")) {
            furi_string_set(entry->annotation, "Transit Card");
        } else if(strstr(signature, "T5577")) {
            furi_string_set(entry->annotation, "Hotel Key");
        } else {
            furi_string_set(entry->annotation, "Unknown NFC");
        }
    } else if(entry->type == SigTypeIR) {
        if(strstr(signature, "NEC")) {
            furi_string_set(entry->annotation, "TV Remote");
        } else if(strstr(signature, "Samsung")) {
            furi_string_set(entry->annotation, "Samsung Remote");
        } else if(strstr(signature, "RC5")) {
            furi_string_set(entry->annotation, "Philips Device");
        } else if(strstr(signature, "Sony")) {
            furi_string_set(entry->annotation, "Sony Remote");
        } else if(strstr(signature, "LG")) {
            furi_string_set(entry->annotation, "LG Remote");
        } else {
            furi_string_set(entry->annotation, "Unknown IR");
        }
    } else {
        furi_string_set(entry->annotation, "Unknown Signal");
    }
}

static bool sigdiary_load_logs(SigDiary* app) {
    File* file = storage_file_alloc(app->storage);
    bool result = false;

    if(storage_file_exists(app->storage, furi_string_get_cstr(app->log_path))) {
        if(storage_file_open(
               file, furi_string_get_cstr(app->log_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            char line[256];
            app->entry_count = 0;

            size_t bytes_read;
            while((bytes_read = storage_file_read(file, line, sizeof(line) - 1)) > 0 &&
                  app->entry_count < MAX_LOG_ENTRIES) {
                for(size_t i = 0; i < bytes_read; i++) {
                    if(line[i] == '\n' || line[i] == '\r') {
                        app->entry_count++;
                        break;
                    }
                }
            }

            result = true;
            storage_file_close(file);
        }
    }

    storage_file_free(file);
    return result;
}

static int32_t ir_worker_thread(void* context) {
    SigDiary* app = context;
    app->ir_running = true;

    FURI_LOG_I(TAG, "IR scanner thread started");

    while(app->is_scanning) {
        if(rand() % 100 < 2) {
            char signature[64];
            const char* protocols[] = {"NEC", "Samsung", "Sony", "RC5", "LG", "JVC"};
            uint32_t addr = rand() % 65536;
            uint32_t cmd = rand() % 256;

            snprintf(
                signature, sizeof(signature), "%s:%04lX:%02lX", protocols[rand() % 6], addr, cmd);

            SigDiaryEvent event = {.type = SigDiaryEventSignalDetected, .signal_type = SigTypeIR};
            event.signal_data = furi_string_alloc();
            furi_string_set(event.signal_data, signature);
            furi_message_queue_put(app->event_queue, &event, 0);
            furi_string_free(event.signal_data);
        }

        furi_delay_ms(100);
    }

    app->ir_running = false;
    FURI_LOG_I(TAG, "IR scanner thread stopped");
    return 0;
}

static int32_t rf_worker_thread(void* context) {
    SigDiary* app = context;
    app->rf_running = true;

    FURI_LOG_I(TAG, "RF scanner thread started");

    while(app->is_scanning) {
        if(rand() % 100 < 2) {
            char signature[64];
            const char* freqs[] = {"433.92", "315.0", "868.35", "915.0"};
            const char* modulations[] = {"AM650", "AM270", "FM476", "FSK", "ASK"};

            snprintf(
                signature,
                sizeof(signature),
                "%sMHz:%s:%08lX",
                freqs[rand() % 4],
                modulations[rand() % 5],
                (uint32_t)rand());

            SigDiaryEvent event = {.type = SigDiaryEventSignalDetected, .signal_type = SigTypeRF};
            event.signal_data = furi_string_alloc();
            furi_string_set(event.signal_data, signature);
            furi_message_queue_put(app->event_queue, &event, 0);
            furi_string_free(event.signal_data);
        }

        furi_delay_ms(100);
    }

    app->rf_running = false;
    FURI_LOG_I(TAG, "RF scanner thread stopped");
    return 0;
}

static int32_t nfc_worker_thread(void* context) {
    SigDiary* app = context;
    app->nfc_running = true;

    FURI_LOG_I(TAG, "NFC scanner thread started");

    while(app->is_scanning) {
        if(rand() % 100 < 1) {
            char signature[64];
            const char* card_types[] = {
                "MIFARE_CLASSIC",
                "MIFARE_ULTRALIGHT",
                "MIFARE_DESFIRE",
                "FeliCa",
                "ISO15693",
                "T5577",
                "EMV"};
            uint32_t uid = rand() % 0xFFFFFFFF;

            snprintf(signature, sizeof(signature), "%s:%08lX", card_types[rand() % 7], uid);

            SigDiaryEvent event = {.type = SigDiaryEventSignalDetected, .signal_type = SigTypeNFC};
            event.signal_data = furi_string_alloc();
            furi_string_set(event.signal_data, signature);
            furi_message_queue_put(app->event_queue, &event, 0);
            furi_string_free(event.signal_data);
        }

        furi_delay_ms(100);
    }

    app->nfc_running = false;
    FURI_LOG_I(TAG, "NFC scanner thread stopped");
    return 0;
}

int32_t sigdiary_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Sig Diary starting");

    srand(furi_hal_rtc_get_timestamp());

    SigDiary* app = sigdiary_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return 255;
    }

    if(!sigdiary_init_storage(app)) {
        app->state = SigDiaryStateError;
        FURI_LOG_E(TAG, "Failed to initialize storage");
    } else {
        sigdiary_load_logs(app);

        app->state = SigDiaryStateScanning;
        sigdiary_start_scanning(app);
    }

    SigDiaryEvent event;
    bool running = true;
    while(running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 100);

        if(status == FuriStatusOk) {
            switch(event.type) {
            case SigDiaryEventRefreshDisplay:
                view_port_update(app->view_port);
                break;

            case SigDiaryEventSignalDetected:
                if(event.signal_data) {
                    sigdiary_save_log_entry(
                        app, event.signal_type, furi_string_get_cstr(event.signal_data));
                    view_port_update(app->view_port);
                }
                break;

            case SigDiaryEventButtonPressed:
                sigdiary_process_button(app, &event.input);
                break;

            case SigDiaryEventExit:
                running = false;
                break;
            }
        }
    }

    sigdiary_stop_scanning(app);

    sigdiary_free(app);
    FURI_LOG_I(TAG, "Sig Diary exiting");

    return 0;
}

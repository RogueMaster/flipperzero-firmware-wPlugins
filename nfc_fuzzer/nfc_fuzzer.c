/* nfc_fuzzer.c — Main application, UI, and entry point for NFC Fuzzer.
 * Provides card type selection, field control, progress display, and log viewer. */

#include "nfc_fuzzer.h"
#include "nfc_fuzzer_worker.h"
#include "nfc_fuzzer_profiles.h"
#include <gui/elements.h>
#include <stdio.h>

#define APP_TAG NFC_FUZZER_LOG_TAG

/* ─────────────────────────────────────────────────
 * Forward declarations
 * ───────────────────────────────────────────────── */

static void nfc_fuzzer_app_show_profile_select(NfcFuzzerApp* app);
static void nfc_fuzzer_app_show_strategy_select(NfcFuzzerApp* app);
static void nfc_fuzzer_app_show_fuzz_run(NfcFuzzerApp* app);
static void nfc_fuzzer_app_show_results_list(NfcFuzzerApp* app);
static void nfc_fuzzer_app_show_result_detail(NfcFuzzerApp* app, uint32_t index);
static void nfc_fuzzer_app_show_settings(NfcFuzzerApp* app);

/* ─────────────────────────────────────────────────
 * Helper: free heap-allocated result label strings
 * (Issue 6: labels must live on the heap since
 *  submenu_add_item does not copy them)
 * ───────────────────────────────────────────────── */

static void nfc_fuzzer_free_result_labels(NfcFuzzerApp* app) {
    if(app->result_labels) {
        for(uint32_t i = 0; i < app->result_labels_count; i++) {
            free(app->result_labels[i]);
        }
        free(app->result_labels);
        app->result_labels = NULL;
        app->result_labels_count = 0;
    }
}

/* ─────────────────────────────────────────────────
 * Helper: grow dynamic results array if needed
 * (Issue 2: results are now dynamically allocated)
 * ───────────────────────────────────────────────── */

static bool nfc_fuzzer_results_ensure_capacity(NfcFuzzerApp* app) {
    if(app->result_count >= app->result_capacity) {
        if(app->result_count >= NFC_FUZZER_MAX_RESULTS) {
            return false; /* Hit hard limit */
        }
        uint32_t new_capacity = app->result_capacity * 2;
        if(new_capacity > NFC_FUZZER_MAX_RESULTS) {
            new_capacity = NFC_FUZZER_MAX_RESULTS;
        }
        if(new_capacity < NFC_FUZZER_INITIAL_RESULT_CAPACITY) {
            new_capacity = NFC_FUZZER_INITIAL_RESULT_CAPACITY;
        }
        NfcFuzzerResult* new_results =
            realloc(app->results, new_capacity * sizeof(NfcFuzzerResult));
        if(!new_results) {
            FURI_LOG_E(APP_TAG, "Failed to grow results array");
            return false;
        }
        app->results = new_results;
        app->result_capacity = new_capacity;
    }
    return true;
}

/* ─────────────────────────────────────────────────
 * Fuzz Run View - custom View with draw/input callbacks
 * ───────────────────────────────────────────────── */

typedef struct {
    uint32_t current_test;
    uint32_t total_tests;
    uint32_t anomaly_count;
    char payload_hex[64];
    bool running;
} FuzzRunViewModel;

static void fuzz_run_view_draw_callback(Canvas* canvas, void* model) {
    FuzzRunViewModel* m = model;
    furi_assert(m);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "NFC Fuzzer Running");

    canvas_set_font(canvas, FontSecondary);

    /* Progress */
    char buf[64];
    snprintf(
        buf,
        sizeof(buf),
        "Test: %lu / %lu",
        (unsigned long)m->current_test,
        (unsigned long)m->total_tests);
    canvas_draw_str(canvas, 2, 26, buf);

    /* Progress bar */
    uint8_t bar_width = 120;
    uint8_t bar_x = 4;
    uint8_t bar_y = 30;
    uint8_t bar_h = 6;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_width, bar_h);
    if(m->total_tests > 0 && m->total_tests != UINT32_MAX) {
        uint32_t fill =
            (uint32_t)(((uint64_t)m->current_test * bar_width) / m->total_tests);
        if(fill > bar_width) fill = bar_width;
        canvas_draw_box(canvas, bar_x, bar_y, (uint8_t)fill, bar_h);
    }

    /* Anomaly count */
    snprintf(buf, sizeof(buf), "Anomalies: %lu", (unsigned long)m->anomaly_count);
    canvas_draw_str(canvas, 2, 46, buf);

    /* Current payload */
    canvas_draw_str(canvas, 2, 56, "Payload:");
    canvas_draw_str(canvas, 2, 64, m->payload_hex);
}

static bool fuzz_run_view_input_callback(InputEvent* event, void* context) {
    NfcFuzzerApp* app = context;
    furi_assert(app);

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        /* Stop worker and go back */
        if(nfc_fuzzer_worker_is_running(app->worker)) {
            nfc_fuzzer_worker_stop(app->worker);
        }
        app->worker_running = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcFuzzerViewProfileSelect);
        return true;
    }
    return false;
}

/* ─────────────────────────────────────────────────
 * Worker callbacks
 * ───────────────────────────────────────────────── */

static void nfc_fuzzer_worker_progress_cb(
    const NfcFuzzerResult* result,
    uint32_t current,
    uint32_t total,
    const uint8_t* payload,
    uint8_t payload_len,
    void* context) {
    NfcFuzzerApp* app = context;
    furi_assert(app);

    /* Issue 8: Acquire mutex before writing shared state */
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    app->current_test = current;
    app->total_tests = total;

    if(payload_len > 0) {
        app->current_payload_len = payload_len;
        memcpy(app->current_payload, payload, payload_len);
    }

    /* Record anomaly */
    if(result != NULL && result->anomaly != NfcFuzzerAnomalyNone) {
        app->anomaly_count++;
        /* Issue 2: Use dynamic results array with grow-as-needed */
        if(nfc_fuzzer_results_ensure_capacity(app)) {
            memcpy(&app->results[app->result_count], result, sizeof(NfcFuzzerResult));
            app->result_count++;
        }

        /* Log to SD card */
        if(app->log_path && app->storage) {
            File* file = storage_file_alloc(app->storage);
            if(storage_file_open(
                   file, furi_string_get_cstr(app->log_path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
                /* Heap-allocate hex buffers to avoid 1532 bytes on the
                 * worker thread stack (same fix as result detail view). */
                char* payload_hex = malloc(NFC_FUZZER_HEX_STR_LEN);
                char* response_hex = malloc(NFC_FUZZER_HEX_STR_LEN);
                if(payload_hex && response_hex) {
                    nfc_fuzzer_bytes_to_hex(result->payload, result->payload_len, payload_hex);
                    nfc_fuzzer_bytes_to_hex(result->response, result->response_len, response_hex);
                    /* Line can be up to ~1555 chars (two 765-char hex strings +
                     * test number + anomaly name + delimiters). Heap-allocate
                     * to avoid truncating log data (was a 256-byte stack buf). */
                    size_t line_size = NFC_FUZZER_HEX_STR_LEN * 2 + 64;
                    char* line = malloc(line_size);
                    if(line) {
                        size_t len = (size_t)snprintf(
                            line,
                            line_size,
                            "%lu,%s,%s,%s\n",
                            (unsigned long)result->test_num,
                            nfc_fuzzer_anomaly_name(result->anomaly),
                            payload_hex,
                            response_hex);
                        if(len >= line_size) len = line_size - 1;
                        if(len > 0) {
                            storage_file_write(file, line, (uint16_t)(len > 0xFFFF ? 0xFFFF : len));
                        }
                        free(line);
                    }
                }
                free(payload_hex);
                free(response_hex);
                storage_file_close(file);
            }
            storage_file_free(file);
        }

        /* Blink LED blue on anomaly */
        notification_message(app->notifications, &sequence_blink_blue_100);
    } else {
        /* Blink LED during normal operation */
        notification_message(app->notifications, &sequence_success);
    }

    furi_mutex_release(app->mutex);

    /* Update view model (with_view_model has its own locking) */
    with_view_model(
        app->view_fuzz_run,
        FuzzRunViewModel * model,
        {
            model->current_test = current;
            model->total_tests = total;
            model->anomaly_count = app->anomaly_count;
            model->running = true;

            /* Payload hex (show first 20 bytes max for display) */
            uint8_t show_len = payload_len > 20 ? 20 : payload_len;
            char hex[64] = {0};
            nfc_fuzzer_bytes_to_hex(payload, show_len, hex);
            strncpy(model->payload_hex, hex, sizeof(model->payload_hex) - 1);
        },
        true);
}

static void nfc_fuzzer_worker_done_cb(void* context) {
    NfcFuzzerApp* app = context;
    furi_assert(app);

    /* Issue 8: Acquire mutex before writing shared state */
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->worker_running = false;
    furi_mutex_release(app->mutex);

    /* Turn off LED */
    notification_message(app->notifications, &sequence_success);

    with_view_model(
        app->view_fuzz_run, FuzzRunViewModel * model, { model->running = false; }, true);

    FURI_LOG_I(APP_TAG, "Fuzzing complete. Anomalies: %lu", (unsigned long)app->anomaly_count);
}

/* ─────────────────────────────────────────────────
 * Profile select scene
 * ───────────────────────────────────────────────── */

static uint32_t profile_select_previous_callback(void* context) {
    (void)context;
    return VIEW_NONE; /* Exit app */
}

static void profile_select_callback(void* context, uint32_t index) {
    NfcFuzzerApp* app = context;
    furi_assert(app);

    if(index == 100) {
        nfc_fuzzer_app_show_results_list(app);
    } else if(index == 101) {
        nfc_fuzzer_app_show_settings(app);
    } else if(index < NfcFuzzerProfileCOUNT) {
        app->selected_profile = (NfcFuzzerProfile)index;
        FURI_LOG_I(
            APP_TAG, "Selected profile: %s", nfc_fuzzer_profile_name(app->selected_profile));
        nfc_fuzzer_app_show_strategy_select(app);
    }
}

static void nfc_fuzzer_app_show_profile_select(NfcFuzzerApp* app) {
    submenu_reset(app->submenu_profile);
    submenu_set_header(app->submenu_profile, "NFC Fuzzer");

    for(uint32_t i = 0; i < NfcFuzzerProfileCOUNT; i++) {
        submenu_add_item(
            app->submenu_profile,
            nfc_fuzzer_profile_name((NfcFuzzerProfile)i),
            i,
            profile_select_callback,
            app);
    }

    submenu_add_item(app->submenu_profile, "[View Results]", 100, profile_select_callback, app);
    submenu_add_item(app->submenu_profile, "[Settings]", 101, profile_select_callback, app);

    view_set_previous_callback(
        submenu_get_view(app->submenu_profile), profile_select_previous_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcFuzzerViewProfileSelect);
}

/* ─────────────────────────────────────────────────
 * Strategy select scene
 * ───────────────────────────────────────────────── */

static void strategy_select_callback(void* context, uint32_t index) {
    NfcFuzzerApp* app = context;
    furi_assert(app);

    if(index < NfcFuzzerStrategyCOUNT) {
        app->selected_strategy = (NfcFuzzerStrategy)index;
        FURI_LOG_I(
            APP_TAG, "Selected strategy: %s", nfc_fuzzer_strategy_name(app->selected_strategy));
        nfc_fuzzer_app_show_fuzz_run(app);
    }
}

static uint32_t strategy_select_previous_callback(void* context) {
    (void)context;
    return NfcFuzzerViewProfileSelect;
}

static void nfc_fuzzer_app_show_strategy_select(NfcFuzzerApp* app) {
    submenu_reset(app->submenu_strategy);
    submenu_set_header(app->submenu_strategy, "Fuzz Strategy");

    for(uint32_t i = 0; i < NfcFuzzerStrategyCOUNT; i++) {
        submenu_add_item(
            app->submenu_strategy,
            nfc_fuzzer_strategy_name((NfcFuzzerStrategy)i),
            i,
            strategy_select_callback,
            app);
    }

    view_set_previous_callback(
        submenu_get_view(app->submenu_strategy), strategy_select_previous_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcFuzzerViewStrategySelect);
}

/* ─────────────────────────────────────────────────
 * Fuzz run scene
 * ───────────────────────────────────────────────── */

static void nfc_fuzzer_app_show_fuzz_run(NfcFuzzerApp* app) {
    /* Reset counters */
    app->current_test = 0;
    app->total_tests = 0;
    app->anomaly_count = 0;
    app->result_count = 0;
    app->current_payload_len = 0;

    /* Reset view model */
    with_view_model(
        app->view_fuzz_run,
        FuzzRunViewModel * model,
        {
            model->current_test = 0;
            model->total_tests = 0;
            model->anomaly_count = 0;
            model->payload_hex[0] = '\0';
            model->running = true;
        },
        true);

    /* Create log file path */
    if(app->log_path) {
        furi_string_free(app->log_path);
    }
    app->log_path = furi_string_alloc();

    /* Ensure log directory exists */
    storage_simply_mkdir(app->storage, NFC_FUZZER_LOG_DIR);

    /* Generate timestamped filename */
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    furi_string_printf(
        app->log_path,
        "%s/nfc_fuzz_%04d%02d%02d_%02d%02d%02d.log",
        NFC_FUZZER_LOG_DIR,
        datetime.year,
        datetime.month,
        datetime.day,
        datetime.hour,
        datetime.minute,
        datetime.second);

    FURI_LOG_I(APP_TAG, "Log file: %s", furi_string_get_cstr(app->log_path));

    /* Write header to log file */
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(
           file, furi_string_get_cstr(app->log_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char header[128];
        /* Issue 7: Clamp snprintf return value to buffer size */
        size_t len = (size_t)snprintf(
            header,
            sizeof(header),
            "# NFC Fuzzer Log - Profile: %s, Strategy: %s\n# test_num,anomaly,payload,response\n",
            nfc_fuzzer_profile_name(app->selected_profile),
            nfc_fuzzer_strategy_name(app->selected_strategy));
        if(len >= sizeof(header)) len = sizeof(header) - 1;
        if(len > 0) {
            storage_file_write(file, header, (uint16_t)len);
        }
        storage_file_close(file);
    } else {
        FURI_LOG_W(APP_TAG, "Failed to create log file");
    }
    storage_file_free(file);

    /* Start worker */
    nfc_fuzzer_worker_set_callback(app->worker, nfc_fuzzer_worker_progress_cb, app);
    nfc_fuzzer_worker_set_done_callback(app->worker, nfc_fuzzer_worker_done_cb, app);
    app->worker_running = true;
    nfc_fuzzer_worker_start(
        app->worker, app->selected_profile, app->selected_strategy, &app->settings);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcFuzzerViewFuzzRun);
}

/* ─────────────────────────────────────────────────
 * Results list scene
 * ───────────────────────────────────────────────── */

static void results_list_callback(void* context, uint32_t index) {
    NfcFuzzerApp* app = context;
    furi_assert(app);

    if(index < app->result_count) {
        nfc_fuzzer_app_show_result_detail(app, index);
    }
}

static uint32_t results_list_previous_callback(void* context) {
    (void)context;
    return NfcFuzzerViewProfileSelect;
}

static void nfc_fuzzer_app_show_results_list(NfcFuzzerApp* app) {
    submenu_reset(app->submenu_results);
    submenu_set_header(app->submenu_results, "Anomalies Found");

    /* Issue 6: Free any previously allocated label strings before resetting */
    nfc_fuzzer_free_result_labels(app);

    if(app->result_count == 0) {
        submenu_add_item(app->submenu_results, "(No anomalies)", 0, NULL, NULL);
    } else {
        /* Issue 6: Allocate label strings on the heap so submenu can reference them.
         * submenu_add_item() does NOT copy the string, so stack-allocated labels
         * would become dangling pointers. */
        app->result_labels = malloc(app->result_count * sizeof(char*));
        if(!app->result_labels) {
            app->result_labels_count = 0;
            submenu_add_item(app->submenu_results, "(Memory error)", 0, NULL, NULL);
        } else {
            app->result_labels_count = app->result_count;

            for(uint32_t i = 0; i < app->result_count; i++) {
                /* Allocate each label on the heap */
                app->result_labels[i] = malloc(48);
                if(!app->result_labels[i]) {
                    app->result_labels_count = i;
                    break;
                }
                snprintf(
                    app->result_labels[i],
                    48,
                    "#%lu %s",
                    (unsigned long)app->results[i].test_num,
                    nfc_fuzzer_anomaly_name(app->results[i].anomaly));
                submenu_add_item(
                    app->submenu_results, app->result_labels[i], i, results_list_callback, app);
            }
        }
    }

    view_set_previous_callback(
        submenu_get_view(app->submenu_results), results_list_previous_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcFuzzerViewResultsList);
}

/* ─────────────────────────────────────────────────
 * Result detail scene
 * ───────────────────────────────────────────────── */

static uint32_t result_detail_previous_callback(void* context) {
    (void)context;
    return NfcFuzzerViewResultsList;
}

static void nfc_fuzzer_app_show_result_detail(NfcFuzzerApp* app, uint32_t index) {
    if(index >= app->result_count) return;

    NfcFuzzerResult* r = &app->results[index];

    /* Heap-allocate hex buffers to avoid 1532 bytes on the main thread stack. */
    char* payload_hex = malloc(NFC_FUZZER_HEX_STR_LEN);
    char* response_hex = malloc(NFC_FUZZER_HEX_STR_LEN);
    if(!payload_hex || !response_hex) {
        free(payload_hex);
        free(response_hex);
        return;
    }
    nfc_fuzzer_bytes_to_hex(r->payload, r->payload_len, payload_hex);
    nfc_fuzzer_bytes_to_hex(r->response, r->response_len, response_hex);

    snprintf(
        app->detail_text,
        sizeof(app->detail_text),
        "Test #%lu\n"
        "Anomaly: %s\n"
        "Payload (%u bytes):\n%s\n"
        "Response (%u bytes):\n%s",
        (unsigned long)r->test_num,
        nfc_fuzzer_anomaly_name(r->anomaly),
        r->payload_len,
        payload_hex,
        r->response_len,
        response_hex);

    free(payload_hex);
    free(response_hex);

    text_box_reset(app->text_box_detail);
    text_box_set_text(app->text_box_detail, app->detail_text);
    text_box_set_font(app->text_box_detail, TextBoxFontText);

    view_set_previous_callback(
        text_box_get_view(app->text_box_detail), result_detail_previous_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcFuzzerViewResultDetail);
}

/* ─────────────────────────────────────────────────
 * Settings scene
 * ───────────────────────────────────────────────── */

static const char* const timeout_names[] = {"50 ms", "100 ms", "250 ms", "500 ms"};
static const char* const delay_names[] = {"0 ms", "10 ms", "50 ms", "100 ms"};
static const char* const auto_stop_names[] = {"Off", "On"};
static const char* const max_cases_names[] = {"100", "1000", "10000", "Unlimited"};

static void settings_timeout_changed(VariableItem* item) {
    NfcFuzzerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= NfcFuzzerTimeoutCOUNT) index = 0;
    app->settings.timeout_index = (NfcFuzzerTimeoutIndex)index;
    variable_item_set_current_value_text(item, timeout_names[index]);
}

static void settings_delay_changed(VariableItem* item) {
    NfcFuzzerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= NfcFuzzerDelayCOUNT) index = 0;
    app->settings.delay_index = (NfcFuzzerDelayIndex)index;
    variable_item_set_current_value_text(item, delay_names[index]);
}

static void settings_auto_stop_changed(VariableItem* item) {
    NfcFuzzerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= 2) index = 0;
    app->settings.auto_stop = (index == 1);
    variable_item_set_current_value_text(item, auto_stop_names[index]);
}

static void settings_max_cases_changed(VariableItem* item) {
    NfcFuzzerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= NfcFuzzerMaxCasesCOUNT) index = 0;
    app->settings.max_cases_index = (NfcFuzzerMaxCasesIndex)index;
    variable_item_set_current_value_text(item, max_cases_names[index]);
}

static uint32_t settings_previous_callback(void* context) {
    (void)context;
    return NfcFuzzerViewProfileSelect;
}

static void nfc_fuzzer_app_show_settings(NfcFuzzerApp* app) {
    VariableItemList* list = app->variable_item_list_settings;
    variable_item_list_reset(list);

    VariableItem* item;

    /* Timeout */
    item = variable_item_list_add(
        list, "Timeout", NfcFuzzerTimeoutCOUNT, settings_timeout_changed, app);
    variable_item_set_current_value_index(item, app->settings.timeout_index);
    variable_item_set_current_value_text(item, timeout_names[app->settings.timeout_index]);

    /* Inter-test delay */
    item = variable_item_list_add(
        list, "Inter-test Delay", NfcFuzzerDelayCOUNT, settings_delay_changed, app);
    variable_item_set_current_value_index(item, app->settings.delay_index);
    variable_item_set_current_value_text(item, delay_names[app->settings.delay_index]);

    /* Auto-stop on anomaly */
    item = variable_item_list_add(list, "Auto-stop", 2, settings_auto_stop_changed, app);
    variable_item_set_current_value_index(item, app->settings.auto_stop ? 1 : 0);
    variable_item_set_current_value_text(item, auto_stop_names[app->settings.auto_stop ? 1 : 0]);

    /* Max test cases */
    item = variable_item_list_add(
        list, "Max Cases", NfcFuzzerMaxCasesCOUNT, settings_max_cases_changed, app);
    variable_item_set_current_value_index(item, app->settings.max_cases_index);
    variable_item_set_current_value_text(item, max_cases_names[app->settings.max_cases_index]);

    view_set_previous_callback(variable_item_list_get_view(list), settings_previous_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcFuzzerViewSettings);
}

/* ─────────────────────────────────────────────────
 * ViewDispatcher navigation callback
 * ───────────────────────────────────────────────── */

static bool nfc_fuzzer_app_custom_event_cb(void* context, uint32_t event) {
    (void)context;
    (void)event;
    return false;
}

static bool nfc_fuzzer_app_back_event_cb(void* context) {
    NfcFuzzerApp* app = context;
    furi_assert(app);

    /* If worker is running, stop it first */
    if(app->worker_running) {
        nfc_fuzzer_worker_stop(app->worker);
        app->worker_running = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcFuzzerViewProfileSelect);
        return true;
    }

    return false; /* Let ViewDispatcher handle back navigation */
}

/* ─────────────────────────────────────────────────
 * App alloc / free
 * ───────────────────────────────────────────────── */

static NfcFuzzerApp* nfc_fuzzer_app_alloc(void) {
    NfcFuzzerApp* app = malloc(sizeof(NfcFuzzerApp));
    furi_assert(app);
    memset(app, 0, sizeof(NfcFuzzerApp));

    /* Open system records */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    /* Default settings */
    app->settings.timeout_index = NfcFuzzerTimeout100ms;
    app->settings.delay_index = NfcFuzzerDelay10ms;
    app->settings.auto_stop = false;
    app->settings.max_cases_index = NfcFuzzerMaxCases1000;

    /* Issue 8: Allocate mutex for shared state synchronization */
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    /* Issue 2: Allocate initial dynamic results array */
    app->result_capacity = NFC_FUZZER_INITIAL_RESULT_CAPACITY;
    app->results = malloc(app->result_capacity * sizeof(NfcFuzzerResult));
    furi_assert(app->results);
    app->result_count = 0;

    /* View Dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nfc_fuzzer_app_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_fuzzer_app_back_event_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Profile select (Submenu) */
    app->submenu_profile = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcFuzzerViewProfileSelect, submenu_get_view(app->submenu_profile));

    /* Strategy select (Submenu) */
    app->submenu_strategy = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        NfcFuzzerViewStrategySelect,
        submenu_get_view(app->submenu_strategy));

    /* Fuzz run (custom View) */
    app->view_fuzz_run = view_alloc();
    view_allocate_model(app->view_fuzz_run, ViewModelTypeLocking, sizeof(FuzzRunViewModel));
    view_set_draw_callback(app->view_fuzz_run, fuzz_run_view_draw_callback);
    view_set_input_callback(app->view_fuzz_run, fuzz_run_view_input_callback);
    view_set_context(app->view_fuzz_run, app);
    view_dispatcher_add_view(app->view_dispatcher, NfcFuzzerViewFuzzRun, app->view_fuzz_run);

    /* Results list (Submenu) */
    app->submenu_results = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcFuzzerViewResultsList, submenu_get_view(app->submenu_results));

    /* Result detail (TextBox) */
    app->text_box_detail = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcFuzzerViewResultDetail, text_box_get_view(app->text_box_detail));

    /* Settings (VariableItemList) */
    app->variable_item_list_settings = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        NfcFuzzerViewSettings,
        variable_item_list_get_view(app->variable_item_list_settings));

    /* Worker */
    app->worker = nfc_fuzzer_worker_alloc();
    furi_assert(app->worker);

    /* Log path */
    app->log_path = NULL;

    /* Result labels */
    app->result_labels = NULL;
    app->result_labels_count = 0;

    FURI_LOG_I(APP_TAG, "App allocated");
    return app;
}

static void nfc_fuzzer_app_free(NfcFuzzerApp* app) {
    furi_assert(app);

    /* Stop worker if still running */
    if(app->worker_running) {
        nfc_fuzzer_worker_stop(app->worker);
        app->worker_running = false;
    }
    nfc_fuzzer_worker_free(app->worker);

    /* Remove views before freeing modules */
    view_dispatcher_remove_view(app->view_dispatcher, NfcFuzzerViewProfileSelect);
    view_dispatcher_remove_view(app->view_dispatcher, NfcFuzzerViewStrategySelect);
    view_dispatcher_remove_view(app->view_dispatcher, NfcFuzzerViewFuzzRun);
    view_dispatcher_remove_view(app->view_dispatcher, NfcFuzzerViewResultsList);
    view_dispatcher_remove_view(app->view_dispatcher, NfcFuzzerViewResultDetail);
    view_dispatcher_remove_view(app->view_dispatcher, NfcFuzzerViewSettings);

    /* Free modules */
    submenu_free(app->submenu_profile);
    submenu_free(app->submenu_strategy);
    view_free(app->view_fuzz_run);
    submenu_free(app->submenu_results);
    text_box_free(app->text_box_detail);
    variable_item_list_free(app->variable_item_list_settings);

    /* Free view dispatcher */
    view_dispatcher_free(app->view_dispatcher);

    /* Free log path */
    if(app->log_path) {
        furi_string_free(app->log_path);
    }

    /* Issue 6: Free heap-allocated result labels */
    nfc_fuzzer_free_result_labels(app);

    /* Issue 2: Free dynamically allocated results array */
    if(app->results) {
        free(app->results);
        app->results = NULL;
    }

    /* Issue 8: Free mutex */
    furi_mutex_free(app->mutex);

    /* Close records */
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);

    free(app);
    FURI_LOG_I(APP_TAG, "App freed");
}

/* ─────────────────────────────────────────────────
 * Entry point
 * ───────────────────────────────────────────────── */

int32_t nfc_fuzzer_app(void* p) {
    (void)p;

    FURI_LOG_I(APP_TAG, "NFC Fuzzer starting");

    NfcFuzzerApp* app = nfc_fuzzer_app_alloc();

    /* Show initial profile selection screen */
    nfc_fuzzer_app_show_profile_select(app);

    /* Run the event loop */
    view_dispatcher_run(app->view_dispatcher);

    nfc_fuzzer_app_free(app);

    FURI_LOG_I(APP_TAG, "NFC Fuzzer exiting");
    return 0;
}

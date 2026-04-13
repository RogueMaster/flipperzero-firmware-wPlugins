/**
 * @file FlipPass.c
 * @brief Main application file for FlipPass.
 *
 * This file contains the core logic for the FlipPass application, including
 * app allocation, deallocation, and the main entry point. It orchestrates
 * the application's lifecycle and manages the view dispatcher and scene
 * manager.
 */
#include "flippass.h"
#include "flippass_db.h"
#include "flippass_rpc.h"
#include "scenes/flippass_db_browser_view.h"
#include "scenes/flippass_progress_view.h"
#include <flipper_format/flipper_format.h>
#include <input/input.h>
#include <stdio.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/stream.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define TAG                           "FlipPass"
#define FLIPPASS_SYSTEM_LOG_MAX_BYTES (24U * 1024U)

#if FLIPPASS_ENABLE_VERBOSE_UNLOCK_LOG
#define FLIPPASS_STARTUP_LOG(app, ...) flippass_log_event(app, __VA_ARGS__)
#else
#define FLIPPASS_STARTUP_LOG(app, ...) \
    do {                               \
        UNUSED(app);                   \
    } while(0)
#endif

#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE
static uint32_t flippass_system_log_capture_suspend_depth = 0U;
#endif

enum {
    FlipPassCustomEventExit = 0x80000000U,
};

enum {
    FlipPassUsbPrepareFlagConnected = 1U << 0,
};

typedef struct {
    volatile uint8_t flags;
    volatile uint8_t activity_serial;
} FlipPassUsbPrepareState;

static bool flippass_usb_send_key(uint16_t hid_key);
static bool flippass_usb_send_special_key_prepared(uint16_t hid_key);
static bool flippass_keyboard_layout_is_valid(const char* path);
static void flippass_usb_prepare_usb_state_callback(FuriHalUsbStateEvent state, void* context);
static void flippass_usb_prepare_state_callback(bool state, void* context);
static uint8_t flippass_usb_prepare_progress_percent(
    uint8_t attempt_index,
    uint32_t elapsed_ms,
    uint32_t attempt_timeout_ms);
static void flippass_usb_prepare_progress(
    App* app,
    uint8_t attempt_index,
    uint32_t elapsed_ms,
    uint32_t attempt_timeout_ms);
static bool flippass_usb_prepare_attach_hid(App* app, uint8_t attempt_index);
static bool flippass_usb_prepare_wait_connected(
    App* app,
    FlipPassUsbPrepareState* prepare_state,
    uint8_t attempt_index,
    uint32_t attempt_timeout_ms,
    uint32_t* waited_ms);
#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE
static void flippass_system_log_capture_init(App* app);
static void flippass_system_log_capture_deinit(App* app);
#else
static void flippass_system_log_capture_init(App* app);
static void flippass_system_log_capture_deinit(App* app);
#endif

static void flippass_input_events_callback(const void* message, void* context) {
    const InputEvent* event = message;
    App* app = context;

    if(event == NULL || app == NULL) {
        return;
    }

    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassCustomEventExit);
    }
}

static bool flippass_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    App* app = context;

    if(event == FlipPassCustomEventExit) {
        flippass_request_exit(app);
        return true;
    }

    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flippass_back_event_callback(void* context) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void flippass_request_exit(App* app) {
    furi_assert(app);
    scene_manager_stop(app->scene_manager);
    view_dispatcher_stop(app->view_dispatcher);
}

void flippass_log_reset(App* app) {
    UNUSED(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));

    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, FLIPPASS_LOG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_cstring(stream, "");
        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

void flippass_log_event(App* app, const char* format, ...) {
    furi_assert(app);
    furi_assert(format);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    if(file_stream_open(stream, FLIPPASS_LOG_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        stream_write_cstring(stream, "AUTO: ");

        va_list args;
        va_start(args, format);
        stream_write_vaformat(stream, format, args);
        va_end(args);

        stream_write_cstring(stream, "\n");
        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

#if FLIPPASS_ENABLE_SYSTEM_TRACE_CAPTURE
static const char* flippass_heap_track_mode_name(FuriHalRtcHeapTrackMode mode) {
    switch(mode) {
    case FuriHalRtcHeapTrackModeNone:
        return "none";
    case FuriHalRtcHeapTrackModeMain:
        return "main";
    case FuriHalRtcHeapTrackModeTree:
        return "tree";
    case FuriHalRtcHeapTrackModeAll:
        return "all";
    default:
        return "unknown";
    }
}

static bool flippass_system_log_capture_flag_present(void) {
    bool present = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    present = storage_file_exists(storage, FLIPPASS_SYSTEM_LOG_ENABLE_FILE_PATH);
    furi_record_close(RECORD_STORAGE);
    return present;
}

static bool flippass_system_log_capture_requested(void) {
    return (furi_hal_rtc_get_heap_track_mode() != FuriHalRtcHeapTrackModeNone) ||
           ((furi_log_get_level() >= FuriLogLevelTrace) &&
            flippass_system_log_capture_flag_present());
}

static void flippass_system_log_reset_file(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));

    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, FLIPPASS_SYSTEM_LOG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        stream_write_cstring(stream, "");
        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

static bool flippass_system_log_line_matches(const char* line) {
    if(line == NULL || line[0] == '\0') {
        return false;
    }

    return strstr(line, "[FlipPassGzip]") != NULL || strstr(line, "[FlipPassParser]") != NULL ||
           strstr(line, "[FlipPassVault]") != NULL ||
           strstr(line, "allocation balance:") != NULL ||
           strstr(line, "Stack watermark is") != NULL || strstr(line, "dangerously low") != NULL;
}

static void flippass_system_log_store_ring_line(App* app, const char* line, size_t line_len) {
    furi_assert(app);
    furi_assert(line);

    if(app->system_log_ring == NULL || line_len == 0U) {
        return;
    }

    const size_t slot_size = FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE;
    char* slot = app->system_log_ring + (app->system_log_ring_next * slot_size);
    const size_t copy_len = (line_len < (slot_size - 1U)) ? line_len : (slot_size - 1U);
    memcpy(slot, line, copy_len);
    slot[copy_len] = '\0';
    if(copy_len < line_len) {
        app->system_log_ring_dropped++;
    }

    app->system_log_ring_next = (app->system_log_ring_next + 1U) % FLIPPASS_SYSTEM_LOG_RING_LINES;
    if(app->system_log_ring_count < FLIPPASS_SYSTEM_LOG_RING_LINES) {
        app->system_log_ring_count++;
    }
}

static void flippass_system_log_append_line(App* app, const char* line) {
    furi_assert(app);
    furi_assert(line);

    if(app->system_log_capture_buffered && app->system_log_ring != NULL) {
        flippass_system_log_store_ring_line(app, line, strlen(line));
        return;
    }

    if(app->system_log_capture_bytes >= FLIPPASS_SYSTEM_LOG_MAX_BYTES) {
        return;
    }

    const size_t line_len = strlen(line);
    if(line_len == 0U) {
        return;
    }

    const size_t available = FLIPPASS_SYSTEM_LOG_MAX_BYTES - app->system_log_capture_bytes;
    if(available == 0U) {
        return;
    }

    const size_t copy_len = (line_len < available) ? line_len : available;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    if(file_stream_open(stream, FLIPPASS_SYSTEM_LOG_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        stream_write(stream, (const uint8_t*)line, copy_len);
        if(app->system_log_capture_bytes + copy_len < FLIPPASS_SYSTEM_LOG_MAX_BYTES) {
            stream_write_cstring(stream, "\n");
            app->system_log_capture_bytes += copy_len + 1U;
        } else {
            app->system_log_capture_bytes += copy_len;
        }
        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

static void flippass_system_log_flush_ring_to_file(App* app) {
    furi_assert(app);

    if(!app->system_log_capture_buffered || app->system_log_ring == NULL) {
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));

    Stream* stream = file_stream_alloc(storage);
    if(file_stream_open(stream, FLIPPASS_SYSTEM_LOG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(size_t index = 0U; index < app->system_log_ring_count; index++) {
            const size_t ring_index =
                (app->system_log_ring_count == FLIPPASS_SYSTEM_LOG_RING_LINES) ?
                    ((app->system_log_ring_next + index) % FLIPPASS_SYSTEM_LOG_RING_LINES) :
                    index;
            const char* line =
                app->system_log_ring + (ring_index * FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE);
            if(line[0] == '\0') {
                continue;
            }
            stream_write(stream, (const uint8_t*)line, strlen(line));
            stream_write_cstring(stream, "\n");
        }

        if(app->system_log_ring_dropped > 0U) {
            char tail[96];
            snprintf(
                tail,
                sizeof(tail),
                "ring dropped=%lu",
                (unsigned long)app->system_log_ring_dropped);
            stream_write(stream, (const uint8_t*)tail, strlen(tail));
            stream_write_cstring(stream, "\n");
        }

        file_stream_close(stream);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

static void flippass_system_log_flush_line(App* app) {
    furi_assert(app);

    if(app->system_log_line_len == 0U) {
        return;
    }

    app->system_log_line[app->system_log_line_len] = '\0';
    if(flippass_system_log_line_matches(app->system_log_line)) {
        flippass_system_log_append_line(app, app->system_log_line);
    }

    app->system_log_line_len = 0U;
    app->system_log_line[0] = '\0';
}

static void flippass_system_log_handler(const uint8_t* data, size_t size, void* context) {
    App* app = context;
    if(app == NULL || !app->system_log_capture_enabled || app->system_log_capture_busy ||
       ((!app->system_log_capture_buffered) && flippass_system_log_capture_is_suspended()) ||
       data == NULL || size == 0U) {
        return;
    }

    app->system_log_capture_busy = true;

    for(size_t index = 0U; index < size; index++) {
        const char ch = (char)data[index];

        if(ch == '\r') {
            continue;
        }

        if(ch == '\n') {
            flippass_system_log_flush_line(app);
            continue;
        }

        if(app->system_log_line_len + 1U >= sizeof(app->system_log_line)) {
            flippass_system_log_flush_line(app);
        }

        if(app->system_log_line_len + 1U < sizeof(app->system_log_line)) {
            app->system_log_line[app->system_log_line_len++] = ch;
        }
    }

    app->system_log_capture_busy = false;
}

static void flippass_system_log_capture_init(App* app) {
    furi_assert(app);

    app->system_log_capture_enabled = false;
    app->system_log_capture_busy = false;
    app->system_log_capture_bytes = 0U;
    app->system_log_line_len = 0U;
    app->system_log_line[0] = '\0';
    app->system_log_ring = NULL;
    app->system_log_ring_count = 0U;
    app->system_log_ring_next = 0U;
    app->system_log_ring_dropped = 0U;
    app->system_log_capture_buffered = false;
    app->system_log_handler.callback = flippass_system_log_handler;
    app->system_log_handler.context = app;

    if(!flippass_system_log_capture_requested()) {
        return;
    }

    flippass_system_log_reset_file();
    app->system_log_ring =
        malloc(FLIPPASS_SYSTEM_LOG_RING_LINES * FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE);
    if(app->system_log_ring != NULL) {
        memset(
            app->system_log_ring,
            0,
            FLIPPASS_SYSTEM_LOG_RING_LINES * FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE);
        app->system_log_capture_buffered = true;
    }
    app->system_log_capture_enabled = furi_log_add_handler(app->system_log_handler);

    if(app->system_log_capture_enabled) {
        char header[160];
        snprintf(
            header,
            sizeof(header),
            "capture enabled level=%d heap=%s",
            (int)furi_log_get_level(),
            flippass_heap_track_mode_name(furi_hal_rtc_get_heap_track_mode()));
        flippass_system_log_append_line(app, header);
        flippass_log_event(
            app,
            "SYSTEM_TRACE_CAPTURE level=%d heap=%s",
            (int)furi_log_get_level(),
            flippass_heap_track_mode_name(furi_hal_rtc_get_heap_track_mode()));
    }
}

static void flippass_system_log_capture_deinit(App* app) {
    furi_assert(app);

    if(!app->system_log_capture_enabled) {
        return;
    }

    furi_log_remove_handler(app->system_log_handler);
    app->system_log_capture_enabled = false;
    flippass_system_log_flush_line(app);
    flippass_system_log_flush_ring_to_file(app);
    app->system_log_capture_busy = false;
    if(app->system_log_ring != NULL) {
        memzero(
            app->system_log_ring,
            FLIPPASS_SYSTEM_LOG_RING_LINES * FLIPPASS_SYSTEM_LOG_RING_LINE_SIZE);
        free(app->system_log_ring);
        app->system_log_ring = NULL;
    }
    app->system_log_ring_count = 0U;
    app->system_log_ring_next = 0U;
    app->system_log_ring_dropped = 0U;
    app->system_log_capture_buffered = false;
}

void flippass_system_log_capture_suspend(void) {
    flippass_system_log_capture_suspend_depth++;
}

void flippass_system_log_capture_resume(void) {
    if(flippass_system_log_capture_suspend_depth > 0U) {
        flippass_system_log_capture_suspend_depth--;
    }
}

bool flippass_system_log_capture_is_suspended(void) {
    return flippass_system_log_capture_suspend_depth > 0U;
}
#else
static void flippass_system_log_capture_init(App* app) {
    UNUSED(app);
}

static void flippass_system_log_capture_deinit(App* app) {
    UNUSED(app);
}

void flippass_system_log_capture_suspend(void) {
}

void flippass_system_log_capture_resume(void) {
}

bool flippass_system_log_capture_is_suspended(void) {
    return false;
}
#endif

static bool flippass_keyboard_layout_is_valid(const char* path) {
    bool valid = false;

    if(path == NULL || path[0] == '\0' || strcmp(path, FLIPPASS_KEYBOARD_LAYOUT_ALT) == 0) {
        return true;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo file_info;
    if(storage_common_stat(storage, path, &file_info) == FSE_OK &&
       !(file_info.flags & FSF_DIRECTORY) && file_info.size == 256U) {
        valid = true;
    }
    furi_record_close(RECORD_STORAGE);

    return valid;
}

void flippass_clear_text_buffer(App* app) {
    furi_assert(app);
    memzero(app->text_buffer, sizeof(app->text_buffer));
}

void flippass_clear_master_password(App* app) {
    furi_assert(app);
    memzero(app->master_password, sizeof(app->master_password));
}

static void
    flippass_usb_prepare_mark_activity(FlipPassUsbPrepareState* prepare_state, bool connected) {
    if(prepare_state != NULL) {
        uint8_t flags = prepare_state->flags;
        if(connected) {
            flags |= FlipPassUsbPrepareFlagConnected;
        }
        prepare_state->flags = flags;
        prepare_state->activity_serial++;
    }
}

static void flippass_usb_prepare_usb_state_callback(FuriHalUsbStateEvent state, void* context) {
    switch(state) {
    case FuriHalUsbStateEventReset:
    case FuriHalUsbStateEventDescriptorRequest:
    case FuriHalUsbStateEventWakeup:
    case FuriHalUsbStateEventSuspend:
        flippass_usb_prepare_mark_activity(context, false);
        break;
    }
}

static void flippass_usb_prepare_state_callback(bool state, void* context) {
    FlipPassUsbPrepareState* prepare_state = context;
    if(prepare_state == NULL) {
        return;
    }

    if(state) {
        flippass_usb_prepare_mark_activity(prepare_state, true);
    } else {
        prepare_state->flags &= (uint8_t)~FlipPassUsbPrepareFlagConnected;
    }
}

static uint8_t flippass_usb_prepare_progress_percent(
    uint8_t attempt_index,
    uint32_t elapsed_ms,
    uint32_t attempt_timeout_ms) {
    const uint32_t total_timeout_ms = attempt_timeout_ms * FLIPPASS_USB_PREPARE_RETRY_COUNT;
    const uint32_t completed_before_attempt =
        (attempt_index > 0U) ? ((uint32_t)(attempt_index - 1U) * attempt_timeout_ms) : 0U;
    uint32_t aggregate_elapsed = completed_before_attempt + elapsed_ms;

    if(total_timeout_ms == 0U) {
        return 5U;
    }

    if(aggregate_elapsed > total_timeout_ms) {
        aggregate_elapsed = total_timeout_ms;
    }

    return (uint8_t)(5U + ((aggregate_elapsed * 35U) / total_timeout_ms));
}

static void flippass_usb_prepare_progress(
    App* app,
    uint8_t attempt_index,
    uint32_t elapsed_ms,
    uint32_t attempt_timeout_ms) {
    char detail[48];

    furi_assert(app);

    snprintf(
        detail,
        sizeof(detail),
        "Waiting for USB HID host (%u/%u).",
        (unsigned)attempt_index,
        (unsigned)FLIPPASS_USB_PREPARE_RETRY_COUNT);
    flippass_progress_update(
        app,
        "Connecting",
        detail,
        flippass_usb_prepare_progress_percent(attempt_index, elapsed_ms, attempt_timeout_ms));
}

static bool flippass_usb_prepare_attach_hid(App* app, uint8_t attempt_index) {
    char detail[40];

    furi_assert(app);

    snprintf(
        detail,
        sizeof(detail),
        "Re-enumerating USB HID (%u/%u).",
        (unsigned)attempt_index,
        (unsigned)FLIPPASS_USB_PREPARE_RETRY_COUNT);
    flippass_progress_update(
        app,
        "Cleaning",
        detail,
        (uint8_t)(3U +
                  (((uint32_t)(attempt_index - 1U) * 2U) / FLIPPASS_USB_PREPARE_RETRY_COUNT)));

    furi_hal_hid_kb_release_all();
    furi_delay_ms(FLIPPASS_USB_RELEASE_DELAY_MS);
    if(!furi_hal_usb_set_config(NULL, NULL)) {
        FLIPPASS_BENCH_LOG(
            app, "USB_PREPARE_FAIL stage=detach attempt=%u", (unsigned)attempt_index);
        return false;
    }

    furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
    furi_hal_hid_kb_release_all();
    if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
        FLIPPASS_BENCH_LOG(
            app, "USB_PREPARE_FAIL stage=attach attempt=%u", (unsigned)attempt_index);
        return false;
    }

    furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
    if(attempt_index > 1U) {
        /* Force a fresh bus-level reconnect on retries to mirror the successful
           manual retry behavior seen on flaky hosts. */
        furi_hal_usb_reinit();
        furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
    }

    return true;
}

static bool flippass_usb_prepare_wait_connected(
    App* app,
    FlipPassUsbPrepareState* prepare_state,
    uint8_t attempt_index,
    uint32_t attempt_timeout_ms,
    uint32_t* waited_ms) {
    uint32_t elapsed_ms = 0U;
    uint32_t deadline_ms = FLIPPASS_USB_ENUMERATION_TIMEOUT_MS;
    uint8_t observed_activity_serial = 0U;

    furi_assert(app);
    furi_assert(prepare_state);
    furi_assert(waited_ms);

    observed_activity_serial = prepare_state->activity_serial;

    while(((prepare_state->flags & FlipPassUsbPrepareFlagConnected) == 0U) &&
          !furi_hal_hid_is_connected() && elapsed_ms < attempt_timeout_ms) {
        if(elapsed_ms >= deadline_ms) {
            break;
        }

        furi_delay_ms(FLIPPASS_USB_POLL_DELAY_MS);
        elapsed_ms += FLIPPASS_USB_POLL_DELAY_MS;

        if(observed_activity_serial != prepare_state->activity_serial) {
            observed_activity_serial = prepare_state->activity_serial;
            deadline_ms = elapsed_ms + FLIPPASS_USB_ENUMERATION_GRACE_MS;
            if(deadline_ms > attempt_timeout_ms) {
                deadline_ms = attempt_timeout_ms;
            }
        }

        flippass_usb_prepare_progress(app, attempt_index, elapsed_ms, attempt_timeout_ms);
    }

    *waited_ms = elapsed_ms;
    return ((prepare_state->flags & FlipPassUsbPrepareFlagConnected) != 0U) ||
           furi_hal_hid_is_connected();
}

bool flippass_usb_begin(App* app) {
    furi_assert(app);

    const bool was_locked = furi_hal_usb_is_locked();
    const uint32_t attempt_timeout_ms =
        FLIPPASS_USB_ENUMERATION_TIMEOUT_MS + FLIPPASS_USB_ENUMERATION_GRACE_MS;
    uint32_t total_waited_ms = 0U;
    FLIPPASS_BENCH_LOG(
        app,
        "USB_PREPARE_BEGIN prev=%u rpc=%u locked=%u",
        app->usb_if_prev != NULL ? 1U : 0U,
        app->rpc_mode ? 1U : 0U,
        was_locked ? 1U : 0U);
    if(was_locked) {
        furi_hal_usb_unlock();
    }

    if(app->usb_if_prev == NULL) {
        app->usb_was_locked = was_locked;
        app->usb_if_prev = furi_hal_usb_get_config();
        app->usb_expect_rpc_session_close = app->rpc_mode;
    }

    for(uint8_t attempt = 1U; attempt <= FLIPPASS_USB_PREPARE_RETRY_COUNT; attempt++) {
        FlipPassUsbPrepareState prepare_state = {0};
        uint32_t waited_ms = 0U;

        furi_hal_usb_set_state_callback(flippass_usb_prepare_usb_state_callback, &prepare_state);
        furi_hal_hid_set_state_callback(flippass_usb_prepare_state_callback, &prepare_state);
        if(!flippass_usb_prepare_attach_hid(app, attempt)) {
            furi_hal_hid_set_state_callback(NULL, NULL);
            furi_hal_usb_set_state_callback(NULL, NULL);
            if(attempt < FLIPPASS_USB_PREPARE_RETRY_COUNT) {
                FLIPPASS_BENCH_LOG(
                    app, "USB_PREPARE_RETRY stage=attach attempt=%u", (unsigned)attempt);
                continue;
            }
            break;
        }

        if(flippass_usb_prepare_wait_connected(
               app, &prepare_state, attempt, attempt_timeout_ms, &waited_ms)) {
            furi_hal_hid_set_state_callback(NULL, NULL);
            furi_hal_usb_set_state_callback(NULL, NULL);
            total_waited_ms += waited_ms;
            FLIPPASS_BENCH_LOG(
                app,
                "USB_PREPARE_OK attempt=%u waited_ms=%lu",
                (unsigned)attempt,
                (unsigned long)total_waited_ms);
            flippass_progress_update(app, "Typing", "USB HID connected.", 40U);
            furi_delay_ms(FLIPPASS_USB_SETTLE_DELAY_MS);
            return true;
        }
        furi_hal_hid_set_state_callback(NULL, NULL);
        furi_hal_usb_set_state_callback(NULL, NULL);

        total_waited_ms += waited_ms;
        FLIPPASS_BENCH_LOG(
            app,
            "USB_PREPARE_FAIL stage=%s attempt=%u waited_ms=%lu",
            (prepare_state.activity_serial != 0U) ? "enum_idle_timeout" : "host_timeout",
            (unsigned)attempt,
            (unsigned long)waited_ms);
        furi_hal_hid_kb_release_all();
        furi_hal_usb_set_config(NULL, NULL);
        furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
        if(attempt < FLIPPASS_USB_PREPARE_RETRY_COUNT) {
            FLIPPASS_BENCH_LOG(
                app, "USB_PREPARE_RETRY stage=connect_timeout next=%u", (unsigned)(attempt + 1U));
        }
    }

    FLIPPASS_BENCH_LOG(
        app,
        "USB_PREPARE_FAIL stage=exhausted retries=%u total_waited_ms=%lu",
        (unsigned)FLIPPASS_USB_PREPARE_RETRY_COUNT,
        (unsigned long)total_waited_ms);
    return false;
}

static bool flippass_usb_send_key(uint16_t hid_key) {
    if(hid_key == HID_KEYBOARD_NONE) {
        return false;
    }

    if(!flippass_usb_press_key_prepared(hid_key)) {
        flippass_usb_release_all_prepared();
        return false;
    }
    furi_delay_ms(FLIPPASS_USB_PRESS_DELAY_MS);
    if(!flippass_usb_release_key_prepared(hid_key)) {
        flippass_usb_release_all_prepared();
        return false;
    }
    furi_delay_ms(FLIPPASS_USB_RELEASE_DELAY_MS);
    return true;
}

bool flippass_usb_press_key_prepared(uint16_t hid_key) {
    return (hid_key != HID_KEYBOARD_NONE) && furi_hal_hid_kb_press(hid_key);
}

bool flippass_usb_release_key_prepared(uint16_t hid_key) {
    return (hid_key != HID_KEYBOARD_NONE) && furi_hal_hid_kb_release(hid_key);
}

bool flippass_usb_release_all_prepared(void) {
    return furi_hal_hid_kb_release_all();
}

static bool flippass_usb_send_special_key_prepared(uint16_t hid_key) {
    if(!flippass_usb_send_key(hid_key)) {
        return false;
    }

    furi_delay_ms(FLIPPASS_USB_STEP_DELAY_MS);
    return true;
}

bool flippass_usb_type_string_prepared(const char* text) {
    furi_assert(text);

    for(size_t i = 0; text[i] != '\0'; i++) {
        if(!flippass_usb_send_key(HID_ASCII_TO_KEY(text[i]))) {
            return false;
        }
        furi_delay_ms(FLIPPASS_USB_STEP_DELAY_MS);
    }

    return true;
}

void flippass_usb_restore(App* app) {
    furi_assert(app);

    furi_hal_hid_kb_release_all();
    furi_delay_ms(FLIPPASS_USB_RELEASE_DELAY_MS);
    if(app->usb_if_prev != NULL) {
        if(!furi_hal_usb_set_config(NULL, NULL)) {
            FLIPPASS_BENCH_LOG(app, "USB_RESTORE_FAIL stage=detach");
        }
        furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
        if(!furi_hal_usb_set_config(app->usb_if_prev, NULL)) {
            FLIPPASS_BENCH_LOG(app, "USB_RESTORE_FAIL stage=restore");
        } else {
            FLIPPASS_BENCH_LOG(app, "USB_RESTORE_OK");
        }
        furi_delay_ms(FLIPPASS_USB_SWITCH_DELAY_MS);
        app->usb_if_prev = NULL;
    }
    app->usb_expect_rpc_session_close = false;
    if(app->usb_was_locked) {
        furi_hal_usb_lock();
        app->usb_was_locked = false;
    }
}

bool flippass_usb_type_string(App* app, const char* text) {
    furi_assert(app);
    furi_assert(text);

    if(!flippass_usb_begin(app)) {
        flippass_usb_restore(app);
        return false;
    }

    const bool ok = flippass_usb_type_string_prepared(text);

    flippass_usb_restore(app);
    return ok;
}

bool flippass_usb_type_login(App* app, const char* username, const char* password) {
    furi_assert(app);
    furi_assert(username);
    furi_assert(password);

    if(!flippass_usb_begin(app)) {
        flippass_usb_restore(app);
        return false;
    }

    if(!flippass_usb_type_string_prepared(username)) {
        flippass_usb_restore(app);
        return false;
    }

    if(!flippass_usb_send_special_key_prepared(HID_KEYBOARD_TAB)) {
        flippass_usb_restore(app);
        return false;
    }

    if(!flippass_usb_type_string_prepared(password)) {
        flippass_usb_restore(app);
        return false;
    }

    if(!flippass_usb_send_special_key_prepared(HID_KEYBOARD_RETURN)) {
        flippass_usb_restore(app);
        return false;
    }

    flippass_usb_restore(app);
    return true;
}

bool flippass_usb_type_key(App* app, uint16_t hid_key) {
    furi_assert(app);

    if(!flippass_usb_begin(app)) {
        flippass_usb_restore(app);
        return false;
    }

    const bool ok = flippass_usb_send_key(hid_key);
    flippass_usb_restore(app);
    return ok;
}

bool flippass_usb_type_autotype(App* app, const KDBXEntry* entry) {
    furi_assert(app);
    furi_assert(entry);

    return flippass_output_type_autotype(app, FlipPassOutputTransportUsb, entry);
}

void flippass_set_status(App* app, const char* title, const char* message) {
    furi_assert(app);

    snprintf(app->status_title, sizeof(app->status_title), "%s", title ? title : "Status");
    snprintf(app->status_message, sizeof(app->status_message), "%s", message ? message : "");
}

void flippass_progress_reset(App* app) {
    furi_assert(app);

    app->progress_started_tick = 0U;
    app->progress_percent = 0U;
    app->progress_title[0] = '\0';
    if(app->progress_view != NULL) {
        flippass_progress_view_reset(app->progress_view);
    }
}

void flippass_progress_begin(App* app, const char* title, const char* stage, uint8_t percent) {
    furi_assert(app);

    snprintf(
        app->progress_title, sizeof(app->progress_title), "%s", title != NULL ? title : "Working");
    app->progress_started_tick = furi_get_tick();
    app->progress_percent = 0U;
    flippass_progress_update(app, stage, "", percent);
}

void flippass_progress_update(App* app, const char* stage, const char* detail, uint8_t percent) {
    char detail_buffer[STATUS_MESSAGE_SIZE];
    char eta_text[24];
    const uint8_t clamped = (percent <= 100U) ? percent : 100U;
    const char* detail_text = detail != NULL ? detail : "";

    furi_assert(app);

    detail_buffer[0] = '\0';
    eta_text[0] = '\0';
    if(clamped >= 5U && clamped < 100U && app->progress_started_tick != 0U) {
        const uint32_t tick_delta = furi_get_tick() - app->progress_started_tick;
        const uint32_t tick_hz = furi_kernel_get_tick_frequency();
        if(tick_hz > 0U) {
            const uint64_t elapsed_ms = ((uint64_t)tick_delta * 1000U) / tick_hz;
            const uint64_t remaining_ms = (elapsed_ms * (100U - clamped)) / clamped;
            const uint32_t remaining_sec = (uint32_t)((remaining_ms + 999U) / 1000U);
            if(remaining_sec >= 60U) {
                snprintf(
                    eta_text,
                    sizeof(eta_text),
                    "~%lum %lus left",
                    (unsigned long)(remaining_sec / 60U),
                    (unsigned long)(remaining_sec % 60U));
            } else {
                snprintf(eta_text, sizeof(eta_text), "~%lus left", (unsigned long)remaining_sec);
            }
        }
    }

    if(detail_text[0] != '\0' && eta_text[0] != '\0') {
        snprintf(detail_buffer, sizeof(detail_buffer), "%s  %s", detail_text, eta_text);
    } else if(detail_text[0] != '\0') {
        snprintf(detail_buffer, sizeof(detail_buffer), "%s", detail_text);
    } else if(eta_text[0] != '\0') {
        snprintf(detail_buffer, sizeof(detail_buffer), "%s", eta_text);
    }

    app->progress_percent = clamped;
    if(app->progress_view != NULL) {
        flippass_progress_view_set_state(
            app->progress_view,
            app->progress_title[0] != '\0' ? app->progress_title : "Working",
            stage != NULL ? stage : "",
            detail_buffer,
            clamped);
    }
}

void flippass_reset_database(App* app) {
    furi_assert(app);

    if(app->root_group != NULL || app->vault != NULL || app->db_arena != NULL ||
       app->database_loaded) {
        FLIPPASS_DIAGNOSTIC_LOG(app, "VAULT_CLEANUP");
    }

    flippass_db_deactivate_entry(app);
    kdbx_parser_reset(app->kdbx_parser);
    kdbx_group_free(app->root_group);
    if(app->vault != NULL) {
        kdbx_vault_free(app->vault);
    }
    if(app->db_arena != NULL) {
        kdbx_arena_free(app->db_arena);
    }
    if(app->pending_gzip_scratch_vault != NULL) {
        kdbx_vault_free(app->pending_gzip_scratch_vault);
    }
    app->db_arena = NULL;
    app->vault = NULL;
    app->active_vault_backend = KDBXVaultBackendNone;
    app->requested_vault_backend = KDBXVaultBackendRam;
    app->pending_gzip_scratch_vault = NULL;
    memset(&app->pending_gzip_scratch_ref, 0, sizeof(app->pending_gzip_scratch_ref));
    app->pending_gzip_plain_size = 0U;
    app->root_group = NULL;
    app->current_group = NULL;
    app->current_entry = NULL;
    app->active_group = NULL;
    app->active_entry = NULL;
    app->browser_selected_index = 0;
    app->action_selected_index = 0;
    app->other_field_selected_index = 0;
    app->other_field_action_selected_index = 0;
    app->pending_entry_action = FlipPassEntryActionNone;
    app->pending_other_field_mask = 0U;
    app->pending_other_custom_field = NULL;
    app->pending_other_field_name[0] = '\0';
    app->close_db_dialog_open = false;
    app->parse_failed = false;
    app->database_loaded = false;
    app->pending_vault_fallback = false;
    app->allow_ext_vault_promotion = false;
    kdbx_protected_stream_reset(&app->protected_stream);
    if(app->text_view_body != NULL) {
        furi_string_reset(app->text_view_body);
    }
    app->text_view_title[0] = '\0';
    app->text_view_return_scene = FlipPassScene_DbEntries;
}

void flippass_close_database(App* app) {
    furi_assert(app);

    FLIPPASS_DIAGNOSTIC_LOG(app, "DATABASE_CLOSE");
    flippass_usb_restore(app);
    flippass_output_release_all(app);
    dialog_ex_reset(app->dialog_ex);
    widget_reset(app->widget);
    submenu_reset(app->submenu);
    text_input_reset(app->text_input);
    flippass_db_browser_view_reset(app->db_browser);
    flippass_progress_reset(app);
    flippass_reset_database(app);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    app->password_header[0] = '\0';
    flippass_set_status(app, "FlipPass", "");
    app->status_return_scene = FlipPassScene_FileBrowser;
}

void flippass_save_settings(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/flippass"));
    FlipperFormat* file = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(file, FLIPPASS_CONFIG_FILE_PATH)) {
        flipper_format_write_string_cstr(
            file, "last_file_path", furi_string_get_cstr(app->file_path));
        flipper_format_write_string_cstr(
            file,
            "keyboard_layout",
            (app->keyboard_layout_path != NULL && !furi_string_empty(app->keyboard_layout_path)) ?
                furi_string_get_cstr(app->keyboard_layout_path) :
                FLIPPASS_KEYBOARD_LAYOUT_ALT);
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void flippass_load_settings(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* keyboard_layout = furi_string_alloc();

    if(flipper_format_file_open_existing(file, FLIPPASS_CONFIG_FILE_PATH)) {
        flipper_format_read_string(file, "last_file_path", app->file_path);
        if(flipper_format_read_string(file, "keyboard_layout", keyboard_layout) &&
           !furi_string_empty(keyboard_layout)) {
            const char* layout_path = furi_string_get_cstr(keyboard_layout);
            if(strcmp(layout_path, FLIPPASS_KEYBOARD_LAYOUT_ALT) != 0 &&
               flippass_keyboard_layout_is_valid(layout_path)) {
                furi_string_set(app->keyboard_layout_path, keyboard_layout);
            } else {
                furi_string_reset(app->keyboard_layout_path);
            }
        } else {
            furi_string_reset(app->keyboard_layout_path);
        }
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(keyboard_layout);
}

/**
 * @brief Custom event callback for the scene manager.
 *
 * This function is called when a custom event is triggered in the application.
 * It passes the event to the scene manager for handling.
 *
 * @param context The application context.
 * @param event The custom event that was triggered.
 * @return True if the event was handled, false otherwise.
 */

/**
 * @brief Allocates and initializes the application.
 *
 * This function allocates the main application struct and initializes all its
 * components, including the GUI, view dispatcher, scene manager, and views.
 *
 * @return A pointer to the allocated App struct.
 */
static App* flippass_app_alloc(const char* args) {
    App* app = malloc(sizeof(App));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&flippass_scene_handlers, app);
    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, flippass_input_events_callback, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flippass_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flippass_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->file_path = furi_string_alloc();
    app->keyboard_layout_path = furi_string_alloc();
    app->file_browser = file_browser_alloc(app->file_path);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewFileBrowser, file_browser_get_view(app->file_browser));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewPasswordEntry, text_input_get_view(app->text_input));

    app->progress_view = flippass_progress_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewLoading, flippass_progress_view_get_view(app->progress_view));

    app->db_browser = flippass_db_browser_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewDbBrowser,
        flippass_db_browser_view_get_view(app->db_browser));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewWidget, widget_get_view(app->widget));

    app->dialog_ex = dialog_ex_alloc();
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->kdbx_parser = kdbx_parser_alloc();
    app->db_arena = NULL;
    app->vault = NULL;
    app->active_vault_backend = KDBXVaultBackendNone;
    app->requested_vault_backend = KDBXVaultBackendRam;
    app->pending_gzip_scratch_vault = NULL;
    memset(&app->pending_gzip_scratch_ref, 0, sizeof(app->pending_gzip_scratch_ref));
    app->pending_gzip_plain_size = 0U;
    app->root_group = NULL;
    app->current_group = NULL;
    app->current_entry = NULL;
    app->active_group = NULL;
    app->active_entry = NULL;
    app->text_view_body = furi_string_alloc();
    kdbx_protected_stream_reset(&app->protected_stream);
    app->usb_if_prev = NULL;
    app->usb_was_locked = false;
    app->usb_expect_rpc_session_close = false;
    app->ble_session = NULL;
    app->browser_selected_index = 0;
    app->action_selected_index = 0;
    app->other_field_selected_index = 0;
    app->other_field_action_selected_index = 0;
    app->pending_entry_action = FlipPassEntryActionNone;
    app->pending_other_field_mask = 0U;
    app->pending_other_custom_field = NULL;
    app->pending_other_field_name[0] = '\0';
    app->keyboard_layout_return_scene = FlipPassScene_DbEntries;
    app->close_db_dialog_open = false;
    app->parse_failed = false;
    app->database_loaded = false;
    app->pending_vault_fallback = false;
    app->allow_ext_vault_promotion = false;
    app->close_test_logged = false;
    app->status_return_scene = FlipPassScene_FileBrowser;
    app->text_view_title[0] = '\0';
    app->text_view_return_scene = FlipPassScene_DbEntries;
    app->progress_started_tick = 0U;
    app->progress_percent = 0U;
    app->progress_title[0] = '\0';
    app->rpc = NULL;
    app->rpc_mode = false;
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    flippass_set_status(app, "FlipPass", "");
    flippass_log_reset(app);
    flippass_system_log_capture_init(app);
    flippass_log_event(app, "APP_START");
    FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=session_cleanup_begin");
    Storage* storage_for_cleanup = furi_record_open(RECORD_STORAGE);
    kdbx_vault_cleanup_runtime_sessions(storage_for_cleanup);
    furi_record_close(RECORD_STORAGE);
    FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=session_cleanup_done");
    flippass_rpc_init(app, args);
    FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=rpc_init mode=%u", app->rpc_mode ? 1U : 0U);

    flippass_load_settings(app);
    FLIPPASS_STARTUP_LOG(
        app,
        "APP_INIT_STEP step=load_settings has_path=%u",
        (app->file_path != NULL && !furi_string_empty(app->file_path)) ? 1U : 0U);

    // Start the application on the file browser scene
    scene_manager_next_scene(app->scene_manager, FlipPassScene_FileBrowser);
    FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=file_browser_scene");

    // If a valid last file is found, transition to the password entry scene
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool has_last_file = app->file_path != NULL &&
                               storage_file_exists(storage, furi_string_get_cstr(app->file_path));
    FLIPPASS_STARTUP_LOG(
        app, "APP_INIT_STEP step=last_file_exists ok=%u", has_last_file ? 1U : 0U);
    if(has_last_file) {
        scene_manager_next_scene(app->scene_manager, FlipPassScene_PasswordEntry);
        FLIPPASS_STARTUP_LOG(app, "APP_INIT_STEP step=password_entry_scene");
    }
    furi_record_close(RECORD_STORAGE);

    return app;
}

/**
 * @brief Frees the application and its resources.
 *
 * This function deallocates all the resources used by the application,
 * including views, view dispatcher, scene manager, and the app struct itself.
 *
 * @param app A pointer to the App struct to free.
 */
static void flippass_app_free(App* app) {
    furi_assert(app);

    furi_pubsub_unsubscribe(app->input_events, app->input_subscription);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewFileBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewPasswordEntry);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewDbBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewDialogEx);
    dialog_ex_free(app->dialog_ex);
    widget_free(app->widget);
    submenu_free(app->submenu);
    flippass_db_browser_view_free(app->db_browser);
    flippass_progress_view_free(app->progress_view);
    text_input_free(app->text_input);
    file_browser_free(app->file_browser);
    furi_string_free(app->file_path);
    furi_string_free(app->keyboard_layout_path);
    flippass_reset_database(app);
    flippass_output_cleanup(app);
    kdbx_parser_free(app->kdbx_parser);
    furi_string_free(app->text_view_body);
    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    flippass_log_event(app, "APP_EXIT");
    flippass_system_log_capture_deinit(app);
    flippass_rpc_deinit(app);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_INPUT_EVENTS);
    free(app);
}

/**
 * @brief Main entry point for the FlipPass application.
 *
 * This function is the main entry point for the application. It allocates
 * the app, runs the view dispatcher, and then frees the app resources.
 *
 * @param p Unused parameter.
 * @return 0 on success.
 */
int32_t flippass_app(void* p) {
    const char* args = p;
    App* app = flippass_app_alloc(args);
    view_dispatcher_run(app->view_dispatcher);
    flippass_app_free(app);
    return 0;
}

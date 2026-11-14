#include "uart_utils.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include "sequential_file.h"
#include <gui/modules/text_box.h>
#include <furi_hal_serial.h>
#include <storage/storage.h>

uint32_t g_uart_rx_session_bytes = 0;
bool g_uart_rx_session_started = false;
uint32_t g_uart_callback_count = 0;

#define WORKER_ALL_RX_EVENTS   (WorkerEvtStop | WorkerEvtRxDone)
#define PCAP_WRITE_CHUNK_SIZE  1024
#define AP_LIST_TIMEOUT_MS     5000
#define INITIAL_BUFFER_SIZE    2048
#define BUFFER_GROWTH_FACTOR   1.5
#define MUTEX_TIMEOUT_MS       2500
#define BUFFER_CLEAR_SIZE      128
#define BUFFER_RESIZE_CHUNK    1024
#define TEXT_SCROLL_GUARD_SIZE 64
#define TEXT_TRUNCATE_FRACTION 6

typedef enum {
    MARKER_STATE_IDLE,
    MARKER_STATE_BEGIN,
    MARKER_STATE_CLOSE
} MarkerState;

static TextBufferManager* text_buffer_alloc(char* backing_buffer) {
    if(!backing_buffer) return NULL;

    TextBufferManager* manager = malloc(sizeof(TextBufferManager));
    if(!manager) return NULL;

    manager->buffer = backing_buffer;
    manager->capacity = TEXT_LOG_BUFFER_SIZE;
    manager->length = 0;
    manager->view_buffer_len = 0;
    manager->view_offset = 0;
    manager->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    if(!manager->mutex) {
        free(manager);
        return NULL;
    }

    memset(manager->buffer, 0, manager->capacity);
    return manager;
}
static void text_buffer_free(TextBufferManager* manager) {
    if(!manager) return;
    if(manager->mutex) furi_mutex_free(manager->mutex);
    free(manager);
}

static void text_buffer_truncate_oldest(TextBufferManager* manager) {
    if(manager->length < manager->capacity / TEXT_TRUNCATE_FRACTION) return;

    size_t keep = manager->capacity - (manager->capacity / TEXT_TRUNCATE_FRACTION);
    if(keep > manager->length) keep = manager->length;

    size_t shift = manager->length - keep;
    if(shift > 0 && keep > 0) {
        memmove(manager->buffer, manager->buffer + shift, keep);
    }

    manager->length = keep;
    manager->buffer[manager->length] = '\0';
}

static void text_buffer_add(TextBufferManager* manager, const char* data, size_t len) {
    if(!manager || !data || !len) return;

    if(furi_mutex_acquire(manager->mutex, 300) != FuriStatusOk) {
        FURI_LOG_E("UART", "Mutex timeout! Dropping data");
        return;
    }

    if(manager->length + len >= manager->capacity) {
        text_buffer_truncate_oldest(manager);
    }

    size_t space = manager->capacity - manager->length - 1;
    size_t to_copy = (len > space) ? space : len;
    if(to_copy > 0) {
        memcpy(manager->buffer + manager->length, data, to_copy);
        manager->length += to_copy;
        manager->buffer[manager->length] = '\0';
    }

    furi_mutex_release(manager->mutex);
}
void uart_reset_text_buffers(UartContext* uart) {
    if(!uart || !uart->text_manager) return;

    if(furi_mutex_acquire(uart->text_manager->mutex, 300) != FuriStatusOk) {
        return;
    }

    memset(uart->text_manager->buffer, 0, uart->text_manager->capacity);
    uart->text_manager->length = 0;
    uart->text_manager->view_buffer_len = 0;
    uart->text_manager->view_offset = 0;

    furi_mutex_release(uart->text_manager->mutex);
}
bool uart_copy_text_buffer(UartContext* uart, char* out, size_t out_size, size_t* out_len) {
    if(out_len) *out_len = 0;
    if(!uart || !uart->text_manager || !out || out_size == 0) {
        return false;
    }

    if(furi_mutex_acquire(uart->text_manager->mutex, 300) != FuriStatusOk) {
        return false;
    }

    size_t len = uart->text_manager->view_buffer_len;
    size_t offset = uart->text_manager->view_offset;
    if(len >= out_size) {
        len = out_size - 1;
    }

    if(len > 0) {
        size_t end = offset + len;
        if(end >= uart->text_manager->capacity) end = uart->text_manager->capacity - 1;
        char saved = uart->text_manager->buffer[end];
        uart->text_manager->buffer[end] = '\0';
        memcpy(out, uart->text_manager->buffer + offset, len);
        uart->text_manager->buffer[end] = saved;
    }
    out[len] = '\0';
    if(out_len) *out_len = len;

    furi_mutex_release(uart->text_manager->mutex);

    return true;
}

bool uart_copy_text_buffer_tail(UartContext* uart, char* out, size_t out_size, size_t* out_len) {
    if(out_len) *out_len = 0;
    if(!uart || !uart->text_manager || !out || out_size == 0) {
        return false;
    }

    if(furi_mutex_acquire(uart->text_manager->mutex, 300) != FuriStatusOk) {
        return false;
    }

    TextBufferManager* mgr = uart->text_manager;
    size_t available = mgr->length;

    size_t copy_size = (available >= out_size) ? (out_size - 1) : available;
    size_t start = (available > copy_size) ? (available - copy_size) : 0;

    if(copy_size > 0) {
        size_t end = start + copy_size;
        if(end >= mgr->capacity) end = mgr->capacity - 1;
        char saved = mgr->buffer[end];
        mgr->buffer[end] = '\0';
        memcpy(out, mgr->buffer + start, copy_size);
        mgr->buffer[end] = saved;
    }
    out[copy_size] = '\0';
    if(out_len) *out_len = copy_size;

    furi_mutex_release(uart->text_manager->mutex);
    return true;
}

static void text_buffer_update_view(TextBufferManager* manager, bool view_from_start) {
    if(!manager) return;

    furi_mutex_acquire(manager->mutex, FuriWaitForever);

    size_t available = manager->length;
    size_t copy_size = (available > VIEW_BUFFER_SIZE - 1) ? VIEW_BUFFER_SIZE - 1 : available;

    size_t start = 0;
    if(!view_from_start && available > copy_size) {
        start = available - copy_size;
    }

    manager->view_offset = start;
    manager->view_buffer_len = copy_size;

    furi_mutex_release(manager->mutex);
}

static char text_buffer_mark_view_terminator(TextBufferManager* manager) {
    size_t end = manager->view_offset + manager->view_buffer_len;
    if(end >= manager->capacity) end = manager->capacity - 1;
    char saved = manager->buffer[end];
    manager->buffer[end] = '\0';
    return saved;
}

static void text_buffer_restore_view_terminator(TextBufferManager* manager, char saved_char) {
    size_t end = manager->view_offset + manager->view_buffer_len;
    if(end >= manager->capacity) end = manager->capacity - 1;
    manager->buffer[end] = saved_char;
}
// The interrupt owns only the input queue. Parsing, files and UI belong to
// the worker, so no capture-start allocation or IRQ-side mutex is needed.
static void
    uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UartContext* uart = context;
    if(!uart || event != FuriHalSerialRxEventData) return;
    uint8_t data = furi_hal_serial_async_rx(handle);
    ++g_uart_callback_count;
    ++g_uart_rx_session_bytes;
    uart->rx_last_tick = furi_get_tick();
    if(furi_stream_buffer_send(uart->rx_stream, &data, 1, 0) != 1) ++uart->rx_dropped_bytes;
    if(uart->rx_thread)
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
}

void handle_uart_rx_data(uint8_t* buf, size_t len, void* context) {
    AppState* state = (AppState*)context;
    if(!state || !state->uart_context || !state->uart_context->is_serial_active || !buf ||
       len == 0) {
        FURI_LOG_W("UART", "Invalid parameters in handle_uart_rx_data");
        return;
    }

    // This stream was already classified as text by the RX parser. Its routing
    // must not depend on whether a later frame is currently arriving.
    if(state->uart_context->storageContext && state->uart_context->storageContext->log_file &&
       storage_file_is_open(state->uart_context->storageContext->log_file)) {
        static size_t bytes_since_sync = 0;

        size_t written =
            storage_file_write(state->uart_context->storageContext->log_file, buf, len);

        if(written != len) {
            FURI_LOG_E("UART", "Failed to write log data: expected %zu, wrote %zu", len, written);
        } else {
            bytes_since_sync += written;
            if(bytes_since_sync >= 8192 && state->uart_context->capture_type == UartFrameText) {
                storage_file_sync(state->uart_context->storageContext->log_file);
                bytes_since_sync = 0;
                FURI_LOG_D("UART", "Synced log file to storage");
            }
        }
    }

    // Update text display
    text_buffer_add(state->uart_context->text_manager, (char*)buf, len);

    if(!state->text_box_user_scrolled) {
        text_buffer_update_view(
            state->uart_context->text_manager, state->settings.view_logs_from_start_index);

        bool view_from_start = state->settings.view_logs_from_start_index;
        if(state->text_box) {
            TextBufferManager* mgr = state->uart_context->text_manager;
            furi_mutex_acquire(mgr->mutex, FuriWaitForever);
            char saved = text_buffer_mark_view_terminator(mgr);
            text_box_set_text(state->text_box, mgr->buffer + mgr->view_offset);
            text_box_set_focus(
                state->text_box, view_from_start ? TextBoxFocusStart : TextBoxFocusEnd);
            text_buffer_restore_view_terminator(mgr, saved);
            furi_mutex_release(mgr->mutex);
        }
    }
}

static void uart_flush_output(UartContext* uart) {
    if(!uart->write_len) return;
    if(uart->write_type == UartFrameText) {
        if(uart->handle_rx_data_cb && uart->state)
            uart->handle_rx_data_cb(uart->write_buf, uart->write_len, uart->state);
    } else if(uart->write_type == UartFrameCsv) {
        if(uart->handle_rx_csv_cb) uart->handle_rx_csv_cb(uart->write_buf, uart->write_len, uart);
    } else {
        if(uart->handle_rx_pcap_cb)
            uart->handle_rx_pcap_cb(uart->write_buf, uart->write_len, uart);
    }
    uart->write_len = 0;
}

static void uart_route_output(UartFrameType type, const uint8_t* data, size_t len, void* context) {
    UartContext* uart = context;
    if(!len) {
        uart_flush_output(uart);
        if(type == UartFramePcap) uart->capture_sync_pending = true;
        return;
    }
    if(uart->capture_type == UartFrameText) type = UartFrameText;
    if(type != uart->write_type) uart_flush_output(uart);
    uart->write_type = type;
    while(len) {
        size_t count = sizeof(uart->write_buf) - uart->write_len;
        if(count > len) count = len;
        memcpy(uart->write_buf + uart->write_len, data, count);
        uart->write_len += count;
        data += count;
        len -= count;
        if(uart->write_len == sizeof(uart->write_buf)) uart_flush_output(uart);
    }
}

// Caller holds worker_mutex; the IRQ can continue filling the single queue.
static void uart_drain_input(UartContext* uart) {
    uint8_t input[128];
    size_t len;
    while((len = furi_stream_buffer_receive(uart->rx_stream, input, sizeof(input), 0)) != 0) {
        uart_framing_feed(&uart->framing, input, len, uart_route_output, uart);
    }
    uart_flush_output(uart);
    if(uart->capture_sync_pending && uart->storageContext &&
       storage_file_is_open(uart->storageContext->current_file)) {
        storage_file_sync(uart->storageContext->current_file);
    }
    uart->capture_sync_pending = false;
}

static int32_t uart_worker(void* context) {
    UartContext* uart = context;
    if(!uart) return -1;
    uint32_t reported_drops = 0;
    for(;;) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_mutex_acquire(uart->worker_mutex, FuriWaitForever);
        uart_drain_input(uart);
        if(events & WorkerEvtStop) {
            uart_framing_finish(&uart->framing, uart_route_output, uart);
            uart_flush_output(uart);
        }
        uint32_t dropped = uart->rx_dropped_bytes;
        if(dropped != reported_drops) {
            FURI_LOG_E("UART", "RX buffer overflow: %lu bytes lost in this session", dropped);
            reported_drops = dropped;
        }
        furi_mutex_release(uart->worker_mutex);
        if(events & WorkerEvtStop) break;
    }
    return 0;
}

void update_text_box_view(AppState* state) {
    if(!state || !state->text_box || !state->uart_context || !state->uart_context->text_manager)
        return;

    text_buffer_update_view(
        state->uart_context->text_manager, state->settings.view_logs_from_start_index);

    bool view_from_start = state->settings.view_logs_from_start_index;
    TextBufferManager* mgr = state->uart_context->text_manager;
    furi_mutex_acquire(mgr->mutex, FuriWaitForever);
    char saved = text_buffer_mark_view_terminator(mgr);
    text_box_set_text(state->text_box, mgr->buffer + mgr->view_offset);
    text_box_set_focus(state->text_box, view_from_start ? TextBoxFocusStart : TextBoxFocusEnd);
    text_buffer_restore_view_terminator(mgr, saved);
    furi_mutex_release(mgr->mutex);
}
UartContext* uart_init(AppState* state) {
    uint32_t start_time = furi_get_tick();
    FURI_LOG_I("UART", "Starting UART initialization");

    UartContext* uart = malloc(sizeof(UartContext));
    if(!uart) {
        FURI_LOG_E("UART", "Failed to allocate UART context");
        return NULL;
    }
    memset(uart, 0, sizeof(UartContext));
    uart->worker_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!uart->worker_mutex) {
        free(uart);
        return NULL;
    }

    uart->state = state;
    uart->is_serial_active = false;
    uart_framing_reset(&uart->framing, UartFrameText);
    uart->rx_stream = furi_stream_buffer_alloc(UART_RX_QUEUE_SIZE, 1);

    if(!uart->rx_stream) {
        FURI_LOG_E("UART", "Failed to allocate rx stream buffer");
        uart_free(uart);
        return NULL;
    }

    // Set callbacks
    uart->handle_rx_data_cb = handle_uart_rx_data;
    uart->handle_rx_pcap_cb = uart_storage_rx_callback;
    uart->handle_rx_csv_cb = uart_storage_rx_callback;

    // Initialize storage
    uart->storageContext = uart_storage_init(uart);
    if(!uart->storageContext) {
        FURI_LOG_E("UART", "Failed to initialize storage");
        uart_free(uart);
        return NULL;
    }

    // Initialize serial with firmware-aware channel selection
    FuriHalSerialId uart_channel = UART_CH_ESP;

    uart->serial_handle = furi_hal_serial_control_acquire(uart_channel);
    if(uart->serial_handle) {
        furi_hal_serial_init(uart->serial_handle, 115200);
        uart->is_serial_active = true;
    } else {
        FURI_LOG_E("UART", "Failed to acquire serial handle");
        uart_free(uart);
        return NULL;
    }

    // Initialize text manager
    uart->text_manager = text_buffer_alloc(state->textBoxBuffer);
    if(!uart->text_manager) {
        FURI_LOG_E("UART", "Failed to allocate text manager");
        uart_free(uart);
        return NULL;
    }

    // Initialize RX thread
    uart->rx_thread = furi_thread_alloc();
    if(uart->rx_thread) {
        furi_thread_set_name(uart->rx_thread, "UART_Receive");
        furi_thread_set_stack_size(uart->rx_thread, 4096);
        furi_thread_set_context(uart->rx_thread, uart);
        furi_thread_set_callback(uart->rx_thread, uart_worker);
        furi_thread_start(uart->rx_thread);
    } else {
        FURI_LOG_E("UART", "Failed to allocate rx thread");
        uart_free(uart);
        return NULL;
    }

    furi_hal_serial_async_rx_start(uart->serial_handle, uart_rx_callback, uart, false);

    uint32_t duration = furi_get_tick() - start_time;
    FURI_LOG_I("UART", "UART initialization complete (Time taken: %lu ms)", duration);

    return uart;
}

void uart_free(UartContext* uart) {
    if(!uart) {
        FURI_LOG_W("UART", "Attempted to free NULL UART context");
        return;
    }

    FURI_LOG_I("UART", "Starting UART cleanup...");

    // First, stop any ongoing UART operations
    if(uart->serial_handle) {
        FURI_LOG_I("UART", "Stopping UART hardware...");
        // Stop async RX first to prevent new callbacks
        furi_hal_serial_async_rx_stop(uart->serial_handle);
        // Clear any pending data
        furi_hal_serial_tx_wait_complete(uart->serial_handle);
        // Release the hardware
        furi_hal_serial_deinit(uart->serial_handle);
        furi_hal_serial_control_release(uart->serial_handle);
        uart->serial_handle = NULL;
        FURI_LOG_I("UART", "UART hardware stopped");
    }

    // Stop the worker thread
    if(uart->rx_thread) {
        FURI_LOG_I("UART", "Stopping worker thread...");
        // Signal thread to stop
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
        // Wait for thread to finish with a timeout
        FuriStatus status = furi_thread_join(uart->rx_thread);
        if(status != FuriStatusOk) {
            FURI_LOG_W("UART", "Thread join failed with status: %d", status);
        }
        furi_thread_free(uart->rx_thread);
        uart->rx_thread = NULL;
        FURI_LOG_I("UART", "Worker thread stopped");
    }

    // Now it's safe to free resources
    FURI_LOG_I("UART", "Freeing resources...");

    // Free streams
    if(uart->rx_stream) {
        furi_stream_buffer_free(uart->rx_stream);
        uart->rx_stream = NULL;
    }

    // Clean up storage context
    if(uart->storageContext) {
        uart_storage_free(uart->storageContext);
        uart->storageContext = NULL;
    }

    // Free text manager
    if(uart->text_manager) {
        text_buffer_free(uart->text_manager);
        uart->text_manager = NULL;
    }

    // Finally free the UART context
    if(uart->worker_mutex) furi_mutex_free(uart->worker_mutex);
    FURI_LOG_I("UART", "Freeing UART context");
    free(uart);
    FURI_LOG_I("UART", "UART cleanup complete");
}

// Stop the UART thread (typically when exiting)
void uart_stop_thread(UartContext* uart) {
    if(uart && uart->rx_thread) {
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
    }
}

// Send data over UART
void uart_send(UartContext* uart, const uint8_t* data, size_t len) {
    if(!uart || !uart->serial_handle || !uart->is_serial_active || !data || len == 0) {
        return;
    }

    // Send data directly without mutex lock for basic commands
    furi_hal_serial_tx(uart->serial_handle, data, len);

    // Small delay to ensure transmission
    furi_delay_ms(5);
}

bool uart_is_esp_connected(UartContext* uart) {
    FURI_LOG_D("UART", "Checking ESP connection...");

    if(!uart || !uart->serial_handle || !uart->text_manager) {
        FURI_LOG_E("UART", "Invalid UART context");
        return false;
    }

    // Check if ESP check is disabled
    if(uart->state && uart->state->settings.disable_esp_check_index) {
        FURI_LOG_D("UART", "ESP connection check disabled by setting");
        return true;
    }

    // Temporarily disable callbacks
    furi_hal_serial_async_rx_stop(uart->serial_handle);

    // Clear and reset buffers atomically
    furi_mutex_acquire(uart->text_manager->mutex, FuriWaitForever);
    memset(uart->text_manager->buffer, 0, uart->text_manager->capacity);
    uart->text_manager->length = 0;
    uart->text_manager->view_buffer_len = 0;
    uart->text_manager->view_offset = 0;
    furi_mutex_release(uart->text_manager->mutex);

    // Re-enable callbacks with clean state
    furi_hal_serial_async_rx_start(uart->serial_handle, uart_rx_callback, uart, false);

    // Quick flush
    furi_hal_serial_tx(uart->serial_handle, (uint8_t*)"\r\n", 2);
    furi_delay_ms(50);

    const char* test_commands[] = {
        "stop\n", // Try stop command first
        "AT\r\n", // AT command as backup
    };
    bool connected = false;
    const uint32_t CMD_TIMEOUT_MS = 250; // Shorter timeout per command

    for(uint8_t cmd_idx = 0;
        cmd_idx < sizeof(test_commands) / sizeof(test_commands[0]) && !connected;
        cmd_idx++) {
        // Send test command
        uart_send(uart, (uint8_t*)test_commands[cmd_idx], strlen(test_commands[cmd_idx]));
        FURI_LOG_D("UART", "Sent command: %s", test_commands[cmd_idx]);

        uint32_t start_time = furi_get_tick();
        while(furi_get_tick() - start_time < CMD_TIMEOUT_MS) {
            furi_mutex_acquire(uart->text_manager->mutex, FuriWaitForever);

            size_t available = uart->text_manager->length;

            if(available > 0) {
                connected = true;
                FURI_LOG_D("UART", "Received %d bytes response", available);
            }

            furi_mutex_release(uart->text_manager->mutex);

            if(connected) break;
            furi_delay_ms(5); // Shorter sleep interval
        }
    }

    FURI_LOG_I("UART", "ESP connection check: %s", connected ? "Success" : "Failed");
    return connected;
}

void uart_wait_capture_idle(UartContext* uart) {
    if(!uart || uart->capture_type == UartFrameText) return;
    uint32_t started = furi_get_tick();
    while(furi_get_tick() - started < furi_ms_to_ticks(3000)) {
        furi_mutex_acquire(uart->worker_mutex, FuriWaitForever);
        bool idle = uart->framing.frame_type == UartFrameText && !uart->framing.prefix_len &&
                    furi_stream_buffer_bytes_available(uart->rx_stream) == 0;
        furi_mutex_release(uart->worker_mutex);
        uint32_t now = furi_get_tick();
        if(now - started >= furi_ms_to_ticks(350) && idle &&
           now - uart->rx_last_tick >= furi_ms_to_ticks(150))
            return;
        furi_delay_ms(10);
    }
    FURI_LOG_W("UART", "Timed out waiting for capture stream to finish");
}

void uart_cleanup_capture_streams(UartContext* uart) {
    if(!uart) return;
    if(uart->is_serial_active) furi_hal_serial_async_rx_stop(uart->serial_handle);
    furi_mutex_acquire(uart->worker_mutex, FuriWaitForever);
    uart_drain_input(uart);
    uart_framing_finish(&uart->framing, uart_route_output, uart);
    uart_flush_output(uart);
    uart->capture_type = UartFrameText;
    if(uart->storageContext) {
        if(storage_file_is_open(uart->storageContext->current_file))
            storage_file_sync(uart->storageContext->current_file);
        if(storage_file_is_open(uart->storageContext->log_file))
            storage_file_sync(uart->storageContext->log_file);
    }
    furi_mutex_release(uart->worker_mutex);
    if(uart->is_serial_active)
        furi_hal_serial_async_rx_start(uart->serial_handle, uart_rx_callback, uart, false);
}

bool uart_receive_data(
    UartContext* uart,
    ViewDispatcher* view_dispatcher,
    AppState* state,
    const char* prefix,
    const char* extension,
    const char* TargetFolder) {
    if(!uart || !uart->storageContext || !view_dispatcher || !state) {
        FURI_LOG_E("UART", "Invalid parameters to uart_receive_data");
        return false;
    }

    // Any new log session starts as a normal one; the connect flow opts in to
    // connect_log_active after this call returns.
    state->connect_log_active = false;

    uart_cleanup_capture_streams(uart);
    if(uart->is_serial_active) {
        furi_hal_serial_async_rx_stop(uart->serial_handle);
    }
    furi_mutex_acquire(uart->worker_mutex, FuriWaitForever);

    // Close any existing file
    if(storage_file_is_open(uart->storageContext->current_file)) {
        storage_file_sync(uart->storageContext->current_file);
        storage_file_close(uart->storageContext->current_file);
        uart->storageContext->HasOpenedFile = false;
    }

    // Capture mode changes only the destination. The UART queue is shared
    // across text, CSV and PCAP, with no allocation at capture startup.
    uart->capture_type = UartFrameText;
    if(extension && strcmp(extension, "pcap") == 0) uart->capture_type = UartFramePcap;
    if(extension && strcmp(extension, "csv") == 0) uart->capture_type = UartFrameCsv;
    furi_stream_buffer_reset(uart->rx_stream);
    uart_framing_reset(&uart->framing, uart->capture_type);
    uart->write_len = 0;

    g_uart_rx_session_bytes = 0;
    g_uart_rx_session_started = false;
    g_uart_callback_count = 0;
    uart->rx_dropped_bytes = 0;

    FURI_LOG_I("UART", "[INIT] Reset RX logging counters for new session");

    state->text_box_user_scrolled = false;

    if(uart->text_manager) {
        const char* hint = "Press Right to resume.\nPress OK to STOP.\n";
        text_buffer_add(uart->text_manager, hint, strlen(hint));
        text_buffer_update_view(uart->text_manager, state->settings.view_logs_from_start_index);

        if(state->text_box) {
            bool view_from_start = state->settings.view_logs_from_start_index;
            TextBufferManager* mgr = state->uart_context->text_manager;
            char saved = text_buffer_mark_view_terminator(mgr);
            text_box_set_text(state->text_box, mgr->buffer + mgr->view_offset);
            text_box_set_focus(
                state->text_box, view_from_start ? TextBoxFocusStart : TextBoxFocusEnd);
            text_buffer_restore_view_terminator(mgr, saved);
        }
    } else if(state->text_box) {
        const char* hint = "Press Right to resume.\nPress OK to STOP.\n";
        text_box_set_text(state->text_box, hint);
        bool view_from_start = state->settings.view_logs_from_start_index;
        text_box_set_focus(state->text_box, view_from_start ? TextBoxFocusStart : TextBoxFocusEnd);
    }

    // Open new file if needed
    if(prefix && extension && TargetFolder && strlen(prefix) > 1) {
        uart->storageContext->HasOpenedFile = sequential_file_open(
            uart->storageContext->storage_api,
            uart->storageContext->current_file,
            TargetFolder,
            prefix,
            extension);

        if(!uart->storageContext->HasOpenedFile) {
            FURI_LOG_E("UART", "Failed to open file");
            uart->capture_type = UartFrameText;
            uart_framing_reset(&uart->framing, UartFrameText);
            furi_mutex_release(uart->worker_mutex);
            if(uart->is_serial_active) {
                furi_hal_serial_async_rx_start(uart->serial_handle, uart_rx_callback, uart, false);
            }
            return false;
        }
    }

    furi_mutex_release(uart->worker_mutex);
    // The destination file and routing are ready before the first RX byte.
    if(uart->is_serial_active) {
        furi_hal_serial_async_rx_start(uart->serial_handle, uart_rx_callback, uart, false);
    }

    // Set the view state before switching
    state->previous_view = state->current_view;
    state->current_view = VIEW_TEXT_BOX;

    furi_delay_ms(5);

    view_dispatcher_switch_to_view(view_dispatcher, VIEW_TEXT_BOX);

    return true;
}

// 6675636B796F7564656B69

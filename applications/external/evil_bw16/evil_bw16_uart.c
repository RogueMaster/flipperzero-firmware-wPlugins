#include "evil_bw16.h"
#include <furi_hal.h>
#include <ctype.h>

#define BAUDRATE                (115200)
#define RX_BUFFER_SIZE          (512)
#define MAX_RECENT_COMMANDS     10
#define COMMAND_ECHO_TIMEOUT_MS 1000

// Forward declarations for response handler functions
static void handle_info_response(EvilBw16UartWorker* worker, const char* line);
static void handle_error_response(EvilBw16UartWorker* worker, const char* line);
static void handle_debug_response(EvilBw16UartWorker* worker, const char* line);
static void handle_cmd_response(EvilBw16UartWorker* worker, const char* line);
static void handle_mgmt_response(EvilBw16UartWorker* worker, const char* line);
static void handle_data_response(EvilBw16UartWorker* worker, const char* line);
static void handle_hop_response(EvilBw16UartWorker* worker, const char* line);
static void handle_generic_response(EvilBw16UartWorker* worker, const char* line);
static void parse_scan_result_line(EvilBw16UartWorker* worker, const char* line);
static bool is_command_echo(EvilBw16UartWorker* worker, const char* line);
static void store_sent_command(EvilBw16UartWorker* worker, const char* command);

struct EvilBw16UartWorker {
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;
    bool running;
    EvilBw16App* app;
    FuriMutex* tx_mutex;
    char recent_commands[MAX_RECENT_COMMANDS][64];
    uint32_t command_timestamps[MAX_RECENT_COMMANDS];
    int recent_command_index;
    char* line_buffer; // Move line buffer to heap to reduce stack usage
    uint32_t last_log_update; // Rate limiting for high-volume WebUI data
};

static EvilBw16UartWorker* uart_worker = NULL;

// UART receive callback function
static void
    uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    EvilBw16UartWorker* worker = (EvilBw16UartWorker*)context;

    if(event == FuriHalSerialRxEventData) {
        // Read available data and put it in stream buffer
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t data = furi_hal_serial_async_rx(handle);
            furi_stream_buffer_send(worker->rx_stream, &data, 1, 0);
        }
    }
}

// Worker thread for processing UART data
static int32_t uart_worker_thread(void* context) {
    EvilBw16UartWorker* worker = (EvilBw16UartWorker*)context;

    char* line_buffer = worker->line_buffer; // Use heap-allocated buffer
    size_t line_pos = 0;
    const size_t max_line_size = 512;

    while(worker->running) {
        uint8_t byte;
        size_t received = furi_stream_buffer_receive(worker->rx_stream, &byte, 1, 100);

        if(received > 0) {
            // Handle incoming byte
            if(byte == '\n' || byte == '\r') {
                if(line_pos > 0) {
                    line_buffer[line_pos] = '\0';

                    // Skip empty lines and filter WebUI spam
                    if(strlen(line_buffer) == 0) {
                        line_pos = 0;
                        continue;
                    }

                    // Filter out WebUI spam patterns to reduce log noise
                    if(strstr(line_buffer, "WebSocket") != NULL ||
                       strstr(line_buffer, "HTTP GET") != NULL ||
                       strstr(line_buffer, "Content-Type:") != NULL ||
                       strstr(line_buffer, "Connection:") != NULL ||
                       (line_buffer[0] == '{' && strstr(line_buffer, "\"type\"") != NULL)) {
                        line_pos = 0;
                        continue; // Skip WebUI internal messages
                    }

                    // Process complete line
                    FURI_LOG_I("EvilBw16", "RX: %s", line_buffer);

                    // Check if this is a command echo - if so, only skip response processing, not display
                    bool is_echo = is_command_echo(worker, line_buffer);

                    // Parse different response types only if not an echo
                    if(!is_echo) {
                        if(strncmp(line_buffer, "[INFO]", 6) == 0) {
                            handle_info_response(worker, line_buffer);
                        } else if(strncmp(line_buffer, "[ERROR]", 7) == 0) {
                            handle_error_response(worker, line_buffer);
                        } else if(strncmp(line_buffer, "[DEBUG]", 7) == 0) {
                            handle_debug_response(worker, line_buffer);
                        } else if(strncmp(line_buffer, "[CMD]", 5) == 0) {
                            handle_cmd_response(worker, line_buffer);
                        } else if(strncmp(line_buffer, "[MGMT]", 6) == 0) {
                            handle_mgmt_response(worker, line_buffer);
                        } else if(strncmp(line_buffer, "[DATA]", 6) == 0) {
                            handle_data_response(worker, line_buffer);
                        } else if(strncmp(line_buffer, "[HOP]", 5) == 0) {
                            handle_hop_response(worker, line_buffer);
                        } else if(
                            strncmp(line_buffer, "RAW UART RX:", 12) == 0 ||
                            strncmp(line_buffer, "Received Command:", 17) == 0 ||
                            strncmp(line_buffer, "SCAN COMMAND", 12) == 0) {
                            // These are our debug messages from Arduino - handle specially
                            handle_debug_response(worker, line_buffer);
                        } else {
                            // Generic response - but be more selective
                            if(strlen(line_buffer) > 3) { // Only process substantial responses
                                handle_generic_response(worker, line_buffer);
                            }
                        }
                    }

                    // ALWAYS add to app log for live display (even command echoes)
                    if(worker->app) {
                        evil_bw16_append_log(worker->app, line_buffer);

                        // Rate limit UI updates for high-volume WebUI data
                        uint32_t now = furi_get_tick();
                        if(now - worker->last_log_update >
                           200) { // Max 5 refreshes per second for WebUI data
                            if(worker->app->view_dispatcher) {
                                // Simply send refresh event - the scene handler will ignore it if not in UART terminal
                                view_dispatcher_send_custom_event(
                                    worker->app->view_dispatcher,
                                    EvilBw16EventUartTerminalRefresh);
                                worker->last_log_update = now;
                            }
                        }
                    }
                }
                line_pos = 0;
            } else if(line_pos < max_line_size - 1) {
                line_buffer[line_pos++] = byte;
            } else {
                // Line too long, reset to prevent buffer overflow
                line_pos = 0;
                FURI_LOG_W("EvilBw16", "Line buffer overflow, resetting");
            }
        }
    }

    return 0;
}

// Response handler functions
static void handle_info_response(EvilBw16UartWorker* worker, const char* line) {
    if(!worker->app) return;

    // Write to debug log
    debug_write_to_sd(line);

    // Parse scan results - look for the actual header format
    if(strstr(line, "Index") != NULL && strstr(line, "SSID") != NULL &&
       strstr(line, "BSSID") != NULL) {
        // This is a scan result header - clear previous results
        worker->app->network_count = 0;
        memset(worker->app->networks, 0, sizeof(worker->app->networks));
        worker->app->scan_in_progress = true;
        FURI_LOG_I("EvilBw16", "Scan results header detected - cleared previous results");
    }
    // Look for actual scan result lines - must start with "[INFO] " followed by a digit and tab
    else if(strncmp(line, "[INFO] ", 7) == 0 && strlen(line) > 8) {
        const char* after_prefix = line + 7; // Skip "[INFO] "
        if(after_prefix[0] >= '0' && after_prefix[0] <= '9' &&
           strchr(after_prefix, '\t') != NULL) {
            // This looks like a scan result line: "[INFO] 0\tSSID\t..."
            parse_scan_result_line(worker, line);
        }
    }
    // Scan completion - wait for "Scan results printed" message
    else if(strstr(line, "Scan results printed") != NULL || strstr(line, "Scan completed") != NULL) {
        // Scan finished - update UI
        worker->app->scan_in_progress = false;
        FURI_LOG_I("EvilBw16", "Scan completed with %d networks", worker->app->network_count);
        // Send event to update scanner scene
        if(worker->app->view_dispatcher) {
            view_dispatcher_send_custom_event(
                worker->app->view_dispatcher, EvilBw16EventScanComplete);
        }
    } else if(strstr(line, "Deauth") != NULL) {
        // Attack progress notification
        FURI_LOG_I("EvilBw16", "Attack progress: %s", line);
    }
}

static void handle_error_response(EvilBw16UartWorker* worker, const char* line) {
    UNUSED(worker);
    FURI_LOG_E("EvilBw16", "Arduino Error: %s", line);
}

static void handle_debug_response(EvilBw16UartWorker* worker, const char* line) {
    UNUSED(worker);
    FURI_LOG_D("EvilBw16", "Arduino Debug: %s", line);
}

static void handle_cmd_response(EvilBw16UartWorker* worker, const char* line) {
    if(!worker->app) return;

    FURI_LOG_I("EvilBw16", "Command response: %s", line);

    // Update sniffer state based on command responses
    if(strstr(line, "sniffing mode") != NULL) {
        worker->app->sniffer_state.is_running = true;
        // Send event to update sniffer UI
        if(worker->app->view_dispatcher) {
            view_dispatcher_send_custom_event(
                worker->app->view_dispatcher, EvilBw16EventSnifferStarted);
        }
    } else if(strstr(line, "Sniffer stopped") != NULL) {
        worker->app->sniffer_state.is_running = false;
        if(worker->app->view_dispatcher) {
            view_dispatcher_send_custom_event(
                worker->app->view_dispatcher, EvilBw16EventSnifferStopped);
        }
    }
}

static void handle_mgmt_response(EvilBw16UartWorker* worker, const char* line) {
    if(!worker->app) return;

    // Increment packet count for management frames
    worker->app->sniffer_state.packet_count++;

    // Log the management frame
    FURI_LOG_I("EvilBw16", "MGMT Frame: %s", line);

    // Send event to update sniffer UI if in sniffer scene
    if(worker->app->view_dispatcher) {
        view_dispatcher_send_custom_event(
            worker->app->view_dispatcher, EvilBw16EventPacketReceived);
    }
}

static void handle_data_response(EvilBw16UartWorker* worker, const char* line) {
    if(!worker->app) return;

    // Increment packet count for data frames
    worker->app->sniffer_state.packet_count++;

    // Log the data frame (especially EAPOL)
    FURI_LOG_I("EvilBw16", "DATA Frame: %s", line);

    // Special handling for EAPOL
    if(strstr(line, "EAPOL") != NULL) {
        FURI_LOG_I("EvilBw16", "EAPOL handshake detected!");
        // Could add special notification here
    }

    if(worker->app->view_dispatcher) {
        view_dispatcher_send_custom_event(
            worker->app->view_dispatcher, EvilBw16EventPacketReceived);
    }
}

static void handle_hop_response(EvilBw16UartWorker* worker, const char* line) {
    UNUSED(worker);
    FURI_LOG_I("EvilBw16", "Channel hop: %s", line);
}

static void handle_generic_response(EvilBw16UartWorker* worker, const char* line) {
    UNUSED(worker);
    FURI_LOG_I("EvilBw16", "Generic: %s", line);
}

// Parse scan result line like: "[INFO] 0\tSSID_NAME\tBSSID\tChannel\tRSSI\tFrequency"
static void parse_scan_result_line(EvilBw16UartWorker* worker, const char* line) {
    if(!worker || !worker->app) return;
    if(worker->app->network_count >= EVIL_BW16_MAX_NETWORKS) return;

    // Skip "[INFO] " prefix (7 characters)
    const char* data = line + 7;

    // Manual tab parsing without strtok (not available in Flipper API)
    const char* field_starts[6] = {0}; // index, ssid, bssid, channel, rssi, frequency
    int field_lengths[6] = {0};
    int field_count = 0;

    // Find field boundaries manually
    const char* current = data;
    field_starts[field_count] = current;

    while(*current && field_count < 6) {
        if(*current == '\t' || *current == '\n' || *current == '\r') {
            field_lengths[field_count] = current - field_starts[field_count];
            field_count++;

            if(field_count < 6) {
                current++; // Skip the tab
                while(*current == ' ')
                    current++; // Skip any spaces after tab
                field_starts[field_count] = current;
            }
        } else {
            current++;
        }
    }

    // Handle the last field if we reached end of string
    if(field_count < 6 && *current == '\0') {
        field_lengths[field_count] = current - field_starts[field_count];
        field_count++;
    }

    // Need at least 5 fields (index, ssid, bssid, channel, rssi)
    if(field_count < 5) {
        FURI_LOG_W("EvilBw16", "Invalid scan line, only %d fields: %s", field_count, line);
        return;
    }

    // Check if we have space for more networks
    if(worker->app->network_count >= EVIL_BW16_MAX_NETWORKS) {
        FURI_LOG_W(
            "EvilBw16",
            "Maximum networks (%d) reached, ignoring additional network",
            EVIL_BW16_MAX_NETWORKS);
        return;
    }

    // Store network data
    int network_idx = worker->app->network_count;
    EvilBw16Network* network = &worker->app->networks[network_idx];

    // Helper function to convert field to string
    char temp_field[128];

    // Parse device index (field 0) - this is the index from the BW16 device
    int len = (field_lengths[0] < 127) ? field_lengths[0] : 127;
    strncpy(temp_field, field_starts[0], len);
    temp_field[len] = '\0';
    int device_index = atoi(temp_field);

    FURI_LOG_I(
        "EvilBw16", "UART: Parsing field[0]='%s' -> device_index=%d", temp_field, device_index);

    // Use our internal array index for consistency, but store device index for commands
    network->index = network_idx; // Internal array index for menu selection
    network->device_index = device_index; // Original device index for commands

    // Initialize selection state
    network->selected = false;

    // Copy SSID (field 1)
    len = (field_lengths[1] < (int)(sizeof(network->ssid) - 1)) ? field_lengths[1] :
                                                                  (int)(sizeof(network->ssid) - 1);
    strncpy(network->ssid, field_starts[1], len);
    network->ssid[len] = '\0';

    // Copy BSSID (field 2)
    len = (field_lengths[2] < (int)(sizeof(network->bssid) - 1)) ?
              field_lengths[2] :
              (int)(sizeof(network->bssid) - 1);
    strncpy(network->bssid, field_starts[2], len);
    network->bssid[len] = '\0';

    // Parse channel (field 3)
    len = (field_lengths[3] < 127) ? field_lengths[3] : 127;
    strncpy(temp_field, field_starts[3], len);
    temp_field[len] = '\0';
    network->channel = atoi(temp_field);

    // Parse RSSI (field 4)
    len = (field_lengths[4] < 127) ? field_lengths[4] : 127;
    strncpy(temp_field, field_starts[4], len);
    temp_field[len] = '\0';
    network->rssi = atoi(temp_field);

    // Determine band from frequency field or channel number
    if(field_count >= 6 && field_lengths[5] > 0) {
        len = (field_lengths[5] < 127) ? field_lengths[5] : 127;
        strncpy(temp_field, field_starts[5], len);
        temp_field[len] = '\0';

        if(strstr(temp_field, "5GHz")) {
            network->band = EvilBw16Band5GHz;
        } else {
            network->band = EvilBw16Band24GHz;
        }
    } else if(network->channel >= 36) {
        network->band = EvilBw16Band5GHz;
    } else {
        network->band = EvilBw16Band24GHz;
    }

    worker->app->network_count++;

    FURI_LOG_I(
        "EvilBw16",
        "Parsed network[%d] (device_idx=%d): '%s' (%s) Ch:%d RSSI:%d %s",
        network_idx,
        device_index,
        network->ssid,
        network->bssid,
        network->channel,
        network->rssi,
        (network->band == EvilBw16Band5GHz) ? "5GHz" : "2.4GHz");
}

EvilBw16UartWorker* evil_bw16_uart_init(EvilBw16App* app) {
    EvilBw16UartWorker* worker = malloc(sizeof(EvilBw16UartWorker));

    worker->app = app;
    worker->running = true;
    worker->tx_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    worker->rx_stream = furi_stream_buffer_alloc(RX_BUFFER_SIZE, 1);

    // Initialize echo filtering
    worker->recent_command_index = 0;
    memset(worker->recent_commands, 0, sizeof(worker->recent_commands));
    memset(worker->command_timestamps, 0, sizeof(worker->command_timestamps));

    // Allocate line buffer on heap to reduce stack usage
    worker->line_buffer = malloc(512); // Larger buffer for WebUI data
    if(!worker->line_buffer) {
        FURI_LOG_E("EvilBw16", "Failed to allocate line buffer");
        furi_stream_buffer_free(worker->rx_stream);
        furi_mutex_free(worker->tx_mutex);
        free(worker);
        return NULL;
    }
    worker->last_log_update = 0;

    // Get serial handle based on GPIO pin configuration
    FuriHalSerialId serial_id;
    const char* pin_description;

    if(app->config.gpio_pins == EvilBw16GpioPins13_14) {
        serial_id = FuriHalSerialIdUsart; // Pins 13/14
        pin_description = "USART (pins 13/14)";
    } else {
        serial_id = FuriHalSerialIdLpuart; // Pins 15/16
        pin_description = "LPUART (pins 15/16)";
    }

    FURI_LOG_I("EvilBw16", "Attempting to acquire %s", pin_description);

    worker->serial_handle = furi_hal_serial_control_acquire(serial_id);
    if(!worker->serial_handle) {
        FURI_LOG_E("EvilBw16", "Failed to acquire serial handle for %s", pin_description);
        FURI_LOG_E("EvilBw16", "Serial ID requested: %d", (int)serial_id);
        furi_stream_buffer_free(worker->rx_stream);
        furi_mutex_free(worker->tx_mutex);
        free(worker);
        return NULL;
    }

    FURI_LOG_I("EvilBw16", "Successfully acquired serial handle for %s", pin_description);

    // Initialize UART
    furi_hal_serial_init(worker->serial_handle, BAUDRATE);

    // Start async RX
    furi_hal_serial_async_rx_start(worker->serial_handle, uart_on_irq_cb, worker, false);

    // Start worker thread with larger stack for handling high-volume WebUI data
    worker->thread = furi_thread_alloc_ex("EvilBw16UartWorker", 4096, uart_worker_thread, worker);
    furi_thread_start(worker->thread);

    uart_worker = worker;

    FURI_LOG_I(
        "EvilBw16", "Hardware UART initialized on %s at %u baud", pin_description, BAUDRATE);
    FURI_LOG_I("EvilBw16", "UART worker thread started");

    return worker;
}

void evil_bw16_uart_restart(EvilBw16App* app) {
    if(!app) return;

    FURI_LOG_I("EvilBw16", "Restarting UART worker with new GPIO configuration...");

    // Stop current UART worker if it exists
    if(app->uart_worker) {
        FURI_LOG_I("EvilBw16", "Stopping existing UART worker...");
        evil_bw16_uart_free(app->uart_worker);
        app->uart_worker = NULL;
        // Small delay to ensure cleanup is complete
        furi_delay_ms(50);
    }

    // Clear any pending data in the stream buffer
    if(app->uart_rx_stream) {
        furi_stream_buffer_reset(app->uart_rx_stream);
    }

    // Clear logs to show fresh start
    evil_bw16_clear_log(app);

    // Reinitialize UART worker with new configuration
    app->uart_worker = evil_bw16_uart_init(app);

    if(app->uart_worker) {
        FURI_LOG_I("EvilBw16", "UART worker restarted successfully");
        // Add small delay to ensure proper initialization
        furi_delay_ms(100);

        // Send a test command to verify communication
        evil_bw16_uart_send_command(app->uart_worker, "info");
    } else {
        FURI_LOG_E("EvilBw16", "Failed to restart UART worker");
    }
}

void evil_bw16_uart_free(EvilBw16UartWorker* worker) {
    if(!worker) return;

    worker->running = false;

    // Stop async RX
    furi_hal_serial_async_rx_stop(worker->serial_handle);

    // Stop worker thread
    furi_thread_join(worker->thread);
    furi_thread_free(worker->thread);

    // Deinitialize UART
    furi_hal_serial_deinit(worker->serial_handle);
    furi_hal_serial_control_release(worker->serial_handle);

    // Free resources
    furi_stream_buffer_free(worker->rx_stream);
    furi_mutex_free(worker->tx_mutex);
    if(worker->line_buffer) {
        free(worker->line_buffer);
    }
    free(worker);

    uart_worker = NULL;

    FURI_LOG_I("EvilBw16", "UART worker freed");
}

void evil_bw16_uart_tx(EvilBw16UartWorker* worker, const uint8_t* data, size_t len) {
    if(!worker || !data || len == 0) return;

    furi_mutex_acquire(worker->tx_mutex, FuriWaitForever);

    // Send data via hardware UART
    furi_hal_serial_tx(worker->serial_handle, data, len);
    furi_hal_serial_tx_wait_complete(worker->serial_handle);

    furi_mutex_release(worker->tx_mutex);

    FURI_LOG_D("EvilBw16", "TX: %.*s", (int)len, data);
}

void evil_bw16_uart_tx_string(EvilBw16UartWorker* worker, const char* str) {
    if(!worker || !str) return;
    evil_bw16_uart_tx(worker, (const uint8_t*)str, strlen(str));
}

void evil_bw16_uart_send_command(EvilBw16UartWorker* worker, const char* command) {
    if(!worker || !command) return;

    // Store command for echo filtering
    store_sent_command(worker, command);

    // Add newline to command
    char cmd_with_newline[256];
    snprintf(cmd_with_newline, sizeof(cmd_with_newline), "%s\n", command);

    evil_bw16_uart_tx_string(worker, cmd_with_newline);

    FURI_LOG_I("EvilBw16", "Sent command: %s", command);
}

bool evil_bw16_uart_read_line(
    EvilBw16UartWorker* worker,
    char* buffer,
    size_t buffer_size,
    uint32_t timeout_ms) {
    if(!worker || !buffer || buffer_size == 0) return false;

    UNUSED(timeout_ms);

    // Try to read a line from stream buffer
    size_t bytes_read = 0;
    uint32_t start_time = furi_get_tick();

    while(bytes_read < buffer_size - 1 && (furi_get_tick() - start_time) < timeout_ms) {
        uint8_t byte;
        if(furi_stream_buffer_receive(worker->rx_stream, &byte, 1, 10) > 0) {
            buffer[bytes_read++] = byte;
            if(byte == '\n') {
                break;
            }
        }
    }

    buffer[bytes_read] = '\0';
    return bytes_read > 0;
}

size_t evil_bw16_uart_rx_available(EvilBw16UartWorker* worker) {
    if(!worker) return 0;
    return furi_stream_buffer_bytes_available(worker->rx_stream);
}

void evil_bw16_uart_flush_rx(EvilBw16UartWorker* worker) {
    if(!worker) return;
    furi_stream_buffer_reset(worker->rx_stream);
}

bool evil_bw16_uart_wait_for_response(
    EvilBw16UartWorker* worker,
    const char* expected_prefix,
    char* response_buffer,
    size_t buffer_size,
    uint32_t timeout_ms) {
    if(!worker || !expected_prefix || !response_buffer) return false;

    UNUSED(buffer_size);
    UNUSED(timeout_ms);

    // For now, return false - focusing on TX first
    response_buffer[0] = '\0';
    return false;
}

bool is_command_echo(EvilBw16UartWorker* worker, const char* line) {
    if(!worker || !line) return false;

    uint32_t current_time = furi_get_tick();

    // Check if this line matches any recently sent command
    for(int i = 0; i < MAX_RECENT_COMMANDS; i++) {
        if(strlen(worker->recent_commands[i]) > 0) {
            // Check if command timestamp is within echo timeout
            if(current_time - worker->command_timestamps[i] < COMMAND_ECHO_TIMEOUT_MS) {
                // Check if line matches the command (case insensitive)
                if(strcasecmp(line, worker->recent_commands[i]) == 0) {
                    FURI_LOG_D("EvilBw16", "Filtered echo: %s", line);
                    return true;
                }
            } else {
                // Clear old command
                worker->recent_commands[i][0] = '\0';
                worker->command_timestamps[i] = 0;
            }
        }
    }

    return false;
}

void store_sent_command(EvilBw16UartWorker* worker, const char* command) {
    if(!worker || !command) return;

    // Store command in circular buffer
    strncpy(worker->recent_commands[worker->recent_command_index], command, 63);
    worker->recent_commands[worker->recent_command_index][63] = '\0';
    worker->command_timestamps[worker->recent_command_index] = furi_get_tick();

    worker->recent_command_index = (worker->recent_command_index + 1) % MAX_RECENT_COMMANDS;
}

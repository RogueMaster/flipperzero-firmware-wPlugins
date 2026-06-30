#include <flipper_http/flipper_http.h>
FlipperHTTP fhttp;
char rx_line_buffer[RX_LINE_BUFFER_SIZE];
uint8_t file_buffer[FILE_BUFFER_SIZE];
size_t file_buffer_len = 0;
// Function to append received data to file
// make sure to initialize the file path before calling this function
bool flipper_http_append_to_file(
    const void *data,
    size_t data_size,
    bool start_new_file,
    char *file_path)
{
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    if (start_new_file)
    {
        // Delete the file if it already exists
        if (storage_file_exists(storage, file_path))
        {
            if (!storage_simply_remove_recursive(storage, file_path))
            {
                FURI_LOG_E(HTTP_TAG, "Failed to delete file: %s", file_path);
                storage_file_free(file);
                furi_record_close(RECORD_STORAGE);
                return false;
            }
        }
        // Open the file in write mode
        if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to open file for writing: %s", file_path);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }
    }
    else
    {
        // Open the file in append mode
        if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_OPEN_APPEND))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to open file for appending: %s", file_path);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }
    }

    // Write the data to the file
    if (storage_file_write(file, data, data_size) != data_size)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to append data to file");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

FuriString *flipper_http_load_from_file(char *file_path)
{
    // Open the storage record
    Storage *storage = furi_record_open(RECORD_STORAGE);
    if (!storage)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to open storage record");
        return NULL;
    }

    // Allocate a file handle
    File *file = storage_file_alloc(storage);
    if (!file)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate storage file");
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Open the file for reading
    if (!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL; // Return false if the file does not exist
    }

    // Allocate a FuriString to hold the received data
    FuriString *str_result = furi_string_alloc();
    if (!str_result)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate FuriString");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Reset the FuriString to ensure it's empty before reading
    furi_string_reset(str_result);

    // Define a buffer to hold the read data
    uint8_t *buffer = (uint8_t *)malloc(MAX_FILE_SHOW);
    if (!buffer)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate buffer");
        furi_string_free(str_result);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Read data into the buffer
    size_t read_count = storage_file_read(file, buffer, MAX_FILE_SHOW);
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E(HTTP_TAG, "Error reading from file.");
        furi_string_free(str_result);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Append each byte to the FuriString
    for (size_t i = 0; i < read_count; i++)
    {
        furi_string_push_back(str_result, buffer[i]);
    }

    // Check if there is more data beyond the maximum size
    char extra_byte;
    storage_file_read(file, &extra_byte, 1);

    // Clean up
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    free(buffer);
    return str_result;
}

// UART worker thread
/**
 * @brief      Worker thread to handle UART data asynchronously.
 * @return     0
 * @param      context   The context to pass to the callback.
 * @note       This function will handle received data asynchronously via the callback.
 */
// UART worker thread
int32_t flipper_http_worker(void *context)
{
    UNUSED(context);
    size_t rx_line_pos = 0;

    while (1)
    {
        uint32_t events = furi_thread_flags_wait(
            WorkerEvtStop | WorkerEvtRxDone, FuriFlagWaitAny, FuriWaitForever);
        if (events & WorkerEvtStop)
        {
            break;
        }
        if (events & WorkerEvtRxDone)
        {
            // Continuously read from the stream buffer until it's empty
            while (!furi_stream_buffer_is_empty(fhttp.flipper_http_stream))
            {
                // Read one byte at a time
                char c = 0;
                size_t received = furi_stream_buffer_receive(fhttp.flipper_http_stream, &c, 1, 0);

                if (received == 0)
                {
                    // No more data to read
                    break;
                }

                // Append the received byte to the file if saving is enabled
                if (fhttp.save_bytes)
                {
                    // Add byte to the buffer
                    file_buffer[file_buffer_len++] = c;
                    // Write to file if buffer is full
                    if (file_buffer_len >= FILE_BUFFER_SIZE)
                    {
                        if (!flipper_http_append_to_file(
                                file_buffer,
                                file_buffer_len,
                                fhttp.just_started_bytes,
                                fhttp.file_path))
                        {
                            FURI_LOG_E(HTTP_TAG, "Failed to append data to file");
                        }
                        file_buffer_len = 0;
                        fhttp.just_started_bytes = false;
                    }
                }

                // Handle line buffering only if callback is set (text data)
                if (fhttp.handle_rx_line_cb)
                {
                    // Handle line buffering
                    if (c == '\n' || rx_line_pos >= RX_LINE_BUFFER_SIZE - 1)
                    {
                        rx_line_buffer[rx_line_pos] = '\0'; // Null-terminate the line

                        // Invoke the callback with the complete line
                        fhttp.handle_rx_line_cb(rx_line_buffer, fhttp.callback_context);

                        // Reset the line buffer position
                        rx_line_pos = 0;
                    }
                    else
                    {
                        rx_line_buffer[rx_line_pos++] = c; // Add character to the line buffer
                    }
                }
            }
        }
    }

    return 0;
}
// Timer callback function
/**
 * @brief      Callback function for the GET timeout timer.
 * @return     0
 * @param      context   The context to pass to the callback.
 * @note       This function will be called when the GET request times out.
 */
void get_timeout_timer_callback(void *context)
{
    UNUSED(context);
    FURI_LOG_E(HTTP_TAG, "Timeout reached: 2 seconds without receiving the end.");

    // Reset the state
    fhttp.started_receiving_get = false;
    fhttp.started_receiving_post = false;
    fhttp.started_receiving_put = false;
    fhttp.started_receiving_delete = false;

    // Update UART state
    fhttp.state = ISSUE;
}

// UART RX Handler Callback (Interrupt Context)
/**
 * @brief      A private callback function to handle received data asynchronously.
 * @return     void
 * @param      handle    The UART handle.
 * @param      event     The event type.
 * @param      context   The context to pass to the callback.
 * @note       This function will handle received data asynchronously via the callback.
 */
void _flipper_http_rx_callback(
    FuriHalSerialHandle *handle,
    FuriHalSerialRxEvent event,
    void *context)
{
    UNUSED(context);
    if (event == FuriHalSerialRxEventData)
    {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(fhttp.flipper_http_stream, &data, 1, 0);
        furi_thread_flags_set(fhttp.rx_thread_id, WorkerEvtRxDone);
    }
}

// UART initialization function
/**
 * @brief      Initialize UART.
 * @return     true if the UART was initialized successfully, false otherwise.
 * @param      callback  The callback function to handle received data (ex. flipper_http_rx_callback).
 * @param      context   The context to pass to the callback.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_init(FlipperHTTP_Callback callback, void *context)
{
    if (!context)
    {
        FURI_LOG_E(HTTP_TAG, "Invalid context provided to flipper_http_init.");
        return false;
    }
    if (!callback)
    {
        FURI_LOG_E(HTTP_TAG, "Invalid callback provided to flipper_http_init.");
        return false;
    }
    fhttp.flipper_http_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    if (!fhttp.flipper_http_stream)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate UART stream buffer.");
        return false;
    }

    fhttp.rx_thread = furi_thread_alloc();
    if (!fhttp.rx_thread)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate UART thread.");
        furi_stream_buffer_free(fhttp.flipper_http_stream);
        return false;
    }

    furi_thread_set_name(fhttp.rx_thread, "FlipperHTTP_RxThread");
    furi_thread_set_stack_size(fhttp.rx_thread, 1024);
    furi_thread_set_context(fhttp.rx_thread, &fhttp);
    furi_thread_set_callback(fhttp.rx_thread, flipper_http_worker);

    fhttp.handle_rx_line_cb = callback;
    fhttp.callback_context = context;

    furi_thread_start(fhttp.rx_thread);
    fhttp.rx_thread_id = furi_thread_get_id(fhttp.rx_thread);

    // handle when the UART control is busy to avoid furi_check failed
    if (furi_hal_serial_control_is_busy(UART_CH))
    {
        FURI_LOG_E(HTTP_TAG, "UART control is busy.");
        return false;
    }

    fhttp.serial_handle = furi_hal_serial_control_acquire(UART_CH);
    if (!fhttp.serial_handle)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to acquire UART control - handle is NULL");
        // Cleanup resources
        furi_thread_free(fhttp.rx_thread);
        furi_stream_buffer_free(fhttp.flipper_http_stream);
        return false;
    }

    // Initialize UART with acquired handle
    furi_hal_serial_init(fhttp.serial_handle, BAUDRATE);

    // Enable RX direction
    furi_hal_serial_enable_direction(fhttp.serial_handle, FuriHalSerialDirectionRx);

    // Start asynchronous RX with the callback
    furi_hal_serial_async_rx_start(fhttp.serial_handle, _flipper_http_rx_callback, &fhttp, false);

    // Wait for the TX to complete to ensure UART is ready
    furi_hal_serial_tx_wait_complete(fhttp.serial_handle);

    // Allocate the timer for handling timeouts
    fhttp.get_timeout_timer = furi_timer_alloc(
        get_timeout_timer_callback, // Callback function
        FuriTimerTypeOnce,          // One-shot timer
        &fhttp                      // Context passed to callback
    );

    if (!fhttp.get_timeout_timer)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate HTTP request timeout timer.");
        // Cleanup resources
        furi_hal_serial_async_rx_stop(fhttp.serial_handle);
        furi_hal_serial_disable_direction(fhttp.serial_handle, FuriHalSerialDirectionRx);
        furi_hal_serial_control_release(fhttp.serial_handle);
        furi_hal_serial_deinit(fhttp.serial_handle);
        furi_thread_flags_set(fhttp.rx_thread_id, WorkerEvtStop);
        furi_thread_join(fhttp.rx_thread);
        furi_thread_free(fhttp.rx_thread);
        furi_stream_buffer_free(fhttp.flipper_http_stream);
        return false;
    }

    // Set the timer thread priority if needed
    furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);

    fhttp.last_response = (char *)malloc(RX_BUF_SIZE);
    if (!fhttp.last_response)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate memory for last_response.");
        return false;
    }

    // FURI_LOG_I(HTTP_TAG, "UART initialized successfully.");
    return true;
}

// Deinitialize UART
/**
 * @brief      Deinitialize UART.
 * @return     void
 * @note       This function will stop the asynchronous RX, release the serial handle, and free the resources.
 */
void flipper_http_deinit()
{
    if (fhttp.serial_handle == NULL)
    {
        FURI_LOG_E(HTTP_TAG, "UART handle is NULL. Already deinitialized?");
        return;
    }
    // Stop asynchronous RX
    furi_hal_serial_async_rx_stop(fhttp.serial_handle);

    // Release and deinitialize the serial handle
    furi_hal_serial_disable_direction(fhttp.serial_handle, FuriHalSerialDirectionRx);
    furi_hal_serial_control_release(fhttp.serial_handle);
    furi_hal_serial_deinit(fhttp.serial_handle);

    // Signal the worker thread to stop
    furi_thread_flags_set(fhttp.rx_thread_id, WorkerEvtStop);
    // Wait for the thread to finish
    furi_thread_join(fhttp.rx_thread);
    // Free the thread resources
    furi_thread_free(fhttp.rx_thread);

    // Free the stream buffer
    furi_stream_buffer_free(fhttp.flipper_http_stream);

    // Free the timer
    if (fhttp.get_timeout_timer)
    {
        furi_timer_free(fhttp.get_timeout_timer);
        fhttp.get_timeout_timer = NULL;
    }

    // Free the last response
    if (fhttp.last_response)
    {
        free(fhttp.last_response);
        fhttp.last_response = NULL;
    }

    // FURI_LOG_I("FlipperHTTP", "UART deinitialized successfully.");
}

// Function to send data over UART with newline termination
/**
 * @brief      Send data over UART with newline termination.
 * @return     true if the data was sent successfully, false otherwise.
 * @param      data  The data to send over UART.
 * @note       The data will be sent over UART with a newline character appended.
 */
bool flipper_http_send_data(const char *data)
{
    size_t data_length = strlen(data);
    if (data_length == 0)
    {
        FURI_LOG_E("FlipperHTTP", "Attempted to send empty data.");
        fhttp.state = ISSUE;
        return false;
    }

    // Create a buffer with data + '\n'
    size_t send_length = data_length + 1; // +1 for '\n'
    if (send_length > 512)
    { // Ensure buffer size is sufficient
        FURI_LOG_E("FlipperHTTP", "Data too long to send over FHTTP.");
        return false;
    }

    char send_buffer[513]; // 512 + 1 for safety
    strncpy(send_buffer, data, 512);
    send_buffer[data_length] = '\n';     // Append newline
    send_buffer[data_length + 1] = '\0'; // Null-terminate

    if (fhttp.state == INACTIVE && ((strstr(send_buffer, "[PING]") == NULL) &&
                                    (strstr(send_buffer, "[WIFI/CONNECT]") == NULL)))
    {
        FURI_LOG_E("FlipperHTTP", "Cannot send data while INACTIVE.");
        fhttp.last_response = "Cannot send data while INACTIVE.";
        return false;
    }

    fhttp.state = SENDING;
    furi_hal_serial_tx(fhttp.serial_handle, (const uint8_t *)send_buffer, send_length);

    // Uncomment below line to log the data sent over UART
    // FURI_LOG_I("FlipperHTTP", "Sent data over UART: %s", send_buffer);
    // fhttp.state = IDLE;
    return true;
}

// Function to send a PING request
/**
 * @brief      Send a PING request to check if the Wifi Dev Board is connected.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 * @note       This is best used to check if the Wifi Dev Board is connected.
 * @note       The state will remain INACTIVE until a PONG is received.
 */
bool flipper_http_ping()
{
    const char *command = "[PING]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send PING command.");
        return false;
    }
    // set state as INACTIVE to be made IDLE if PONG is received
    fhttp.state = INACTIVE;
    // The response will be handled asynchronously via the callback
    return true;
}

// Function to list available commands
/**
 * @brief      Send a command to list available commands.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_list_commands()
{
    const char *command = "[LIST]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send LIST command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to turn on the LED
/**
 * @brief      Allow the LED to display while processing.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_led_on()
{
    const char *command = "[LED/ON]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send LED ON command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to turn off the LED
/**
 * @brief      Disable the LED from displaying while processing.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_led_off()
{
    const char *command = "[LED/OFF]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send LED OFF command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to parse JSON data
/**
 * @brief      Parse JSON data.
 * @return     true if the JSON data was parsed successfully, false otherwise.
 * @param      key       The key to parse from the JSON data.
 * @param      json_data The JSON data to parse.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_parse_json(const char *key, const char *json_data)
{
    if (!key || !json_data)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_parse_json.");
        return false;
    }

    char buffer[256];
    int ret =
        snprintf(buffer, sizeof(buffer), "[PARSE]{\"key\":\"%s\",\"json\":%s}", key, json_data);
    if (ret < 0 || ret >= (int)sizeof(buffer))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format JSON parse command.");
        return false;
    }

    if (!flipper_http_send_data(buffer))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send JSON parse command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to parse JSON array data
/**
 * @brief      Parse JSON array data.
 * @return     true if the JSON array data was parsed successfully, false otherwise.
 * @param      key       The key to parse from the JSON array data.
 * @param      index     The index to parse from the JSON array data.
 * @param      json_data The JSON array data to parse.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_parse_json_array(const char *key, int index, const char *json_data)
{
    if (!key || !json_data)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_parse_json_array.");
        return false;
    }

    char buffer[256];
    int ret = snprintf(
        buffer,
        sizeof(buffer),
        "[PARSE/ARRAY]{\"key\":\"%s\",\"index\":%d,\"json\":%s}",
        key,
        index,
        json_data);
    if (ret < 0 || ret >= (int)sizeof(buffer))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format JSON parse array command.");
        return false;
    }

    if (!flipper_http_send_data(buffer))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send JSON parse array command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to scan for WiFi networks
/**
 * @brief      Send a command to scan for WiFi networks.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_scan_wifi()
{
    const char *command = "[WIFI/SCAN]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send WiFi scan command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to save WiFi settings (returns true if successful)
/**
 * @brief      Send a command to save WiFi settings.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_save_wifi(const char *ssid, const char *password)
{
    if (!ssid || !password)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_save_wifi.");
        return false;
    }
    char buffer[256];
    int ret = snprintf(
        buffer, sizeof(buffer), "[WIFI/SAVE]{\"ssid\":\"%s\",\"password\":\"%s\"}", ssid, password);
    if (ret < 0 || ret >= (int)sizeof(buffer))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format WiFi save command.");
        return false;
    }

    if (!flipper_http_send_data(buffer))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send WiFi save command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to get IP address of WiFi Devboard
/**
 * @brief      Send a command to get the IP address of the WiFi Devboard
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_ip_address()
{
    const char *command = "[IP/ADDRESS]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send IP address command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to get IP address of the connected WiFi network
/**
 * @brief      Send a command to get the IP address of the connected WiFi network.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_ip_wifi()
{
    const char *command = "[WIFI/IP]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send WiFi IP command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to disconnect from WiFi (returns true if successful)
/**
 * @brief      Send a command to disconnect from WiFi.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_disconnect_wifi()
{
    const char *command = "[WIFI/DISCONNECT]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send WiFi disconnect command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to connect to WiFi (returns true if successful)
/**
 * @brief      Send a command to connect to WiFi.
 * @return     true if the request was successful, false otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_connect_wifi()
{
    const char *command = "[WIFI/CONNECT]";
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send WiFi connect command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}

// Function to send a GET request
/**
 * @brief      Send a GET request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param      url  The URL to send the GET request to.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_get_request(const char *url)
{
    if (!url)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_get_request.");
        return false;
    }

    // Prepare GET request command
    char command[512];
    int ret = snprintf(command, sizeof(command), "[GET]%s", url);
    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format GET request command.");
        return false;
    }

    // Send GET request via UART
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send GET request command.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}
// Function to send a GET request with headers
/**
 * @brief      Send a GET request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param      url  The URL to send the GET request to.
 * @param      headers  The headers to send with the GET request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_get_request_with_headers(const char *url, const char *headers)
{
    if (!url || !headers)
    {
        FURI_LOG_E(
            "FlipperHTTP", "Invalid arguments provided to flipper_http_get_request_with_headers.");
        return false;
    }

    // Prepare GET request command with headers
    char command[512];
    int ret = snprintf(
        command, sizeof(command), "[GET/HTTP]{\"url\":\"%s\",\"headers\":%s}", url, headers);
    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format GET request command with headers.");
        return false;
    }

    // Send GET request via UART
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send GET request command with headers.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}
// Function to send a GET request with headers and return bytes
/**
 * @brief      Send a GET request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param      url  The URL to send the GET request to.
 * @param      headers  The headers to send with the GET request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_get_request_bytes(const char *url, const char *headers)
{
    if (!url || !headers)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_get_request_bytes.");
        return false;
    }

    // Prepare GET request command with headers
    char command[256];
    int ret = snprintf(
        command, sizeof(command), "[GET/BYTES]{\"url\":\"%s\",\"headers\":%s}", url, headers);
    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format GET request command with headers.");
        return false;
    }

    // Send GET request via UART
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send GET request command with headers.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}
// Function to send a POST request with headers
/**
 * @brief      Send a POST request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param      url  The URL to send the POST request to.
 * @param      headers  The headers to send with the POST request.
 * @param      data  The data to send with the POST request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_post_request_with_headers(
    const char *url,
    const char *headers,
    const char *payload)
{
    if (!url || !headers || !payload)
    {
        FURI_LOG_E(
            "FlipperHTTP",
            "Invalid arguments provided to flipper_http_post_request_with_headers.");
        return false;
    }

    // Prepare POST request command with headers and data
    char command[256];
    int ret = snprintf(
        command,
        sizeof(command),
        "[POST/HTTP]{\"url\":\"%s\",\"headers\":%s,\"payload\":%s}",
        url,
        headers,
        payload);
    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format POST request command with headers and data.");
        return false;
    }

    // Send POST request via UART
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send POST request command with headers and data.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}
// Function to send a POST request with headers and return bytes
/**
 * @brief      Send a POST request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param      url  The URL to send the POST request to.
 * @param      headers  The headers to send with the POST request.
 * @param      payload  The data to send with the POST request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_post_request_bytes(const char *url, const char *headers, const char *payload)
{
    if (!url || !headers || !payload)
    {
        FURI_LOG_E(
            "FlipperHTTP", "Invalid arguments provided to flipper_http_post_request_bytes.");
        return false;
    }

    // Prepare POST request command with headers and data
    char command[256];
    int ret = snprintf(
        command,
        sizeof(command),
        "[POST/BYTES]{\"url\":\"%s\",\"headers\":%s,\"payload\":%s}",
        url,
        headers,
        payload);
    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format POST request command with headers and data.");
        return false;
    }

    // Send POST request via UART
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send POST request command with headers and data.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}
// Function to send a PUT request with headers
/**
 * @brief      Send a PUT request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param      url  The URL to send the PUT request to.
 * @param      headers  The headers to send with the PUT request.
 * @param      data  The data to send with the PUT request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_put_request_with_headers(
    const char *url,
    const char *headers,
    const char *payload)
{
    if (!url || !headers || !payload)
    {
        FURI_LOG_E(
            "FlipperHTTP", "Invalid arguments provided to flipper_http_put_request_with_headers.");
        return false;
    }

    // Prepare PUT request command with headers and data
    char command[256];
    int ret = snprintf(
        command,
        sizeof(command),
        "[PUT/HTTP]{\"url\":\"%s\",\"headers\":%s,\"payload\":%s}",
        url,
        headers,
        payload);
    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format PUT request command with headers and data.");
        return false;
    }

    // Send PUT request via UART
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send PUT request command with headers and data.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}
// Function to send a DELETE request with headers
/**
 * @brief      Send a DELETE request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param      url  The URL to send the DELETE request to.
 * @param      headers  The headers to send with the DELETE request.
 * @param      data  The data to send with the DELETE request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_delete_request_with_headers(
    const char *url,
    const char *headers,
    const char *payload)
{
    if (!url || !headers || !payload)
    {
        FURI_LOG_E(
            "FlipperHTTP",
            "Invalid arguments provided to flipper_http_delete_request_with_headers.");
        return false;
    }

    // Prepare DELETE request command with headers and data
    char command[256];
    int ret = snprintf(
        command,
        sizeof(command),
        "[DELETE/HTTP]{\"url\":\"%s\",\"headers\":%s,\"payload\":%s}",
        url,
        headers,
        payload);
    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E(
            "FlipperHTTP", "Failed to format DELETE request command with headers and data.");
        return false;
    }

    // Send DELETE request via UART
    if (!flipper_http_send_data(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to send DELETE request command with headers and data.");
        return false;
    }

    // The response will be handled asynchronously via the callback
    return true;
}
// Function to handle received data asynchronously
/**
 * @brief      Callback function to handle received data asynchronously.
 * @return     void
 * @param      line     The received line.
 * @param      context  The context passed to the callback.
 * @note       The received data will be handled asynchronously via the callback and handles the state of the UART.
 */
void flipper_http_rx_callback(const char *line, void *context)
{
    if (!line || !context)
    {
        FURI_LOG_E(HTTP_TAG, "Invalid arguments provided to flipper_http_rx_callback.");
        return;
    }

    // Trim the received line to check if it's empty
    char *trimmed_line = trim(line);
    if (trimmed_line != NULL && trimmed_line[0] != '\0')
    {
        // if the line is not [GET/END] or [POST/END] or [PUT/END] or [DELETE/END]
        if (strstr(trimmed_line, "[GET/END]") == NULL &&
            strstr(trimmed_line, "[POST/END]") == NULL &&
            strstr(trimmed_line, "[PUT/END]") == NULL &&
            strstr(trimmed_line, "[DELETE/END]") == NULL)
        {
            strncpy(fhttp.last_response, trimmed_line, RX_BUF_SIZE);
        }
    }
    free(trimmed_line); // Free the allocated memory for trimmed_line

    if (fhttp.state != INACTIVE && fhttp.state != ISSUE)
    {
        fhttp.state = RECEIVING;
    }

    // Uncomment below line to log the data received over UART
    // FURI_LOG_I(HTTP_TAG, "Received UART line: %s", line);

    // Check if we've started receiving data from a GET request
    if (fhttp.started_receiving_get)
    {
        // Restart the timeout timer each time new data is received
        furi_timer_restart(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);

        if (strstr(line, "[GET/END]") != NULL)
        {
            FURI_LOG_I(HTTP_TAG, "GET request completed.");
            // Stop the timer since we've completed the GET request
            furi_timer_stop(fhttp.get_timeout_timer);
            fhttp.started_receiving_get = false;
            fhttp.just_started_get = false;
            fhttp.state = IDLE;
            fhttp.save_bytes = false;
            fhttp.save_received_data = false;

            if (fhttp.is_bytes_request)
            {
                // Search for the binary marker `[GET/END]` in the file buffer
                const char marker[] = "[GET/END]";
                const size_t marker_len = sizeof(marker) - 1; // Exclude null terminator

                for (size_t i = 0; i <= file_buffer_len - marker_len; i++)
                {
                    // Check if the marker is found
                    if (memcmp(&file_buffer[i], marker, marker_len) == 0)
                    {
                        // Remove the marker by shifting the remaining data left
                        size_t remaining_len = file_buffer_len - (i + marker_len);
                        memmove(&file_buffer[i], &file_buffer[i + marker_len], remaining_len);
                        file_buffer_len -= marker_len;
                        break;
                    }
                }

                // If there is data left in the buffer, append it to the file
                if (file_buffer_len > 0)
                {
                    if (!flipper_http_append_to_file(file_buffer, file_buffer_len, false, fhttp.file_path))
                    {
                        FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
                    }
                    file_buffer_len = 0;
                }
            }

            fhttp.is_bytes_request = false;
            return;
        }

        // Append the new line to the existing data
        if (fhttp.save_received_data &&
            !flipper_http_append_to_file(
                line, strlen(line), !fhttp.just_started_get, fhttp.file_path))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
            fhttp.started_receiving_get = false;
            fhttp.just_started_get = false;
            fhttp.state = IDLE;
            return;
        }

        if (!fhttp.just_started_get)
        {
            fhttp.just_started_get = true;
        }
        return;
    }

    // Check if we've started receiving data from a POST request
    else if (fhttp.started_receiving_post)
    {
        // Restart the timeout timer each time new data is received
        furi_timer_restart(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);

        if (strstr(line, "[POST/END]") != NULL)
        {
            FURI_LOG_I(HTTP_TAG, "POST request completed.");
            // Stop the timer since we've completed the POST request
            furi_timer_stop(fhttp.get_timeout_timer);
            fhttp.started_receiving_post = false;
            fhttp.just_started_post = false;
            fhttp.state = IDLE;
            fhttp.save_bytes = false;
            fhttp.save_received_data = false;

            if (fhttp.is_bytes_request)
            {
                // Search for the binary marker `[POST/END]` in the file buffer
                const char marker[] = "[POST/END]";
                const size_t marker_len = sizeof(marker) - 1; // Exclude null terminator

                for (size_t i = 0; i <= file_buffer_len - marker_len; i++)
                {
                    // Check if the marker is found
                    if (memcmp(&file_buffer[i], marker, marker_len) == 0)
                    {
                        // Remove the marker by shifting the remaining data left
                        size_t remaining_len = file_buffer_len - (i + marker_len);
                        memmove(&file_buffer[i], &file_buffer[i + marker_len], remaining_len);
                        file_buffer_len -= marker_len;
                        break;
                    }
                }

                // If there is data left in the buffer, append it to the file
                if (file_buffer_len > 0)
                {
                    if (!flipper_http_append_to_file(file_buffer, file_buffer_len, false, fhttp.file_path))
                    {
                        FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
                    }
                    file_buffer_len = 0;
                }
            }

            fhttp.is_bytes_request = false;
            return;
        }

        // Append the new line to the existing data
        if (fhttp.save_received_data &&
            !flipper_http_append_to_file(
                line, strlen(line), !fhttp.just_started_post, fhttp.file_path))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
            fhttp.started_receiving_post = false;
            fhttp.just_started_post = false;
            fhttp.state = IDLE;
            return;
        }

        if (!fhttp.just_started_post)
        {
            fhttp.just_started_post = true;
        }
        return;
    }

    // Check if we've started receiving data from a PUT request
    else if (fhttp.started_receiving_put)
    {
        // Restart the timeout timer each time new data is received
        furi_timer_restart(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);

        if (strstr(line, "[PUT/END]") != NULL)
        {
            FURI_LOG_I(HTTP_TAG, "PUT request completed.");
            // Stop the timer since we've completed the PUT request
            furi_timer_stop(fhttp.get_timeout_timer);
            fhttp.started_receiving_put = false;
            fhttp.just_started_put = false;
            fhttp.state = IDLE;
            fhttp.save_bytes = false;
            fhttp.is_bytes_request = false;
            fhttp.save_received_data = false;
            return;
        }

        // Append the new line to the existing data
        if (fhttp.save_received_data &&
            !flipper_http_append_to_file(
                line, strlen(line), !fhttp.just_started_put, fhttp.file_path))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
            fhttp.started_receiving_put = false;
            fhttp.just_started_put = false;
            fhttp.state = IDLE;
            return;
        }

        if (!fhttp.just_started_put)
        {
            fhttp.just_started_put = true;
        }
        return;
    }

    // Check if we've started receiving data from a DELETE request
    else if (fhttp.started_receiving_delete)
    {
        // Restart the timeout timer each time new data is received
        furi_timer_restart(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);

        if (strstr(line, "[DELETE/END]") != NULL)
        {
            FURI_LOG_I(HTTP_TAG, "DELETE request completed.");
            // Stop the timer since we've completed the DELETE request
            furi_timer_stop(fhttp.get_timeout_timer);
            fhttp.started_receiving_delete = false;
            fhttp.just_started_delete = false;
            fhttp.state = IDLE;
            fhttp.save_bytes = false;
            fhttp.is_bytes_request = false;
            fhttp.save_received_data = false;
            return;
        }

        // Append the new line to the existing data
        if (fhttp.save_received_data &&
            !flipper_http_append_to_file(
                line, strlen(line), !fhttp.just_started_delete, fhttp.file_path))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
            fhttp.started_receiving_delete = false;
            fhttp.just_started_delete = false;
            fhttp.state = IDLE;
            return;
        }

        if (!fhttp.just_started_delete)
        {
            fhttp.just_started_delete = true;
        }
        return;
    }

    // Handle different types of responses
    if (strstr(line, "[SUCCESS]") != NULL || strstr(line, "[CONNECTED]") != NULL)
    {
        FURI_LOG_I(HTTP_TAG, "Operation succeeded.");
    }
    else if (strstr(line, "[INFO]") != NULL)
    {
        FURI_LOG_I(HTTP_TAG, "Received info: %s", line);

        if (fhttp.state == INACTIVE && strstr(line, "[INFO] Already connected to Wifi.") != NULL)
        {
            fhttp.state = IDLE;
        }
    }
    else if (strstr(line, "[GET/SUCCESS]") != NULL)
    {
        FURI_LOG_I(HTTP_TAG, "GET request succeeded.");
        fhttp.started_receiving_get = true;
        furi_timer_start(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);
        fhttp.state = RECEIVING;
        // for GET request, save data only if it's a bytes request
        fhttp.save_bytes = fhttp.is_bytes_request;
        fhttp.just_started_bytes = true;
        file_buffer_len = 0;
        return;
    }
    else if (strstr(line, "[POST/SUCCESS]") != NULL)
    {
        FURI_LOG_I(HTTP_TAG, "POST request succeeded.");
        fhttp.started_receiving_post = true;
        furi_timer_start(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);
        fhttp.state = RECEIVING;
        // for POST request, save data only if it's a bytes request
        fhttp.save_bytes = fhttp.is_bytes_request;
        fhttp.just_started_bytes = true;
        file_buffer_len = 0;
        return;
    }
    else if (strstr(line, "[PUT/SUCCESS]") != NULL)
    {
        FURI_LOG_I(HTTP_TAG, "PUT request succeeded.");
        fhttp.started_receiving_put = true;
        furi_timer_start(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);
        fhttp.state = RECEIVING;
        return;
    }
    else if (strstr(line, "[DELETE/SUCCESS]") != NULL)
    {
        FURI_LOG_I(HTTP_TAG, "DELETE request succeeded.");
        fhttp.started_receiving_delete = true;
        furi_timer_start(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);
        fhttp.state = RECEIVING;
        return;
    }
    else if (strstr(line, "[DISCONNECTED]") != NULL)
    {
        FURI_LOG_I(HTTP_TAG, "WiFi disconnected successfully.");
    }
    else if (strstr(line, "[ERROR]") != NULL)
    {
        FURI_LOG_E(HTTP_TAG, "Received error: %s", line);
        fhttp.state = ISSUE;
        return;
    }
    else if (strstr(line, "[PONG]") != NULL)
    {
        FURI_LOG_I(HTTP_TAG, "Received PONG response: Wifi Dev Board is still alive.");

        // send command to connect to WiFi
        if (fhttp.state == INACTIVE)
        {
            fhttp.state = IDLE;
            return;
        }
    }

    if (fhttp.state == INACTIVE && strstr(line, "[PONG]") != NULL)
    {
        fhttp.state = IDLE;
    }
    else if (fhttp.state == INACTIVE && strstr(line, "[PONG]") == NULL)
    {
        fhttp.state = INACTIVE;
    }
    else
    {
        fhttp.state = IDLE;
    }
}

// Function to trim leading and trailing spaces and newlines from a constant string
char *trim(const char *str)
{
    const char *end;
    char *trimmed_str;
    size_t len;

    // Trim leading space
    while (isspace((unsigned char)*str))
        str++;

    // All spaces?
    if (*str == 0)
        return strdup(""); // Return an empty string if all spaces

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    // Set length for the trimmed string
    len = end - str + 1;

    // Allocate space for the trimmed string and null terminator
    trimmed_str = (char *)malloc(len + 1);
    if (trimmed_str == NULL)
    {
        return NULL; // Handle memory allocation failure
    }

    // Copy the trimmed part of the string into trimmed_str
    strncpy(trimmed_str, str, len);
    trimmed_str[len] = '\0'; // Null terminate the string

    return trimmed_str;
}

/**
 * @brief Process requests and parse JSON data asynchronously
 * @param http_request The function to send the request
 * @param parse_json The function to parse the JSON
 * @return true if successful, false otherwise
 */
bool flipper_http_process_response_async(bool (*http_request)(void), bool (*parse_json)(void))
{
    if (http_request()) // start the async request
    {
        furi_timer_start(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);
        fhttp.state = RECEIVING;
    }
    else
    {
        FURI_LOG_E(HTTP_TAG, "Failed to send request");
        return false;
    }
    while (fhttp.state == RECEIVING && furi_timer_is_running(fhttp.get_timeout_timer) > 0)
    {
        // Wait for the request to be received
        furi_delay_ms(100);
    }
    furi_timer_stop(fhttp.get_timeout_timer);
    if (!parse_json()) // parse the JSON before switching to the view (synchonous)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to parse the JSON...");
        return false;
    }
    return true;
}

/**
 * @brief Perform a task while displaying a loading screen
 * @param http_request The function to send the request
 * @param parse_response The function to parse the response
 * @param success_view_id The view ID to switch to on success
 * @param failure_view_id The view ID to switch to on failure
 * @param view_dispatcher The view dispatcher to use
 * @return
 */
void flipper_http_loading_task(bool (*http_request)(void),
                               bool (*parse_response)(void),
                               uint32_t success_view_id,
                               uint32_t failure_view_id,
                               ViewDispatcher **view_dispatcher)
{
    Loading *loading;
    int32_t loading_view_id = 987654321; // Random ID

    loading = loading_alloc();
    if (!loading)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate loading");
        view_dispatcher_switch_to_view(*view_dispatcher, failure_view_id);

        return;
    }

    view_dispatcher_add_view(*view_dispatcher, loading_view_id, loading_get_view(loading));

    // Switch to the loading view
    view_dispatcher_switch_to_view(*view_dispatcher, loading_view_id);

    // Make the request
    if (!flipper_http_process_response_async(http_request, parse_response))
    {
        FURI_LOG_E(HTTP_TAG, "Failed to make request");
        view_dispatcher_switch_to_view(*view_dispatcher, failure_view_id);
        view_dispatcher_remove_view(*view_dispatcher, loading_view_id);
        loading_free(loading);

        return;
    }

    // Switch to the success view
    view_dispatcher_switch_to_view(*view_dispatcher, success_view_id);
    view_dispatcher_remove_view(*view_dispatcher, loading_view_id);
    loading_free(loading);
}

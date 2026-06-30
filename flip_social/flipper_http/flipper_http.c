// Description: Flipper HTTP API (For use with Flipper Zero and the FlipperHTTP flash: https://github.com/jblanked/FlipperHTTP)
// License: MIT
// Author: JBlanked
// File: flipper_http.c
#include <flipper_http/flipper_http.h>

/**
 * @brief      Worker thread to handle UART data asynchronously.
 * @return     0
 * @param      context   The FlipperHTTP context.
 * @note       This function will handle received data asynchronously via the callback.
 */
static int32_t flipper_http_worker(void *context)
{
    if (!context)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return -1;
    }
    FlipperHTTP *fhttp = (FlipperHTTP *)context;
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return -1;
    }
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
            while (!furi_stream_buffer_is_empty(fhttp->flipper_http_stream))
            {
                // Read one byte at a time
                char c = 0;
                size_t received = furi_stream_buffer_receive(fhttp->flipper_http_stream, &c, 1, 0);

                if (received == 0)
                {
                    // No more data to read
                    break;
                }

                fhttp->bytes_received += received;

                // print amount of bytes received
                // FURI_LOG_I(HTTP_TAG, "Bytes received: %d", fhttp->bytes_received);

                // Append the received byte to the file if saving is enabled
                if (fhttp->save_bytes)
                {
                    // Add byte to the buffer
                    fhttp->file_buffer[fhttp->file_buffer_len++] = c;
                    // Write to file if buffer is full
                    if (fhttp->file_buffer_len >= FILE_BUFFER_SIZE)
                    {
                        if (!flipper_http_append_to_file(
                                fhttp->file_buffer,
                                fhttp->file_buffer_len,
                                fhttp->just_started_bytes,
                                fhttp->file_path))
                        {
                            FURI_LOG_E(HTTP_TAG, "Failed to append data to file");
                        }
                        fhttp->file_buffer_len = 0;
                        fhttp->just_started_bytes = false;
                    }
                }

                // Handle line buffering only if callback is set (text data)
                if (fhttp->handle_rx_line_cb)
                {
                    // Handle line buffering
                    if (c == '\n' || rx_line_pos >= RX_LINE_BUFFER_SIZE - 1)
                    {
                        fhttp->rx_line_buffer[rx_line_pos] = '\0'; // Null-terminate the line

                        // Invoke the callback with the complete line
                        fhttp->handle_rx_line_cb(fhttp->rx_line_buffer, fhttp->callback_context);

                        // Reset the line buffer position
                        rx_line_pos = 0;
                    }
                    else
                    {
                        fhttp->rx_line_buffer[rx_line_pos++] = c; // Add character to the line buffer
                    }
                }
            }
        }
    }

    return 0;
}

// UART RX Handler Callback (Interrupt Context)
/**
 * @brief      A private callback function to handle received data asynchronously.
 * @return     void
 * @param      handle    The UART handle.
 * @param      event     The event type.
 * @param      context   The FlipperHTTP context.
 * @note       This function will handle received data asynchronously via the callback.
 */
static void _flipper_http_rx_callback(
    FuriHalSerialHandle *handle,
    FuriHalSerialRxEvent event,
    void *context)
{
    FlipperHTTP *fhttp = (FlipperHTTP *)context;
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return;
    }
    if (event == FuriHalSerialRxEventData)
    {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(fhttp->flipper_http_stream, &data, 1, 0);
        furi_thread_flags_set(fhttp->rx_thread_id, WorkerEvtRxDone);
    }
}

// Timer callback function
/**
 * @brief      Callback function for the GET timeout timer.
 * @return     0
 * @param      context   The FlipperHTTP context.
 * @note       This function will be called when the GET request times out.
 */
static void get_timeout_timer_callback(void *context)
{
    FlipperHTTP *fhttp = (FlipperHTTP *)context;
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return;
    }
    FURI_LOG_E(HTTP_TAG, "Timeout reached without receiving the end.");

    // Reset the state
    fhttp->started_receiving = false;

    // Update UART state
    fhttp->state = ISSUE;
}

static void flipper_http_rx_callback(const char *line, void *context); // forward declaration

// UART initialization function
/**
 * @brief      Initialize UART.
 * @return     FlipperHTTP context if the UART was initialized successfully, NULL otherwise.
 * @note       The received data will be handled asynchronously via the callback.
 */
FlipperHTTP *flipper_http_alloc()
{
    FlipperHTTP *fhttp = (FlipperHTTP *)malloc(sizeof(FlipperHTTP));
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate FlipperHTTP.");
        return NULL;
    }
    memset(fhttp, 0, sizeof(FlipperHTTP)); // Initialize allocated memory to zero

    fhttp->flipper_http_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    if (!fhttp->flipper_http_stream)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate UART stream buffer.");
        free(fhttp);
        return NULL;
    }

    fhttp->rx_thread = furi_thread_alloc();
    if (!fhttp->rx_thread)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate UART thread.");
        furi_stream_buffer_free(fhttp->flipper_http_stream);
        free(fhttp);
        return NULL;
    }

    furi_thread_set_name(fhttp->rx_thread, "FlipperHTTP_RxThread");
    furi_thread_set_stack_size(fhttp->rx_thread, 1024);
    furi_thread_set_context(fhttp->rx_thread, fhttp); // Corrected context
    furi_thread_set_callback(fhttp->rx_thread, flipper_http_worker);

    fhttp->handle_rx_line_cb = flipper_http_rx_callback;
    fhttp->callback_context = fhttp;

    furi_thread_start(fhttp->rx_thread);
    fhttp->rx_thread_id = furi_thread_get_id(fhttp->rx_thread);

    // Handle when the UART control is busy to avoid furi_check failed
    if (furi_hal_serial_control_is_busy(UART_CH))
    {
        FURI_LOG_E(HTTP_TAG, "UART control is busy.");
        // Cleanup resources
        furi_thread_flags_set(fhttp->rx_thread_id, WorkerEvtStop);
        furi_thread_join(fhttp->rx_thread);
        furi_thread_free(fhttp->rx_thread);
        furi_stream_buffer_free(fhttp->flipper_http_stream);
        free(fhttp);
        return NULL;
    }

    fhttp->serial_handle = furi_hal_serial_control_acquire(UART_CH);
    if (!fhttp->serial_handle)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to acquire UART control - handle is NULL");
        // Cleanup resources
        furi_thread_flags_set(fhttp->rx_thread_id, WorkerEvtStop);
        furi_thread_join(fhttp->rx_thread);
        furi_thread_free(fhttp->rx_thread);
        furi_stream_buffer_free(fhttp->flipper_http_stream);
        free(fhttp);
        return NULL;
    }

    // Initialize UART with acquired handle
    furi_hal_serial_init(fhttp->serial_handle, BAUDRATE);

    // Enable RX direction
    furi_hal_serial_enable_direction(fhttp->serial_handle, FuriHalSerialDirectionRx);

    // Start asynchronous RX with the corrected callback and context
    furi_hal_serial_async_rx_start(fhttp->serial_handle, _flipper_http_rx_callback, fhttp, false); // Corrected context

    // Wait for the TX to complete to ensure UART is ready
    furi_hal_serial_tx_wait_complete(fhttp->serial_handle);

    // Allocate the timer for handling timeouts
    fhttp->get_timeout_timer = furi_timer_alloc(
        get_timeout_timer_callback, // Callback function
        FuriTimerTypeOnce,          // One-shot timer
        fhttp                       // Corrected context
    );

    if (!fhttp->get_timeout_timer)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate HTTP request timeout timer.");
        // Cleanup resources
        furi_hal_serial_async_rx_stop(fhttp->serial_handle);
        furi_hal_serial_disable_direction(fhttp->serial_handle, FuriHalSerialDirectionRx);
        furi_hal_serial_control_release(fhttp->serial_handle);
        furi_hal_serial_deinit(fhttp->serial_handle);
        furi_thread_flags_set(fhttp->rx_thread_id, WorkerEvtStop);
        furi_thread_join(fhttp->rx_thread);
        furi_thread_free(fhttp->rx_thread);
        furi_stream_buffer_free(fhttp->flipper_http_stream);
        free(fhttp);
        return NULL;
    }

    // Set the timer thread priority if needed
    furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);

    fhttp->last_response = (char *)malloc(RX_BUF_SIZE);
    if (!fhttp->last_response)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate memory for last_response.");
        // Cleanup resources
        furi_timer_free(fhttp->get_timeout_timer);
        furi_hal_serial_async_rx_stop(fhttp->serial_handle);
        furi_hal_serial_disable_direction(fhttp->serial_handle, FuriHalSerialDirectionRx);
        furi_hal_serial_control_release(fhttp->serial_handle);
        furi_hal_serial_deinit(fhttp->serial_handle);
        furi_thread_flags_set(fhttp->rx_thread_id, WorkerEvtStop);
        furi_thread_join(fhttp->rx_thread);
        furi_thread_free(fhttp->rx_thread);
        furi_stream_buffer_free(fhttp->flipper_http_stream);
        free(fhttp);
        return NULL;
    }
    memset(fhttp->last_response, 0, RX_BUF_SIZE); // Initialize last_response

    fhttp->state = IDLE;

    // FURI_LOG_I(HTTP_TAG, "UART initialized successfully.");
    return fhttp;
}

// Deinitialize UART
/**
 * @brief      Deinitialize UART.
 * @return     void
 * @param fhttp The FlipperHTTP context
 * @note       This function will stop the asynchronous RX, release the serial handle, and free the resources.
 */
void flipper_http_free(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return;
    }
    if (fhttp->serial_handle == NULL)
    {
        FURI_LOG_E(HTTP_TAG, "UART handle is NULL. Already deinitialized?");
        return;
    }
    // Stop asynchronous RX
    furi_hal_serial_async_rx_stop(fhttp->serial_handle);

    // Release and deinitialize the serial handle
    furi_hal_serial_disable_direction(fhttp->serial_handle, FuriHalSerialDirectionRx);
    furi_hal_serial_control_release(fhttp->serial_handle);
    furi_hal_serial_deinit(fhttp->serial_handle);

    // Signal the worker thread to stop
    furi_thread_flags_set(fhttp->rx_thread_id, WorkerEvtStop);
    // Wait for the thread to finish
    furi_thread_join(fhttp->rx_thread);
    // Free the thread resources
    furi_thread_free(fhttp->rx_thread);

    // Free the stream buffer
    furi_stream_buffer_free(fhttp->flipper_http_stream);

    // Free the timer
    if (fhttp->get_timeout_timer)
    {
        furi_timer_free(fhttp->get_timeout_timer);
        fhttp->get_timeout_timer = NULL;
    }

    // Free the last response
    if (fhttp->last_response)
    {
        free(fhttp->last_response);
        fhttp->last_response = NULL;
    }

    // Free the FlipperHTTP context
    free(fhttp);
    fhttp = NULL;

    // FURI_LOG_I("FlipperHTTP", "UART deinitialized successfully.");
}

/**
 * @brief      Append received data to a file.
 * @return     true if the data was appended successfully, false otherwise.
 * @param      data        The data to append to the file.
 * @param      data_size   The size of the data to append.
 * @param      start_new_file  Flag to indicate if a new file should be created.
 * @param      file_path   The path to the file.
 * @note       Make sure to initialize the file path before calling this function.
 */
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

/**
 * @brief      Load data from a file.
 * @return     The loaded data as a FuriString.
 * @param      file_path The path to the file to load.
 */
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
        FURI_LOG_E(HTTP_TAG, "Failed to open file for reading: %s", file_path);
        return NULL;
    }

    size_t file_size = storage_file_size(file);

    // final memory check
    if (memmgr_heap_get_max_free_block() < file_size)
    {
        FURI_LOG_E(HTTP_TAG, "Not enough heap to read file.");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Allocate a buffer to hold the read data
    uint8_t *buffer = (uint8_t *)malloc(file_size);
    if (!buffer)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate buffer");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
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

    // Clean up
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    free(buffer);
    return str_result;
}

/**
 * @brief      Load data from a file with a size limit.
 * @return     The loaded data as a FuriString.
 * @param      file_path The path to the file to load.
 * @param      limit     The size limit for loading data.
 */
FuriString *flipper_http_load_from_file_with_limit(char *file_path, size_t limit)
{
    if (memmgr_heap_get_max_free_block() < limit)
    {
        FURI_LOG_E(HTTP_TAG, "Not enough heap to read file.");
        return NULL;
    }

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
        FURI_LOG_E(HTTP_TAG, "Failed to open file for reading: %s", file_path);
        return NULL;
    }

    size_t file_size = storage_file_size(file);

    if (file_size > limit)
    {
        FURI_LOG_E(HTTP_TAG, "File size exceeds limit: %d > %d", file_size, limit);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // final memory check
    if (memmgr_heap_get_max_free_block() < file_size)
    {
        FURI_LOG_E(HTTP_TAG, "Not enough heap to read file.");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Allocate a buffer to hold the read data
    uint8_t *buffer = (uint8_t *)malloc(file_size);
    if (!buffer)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate buffer");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Allocate a FuriString with preallocated capacity
    FuriString *str_result = furi_string_alloc();
    if (!str_result)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate FuriString");
        free(buffer);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    furi_string_reserve(str_result, file_size);

    // Read data into the buffer
    size_t read_count = storage_file_read(file, buffer, file_size);
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E(HTTP_TAG, "Error reading from file.");
        furi_string_free(str_result);
        free(buffer);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    if (read_count == 0)
    {
        FURI_LOG_E(HTTP_TAG, "No data read from file.");
        furi_string_free(str_result);
        free(buffer);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // Append the entire buffer to FuriString in one operation
    furi_string_cat_str(str_result, (char *)buffer);

    // Clean up
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    free(buffer);
    return str_result;
}

/**
 * @brief Perform a task while displaying a loading screen
 * @param fhttp The FlipperHTTP context
 * @param http_request The function to send the request
 * @param parse_response The function to parse the response
 * @param success_view_id The view ID to switch to on success
 * @param failure_view_id The view ID to switch to on failure
 * @param view_dispatcher The view dispatcher to use
 * @return
 */
void flipper_http_loading_task(FlipperHTTP *fhttp,
                               bool (*http_request)(void),
                               bool (*parse_response)(void),
                               uint32_t success_view_id,
                               uint32_t failure_view_id,
                               ViewDispatcher **view_dispatcher)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return;
    }
    if (fhttp->state == INACTIVE)
    {
        view_dispatcher_switch_to_view(*view_dispatcher, failure_view_id);
        return;
    }
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
    if (!flipper_http_process_response_async(fhttp, http_request, parse_response))
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
    loading_free(loading); // comment this out if you experience a freeze
}

/**
 * @brief      Parse JSON data.
 * @return     true if the JSON data was parsed successfully, false otherwise.
 * @param      fhttp The FlipperHTTP context
 * @param      key       The key to parse from the JSON data.
 * @param      json_data The JSON data to parse.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_parse_json(FlipperHTTP *fhttp, const char *key, const char *json_data)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return false;
    }
    if (!key || !json_data)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_parse_json.");
        return false;
    }

    char buffer[512];
    int ret =
        snprintf(buffer, sizeof(buffer), "[PARSE]{\"key\":\"%s\",\"json\":%s}", key, json_data);

    if (ret < 0 || ret >= (int)sizeof(buffer))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format JSON parse command.");
        return false;
    }

    return flipper_http_send_data(fhttp, buffer);
}

/**
 * @brief      Parse JSON array data.
 * @return     true if the JSON array data was parsed successfully, false otherwise.
 * @param      fhttp The FlipperHTTP context
 * @param      key       The key to parse from the JSON array data.
 * @param      index     The index to parse from the JSON array data.
 * @param      json_data The JSON array data to parse.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_parse_json_array(FlipperHTTP *fhttp, const char *key, int index, const char *json_data)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return false;
    }
    if (!key || !json_data)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_parse_json_array.");
        return false;
    }

    char buffer[512];
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

    return flipper_http_send_data(fhttp, buffer);
}

/**
 * @brief Process requests and parse JSON data asynchronously
 * @param fhttp The FlipperHTTP context
 * @param http_request The function to send the request
 * @param parse_json The function to parse the JSON
 * @return true if successful, false otherwise
 */
bool flipper_http_process_response_async(FlipperHTTP *fhttp, bool (*http_request)(void), bool (*parse_json)(void))
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return false;
    }
    if (http_request()) // start the async request
    {
        furi_timer_start(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);
        fhttp->state = RECEIVING;
    }
    else
    {
        FURI_LOG_E(HTTP_TAG, "Failed to send request");
        return false;
    }
    while (fhttp->state == RECEIVING && furi_timer_is_running(fhttp->get_timeout_timer) > 0)
    {
        // Wait for the request to be received
        furi_delay_ms(100);
    }
    furi_timer_stop(fhttp->get_timeout_timer);
    if (!parse_json()) // parse the JSON before switching to the view (synchonous)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to parse the JSON...");
        return false;
    }
    return true;
}

/**
 * @brief      Send a request to the specified URL.
 * @return     true if the request was successful, false otherwise.
 * @param      fhttp The FlipperHTTP context
 * @param      method The HTTP method to use.
 * @param      url  The URL to send the request to.
 * @param      headers  The headers to send with the request.
 * @param      payload  The data to send with the request.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_request(FlipperHTTP *fhttp, HTTPMethod method, const char *url, const char *headers, const char *payload)
{
    if (!fhttp)
    {
        FURI_LOG_E("FlipperHTTP", "Failed to get context.");
        return false;
    }
    if (!url)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_request.");
        return false;
    }

    // Prepare request command
    char command[512];
    int ret = 0;

    switch (method)
    {
    case GET:
        if (headers && strlen(headers) > 0)
            ret = snprintf(command, sizeof(command), "[GET/HTTP]{\"url\":\"%s\",\"headers\":%s}", url, headers);
        else
            ret = snprintf(command, sizeof(command), "[GET]%s", url);
        break;
    case POST:
        if (!headers || !payload)
        {
            FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_request.");
            return false;
        }
        ret = snprintf(command, sizeof(command), "[POST/HTTP]{\"url\":\"%s\",\"headers\":%s,\"payload\":%s}", url, headers, payload);
        break;
    case PUT:
        if (!headers || !payload)
        {
            FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_request.");
            return false;
        }
        ret = snprintf(command, sizeof(command), "[PUT/HTTP]{\"url\":\"%s\",\"headers\":%s,\"payload\":%s}", url, headers, payload);
        break;
    case DELETE:
        if (!headers || !payload)
        {
            FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_request.");
            return false;
        }
        ret = snprintf(command, sizeof(command), "[DELETE/HTTP]{\"url\":\"%s\",\"headers\":%s,\"payload\":%s}", url, headers, payload);
        break;
    case BYTES:
        if (!headers)
        {
            FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_request.");
            return false;
        }
        if (strlen(fhttp->file_path) == 0)
        {
            FURI_LOG_E("FlipperHTTP", "File path is not set.");
            return false;
        }
        fhttp->save_received_data = false;
        fhttp->is_bytes_request = true;
        ret = snprintf(command, sizeof(command), "[GET/BYTES]{\"url\":\"%s\",\"headers\":%s}", url, headers);
        break;
    case BYTES_POST:
        if (!headers || !payload)
        {
            FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_request.");
            return false;
        }
        if (strlen(fhttp->file_path) == 0)
        {
            FURI_LOG_E("FlipperHTTP", "File path is not set.");
            return false;
        }
        fhttp->save_received_data = false;
        fhttp->is_bytes_request = true;
        ret = snprintf(command, sizeof(command), "[POST/BYTES]{\"url\":\"%s\",\"headers\":%s,\"payload\":%s}", url, headers, payload);
        break;
    }

    // check if ret is valid
    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format request command.");
        return false;
    }

    // set method
    fhttp->method = method;

    // Send request via UART
    return flipper_http_send_data(fhttp, command);
}

/**
 * @brief      Send a command to save WiFi settings.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_save_wifi(FlipperHTTP *fhttp, const char *ssid, const char *password)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return false;
    }
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

    return flipper_http_send_data(fhttp, buffer);
}

/**
 * @brief      Send a command.
 * @return     true if the request was successful, false otherwise.
 * @param      fhttp The FlipperHTTP context
 * @param      command The command to send.
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_send_command(FlipperHTTP *fhttp, HTTPCommand command)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return false;
    }
    switch (command)
    {
    case HTTP_CMD_WIFI_CONNECT:
        return flipper_http_send_data(fhttp, "[WIFI/CONNECT]");
    case HTTP_CMD_WIFI_DISCONNECT:
        return flipper_http_send_data(fhttp, "[WIFI/DISCONNECT]");
    case HTTP_CMD_IP_ADDRESS:
        return flipper_http_send_data(fhttp, "[IP/ADDRESS]");
    case HTTP_CMD_IP_WIFI:
        return flipper_http_send_data(fhttp, "[WIFI/IP]");
    case HTTP_CMD_SCAN:
        return flipper_http_send_data(fhttp, "[WIFI/SCAN]");
    case HTTP_CMD_LIST_COMMANDS:
        return flipper_http_send_data(fhttp, "[LIST]");
    case HTTP_CMD_LED_ON:
        return flipper_http_send_data(fhttp, "[LED/ON]");
    case HTTP_CMD_LED_OFF:
        return flipper_http_send_data(fhttp, "[LED/OFF]");
    case HTTP_CMD_PING:
        fhttp->state = INACTIVE; // set state as INACTIVE to be made IDLE if PONG is received
        return flipper_http_send_data(fhttp, "[PING]");
    default:
        FURI_LOG_E(HTTP_TAG, "Invalid command.");
        return false;
    }
}

/**
 * @brief      Send data over UART with newline termination.
 * @return     true if the data was sent successfully, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      data  The data to send over UART.
 * @note       The data will be sent over UART with a newline character appended.
 */
bool flipper_http_send_data(FlipperHTTP *fhttp, const char *data)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return false;
    }

    size_t data_length = strlen(data);
    if (data_length == 0)
    {
        FURI_LOG_E("FlipperHTTP", "Attempted to send empty data.");
        return false;
    }

    // Create a buffer with data + '\n'
    size_t send_length = data_length + 1; // +1 for '\n'
    if (send_length > 512)
    { // Ensure buffer size is sufficient
        FURI_LOG_E("FlipperHTTP", "Data too long to send over FHTTP->");
        return false;
    }

    char send_buffer[513]; // 512 + 1 for safety
    strncpy(send_buffer, data, 512);
    send_buffer[data_length] = '\n';     // Append newline
    send_buffer[data_length + 1] = '\0'; // Null-terminate

    if (fhttp->state == INACTIVE && ((strstr(send_buffer, "[PING]") == NULL) &&
                                     (strstr(send_buffer, "[WIFI/CONNECT]") == NULL)))
    {
        FURI_LOG_E("FlipperHTTP", "Cannot send data while INACTIVE.");
        fhttp->last_response = "Cannot send data while INACTIVE.";
        return false;
    }

    fhttp->state = SENDING;
    furi_hal_serial_tx(fhttp->serial_handle, (const uint8_t *)send_buffer, send_length);

    // FURI_LOG_I("FlipperHTTP", "Sent data over UART: %s", send_buffer);
    fhttp->state = IDLE;
    return true;
}

// Function to set content length and status code
static void set_header(FlipperHTTP *fhttp)
{
    // example response: [GET/SUCCESS]{"Status-Code":200,"Content-Length":12528}
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Invalid arguments provided to set_header.");
        return;
    }

    size_t error_size = -1;

    // reset values
    fhttp->content_length = 0;
    fhttp->status_code = 0;
    fhttp->bytes_received = 0;

    FuriString *furi_string = furi_string_alloc_set_str(fhttp->last_response);
    if (!furi_string)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to allocate memory for furi_string.");
        return;
    }

    size_t status_code_start = furi_string_search_str(furi_string, "\"Status-Code\":", 0);
    if (status_code_start != error_size)
    {
        // trim everything, including the status code and colon
        furi_string_right(furi_string, status_code_start + strlen("\"Status-Code\":"));

        // find comma (we have this currently: 200,"Content-Length":12528})
        size_t comma = furi_string_search_str(furi_string, ",\"Content-Length\":", 0);
        if (comma == error_size)
        {
            FURI_LOG_E(HTTP_TAG, "Failed to find comma in furi_string.");
            furi_string_free(furi_string);
            return;
        }

        // set status code
        FuriString *status_code_str = furi_string_alloc();

        // dest, src, start, length
        furi_string_set_n(status_code_str, furi_string, 0, comma);
        fhttp->status_code = atoi(furi_string_get_cstr(status_code_str));
        furi_string_free(status_code_str);

        // trim left to remove everything before the content length
        furi_string_right(furi_string, comma + strlen(",\"Content-Length\":"));

        // find closing brace (we have this currently: 12528})
        size_t closing_brace = furi_string_search_str(furi_string, "}", 0);
        if (closing_brace == error_size)
        {
            FURI_LOG_E(HTTP_TAG, "Failed to find closing brace in furi_string.");
            furi_string_free(furi_string);
            return;
        }

        // set content length
        FuriString *content_length_str = furi_string_alloc();

        // dest, src, start, length
        furi_string_set_n(content_length_str, furi_string, 0, closing_brace);
        fhttp->content_length = atoi(furi_string_get_cstr(content_length_str));
        furi_string_free(content_length_str);
    }

    // print results
    // FURI_LOG_I(HTTP_TAG, "Status Code: %d", fhttp->status_code);
    // FURI_LOG_I(HTTP_TAG, "Content Length: %d", fhttp->content_length);

    // free the furi_string
    furi_string_free(furi_string);
}

// Function to trim leading and trailing spaces and newlines from a constant string
static char *trim(const char *str)
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
 * @brief      Callback function to handle received data asynchronously.
 * @return     void
 * @param      line     The received line.
 * @param      context  The FlipperHTTP context.
 * @note       The received data will be handled asynchronously via the callback and handles the state of the UART.
 */
static void flipper_http_rx_callback(const char *line, void *context)
{
    FlipperHTTP *fhttp = (FlipperHTTP *)context;
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return;
    }
    if (!line)
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
            strncpy(fhttp->last_response, trimmed_line, RX_BUF_SIZE);
        }
    }
    free(trimmed_line); // Free the allocated memory for trimmed_line

    if (fhttp->state != INACTIVE && fhttp->state != ISSUE)
    {
        fhttp->state = RECEIVING;
    }

    // Uncomment below line to log the data received over UART
    //(HTTP_TAG, "Received UART line: %s", line);

    // Check if we've started receiving data from a GET request
    if (fhttp->started_receiving && (fhttp->method == GET || fhttp->method == BYTES))
    {
        // Restart the timeout timer each time new data is received
        furi_timer_restart(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);

        if (strstr(line, "[GET/END]") != NULL)
        {
            // FURI_LOG_I(HTTP_TAG, "GET request completed.");
            //  Stop the timer since we've completed the GET request
            furi_timer_stop(fhttp->get_timeout_timer);
            fhttp->started_receiving = false;
            fhttp->just_started = false;
            fhttp->state = IDLE;
            fhttp->save_bytes = false;
            fhttp->save_received_data = false;

            if (fhttp->is_bytes_request)
            {
                // Search for the binary marker `[GET/END]` in the file buffer
                const char marker[] = "[GET/END]";
                const size_t marker_len = sizeof(marker) - 1; // Exclude null terminator

                for (size_t i = 0; i <= fhttp->file_buffer_len - marker_len; i++)
                {
                    // Check if the marker is found
                    if (memcmp(&fhttp->file_buffer[i], marker, marker_len) == 0)
                    {
                        // Remove the marker by shifting the remaining data left
                        size_t remaining_len = fhttp->file_buffer_len - (i + marker_len);
                        memmove(&fhttp->file_buffer[i], &fhttp->file_buffer[i + marker_len], remaining_len);
                        fhttp->file_buffer_len -= marker_len;
                        break;
                    }
                }

                // If there is data left in the buffer, append it to the file
                if (fhttp->file_buffer_len > 0)
                {
                    if (!flipper_http_append_to_file(fhttp->file_buffer, fhttp->file_buffer_len, false, fhttp->file_path))
                    {
                        FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
                    }
                    fhttp->file_buffer_len = 0;
                }
            }

            fhttp->is_bytes_request = false;
            return;
        }

        // Append the new line to the existing data
        if (fhttp->save_received_data &&
            !flipper_http_append_to_file(
                line, strlen(line), !fhttp->just_started, fhttp->file_path))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
            fhttp->started_receiving = false;
            fhttp->just_started = false;
            fhttp->state = IDLE;
            return;
        }

        if (!fhttp->just_started)
        {
            fhttp->just_started = true;
        }
        return;
    }

    // Check if we've started receiving data from a POST request
    else if (fhttp->started_receiving && (fhttp->method == POST || fhttp->method == BYTES_POST))
    {
        // Restart the timeout timer each time new data is received
        furi_timer_restart(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);

        if (strstr(line, "[POST/END]") != NULL)
        {
            // FURI_LOG_I(HTTP_TAG, "POST request completed.");
            //  Stop the timer since we've completed the POST request
            furi_timer_stop(fhttp->get_timeout_timer);
            fhttp->started_receiving = false;
            fhttp->just_started = false;
            fhttp->state = IDLE;
            fhttp->save_bytes = false;
            fhttp->save_received_data = false;

            if (fhttp->is_bytes_request)
            {
                // Search for the binary marker `[POST/END]` in the file buffer
                const char marker[] = "[POST/END]";
                const size_t marker_len = sizeof(marker) - 1; // Exclude null terminator

                for (size_t i = 0; i <= fhttp->file_buffer_len - marker_len; i++)
                {
                    // Check if the marker is found
                    if (memcmp(&fhttp->file_buffer[i], marker, marker_len) == 0)
                    {
                        // Remove the marker by shifting the remaining data left
                        size_t remaining_len = fhttp->file_buffer_len - (i + marker_len);
                        memmove(&fhttp->file_buffer[i], &fhttp->file_buffer[i + marker_len], remaining_len);
                        fhttp->file_buffer_len -= marker_len;
                        break;
                    }
                }

                // If there is data left in the buffer, append it to the file
                if (fhttp->file_buffer_len > 0)
                {
                    if (!flipper_http_append_to_file(fhttp->file_buffer, fhttp->file_buffer_len, false, fhttp->file_path))
                    {
                        FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
                    }
                    fhttp->file_buffer_len = 0;
                }
            }

            fhttp->is_bytes_request = false;
            return;
        }

        // Append the new line to the existing data
        if (fhttp->save_received_data &&
            !flipper_http_append_to_file(
                line, strlen(line), !fhttp->just_started, fhttp->file_path))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
            fhttp->started_receiving = false;
            fhttp->just_started = false;
            fhttp->state = IDLE;
            return;
        }

        if (!fhttp->just_started)
        {
            fhttp->just_started = true;
        }
        return;
    }

    // Check if we've started receiving data from a PUT request
    else if (fhttp->started_receiving && fhttp->method == PUT)
    {
        // Restart the timeout timer each time new data is received
        furi_timer_restart(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);

        if (strstr(line, "[PUT/END]") != NULL)
        {
            // FURI_LOG_I(HTTP_TAG, "PUT request completed.");
            //  Stop the timer since we've completed the PUT request
            furi_timer_stop(fhttp->get_timeout_timer);
            fhttp->started_receiving = false;
            fhttp->just_started = false;
            fhttp->state = IDLE;
            fhttp->save_bytes = false;
            fhttp->is_bytes_request = false;
            fhttp->save_received_data = false;
            return;
        }

        // Append the new line to the existing data
        if (fhttp->save_received_data &&
            !flipper_http_append_to_file(
                line, strlen(line), !fhttp->just_started, fhttp->file_path))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
            fhttp->started_receiving = false;
            fhttp->just_started = false;
            fhttp->state = IDLE;
            return;
        }

        if (!fhttp->just_started)
        {
            fhttp->just_started = true;
        }
        return;
    }

    // Check if we've started receiving data from a DELETE request
    else if (fhttp->started_receiving && fhttp->method == DELETE)
    {
        // Restart the timeout timer each time new data is received
        furi_timer_restart(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);

        if (strstr(line, "[DELETE/END]") != NULL)
        {
            // FURI_LOG_I(HTTP_TAG, "DELETE request completed.");
            //  Stop the timer since we've completed the DELETE request
            furi_timer_stop(fhttp->get_timeout_timer);
            fhttp->started_receiving = false;
            fhttp->just_started = false;
            fhttp->state = IDLE;
            fhttp->save_bytes = false;
            fhttp->is_bytes_request = false;
            fhttp->save_received_data = false;
            return;
        }

        // Append the new line to the existing data
        if (fhttp->save_received_data &&
            !flipper_http_append_to_file(
                line, strlen(line), !fhttp->just_started, fhttp->file_path))
        {
            FURI_LOG_E(HTTP_TAG, "Failed to append data to file.");
            fhttp->started_receiving = false;
            fhttp->just_started = false;
            fhttp->state = IDLE;
            return;
        }

        if (!fhttp->just_started)
        {
            fhttp->just_started = true;
        }
        return;
    }

    // Handle different types of responses
    if (strstr(line, "[SUCCESS]") != NULL || strstr(line, "[CONNECTED]") != NULL)
    {
        // FURI_LOG_I(HTTP_TAG, "Operation succeeded.");
    }
    else if (strstr(line, "[INFO]") != NULL)
    {
        // FURI_LOG_I(HTTP_TAG, "Received info: %s", line);

        if (fhttp->state == INACTIVE && strstr(line, "[INFO] Already connected to Wifi.") != NULL)
        {
            fhttp->state = IDLE;
        }
    }
    else if (strstr(line, "[GET/SUCCESS]") != NULL)
    {
        // FURI_LOG_I(HTTP_TAG, "GET request succeeded.");
        furi_timer_start(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);

        fhttp->started_receiving = true;
        fhttp->state = RECEIVING;

        // for GET request, save data only if it's a bytes request
        fhttp->save_bytes = fhttp->is_bytes_request;
        fhttp->just_started_bytes = true;
        fhttp->file_buffer_len = 0;

        // set header
        set_header(fhttp);
        return;
    }
    else if (strstr(line, "[POST/SUCCESS]") != NULL)
    {
        // FURI_LOG_I(HTTP_TAG, "POST request succeeded.");
        furi_timer_start(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);

        fhttp->started_receiving = true;
        fhttp->state = RECEIVING;

        // for POST request, save data only if it's a bytes request
        fhttp->save_bytes = fhttp->is_bytes_request;
        fhttp->just_started_bytes = true;
        fhttp->file_buffer_len = 0;

        // set header
        set_header(fhttp);
        return;
    }
    else if (strstr(line, "[PUT/SUCCESS]") != NULL)
    {
        // FURI_LOG_I(HTTP_TAG, "PUT request succeeded.");
        furi_timer_start(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);

        fhttp->started_receiving = true;
        fhttp->state = RECEIVING;

        // set header
        set_header(fhttp);
        return;
    }
    else if (strstr(line, "[DELETE/SUCCESS]") != NULL)
    {
        // FURI_LOG_I(HTTP_TAG, "DELETE request succeeded.");
        furi_timer_start(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);

        fhttp->started_receiving = true;
        fhttp->state = RECEIVING;

        // set header
        set_header(fhttp);
        return;
    }
    else if (strstr(line, "[DISCONNECTED]") != NULL)
    {
        // FURI_LOG_I(HTTP_TAG, "WiFi disconnected successfully.");
    }
    else if (strstr(line, "[ERROR]") != NULL)
    {
        FURI_LOG_E(HTTP_TAG, "Received error: %s", line);
        fhttp->state = ISSUE;
        return;
    }
    else if (strstr(line, "[PONG]") != NULL)
    {
        // FURI_LOG_I(HTTP_TAG, "Received PONG response: Wifi Dev Board is still alive.");

        // send command to connect to WiFi
        if (fhttp->state == INACTIVE)
        {
            fhttp->state = IDLE;
            return;
        }
    }

    if (fhttp->state == INACTIVE && strstr(line, "[PONG]") != NULL)
    {
        fhttp->state = IDLE;
    }
    else if (fhttp->state == INACTIVE && strstr(line, "[PONG]") == NULL)
    {
        fhttp->state = INACTIVE;
    }
    else
    {
        fhttp->state = IDLE;
    }
}

/**
 * @brief      Send a request to the specified URL to start a WebSocket connection.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @param      url  The URL to send the WebSocket request to.
 * @param port The port to connect to
 * @param headers The headers to send with the WebSocket request
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_websocket_start(FlipperHTTP *fhttp, const char *url, uint16_t port, const char *headers)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return false;
    }
    if (!url || !headers)
    {
        FURI_LOG_E("FlipperHTTP", "Invalid arguments provided to flipper_http_websocket_start.");
        return false;
    }

    // Prepare WebSocket request command with headers
    char command[512];
    int ret = snprintf(
        command,
        sizeof(command),
        "[SOCKET/START]{\"url\":\"%s\",\"port\":%d,\"headers\":%s}",
        url,
        port,
        headers);

    if (ret < 0 || ret >= (int)sizeof(command))
    {
        FURI_LOG_E("FlipperHTTP", "Failed to format WebSocket start command with headers.");
        return false;
    }

    // Send WebSocket request via UART
    return flipper_http_send_data(fhttp, command);
}

/**
 * @brief      Send a request to stop the WebSocket connection.
 * @return     true if the request was successful, false otherwise.
 * @param fhttp The FlipperHTTP context
 * @note       The received data will be handled asynchronously via the callback.
 */
bool flipper_http_websocket_stop(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to get context.");
        return false;
    }
    return flipper_http_send_data(fhttp, "[SOCKET/STOP]");
}
// Description: Flipper HTTP API (For use with Flipper Zero and the FlipperHTTP flash: https://github.com/jblanked/FlipperHTTP)
// License: MIT
// Author: JBlanked
// File: flipper_http.h
#pragma once

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/loading.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_serial.h>
#include <storage/storage.h>
#include <cfw/cfw.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_TAG               "FlipperHTTP" // change this to your app name
#define http_tag               "hello_world" // change this to your app id
#define UART_CH                (cfw_settings.uart_esp_channel) // UART channel
#define TIMEOUT_DURATION_TICKS (5 * 1000) // 5 seconds
#define BAUDRATE               (115200) // UART baudrate
#define RX_BUF_SIZE            2048 // UART RX buffer size
#define RX_LINE_BUFFER_SIZE    2048 // UART RX line buffer size (increase for large responses)
#define MAX_FILE_SHOW          2048 // Maximum data from file to show
#define FILE_BUFFER_SIZE       512 // File buffer size

// Forward declaration for callback
typedef void (*FlipperHTTP_Callback)(const char* line, void* context);

// State variable to track the UART state
typedef enum {
    INACTIVE, // Inactive state
    IDLE, // Default state
    RECEIVING, // Receiving data
    SENDING, // Sending data
    ISSUE, // Issue with connection
} HTTPState;

// Event Flags for UART Worker Thread
typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

typedef enum {
    GET, // GET request
    POST, // POST request
    PUT, // PUT request
    DELETE, // DELETE request
    //
    BYTES, // Stream bytes to file
    BYTES_POST, // Stream bytes to file after a POST request
} HTTPMethod;

typedef enum {
    HTTP_CMD_WIFI_CONNECT, // [WIFI/CONNECT] - connect to a WiFi network
    HTTP_CMD_WIFI_DISCONNECT, // [WIFI/DISCONNECT] - disconnect from the current WiFi network
    HTTP_CMD_IP_ADDRESS, // [IP/ADDRESS] - get the current IP address of the device
    HTTP_CMD_IP_WIFI, // [WIFI/IP] - get the current IP address of the WiFi interface
    HTTP_CMD_SCAN, // [WIFI/SCAN] - scan for available WiFi networks
    HTTP_CMD_LIST_COMMANDS, // [LIST] - list all available commands
    HTTP_CMD_LED_ON, // [LED/ON] - allow LED blinking when processing
    HTTP_CMD_LED_OFF, // [LED/OFF] - disable LED blinking when processing
    HTTP_CMD_PING, // [PING] - respond with [PONG]
    HTTP_CMD_VERSION, // [VERSION] - get the current version of the firmware
    HTTP_CMD_STATUS, // [WIFI/STATUS] - check if connected to WiFi
    HTTP_CMD_REBOOT, // [REBOOT] - reboot the device
    HTTP_CMD_SSID, // [WIFI/SSID] - get the current connected SSID
    HTTP_CMD_WIFI_LIST, // [WIFI/LIST] - list saved WiFi networks
} HTTPCommand; // list of non-input commands

// FlipperHTTP Structure
typedef struct {
    FuriStreamBuffer* flipper_http_stream; // Stream buffer for UART communication
    FuriHalSerialHandle* serial_handle; // Serial handle for UART communication
    FuriThread* rx_thread; // Worker thread for UART
    FuriThreadId rx_thread_id; // Worker thread ID
    FlipperHTTP_Callback handle_rx_line_cb; // Callback for received lines
    void* callback_context; // Context for the callback
    HTTPState state; // State of the UART
    HTTPMethod method; // HTTP method
    char* last_response; // variable to store the last received data from the UART
    char file_path[256]; // Path to save the received data
    FuriTimer* get_timeout_timer; // Timer for HTTP request timeout
    bool started_receiving; // Indicates if a request has started
    bool just_started; // Indicates if data reception has just started
    bool is_bytes_request; // Flag to indicate if the request is for bytes
    bool save_bytes; // Flag to save the received data to a file
    bool save_received_data; // Flag to save the received data to a file
    bool just_started_bytes; // Indicates if bytes data reception has just started
    size_t bytes_received; // Number of bytes received
    char rx_line_buffer[RX_LINE_BUFFER_SIZE]; // Buffer for received lines
    uint8_t file_buffer[FILE_BUFFER_SIZE]; // Buffer for file data
    size_t file_buffer_len; // Length of the file buffer
    size_t content_length; // Length of the content received
    int status_code; // HTTP status code
} FlipperHTTP;

/**
     * @brief      Initialize UART.
     * @return     FlipperHTTP context if the UART was initialized successfully, NULL otherwise.
     * @note       The received data will be handled asynchronously via the callback.
     */
FlipperHTTP* flipper_http_alloc();

/**
     * @brief      Deinitialize UART.
     * @return     void
     * @param fhttp The FlipperHTTP context
     * @note       This function will stop the asynchronous RX, release the serial handle, and free the resources.
     */
void flipper_http_free(FlipperHTTP* fhttp);

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
    const void* data,
    size_t data_size,
    bool start_new_file,
    char* file_path);

/**
     * @brief      Load data from a file.
     * @return     The loaded data as a FuriString.
     * @param      file_path The path to the file to load.
     */
FuriString* flipper_http_load_from_file(char* file_path);

/**
     * @brief      Load data from a file with a size limit.
     * @return     The loaded data as a FuriString.
     * @param      file_path The path to the file to load.
     * @param      limit     The size limit for loading data.
     */
FuriString* flipper_http_load_from_file_with_limit(char* file_path, size_t limit);

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
void flipper_http_loading_task(
    FlipperHTTP* fhttp,
    bool (*http_request)(void),
    bool (*parse_response)(void),
    uint32_t success_view_id,
    uint32_t failure_view_id,
    ViewDispatcher** view_dispatcher);

/**
     * @brief      Parse JSON data.
     * @return     true if the JSON data was parsed successfully, false otherwise.
     * @param fhttp The FlipperHTTP context
     * @param      key       The key to parse from the JSON data.
     * @param      json_data The JSON data to parse.
     * @note       The received data will be handled asynchronously via the callback.
     */
bool flipper_http_parse_json(FlipperHTTP* fhttp, const char* key, const char* json_data);

/**
     * @brief      Parse JSON array data.
     * @return     true if the JSON array data was parsed successfully, false otherwise.
     * @param fhttp The FlipperHTTP context
     * @param      key       The key to parse from the JSON array data.
     * @param      index     The index to parse from the JSON array data.
     * @param      json_data The JSON array data to parse.
     * @note       The received data will be handled asynchronously via the callback.
     */
bool flipper_http_parse_json_array(
    FlipperHTTP* fhttp,
    const char* key,
    int index,
    const char* json_data);

/**
     * @brief Process requests and parse JSON data asynchronously
     * @param fhttp The FlipperHTTP context
     * @param http_request The function to send the request
     * @param parse_json The function to parse the JSON
     * @return true if successful, false otherwise
     */
bool flipper_http_process_response_async(
    FlipperHTTP* fhttp,
    bool (*http_request)(void),
    bool (*parse_json)(void));

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
bool flipper_http_request(
    FlipperHTTP* fhttp,
    HTTPMethod method,
    const char* url,
    const char* headers,
    const char* payload);

/**
     * @brief      Send a command to save WiFi settings.
     * @return     true if the request was successful, false otherwise.
     * @param fhttp The FlipperHTTP context
     * @note       The received data will be handled asynchronously via the callback.
     */
bool flipper_http_save_wifi(FlipperHTTP* fhttp, const char* ssid, const char* password);

/**
     * @brief      Send a command.
     * @return     true if the request was successful, false otherwise.
     * @param      fhttp The FlipperHTTP context
     * @param      command The command to send.
     * @note       The received data will be handled asynchronously via the callback.
     */
bool flipper_http_send_command(FlipperHTTP* fhttp, HTTPCommand command);

/**
     * @brief      Send data over UART with newline termination.
     * @return     true if the data was sent successfully, false otherwise.
     * @param fhttp The FlipperHTTP context
     * @param      data  The data to send over UART.
     * @note       The data will be sent over UART with a newline character appended.
     */
bool flipper_http_send_data(FlipperHTTP* fhttp, const char* data);

/**
     * @brief      Send a request to the specified URL to start a WebSocket connection.
     * @return     true if the request was successful, false otherwise.
     * @param fhttp The FlipperHTTP context
     * @param      url  The URL to send the WebSocket request to.
     * @param port The port to connect to
     * @param headers The headers to send with the WebSocket request
     * @note       The received data will be handled asynchronously via the callback.
     */
bool flipper_http_websocket_start(
    FlipperHTTP* fhttp,
    const char* url,
    uint16_t port,
    const char* headers);

/**
     * @brief      Send a request to stop the WebSocket connection.
     * @return     true if the request was successful, false otherwise.
     * @param fhttp The FlipperHTTP context
     * @note       The received data will be handled asynchronously via the callback.
     */
bool flipper_http_websocket_stop(FlipperHTTP* fhttp);

#ifdef __cplusplus
}
#endif

#ifndef UART_UTILS_H
#define UART_UTILS_H

#include <furi_hal_serial.h>
#include <furi_hal_serial_types.h>
#include <furi_hal_serial_control.h>
#include <furi/core/thread.h>
#include <furi/core/stream_buffer.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include "menu.h"
#include "uart_storage.h"
#include <stdbool.h>
#include "firmware_api.h"

#ifdef HAS_MOMENTUM_SUPPORT
#define UART_CH_ESP (momentum_settings.uart_esp_channel)
#define UART_CH_GPS (momentum_settings.uart_nmea_channel)
#else
#define UART_CH_ESP FuriHalSerialIdUsart
#define UART_CH_GPS FuriHalSerialIdLpuart
#endif

#define BAUDRATE (115200)

#define TEXT_BOX_STORE_SIZE           (8 * 1024) // 8KB text box buffer size
#define RX_BUF_SIZE                   2048
#define PCAP_BUF_SIZE                 4096
#define GHOST_ESP_APP_FOLDER          "/ext/apps_data/ghost_esp"
#define GHOST_ESP_APP_FOLDER_PCAPS    "/ext/apps_data/ghost_esp/pcaps"
#define GHOST_ESP_APP_FOLDER_WARDRIVE "/ext/apps_data/ghost_esp/wardrive"
#define GHOST_ESP_APP_FOLDER_LOGS     "/ext/apps_data/ghost_esp/logs"
#define GHOST_ESP_APP_SETTINGS_FILE   "/ext/apps_data/ghost_esp/settings.ini"
#define ESP_CHECK_TIMEOUT_MS          100
#define RING_BUFFER_SIZE              (4 * 1024) // 4KB for incoming data
#define TEXT_LOG_BUFFER_SIZE          (TEXT_BOX_STORE_SIZE + RING_BUFFER_SIZE) // reuse total 16KB
#define VIEW_BUFFER_SIZE              TEXT_LOG_BUFFER_SIZE
#define PCAP_GLOBAL_HEADER_SIZE       24
#define PCAP_PACKET_HEADER_SIZE       16

void update_text_box_view(AppState* state);
void handle_uart_rx_data(uint8_t* buf, size_t len, void* context);

typedef struct {
    char* buffer; // Shared text buffer
    size_t length; // Current length of content
    size_t capacity; // Total buffer capacity
    size_t view_buffer_len; // Length of current view content
    size_t view_offset; // Offset into buffer for current view
    FuriMutex* mutex; // Synchronization mutex
} TextBufferManager;

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
    WorkerEvtPcapDone = (1 << 2),
    WorkerEvtStorage = (1 << 3),
    WorkerEvtCsvDone = (1 << 4),
} WorkerEvtFlags;

typedef struct {
    char bssid[18];
    char ssid[33];
    double latitude;
    double longitude;
    int8_t rssi;
    uint8_t channel;
    char encryption_type[20];
    int64_t timestamp;
} wardriving_data_t;

typedef struct UartContext {
    FuriHalSerialHandle* serial_handle;
    FuriHalSerialHandle* gps_handle;
    FuriStreamBuffer* gps_stream;
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream;
    FuriStreamBuffer* pcap_stream;
    FuriStreamBuffer* csv_stream;
    bool pcap;
    bool csv;
    bool pcap_flush_pending;
    uint8_t mark_test_buf[11]; // Fixed size for markers
    uint8_t mark_test_idx;
    uint8_t mark_candidate_mask;
    uint8_t rx_buf[RX_BUF_SIZE + 1]; // Add +1 for null termination
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context);
    void (*handle_rx_pcap_cb)(uint8_t* buf, size_t len, void* context);
    void (*handle_rx_csv_cb)(uint8_t* buf, size_t len, void* context);
    AppState* state;
    UartStorageContext* storageContext;
    bool is_serial_active;
    TextBufferManager* text_manager;
    size_t pcap_buf_len; // Current PCAP buffer length
} UartContext;

// Function prototypes
UartContext* uart_init(AppState* state);
void uart_free(UartContext* uart);
void uart_stop_thread(UartContext* uart);
void uart_send(UartContext* uart, const uint8_t* data, size_t len);
bool uart_is_marauder_firmware(UartContext* uart);
bool uart_receive_data(
    UartContext* uart,
    ViewDispatcher* view_dispatcher,
    AppState* current_view,
    const char* prefix,
    const char* extension,
    const char* TargetFolder);
bool uart_is_esp_connected(UartContext* uart);
void uart_storage_reset_logs(UartStorageContext* ctx);
void uart_storage_safe_cleanup(UartStorageContext* ctx);
void uart_reset_text_buffers(UartContext* uart);
bool uart_copy_text_buffer(UartContext* uart, char* out, size_t out_size, size_t* out_len);
bool uart_copy_text_buffer_tail(UartContext* uart, char* out, size_t out_size, size_t* out_len);
void uart_cleanup_capture_streams(UartContext* uart);

#endif

// 6675636B796F7564656B69

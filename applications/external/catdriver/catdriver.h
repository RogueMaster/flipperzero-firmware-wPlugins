#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <notification/notification_messages.h>
#include <toolbox/stream/file_stream.h>

#include "helpers/minmea.h"
#include "catdriver_icons.h"

#define appname "catdriver"

#define RX_BUF_SIZE       2048
#define BATCH_BUFFER_SIZE 30
#define RECENT_APS_SIZE   30

#define MAX_SSID_LENGTH  32
#define MAX_BSSID_LENGTH 18
#define MAX_AUTH_LENGTH  32

#define FILE_PATH EXT_PATH("apps_data/catdriver")

typedef enum {
    ScanModeAll = 0,
    ScanModeWifi = 1,
    ScanModeBle = 2,
} ScanMode;

typedef enum {
    LangEN = 0,
    LangES = 1,
} AppLanguage;

typedef enum {
    EventTypeKey,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} Event;

typedef enum {
    VIEW_MENU,
    VIEW_SCAN,
    VIEW_SETTINGS,
    VIEW_ABOUT,
    VIEW_GPS,
    VIEW_ESP_INFO,
} ViewState;

typedef struct {
    char recievedMac[MAX_BSSID_LENGTH + 1];
    char sentMac[MAX_BSSID_LENGTH + 1];
} Packet;

typedef struct {
    char ssid[MAX_SSID_LENGTH + 1];
    char bssid[MAX_BSSID_LENGTH + 1];
    char authmode[MAX_AUTH_LENGTH + 1];
    char type[8]; // "WIFI" or "BLE"
    int8_t rssi;
    uint8_t channel;
    DateTime datetime;
    uint16_t packetRxCount;
    uint16_t packetTxCount;
    float latitude;
    float longitude;
} AccessPoint;

typedef struct {
    float latitude;
    float longitude;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t satelites;
} GpsData;

typedef struct {
    FuriMessageQueue* queue;
    FuriMutex* mutex;

    FuriThread* thread_esp;
    FuriStreamBuffer* rx_stream_esp;
    uint8_t rx_buf_esp[2048];
    FuriHalSerialHandle* serial_handle_esp;

    FuriThread* thread_gps;
    FuriStreamBuffer* rx_stream_gps;
    uint8_t rx_buf_gps[2048];
    FuriHalSerialHandle* serial_handle_gps;

    // RAM Batch buffer (30 APs before append to SD)
    uint16_t batch_count;
    AccessPoint batch_buffer[BATCH_BUFFER_SIZE];

    // Recent APs circular window for UI
    uint16_t recent_count;
    int16_t recent_index;
    AccessPoint recent_aps[RECENT_APS_SIZE];
    AccessPoint active_access_point;

    uint32_t total_scanned_count;
    uint32_t total_saved_count;

    char file_name[128];
    bool file_header_written;

    ScanMode scan_mode;
    AppLanguage language;
    uint8_t menu_selected_button; // 0: ALL, 1: WIFI, 2: BLE, 3: Settings, 4: About
    uint8_t menu_last_card; // 0..3: remembers last selected card

    bool pressedButton;
    ViewState view_state;
    uint8_t about_page; // 0, 1, 2
    GpsData gps_data;

    uint8_t settings_selected_index; // 0: Language, 1: ESP Info, 2: GPS Info
    ViewState return_from_gps_view;
    char esp_version_info[64];
    bool esp_info_received;
    uint32_t esp_query_tick;
    uint8_t esp_scroll_offset;
} Context;

void catdriver_query_esp_info(Context* ctx);

void save_batch_file(Context* ctx);
void catdriver_set_scan_mode(Context* ctx, ScanMode mode);
void catdriver_stop_scan(Context* ctx);
void catdriver_start_scan_session(Context* ctx, ScanMode mode);
void catdriver_stop_scan_session(Context* ctx);

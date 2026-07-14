#ifndef CAIXIANLIN_TYPES_H
#define CAIXIANLIN_TYPES_H

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <notification/notification_messages.h>
#include <lib/subghz/devices/devices.h>

#define TAG "CaixianlinRemote"

// Settings file path
#define SETTINGS_PATH APP_DATA_PATH("settings.txt")

// Protocol timing constants (microseconds)
// Based on analysis of working recordings - uses 3:1 and 1:3 ratios
#define TE_US         250
#define SYNC_HIGH_US  (5 * TE_US) // 1250
#define SYNC_LOW_US   (3 * TE_US) // 750
#define BIT_1_HIGH_US (3 * TE_US) // 750
#define BIT_1_LOW_US  (1 * TE_US) // 250
#define BIT_0_HIGH_US (1 * TE_US) // 250
#define BIT_0_LOW_US  (3 * TE_US) // 750

#define PACKET_BITS        42
#define SIGNAL_LENGTH      ((PACKET_BITS * 2) + 2) // 42 bits (high+low pairs) + 2 sync (high+low)
#define SIGNAL_BUFFER_SIZE 128
#define WORK_BUFFER_SIZE   512

// For RX capture
#define RX_BUFFER_SIZE   2048
#define SYNC_HIGH_MIN_US 1000
#define SYNC_HIGH_MAX_US 1800
#define SYNC_LOW_MIN_US  500
#define SYNC_LOW_MAX_US  1200

// Modes
#define MODE_SHOCK   1
#define MODE_VIBRATE 2
#define MODE_BEEP    3

// App screens
typedef enum {
    ScreenSetup,
    ScreenMain,
    ScreenListen,
} AppScreen;

// Transmission state
typedef struct {
    LevelDuration buffer[SIGNAL_BUFFER_SIZE];
    size_t buffer_len;
    size_t buffer_index;
} TxState;

// RX capture state
typedef struct {
    FuriStreamBuffer* stream_buffer; // Ring buffer for ISR to write to
    int32_t work_buffer[WORK_BUFFER_SIZE]; // Working buffer for decode processing
    size_t work_buffer_len; // Number of samples in work buffer
    size_t processed; // Number of samples processed
    uint16_t captured_station_id;
    uint8_t captured_channel;
    bool capture_valid;
} RxCapture;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    NotificationApp* notifications;
    FuriMessageQueue* event_queue;

    const SubGhzDevice* radio_device;

    uint16_t station_id;
    uint8_t channel;
    uint8_t mode;
    uint8_t strength;

    TxState tx_state;
    RxCapture rx_capture;

    bool is_transmitting;
    bool is_listening;
    bool running;

    AppScreen screen;
    int setup_selected; // 0=Station ID, 1=Channel, 2=Listen, 3=Done
    int station_id_digit; // Which digit being edited (0-4)
    bool editing_station_id;

    uint32_t frame_counter; // For UI animations
} CaixianlinRemoteApp;

extern const char* mode_names[4];

void app_init(CaixianlinRemoteApp* app);

#endif // CAIXIANLIN_TYPES_H

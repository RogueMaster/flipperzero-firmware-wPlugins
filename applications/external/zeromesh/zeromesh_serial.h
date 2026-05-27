#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#include "lib/nanopb/pb.h"
#include "lib/nanopb/pb_encode.h"
#include "lib/nanopb/pb_decode.h"

#include "lib/meshtastic_api/meshtastic/mesh.pb.h"
#include "lib/meshtastic_api/meshtastic/portnums.pb.h"

#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>

#define ZEROMESH_MAGIC0 0x94
#define ZEROMESH_MAGIC1 0xC3

#define RX_STREAM_SIZE 4096
#define MAX_FRAME_SIZE 512

#define LOG_LINES 18
#define LOG_COLS  64

#define PAYLOAD_CAPTURE_MAX 256

#define PAGE_MESSAGES  0
#define PAGE_ROSTER    1
#define PAGE_STATS     2
#define PAGE_SIGNAL    3
#define PAGE_LOGS      4
#define PAGE_SETTINGS  5
#define PAGE_COUNT     6

#define MSG_HISTORY 8

#define ROSTER_MAX_NODES 16

#define SETTINGS_PATH "/ext/zeromesh/settings.cfg"
#define MAX_CHANNELS 8

#define MAX_RINGTONE_PATH 128

typedef enum {
    RosterStateList = 0,
    RosterStateChat,
    RosterStateDetails
} RosterState;

typedef struct {
    uint32_t node_id;
    uint32_t last_seen;
    int8_t last_snr;
    int16_t last_rssi;
    uint8_t battery_level;
    float voltage;
    bool has_telemetry;
	bool has_new_dm;
} NodeEntry;

typedef struct {
    NodeEntry nodes[ROSTER_MAX_NODES];
    uint8_t count;
    uint8_t selected_idx;
    RosterState state;
    uint8_t chat_scroll;
} NodeRoster;

typedef struct {
    char text[128];
    uint32_t from;
    uint32_t to;
    bool is_tx;
    uint32_t timestamp;
} Message;

typedef struct {
    Message msgs[MSG_HISTORY];
    uint8_t head;
    uint8_t count;
} MessageHistory;

typedef enum {
    RingtoneNone = 0,
    RingtoneShort,
    RingtoneDouble,
    RingtoneTriple,
    RingtoneLong,
    RingtoneSOS,
    RingtoneChirp,
    RingtoneNokia,
    RingtoneDescend,
    RingtoneBounce,
    RingtoneAlert,
    RingtonePulse,
    RingtoneSiren,
    RingtoneBeep3,
    RingtoneTrill,
    RingtoneMario,
    RingtoneLevelUp,
    RingtoneMetric,
    RingtoneMinimalist,
    RINGTONE_COUNT
} RingtoneType;

typedef enum {
    LMH_Scroll = 0,
    LMH_Wrap,
    LMH_COUNT
} LongMessageHandling;

typedef enum {
    SettingUart = 0,
    SettingBaud,
    SettingVibro,
    SettingLed,
    SettingRingtone,
    SettingScrollSpeed,
    SettingScrollFramerate,
    SettingLMH,
    SETTING_COUNT
} SettingItem;

typedef struct {
    Gui* gui;
    ViewPort* vp;
    FuriMutex* lock;

    FuriHalSerialId uart_id;
    uint32_t baud;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;

    FuriThread* rx_thread;
    volatile bool stop_thread;

    uint8_t hdr[4];
    uint8_t hdr_pos;

    uint16_t frame_len;
    uint16_t frame_pos;
    uint8_t frame_buf[MAX_FRAME_SIZE];

    uint32_t rx_bytes;
    uint32_t rx_frames_ok;
    uint32_t rx_bad_magic;
    uint32_t rx_bad_len;
    uint32_t rx_decode_fail;

    uint32_t tx_frames;
    uint32_t tx_encode_fail;

    char lines[LOG_LINES][LOG_COLS];
    uint8_t line_head;

    char status[LOG_COLS];
    
    MessageHistory history;
    
    uint8_t ui_mode;
    
    uint8_t msg_scroll_offset;
    
    bool log_paused;
    uint8_t log_scroll_offset;
    
    char last_rx_text[128];
    uint32_t last_rx_from;
    uint32_t last_rx_to;
    uint32_t last_rx_id;
    int8_t last_rx_snr;
    int16_t last_rx_rssi;
    bool has_rx_signal_data;
    
    uint32_t my_node_num;
    
    uint32_t sent_msg_ids[8];
    uint8_t sent_msg_head;
    
    uint8_t settings_cursor;
    bool settings_editing;
    
    bool notify_vibro;
    bool notify_led;
    RingtoneType notify_ringtone;
    
    uint8_t scroll_speed;
    uint8_t scroll_framerate;
    LongMessageHandling lmh_mode;
    
    uint8_t current_channel;
    uint8_t num_channels;
    
    volatile bool notify_active;
    uint32_t notify_start_tick;
    
    bool show_keyboard;
    char text_buffer[64];
    ViewDispatcher* kb_dispatcher;
    TextInput* text_input;

    NodeRoster roster;
} ZeroMeshApp;

int32_t zeromesh_serial_app(void* p);
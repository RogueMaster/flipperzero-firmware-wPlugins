#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#include "lib/nanopb/pb.h"
#include "lib/nanopb/pb_encode.h"
#include "lib/nanopb/pb_decode.h"

#include "lib/meshtastic_api/meshtastic/mesh.pb.h"
#include "lib/meshtastic_api/meshtastic/config.pb.h"
#include "lib/meshtastic_api/meshtastic/portnums.pb.h"

#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <bt/bt_service/bt.h>
#include <furi_hal_bt.h>

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
#define PAGE_MAP       6
#define PAGE_NODECFG   7
#define PAGE_COUNT     8

#define MSG_HISTORY 8

#define ROSTER_MAX_NODES 16

/* The catalogue only permits writes to the app data directory. The old
   location is still read so an existing install keeps its settings. */
#define SETTINGS_PATH     APP_DATA_PATH("settings.cfg")
#define SETTINGS_PATH_OLD "/ext/zeromesh/settings.cfg"
#define MAX_CHANNELS 8

#define MAX_RINGTONE_PATH 128

/* Custom tones live beside the map data. The cap is small on purpose: the
   names sit in the app struct and the list is cycled one step at a time. */
#define RINGTONE_DIR        APP_DATA_PATH("ringtones")
#define RINGTONE_CUSTOM_MAX 8
#define RINGTONE_NAME_MAX   32

/* Defined in zeromesh_map.c so carto headers stay out of this header. */
typedef struct MapState MapState;

typedef enum {
    PendingNone = 0,
    PendingPosReq,
    PendingInfoReq,
    PendingSetLora,
    PendingSetRole,
    PendingSetGps,
    PendingGetChannel,
    PendingSetChannel,
    PendingSetFixed,
    PendingClearFixed,
    PendingReboot,
    PendingInfoAll,
    PendingSendText,
    PendingPlayTone,
} PendingAction;

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

    /* Position as Meshtastic sends it: degrees * 1e7, signed. */
    int32_t latitude_i;
    int32_t longitude_i;
    int32_t altitude;
    uint32_t pos_time;
    bool has_position;
    uint8_t sats;
    bool sats_seen;
    bool has_fix;

    char short_name[8];
    char long_name[24];
    bool has_name;
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
    ZmTransportUart = 0,
    ZmTransportBle,
    ZmTransportCount
} ZmTransport;

typedef enum {
    SettingTransport = 0,
    SettingUart,
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

    ZmTransport transport;
    Bt* bt;
    FuriHalBleProfileBase* ble_profile;
    volatile bool ble_connected;
    bool ble_failed;
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

    uint8_t my_sats;
    bool my_sats_seen;
    bool my_has_fix;
    
    uint32_t my_node_num;

    bool has_node_config;
    uint8_t cfg_region;
    uint8_t cfg_preset;
    uint8_t cfg_role;
    uint8_t cfg_gps;
    uint8_t cfg_fixed;
    uint8_t cfg_ch_private;
    uint8_t cfg_ch_pos;
    uint8_t cfg_ch_psk_len;
    uint8_t cfg_ch_psk[32];
    char cfg_ch_name[16];
    bool cfg_ch_known;
    bool cfg_pos_known;
    meshtastic_Config_PositionConfig cfg_pos;
    char my_long_name[24];
    char my_short_name[8];
    uint8_t nodecfg_cursor;
    bool nodecfg_editing;

    volatile bool need_render;
    volatile bool pending_notify;
    volatile PendingAction pending_action;
    uint32_t pending_node;
    uint8_t pending_a;
    uint8_t pending_b;
    char pending_text[64];
    
    uint32_t sent_msg_ids[8];
    uint8_t sent_msg_head;
    
    uint8_t settings_cursor;
    bool settings_editing;
    
    bool notify_vibro;
    bool notify_led;
    uint16_t notify_ringtone;
    char custom_files[RINGTONE_CUSTOM_MAX][RINGTONE_NAME_MAX];
    uint8_t custom_count;
    
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

    /* Opaque: defined in zeromesh_map.c so carto headers stay out of here. */
    MapState* map;
} ZeroMeshApp;

int32_t zeromesh_serial_app(void* p);
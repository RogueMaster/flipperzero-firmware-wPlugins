#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <bt/bt_service/bt.h>
#include <furi_ble/profile_interface.h>
#include <profiles/serial_profile.h>
#include <string.h>
#include <stdio.h>

#define TAG "JAYX"

#define JAYX_VERSION   "0.3"
#define JAYX_AUTHOR    "Jay"
#define JAYX_PROTO_VER 3
#define JAYX_MAGIC     0x4A58u

#define JAYX_MSG_LIVE          0
#define JAYX_MSG_SPECS_SECTION 1

#define JAYX_LIVE_SIZE          44
#define JAYX_SPECS_SECTION_SIZE 126
#define JAYX_MAX_PACKET         JAYX_SPECS_SECTION_SIZE

#define JAYX_GAME_NAME_LEN 20

#define JAYX_SPEC_SECTIONS_MAX 5
#define JAYX_SPEC_TITLE_LEN    12
#define JAYX_SPEC_LINE_LEN     36
#define JAYX_SPEC_LINES        3

#define JAYX_RX_STREAM_SIZE 1024
#define JAYX_LINK_TIMEOUT_S 5
#define JAYX_FPS_STICKY_S   3

#define JAYX_PAGE_COUNT 4
#define JAYX_DOT_Y      60

/* Specs card layout */
#define JAYX_SPECS_HEADER_H 11

typedef enum {
    JayxUiModeSelect,
    JayxUiWaiting,
    JayxUiActive,
    JayxUiLost,
    JayxUiBtDev, /* Bluetooth not ready yet */
} JayxUiState;

typedef enum {
    JayxTransportNone = 0,
    JayxTransportUsb,
    JayxTransportBt,
} JayxTransport;

typedef enum {
    JayxPageSystem = 0,
    JayxPageGame = 1,
    JayxPageSpecs = 2,
    JayxPageAbout = 3,
} JayxPage;

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t msg_type;
    uint8_t cpu_usage;
    uint8_t cpu_temp_c;
    uint16_t ram_max;
    uint8_t ram_usage;
    char ram_unit[4];
    uint8_t gpu_usage;
    uint8_t gpu_temp_c;
    uint16_t vram_max;
    uint8_t vram_usage;
    char vram_unit[4];
    uint16_t fps;
    char game_name[JAYX_GAME_NAME_LEN];
} JayxLivePacket;

typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t msg_type;
    uint8_t section_index;
    uint8_t section_count;
    char title[JAYX_SPEC_TITLE_LEN];
    char line0[JAYX_SPEC_LINE_LEN];
    char line1[JAYX_SPEC_LINE_LEN];
    char line2[JAYX_SPEC_LINE_LEN];
} JayxSpecsSectionPacket;
#pragma pack(pop)

_Static_assert(sizeof(JayxLivePacket) == JAYX_LIVE_SIZE, "JayxLivePacket size");
_Static_assert(sizeof(JayxSpecsSectionPacket) == JAYX_SPECS_SECTION_SIZE, "specs section size");

typedef struct {
    char title[JAYX_SPEC_TITLE_LEN + 1];
    char line[JAYX_SPEC_LINES][JAYX_SPEC_LINE_LEN + 1];
    bool valid;
} JayxSpecSection;

typedef struct JayxApp JayxApp;

struct JayxApp {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notification;

    JayxUiState ui_state;
    JayxTransport transport;
    JayxTransport select_cursor;
    JayxPage page;

    JayxLivePacket data;

    JayxSpecSection specs[JAYX_SPEC_SECTIONS_MAX];
    uint8_t specs_count;
    bool specs_valid;
    uint8_t specs_scroll;

    uint16_t sticky_fps;
    uint32_t sticky_fps_ts;

    uint8_t rx_scratch[JAYX_MAX_PACKET * 2];
    size_t rx_len;
    FuriStreamBuffer* rx_stream;
    uint32_t last_packet_ts;
    volatile bool rx_pending;

    FuriHalUsbInterface* previous_usb_config;

    Bt* bt;
    FuriHalBleProfileBase* ble_profile;
    bool bt_connected;
};

void jayx_feed_bytes(JayxApp* app, const uint8_t* data, size_t len);
void jayx_process_rx(JayxApp* app);
void jayx_draw_page_dots(Canvas* canvas, JayxPage page);
void jayx_copy_field(char* dst, size_t dst_len, const char* src, size_t src_len);
void jayx_specs_clamp_scroll(JayxApp* app);

void jayx_transport_start(JayxApp* app, JayxTransport t);
void jayx_transport_stop(JayxApp* app);

void draw_connect_view(Canvas* canvas, JayxApp* app);
void draw_system_view(Canvas* canvas, JayxApp* app);
void draw_game_view(Canvas* canvas, JayxApp* app);
void draw_specs_view(Canvas* canvas, JayxApp* app);
void draw_about_view(Canvas* canvas, JayxApp* app);
void draw_status_view(Canvas* canvas, JayxApp* app);

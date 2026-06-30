#pragma once

#include <furi.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CC_UART_BAUD         921600U
#define CC_PROTOCOL_VERSION  1U
#define CC_MAX_PAYLOAD       220U
#define CC_STATUS_PAYLOAD_MAX 96U
#define CC_UNIT_TEXT_LEN     12U

typedef enum {
    CcStatusOk = 0,
    CcStatusBadCmd = 1,
    CcStatusBadArg = 2,
    CcStatusBusErr = 3,
    CcStatusLocked = 4,
    CcStatusNoMem = 5,
    CcStatusCrcFail = 6,
    CcStatusNotActive = 7,
    CcStatusUnknown = 255,
} CcStatusCode;

typedef enum {
    CcBusCan0 = 0,
    CcBusCan1 = 1,
    CcBusBoth = 2,
} CcBus;

typedef enum {
    CcToolNone = 0,
    CcToolReadAll = 1,
    CcToolFiltered = 2,
    CcToolWrite = 3,
    CcToolSpeed = 4,
    CcToolValtrack = 5,
    CcToolUniqueIds = 6,
    CcToolBittrack = 7,
    CcToolReverse = 8,
    CcToolObdPid = 9,
    CcToolDbcDecode = 10,
    CcToolCustomInject = 11,
    CcToolReplay = 12,
} CcToolId;

typedef enum {
    CcToolEvtInfo = 0,
    CcToolEvtDelta = 1,
    CcToolEvtSummary = 2,
    CcToolEvtWarning = 3,
    CcToolEvtError = 4,
} CcToolEventCode;

typedef enum {
    CcEventTypeNone = 0,
    CcEventTypeCanFrame,
    CcEventTypeTool,
    CcEventTypeDbcDecode,
    CcEventTypeStatus,
    CcEventTypeDrops,
    CcEventTypeUnknown,
} CcEventType;

typedef struct {
    uint32_t ts_ms;
    CcBus bus;
    uint32_t id;
    bool ext;
    bool rtr;
    uint8_t dlc;
    uint8_t data[8];
} CcCanFrameEvent;

typedef struct {
    CcToolId tool;
    CcToolEventCode code;
    uint8_t text_len;
    char text[121];
} CcToolEvent;

typedef struct {
    uint16_t sid;
    CcBus bus;
    uint32_t frame_id;
    int64_t raw;
    float value;
    bool in_range;
    char unit[CC_UNIT_TEXT_LEN + 1];
} CcDbcDecodeEvent;

typedef struct {
    uint16_t len;
    uint8_t payload[CC_STATUS_PAYLOAD_MAX];
} CcStatusEvent;

typedef struct {
    uint32_t cli_dropped;
    uint32_t flipper_dropped;
} CcDropsEvent;

typedef struct {
    CcEventType type;
    uint8_t id;
    uint16_t raw_len;
    union {
        CcCanFrameEvent can_frame;
        CcToolEvent tool;
        CcDbcDecodeEvent dbc_decode;
        CcStatusEvent status;
        CcDropsEvent drops;
    } data;
} CcEvent;

typedef struct {
    CcBus bus;
    uint32_t id;
    bool ext;
    uint8_t start_bit;
    uint8_t bit_len;
    bool motorola_order;
    bool signed_value;
    float factor;
    float offset;
    float min;
    float max;
    char unit[CC_UNIT_TEXT_LEN + 1];
    uint16_t sid;
} CcDbcSignalDef;

typedef struct CcClient CcClient;

CcClient* cc_client_alloc(void);
void cc_client_free(CcClient* client);

bool cc_client_open(CcClient* client);
void cc_client_close(CcClient* client);
bool cc_client_is_open(const CcClient* client);

bool cc_client_poll(CcClient* client, uint32_t timeout_ms);
bool cc_client_pop_event(CcClient* client, CcEvent* out_event);

bool cc_client_ping(CcClient* client, CcStatusCode* out_status);
bool cc_client_get_info(CcClient* client, CcStatusCode* out_status);

bool cc_client_bus_set_cfg(
    CcClient* client,
    CcBus bus,
    uint32_t bitrate,
    bool listen_only,
    CcStatusCode* out_status);

bool cc_client_bus_get_cfg(CcClient* client, CcBus bus, CcStatusCode* out_status);

bool cc_client_bus_set_filter(
    CcClient* client,
    CcBus bus,
    uint32_t mask,
    uint32_t filter,
    bool ext_match,
    bool ext,
    CcStatusCode* out_status);

bool cc_client_bus_clear_filter(CcClient* client, CcBus bus, CcStatusCode* out_status);

bool cc_client_send_frame(
    CcClient* client,
    CcBus bus,
    uint32_t id,
    bool ext,
    bool rtr,
    uint8_t dlc,
    const uint8_t data[8],
    CcStatusCode* out_status);

bool cc_client_tool_start(
    CcClient* client,
    CcToolId tool_id,
    const char* args,
    CcStatusCode* out_status);

bool cc_client_tool_stop(CcClient* client, CcStatusCode* out_status);
bool cc_client_tool_status(CcClient* client, CcStatusCode* out_status);

bool cc_client_tool_config(CcClient* client, const char* args, CcStatusCode* out_status);

bool cc_client_dbc_clear(CcClient* client, CcStatusCode* out_status);

bool cc_client_dbc_add_signal(
    CcClient* client,
    const CcDbcSignalDef* def,
    CcStatusCode* out_status);

bool cc_client_dbc_remove_signal(CcClient* client, uint16_t sid, CcStatusCode* out_status);
bool cc_client_dbc_list(CcClient* client, CcStatusCode* out_status);

bool cc_client_stats_get(CcClient* client, CcStatusCode* out_status);
bool cc_client_wifi_get_cfg(CcClient* client, CcStatusCode* out_status);
bool cc_client_wifi_set_cfg(CcClient* client, const char* args, CcStatusCode* out_status);
bool cc_client_led_set_brightness(CcClient* client, uint8_t brightness, CcStatusCode* out_status);

const char* cc_status_to_string(CcStatusCode status);
const char* cc_bus_to_string(CcBus bus);
const char* cc_tool_to_string(CcToolId tool);

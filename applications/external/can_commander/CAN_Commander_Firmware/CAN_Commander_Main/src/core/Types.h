#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "../BoardConfig.h"

namespace core {

enum class BusId : uint8_t {
    CAN0 = 0,
    CAN1 = 1,
    BOTH = 2,
};

enum class InterfaceOwner : uint8_t {
    NONE = 0,
    CLI = 1,
    FLIPPER = 2,
};

enum class Source : uint8_t {
    CLI = 0,
    FLIPPER = 1,
};

enum class ByteOrder : uint8_t {
    INTEL = 0,
    MOTOROLA = 1,
};

enum class SignMode : uint8_t {
    UNSIGNED = 0,
    SIGNED = 1,
};

enum class ToolId : uint8_t {
    NONE = 0,
    READ_ALL = 1,
    FILTERED = 2,
    WRITE = 3,
    SPEED = 4,
    VALTRACK = 5,
    UNIQUE_IDS = 6,
    BITTRACK = 7,
    REVERSE = 8,
    OBD_PID = 9,
    DBC_DECODE = 10,
    CUSTOM_INJECT = 11,
    REPLAY = 12,
};

enum class ToolEventCode : uint8_t {
    INFO = 0,
    DELTA = 1,
    SUMMARY = 2,
    WARNING = 3,
    ERROR = 4,
};

enum class FlipperPacketKind : uint8_t {
    CMD = 0x01,
    RESP = 0x02,
    EVENT = 0x03,
    ERROR = 0x04,
};

enum class FlipperCommandId : uint8_t {
    PING = 0x01,
    GET_INFO = 0x02,
    BUS_SET_CFG = 0x10,
    BUS_GET_CFG = 0x11,
    BUS_SET_FILTER = 0x12,
    BUS_CLEAR_FILTER = 0x13,
    BUS_SEND_FRAME = 0x14,
    TOOL_START = 0x20,
    TOOL_STOP = 0x21,
    TOOL_STATUS = 0x22,
    TOOL_CONFIG = 0x23,
    DBC_CLEAR = 0x30,
    DBC_ADD_SIGNAL = 0x31,
    DBC_REMOVE_SIGNAL = 0x32,
    DBC_LIST = 0x33,
    STATS_GET = 0x40,
    WIFI_GET_CFG = 0x50,
    WIFI_SET_CFG = 0x51,
    LED_SET_CFG = 0x60,
};

enum class FlipperEventId : uint8_t {
    CAN_FRAME_EVT = 0x80,
    TOOL_EVT = 0x81,
    DBC_DECODE_EVT = 0x82,
    STATUS_EVT = 0x83,
    DROPS_EVT = 0x84,
};

struct CanFrame {
    uint32_t ts_ms;
    // Capture time in microseconds (micros() when read from the controller); 0 for
    // frames built by the firmware.
    uint32_t ts_us;
    BusId bus;
    uint32_t id;
    bool ext;
    bool rtr;
    uint8_t dlc;
    uint8_t data[8];
};

struct BusConfig {
    uint32_t bitrate;
    bool listen_only;
};

struct BusFilter {
    bool enabled;
    uint32_t mask;
    uint32_t filter;
    bool require_ext_match;
    bool ext;
};

struct ToolEvent {
    ToolId tool;
    ToolEventCode code;
    char text[128];
};

struct DbcSignalDef {
    uint16_t sid;
    BusId bus;
    uint32_t id;
    bool ext;
    bool mux_enabled;
    uint8_t mux_start_bit;
    uint8_t mux_bit_len;
    uint64_t mux_value;
    uint8_t start_bit;
    uint8_t bit_len;
    ByteOrder order;
    SignMode sign;
    float factor;
    float offset;
    float min;
    float max;
    char unit[12];
    bool used;
};

struct DbcDecodedValue {
    uint16_t sid;
    BusId bus;
    uint32_t frame_id;
    int64_t raw;
    float value;
    bool in_range;
    char unit[12];
};

struct CliLine {
    char line[BoardConfig::CLI_LINE_MAX];
};

struct CliOut {
    char line[BoardConfig::CLI_OUT_MAX];
};

struct FlipperPacket {
    uint8_t ver;
    FlipperPacketKind kind;
    uint8_t id;
    uint8_t seq;
    uint16_t len;
    uint8_t payload[BoardConfig::MAX_FLIPPER_PAYLOAD];
};

struct SinkCounters {
    uint32_t produced = 0;
    uint32_t sent = 0;
    uint32_t dropped = 0;
};

struct EventCounters {
    uint32_t can_frame = 0;
    uint32_t tool_evt = 0;
    uint32_t dbc_evt = 0;
    uint32_t status_evt = 0;
    uint32_t drops_evt = 0;
};

static constexpr uint32_t kMaxStdCanId = 0x7FFUL;
static constexpr uint32_t kMaxExtCanId = 0x1FFFFFFFUL;

inline bool isValidCanId(uint32_t id, bool ext) {
    return ext ? (id <= kMaxExtCanId) : (id <= kMaxStdCanId);
}

inline bool isConcreteBus(BusId bus) {
    return bus == BusId::CAN0 || bus == BusId::CAN1;
}

inline bool isValidBusId(BusId bus) {
    return bus == BusId::CAN0 || bus == BusId::CAN1 || bus == BusId::BOTH;
}

inline const char* busName(BusId bus) {
    switch(bus) {
    case BusId::CAN0:
        return "can0";
    case BusId::CAN1:
        return "can1";
    case BusId::BOTH:
        return "both";
    default:
        return "?";
    }
}

} // namespace core

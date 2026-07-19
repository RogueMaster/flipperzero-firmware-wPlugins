#pragma once

#include <stdint.h>
#include <stddef.h>
#include <furi.h>

#define AKB_MAGIC_0 0xFB
#define AKB_MAGIC_1 0x4B

/** All current frames use a 3-byte payload after magic+len. */
#define AKB_FRAME_PAYLOAD_LEN 3
#define AKB_FRAME_TOTAL_LEN   6

#define AKB_EVENT_KEY_DOWN    0x01
#define AKB_EVENT_KEY_UP      0x02
#define AKB_EVENT_MOUSE_MOVE  0x10
#define AKB_EVENT_MOUSE_DOWN  0x11
#define AKB_EVENT_MOUSE_UP    0x12
#define AKB_EVENT_MOUSE_SCROLL 0x13

typedef enum {
    AkbHidCmdKeyDown,
    AkbHidCmdKeyUp,
    AkbHidCmdMouseMove,
    AkbHidCmdMouseButtonDown,
    AkbHidCmdMouseButtonUp,
    AkbHidCmdMouseScroll,
    AkbHidCmdPanic,
} AkbHidCmdType;

typedef struct {
    AkbHidCmdType type;
    uint8_t mods;
    uint8_t keycode;
    int8_t dx;
    int8_t dy;
    uint8_t mouse_button;
    int8_t scroll;
} AkbHidCmd;

typedef struct {
    uint8_t buffer[256];
    size_t length;
    FuriMessageQueue* hid_queue;
} AkbProtocolParser;

void akb_protocol_init(AkbProtocolParser* parser, FuriMessageQueue* hid_queue);

/** Feed BLE bytes; returns number of accepted frames enqueued for USB. */
size_t akb_protocol_feed(AkbProtocolParser* parser, const uint8_t* data, size_t size);

#pragma once

// Minimal RX-only u-blox UBX parser, by apfxtech

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UBX_SYNC1       0xB5
#define UBX_SYNC2       0x62
#define UBX_NAV_CLASS   0x01
#define UBX_NAV_PVT_ID  0x07
#define UBX_NAV_PVT_LEN 92

typedef enum {
    UboxStateSync1,
    UboxStateSync2,
    UboxStateClass,
    UboxStateId,
    UboxStateLen1,
    UboxStateLen2,
    UboxStatePayload,
    UboxStateCkA,
    UboxStateCkB,
} UboxState;

typedef struct {
    UboxState state;
    uint8_t msg_class;
    uint8_t msg_id;
    uint16_t len;
    uint16_t index;
    uint8_t ck_a;
    uint8_t ck_b;
    uint8_t calc_a;
    uint8_t calc_b;
    uint8_t payload[UBX_NAV_PVT_LEN];
} UboxRx;

typedef struct {
    int32_t lat; /**< Latitude in degrees * 1e7 */
    int32_t lon; /**< Longitude in degrees * 1e7 */
    uint8_t sats; /**< Satellites used in fix */
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    bool valid; /**< gnssFixOK */
} UboxPvt;

/** Reset the parser to its initial state */
void ubox_rx_init(UboxRx* rx);

/**
 * Feed a single byte to the parser.
 *
 * @param rx    Parser state
 * @param byte  Incoming byte
 * @param out   Filled with the decoded fix when a NAV-PVT frame completes
 * @return true when *out was updated with a fresh NAV-PVT
 */
bool ubox_rx_byte(UboxRx* rx, uint8_t byte, UboxPvt* out);

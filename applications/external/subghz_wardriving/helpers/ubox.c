#include "ubox.h"

static inline void ubox_checksum(UboxRx* rx, uint8_t byte) {
    rx->calc_a += byte;
    rx->calc_b += rx->calc_a;
}

static inline uint32_t ubox_get_u32(const uint8_t* p, size_t offset) {
    return (uint32_t)p[offset] | ((uint32_t)p[offset + 1] << 8) | ((uint32_t)p[offset + 2] << 16) |
           ((uint32_t)p[offset + 3] << 24);
}

static inline void ubox_restart(UboxRx* rx) {
    rx->state = UboxStateSync1;
    rx->index = 0;
    rx->calc_a = 0;
    rx->calc_b = 0;
}

void ubox_rx_init(UboxRx* rx) {
    *rx = (UboxRx){0};
}

bool ubox_rx_byte(UboxRx* rx, uint8_t byte, UboxPvt* out) {
    switch(rx->state) {
    case UboxStateSync1:
        if(byte == UBX_SYNC1) {
            ubox_restart(rx);
            rx->state = UboxStateSync2;
        }
        break;

    case UboxStateSync2:
        if(byte == UBX_SYNC2) {
            rx->state = UboxStateClass;
        } else if(byte != UBX_SYNC1) {
            rx->state = UboxStateSync1;
        }
        break;

    case UboxStateClass:
        rx->msg_class = byte;
        ubox_checksum(rx, byte);
        rx->state = UboxStateId;
        break;

    case UboxStateId:
        rx->msg_id = byte;
        ubox_checksum(rx, byte);
        rx->state = UboxStateLen1;
        break;

    case UboxStateLen1:
        rx->len = byte;
        ubox_checksum(rx, byte);
        rx->state = UboxStateLen2;
        break;

    case UboxStateLen2:
        rx->len |= (uint16_t)byte << 8;
        ubox_checksum(rx, byte);
        rx->index = 0;
        rx->state = rx->len ? UboxStatePayload : UboxStateCkA;
        break;

    case UboxStatePayload:
        // Store only what fits; longer messages are still walked past (and
        // checksummed) so the parser stays in sync, then dropped as non-PVT.
        if(rx->index < UBX_NAV_PVT_LEN) rx->payload[rx->index] = byte;
        rx->index++;
        ubox_checksum(rx, byte);
        if(rx->index == rx->len) rx->state = UboxStateCkA;
        break;

    case UboxStateCkA:
        rx->ck_a = byte;
        rx->state = UboxStateCkB;
        break;

    case UboxStateCkB: {
        rx->ck_b = byte;
        bool ok = rx->calc_a == rx->ck_a && rx->calc_b == rx->ck_b;
        rx->state = UboxStateSync1;
        if(ok && rx->msg_class == UBX_NAV_CLASS && rx->msg_id == UBX_NAV_PVT_ID &&
           rx->len == UBX_NAV_PVT_LEN) {
            const uint8_t* p = rx->payload;
            out->hour = p[8];
            out->min = p[9];
            out->sec = p[10];
            out->valid = p[21] & 0x01; // flags: gnssFixOK
            out->sats = p[23];
            out->lon = (int32_t)ubox_get_u32(p, 24);
            out->lat = (int32_t)ubox_get_u32(p, 28);
            return true;
        }
        break;
    }
    }

    return false;
}

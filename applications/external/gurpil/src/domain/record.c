#include "include/domain/record.h"

enum {
    RECORD_MAGIC = 0x42, // ASCII 'B' for "Best distance"
    RECORD_VERSION = 0x01, // Format version 1
};

size_t record_serialize(int32_t best, uint8_t* out, size_t out_len) {
    if(out_len < RECORD_BYTES) {
        return 0;
    }

    // Byte 0: magic marker
    out[0] = RECORD_MAGIC;
    // Byte 1: version
    out[1] = RECORD_VERSION;
    // Bytes 2-3: reserved for future use
    out[2] = 0x00;
    out[3] = 0x00;

    // Bytes 4-7: distance as little-endian int32_t (unsigned cast allows proper bit shifting)
    uint32_t best_u = (uint32_t)best;
    out[4] = (uint8_t)(best_u & 0xFF);
    out[5] = (uint8_t)((best_u >> 8) & 0xFF);
    out[6] = (uint8_t)((best_u >> 16) & 0xFF);
    out[7] = (uint8_t)((best_u >> 24) & 0xFF);

    return RECORD_BYTES;
}

bool record_is_valid(const uint8_t* buf, size_t len) {
    return buf != NULL && len >= RECORD_BYTES && buf[0] == RECORD_MAGIC &&
           buf[1] == RECORD_VERSION;
}

int record_backup_slot(const bool taken[RECORD_BACKUP_SLOTS]) {
    for(int i = 0; i < RECORD_BACKUP_SLOTS; i++) {
        if(!taken[i]) {
            return i;
        }
    }
    return -1;
}

int32_t record_parse(const uint8_t* buf, size_t len) {
    if(!record_is_valid(buf, len)) {
        return 0;
    }

    // Bytes 4-7: distance as little-endian int32_t; cast back to signed is bit-pattern preserving.
    uint32_t distance_u = ((uint32_t)buf[4]) | (((uint32_t)buf[5]) << 8) |
                          (((uint32_t)buf[6]) << 16) | (((uint32_t)buf[7]) << 24);
    return (int32_t)distance_u;
}

int32_t record_update(int32_t prev_best, int32_t distance) {
    // A negative distance never lowers the record.
    if(distance < 0) {
        distance = prev_best;
    }
    return distance > prev_best ? distance : prev_best;
}

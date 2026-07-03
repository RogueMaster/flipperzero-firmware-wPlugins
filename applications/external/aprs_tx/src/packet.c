#include "packet.h"

#include <string.h>

static uint16_t packet_crc(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    uint16_t i;
    uint8_t j;

    for(i = 0; i < len; i++) {
        crc ^= data[i];
        for(j = 0; j < 8; j++) {
            if(crc & 1)
                crc = (crc >> 1) ^ 0x8408;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFF;
}

static void packet_push_byte_bits(uint8_t* out, uint16_t* n, uint8_t v) {
    uint8_t i;

    for(i = 0; i < 8; i++) {
        out[*n] = (v >> i) & 1;
        (*n)++;
    }
}

void packet_init(Packet* p) {
    memset(p, 0, sizeof(Packet));
}

void packet_add_fcs(Packet* p) {
    uint16_t crc;

    memcpy(p->fcs, p->ax25, p->ax25_len);
    p->fcs_len = p->ax25_len;

    crc = packet_crc(p->ax25, p->ax25_len);
    p->fcs[p->fcs_len++] = crc & 255;

    p->fcs[p->fcs_len++] = crc >> 8;
}

void packet_stuff(Packet* p) {
    uint16_t i;
    uint16_t n = 0;
    uint8_t j;
    uint8_t c = 0;

    uint8_t bit;

    packet_push_byte_bits(p->stuffed, &n, 0x7E);

    for(i = 0; i < p->fcs_len; i++) {
        for(j = 0; j < 8; j++) {
            bit = (p->fcs[i] >> j) & 1;
            p->stuffed[n++] = bit;
            if(bit) {
                c++;
                if(c == 5) {
                    p->stuffed[n++] = 0;
                    c = 0;
                }
            } else {
                c = 0;
            }
        }
    }

    packet_push_byte_bits(p->stuffed, &n, 0x7E);
    p->stuffed_len = n;
}

void packet_nrzi(Packet* p) {
    uint16_t i;
    uint8_t level = 1;

    p->nrzi_len = 0;

    for(i = 0; i < p->stuffed_len; i++) {
        if(p->stuffed[i] == 0) level ^= 1;
        p->nrzi[p->nrzi_len++] = level;
    }
}

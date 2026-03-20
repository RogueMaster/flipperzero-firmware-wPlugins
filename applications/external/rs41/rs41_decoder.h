#pragma once
#include <stdint.h>
#include <stdbool.h>

#define RS41_PAYLOAD_LEN 312  /* bytes after the 8-byte on-air sync word */

typedef struct {
    char    id[9];      /* 8-char sonde serial (e.g. "S3650123") + NUL */
    float   lat;        /* degrees, N > 0 */
    float   lon;        /* degrees, E > 0 */
    float   alt;        /* metres MSL */
    uint8_t sats;       /* GPS satellites tracked */
    float   temp;       /* °C;  -999.0f = not decoded */
    float   pres;       /* hPa; -1.0f   = not decoded */
    int16_t rssi;       /* dBm */
    bool    has_gps;    /* lat / lon / alt valid */
} Rs41Frame;

/*
 * Decode one RS41 payload received from the CC1101 FIFO.
 *
 * fifo_bytes : RS41_PAYLOAD_LEN (312) bytes taken directly from the CC1101
 *              RX FIFO *after* the CC1101's 2-byte sync word.  The first 6
 *              bytes are the remaining sync (0x53 88 44 69 48 1F in bit-
 *              reversed form) and must be skipped by the caller before passing
 *              the pointer here.  The bytes are still in CC1101 MSB-first
 *              order; this function applies the BITREV table internally.
 * rssi       : RSSI in dBm (from furi_hal_subghz_get_rssi()).
 * out        : filled on success.
 * returns    : true if at least the STATUS block (sonde ID) was decoded.
 */
bool rs41_decode(const uint8_t* fifo_bytes, int16_t rssi, Rs41Frame* out);

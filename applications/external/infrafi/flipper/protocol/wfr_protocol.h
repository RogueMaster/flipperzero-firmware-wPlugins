#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * InfraFi Protocol — RC-6 Scancode Encoding
 *
 * Uses standard RC-6 IR messages (decoded by the kernel) instead of raw timings.
 * Each RC-6 message carries 1 byte of payload via: address (framing) + command (data).
 * Payload is a WiFi QR string: WIFI:T:<type>;S:<ssid>;P:<pass>;H:<hidden>;;
 *
 * Address byte layout:
 *   Bits 7-4: Magic = 0xA (identifies InfraFi messages)
 *   Bits 3-2: Frame type (00=START, 01=DATA, 10=END, 11=ACK)
 *   Bits 1-0: Pass number (0-3, which retransmission attempt)
 *
 * Messages per transmission (Flipper → daemon):
 *   START (len=N) → DATA × N (one byte each) → END (crc8)
 *
 * ACK response (daemon → Flipper):
 *   Same framing. Payload is "OK:<ip>" on success or "FAIL" on failure.
 */

/* RC-6 address byte encoding */
#define WFR_RC6_MAGIC        0xA0
#define WFR_RC6_MAGIC_MASK   0xF0
#define WFR_RC6_TYPE_MASK    0x0C
#define WFR_RC6_PASS_MASK    0x03

#define WFR_RC6_TYPE_START   0x00
#define WFR_RC6_TYPE_DATA    0x04
#define WFR_RC6_TYPE_END     0x08

/* Frame type for ACK responses (daemon → Flipper) */
#define WFR_RC6_TYPE_ACK     0x0C

/* Timing */
#define WFR_RC6_INTER_MSG_MS      20   /* Delay between RC-6 messages (ms) */
#define WFR_RC6_RETRANSMIT_GAP_MS 200  /* Gap between retransmission passes */
#define WFR_RETRANSMIT_COUNT      1

/* ACK timeout — how long Flipper waits for a response (seconds) */
#define WFR_ACK_TIMEOUT_SEC       30

/* ACK payload prefixes */
#define WFR_ACK_PREFIX_OK   "OK:"
#define WFR_ACK_PREFIX_FAIL "FAIL"

/* Protocol limits */
#define WFR_MAX_TOTAL_PAYLOAD 255

/* WiFi credential limits */
#define WFR_SSID_MAX_LEN 32
#define WFR_PASS_MAX_LEN 63

/* Security types */
#define WFR_SEC_OPEN 0
#define WFR_SEC_WPA  1
#define WFR_SEC_WEP  2
#define WFR_SEC_SAE  3

/* WiFi credentials struct — shared between Flipper and daemon */
typedef struct {
    char ssid[WFR_SSID_MAX_LEN + 1];
    char password[WFR_PASS_MAX_LEN + 1];
    uint8_t security;
    bool hidden;
} WfrWifiCreds;

/*
 * CRC-8/CCITT (poly 0x07, init 0x00)
 * Used for payload integrity check.
 */
uint8_t wfr_crc8(const uint8_t* data, size_t len);

/*
 * Build a WiFi QR string from credentials.
 * Returns number of bytes written (excluding null terminator), or 0 on error.
 */
size_t wfr_build_wifi_string(const WfrWifiCreds* creds, char* out, size_t out_size);

/*
 * Parse a WiFi QR string into credentials.
 * Returns true on success, false on parse error.
 */
bool wfr_parse_wifi_string(const char* str, WfrWifiCreds* creds);

#ifdef __cplusplus
}
#endif

#pragma once

// Self-contained USB CCID smart-card reader gadget (WUDF-compliant).
//
// The firmware's stock `usb_ccid` interface advertises only 2 endpoints (no
// interrupt IN) and dwProtocols = T=0 only. Windows' WUDF usbccid driver
// refuses to start it ("Code 10"), and it can't present a T=1 card. This
// module defines its own FuriHalUsbInterface with the interrupt endpoint added
// and dwProtocols = T=0|T=1, then bridges host APDUs to the SAM via callbacks.

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

// Largest card response we return. Sized to the SAM's full frame (Seader's
// UART buffer) so large key-scan replies (>261 B) aren't truncated.
#define SEADER_CCID_MAX_RESP 272u
// Max CCID message length (advertised as dwMaxCCIDMessageLength), both
// directions = 10-byte header + response body.
#define SEADER_CCID_MSG_MAX  (10u + SEADER_CCID_MAX_RESP)

typedef struct {
    uint16_t vid;
    uint16_t pid;
    const char* manuf;
    const char* product;

    // Fill `atr` (<= 33 bytes) and set *atr_len. Called on ICC power-on.
    void (*get_atr)(void* ctx, uint8_t* atr, uint16_t* atr_len);

    // Relay one APDU to the SAM. Must always fill `resp`/`*resp_len` (e.g. 6F00
    // on failure) and may block until the SAM answers. Returns true on success.
    bool (
        *xfr)(void* ctx, const uint8_t* apdu, uint16_t apdu_len, uint8_t* resp, uint16_t* resp_len);

    void* ctx;
} SeaderCcidReaderConfig;

// Bring the USB CCID reader up (saves + replaces the current USB config) and
// tear it down (restores the previous USB config). Not re-entrant; one at a time.
void seader_usb_ccid_reader_start(const SeaderCcidReaderConfig* cfg);
void seader_usb_ccid_reader_stop(void);

// Diagnostics: last CCID command type received + total command count.
void seader_usb_ccid_reader_stats(uint8_t* last_cmd, uint32_t* count);
const char* seader_usb_ccid_cmd_name(uint8_t type);

// Deeper diagnostics: ATR length returned on last PowerOn, and the return value
// of the last bulk-IN write (>=0 bytes written, <0 = write failed).
void seader_usb_ccid_reader_debug(uint16_t* atr_len, int32_t* tx_last);

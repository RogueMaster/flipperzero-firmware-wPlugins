#pragma once

#include "uscuid_ul_poller.h"
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USCUID_UL_POLLER_MAX_BUFFER_SIZE (64U)
#define USCUID_UL_POLLER_MAX_FWT         (60000U)

// Magic wakeup, raw frames like a real Gen1A chip (see gen1a_poller).
#define USCUID_UL_WUPA_A (0x40) // 7-bit
#define USCUID_UL_DATA_A (0x43)
#define USCUID_UL_WUPA_B (0x20) // 7-bit
#define USCUID_UL_DATA_B (0x23)
#define USCUID_UL_ACK    (0x0A) // 4-bit

// Commands available once in backdoor mode (same opcodes as plain UL).
#define USCUID_UL_CMD_WRITE      (0xA2) // A2 <page> <4 bytes> -> ACK
#define USCUID_UL_CMD_PWD_AUTH   (0x1B) // 1B <pwd 4 bytes>    -> 2-byte PACK
#define USCUID_UL_PACK_SIZE      (2) // PWD-AUTH success response (PACK) length
// Read configuration. Doc lists E0 50; the identify example shows E1 00 -- which is
// canonical needs confirming on hardware (TMD-5S). Kept here so it is a one-line change.
#define USCUID_UL_CMD_READ_CFG_0 (0xE0)
#define USCUID_UL_CMD_READ_CFG_1 (0x50)

#define USCUID_UL_CONFIG_MAGIC   (0x85) // config[0] in factory ("85") mode
// When the gen1a magic backdoor is enabled, config[0..1] become 7A FF instead of 85 00.
// Only those two bytes change; preset/version stay at the same offsets, so no re-alignment.
#define USCUID_UL_CFG_GEN1A_ON_0 (0x7A)
#define USCUID_UL_CFG_GEN1A_ON_1 (0xFF)
#define USCUID_UL_CFG_AUTH       (4) // 00=PWD, 0A=2TDEA(UL-C); 0xAA marks a UL-Y at preset 5A
#define USCUID_UL_AUTH_UL_Y      (0xAA)
#define USCUID_UL_CFG_PRESET     (7) // C3=UL11 3C=UL21 A5=NTAG213 5A=NTAG215 AA=NTAG216 00=UL-C
#define USCUID_UL_CFG_VENDOR     (9) // version vendor byte: 04=NXP, 34=Mikron (Ultra)
#define USCUID_UL_VENDOR_MIKRON  (0x34)

#define USCUID_UL_PRESET_UL11    (0xC3)
#define USCUID_UL_PRESET_UL21    (0x3C)
#define USCUID_UL_PRESET_ULC     (0x00)
#define USCUID_UL_PRESET_NTAG213 (0xA5)
#define USCUID_UL_PRESET_NTAG215 (0x5A)
#define USCUID_UL_PRESET_NTAG216 (0xAA)

typedef enum {
    UscuidUlPollerStateIdle,
    UscuidUlPollerStateRequestMode,
    UscuidUlPollerStateRequestDataToWrite,
    UscuidUlPollerStateWrite,
    UscuidUlPollerStateSuccess,
    UscuidUlPollerStatePartial, // finished, but some soft (config/lock) pages didn't take
    UscuidUlPollerStateFail,
    UscuidUlPollerStateAuthFailed, // PWD-AUTH rejected before any write

    UscuidUlPollerStateNum,
} UscuidUlPollerState;

typedef enum {
    UscuidUlPollerSessionStateIdle,
    UscuidUlPollerSessionStateStarted,
    UscuidUlPollerSessionStateStopRequest,
} UscuidUlPollerSessionState;

struct UscuidUlPoller {
    Nfc* nfc;
    UscuidUlPollerState state;
    UscuidUlPollerSessionState session_state;

    // Write transport: wakeup == None selects the direct (normal-activation + A2) engine
    // for CUID/ATS-mode tags; A/B selects the backdoor (raw 40/43 wakeup) engine.
    UscuidUlWakeup wakeup;
    UscuidUlPollerMode mode; // page-order strategy (Write = clone, block 0 last; Wipe = ascending)
    NfcPoller* iso3_nfc_poller; // owned; allocated only for the direct engine
    Iso14443_3aPoller* iso3_poller; // borrowed from the iso3 poller event (direct engine)
    const MfUltralightData* data; // source dump, not owned (stable for the session)
    uint16_t pages_total; // pages to write (from the dump)
    uint16_t write_index; // 0-based write position (order: see uscuid_ul_poller_page_for_index)
    uint16_t written; // pages the tag ACKed (ACK/NAK only, no read-back)
    // Best-effort clone bookkeeping: pages are written in order; a page the tag NAKs is logged
    // and its bit set here, never aborts (reported as a Partial result).
    uint16_t failed_count;
    uint8_t failed_bitmap[USCUID_UL_FAILED_BITMAP_SIZE];

    // Optional PWD-AUTH (direct engine only). authed clears on each (re-)activation so we
    // re-auth after a reset; auth_ever_ok gates the no-retry rule (a wrong initial password
    // must abort, not loop, to avoid bricking the tag via AUTHLIM).
    bool password_set;
    uint8_t password[USCUID_UL_PWD_SIZE];
    bool authed;
    bool auth_ever_ok;

    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    UscuidUlPollerEvent event;
    UscuidUlPollerEventData event_data;

    UscuidUlPollerCallback callback;
    void* context;
};

// True when the write transport is the direct (no-backdoor) iso3 engine.
bool uscuid_ul_is_direct(const UscuidUlPoller* instance);

// Low-level primitives (raw nfc_poller_trx + manual CRC, like gen1a_poller).
UscuidUlPollerError uscuid_ul_poller_wakeup(UscuidUlPoller* instance, UscuidUlWakeup variant);
UscuidUlPollerError uscuid_ul_poller_read_config(UscuidUlPoller* instance, uint8_t* config);
UscuidUlPollerError
    uscuid_ul_poller_write_page(UscuidUlPoller* instance, uint8_t page, const uint8_t* data);
// PWD-AUTH on the direct (iso3) engine: 1B <pwd> -> 2-byte PACK. None = accepted.
UscuidUlPollerError uscuid_ul_poller_auth_pwd(UscuidUlPoller* instance);

// Maps the 0-based write position to the actual page for the given order strategy:
// Write = "pages 4..total-1 ascending, then 3,2,1,0" (block 0 last); Wipe = plain ascending 0..N.
uint8_t
    uscuid_ul_poller_page_for_index(UscuidUlPollerMode mode, uint16_t index, uint16_t pages_total);

// True when the 16-byte config page looks like a USCUID-UL config: factory "85" framing,
// or the "7A FF" gen1a-backdoor-enabled framing (rest of the layout unchanged).
bool uscuid_ul_config_is_magic(const uint8_t* config);

// Fills type/flags from a 16-byte config page (config[0] is 0x85, or 0x7A 0xFF when the
// magic backdoor is enabled — either way preset/version sit at the same offsets).
void uscuid_ul_classify(const uint8_t* config, UscuidUlData* data);

#ifdef __cplusplus
}
#endif

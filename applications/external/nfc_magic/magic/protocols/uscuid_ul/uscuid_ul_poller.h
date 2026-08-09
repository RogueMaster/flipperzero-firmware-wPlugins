#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USCUID_UL_CONFIG_SIZE        (16)
// Failed pages are tracked as a bitmap (1 bit/page) so a Partial result can list every page
// that didn't take, in ascending order. Sized for the largest supported type (NTAG216, 231 pg).
#define USCUID_UL_MAX_PAGES          (231)
#define USCUID_UL_FAILED_BITMAP_SIZE ((USCUID_UL_MAX_PAGES + 7) / 8)

typedef enum {
    UscuidUlPollerErrorNone,
    UscuidUlPollerErrorTimeout,
    UscuidUlPollerErrorNotPresent,
    UscuidUlPollerErrorProtocol,
} UscuidUlPollerError;

// Which magic backdoor wakeup the card answered. The poller probes A (40/43) then B
// (20/23) and records whichever ACKs; it does NOT read a wakeup-selector byte from config
// (detection happens before config is read). None == no backdoor answered.
typedef enum {
    UscuidUlWakeupNone,
    UscuidUlWakeupA,
    UscuidUlWakeupB,
} UscuidUlWakeup;

// Result of non-destructive detection / variation check.
typedef struct {
    bool is_uscuid_ul; // confirmed: config matched the magic framing (0x85, or 0x7A 0xFF backdoor-on)
    uint8_t config[USCUID_UL_CONFIG_SIZE];
    UscuidUlWakeup wakeup; // backdoor entry that answered (or None)
    MfUltralightType type; // preset byte cfg[7] (cfg[9] refines UL21 -> Ultra). Valid ONLY when
        // type_known; left at Origin otherwise -- gate reads of type/is_ultra on type_known.
    bool type_known; // preset byte recognised
    bool is_ultra; // UL21 + Mikron vendor (cfg[9]==0x34) -> display "UL21 (Ultra)"
    bool maybe_ul5; // unpersonalized UL-5 hint (UID prefix AA 55); detect-only, never writable
} UscuidUlData;

// Page-write order strategy (shares the whole write engine; only the order differs).
typedef enum {
    UscuidUlPollerModeWrite, // clone a dump: data pages 4..N first, block 0 (UID) last
    UscuidUlPollerModeWipe, // factory reset: ascending 0..N (clears static locks before locked pages)
} UscuidUlPollerMode;

typedef enum {
    UscuidUlPollerEventTypeDetected,
    UscuidUlPollerEventTypeRequestMode,
    UscuidUlPollerEventTypeRequestDataToWrite,
    UscuidUlPollerEventTypeWriteProgress,
    UscuidUlPollerEventTypeSuccess,
    UscuidUlPollerEventTypePartial, // some pages wrote, >=1 did not (failed_bitmap; may incl. UID)
    UscuidUlPollerEventTypeFail,
    UscuidUlPollerEventTypeAuthFailed, // PWD-AUTH was rejected; nothing was written
} UscuidUlPollerEventType;

#define USCUID_UL_PWD_SIZE (4) // MFUL/NTAG password length

typedef struct {
    UscuidUlPollerMode mode;
} UscuidUlPollerEventDataRequestMode;

typedef struct {
    const MfUltralightData* data; // source dump (not owned; lives in the app device)
} UscuidUlPollerEventDataRequestDataToWrite;

typedef struct {
    uint16_t pages_written;
    uint16_t pages_total;
} UscuidUlPollerEventDataWriteProgress;

typedef struct {
    uint16_t pages_written; // pages successfully written before the failure
    uint16_t pages_total; // pages that were to be written
} UscuidUlPollerEventDataFail;

// Soft-failure result: some pages were written and one or more were not. failed_bitmap lists the
// pages that did not take -- usually config/lock pages (e.g. PACK), but since clone mode writes the
// UID page LAST it can also be the UID if the tag NAKs it. The clone may be usable but isn't
// guaranteed complete.
typedef struct {
    uint16_t pages_written; // pages successfully written
    uint16_t pages_total; // pages that were to be written
    uint16_t failed_count; // number of pages that didn't take
    uint8_t failed_bitmap[USCUID_UL_FAILED_BITMAP_SIZE]; // bit N set = page N failed
} UscuidUlPollerEventDataPartial;

typedef union {
    UscuidUlPollerEventDataRequestMode poller_mode;
    UscuidUlPollerEventDataRequestDataToWrite data_to_write;
    UscuidUlPollerEventDataWriteProgress write_progress;
    UscuidUlPollerEventDataFail fail;
    UscuidUlPollerEventDataPartial partial;
} UscuidUlPollerEventData;

typedef struct {
    UscuidUlPollerEventType type;
    UscuidUlPollerEventData* data;
} UscuidUlPollerEvent;

typedef NfcCommand (*UscuidUlPollerCallback)(UscuidUlPollerEvent event, void* context);

typedef struct UscuidUlPoller UscuidUlPoller;

// Non-destructive: tries magic wakeup (A then B) + config read, classifies the variant,
// and sets the AA-55 UL-5 hint. Returns None when a USCUID-UL is confirmed.
UscuidUlPollerError uscuid_ul_poller_detect(Nfc* nfc, UscuidUlData* data);

// UI label for the variant: "UL11", "UL21", "UL21 (Ultra)", "NTAG213/215/216",
// "UL-C (write N/A)", or "Unknown". Returns a static string.
const char* uscuid_ul_get_variant_name(const UscuidUlData* data);

// Compact name for a plain Ultralight type: "UL11"/"UL21"/"NTAG213/5/6"/"UL-C"/"Unknown".
const char* uscuid_ul_type_name(MfUltralightType type);

// True when this detected tag can be written: a confirmed USCUID-UL with a recognised,
// supported type. UL-C is display-only ("write N/A"); an unrecognised preset is not writable.
bool uscuid_ul_data_is_writable(const UscuidUlData* data);

UscuidUlPoller* uscuid_ul_poller_alloc(Nfc* nfc);
void uscuid_ul_poller_free(UscuidUlPoller* instance);

// Selects the write transport before start: pass the wakeup recorded during detection.
// None -> direct (normal activation + A2, for CUID/ATS tags); A/B -> backdoor (40/43-20/23).
void uscuid_ul_poller_set_wakeup(UscuidUlPoller* instance, UscuidUlWakeup wakeup);

// Enables PWD-AUTH before writes (direct engine only): the 4-byte password is sent (0x1B)
// after activation, so protected pages on a genuine/ATS tag become writable. A rejected
// password aborts with AuthFailed without retrying (retries can brick the tag via AUTHLIM).
void uscuid_ul_poller_set_password(UscuidUlPoller* instance, const uint8_t* password);
void uscuid_ul_poller_start(
    UscuidUlPoller* instance,
    UscuidUlPollerCallback callback,
    void* context);
void uscuid_ul_poller_stop(UscuidUlPoller* instance);

#ifdef __cplusplus
}
#endif

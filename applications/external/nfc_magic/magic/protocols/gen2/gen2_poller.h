#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/nfc_device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    Gen2PollerErrorNone,
    Gen2PollerErrorNotPresent,
    Gen2PollerErrorProtocol,
    Gen2PollerErrorAuth,
    Gen2PollerErrorTimeout,
    Gen2PollerErrorAccess,
} Gen2PollerError;

// Gen2 sub-classification, refined from coarse to specific.
typedef enum {
    Gen2TypeUnknown, // Not a Gen2 card, or Gen2 not yet classified
    Gen2TypeAts, // Matched a known magic-CUID ATS fingerprint; these products are
        // known to be CUID, so the write probe is skipped ("CUID. ATS")
    Gen2TypeCuid, // Block 0 directly writable (CUID), confirmed by write probe
    Gen2TypeCuidStaticNonce, // CUID that also returns a static (non-random) nonce
} Gen2Type;

// Possible write problems, sorted by priority top to bottom
typedef union {
    uint8_t all_problems;
    struct {
        bool uid_locked : 1; // UID may be non-rewritable. Check data after writing
        bool no_data    : 1; // Shouldn't happen, mfc_data missing in nfc device
        bool locked_access_bits : 1; // Access bits on the target card don't allow writing in some cases
        bool missing_target_keys : 1; // Keys to write some sectors are not available
        bool missing_source_data : 1; // The source dump is incomplete
    };
} Gen2PollerWriteProblems;

#define GEN2_POLLER_WRITE_PROBLEMS_LEN (5)

extern const char* const gen2_problem_strings[];

// Wipe/clone outcome is tracked per block, so the "not done" list is exact -- e.g. only block 0 of
// a genuine MFC (read-only manufacturer block), not its whole sector. 4K has the most blocks (256).
#define GEN2_POLLER_MAX_BLOCKS        (256)
#define GEN2_POLLER_BLOCK_BITMAP_SIZE ((GEN2_POLLER_MAX_BLOCKS + 7) / 8)
_Static_assert(
    GEN2_POLLER_MAX_BLOCKS <= GEN2_POLLER_BLOCK_BITMAP_SIZE * 8,
    "Gen2 block bitmap too small for max blocks");

typedef enum {
    Gen2PollerEventTypeDetected,
    Gen2PollerEventTypeRequestMode,
    Gen2PollerEventTypeRequestDataToWrite,
    Gen2PollerEventTypeRequestTargetData,

    Gen2PollerEventTypeSuccess,
    Gen2PollerEventTypePartial, // wipe/clone done except blocks that couldn't be written
    Gen2PollerEventTypeFail,
} Gen2PollerEventType;

typedef enum {
    Gen2PollerModeWipe,
    Gen2PollerModeWrite,
} Gen2PollerMode;

typedef struct {
    Gen2PollerMode mode;
} Gen2PollerEventDataRequestMode;

typedef struct {
    const MfClassicData* mfc_data;
} Gen2PollerEventDataRequestDataToWrite;

typedef struct {
    const MfClassicData* mfc_data;
} Gen2PollerEventDataRequestTargetData;

// Wipe/clone soft-failure: every block was written/cleared except those that couldn't be (no key,
// or a read-only block). failed_bitmap bit N set = block N could not be written.
// Invariant: 0 < failed_count < blocks_total -- the all-failed and none-failed cases are reported
// as Fail/Success, never Partial (so a blocks_total - failed_count subtraction never underflows).
typedef struct {
    uint16_t blocks_total;
    uint16_t failed_count;
    uint8_t failed_bitmap[GEN2_POLLER_BLOCK_BITMAP_SIZE];
} Gen2PollerEventDataPartial;

typedef union {
    Gen2PollerEventDataRequestMode poller_mode;
    Gen2PollerEventDataRequestDataToWrite data_to_write;
    Gen2PollerEventDataRequestTargetData target_data;
    Gen2PollerEventDataPartial partial;
} Gen2PollerEventData;

typedef struct {
    Gen2PollerEventType type;
    Gen2PollerEventData* data;
} Gen2PollerEvent;

typedef NfcCommand (*Gen2PollerCallback)(Gen2PollerEvent event, void* context);

typedef struct Gen2Poller Gen2Poller;

Gen2PollerError gen2_poller_detect(Nfc* nfc);

// Detects whether the card is Gen2 and classifies its sub-type (ATS / CUID /
// CUID with static nonce). Returns Gen2PollerErrorNone when any Gen2 type is found.
Gen2PollerError gen2_poller_detect_type(Nfc* nfc, Gen2Type* type);

// Gen2 sub-type detail shown under the "Gen 2" type line (e.g. "CUID. Static nonce"),
// or NULL if none.
const char* gen2_type_get_detail(Gen2Type type);

Gen2Poller* gen2_poller_alloc(Nfc* nfc);

void gen2_poller_free(Gen2Poller* instance);

void gen2_poller_start(Gen2Poller* instance, Gen2PollerCallback callback, void* context);

void gen2_poller_stop(Gen2Poller* instance);

Gen2PollerWriteProblems gen2_poller_check_target_problems(NfcDevice* target_dev);

Gen2PollerWriteProblems gen2_poller_check_source_problems(NfcDevice* source_dev);

#ifdef __cplusplus
}
#endif

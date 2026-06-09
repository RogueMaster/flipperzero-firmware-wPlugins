#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    Gen1aPollerEventTypeDetected,
    Gen1aPollerEventTypeRequestMode,
    Gen1aPollerEventTypeRequestDataToWrite,
    Gen1aPollerEventTypeRequestDataToDump,

    Gen1aPollerEventTypeSuccess,
    Gen1aPollerEventTypeFail,
} Gen1aPollerEventType;

typedef enum {
    Gen1aPollerModeWipe,
    Gen1aPollerModeDump,
    Gen1aPollerModeWrite,
} Gen1aPollerMode;

typedef struct {
    Gen1aPollerMode mode;
} Gen1aPollerEventDataRequestMode;

typedef struct {
    const MfClassicData* mfc_data;
} Gen1aPollerEventDataRequestDataToWrite;

typedef struct {
    MfClassicData* mfc_data;
} Gen1aPollerEventDataRequestDataToDump;

typedef union {
    Gen1aPollerEventDataRequestMode request_mode;
    Gen1aPollerEventDataRequestDataToWrite data_to_write;
    Gen1aPollerEventDataRequestDataToDump data_to_dump;
} Gen1aPollerEventData;

typedef struct {
    Gen1aPollerEventType type;
    Gen1aPollerEventData* data;
} Gen1aPollerEvent;

typedef NfcCommand (*Gen1aPollerCallback)(Gen1aPollerEvent event, void* context);

typedef struct Gen1aPoller Gen1aPoller;

bool gen1a_poller_detect(Nfc* nfc);

Gen1aPoller* gen1a_poller_alloc(Nfc* nfc);

// Sets the target UID length (4 or 7) for wipe: controls the block-0 layout and the
// all-zero UID written. Values other than 7 are treated as 4.
void gen1a_poller_set_uid_len(Gen1aPoller* instance, uint8_t uid_len);

void gen1a_poller_free(Gen1aPoller* instance);

void gen1a_poller_start(Gen1aPoller* instance, Gen1aPollerCallback callback, void* context);

void gen1a_poller_stop(Gen1aPoller* instance);

#ifdef __cplusplus
}
#endif

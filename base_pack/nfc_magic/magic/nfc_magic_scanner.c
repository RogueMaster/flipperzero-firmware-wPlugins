#include "nfc_magic_scanner.h"

#include "core/check.h"
#include "protocols/gen4/gen4.h"
#include "protocols/gen1a/gen1a_poller.h"
#include "protocols/gen2/gen2_poller.h"
#include "protocols/gen4/gen4_poller.h"
#include "protocols/uscuid_ul/uscuid_ul_poller.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>

#include <furi/furi.h>

#define TAG "NfcMagicScanner"

typedef enum {
    NfcMagicScannerSessionStateIdle,
    NfcMagicScannerSessionStateActive,
    NfcMagicScannerSessionStateStopRequest,
} NfcMagicScannerSessionState;

struct NfcMagicScanner {
    Nfc* nfc;
    NfcMagicScannerSessionState session_state;

    Gen4Password gen4_password;
    Gen4* gen4_data;
    Gen2Type gen2_type;
    uint8_t gen1_uid_len;
    UscuidUlData uscuid_ul_data;

    NfcMagicScannerCallback callback;
    void* context;

    FuriThread* scan_worker;
};

static const NfcProtocol nfc_magic_scanner_not_magic_protocols[] = {
    NfcProtocolIso14443_3b,
    // NfcProtocolIso15693_3 is intentionally absent. ISO15693 (NfcV) tags are handled as
    // magic ISO15693 candidates in nfc_magic_scanner_detect_pass().
    NfcProtocolFelica,
};

static void nfc_magic_scanner_reset(NfcMagicScanner* instance) {
    instance->session_state = NfcMagicScannerSessionStateIdle;
    instance->gen2_type = Gen2TypeUnknown;
    instance->gen1_uid_len = 0;
    memset(&instance->uscuid_ul_data, 0, sizeof(UscuidUlData));
}

NfcMagicScanner* nfc_magic_scanner_alloc(Nfc* nfc) {
    furi_assert(nfc);

    NfcMagicScanner* instance = malloc(sizeof(NfcMagicScanner));
    instance->nfc = nfc;
    instance->gen4_data = gen4_alloc();

    return instance;
}

void nfc_magic_scanner_free(NfcMagicScanner* instance) {
    furi_assert(instance);

    gen4_free(instance->gen4_data);
    free(instance);
}

void nfc_magic_scanner_set_gen4_password(NfcMagicScanner* instance, Gen4Password password) {
    furi_assert(instance);

    instance->gen4_password = password;
}

#define NFC_MAGIC_SAK_ULTRALIGHT (0x00)

// One ISO14443-3A identity read via a standard activation. SAK splits the magic families
// (Ultralight/NTAG answer SAK 0x00, MIFARE Classic does not) BEFORE any backdoor frame,
// and the UID length (4/7) tells 4- vs 7-byte Gen1 tags apart (the wakeup is UID-agnostic).
typedef struct {
    bool activated;
    uint8_t sak;
    uint8_t uid_len; // 4 or 7, else 0 ("unknown")
    uint8_t uid0; // first two UID bytes, for the unpersonalized UL-5 (AA 55) heuristic
    uint8_t uid1;
} NfcMagicScannerIdentity;

static NfcMagicScannerIdentity nfc_magic_scanner_read_identity(Nfc* nfc) {
    NfcMagicScannerIdentity id = {.activated = false, .sak = 0, .uid_len = 0};

    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    if(nfc_poller_detect(poller)) {
        const Iso14443_3aData* data = nfc_poller_get_data(poller);
        id.activated = true;
        id.sak = data->sak;
        id.uid_len = (data->uid_len == 4 || data->uid_len == 7) ? data->uid_len : 0;
        if(data->uid_len >= 2) {
            id.uid0 = data->uid[0];
            id.uid1 = data->uid[1];
        }
    }
    nfc_poller_free(poller);

    return id;
}

static bool nfc_magic_scanner_detect_gen4(NfcMagicScanner* instance) {
    gen4_reset(instance->gen4_data);
    Gen4 gen4_data;
    Gen4PollerError error = gen4_poller_detect(instance->nfc, instance->gen4_password, &gen4_data);
    if(error == Gen4PollerErrorNone) {
        gen4_copy(instance->gen4_data, &gen4_data);
        return true;
    }
    return false;
}

static bool nfc_magic_scanner_detect_mf_classic(Nfc* nfc) {
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolMfClassic);
    bool detected = nfc_poller_detect(poller);
    nfc_poller_free(poller);
    return detected;
}

static bool nfc_magic_scanner_detect_iso15693(Nfc* nfc) {
    // ISO15693 (NfcV) is a different RF technology from ISO14443-3A, so it gets its own
    // activation probe. There is no reliable non-destructive backdoor probe for a magic
    // ISO15693 tag, so any ISO15693 tag that activates is treated as a ISO15693 *candidate* --
    // magic-ness is proven at write time via a UID read-back. This mirrors how a generic
    // MIFARE Classic is reported as an unconfirmed candidate.
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso15693_3);
    bool detected = nfc_poller_detect(poller);
    nfc_poller_free(poller);
    return detected;
}

static bool nfc_magic_scanner_detect_not_magic(Nfc* nfc) {
    for(size_t i = 0; i < COUNT_OF(nfc_magic_scanner_not_magic_protocols); i++) {
        NfcPoller* poller = nfc_poller_alloc(nfc, nfc_magic_scanner_not_magic_protocols[i]);
        bool detected = nfc_poller_detect(poller);
        nfc_poller_free(poller);
        if(detected) {
            return true;
        }
    }
    return false;
}

// One detection pass. SAK from the standard activation picks the family before any
// backdoor frame, which prunes wrong-family probes and stops a 7AFF USCUID-UL (which
// answers the same 40/43 wakeup as a Gen1A) from being misdetected as Gen1.
static bool nfc_magic_scanner_detect_pass(NfcMagicScanner* instance, NfcMagicProtocol* protocol) {
    const NfcMagicScannerIdentity id = nfc_magic_scanner_read_identity(instance->nfc);

    // Gen4 (UMC) is family-agnostic and definitive; probe it first so a wiped UMC isn't
    // mistaken for a Gen2 CUID or a blank Ultralight.
    if(nfc_magic_scanner_detect_gen4(instance)) {
        *protocol = NfcMagicProtocolGen4;
        return true;
    }

    // ISO15693 magic candidate. Different RF tech, so it does not depend on the
    // ISO14443-3A identity read above.
    if(nfc_magic_scanner_detect_iso15693(instance->nfc)) {
        *protocol = NfcMagicProtocolIso15693;
        return true;
    }

    if(id.activated && id.sak == NFC_MAGIC_SAK_ULTRALIGHT) {
        // Ultralight family.
        if(uscuid_ul_poller_detect(instance->nfc, &instance->uscuid_ul_data) ==
           UscuidUlPollerErrorNone) {
            *protocol = NfcMagicProtocolUscuidUl;
            return true;
        }
        // Unpersonalized UL-5 has a locked config (so the probe above fails) but is
        // identifiable by its UID prefix AA 55. Report it as a detect-only hint.
        if(id.uid0 == 0xAA && id.uid1 == 0x55) {
            memset(&instance->uscuid_ul_data, 0, sizeof(UscuidUlData));
            instance->uscuid_ul_data.maybe_ul5 = true;
            *protocol = NfcMagicProtocolUscuidUl;
            return true;
        }
        // Activated as an Ultralight but with no magic signature and no UL-5 hint: not a
        // confirmed magic tag. Classify it as "not detected" (zeroed data => direct engine,
        // wakeup None) so the user can still attempt a write, mirroring the Classic fallback
        // below. Returning true here also stops the worker spinning forever on a genuine tag.
        memset(&instance->uscuid_ul_data, 0, sizeof(UscuidUlData));
        *protocol = NfcMagicProtocolUscuidUlNotDetected;
        return true;
    } else if(id.activated) {
        // MIFARE Classic family.
        if(gen1a_poller_detect(instance->nfc)) {
            instance->gen1_uid_len = id.uid_len;
            *protocol = NfcMagicProtocolGen1;
            return true;
        }
        if(gen2_poller_detect_type(instance->nfc, &instance->gen2_type) == Gen2PollerErrorNone) {
            *protocol = NfcMagicProtocolGen2;
            return true;
        }
        if(nfc_magic_scanner_detect_mf_classic(instance->nfc)) {
            *protocol = NfcMagicProtocolClassic;
            return true;
        }
    } else {
        // No standard activation: try the backdoor wakeups to revive a bricked magic card.
        if(gen1a_poller_detect(instance->nfc)) {
            instance->gen1_uid_len = 0;
            *protocol = NfcMagicProtocolGen1;
            return true;
        }
        if(uscuid_ul_poller_detect(instance->nfc, &instance->uscuid_ul_data) ==
           UscuidUlPollerErrorNone) {
            *protocol = NfcMagicProtocolUscuidUl;
            return true;
        }
    }

    return false;
}

static int32_t nfc_magic_scanner_worker(void* context) {
    furi_assert(context);

    NfcMagicScanner* instance = context;
    furi_assert(instance->session_state == NfcMagicScannerSessionStateActive);

    while(instance->session_state == NfcMagicScannerSessionStateActive) {
        NfcMagicProtocol protocol = NfcMagicProtocolInvalid;

        if(nfc_magic_scanner_detect_pass(instance, &protocol)) {
            NfcMagicScannerEvent event = {
                .type = NfcMagicScannerEventTypeDetected,
                .data.protocol = protocol,
                .data.gen2_type = instance->gen2_type,
                .data.gen1_uid_len = instance->gen1_uid_len,
                .data.uscuid_ul = instance->uscuid_ul_data,
            };
            instance->callback(event, instance->context);
            break;
        }

        // Non-ISO14443-3A cards (ISO14443-3B / ISO15693 / FeliCa) are simply not magic.
        if(nfc_magic_scanner_detect_not_magic(instance->nfc)) {
            NfcMagicScannerEvent event = {
                .type = NfcMagicScannerEventTypeDetectedNotMagic,
            };
            instance->callback(event, instance->context);
            break;
        }
    }

    nfc_magic_scanner_reset(instance);

    return 0;
}

void nfc_magic_scanner_start(
    NfcMagicScanner* instance,
    NfcMagicScannerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;

    instance->scan_worker = furi_thread_alloc();
    furi_thread_set_name(instance->scan_worker, "NfcMagicScanWorker");
    furi_thread_set_context(instance->scan_worker, instance);
    furi_thread_set_stack_size(instance->scan_worker, 4 * 1024);
    furi_thread_set_callback(instance->scan_worker, nfc_magic_scanner_worker);
    furi_thread_start(instance->scan_worker);

    instance->session_state = NfcMagicScannerSessionStateActive;
}

void nfc_magic_scanner_stop(NfcMagicScanner* instance) {
    furi_assert(instance);

    instance->session_state = NfcMagicScannerSessionStateStopRequest;
    furi_thread_join(instance->scan_worker);
    instance->session_state = NfcMagicScannerSessionStateIdle;

    furi_thread_free(instance->scan_worker);
    instance->scan_worker = NULL;
    instance->callback = NULL;
    instance->context = NULL;
}

const Gen4* nfc_magic_scanner_get_gen4_data(NfcMagicScanner* instance) {
    furi_assert(instance);

    return instance->gen4_data;
}

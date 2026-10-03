#include "nfc_magic_scanner.h"

#include "core/check.h"
#include "protocols/gen4/gen4.h"
#include "protocols/gen1a/gen1a_poller.h"
#include "protocols/gen2/gen2_poller.h"
#include "protocols/gen4/gen4_poller.h"
#include "protocols/uscuid_ul/uscuid_ul_poller.h"
#include "../helpers/mfc_key_cache.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>

#include <furi/furi.h>

#define TAG "NfcMagicScanner"

// One key A plus one key B for sector 0 is everything the key cache can offer the CUID probe.
#define NFC_MAGIC_SCANNER_MFC_PROBE_KEYS_MAX (2)

typedef enum {
    NfcMagicScannerSessionStateIdle,
    NfcMagicScannerSessionStateActive,
    NfcMagicScannerSessionStateStopRequest,
} NfcMagicScannerSessionState;

struct NfcMagicScanner {
    Nfc* nfc;
    Storage* storage;
    NfcMagicScannerSessionState session_state;

    Gen4Password gen4_password;
    Gen4* gen4_data;
    Gen2Type gen2_type;
    uint8_t gen1_uid_len;
    uint8_t uid[ISO14443_3A_MAX_UID_SIZE];
    uint8_t uid_len;
    UscuidUlData uscuid_ul_data;

    // Sector-0 keys the key cache holds for the card named by mfc_probe_uid, handed to the Gen2
    // CUID write probe. Kept across detect passes so the cache is read once per card, not once
    // per pass; mfc_probe_uid_len is 0 until the first lookup of a session.
    Gen2ProbeKey mfc_probe_keys[NFC_MAGIC_SCANNER_MFC_PROBE_KEYS_MAX];
    size_t mfc_probe_key_count;
    uint8_t mfc_probe_uid[ISO14443_3A_MAX_UID_SIZE];
    uint8_t mfc_probe_uid_len;

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
    memset(instance->uid, 0, sizeof(instance->uid));
    instance->uid_len = 0;
    memset(&instance->uscuid_ul_data, 0, sizeof(UscuidUlData));
    instance->mfc_probe_key_count = 0;
    memset(instance->mfc_probe_uid, 0, sizeof(instance->mfc_probe_uid));
    instance->mfc_probe_uid_len = 0;
}

NfcMagicScanner* nfc_magic_scanner_alloc(Nfc* nfc, Storage* storage) {
    furi_assert(nfc);
    furi_assert(storage);

    NfcMagicScanner* instance = malloc(sizeof(NfcMagicScanner));
    instance->nfc = nfc;
    instance->storage = storage;
    instance->gen4_data = gen4_alloc();
    // Nothing else initialises the scan state before the first session: start doesn't reset, and
    // reset otherwise runs only once the worker exits. The probe memo below in particular has to
    // begin zeroed -- a chance match against malloc'd bytes would skip the lookup and hand
    // gen2_poller_detect_type a garbage key count to iterate.
    nfc_magic_scanner_reset(instance);

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
// the UID length (4/7) tells 4- vs 7-byte Gen1 tags apart (the wakeup is UID-agnostic), and
// the UID itself names the NFC app's per-UID key cache entry, which a magic clone inherits
// from the card it was cloned from.
typedef struct {
    bool activated;
    uint8_t sak;
    uint8_t uid[ISO14443_3A_MAX_UID_SIZE];
    uint8_t uid_len; // Bytes valid in uid; 0 when not activated
} NfcMagicScannerIdentity;

static NfcMagicScannerIdentity nfc_magic_scanner_read_identity(Nfc* nfc) {
    NfcMagicScannerIdentity id = {.activated = false, .sak = 0, .uid_len = 0};

    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    if(nfc_poller_detect(poller)) {
        const Iso14443_3aData* data = nfc_poller_get_data(poller);
        id.activated = true;
        id.sak = data->sak;
        id.uid_len = MIN(data->uid_len, (uint8_t)sizeof(id.uid));
        memcpy(id.uid, data->uid, id.uid_len);
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

// Key B before key A, matching the probe's own FF..FF pair: where a sector has been personalised
// at all, write access is the permission more often kept for key B. Guessing wrong costs one RF
// session, so this is a preference and not a requirement.
static const MfClassicKeyType nfc_magic_scanner_mfc_probe_key_types[] = {
    MfClassicKeyTypeB,
    MfClassicKeyTypeA,
};
_Static_assert(
    COUNT_OF(nfc_magic_scanner_mfc_probe_key_types) <= NFC_MAGIC_SCANNER_MFC_PROBE_KEYS_MAX,
    "Scanner probe key array too small for the key types tried");

// Collects the sector-0 keys the NFC app recorded for this UID (read_identity above says why a
// clone's UID names them). They are what gets a personalised clone as far as the CUID write probe
// at all, since the probe's default FF..FF opens only a blank sector 0. The lookup is memoised per
// UID, hit or miss, so repeating a detect pass on the same card touches no storage.
static void nfc_magic_scanner_load_mfc_probe_keys(
    NfcMagicScanner* instance,
    const uint8_t* uid,
    uint8_t uid_len) {
    if(instance->mfc_probe_uid_len == uid_len &&
       memcmp(instance->mfc_probe_uid, uid, uid_len) == 0) {
        return;
    }

    instance->mfc_probe_key_count = 0;
    memcpy(instance->mfc_probe_uid, uid, uid_len);
    instance->mfc_probe_uid_len = uid_len;

    MfcKeyCache* key_cache = mfc_key_cache_load(instance->storage, uid, uid_len);
    if(key_cache == NULL) return;

    for(size_t i = 0; i < COUNT_OF(nfc_magic_scanner_mfc_probe_key_types); i++) {
        const MfClassicKeyType key_type = nfc_magic_scanner_mfc_probe_key_types[i];
        Gen2ProbeKey* probe_key = &instance->mfc_probe_keys[instance->mfc_probe_key_count];
        if(mfc_key_cache_get_key(key_cache, 0, key_type, &probe_key->key)) {
            probe_key->key_type = key_type;
            instance->mfc_probe_key_count++;
        }
    }
    FURI_LOG_D(TAG, "Sector 0 keys from cache: %u", (unsigned)instance->mfc_probe_key_count);

    mfc_key_cache_free(key_cache);
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
    memcpy(instance->uid, id.uid, sizeof(instance->uid));
    instance->uid_len = id.uid_len;

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
        if(id.uid_len >= 2 && id.uid[0] == 0xAA && id.uid[1] == 0x55) {
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
            // Gen1 only knows the two standard Classic UID lengths; anything else is "unknown".
            instance->gen1_uid_len =
                (id.uid_len == ISO14443_3A_UID_4_BYTES || id.uid_len == ISO14443_3A_UID_7_BYTES) ?
                    id.uid_len :
                    0;
            *protocol = NfcMagicProtocolGen1;
            return true;
        }
        nfc_magic_scanner_load_mfc_probe_keys(instance, id.uid, id.uid_len);
        if(gen2_poller_detect_type(
               instance->nfc,
               instance->mfc_probe_keys,
               instance->mfc_probe_key_count,
               &instance->gen2_type) == Gen2PollerErrorNone) {
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
                .data.uid_len = instance->uid_len,
            };
            memcpy(event.data.uid, instance->uid, sizeof(event.data.uid));
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

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/mf_desfire/mf_desfire.h>
#include <nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include <nfc/protocols/mf_plus/mf_plus.h>
#include <nfc/protocols/mf_plus/mf_plus_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <nfc/protocols/felica/felica.h>
#include <nfc/protocols/felica/felica_poller.h>

#include "observation_provider.h"

/* -------------------------------------------------------------------------
 * Internal state machine
 * -------------------------------------------------------------------------
 *
 * Idle ──start()──> Scanning ──(scanner cb)──> ReadPending
 *                                                   │
 *                                            poll() stops scanner,
 *                                            starts ISO14443-3a poller
 *                                                   │
 *                                               Reading
 *                                              /       \
 *                                    (poller cb)       (poller cb)
 *                                       Ready          Error/Fail
 *                                          │                │
 *                                        Done          ReadFailed
 *                                          │                │
 *                                    poll() returns    poll() restarts
 *                                    true, goes Idle    scanner
 *
 * All reads/writes of shared fields happen under mutex.
 * All NFC API calls (start/stop/free) happen OUTSIDE the mutex to avoid
 * deadlocks with the NFC worker thread that runs the callbacks.
 * -------------------------------------------------------------------------
 */

typedef enum {
    ProviderStateIdle,
    ProviderStateScanning,
    ProviderStateReadPending,
    ProviderStateReading,
    ProviderStateDone,
    ProviderStateReadFailed,
} ProviderState;

struct ObservationProvider {
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;
    FuriMutex* mutex;
    ProviderState state;
    /* Set by scanner callback, read by poll() */
    CardType detected_card_type;
    /* Always NfcProtocolIso14443_3a for 3a-family cards */
    NfcProtocol uid_protocol;
    /* Populated by poller callback */
    AccessObservation pending;
};

/* -------------------------------------------------------------------------
 * Protocol helpers
 * -------------------------------------------------------------------------
 */

/** Map a detected protocol to its CardType. Higher-priority protocols are
 *  checked first so the richest classification wins. */
static CardType protocol_to_card_type(NfcProtocol protocol) {
    switch(protocol) {
    case NfcProtocolMfDesfire:
        return CardTypeMifareDesfire;
    case NfcProtocolMfPlus:
        return CardTypeMifarePlus;
    case NfcProtocolMfClassic:
        return CardTypeMifareClassic;
    case NfcProtocolMfUltralight:
        return CardTypeMifareUltralight;
    case NfcProtocolIso14443_4a:
    case NfcProtocolIso14443_3a:
        return CardTypeIso14443A;
    case NfcProtocolIso14443_4b:
    case NfcProtocolIso14443_3b:
        return CardTypeIso14443B;
    case NfcProtocolIso15693_3:
        return CardTypeIso15693;
    case NfcProtocolFelica:
        return CardTypeFelica;
    case NfcProtocolSlix:
        return CardTypeSlix;
    case NfcProtocolSt25tb:
        return CardTypeSt25tb;
    default:
        return CardTypeUnknown;
    }
}

/** Priority-ordered list used for card-type classification. */
static const NfcProtocol CARD_TYPE_PRIORITY[] = {
    NfcProtocolMfDesfire,
    NfcProtocolMfPlus,
    NfcProtocolMfClassic,
    NfcProtocolMfUltralight,
    NfcProtocolIso14443_4a,
    NfcProtocolIso14443_4b,
    NfcProtocolIso14443_3a,
    NfcProtocolIso14443_3b,
    NfcProtocolIso15693_3,
    NfcProtocolFelica,
    NfcProtocolSlix,
    NfcProtocolSt25tb,
};
static const size_t CARD_TYPE_PRIORITY_COUNT =
    sizeof(CARD_TYPE_PRIORITY) / sizeof(CARD_TYPE_PRIORITY[0]);

/** Pick the richest card type from the list the scanner reported. */
static CardType best_card_type(const NfcProtocol* protos, size_t count) {
    for(size_t p = 0; p < CARD_TYPE_PRIORITY_COUNT; p++) {
        for(size_t i = 0; i < count; i++) {
            if(protos[i] == CARD_TYPE_PRIORITY[p]) {
                return protocol_to_card_type(CARD_TYPE_PRIORITY[p]);
            }
        }
    }
    return CardTypeUnknown;
}

/**
 * Determine the transport-layer protocol to use for UID reading.
 *
 * All MIFARE variants (Classic, Ultralight, DESFire, Plus) and ISO14443-4a
 * are built on ISO14443-3a, so we always start an ISO14443-3a poller for them.
 * That gives us the UID from the anti-collision step without needing auth.
 *
 * Uses nfc_protocol_has_parent() from the SDK to handle the hierarchy
 * automatically rather than maintaining a hard-coded list.
 */
static NfcProtocol uid_transport(const NfcProtocol* protos, size_t count) {
    /* Prefer DESFire poller — gives version (EV1/EV2/EV3) and UID. */
    for(size_t i = 0; i < count; i++) {
        if(protos[i] == NfcProtocolMfDesfire) return NfcProtocolMfDesfire;
    }
    /* Prefer MfPlus poller — gives security level and UID. */
    for(size_t i = 0; i < count; i++) {
        if(protos[i] == NfcProtocolMfPlus) return NfcProtocolMfPlus;
    }
    /* Prefer MfClassic poller — attempts default key auth on sector 0. */
    for(size_t i = 0; i < count; i++) {
        if(protos[i] == NfcProtocolMfClassic) return NfcProtocolMfClassic;
    }
    /* Prefer MfUltralight over bare ISO14443-3a so we get NTAG sub-type. */
    for(size_t i = 0; i < count; i++) {
        if(protos[i] == NfcProtocolMfUltralight ||
           nfc_protocol_has_parent(protos[i], NfcProtocolMfUltralight)) {
            return NfcProtocolMfUltralight;
        }
    }
    for(size_t i = 0; i < count; i++) {
        if(protos[i] == NfcProtocolIso14443_3a ||
           nfc_protocol_has_parent(protos[i], NfcProtocolIso14443_3a)) {
            return NfcProtocolIso14443_3a;
        }
    }
    for(size_t i = 0; i < count; i++) {
        if(protos[i] == NfcProtocolIso14443_3b ||
           nfc_protocol_has_parent(protos[i], NfcProtocolIso14443_3b)) {
            return NfcProtocolIso14443_3b;
        }
    }
    /* ISO15693 — covers HID iCLASS DP, SLIX, and generic ISO15693 tags */
    for(size_t i = 0; i < count; i++) {
        if(protos[i] == NfcProtocolIso15693_3 ||
           nfc_protocol_has_parent(protos[i], NfcProtocolIso15693_3)) {
            return NfcProtocolIso15693_3;
        }
    }
    /* Fallback: use first detected protocol */
    return (count > 0) ? protos[0] : NfcProtocolInvalid;
}

/* -------------------------------------------------------------------------
 * Sub-type helpers
 * -------------------------------------------------------------------------
 */

/**
 * Map DESFire version bytes to CardType.
 * hw_major bytes come from the GetVersion command response.
 * Reference: NXP AN10609 / actual card responses.
 */
static CardType desfire_version_to_card_type(const MfDesfireVersion* ver) {
    if(ver->hw_major == 0x01) return CardTypeMifareDesfireEV1;
    if(ver->hw_major == 0x12) return CardTypeMifareDesfireEV2;
    if(ver->hw_major == 0x33) return CardTypeMifareDesfireEV3;
    /* DESFire Light: hw_type = 0x08 */
    if(ver->hw_type == 0x08) return CardTypeMifareDesfireLight;
    return CardTypeMifareDesfire;
}

/** Map MIFARE Plus security level to CardType. */
static CardType mf_plus_sl_to_card_type(MfPlusSecurityLevel sl) {
    switch(sl) {
    case MfPlusSecurityLevel1:
        return CardTypeMifarePlusSL1;
    case MfPlusSecurityLevel2:
        return CardTypeMifarePlusSL2;
    case MfPlusSecurityLevel3:
        return CardTypeMifarePlusSL3;
    default:
        return CardTypeMifarePlus;
    }
}

/** Refine MIFARE Classic generic type to 1K/4K/Mini using the SAK byte. */
static CardType classic_subtype_from_sak(uint8_t sak, CardType detected) {
    if(detected != CardTypeMifareClassic && detected != CardTypeMifareClassic1K &&
       detected != CardTypeMifareClassic4K && detected != CardTypeMifareClassicMini) {
        return detected;
    }
    switch(sak) {
    case 0x09:
        return CardTypeMifareClassicMini;
    case 0x08:
    case 0x28: /* some emulated 1K cards respond with 0x28 */
        return CardTypeMifareClassic1K;
    case 0x18:
    case 0x38: /* some emulated 4K cards respond with 0x38 */
        return CardTypeMifareClassic4K;
    default:
        return CardTypeMifareClassic;
    }
}

/** Map SDK MfUltralightType to our CardType. */
static CardType mf_ultralight_type_to_card_type(MfUltralightType type) {
    switch(type) {
    case MfUltralightTypeMfulC:
        return CardTypeMifareUltralightC;
    case MfUltralightTypeNTAG203:
        return CardTypeNtag203;
    case MfUltralightTypeNTAG213:
        return CardTypeNtag213;
    case MfUltralightTypeNTAG215:
        return CardTypeNtag215;
    case MfUltralightTypeNTAG216:
        return CardTypeNtag216;
    case MfUltralightTypeNTAGI2C1K:
    case MfUltralightTypeNTAGI2C2K:
    case MfUltralightTypeNTAGI2CPlus1K:
    case MfUltralightTypeNTAGI2CPlus2K:
        return CardTypeNtagI2C;
    default:
        return CardTypeMifareUltralight;
    }
}

/* -------------------------------------------------------------------------
 * Validation (same rules as before)
 * -------------------------------------------------------------------------
 */

static bool observation_is_valid(const AccessObservation* obs) {
    if(!obs) return false;
    if(obs->tech != TechTypeNfc13Mhz) return false;
    if(obs->card_type == CardTypeUnknown) return false;
    if(!obs->uid_present) return false;
    if(obs->uid_len == 0) return false;
    return true;
}

/* -------------------------------------------------------------------------
 * Scanner callback  (runs on NFC worker thread)
 * -------------------------------------------------------------------------
 */

static void scanner_callback(NfcScannerEvent event, void* context) {
    ObservationProvider* p = context;

    if(event.type != NfcScannerEventTypeDetected) return;
    if(event.data.protocol_num == 0) return;

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    if(p->state == ProviderStateScanning) {
        p->detected_card_type = best_card_type(event.data.protocols, event.data.protocol_num);
        p->uid_protocol = uid_transport(event.data.protocols, event.data.protocol_num);
        p->state = ProviderStateReadPending;
    }

    furi_mutex_release(p->mutex);
}

/* -------------------------------------------------------------------------
 * ISO14443-3a poller callback  (runs on NFC worker thread)
 *
 * Iso14443_3aPollerEventTypeReady fires after the card has been activated
 * (anti-collision complete, UID in poller data).  We read the UID here
 * while the card is definitely present, then stop.
 * -------------------------------------------------------------------------
 */

static NfcCommand iso14443_3a_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    const Iso14443_3aPollerEvent* iso_event = (const Iso14443_3aPollerEvent*)event.event_data;

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    if(iso_event->type == Iso14443_3aPollerEventTypeReady) {
        /* nfc_poller_get_data() on the outer NfcPoller returns Iso14443_3aData* */
        const Iso14443_3aData* data = (const Iso14443_3aData*)nfc_poller_get_data(p->poller);

        if(data) {
            size_t uid_len = 0;
            const uint8_t* uid = iso14443_3a_get_uid(data, &uid_len);
            uint8_t sak = iso14443_3a_get_sak(data);
            uint8_t atqa[2];
            iso14443_3a_get_atqa(data, atqa);

            p->pending = (AccessObservation){0};
            p->pending.tech = TechTypeNfc13Mhz;
            p->pending.card_type = classic_subtype_from_sak(sak, p->detected_card_type);
            p->pending.metadata_complete = true;
            p->pending.sak_atqa_present = true;
            p->pending.sak = sak;
            p->pending.atqa[0] = atqa[0];
            p->pending.atqa[1] = atqa[1];

            if(uid && uid_len > 0) {
                p->pending.uid_present = true;
                p->pending.uid_len = uid_len <= sizeof(p->pending.uid) ? uid_len :
                                                                         sizeof(p->pending.uid);
                for(size_t i = 0; i < p->pending.uid_len; i++) {
                    p->pending.uid[i] = uid[i];
                }
            }

            p->state = ProviderStateDone;
        } else {
            p->state = ProviderStateReadFailed;
        }
    } else {
        /* Iso14443_3aPollerEventTypeError */
        p->state = ProviderStateReadFailed;
    }

    furi_mutex_release(p->mutex);
    return NfcCommandStop;
}

/* -------------------------------------------------------------------------
 * MfUltralight poller callback  (runs on NFC worker thread)
 *
 * The poller fires RequestMode first (we ask for Read), then AuthRequest if
 * the card requires a password (we skip it), then ReadSuccess/ReadFailed.
 * -------------------------------------------------------------------------
 */

static NfcCommand mf_ultralight_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    MfUltralightPollerEvent* ul_event = (MfUltralightPollerEvent*)event.event_data;

    /* These two events don't touch shared provider state — no mutex needed. */
    if(ul_event->type == MfUltralightPollerEventTypeRequestMode) {
        ul_event->data->poller_mode = MfUltralightPollerModeRead;
        return NfcCommandContinue;
    }
    if(ul_event->type == MfUltralightPollerEventTypeAuthRequest) {
        ul_event->data->auth_context.skip_auth = true;
        return NfcCommandContinue;
    }

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    if(ul_event->type == MfUltralightPollerEventTypeReadSuccess) {
        const MfUltralightData* ul_data = (const MfUltralightData*)nfc_poller_get_data(p->poller);

        if(ul_data) {
            size_t uid_len = 0;
            const uint8_t* uid = mf_ultralight_get_uid(ul_data, &uid_len);

            p->pending = (AccessObservation){0};
            p->pending.tech = TechTypeNfc13Mhz;
            p->pending.card_type = mf_ultralight_type_to_card_type(ul_data->type);
            p->pending.metadata_complete = true;

            if(uid && uid_len > 0) {
                p->pending.uid_present = true;
                p->pending.uid_len = uid_len <= sizeof(p->pending.uid) ? uid_len :
                                                                         sizeof(p->pending.uid);
                for(size_t i = 0; i < p->pending.uid_len; i++) {
                    p->pending.uid[i] = uid[i];
                }
            }

            p->state = ProviderStateDone;
        } else {
            p->state = ProviderStateReadFailed;
        }
    } else {
        p->state = ProviderStateReadFailed;
    }

    furi_mutex_release(p->mutex);
    return NfcCommandStop;
}

/* -------------------------------------------------------------------------
 * MfDesfire poller callback  (runs on NFC worker thread)
 *
 * The poller reads GetVersion (always available without auth), then attempts
 * to read application metadata.  ReadSuccess fires when the read sequence
 * completes — even partial reads with auth-protected files give us the version.
 * -------------------------------------------------------------------------
 */

static NfcCommand mf_desfire_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    const MfDesfirePollerEvent* df_event = (const MfDesfirePollerEvent*)event.event_data;

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    if(df_event->type == MfDesfirePollerEventTypeReadSuccess) {
        const MfDesfireData* df_data = (const MfDesfireData*)nfc_poller_get_data(p->poller);

        if(df_data) {
            size_t uid_len = 0;
            const uint8_t* uid = mf_desfire_get_uid(df_data, &uid_len);

            p->pending = (AccessObservation){0};
            p->pending.tech = TechTypeNfc13Mhz;
            p->pending.card_type = desfire_version_to_card_type(&df_data->version);
            p->pending.metadata_complete = true;
            /* DESFire cards have AES-protected application memory */
            p->pending.user_memory_present = true;

            if(uid && uid_len > 0) {
                p->pending.uid_present = true;
                p->pending.uid_len = uid_len <= sizeof(p->pending.uid) ? uid_len :
                                                                         sizeof(p->pending.uid);
                for(size_t i = 0; i < p->pending.uid_len; i++) {
                    p->pending.uid[i] = uid[i];
                }
            }

            p->state = ProviderStateDone;
        } else {
            p->state = ProviderStateReadFailed;
        }
    } else {
        p->state = ProviderStateReadFailed;
    }

    furi_mutex_release(p->mutex);
    return NfcCommandStop;
}

/* -------------------------------------------------------------------------
 * MfPlus poller callback  (runs on NFC worker thread)
 *
 * The SDK determines the security level (SL0–SL3) automatically by reading
 * the card's version and configuration responses.
 * -------------------------------------------------------------------------
 */

static NfcCommand mf_plus_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    const MfPlusPollerEvent* plus_event = (const MfPlusPollerEvent*)event.event_data;

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    if(plus_event->type == MfPlusPollerEventTypeReadSuccess) {
        const MfPlusData* plus_data = (const MfPlusData*)nfc_poller_get_data(p->poller);

        if(plus_data) {
            size_t uid_len = 0;
            const uint8_t* uid = mf_plus_get_uid(plus_data, &uid_len);

            p->pending = (AccessObservation){0};
            p->pending.tech = TechTypeNfc13Mhz;
            p->pending.card_type = mf_plus_sl_to_card_type(plus_data->security_level);
            p->pending.metadata_complete = true;
            /* SL2/SL3 have AES-protected sectors; SL1 is Classic-compat */
            p->pending.user_memory_present =
                (plus_data->security_level == MfPlusSecurityLevel2 ||
                 plus_data->security_level == MfPlusSecurityLevel3);

            if(uid && uid_len > 0) {
                p->pending.uid_present = true;
                p->pending.uid_len = uid_len <= sizeof(p->pending.uid) ? uid_len :
                                                                         sizeof(p->pending.uid);
                for(size_t i = 0; i < p->pending.uid_len; i++) {
                    p->pending.uid[i] = uid[i];
                }
            }

            p->state = ProviderStateDone;
        } else {
            p->state = ProviderStateReadFailed;
        }
    } else {
        p->state = ProviderStateReadFailed;
    }

    furi_mutex_release(p->mutex);
    return NfcCommandStop;
}

/* -------------------------------------------------------------------------
 * MfClassic poller callback  (runs on NFC worker thread)
 *
 * Uses MfClassicPollerModeRead. The poller fires RequestReadSector starting
 * from sector 0. On that event we manually call mf_classic_poller_auth with
 * two well-known default keys (FFFFFFFFFFFF and A0A1A2A3A4A5, both key types)
 * and record whether any succeeded. We then halt the card and stop — no full
 * sector read is performed and no card data is retained.
 *
 * Default keys checked (in order):
 *   FFFFFFFFFFFF key A, FFFFFFFFFFFF key B
 *   A0A1A2A3A4A5 key A, A0A1A2A3A4A5 key B
 * -------------------------------------------------------------------------
 */

static NfcCommand mf_classic_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    MfClassicPollerEvent* cl_event = (MfClassicPollerEvent*)event.event_data;

    if(cl_event->type == MfClassicPollerEventTypeRequestMode) {
        cl_event->data->poller_mode.mode = MfClassicPollerModeRead;
        cl_event->data->poller_mode.data = NULL;
        return NfcCommandContinue;
    }

    if(cl_event->type == MfClassicPollerEventTypeRequestReadSector &&
       cl_event->data->read_sector_request_data.sector_num == 0) {
        static const uint8_t DEFAULT_KEYS[][MF_CLASSIC_KEY_SIZE] = {
            {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* FFFFFFFFFFFF */
            {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}, /* A0A1A2A3A4A5 */
        };
        static const MfClassicKeyType KEY_TYPES[] = {MfClassicKeyTypeA, MfClassicKeyTypeB};

        MfClassicPoller* cl_poller = (MfClassicPoller*)event.instance;
        MfClassicAuthContext auth_ctx;
        bool default_key_found = false;

        for(size_t k = 0; k < 2 && !default_key_found; k++) {
            for(size_t t = 0; t < 2 && !default_key_found; t++) {
                MfClassicKey key;
                memcpy(key.data, DEFAULT_KEYS[k], MF_CLASSIC_KEY_SIZE);
                MfClassicError err =
                    mf_classic_poller_auth(cl_poller, 0, &key, KEY_TYPES[t], &auth_ctx, false);
                if(err == MfClassicErrorNone) default_key_found = true;
            }
        }

        mf_classic_poller_halt(cl_poller);

        const MfClassicData* cl_data = (const MfClassicData*)nfc_poller_get_data(p->poller);

        furi_mutex_acquire(p->mutex, FuriWaitForever);

        if(cl_data) {
            size_t uid_len = 0;
            const uint8_t* uid = mf_classic_get_uid(cl_data, &uid_len);
            const Iso14443_3aData* iso_data = mf_classic_get_base_data(cl_data);

            p->pending = (AccessObservation){0};
            p->pending.tech = TechTypeNfc13Mhz;
            p->pending.metadata_complete = true;
            p->pending.default_keys_readable = default_key_found;

            if(iso_data) {
                uint8_t sak = iso14443_3a_get_sak(iso_data);
                uint8_t atqa[2];
                iso14443_3a_get_atqa(iso_data, atqa);
                p->pending.card_type = classic_subtype_from_sak(sak, p->detected_card_type);
                p->pending.sak_atqa_present = true;
                p->pending.sak = sak;
                p->pending.atqa[0] = atqa[0];
                p->pending.atqa[1] = atqa[1];
            } else {
                p->pending.card_type = p->detected_card_type;
            }

            if(uid && uid_len > 0) {
                p->pending.uid_present = true;
                p->pending.uid_len = uid_len <= sizeof(p->pending.uid) ? uid_len :
                                                                         sizeof(p->pending.uid);
                memcpy(p->pending.uid, uid, p->pending.uid_len);
            }

            p->state = ProviderStateDone;
        } else {
            p->state = ProviderStateReadFailed;
        }

        furi_mutex_release(p->mutex);
        return NfcCommandStop;
    }

    /* Fail, CardLost, or any event before sector 0 is requested. */
    furi_mutex_acquire(p->mutex, FuriWaitForever);
    p->state = ProviderStateReadFailed;
    furi_mutex_release(p->mutex);
    return NfcCommandStop;
}

/* -------------------------------------------------------------------------
 * ISO15693-3 poller callback  (runs on NFC worker thread)
 *
 * EventTypeReady  — full activation succeeded (inventory + system info).
 * EventTypeError  — activation partially failed (e.g. card responded to
 *                   inventory but not to Get System Info, which is the case
 *                   for HID iCLASS DP).  We still try nfc_poller_get_data
 *                   because the UID is populated after the inventory step
 *                   regardless of whether system info succeeded.
 *
 * ISO15693 UID layout (as stored by the Flipper, UID received LSB-first):
 *   uid[0..5] = serial number bytes
 *   uid[6]    = IC manufacturer code (ISO/IEC 7816-6)
 *   uid[7]    = 0xE0 (ISO15693 tag marker)
 *
 * Manufacturer codes relevant to access control:
 *   0x07 = Texas Instruments — used in HID iCLASS DP
 *   0x04 = NXP               — used in SLIX/ICODE
 *   0x02 = STMicroelectronics
 * -------------------------------------------------------------------------
 */

static NfcCommand iso15693_3_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    const Iso15693_3PollerEvent* iso_event = (const Iso15693_3PollerEvent*)event.event_data;

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    /* Try to extract data on both Ready and Error — the UID is populated
       after inventory even if Get System Info fails (e.g. iCLASS DP). */
    const Iso15693_3Data* data = (const Iso15693_3Data*)nfc_poller_get_data(p->poller);

    if(data) {
        size_t uid_len = 0;
        const uint8_t* uid = iso15693_3_get_uid(data, &uid_len);

        p->pending = (AccessObservation){0};
        p->pending.tech = TechTypeNfc13Mhz;
        /* metadata_complete only if full activation succeeded */
        p->pending.metadata_complete = (iso_event->type == Iso15693_3PollerEventTypeReady);

        /* Classify by manufacturer code in uid[6] (LSB-first UID storage). */
        if(uid && uid_len == 8 && uid[6] == 0x07) {
            /* TI — HID iCLASS DP */
            p->pending.card_type = CardTypeHidIclass;
        } else {
            p->pending.card_type = CardTypeIso15693;
        }

        if(uid && uid_len > 0) {
            p->pending.uid_present = true;
            p->pending.uid_len = uid_len <= sizeof(p->pending.uid) ? uid_len :
                                                                     sizeof(p->pending.uid);
            for(size_t i = 0; i < p->pending.uid_len; i++) {
                p->pending.uid[i] = uid[i];
            }
        }

        p->state = ProviderStateDone;
    } else {
        p->state = ProviderStateReadFailed;
    }

    furi_mutex_release(p->mutex);
    return NfcCommandStop;
}

/* -------------------------------------------------------------------------
 * FeliCa poller callback  (runs on NFC worker thread)
 *
 * FelicaPollerEventTypeReady   — full read complete; workflow_type set.
 * FelicaPollerEventTypeIncomplete — activated but not fully read;
 *                                   UID is still available.
 *
 * FelicaWorkflowType:
 *   FelicaLite     — no mutual authentication, used in transit/building
 *                    access where the UID or a static value is the credential
 *   FelicaStandard — proprietary crypto; security depends on configuration
 *   FelicaUnknown  — workflow could not be determined
 * -------------------------------------------------------------------------
 */

static NfcCommand felica_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    const FelicaPollerEvent* fc_event = (const FelicaPollerEvent*)event.event_data;

    if(fc_event->type != FelicaPollerEventTypeReady &&
       fc_event->type != FelicaPollerEventTypeIncomplete) {
        /* Error — restart scanner */
        furi_mutex_acquire(p->mutex, FuriWaitForever);
        p->state = ProviderStateReadFailed;
        furi_mutex_release(p->mutex);
        return NfcCommandStop;
    }

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    const FelicaData* fc_data = (const FelicaData*)nfc_poller_get_data(p->poller);

    if(fc_data) {
        size_t uid_len = 0;
        const uint8_t* uid = felica_get_uid(fc_data, &uid_len);

        p->pending = (AccessObservation){0};
        p->pending.tech = TechTypeNfc13Mhz;
        p->pending.metadata_complete = (fc_event->type == FelicaPollerEventTypeReady);

        /* Classify by workflow type */
        if(fc_data->workflow_type == FelicaLite) {
            p->pending.card_type = CardTypeFeliCaLite;
        } else {
            p->pending.card_type = CardTypeFelica;
        }

        if(uid && uid_len > 0) {
            p->pending.uid_present = true;
            p->pending.uid_len = uid_len <= sizeof(p->pending.uid) ? uid_len :
                                                                     sizeof(p->pending.uid);
            for(size_t i = 0; i < p->pending.uid_len; i++) {
                p->pending.uid[i] = uid[i];
            }
        }

        p->state = ProviderStateDone;
    } else {
        p->state = ProviderStateReadFailed;
    }

    furi_mutex_release(p->mutex);
    return NfcCommandStop;
}

/* -------------------------------------------------------------------------
 * Internal helpers (main thread only — no races with each other)
 * -------------------------------------------------------------------------
 */

/** Allocate and start a fresh NfcScanner. Updates mutex-protected fields. */
static void provider_start_scanner(ObservationProvider* p) {
    NfcScanner* sc = nfc_scanner_alloc(p->nfc);

    furi_mutex_acquire(p->mutex, FuriWaitForever);
    p->scanner = sc;
    p->state = ProviderStateScanning;
    furi_mutex_release(p->mutex);

    if(sc) {
        nfc_scanner_start(sc, scanner_callback, p);
    }
}

/** Choose the right poller callback for the given protocol. */
static NfcGenericCallback callback_for_protocol(NfcProtocol protocol) {
    switch(protocol) {
    case NfcProtocolMfDesfire:
        return mf_desfire_poller_cb;
    case NfcProtocolMfPlus:
        return mf_plus_poller_cb;
    case NfcProtocolMfClassic:
        return mf_classic_poller_cb;
    case NfcProtocolMfUltralight:
        return mf_ultralight_poller_cb;
    case NfcProtocolIso14443_3a:
        return iso14443_3a_poller_cb;
    case NfcProtocolIso15693_3:
        return iso15693_3_poller_cb;
    case NfcProtocolFelica:
        return felica_poller_cb;
    default:
        return NULL;
    }
}

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

ObservationProvider* observation_provider_alloc(void) {
    ObservationProvider* p = malloc(sizeof(ObservationProvider));
    if(!p) return NULL;

    p->nfc = nfc_alloc();
    p->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    p->scanner = NULL;
    p->poller = NULL;
    p->state = ProviderStateIdle;
    p->detected_card_type = CardTypeUnknown;
    p->uid_protocol = NfcProtocolInvalid;
    p->pending = (AccessObservation){0};

    if(!p->nfc || !p->mutex) {
        if(p->nfc) nfc_free(p->nfc);
        if(p->mutex) furi_mutex_free(p->mutex);
        free(p);
        return NULL;
    }

    return p;
}

void observation_provider_free(ObservationProvider* provider) {
    if(!provider) return;
    observation_provider_stop(provider);
    nfc_free(provider->nfc);
    furi_mutex_free(provider->mutex);
    free(provider);
}

void observation_provider_start(ObservationProvider* provider) {
    if(!provider) return;

    furi_mutex_acquire(provider->mutex, FuriWaitForever);
    bool idle = (provider->state == ProviderStateIdle);
    furi_mutex_release(provider->mutex);

    if(idle) {
        provider_start_scanner(provider);
    }
}

void observation_provider_stop(ObservationProvider* provider) {
    if(!provider) return;

    /* Atomically take ownership of any live scanner/poller and go idle. */
    furi_mutex_acquire(provider->mutex, FuriWaitForever);
    NfcScanner* sc = provider->scanner;
    NfcPoller* pl = provider->poller;
    provider->scanner = NULL;
    provider->poller = NULL;
    provider->state = ProviderStateIdle;
    furi_mutex_release(provider->mutex);

    /* Stop/free outside the mutex so callbacks can complete without deadlock. */
    if(sc) {
        nfc_scanner_stop(sc);
        nfc_scanner_free(sc);
    }
    if(pl) {
        nfc_poller_stop(pl);
        nfc_poller_free(pl);
    }
}

bool observation_provider_poll(ObservationProvider* provider, AccessObservation* out) {
    if(!provider || !out) return false;

    furi_mutex_acquire(provider->mutex, FuriWaitForever);
    ProviderState state = provider->state;
    furi_mutex_release(provider->mutex);

    /* ── ReadPending: scanner found a card, transition to poller ── */
    if(state == ProviderStateReadPending) {
        /* Atomically take the scanner and protocol info, mark as Reading. */
        furi_mutex_acquire(provider->mutex, FuriWaitForever);
        if(provider->state != ProviderStateReadPending) {
            /* Raced with stop() — nothing to do. */
            furi_mutex_release(provider->mutex);
            return false;
        }
        NfcScanner* sc = provider->scanner;
        provider->scanner = NULL;
        NfcProtocol proto = provider->uid_protocol;
        provider->state = ProviderStateReading;
        furi_mutex_release(provider->mutex);

        /* Stop scanner outside the mutex. */
        if(sc) {
            nfc_scanner_stop(sc);
            nfc_scanner_free(sc);
        }

        NfcGenericCallback cb = callback_for_protocol(proto);
        if(cb) {
            NfcPoller* pl = nfc_poller_alloc(provider->nfc, proto);

            furi_mutex_acquire(provider->mutex, FuriWaitForever);
            provider->poller = pl;
            furi_mutex_release(provider->mutex);

            if(pl) {
                nfc_poller_start(pl, cb, provider);
            } else {
                /* Poller allocation failed — restart scanner. */
                provider_start_scanner(provider);
            }
        } else {
            /* Unsupported transport protocol — restart scanner. */
            provider_start_scanner(provider);
        }
        return false;
    }

    /* ── Done: poller read succeeded ── */
    if(state == ProviderStateDone) {
        furi_mutex_acquire(provider->mutex, FuriWaitForever);
        if(provider->state != ProviderStateDone) {
            furi_mutex_release(provider->mutex);
            return false;
        }
        NfcPoller* pl = provider->poller;
        provider->poller = NULL;
        AccessObservation result = provider->pending;
        provider->state = ProviderStateIdle;
        furi_mutex_release(provider->mutex);

        /* Stop/free poller outside mutex. */
        if(pl) {
            nfc_poller_stop(pl);
            nfc_poller_free(pl);
        }

        if(observation_is_valid(&result)) {
            *out = result;
            return true;
        }

        /* Result was invalid — restart scanning automatically. */
        provider_start_scanner(provider);
        return false;
    }

    /* ── ReadFailed: poller hit an error, restart scanner ── */
    if(state == ProviderStateReadFailed) {
        furi_mutex_acquire(provider->mutex, FuriWaitForever);
        if(provider->state != ProviderStateReadFailed) {
            furi_mutex_release(provider->mutex);
            return false;
        }
        NfcPoller* pl = provider->poller;
        provider->poller = NULL;
        provider->state = ProviderStateScanning;
        furi_mutex_release(provider->mutex);

        if(pl) {
            nfc_poller_stop(pl);
            nfc_poller_free(pl);
        }

        provider_start_scanner(provider);
        return false;
    }

    return false;
}

Nfc* observation_provider_get_nfc(ObservationProvider* provider) {
    return provider ? provider->nfc : NULL;
}

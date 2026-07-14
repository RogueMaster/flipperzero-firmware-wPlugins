#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <toolbox/bit_buffer.h>
#include <string.h>
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
    /* Worker-thread-only: set when the Ultralight/NTAG default password auths. */
    bool ul_default_pwd_ok;
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
    /* Prefer ISO14443-4a for ISO-7816 smart cards (e.g. HID Seos) so we can
     * send APDUs to identify the applet. Non-Seos 4a cards fall back to the
     * generic ISO14443-A classification. */
    for(size_t i = 0; i < count; i++) {
        if(protos[i] == NfcProtocolIso14443_4a) return NfcProtocolIso14443_4a;
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

/* -------------------------------------------------------------------------
 * ISO14443-4a poller callback  (runs on NFC worker thread)
 *
 * For ISO-7816 smart cards (Seos and other 4-layer cards) we activate the
 * ISO14443-4a layer and attempt an APDU SELECT of the HID Seos application.
 * A 0x9000 response confirms HID Seos. This is a passive identify only — it
 * does NOT read PACS (facility code / card number), which requires a HID SAM
 * (see issue #53). Non-Seos 4a cards classify as generic ISO14443-A.
 * -------------------------------------------------------------------------
 */

/* HID Seos applet AID. */
static const uint8_t SEOS_AID[] = {0xA0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01};

static bool iso14443_4a_card_is_seos(Iso14443_4aPoller* poller) {
    /* ISO-7816 SELECT by name (P1=0x04): 00 A4 04 00 <Lc> <AID> 00 */
    uint8_t apdu[5 + sizeof(SEOS_AID) + 1];
    size_t n = 0;
    apdu[n++] = 0x00; /* CLA */
    apdu[n++] = 0xA4; /* INS: SELECT */
    apdu[n++] = 0x04; /* P1: select by name (AID) */
    apdu[n++] = 0x00; /* P2 */
    apdu[n++] = (uint8_t)sizeof(SEOS_AID); /* Lc */
    memcpy(&apdu[n], SEOS_AID, sizeof(SEOS_AID));
    n += sizeof(SEOS_AID);
    apdu[n++] = 0x00; /* Le */

    BitBuffer* tx = bit_buffer_alloc(sizeof(apdu));
    BitBuffer* rx = bit_buffer_alloc(64);
    bit_buffer_append_bytes(tx, apdu, n);

    bool is_seos = false;
    if(iso14443_4a_poller_send_block(poller, tx, rx) == Iso14443_4aErrorNone) {
        size_t len = bit_buffer_get_size_bytes(rx);
        /* Success = trailing status word 0x9000. */
        if(len >= 2 && bit_buffer_get_byte(rx, len - 2) == 0x90 &&
           bit_buffer_get_byte(rx, len - 1) == 0x00) {
            is_seos = true;
        }
    }

    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return is_seos;
}

static NfcCommand iso14443_4a_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    const Iso14443_4aPollerEvent* ev = (const Iso14443_4aPollerEvent*)event.event_data;

    if(ev->type != Iso14443_4aPollerEventTypeReady) {
        furi_mutex_acquire(p->mutex, FuriWaitForever);
        p->state = ProviderStateReadFailed;
        furi_mutex_release(p->mutex);
        return NfcCommandStop;
    }

    /* APDU exchange runs on the worker thread before we touch shared state. */
    bool seos = iso14443_4a_card_is_seos((Iso14443_4aPoller*)event.instance);

    furi_mutex_acquire(p->mutex, FuriWaitForever);

    const Iso14443_4aData* data = (const Iso14443_4aData*)nfc_poller_get_data(p->poller);
    if(data) {
        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(data->iso14443_3a_data, &uid_len);

        p->pending = (AccessObservation){0};
        p->pending.tech = TechTypeNfc13Mhz;
        p->pending.metadata_complete = true;
        if(seos) {
            p->pending.card_type = CardTypeSeos;
            /* Seos is an AES secure element — credential is in protected memory. */
            p->pending.user_memory_present = true;
        } else {
            p->pending.card_type = CardTypeIso14443A;
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
 * The poller fires RequestMode first (we ask for Read), then AuthRequest if the
 * card requires a password. We test the factory password FFFFFFFF only when the
 * config is readable and no AUTHLIM lockout is configured (non-destructive);
 * otherwise we skip auth. Then ReadSuccess (incl. after a successful default-
 * password auth) or ReadFailed/CardLocked (protected — salvage type + UID).
 * -------------------------------------------------------------------------
 */

static NfcCommand mf_ultralight_poller_cb(NfcGenericEvent event, void* context) {
    ObservationProvider* p = context;
    MfUltralightPollerEvent* ul_event = (MfUltralightPollerEvent*)event.event_data;

    /* These two events don't touch shared provider state — no mutex needed. */
    if(ul_event->type == MfUltralightPollerEventTypeRequestMode) {
        ul_event->data->poller_mode = MfUltralightPollerModeRead;
        p->ul_default_pwd_ok = false; /* reset per scan */
        return NfcCommandContinue;
    }
    if(ul_event->type == MfUltralightPollerEventTypeAuthRequest) {
        /* Config-gated default-password test (non-destructive): only send the
         * factory password FFFFFFFF if the card exposes its config pages AND has
         * no failed-auth lockout (AUTHLIM == 0), so a wrong guess can never trip
         * a lockout / brick the card. Otherwise skip auth entirely. */
        const MfUltralightData* cfg_data = (const MfUltralightData*)nfc_poller_get_data(p->poller);
        MfUltralightConfigPages* cfg = NULL;
        bool safe_to_test = cfg_data && mf_ultralight_get_config_page(cfg_data, &cfg) && cfg &&
                            cfg->access.authlim == 0;
        if(safe_to_test) {
            ul_event->data->auth_context.skip_auth = false;
            memset(
                ul_event->data->auth_context.password.data,
                0xFF,
                sizeof(ul_event->data->auth_context.password.data));
        } else {
            ul_event->data->auth_context.skip_auth = true;
        }
        return NfcCommandContinue;
    }
    if(ul_event->type == MfUltralightPollerEventTypeAuthSuccess) {
        /* The factory password (FFFFFFFF) we supplied was accepted. */
        p->ul_default_pwd_ok = true;
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
            p->pending.default_password_readable = p->ul_default_pwd_ok;

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
    } else if(
        ul_event->type == MfUltralightPollerEventTypeReadFailed ||
        ul_event->type == MfUltralightPollerEventTypeCardLocked) {
        /* Password-locked / protected Ultralight (e.g. MIFARE UL EV1 hotel keys
         * such as VingCard). The page read NAKs, but GET_VERSION + anticollision
         * already gave us the chip type and UID — classify from those instead of
         * looping forever on a read we don't need for an audit. The protected
         * pages are not a "secure element"; for scoring this stays a UID-based
         * credential (cloneable), which is the honest verdict for these cards. */
        const MfUltralightData* ul_data = (const MfUltralightData*)nfc_poller_get_data(p->poller);
        size_t uid_len = 0;
        const uint8_t* uid = ul_data ? mf_ultralight_get_uid(ul_data, &uid_len) : NULL;

        if(uid && uid_len > 0) {
            p->pending = (AccessObservation){0};
            p->pending.tech = TechTypeNfc13Mhz;
            p->pending.card_type = mf_ultralight_type_to_card_type(ul_data->type);
            p->pending.metadata_complete = true;
            p->pending.memory_locked = true;

            p->pending.uid_present = true;
            p->pending.uid_len = uid_len <= sizeof(p->pending.uid) ? uid_len :
                                                                     sizeof(p->pending.uid);
            for(size_t i = 0; i < p->pending.uid_len; i++) {
                p->pending.uid[i] = uid[i];
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
            /* SL3 = full AES + ISO14443-4 with protected application memory.
             * SL2 is the weak transitional mode (AES auth but Classic frame
             * structure, downgrade-prone), so it is NOT treated as having
             * protected memory — it scores MODERATE, not SECURE (see
             * docs/card-types.md). SL1 is Classic-compatible (no AES in use). */
            p->pending.user_memory_present = (plus_data->security_level == MfPlusSecurityLevel3);

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

/* Well-known public MIFARE Classic keys, each tried as both key A and key B.
 * Covers the standard mfoc / Proxmark3 default-key set (incl. 4D3A99C351DD and
 * 1A982C7E459A) plus the NXP MAD / NFC Forum NDEF keys and common vendor
 * defaults. Kept small so the per-sector auth sweep stays fast. */
static const uint8_t MF_CLASSIC_DEFAULT_KEYS[][MF_CLASSIC_KEY_SIZE] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* FFFFFFFFFFFF — factory transport default */
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}, /* A0A1A2A3A4A5 — MAD key A (NXP AN10787)   */
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}, /* D3F7D3F7D3F7 — NFC Forum NDEF public key */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 000000000000 — blanked / all-zero key    */
    {0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0}, /* A0B0C0D0E0F0 — common vendor default      */
    {0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1}, /* A1B1C1D1E1F1 — common vendor default      */
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5}, /* B0B1B2B3B4B5 — common vendor default      */
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, /* AABBCCDDEEFF — common vendor default      */
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD}, /* 4D3A99C351DD — mfoc/Proxmark public key   */
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A}, /* 1A982C7E459A — mfoc/Proxmark public key   */
};
static const MfClassicKeyType MF_CLASSIC_KEY_TYPES[] = {MfClassicKeyTypeA, MfClassicKeyTypeB};

/* Try every default key (as key A and key B) against the sector that contains
 * block_num. Returns true at the first key that authenticates. Re-activates the
 * card on each attempt (skip_module_activation=false) so it is safe to call
 * across sectors in sequence. */
static bool mf_classic_block_has_default_key(MfClassicPoller* poller, uint8_t block_num) {
    const size_t num_keys = sizeof(MF_CLASSIC_DEFAULT_KEYS) / sizeof(MF_CLASSIC_DEFAULT_KEYS[0]);
    const size_t num_types = sizeof(MF_CLASSIC_KEY_TYPES) / sizeof(MF_CLASSIC_KEY_TYPES[0]);
    for(size_t k = 0; k < num_keys; k++) {
        for(size_t t = 0; t < num_types; t++) {
            MfClassicKey key;
            memcpy(key.data, MF_CLASSIC_DEFAULT_KEYS[k], MF_CLASSIC_KEY_SIZE);
            MfClassicAuthContext auth_ctx;
            if(mf_classic_poller_auth(
                   poller, block_num, &key, MF_CLASSIC_KEY_TYPES[t], &auth_ctx, false) ==
               MfClassicErrorNone) {
                return true;
            }
        }
    }
    return false;
}

/* -------------------------------------------------------------------------
 * MfClassic poller callback  (runs on NFC worker thread)
 *
 * Uses MfClassicPollerModeRead. The poller fires RequestReadSector starting
 * from sector 0. Sector 0 (the access-control sector) is always tested against
 * the default-key list; only if it still uses a default key do we sweep the
 * remaining sectors and count how many are on default keys (N/M). A re-keyed
 * sector 0 means a managed card and returns fast. No sector data is retained.
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

    /* Card detected event fires before the first RequestReadSector — let it pass. */
    if(cl_event->type == MfClassicPollerEventTypeCardDetected) {
        return NfcCommandContinue;
    }

    /* DataUpdate fires periodically during MfClassicPollerModeRead to report
     * progress. We stop after sector 0 so it rarely fires, but handle it
     * explicitly to avoid hitting the catch-all before sector 0 is reached. */
    if(cl_event->type == MfClassicPollerEventTypeDataUpdate) {
        return NfcCommandContinue;
    }

    if(cl_event->type == MfClassicPollerEventTypeRequestReadSector &&
       cl_event->data->read_sector_request_data.sector_num == 0) {
        MfClassicPoller* cl_poller = (MfClassicPoller*)event.instance;

        /* Card type → sector count (read before auth; the data buffer stays
         * valid afterwards for UID/SAK/ATQA). */
        const MfClassicData* cl_data = (const MfClassicData*)nfc_poller_get_data(p->poller);
        uint8_t total_sectors = cl_data ? mf_classic_get_total_sectors_num(cl_data->type) : 0;

        /* Always test sector 0 against the full key list. Only if it is still on
         * a default key do we sweep the remaining sectors and count how many use
         * default keys — a re-keyed sector 0 (managed card) returns fast. */
        bool default_key_found = mf_classic_block_has_default_key(cl_poller, 0);
        uint8_t default_key_sectors = default_key_found ? 1 : 0;
        if(default_key_found) {
            for(uint8_t s = 1; s < total_sectors; s++) {
                uint8_t block = mf_classic_get_first_block_num_of_sector(s);
                if(mf_classic_block_has_default_key(cl_poller, block)) default_key_sectors++;
            }
        }

        mf_classic_poller_halt(cl_poller);

        furi_mutex_acquire(p->mutex, FuriWaitForever);

        if(cl_data) {
            size_t uid_len = 0;
            const uint8_t* uid = mf_classic_get_uid(cl_data, &uid_len);
            const Iso14443_3aData* iso_data = mf_classic_get_base_data(cl_data);

            p->pending = (AccessObservation){0};
            p->pending.tech = TechTypeNfc13Mhz;
            p->pending.metadata_complete = true;
            p->pending.default_keys_readable = default_key_found;
            p->pending.total_sectors = total_sectors;
            p->pending.default_key_sectors = default_key_sectors;

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

    /* CardLost, Fail, or any unhandled event — do not overwrite a successful result. */
    furi_mutex_acquire(p->mutex, FuriWaitForever);
    if(p->state != ProviderStateDone) {
        p->state = ProviderStateReadFailed;
    }
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

        /* Classify by workflow type. Standard FeliCa has proprietary mutual
         * authentication protecting its blocks (scores SECURE); Lite has no
         * mutual auth — it is a UID-only credential (scores HIGH RISK). */
        if(fc_data->workflow_type == FelicaLite) {
            p->pending.card_type = CardTypeFeliCaLite;
        } else {
            p->pending.card_type = CardTypeFelica;
            p->pending.user_memory_present = true;
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
    case NfcProtocolIso14443_4a:
        return iso14443_4a_poller_cb;
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

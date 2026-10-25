#include "magic_tag.h"

#include <furi.h>
#include <nfc/nfc_poller.h>
#include <nfc/helpers/iso14443_crc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include <string.h>

typedef struct {
    FuriSemaphore* complete;
    NfcPoller* poller;
    bool gen1;
    bool gen3;
} IsoMagicProbe;

static NfcCommand iso_magic_probe_callback(NfcGenericEvent event, void* context) {
    IsoMagicProbe* probe = context;
    if(event.protocol != NfcProtocolIso15693_3) return NfcCommandContinue;
    Iso15693_3PollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso15693_3PollerEventTypeReady) return NfcCommandContinue;
    Iso15693_3Poller* poller = event.instance;
    Iso15693_3SystemInfo info = {0};
    if(iso15693_3_poller_get_system_info(poller, &info) == Iso15693_3ErrorNone) {
        const Iso15693_3Data* card = nfc_poller_get_data(probe->poller);
        size_t uid_len = 0;
        const uint8_t* uid = iso15693_3_get_uid(card, &uid_len);
        MagicGenType gen = mtools_detect_iso15693_ready(poller, &info, uid, uid_len);
        probe->gen1 = gen == MagicGenIso15693Gen1;
        probe->gen3 = gen == MagicGenIso15693Gen3;
    }
    furi_semaphore_release(probe->complete);
    return NfcCommandStop;
}

static bool detect_iso15693_fingerprint(Nfc* nfc, bool gen3) {
    IsoMagicProbe probe = {.complete = furi_semaphore_alloc(1, 0)};
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso15693_3);
    probe.poller = poller;
    nfc_poller_start(poller, iso_magic_probe_callback, &probe);
    bool complete = furi_semaphore_acquire(probe.complete, furi_ms_to_ticks(3000)) == FuriStatusOk;
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_semaphore_free(probe.complete);
    return complete && (gen3 ? probe.gen3 : probe.gen1);
}

/* Gen1 UID storage uses fixed addresses 0x38/0x39, even on variants
 * advertising fewer user blocks. Use unaddressed reads for clone firmware. */
static bool iso_gen1_hidden_uid_matches(
    Iso15693_3Poller* poller, const uint8_t uid[8]) {
    BitBuffer* tx = bit_buffer_alloc(3);
    BitBuffer* rx = bit_buffer_alloc(8);
    bool matches = true;
    for(uint8_t part = 0; part < 2 && matches; part++) {
        const uint8_t command[] = {0x02, 0x20, (uint8_t)(0x38 + part)};
        bit_buffer_copy_bytes(tx, command, sizeof(command));
        bit_buffer_reset(rx);
        Iso15693_3Error error = iso15693_3_poller_send_frame(poller, tx, rx, 1356000U);
        FURI_LOG_D(
            "MTools", "ISO15 hidden %02X: err=%u len=%u data=%02X %02X %02X %02X %02X",
            command[2], error, (unsigned)bit_buffer_get_size_bytes(rx),
            bit_buffer_get_size_bytes(rx) > 0 ? bit_buffer_get_byte(rx, 0) : 0,
            bit_buffer_get_size_bytes(rx) > 1 ? bit_buffer_get_byte(rx, 1) : 0,
            bit_buffer_get_size_bytes(rx) > 2 ? bit_buffer_get_byte(rx, 2) : 0,
            bit_buffer_get_size_bytes(rx) > 3 ? bit_buffer_get_byte(rx, 3) : 0,
            bit_buffer_get_size_bytes(rx) > 4 ? bit_buffer_get_byte(rx, 4) : 0);
        matches = error == Iso15693_3ErrorNone && bit_buffer_get_size_bytes(rx) == 5 &&
                  !(bit_buffer_get_byte(rx, 0) & 1U);
        for(size_t i = 0; i < 4 && matches; i++)
            matches = bit_buffer_get_byte(rx, 1 + i) == uid[7 - part * 4 - i];
    }
    bit_buffer_free(rx);
    bit_buffer_free(tx);
    return matches;
}

MagicGenType mtools_detect_iso15693_ready(
    Iso15693_3Poller* poller,
    const Iso15693_3SystemInfo* info,
    const uint8_t* uid,
    size_t uid_len) {
    if(!info) return MagicGenCount;
    FURI_LOG_I(
        "MTools",
        "ISO15 info: blocks=%u size=%u IC=%02X",
        info->block_count,
        info->block_size,
        info->ic_ref);
    /* IC reference and capacity are not magic fingerprints. Only probe the
     * fixed UID addresses when they lie beyond the advertised user memory;
     * matching bytes in ordinary user blocks are not proof of a backdoor.
     * Gen1 variants rejecting hidden reads remain unconfirmed. */
    if(uid && uid_len == 8 && info->block_size == 4 && info->block_count > 0 &&
       info->block_count <= 0x38 && iso_gen1_hidden_uid_matches(poller, uid))
        return MagicGenIso15693Gen1;
    if(info->block_count == 80 && info->block_size == 4) {
        uint8_t activation[8];
        static const uint8_t expected[8] = {0xA5, 0x2B, 0x44, 0x2C, 0x21, 0xAE, 0x93, 0x00};
        if(iso15693_3_poller_read_block(poller, activation, 0x14, 4) ==
               Iso15693_3ErrorNone &&
           iso15693_3_poller_read_block(poller, activation + 4, 0x15, 4) ==
               Iso15693_3ErrorNone &&
           memcmp(activation, expected, sizeof(expected)) == 0)
            return MagicGenIso15693Gen3;
    }
    /* Gen2 has no known read-only fingerprint. A normal ISO15693 card can have
     * the same reported memory layout, so exclusion is not proof of Gen2. */
    return MagicGenCount;
}

typedef struct {
    FuriSemaphore* complete;
    NfcPoller* poller;
    MagicGenType gen;
    uint8_t wakeup_first;
    bool match;
    uint8_t gen4_config[30];
} MfcMagicProbe;

static bool mfc_ack(const BitBuffer* rx) {
    return bit_buffer_get_size(rx) >= 4 && (bit_buffer_get_byte(rx, 0) & 0x0F) == 0x0A;
}

static NfcCommand mfc_magic_probe_callback(NfcGenericEvent event, void* context) {
    MfcMagicProbe* probe = context;
    if(event.protocol != NfcProtocolIso14443_3a) return NfcCommandContinue;
    Iso14443_3aPollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso14443_3aPollerEventTypeReady) return NfcCommandContinue;
    Iso14443_3aPoller* poller = event.instance;
    BitBuffer* tx = bit_buffer_alloc(32);
    BitBuffer* rx = bit_buffer_alloc(64);
    Iso14443_3aError error = Iso14443_3aErrorNotPresent;
    if(probe->gen == MagicGenMfcGdm && probe->wakeup_first == 0x80) {
        /* A GDM card with wakeup disabled still returns a four-byte nonce
         * for its proprietary 0x80 authentication command. No key is sent. */
        const uint8_t auth_command[2] = {0x80, 0x00};
        bit_buffer_copy_bytes(tx, auth_command, sizeof(auth_command));
        iso14443_crc_append(Iso14443CrcTypeA, tx);
        error = iso14443_3a_poller_txrx(poller, tx, rx, 1356000U);
        probe->match = error == Iso14443_3aErrorNone && bit_buffer_get_size(rx) == 32;
    } else if(probe->gen == MagicGenMfcGen1a || probe->gen == MagicGenMfcGdm) {
        iso14443_3a_poller_halt(poller);
        uint8_t wakeup = probe->wakeup_first;
        bit_buffer_copy_bytes(tx, &wakeup, 1);
        bit_buffer_set_size(tx, 7);
        error = iso14443_3a_poller_txrx(poller, tx, rx, 1356000U);
        if(error == Iso14443_3aErrorNone && mfc_ack(rx)) {
            wakeup = probe->wakeup_first == 0x20 ? 0x23 : 0x43;
            bit_buffer_copy_bytes(tx, &wakeup, 1);
            bit_buffer_reset(rx);
            error = iso14443_3a_poller_txrx(poller, tx, rx, 1356000U);
            if(error == Iso14443_3aErrorNone && mfc_ack(rx)) {
                /* Gen1a wakeup is a capability, also supported by some GDM cards. */
                if(probe->gen == MagicGenMfcGen1a) {
                    probe->match = true;
                    goto done;
                }
                const uint8_t config_command[2] = {0xE0, 0x00};
                bit_buffer_copy_bytes(tx, config_command, sizeof(config_command));
                bit_buffer_reset(rx);
                bool gdm = iso14443_3a_poller_send_standard_frame(
                               poller, tx, rx, 1356000U) == Iso14443_3aErrorNone &&
                           bit_buffer_get_size_bytes(rx) == 16;
                probe->match = gdm;
            }
        }
    } else if(probe->gen == MagicGenMfcGen3) {
        /* 30 00 is also a normal Ultralight/NTAG READ and returns 16 bytes.
         * Only probe cards advertising a Classic SAK. */
        const Iso14443_3aData* data = nfc_poller_get_data(probe->poller);
        uint8_t sak = iso14443_3a_get_sak(data);
        if(sak != 0x08 && sak != 0x18) goto done;
        const uint8_t command[2] = {0x30, 0x00};
        bit_buffer_copy_bytes(tx, command, sizeof(command));
        error = iso14443_3a_poller_send_standard_frame(poller, tx, rx, 1356000U);
        probe->match = error == Iso14443_3aErrorNone && bit_buffer_get_size_bytes(rx) == 16;
    } else if(probe->gen == MagicGenMfcGen4) {
        const uint8_t command[6] = {0xCF, 0, 0, 0, 0, 0xC6};
        bit_buffer_copy_bytes(tx, command, sizeof(command));
        error = iso14443_3a_poller_send_standard_frame(poller, tx, rx, 1356000U);
        probe->match = error == Iso14443_3aErrorNone && bit_buffer_get_size_bytes(rx) >= 30;
        if(probe->match) memcpy(probe->gen4_config, bit_buffer_get_data(rx), 30);
    } else if(probe->gen == MagicGenMfcGen2) {
        /* Like Proxmark3 MifareCIdent, issue RATS even when SAK does not
         * advertise ISO14443-4: some Classic Gen2 clones still return ATS. */
        const uint8_t rats[2] = {0xE0, 0x80};
        bit_buffer_copy_bytes(tx, rats, sizeof(rats));
        error = iso14443_3a_poller_send_standard_frame(poller, tx, rx, 1356000U);
        static const uint8_t ats_a[] = {0x09, 0x78, 0x00, 0x91, 0x02, 0xDA, 0xBC, 0x19, 0x10};
        /* PM3's 7B signature includes trailing CRC 56 3D. The standard
         * frame API strips CRC-A, so compare only the 13 ATS bytes. */
        static const uint8_t ats_b[] = {0x0D, 0x78, 0x00, 0x71, 0x02, 0x88, 0x49, 0xA1,
                                        0x30, 0x20, 0x15, 0x06, 0x08};
        size_t size = bit_buffer_get_size_bytes(rx);
        probe->match = error == Iso14443_3aErrorNone &&
                       ((size >= sizeof(ats_a) && memcmp(bit_buffer_get_data(rx), ats_a, sizeof(ats_a)) == 0) ||
                        (size >= sizeof(ats_b) && memcmp(bit_buffer_get_data(rx), ats_b, sizeof(ats_b)) == 0));
        FURI_LOG_I("MTools", "MFC Gen2 ATS: error=%u bytes=%u match=%u", error,
                   (unsigned)size, probe->match);
    }
done:
    bit_buffer_free(rx);
    bit_buffer_free(tx);
    furi_semaphore_release(probe->complete);
    return NfcCommandStop;
}

static bool detect_mfc_probe_with_config(
    Nfc* nfc, MagicGenType gen, uint8_t wakeup_first, uint8_t config[30]) {
    MfcMagicProbe probe = {
        .complete = furi_semaphore_alloc(1, 0), .gen = gen, .wakeup_first = wakeup_first};
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    probe.poller = poller;
    nfc_poller_start(poller, mfc_magic_probe_callback, &probe);
    bool complete = furi_semaphore_acquire(probe.complete, furi_ms_to_ticks(1500)) == FuriStatusOk;
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    if(complete && probe.match && config) memcpy(config, probe.gen4_config, 30);
    furi_semaphore_free(probe.complete);
    return complete && probe.match;
}

static bool detect_mfc_probe(Nfc* nfc, MagicGenType gen, uint8_t wakeup_first) {
    return detect_mfc_probe_with_config(nfc, gen, wakeup_first, NULL);
}

bool mtools_mfc_gen4_read_config(Nfc* nfc, uint8_t config[30]) {
    return nfc && config &&
           detect_mfc_probe_with_config(nfc, MagicGenMfcGen4, 0, config);
}

static bool detect_mfc_gen1a(Nfc* nfc) { return detect_mfc_probe(nfc, MagicGenMfcGen1a, 0x40); }

typedef struct {
    FuriSemaphore* complete;
    bool match;
} MfcGen2Probe;

static NfcCommand mfc_gen2_probe_callback(NfcGenericEvent event, void* context) {
    MfcGen2Probe* probe = context;
    if(event.protocol != NfcProtocolMfClassic) return NfcCommandContinue;
    MfClassicPollerEvent* poller_event = event.event_data;
    if(poller_event->type != MfClassicPollerEventTypeCardDetected) return NfcCommandContinue;

    MfClassicPoller* poller = event.instance;
    MfClassicKey key;
    memset(key.data, 0xFF, sizeof(key.data));
    MfClassicAuthContext auth = {0};
    MfClassicError auth_error = mf_classic_poller_auth(
        poller, 0, &key, MfClassicKeyTypeB, &auth, false);
    if(auth_error == MfClassicErrorNone) {
        BitBuffer* tx = bit_buffer_alloc(4);
        BitBuffer* rx = bit_buffer_alloc(4);
        const uint8_t command[] = {0xA0, 0x00};
        bit_buffer_copy_bytes(tx, command, sizeof(command));
        iso14443_crc_append(Iso14443CrcTypeA, tx);
        MfClassicError error = mf_classic_poller_send_encrypted_frame(poller, tx, rx, 1356000U);
        probe->match = error == MfClassicErrorNone && bit_buffer_get_size(rx) == 4 &&
                       (bit_buffer_get_byte(rx, 0) & 0x0F) == 0x0A;
        bit_buffer_free(rx);
        bit_buffer_free(tx);
        FURI_LOG_I("MTools", "MFC Gen2 authenticated A0: error=%u match=%u", error, probe->match);
        /* Stop after the command ACK. Never send the 16-byte write payload,
         * HALT, or another command while the tag expects write data. */
    } else {
        FURI_LOG_I("MTools", "MFC Gen2 default Key B auth failed: %u", auth_error);
    }
    furi_semaphore_release(probe->complete);
    return NfcCommandStop;
}

static bool detect_mfc_gen2(Nfc* nfc) {
    if(detect_mfc_probe(nfc, MagicGenMfcGen2, 0)) return true;
    MfcGen2Probe probe = {.complete = furi_semaphore_alloc(1, 0)};
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolMfClassic);
    nfc_poller_start(poller, mfc_gen2_probe_callback, &probe);
    bool complete = furi_semaphore_acquire(probe.complete, furi_ms_to_ticks(1500)) == FuriStatusOk;
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_semaphore_free(probe.complete);
    return complete && probe.match;
}
static bool detect_mfc_gen3(Nfc* nfc) { return detect_mfc_probe(nfc, MagicGenMfcGen3, 0); }

static bool detect_mfc_gen4(Nfc* nfc) {
    return detect_mfc_probe(nfc, MagicGenMfcGen4, 0);
}

static bool detect_mfc_gdm(Nfc* nfc) {
    return detect_mfc_probe(nfc, MagicGenMfcGdm, 0x80) ||
           detect_mfc_probe(nfc, MagicGenMfcGdm, 0x20) ||
           detect_mfc_probe(nfc, MagicGenMfcGdm, 0x40);
}

static bool detect_iso15693_gen1(Nfc* nfc) {
    return detect_iso15693_fingerprint(nfc, false);
}

static bool detect_iso15693_gen3(Nfc* nfc) {
    return detect_iso15693_fingerprint(nfc, true);
}

typedef struct {
    FuriSemaphore* complete;
    NfcPoller* poller;
    bool compatible_write;
    bool matches;
} MfuMagicProbe;

static NfcCommand mfu_magic_probe_callback(NfcGenericEvent event, void* context) {
    MfuMagicProbe* probe = context;
    if(event.protocol != NfcProtocolIso14443_3a) return NfcCommandContinue;
    Iso14443_3aPollerEvent* ready = event.event_data;
    if(ready->type != Iso14443_3aPollerEventTypeReady) return NfcCommandContinue;
    const Iso14443_3aData* card = nfc_poller_get_data(probe->poller);
    size_t length = 0;
    iso14443_3a_get_uid(card, &length);
    if(length == 7 && iso14443_3a_get_sak(card) == 0) {
        BitBuffer* tx = bit_buffer_alloc(32);
        BitBuffer* rx = bit_buffer_alloc(32);
        if(!probe->compatible_write) {
            /* Proxmark3 ul_magic_test: 14 bytes on air, including CRC-A.
             * send_standard_frame validates/removes CRC, leaving 12 bytes. */
            static const uint8_t ats[] = {
                0x0A, 0x78, 0x00, 0x81, 0x02, 0xDB, 0xA0, 0xC1, 0x19, 0x40, 0x2A, 0xB5};
            const uint8_t rats[] = {0xE0, 0x80};
            bit_buffer_copy_bytes(tx, rats, sizeof(rats));
            Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
                event.instance, tx, rx, 1356000U);
            probe->matches = error == Iso14443_3aErrorNone &&
                             bit_buffer_get_size_bytes(rx) == sizeof(ats) &&
                             memcmp(bit_buffer_get_data(rx), ats, sizeof(ats)) == 0;
            FURI_LOG_I("MTools", "MFU Gen2 ATS: error=%u bytes=%u match=%u", error,
                       (unsigned)bit_buffer_get_size_bytes(rx), probe->matches);
        } else {
            /* Compatibility WRITE stage 1 only, as described by Proxmark3.
             * Never send its 16-byte data stage or an A2 page write. Stop the
             * poller immediately after ACK/NACK to cancel pending writes. */
            const uint8_t command[] = {0xA0, 0x00};
            bit_buffer_copy_bytes(tx, command, sizeof(command));
            iso14443_crc_append(Iso14443CrcTypeA, tx);
            Iso14443_3aError error = iso14443_3a_poller_txrx(
                event.instance, tx, rx, 1356000U);
            probe->matches = error == Iso14443_3aErrorNone &&
                             bit_buffer_get_size(rx) == 4 &&
                             (bit_buffer_get_byte(rx, 0) & 0x0F) == 0x0A;
            FURI_LOG_I("MTools", "MFU Gen2 A0 probe: error=%u bits=%u ack=%u", error,
                       (unsigned)bit_buffer_get_size(rx), probe->matches);
        }
        bit_buffer_free(rx);
        bit_buffer_free(tx);
    }
    furi_semaphore_release(probe->complete);
    return NfcCommandStop;
}

static bool detect_mfu_direct_write(Nfc* nfc) {
    MfuMagicProbe probe = {.complete = furi_semaphore_alloc(1, 0)};
    for(uint8_t stage = 0; stage < 2; stage++) {
        probe.compatible_write = stage == 1;
        probe.poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(probe.poller, mfu_magic_probe_callback, &probe);
        bool completed = furi_semaphore_acquire(probe.complete, furi_ms_to_ticks(2000)) == FuriStatusOk;
        nfc_poller_stop(probe.poller);
        nfc_poller_free(probe.poller);
        if(completed && probe.matches) break;
        if(stage == 0) furi_delay_ms(80);
    }
    furi_semaphore_free(probe.complete);
    return probe.matches;
}

bool mtools_detect_magic_tag(Nfc* nfc, MagicGenType gen) {
    if(!nfc) return false;
    switch(gen) {
    case MagicGenMfcGen1a: return detect_mfc_gen1a(nfc);
    case MagicGenMfcGen2: return detect_mfc_gen2(nfc);
    case MagicGenMfcGen3: return detect_mfc_gen3(nfc);
    case MagicGenMfcGen4: return detect_mfc_gen4(nfc);
    case MagicGenMfcGdm: return detect_mfc_gdm(nfc);
    case MagicGenIso15693Gen1: return detect_iso15693_gen1(nfc);
    case MagicGenIso15693Gen2: return false;
    case MagicGenIso15693Gen3: return detect_iso15693_gen3(nfc);
    case MagicGenMfuDirectWrite: return detect_mfu_direct_write(nfc);
    case MagicGenCount: break;
    }
    return false;
}

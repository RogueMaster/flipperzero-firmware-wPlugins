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
        probe->gen1 = info.block_count == 28 && info.ic_ref == 0x01;
        if(!probe->gen1 && info.block_size == 4 && info.block_count < 254) {
            uint8_t hidden[8];
            if(iso15693_3_poller_read_block(poller, hidden, info.block_count, 4) ==
                   Iso15693_3ErrorNone &&
               iso15693_3_poller_read_block(poller, hidden + 4, info.block_count + 1, 4) ==
                   Iso15693_3ErrorNone) {
                const Iso15693_3Data* card = nfc_poller_get_data(probe->poller);
                size_t uid_len = 0;
                const uint8_t* uid = iso15693_3_get_uid(card, &uid_len);
                uint8_t reconstructed[8];
                for(size_t i = 0; i < 4; i++) {
                    reconstructed[i] = hidden[7 - i];
                    reconstructed[4 + i] = hidden[3 - i];
                }
                probe->gen1 = uid_len == 8 && memcmp(reconstructed, uid, 8) == 0;
            }
        }
        if(info.block_count == 80 && info.block_size == 4) {
            uint8_t activation[8];
            static const uint8_t expected[8] = {0xA5, 0x2B, 0x44, 0x2C, 0x21, 0xAE, 0x93, 0x00};
            probe->gen3 = iso15693_3_poller_read_block(poller, activation, 0x14, 4) == Iso15693_3ErrorNone &&
                          iso15693_3_poller_read_block(poller, activation + 4, 0x15, 4) == Iso15693_3ErrorNone &&
                          memcmp(activation, expected, sizeof(expected)) == 0;
        }
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
    if(info->block_count == 28 && info->ic_ref == 0x01) return MagicGenIso15693Gen1;
    if(uid && uid_len == 8 && info->block_size == 4 && info->block_count < 254) {
        uint8_t hidden[8];
        if(iso15693_3_poller_read_block(poller, hidden, info->block_count, 4) ==
               Iso15693_3ErrorNone &&
           iso15693_3_poller_read_block(poller, hidden + 4, info->block_count + 1, 4) ==
               Iso15693_3ErrorNone) {
            uint8_t reconstructed[8];
            for(size_t i = 0; i < 4; i++) {
                reconstructed[i] = hidden[7 - i];
                reconstructed[4 + i] = hidden[3 - i];
            }
            if(memcmp(reconstructed, uid, 8) == 0) return MagicGenIso15693Gen1;
        }
    }
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
    if(probe->gen == MagicGenMfcGtu && probe->wakeup_first == 0x80) {
        /* A GDM card with wakeup disabled still returns a four-byte nonce
         * for its proprietary 0x80 authentication command. No key is sent. */
        const uint8_t auth_command[2] = {0x80, 0x00};
        bit_buffer_copy_bytes(tx, auth_command, sizeof(auth_command));
        iso14443_crc_append(Iso14443CrcTypeA, tx);
        error = iso14443_3a_poller_txrx(poller, tx, rx, 1356000U);
        probe->match = error == Iso14443_3aErrorNone && bit_buffer_get_size(rx) == 32;
    } else if(probe->gen == MagicGenMfcGen1a || probe->gen == MagicGenMfcGtu) {
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
                const uint8_t config_command[2] = {0xE0, 0x00};
                bit_buffer_copy_bytes(tx, config_command, sizeof(config_command));
                bit_buffer_reset(rx);
                bool gdm = iso14443_3a_poller_send_standard_frame(
                               poller, tx, rx, 1356000U) == Iso14443_3aErrorNone &&
                           bit_buffer_get_size_bytes(rx) == 16;
                probe->match = probe->gen == MagicGenMfcGtu ? gdm : !gdm;
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
        /* Only known ATS signatures are positive evidence. Most CUID cards
         * have no read-only signature and remain unconfirmed. */
        const uint8_t rats[2] = {0xE0, 0x80};
        const Iso14443_3aData* data = nfc_poller_get_data(probe->poller);
        if(iso14443_3a_get_sak(data) & 0x20U) {
            bit_buffer_copy_bytes(tx, rats, sizeof(rats));
            error = iso14443_3a_poller_send_standard_frame(poller, tx, rx, 1356000U);
        }
        static const uint8_t ats_a[] = {0x09, 0x78, 0x00, 0x91, 0x02, 0xDA, 0xBC, 0x19, 0x10};
        static const uint8_t ats_b[] = {0x0D, 0x78, 0x00, 0x71, 0x02, 0x88, 0x49, 0xA1,
                                        0x30, 0x20, 0x15, 0x06, 0x08, 0x56, 0x3D};
        size_t size = bit_buffer_get_size_bytes(rx);
        probe->match = error == Iso14443_3aErrorNone &&
                       ((size >= sizeof(ats_a) && memcmp(bit_buffer_get_data(rx), ats_a, sizeof(ats_a)) == 0) ||
                        (size >= sizeof(ats_b) && memcmp(bit_buffer_get_data(rx), ats_b, sizeof(ats_b)) == 0));
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
    if(mf_classic_poller_auth(poller, 0, &key, MfClassicKeyTypeB, &auth, false) ==
       MfClassicErrorNone) {
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
        /* Stop after the command ACK. Never send the 16-byte write payload. */
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

static bool detect_mfc_gtu(Nfc* nfc) {
    return detect_mfc_probe(nfc, MagicGenMfcGtu, 0x80) ||
           detect_mfc_probe(nfc, MagicGenMfcGtu, 0x20) ||
           detect_mfc_probe(nfc, MagicGenMfcGtu, 0x40);
}

static bool detect_iso15693_gen1(Nfc* nfc) {
    return detect_iso15693_fingerprint(nfc, false);
}

static bool detect_iso15693_gen2(Nfc* nfc) {
    (void)nfc;
    /* TODO: Report Gen2 only after excluding Gen1/Gen3; no positive read-only signature. */
    return false;
}

static bool detect_iso15693_gen3(Nfc* nfc) {
    return detect_iso15693_fingerprint(nfc, true);
}

bool mtools_detect_magic_tag(Nfc* nfc, MagicGenType gen) {
    if(!nfc) return false;
    switch(gen) {
    case MagicGenMfcGen1a: return detect_mfc_gen1a(nfc);
    case MagicGenMfcGen2: return detect_mfc_gen2(nfc);
    case MagicGenMfcGen3: return detect_mfc_gen3(nfc);
    case MagicGenMfcGen4: return detect_mfc_gen4(nfc);
    case MagicGenMfcGtu: return detect_mfc_gtu(nfc);
    case MagicGenIso15693Gen1: return detect_iso15693_gen1(nfc);
    case MagicGenIso15693Gen2: return detect_iso15693_gen2(nfc);
    case MagicGenIso15693Gen3: return detect_iso15693_gen3(nfc);
    case MagicGenCount: break;
    }
    return false;
}

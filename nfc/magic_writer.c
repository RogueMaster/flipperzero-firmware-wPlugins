#include "magic_tag.h"

#include <string.h>
#include <stdio.h>
#include <furi.h>
#include <furi_hal_random.h>
#include <bit_lib/bit_lib.h>
#include <nfc/nfc_poller.h>
#include <nfc/helpers/crypto1.h>
#include <nfc/helpers/iso14443_crc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#define MFC_BLOCK0_SIZE 16
static char magic_write_error[26];

const char* mtools_magic_write_error(void) {
    return magic_write_error;
}

static uint8_t mfc_uid_bcc(const uint8_t uid[4]) {
    return uid[0] ^ uid[1] ^ uid[2] ^ uid[3];
}

/* Update the UID and recompute the four-byte Classic BCC in a prepared block 0. */
void mtools_mfc_prepare_block0(uint8_t block0[MFC_BLOCK0_SIZE], const uint8_t uid[4]) {
    memcpy(block0, uid, 4);
    block0[4] = mfc_uid_bcc(uid);
}

typedef struct {
    FuriSemaphore* complete;
    NfcPoller* poller;
    MagicGenType gen;
    uint8_t wakeup_first;
    uint8_t uid[10];
    uint8_t uid_len;
    uint8_t block0[16];
    bool success;
    bool length_mismatch;
    uint8_t gen3_failure;
    uint8_t gen4_failure;
    uint8_t gdm_failure;
    uint8_t gen4_config[30];
    uint8_t gdm_original_config[16];
    bool gdm_bridged;
} MfcRawWrite;

typedef struct {
    FuriSemaphore* complete;
    NfcPoller* poller;
    uint8_t original_config[16];
    bool success;
} MfcGdmBridge;

static bool mfc_gdm_crypto_ack(
    Iso14443_3aPoller* poller, Crypto1* crypto,
    BitBuffer* plain, BitBuffer* encrypted, BitBuffer* response, BitBuffer* decrypted,
    const uint8_t* command, size_t length) {
    bit_buffer_copy_bytes(plain, command, length);
    iso14443_crc_append(Iso14443CrcTypeA, plain);
    crypto1_encrypt(crypto, NULL, plain, encrypted);
    bit_buffer_reset(response);
    if(iso14443_3a_poller_txrx_custom_parity(poller, encrypted, response, 1356000U) !=
           Iso14443_3aErrorNone || bit_buffer_get_size(response) != 4) return false;
    crypto1_decrypt(crypto, response, decrypted);
    return (bit_buffer_get_byte(decrypted, 0) & 0x0F) == 0x0A;
}

static bool mfc_gdm_crypto_read_config(
    Iso14443_3aPoller* poller, Crypto1* crypto,
    BitBuffer* plain, BitBuffer* encrypted, BitBuffer* response, BitBuffer* decrypted,
    uint8_t config[16]) {
    const uint8_t command[2] = {0xE0, 0x00};
    bit_buffer_copy_bytes(plain, command, sizeof(command));
    iso14443_crc_append(Iso14443CrcTypeA, plain);
    crypto1_encrypt(crypto, NULL, plain, encrypted);
    bit_buffer_reset(response);
    if(iso14443_3a_poller_txrx_custom_parity(poller, encrypted, response, 1356000U) !=
           Iso14443_3aErrorNone || bit_buffer_get_size_bytes(response) != 18) return false;
    crypto1_decrypt(crypto, response, decrypted);
    if(!iso14443_crc_check(Iso14443CrcTypeA, decrypted)) return false;
    iso14443_crc_trim(decrypted);
    bit_buffer_write_bytes(decrypted, config, 16);
    return true;
}

static NfcCommand mfc_gdm_bridge_callback(NfcGenericEvent event, void* context) {
    MfcGdmBridge* bridge = context;
    if(event.protocol != NfcProtocolIso14443_3a) return NfcCommandContinue;
    Iso14443_3aPollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso14443_3aPollerEventTypeReady) return NfcCommandContinue;
    Iso14443_3aPoller* poller = event.instance;
    BitBuffer* plain = bit_buffer_alloc(32);
    BitBuffer* encrypted = bit_buffer_alloc(32);
    BitBuffer* response = bit_buffer_alloc(32);
    BitBuffer* decrypted = bit_buffer_alloc(32);
    Crypto1* crypto = crypto1_alloc();
    do {
        const uint8_t auth_command[2] = {0x80, 0x00};
        bit_buffer_copy_bytes(plain, auth_command, sizeof(auth_command));
        iso14443_crc_append(Iso14443CrcTypeA, plain);
        bit_buffer_reset(response);
        if(iso14443_3a_poller_txrx(poller, plain, response, 1356000U) !=
               Iso14443_3aErrorNone || bit_buffer_get_size_bytes(response) != 4) break;
        uint8_t nonce[4];
        bit_buffer_write_bytes(response, nonce, sizeof(nonce));
        uint8_t reader_nonce[4];
        furi_hal_random_fill_buf(reader_nonce, sizeof(reader_nonce));
        const Iso14443_3aData* card = nfc_poller_get_data(bridge->poller);
        crypto1_encrypt_reader_nonce(
            crypto, 0, iso14443_3a_get_cuid(card), nonce, reader_nonce, encrypted, false);
        bit_buffer_reset(response);
        if(iso14443_3a_poller_txrx_custom_parity(poller, encrypted, response, 1356000U) !=
               Iso14443_3aErrorNone || bit_buffer_get_size_bytes(response) != 4) break;
        crypto1_word(crypto, 0, 0);
        if(!mfc_gdm_crypto_read_config(
               poller, crypto, plain, encrypted, response, decrypted,
               bridge->original_config)) break;
        uint8_t temporary_config[16];
        memcpy(temporary_config, bridge->original_config, 16);
        temporary_config[0] = 0x7A;
        temporary_config[1] = 0xFF;
        if(memcmp(temporary_config, bridge->original_config, 16) != 0) {
            const uint8_t write_config[2] = {0xE1, 0x00};
            if(!mfc_gdm_crypto_ack(
                   poller, crypto, plain, encrypted, response, decrypted,
                   write_config, sizeof(write_config)) ||
               !mfc_gdm_crypto_ack(
                   poller, crypto, plain, encrypted, response, decrypted,
                   temporary_config, sizeof(temporary_config))) break;
            uint8_t observed[16];
            if(!mfc_gdm_crypto_read_config(
                   poller, crypto, plain, encrypted, response, decrypted, observed) ||
               memcmp(observed, temporary_config, 16) != 0) break;
        }
        bridge->success = true;
    } while(false);
    crypto1_free(crypto);
    bit_buffer_free(decrypted);
    bit_buffer_free(response);
    bit_buffer_free(encrypted);
    bit_buffer_free(plain);
    furi_semaphore_release(bridge->complete);
    return NfcCommandStop;
}

static bool mfc_gdm_open_wakeup(Nfc* nfc, uint8_t original_config[16]) {
    MfcGdmBridge bridge = {.complete = furi_semaphore_alloc(1, 0)};
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    bridge.poller = poller;
    nfc_poller_start(poller, mfc_gdm_bridge_callback, &bridge);
    bool completed = furi_semaphore_acquire(
        bridge.complete, furi_ms_to_ticks(2000)) == FuriStatusOk;
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_semaphore_free(bridge.complete);
    if(completed && bridge.success) memcpy(original_config, bridge.original_config, 16);
    return completed && bridge.success;
}

static bool mfc_raw_ack(
    Iso14443_3aPoller* poller, BitBuffer* tx, BitBuffer* rx,
    const uint8_t* command, size_t length, bool append_crc, bool short_frame) {
    bit_buffer_copy_bytes(tx, command, length);
    if(short_frame) bit_buffer_set_size(tx, 7);
    if(append_crc) iso14443_crc_append(Iso14443CrcTypeA, tx);
    bit_buffer_reset(rx);
    return iso14443_3a_poller_txrx(poller, tx, rx, 1356000U) == Iso14443_3aErrorNone &&
           bit_buffer_get_size(rx) >= 4 && (bit_buffer_get_byte(rx, 0) & 0x0F) == 0x0A;
}

static bool mfc_gen3_command(
    Iso14443_3aPoller* poller, BitBuffer* tx, BitBuffer* rx,
    const uint8_t* command, size_t length, uint32_t timeout_cycles) {
    bit_buffer_copy_bytes(tx, command, length);
    /* Gen3 status is a bare 90 00 response; it has no ISO14443-A CRC. */
    iso14443_crc_append(Iso14443CrcTypeA, tx);
    bit_buffer_reset(rx);
    Iso14443_3aError error = iso14443_3a_poller_txrx(poller, tx, rx, timeout_cycles);
    return error == Iso14443_3aErrorNone && bit_buffer_get_size_bytes(rx) >= 2 &&
           bit_buffer_get_byte(rx, 0) == 0x90 && bit_buffer_get_byte(rx, 1) == 0x00;
}

static bool mfc_gdm_wakeup(
    Iso14443_3aPoller* poller, BitBuffer* tx, BitBuffer* rx, uint8_t first) {
    iso14443_3a_poller_halt(poller);
    const uint8_t second = first == 0x20 ? 0x23 : 0x43;
    return mfc_raw_ack(poller, tx, rx, &first, 1, false, true) &&
           mfc_raw_ack(poller, tx, rx, &second, 1, false, false);
}

static bool mfc_gdm_read_active(
    Iso14443_3aPoller* poller, BitBuffer* tx, BitBuffer* rx,
    uint8_t command, uint8_t block, uint8_t data[16]) {
    const uint8_t request[2] = {command, block};
    bit_buffer_copy_bytes(tx, request, sizeof(request));
    bit_buffer_reset(rx);
    if(iso14443_3a_poller_send_standard_frame(poller, tx, rx, 1356000U) !=
           Iso14443_3aErrorNone || bit_buffer_get_size_bytes(rx) != 16) return false;
    memcpy(data, bit_buffer_get_data(rx), 16);
    return true;
}

static bool mfc_gdm_write_active(
    Iso14443_3aPoller* poller, BitBuffer* tx, BitBuffer* rx,
    uint8_t command, const uint8_t data[16], uint8_t read_command) {
    const uint8_t request[2] = {command, 0};
    uint8_t observed[16];
    return mfc_raw_ack(poller, tx, rx, request, sizeof(request), true, false) &&
           mfc_raw_ack(poller, tx, rx, data, 16, true, false) &&
           mfc_gdm_read_active(poller, tx, rx, read_command, 0, observed) &&
           memcmp(observed, data, 16) == 0;
}

static bool mfc_gen4_command(
    Iso14443_3aPoller* poller, BitBuffer* tx, BitBuffer* rx,
    const uint8_t* command, size_t length) {
    bit_buffer_copy_bytes(tx, command, length);
    iso14443_crc_append(Iso14443CrcTypeA, tx);
    bit_buffer_reset(rx);
    Iso14443_3aError error = iso14443_3a_poller_txrx(poller, tx, rx, 13560000U);
    return error == Iso14443_3aErrorNone &&
           ((bit_buffer_get_size(rx) >= 16 && bit_buffer_get_byte(rx, 0) == 0x90 &&
             bit_buffer_get_byte(rx, 1) == 0x00) ||
            (bit_buffer_get_size(rx) == 4 && (bit_buffer_get_byte(rx, 0) & 0x0F) == 0x0A));
}

typedef struct {
    FuriSemaphore* complete;
    uint8_t block0[16];
    bool success;
} MfcGen4Readback;

static NfcCommand mfc_gen4_readback_callback(NfcGenericEvent event, void* context) {
    MfcGen4Readback* readback = context;
    if(event.protocol != NfcProtocolIso14443_3a) return NfcCommandContinue;
    Iso14443_3aPollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso14443_3aPollerEventTypeReady) return NfcCommandContinue;
    Iso14443_3aPoller* poller = event.instance;
    BitBuffer* tx = bit_buffer_alloc(16);
    BitBuffer* rx = bit_buffer_alloc(32);
    const uint8_t read_block0[7] = {0xCF, 0, 0, 0, 0, 0xCE, 0x00};
    bit_buffer_copy_bytes(tx, read_block0, sizeof(read_block0));
    readback->success = iso14443_3a_poller_send_standard_frame(poller, tx, rx, 1356000U) ==
                            Iso14443_3aErrorNone && bit_buffer_get_size_bytes(rx) == 16;
    if(readback->success) memcpy(readback->block0, bit_buffer_get_data(rx), 16);
    bit_buffer_free(rx);
    bit_buffer_free(tx);
    furi_semaphore_release(readback->complete);
    return NfcCommandStop;
}

static bool mfc_gen4_read_block0(Nfc* nfc, uint8_t block0[16]) {
    MfcGen4Readback readback = {.complete = furi_semaphore_alloc(1, 0)};
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(poller, mfc_gen4_readback_callback, &readback);
    bool completed = furi_semaphore_acquire(readback.complete, furi_ms_to_ticks(1500)) == FuriStatusOk;
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_semaphore_free(readback.complete);
    if(completed && readback.success) memcpy(block0, readback.block0, 16);
    return completed && readback.success;
}

static NfcCommand mfc_raw_write_callback(NfcGenericEvent event, void* context) {
    MfcRawWrite* write = context;
    if(event.protocol != NfcProtocolIso14443_3a) return NfcCommandContinue;
    Iso14443_3aPollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso14443_3aPollerEventTypeReady) return NfcCommandContinue;
    const Iso14443_3aData* card = nfc_poller_get_data(write->poller);
    size_t current_uid_len = 0;
    iso14443_3a_get_uid(card, &current_uid_len);
    if(current_uid_len != write->uid_len && write->gen != MagicGenMfcGen4 &&
       write->gen != MagicGenMfcGtu) {
        write->length_mismatch = true;
        furi_semaphore_release(write->complete);
        return NfcCommandStop;
    }
    Iso14443_3aPoller* poller = event.instance;
    BitBuffer* tx = bit_buffer_alloc(32);
    BitBuffer* rx = bit_buffer_alloc(32);
    bool success = false;
    if(write->gen == MagicGenMfcGen1a) {
        iso14443_3a_poller_halt(poller);
        const uint8_t wakeup_a = write->wakeup_first;
        const uint8_t wakeup_b = wakeup_a == 0x20 ? 0x23 : 0x43;
        const uint8_t write_block0[2] = {0xA0, 0x00};
        success = mfc_raw_ack(poller, tx, rx, &wakeup_a, 1, false, true) &&
                  mfc_raw_ack(poller, tx, rx, &wakeup_b, 1, false, false) &&
                  mfc_raw_ack(poller, tx, rx, write_block0, 2, true, false) &&
                  mfc_raw_ack(poller, tx, rx, write->block0, 16, true, false);
    } else if(write->gen == MagicGenMfcGen3) {
        /* Detection already read block 0 in a separate poller session.
         * Start the write with a fresh card activation, as PN532 does. */
        uint8_t set_uid[5 + 7] = {0x90, 0xFB, 0xCC, 0xCC, 0x07};
        memcpy(set_uid + 5, write->uid, write->uid_len);
        success = mfc_gen3_command(poller, tx, rx, set_uid, 5 + write->uid_len, 27120000U);
        if(!success) write->gen3_failure = 1;
        if(success) {
            uint8_t set_block0[21] = {0x90, 0xF0, 0xCC, 0xCC, 0x10};
            memcpy(set_block0 + 5, write->block0, 16);
            success = mfc_gen3_command(poller, tx, rx, set_block0, sizeof(set_block0), 13560000U);
            if(!success) write->gen3_failure = 2;
        }
    } else if(write->gen == MagicGenMfcGen4) {
        /* UMC Gen4 uses CF + default 00000000 password, not Gen1a A0 writes. */
        const uint8_t* config = write->gen4_config;
        success = true;
        const size_t sak_offset = write->uid_len == 4 ? 5 : 7;
        const uint8_t sak = write->block0[sak_offset];
        const uint8_t max_block = sak == 0x18 ? 0xFF : 0x3F;
        const uint8_t target_length = write->uid_len == 7 ? 1 : 0;
        uint8_t set_mode[7] = {0xCF, 0, 0, 0, 0, 0x69, 0};
        uint8_t set_length[7] = {0xCF, 0, 0, 0, 0, 0x68, target_length};
        uint8_t set_identity[9] = {
            0xCF, 0, 0, 0, 0, 0x35,
            write->block0[sak_offset + 1], write->block0[sak_offset + 2], sak};
        uint8_t set_capacity[7] = {0xCF, 0, 0, 0, 0, 0x6B, max_block};
        uint8_t set_block0[23] = {0xCF, 0, 0, 0, 0, 0xCD, 0x00};
        memcpy(set_block0 + 7, write->block0, 16);
        if(config[0] != 0 && !(success = mfc_gen4_command(poller, tx, rx, set_mode, sizeof(set_mode))))
            write->gen4_failure = 2;
        if(success && config[1] != target_length &&
           !(success = mfc_gen4_command(poller, tx, rx, set_length, sizeof(set_length))))
            write->gen4_failure = 3;
        if(success && (config[24] != set_identity[6] || config[25] != set_identity[7] ||
                       config[26] != sak) &&
           !(success = mfc_gen4_command(poller, tx, rx, set_identity, sizeof(set_identity))))
            write->gen4_failure = 4;
        if(success && config[28] != max_block &&
           !(success = mfc_gen4_command(poller, tx, rx, set_capacity, sizeof(set_capacity))))
            write->gen4_failure = 5;
        if(success && !(success = mfc_gen4_command(poller, tx, rx, set_block0, sizeof(set_block0))))
            write->gen4_failure = 6;
    } else if(write->gen == MagicGenMfcGtu) {
        /* GDM/USCUID uses wakeup, public block 0, hidden block 0 and E1 config.
         * Keep the old personalization until both UID blocks are ready. */
        uint8_t first = 0x20;
        uint8_t config[16];
        success = mfc_gdm_wakeup(poller, tx, rx, first) &&
                  mfc_gdm_read_active(poller, tx, rx, 0xE0, 0, config);
        if(!success) {
            first = 0x40;
            success = mfc_gdm_wakeup(poller, tx, rx, first) &&
                      mfc_gdm_read_active(poller, tx, rx, 0xE0, 0, config);
        }
        if(!success) write->gdm_failure = 1;
        if(success) {
            const uint8_t configured_first = config[2] == 0x85 ? 0x20 : 0x40;
            if(first != configured_first)
                success = mfc_gdm_wakeup(poller, tx, rx, configured_first);
            first = configured_first;
            if(!success) write->gdm_failure = 1;
        }
        if(success && write->uid_len == 7) {
            uint8_t hidden[16] = {0};
            hidden[0] = 0x88;
            memcpy(hidden + 1, write->uid, 3);
            hidden[4] = 0x88 ^ write->uid[0] ^ write->uid[1] ^ write->uid[2];
            hidden[5] = 0x04;
            memcpy(hidden + 6, write->uid + 3, 4);
            hidden[10] = write->uid[3] ^ write->uid[4] ^ write->uid[5] ^ write->uid[6];
            hidden[11] = 0x08;
            success = mfc_gdm_write_active(poller, tx, rx, 0xA8, hidden, 0x38);
            if(!success) write->gdm_failure = 2;
        }
        if(success) {
            /* Restart the backdoor after a hidden-block write. */
            success = mfc_gdm_wakeup(poller, tx, rx, first) &&
                      mfc_gdm_write_active(poller, tx, rx, 0xA0, write->block0, 0x30);
            if(!success) write->gdm_failure = 3;
        }
        if(success) {
            uint8_t final_config[16];
            memcpy(final_config,
                   write->gdm_bridged ? write->gdm_original_config : config, 16);
            final_config[9] = write->uid_len == 4 ? 0x00 :
                (config[9] == 0x5A || config[9] == 0xC3 || config[9] == 0xA5 ?
                     config[9] : 0x5A);
            if(memcmp(final_config, config, 16) != 0) {
                success = mfc_gdm_wakeup(poller, tx, rx, first) &&
                          mfc_gdm_write_active(poller, tx, rx, 0xE1, final_config, 0xE0);
                if(!success) write->gdm_failure = 4;
            }
        }
    }
    bit_buffer_free(rx);
    bit_buffer_free(tx);
    write->success = success;
    furi_semaphore_release(write->complete);
    return NfcCommandStop;
}

static bool write_mfc_raw(
    Nfc* nfc, MagicGenType gen, const uint8_t* uid, size_t uid_len,
    const uint8_t* edited_block0) {
    if(uid_len != 4 && uid_len != 7) {
        snprintf(magic_write_error, sizeof(magic_write_error), "MFC 4/7B only");
        return false;
    }
    if(!edited_block0 && gen != MagicGenMfcGtu) {
        snprintf(magic_write_error, sizeof(magic_write_error), "Edit Block0 first");
        return false;
    }
    uint8_t gen4_config[30] = {0};
    bool detected = gen == MagicGenMfcGen4 ?
        mtools_mfc_gen4_read_config(nfc, gen4_config) : mtools_detect_magic_tag(nfc, gen);
    if(!detected) {
        snprintf(magic_write_error, sizeof(magic_write_error), "Wrong magic type");
        return false;
    }
    MfcRawWrite write = {
        .complete = furi_semaphore_alloc(1, 0), .gen = gen, .uid_len = uid_len,
        .wakeup_first = 0x40};
    if(gen == MagicGenMfcGen4) memcpy(write.gen4_config, gen4_config, sizeof(gen4_config));
    memcpy(write.uid, uid, uid_len);
    if(edited_block0) memcpy(write.block0, edited_block0, 16);
    if(uid_len == 4) mtools_mfc_prepare_block0(write.block0, uid);
    else memcpy(write.block0, uid, uid_len);
    if(gen == MagicGenMfcGtu) {
        /* GDM's public block 0 has a different seven-byte layout from UMC. */
        memset(write.block0, 0, sizeof(write.block0));
        memcpy(write.block0, uid, uid_len);
        if(uid_len == 7) {
            write.block0[7] = 0x88;
            write.block0[8] = 0x44;
        } else {
            write.block0[4] = mfc_uid_bcc(uid);
            write.block0[5] = 0x08;
            write.block0[6] = 0x04;
        }
    }
    if(gen == MagicGenMfcGen4) {
        const size_t sak_offset = uid_len == 4 ? 5 : 7;
        const uint8_t sak = write.block0[sak_offset];
        if(sak != 0x08 && sak != 0x18) {
            snprintf(magic_write_error, sizeof(magic_write_error), "Select MFC 1K/4K");
            furi_semaphore_free(write.complete);
            return false;
        }
        /* Keep the selected Classic profile consistent in both config and block 0. */
        write.block0[sak_offset + 1] = uid_len == 7 ? (sak == 0x18 ? 0x42 : 0x44) :
                                                       (sak == 0x18 ? 0x02 : 0x04);
        write.block0[sak_offset + 2] = 0x00;
    }
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    write.poller = poller;
    nfc_poller_start(poller, mfc_raw_write_callback, &write);
    bool completed = furi_semaphore_acquire(
        write.complete, furi_ms_to_ticks(gen == MagicGenMfcGen4 ? 8000 : 4000)) == FuriStatusOk;
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    if(gen == MagicGenMfcGtu && completed && !write.success && write.gdm_failure == 1) {
        uint8_t original_config[16];
        if(mfc_gdm_open_wakeup(nfc, original_config)) {
            memcpy(write.gdm_original_config, original_config, 16);
            write.gdm_bridged = true;
            write.gdm_failure = 0;
            furi_delay_ms(80);
            poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
            write.poller = poller;
            nfc_poller_start(poller, mfc_raw_write_callback, &write);
            completed = furi_semaphore_acquire(
                write.complete, furi_ms_to_ticks(4000)) == FuriStatusOk;
            nfc_poller_stop(poller);
            nfc_poller_free(poller);
        } else {
            write.gdm_failure = 5;
        }
    }
    furi_semaphore_free(write.complete);
    if(!completed || !write.success) {
        snprintf(magic_write_error, sizeof(magic_write_error), "%s",
                 write.length_mismatch ? "UID length differs" :
                 gen == MagicGenMfcGen3 && write.gen3_failure == 1 ? "Gen3 UID rejected" :
                 gen == MagicGenMfcGen3 && write.gen3_failure == 2 ? "Gen3 B0 rejected" :
                 gen == MagicGenMfcGen4 && write.gen4_failure == 2 ? "Gen4 MFC mode" :
                 gen == MagicGenMfcGen4 && write.gen4_failure == 3 ? "Gen4 UID length" :
                 gen == MagicGenMfcGen4 && write.gen4_failure == 4 ? "Gen4 ATQA/SAK" :
                 gen == MagicGenMfcGen4 && write.gen4_failure == 5 ? "Gen4 capacity" :
                 gen == MagicGenMfcGen4 && write.gen4_failure == 6 ? "Gen4 block0" :
                 gen == MagicGenMfcGtu && write.gdm_failure == 1 ? "GDM wakeup unavailable" :
                 gen == MagicGenMfcGtu && write.gdm_failure == 2 ? "GDM hidden B0" :
                 gen == MagicGenMfcGtu && write.gdm_failure == 3 ? "GDM public B0" :
                 gen == MagicGenMfcGtu && write.gdm_failure == 4 ? "GDM config" :
                 gen == MagicGenMfcGtu && write.gdm_failure == 5 ? "GDM auth/bridge fail" :
                 "Write failed");
        return false;
    }
    furi_delay_ms(80);
    Iso14443_3aData selected = {0};
    Iso14443_3aError select_error = Iso14443_3aErrorNotPresent;
    for(uint8_t attempt = 0; attempt < 3; attempt++) {
        select_error = iso14443_3a_poller_sync_read(nfc, &selected);
        if(select_error == Iso14443_3aErrorNone) break;
        furi_delay_ms(80);
    }
    if(select_error != Iso14443_3aErrorNone) {
        snprintf(magic_write_error, sizeof(magic_write_error), "Verify failed");
        return false;
    }
    size_t selected_len = 0;
    const uint8_t* selected_uid = iso14443_3a_get_uid(&selected, &selected_len);
    const size_t sak_offset = uid_len == 4 ? 5 : 7;
    bool verified = selected_len == uid_len && memcmp(selected_uid, uid, uid_len) == 0;
    if(gen != MagicGenMfcGtu)
        verified = verified && selected.sak == write.block0[sak_offset] &&
                   memcmp(selected.atqa, write.block0 + sak_offset + 1, 2) == 0;
    if(!verified) snprintf(magic_write_error, sizeof(magic_write_error), "Verify mismatch");
    if(verified && gen == MagicGenMfcGen4) {
        uint8_t config[30];
        uint8_t observed[16];
        const uint8_t expected_length = uid_len == 7 ? 1 : 0;
        const uint8_t expected_max = write.block0[sak_offset] == 0x18 ? 0xFF : 0x3F;
        verified = mtools_mfc_gen4_read_config(nfc, config) &&
                   config[0] == 0 && config[1] == expected_length &&
                   config[24] == write.block0[sak_offset + 1] &&
                   config[25] == write.block0[sak_offset + 2] &&
                   config[26] == write.block0[sak_offset] && config[28] == expected_max &&
                   mfc_gen4_read_block0(nfc, observed) &&
                   memcmp(observed, write.block0, 16) == 0;
        if(!verified) snprintf(magic_write_error, sizeof(magic_write_error), "Gen4 verify failed");
    }
    return verified;
}

static bool write_mfc_gen1a(
    Nfc* nfc, const uint8_t* uid, size_t uid_len, const uint8_t* edited_block0) {
    return write_mfc_raw(nfc, MagicGenMfcGen1a, uid, uid_len, edited_block0);
}

static MfClassicKey mfc_default_key(void) {
    MfClassicKey key;
    memset(key.data, 0xFF, sizeof(key.data));
    return key;
}

static bool mfc_auth_error(MfClassicError error) {
    return error == MfClassicErrorAuth || error == MfClassicErrorTimeout;
}

static void mfc_set_write_error(MfClassicError error_a, MfClassicError error_b) {
    FURI_LOG_W("MTools", "Gen2 write failed: A=%d B=%d", error_a, error_b);
    snprintf(magic_write_error, sizeof(magic_write_error), "%s",
             mfc_auth_error(error_a) && mfc_auth_error(error_b) ? "认证出错" : "Write failed");
}

bool mtools_mfc_read_block0(Nfc* nfc, uint8_t block0[16]) {
    MfClassicKey key = mfc_default_key();
    MfClassicBlock block = {0};
    MfClassicError error = MfClassicErrorNotPresent;
    MfClassicError error_a = MfClassicErrorNotPresent;
    MfClassicError error_b = MfClassicErrorNotPresent;
    for(uint8_t attempt = 0; attempt < 3 && error != MfClassicErrorNone; attempt++) {
        error_a = mf_classic_poller_sync_read_block(nfc, 0, &key, MfClassicKeyTypeA, &block);
        error = error_a;
        if(error != MfClassicErrorNone) {
            error_b = mf_classic_poller_sync_read_block(nfc, 0, &key, MfClassicKeyTypeB, &block);
            error = error_b;
        }
        if(error != MfClassicErrorNone && attempt < 2) furi_delay_ms(40);
    }
    if(error != MfClassicErrorNone) {
        FURI_LOG_W("MTools", "Read block 0 failed: A=%d B=%d", error_a, error_b);
        snprintf(magic_write_error, sizeof(magic_write_error), "%s",
                 mfc_auth_error(error_a) && mfc_auth_error(error_b) ? "认证出错" : "Read failed");
        return false;
    }
    memcpy(block0, block.data, 16);
    magic_write_error[0] = 0;
    return true;
}

static bool write_mfc_gen2(
    Nfc* nfc,
    const uint8_t* uid,
    size_t uid_len,
    const uint8_t* edited_block0) {
    if(uid_len != 4 && uid_len != 7) {
        snprintf(magic_write_error, sizeof(magic_write_error), "Gen2 UID length");
        return false;
    }
    MfClassicBlock block = {0};
    if(edited_block0) {
        memcpy(block.data, edited_block0, 16);
    } else {
        if(!mtools_mfc_read_block0(nfc, block.data)) return false;
    }
    if(uid_len == 4)
        mtools_mfc_prepare_block0(block.data, uid);
    else
        memcpy(block.data, uid, 7);
    MfClassicKey key = mfc_default_key();
    MfClassicError error_a =
        mf_classic_poller_sync_write_block(nfc, 0, &key, MfClassicKeyTypeA, &block);
    MfClassicError error_b = MfClassicErrorNone;
    if(error_a != MfClassicErrorNone) {
        FURI_LOG_W("MTools", "Gen2 write A failed: %d", error_a);
        error_b = mf_classic_poller_sync_write_block(nfc, 0, &key, MfClassicKeyTypeB, &block);
    }
    /* A UID change can invalidate the encrypted session. Re-select the card first. */
    Iso14443_3aData selected = {0};
    Iso14443_3aError select_error = Iso14443_3aErrorNotPresent;
    for(uint8_t attempt = 0; attempt < 3; attempt++) {
        select_error = iso14443_3a_poller_sync_read(nfc, &selected);
        if(select_error == Iso14443_3aErrorNone) break;
        furi_delay_ms(40);
    }
    if(select_error != Iso14443_3aErrorNone) {
        mfc_set_write_error(error_a, error_b);
        return false;
    }
    size_t selected_len = 0;
    const uint8_t* selected_uid = iso14443_3a_get_uid(&selected, &selected_len);
    if(selected_len != uid_len || memcmp(selected_uid, uid, uid_len) != 0) {
        mfc_set_write_error(error_a, error_b);
        return false;
    }
    const size_t sak_offset = uid_len == 4 ? 5 : 7;
    if(selected.sak != block.data[sak_offset] ||
       memcmp(selected.atqa, block.data + sak_offset + 1, 2) != 0) {
        snprintf(magic_write_error, sizeof(magic_write_error), "SAK/ATQA mismatch");
        return false;
    }
    /* Compare all 16 bytes when authentication still works after changing UID. */
    uint8_t observed[16];
    if(!mtools_mfc_read_block0(nfc, observed)) {
        FURI_LOG_W("MTools", "UID verified; full block 0 readback unavailable");
        magic_write_error[0] = 0;
        return true;
    }
    bool verified = memcmp(observed, block.data, 16) == 0;
    if(!verified)
        snprintf(magic_write_error, sizeof(magic_write_error), "Verify mismatch");
    FURI_LOG_I("MTools", "Gen2 block 0 verify: %s", verified ? "OK" : "mismatch");
    return verified;
}

static bool write_mfc_gen3(
    Nfc* nfc, const uint8_t* uid, size_t uid_len, const uint8_t* edited_block0) {
    return write_mfc_raw(nfc, MagicGenMfcGen3, uid, uid_len, edited_block0);
}

static bool write_mfc_gen4(
    Nfc* nfc, const uint8_t* uid, size_t uid_len, const uint8_t* edited_block0) {
    return write_mfc_raw(nfc, MagicGenMfcGen4, uid, uid_len, edited_block0);
}

static bool write_mfc_gtu(
    Nfc* nfc, const uint8_t* uid, size_t uid_len, const uint8_t* edited_block0) {
    return write_mfc_raw(nfc, MagicGenMfcGtu, uid, uid_len, edited_block0);
}

typedef struct {
    FuriSemaphore* complete;
    uint8_t uid[8];
    uint8_t generation;
    bool success;
} IsoUidWrite;

static bool iso_send_uid_frame(Iso15693_3Poller* poller, const uint8_t* frame, size_t length) {
    BitBuffer* tx = bit_buffer_alloc(16);
    BitBuffer* rx = bit_buffer_alloc(16);
    bit_buffer_copy_bytes(tx, frame, length);
    bool result = iso15693_3_poller_send_frame(poller, tx, rx, 1356000U) == Iso15693_3ErrorNone &&
                  bit_buffer_get_size_bytes(rx) >= 1 && !(bit_buffer_get_byte(rx, 0) & 1U);
    bit_buffer_free(rx);
    bit_buffer_free(tx);
    return result;
}

static NfcCommand iso_uid_write_callback(NfcGenericEvent event, void* context) {
    IsoUidWrite* write = context;
    if(event.protocol != NfcProtocolIso15693_3) return NfcCommandContinue;
    Iso15693_3PollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso15693_3PollerEventTypeReady) return NfcCommandContinue;
    Iso15693_3Poller* poller = event.instance;
    bool valid = true;
    uint8_t block_count = 0;
    if(write->generation == 1) {
        Iso15693_3SystemInfo info = {0};
        valid = iso15693_3_poller_get_system_info(poller, &info) == Iso15693_3ErrorNone &&
                info.block_size == 4 && info.block_count == 28 && info.ic_ref == 0x01;
        block_count = info.block_count;
    }
    for(uint8_t part = 0; part < 2 && valid; part++) {
        uint8_t frame[8];
        size_t length;
        if(write->generation == 1) {
            frame[0] = 0x01;
            frame[1] = 0x21;
            frame[2] = block_count + part;
            length = 7;
        } else {
            frame[0] = 0x02;
            frame[1] = 0xE0;
            frame[2] = 0x09;
            frame[3] = 0x40 + part;
            length = 8;
        }
        size_t offset = write->generation == 1 ? 3 : 4;
        for(size_t i = 0; i < 4; i++) frame[offset + i] = write->uid[(part ? 0 : 4) + (3 - i)];
        valid = iso_send_uid_frame(poller, frame, length);
    }
    write->success = valid;
    furi_semaphore_release(write->complete);
    return NfcCommandStop;
}

typedef struct {
    FuriSemaphore* complete;
    NfcPoller* poller;
    uint8_t uid[8];
    bool matches;
} IsoUidVerify;

static NfcCommand iso_uid_verify_callback(NfcGenericEvent event, void* context) {
    IsoUidVerify* verify = context;
    if(event.protocol != NfcProtocolIso15693_3) return NfcCommandContinue;
    Iso15693_3PollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso15693_3PollerEventTypeReady) return NfcCommandContinue;
    const Iso15693_3Data* data = nfc_poller_get_data(verify->poller);
    size_t uid_len = 0;
    const uint8_t* observed = iso15693_3_get_uid(data, &uid_len);
    verify->matches = uid_len == 8 && memcmp(observed, verify->uid, 8) == 0;
    furi_semaphore_release(verify->complete);
    return NfcCommandStop;
}

static bool write_iso15693_uid(Nfc* nfc, const uint8_t uid[8], uint8_t generation) {
    if(uid[0] != 0xE0) return false;
    IsoUidWrite write = {.complete = furi_semaphore_alloc(1, 0), .generation = generation};
    memcpy(write.uid, uid, sizeof(write.uid));
    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso15693_3);
    nfc_poller_start(poller, iso_uid_write_callback, &write);
    bool completed = furi_semaphore_acquire(write.complete, furi_ms_to_ticks(4000)) == FuriStatusOk;
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_semaphore_free(write.complete);
    if(!completed || !write.success) return false;

    /* Gen1 applies hidden UID blocks after the RF field is restarted. */
    furi_delay_ms(80);
    IsoUidVerify verify = {.complete = furi_semaphore_alloc(1, 0)};
    memcpy(verify.uid, uid, sizeof(verify.uid));
    poller = nfc_poller_alloc(nfc, NfcProtocolIso15693_3);
    verify.poller = poller;
    nfc_poller_start(poller, iso_uid_verify_callback, &verify);
    completed = furi_semaphore_acquire(verify.complete, furi_ms_to_ticks(3000)) == FuriStatusOk;
    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_semaphore_free(verify.complete);
    return completed && verify.matches;
}

static bool write_iso15693_gen1(Nfc* nfc, const uint8_t uid[8]) {
    return write_iso15693_uid(nfc, uid, 1);
}

static bool write_iso15693_gen2(Nfc* nfc, const uint8_t uid[8]) {
    return write_iso15693_uid(nfc, uid, 2);
}

typedef struct {
    NfcPoller* poller;
    FuriSemaphore* complete;
    uint8_t uid[8];
    bool success;
} IsoGen3Write;

static NfcCommand iso_gen3_write_callback(NfcGenericEvent event, void* context) {
    IsoGen3Write* write = context;
    if(event.protocol != NfcProtocolIso15693_3) return NfcCommandContinue;
    Iso15693_3PollerEvent* poller_event = event.event_data;
    if(poller_event->type != Iso15693_3PollerEventTypeReady) return NfcCommandContinue;

    Iso15693_3Poller* poller = event.instance;
    Iso15693_3SystemInfo info = {0};
    uint8_t activation[8];
    static const uint8_t expected_activation[8] = {0xA5, 0x2B, 0x44, 0x2C, 0x21, 0xAE, 0x93, 0x00};
    bool valid = iso15693_3_poller_get_system_info(poller, &info) == Iso15693_3ErrorNone &&
                 info.block_count == 80 && info.block_size == 4 &&
                 iso15693_3_poller_read_block(poller, activation, 0x14, 4) == Iso15693_3ErrorNone &&
                 iso15693_3_poller_read_block(poller, activation + 4, 0x15, 4) == Iso15693_3ErrorNone &&
                 memcmp(activation, expected_activation, sizeof(activation)) == 0;
    if(valid) {
        BitBuffer* tx = bit_buffer_alloc(8);
        BitBuffer* rx = bit_buffer_alloc(32);
        for(uint8_t part = 0; part < 2 && valid; part++) {
            uint8_t frame[7] = {0x02, 0x21, (uint8_t)(0x10 + part)};
            for(size_t i = 0; i < 4; i++) frame[3 + i] = write->uid[(part ? 0 : 4) + (3 - i)];
            bit_buffer_copy_bytes(tx, frame, sizeof(frame));
            bit_buffer_reset(rx);
            valid = iso15693_3_poller_send_frame(poller, tx, rx, 1356000U) == Iso15693_3ErrorNone &&
                    bit_buffer_get_size_bytes(rx) >= 1 && !(bit_buffer_get_byte(rx, 0) & 1U);
        }
        bit_buffer_free(rx);
        bit_buffer_free(tx);
    }
    if(valid) {
        uint8_t observed[8] = {0};
        valid = iso15693_3_poller_inventory(poller, observed) == Iso15693_3ErrorNone &&
                memcmp(observed, write->uid, sizeof(observed)) == 0;
    }
    write->success = valid;
    furi_semaphore_release(write->complete);
    return NfcCommandStop;
}

static bool write_iso15693_gen3(Nfc* nfc, const uint8_t uid[8]) {
    if(uid[0] != 0xE0) return false;
    IsoGen3Write write = {.complete = furi_semaphore_alloc(1, 0)};
    memcpy(write.uid, uid, sizeof(write.uid));
    write.poller = nfc_poller_alloc(nfc, NfcProtocolIso15693_3);
    nfc_poller_start(write.poller, iso_gen3_write_callback, &write);
    bool completed = furi_semaphore_acquire(write.complete, furi_ms_to_ticks(4000)) == FuriStatusOk;
    nfc_poller_stop(write.poller);
    nfc_poller_free(write.poller);
    furi_semaphore_free(write.complete);
    return completed && write.success;
}

bool mtools_magic_uid_write_implemented(MagicGenType gen) {
    return gen == MagicGenMfcGen1a || gen == MagicGenMfcGen2 ||
           gen == MagicGenMfcGen3 || gen == MagicGenMfcGen4 || gen == MagicGenMfcGtu ||
           gen == MagicGenIso15693Gen1 ||
           gen == MagicGenIso15693Gen2 || gen == MagicGenIso15693Gen3;
}

bool mtools_magic_uid_length_supported(MagicGenType gen, size_t uid_len) {
    switch(gen) {
    case MagicGenMfcGen1a:
    case MagicGenMfcGen3: return uid_len == 4 || uid_len == 7;
    case MagicGenMfcGen2: return uid_len == 4 || uid_len == 7;
    case MagicGenMfcGen4:
    case MagicGenMfcGtu: return uid_len == 4 || uid_len == 7;
    case MagicGenIso15693Gen1:
    case MagicGenIso15693Gen2:
    case MagicGenIso15693Gen3: return uid_len == 8;
    default: return false;
    }
}

bool mtools_write_magic_uid(Nfc* nfc, MagicGenType gen, const uint8_t* uid, size_t uid_len) {
    return mtools_write_magic_uid_with_block0(nfc, gen, uid, uid_len, NULL);
}

bool mtools_write_magic_uid_with_block0(
    Nfc* nfc,
    MagicGenType gen,
    const uint8_t* uid,
    size_t uid_len,
    const uint8_t* edited_block0) {
    magic_write_error[0] = 0;
    if(!nfc || !uid || !mtools_magic_uid_length_supported(gen, uid_len)) return false;
    switch(gen) {
    case MagicGenMfcGen1a: return write_mfc_gen1a(nfc, uid, uid_len, edited_block0);
    case MagicGenMfcGen2: return write_mfc_gen2(nfc, uid, uid_len, edited_block0);
    case MagicGenMfcGen3: return write_mfc_gen3(nfc, uid, uid_len, edited_block0);
    case MagicGenMfcGen4: return write_mfc_gen4(nfc, uid, uid_len, edited_block0);
    case MagicGenMfcGtu: return write_mfc_gtu(nfc, uid, uid_len, edited_block0);
    case MagicGenIso15693Gen1: return uid_len == 8 && write_iso15693_gen1(nfc, uid);
    case MagicGenIso15693Gen2: return uid_len == 8 && write_iso15693_gen2(nfc, uid);
    case MagicGenIso15693Gen3: return uid_len == 8 && write_iso15693_gen3(nfc, uid);
    case MagicGenCount: break;
    }
    return false;
}

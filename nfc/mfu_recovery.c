#include "mfu_recovery.h"

#include <furi.h>
#include <toolbox/bit_buffer.h>
#include <nfc/helpers/iso14443_crc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#include <string.h>
#include <stdio.h>

#define TAG "MfuRecovery"

typedef struct {
    FuriSemaphore* complete;
    Nfc* nfc;
    bool do_repair;
    MfuRecoveryResult* result;
    const bool* running;
    bool success;
} MfuRecoverySession;

static NfcCommand mfu_recovery_callback(NfcEvent event, void* context) {
    MfuRecoverySession* session = context;
    if(session->running && !*session->running) {
        furi_semaphore_release(session->complete);
        return NfcCommandStop;
    }
    if(event.type != NfcEventTypePollerReady) return NfcCommandContinue;

    BitBuffer* tx = bit_buffer_alloc(64);
    BitBuffer* rx = bit_buffer_alloc(64);
    bool stop = false;

    do {
        // Step 1: WUPA short frame (0x52)
        NfcError err = nfc_iso14443a_poller_trx_short_frame(
            session->nfc, NfcIso14443aShortFrameAllReqa, rx, 1356000U);
        if(err != NfcErrorNone || bit_buffer_get_size_bytes(rx) < 2) {
            break; // No tag in field, continue polling
        }
        uint8_t atqa[2];
        atqa[0] = bit_buffer_get_byte(rx, 0);
        atqa[1] = bit_buffer_get_byte(rx, 1);

        // Step 2: CL1 SDD (93 20)
        const uint8_t sdd1_cmd[2] = {0x93, 0x20};
        bit_buffer_copy_bytes(tx, sdd1_cmd, sizeof(sdd1_cmd));
        bit_buffer_reset(rx);
        err = nfc_iso14443a_poller_trx_sdd_frame(session->nfc, tx, rx, 1356000U);
        if(err != NfcErrorNone || bit_buffer_get_size_bytes(rx) != 5) {
            break;
        }
        uint8_t cl1[5];
        bit_buffer_write_bytes_mid(rx, cl1, 0, 5);
        if(cl1[0] != 0x88) {
            snprintf(session->result->error_message, sizeof(session->result->error_message), "Not 7B Type 2 tag");
            stop = true;
            break;
        }

        // Step 3: CL1 SELECT (93 70 + cl1 5B + CRC-A)
        // CRITICAL: We forward cl1 as-is to bypass reader-side BCC check!
        uint8_t sel1[7];
        sel1[0] = 0x93;
        sel1[1] = 0x70;
        memcpy(sel1 + 2, cl1, 5);
        bit_buffer_copy_bytes(tx, sel1, sizeof(sel1));
        iso14443_crc_append(Iso14443CrcTypeA, tx);
        bit_buffer_reset(rx);
        err = nfc_poller_trx(session->nfc, tx, rx, 1356000U);
        if(err != NfcErrorNone || bit_buffer_get_size_bytes(rx) < 3) {
            snprintf(session->result->error_message, sizeof(session->result->error_message), "CL1 Select failed");
            stop = true;
            break;
        }
        uint8_t sak1 = bit_buffer_get_byte(rx, 0);
        if(!(sak1 & 0x04)) {
            snprintf(session->result->error_message, sizeof(session->result->error_message), "Cascade 2 missing");
            stop = true;
            break;
        }

        // Step 4: CL2 SDD (95 20)
        const uint8_t sdd2_cmd[2] = {0x95, 0x20};
        bit_buffer_copy_bytes(tx, sdd2_cmd, sizeof(sdd2_cmd));
        bit_buffer_reset(rx);
        err = nfc_iso14443a_poller_trx_sdd_frame(session->nfc, tx, rx, 1356000U);
        if(err != NfcErrorNone || bit_buffer_get_size_bytes(rx) != 5) {
            snprintf(session->result->error_message, sizeof(session->result->error_message), "CL2 SDD failed");
            stop = true;
            break;
        }
        uint8_t cl2[5];
        bit_buffer_write_bytes_mid(rx, cl2, 0, 5);

        // Step 5: CL2 SELECT (95 70 + cl2 5B + CRC-A)
        // Forward cl2 as-is to complete cascade selection
        uint8_t sel2[7];
        sel2[0] = 0x95;
        sel2[1] = 0x70;
        memcpy(sel2 + 2, cl2, 5);
        bit_buffer_copy_bytes(tx, sel2, sizeof(sel2));
        iso14443_crc_append(Iso14443CrcTypeA, tx);
        bit_buffer_reset(rx);
        err = nfc_poller_trx(session->nfc, tx, rx, 1356000U);
        if(err != NfcErrorNone || bit_buffer_get_size_bytes(rx) < 3) {
            snprintf(session->result->error_message, sizeof(session->result->error_message), "CL2 Select failed");
            stop = true;
            break;
        }
        uint8_t sak2 = bit_buffer_get_byte(rx, 0);
        if(sak2 != 0x00) {
            snprintf(session->result->error_message, sizeof(session->result->error_message), "SAK %02X != 00", sak2);
            stop = true;
            break;
        }

        // Step 6: Assemble UID and calculate expected BCCs
        uint8_t uid[7];
        uid[0] = cl1[1];
        uid[1] = cl1[2];
        uid[2] = cl1[3];
        uid[3] = cl2[0];
        uid[4] = cl2[1];
        uid[5] = cl2[2];
        uid[6] = cl2[3];

        uint8_t bcc0_expected = 0x88 ^ uid[0] ^ uid[1] ^ uid[2];
        uint8_t bcc1_expected = uid[3] ^ uid[4] ^ uid[5] ^ uid[6];

        // Step 7: READ pages 0-3 (30 00)
        const uint8_t read_cmd[2] = {0x30, 0x00};
        bit_buffer_copy_bytes(tx, read_cmd, sizeof(read_cmd));
        iso14443_crc_append(Iso14443CrcTypeA, tx);
        bit_buffer_reset(rx);
        err = nfc_poller_trx(session->nfc, tx, rx, 1356000U);
        if(err != NfcErrorNone || bit_buffer_get_size_bytes(rx) < 18) {
            snprintf(session->result->error_message, sizeof(session->result->error_message), "READ page 0 failed");
            stop = true;
            break;
        }
        if(!iso14443_crc_check(Iso14443CrcTypeA, rx)) {
            snprintf(session->result->error_message, sizeof(session->result->error_message), "Page 0 CRC bad");
            stop = true;
            break;
        }

        uint8_t pages[16];
        bit_buffer_write_bytes_mid(rx, pages, 0, 16);

        // Fill diagnostic results
        memcpy(session->result->uid, uid, 7);
        memcpy(session->result->atqa, atqa, 2);
        session->result->sak1 = sak1;
        session->result->sak2 = sak2;
        memcpy(session->result->pages, pages, 16);
        session->result->bcc0_actual = pages[3];
        session->result->bcc0_expected = bcc0_expected;
        session->result->bcc1_actual = pages[8];
        session->result->bcc1_expected = bcc1_expected;
        session->result->bcc0_valid = (pages[3] == bcc0_expected);
        session->result->bcc1_valid = (pages[8] == bcc1_expected);
        session->result->needs_recovery = (!session->result->bcc0_valid || !session->result->bcc1_valid);

        if(!session->do_repair) {
            session->success = true;
            stop = true;
            break;
        }

        // Step 8: DirectWrite repair if requested
        if(!session->result->needs_recovery) {
            session->success = true;
            stop = true;
            break;
        }

        uint8_t p0[4] = {uid[0], uid[1], uid[2], bcc0_expected};
        uint8_t p1[4] = {uid[3], uid[4], uid[5], uid[6]};
        uint8_t p2[4] = {bcc1_expected, pages[9], pages[10], pages[11]};
        const uint8_t* write_pages[3] = {p0, p1, p2};
        bool write_ok = true;

        for(uint8_t p = 0; p < 3 && write_ok; p++) {
            uint8_t w_cmd[6] = {0xA2, p, write_pages[p][0], write_pages[p][1], write_pages[p][2], write_pages[p][3]};
            bit_buffer_copy_bytes(tx, w_cmd, sizeof(w_cmd));
            iso14443_crc_append(Iso14443CrcTypeA, tx);
            bit_buffer_reset(rx);
            err = nfc_poller_trx(session->nfc, tx, rx, 1356000U);
            write_ok = (err == NfcErrorNone && bit_buffer_get_size(rx) == 4 &&
                        (bit_buffer_get_byte(rx, 0) & 0x0F) == 0x0A);
            if(!write_ok) {
                snprintf(session->result->error_message, sizeof(session->result->error_message), "Page %u write NACK", p);
            }
        }

        if(write_ok) {
            // Post-write readback verification
            bit_buffer_copy_bytes(tx, read_cmd, sizeof(read_cmd));
            iso14443_crc_append(Iso14443CrcTypeA, tx);
            bit_buffer_reset(rx);
            if(nfc_poller_trx(session->nfc, tx, rx, 1356000U) == NfcErrorNone &&
               bit_buffer_get_size_bytes(rx) >= 18 &&
               iso14443_crc_check(Iso14443CrcTypeA, rx)) {
                uint8_t verify_pages[16];
                bit_buffer_write_bytes_mid(rx, verify_pages, 0, 16);
                if(memcmp(verify_pages, p0, 4) == 0 &&
                   memcmp(verify_pages + 4, p1, 4) == 0 &&
                   verify_pages[8] == bcc1_expected) {
                    session->result->written = true;
                    session->success = true;
                } else {
                    snprintf(session->result->error_message, sizeof(session->result->error_message), "Verify mismatch");
                }
            } else {
                snprintf(session->result->error_message, sizeof(session->result->error_message), "Post-read failed");
            }
        }
        stop = true;
    } while(false);

    bit_buffer_free(rx);
    bit_buffer_free(tx);

    if(stop) {
        furi_semaphore_release(session->complete);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static bool mfu_recovery_run_session(Nfc* nfc, MfuRecoveryResult* result, bool do_repair, const bool* running) {
    if(!nfc || !result) return false;
    if(running && !*running) return false;
    memset(result, 0, sizeof(*result));

    MfuRecoverySession session = {
        .complete = furi_semaphore_alloc(1, 0),
        .nfc = nfc,
        .do_repair = do_repair,
        .result = result,
        .running = running,
        .success = false,
    };

    nfc_config(nfc, NfcModePoller, NfcTechIso14443a);
    nfc_set_guard_time_us(nfc, 5000);
    nfc_set_fdt_poll_fc(nfc, 1620);
    nfc_set_fdt_poll_poll_us(nfc, 1100);

    nfc_start(nfc, mfu_recovery_callback, &session);
    uint32_t timeout_ms = do_repair ? 1500 : 250;
    bool completed = (furi_semaphore_acquire(session.complete, furi_ms_to_ticks(timeout_ms)) == FuriStatusOk);
    nfc_stop(nfc);
    furi_semaphore_free(session.complete);

    return completed && session.success;
}

bool mtools_mfu_recover_diagnose(Nfc* nfc, MfuRecoveryResult* result, const bool* running) {
    return mfu_recovery_run_session(nfc, result, false, running);
}

bool mtools_mfu_recover_repair(Nfc* nfc, MfuRecoveryResult* result, const bool* running) {
    bool ok = mfu_recovery_run_session(nfc, result, true, running);
    if(ok && result->written && (!running || *running)) {
        furi_delay_ms(50);
        Iso14443_3aData selected = {0};
        if(iso14443_3a_poller_sync_read(nfc, &selected) == Iso14443_3aErrorNone) {
            size_t sel_len = 0;
            const uint8_t* sel_uid = iso14443_3a_get_uid(&selected, &sel_len);
            if(sel_len == 7 && memcmp(sel_uid, result->uid, 7) == 0) {
                result->verified = true;
            }
        }
    }
    return ok;
}

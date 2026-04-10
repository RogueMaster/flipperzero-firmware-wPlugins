#include "mfp_listener.h"
#include "mfp_app.h"
#include "mfp_keys.h"

#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/helpers/iso14443_crc.h>
#include <bit_lib/bit_lib.h>
#include <furi.h>
#include <furi_hal_random.h>
#include <toolbox/simple_array.h>
#include <string.h>

#define TAG "MfpListener"

#define MFP_CMD_GET_VERSION      0x60
#define MFP_CMD_AUTH1_PART1      0x70
#define MFP_CMD_AUTH1_PART2      0x72
#define MFP_CMD_AUTH_NONFIRST    0x76
#define MFP_CMD_READ_ENC         0x31
#define MFP_CMD_READ_ENC_NOMAC   0x33
#define MFP_CMD_READ_PLAIN       0x37
#define MFP_CMD_WRITE_ENC        0xA1

#define MFP_STATUS_OK            0x90
#define MFP_STATUS_AUTH_ERR      0x06

/* Helper: send a raw response as an I-block.
 * Frame = [PCB | CID(if present) | INF]. Hardware adds CRC_A. */
static void mfp_listener_send_inf(MfpListener* inst, const uint8_t* inf, size_t inf_len) {
    uint8_t frame[64];
    size_t n = 0;

    frame[n++] = inst->current_pcb;
    /* If CID bit is set in PCB (0x08), include CID byte (0 for our card) */
    if(inst->current_pcb & 0x08) {
        frame[n++] = 0x00;
    }
    if(inf_len > 0 && inf != NULL) {
        if(n + inf_len > sizeof(frame)) return;
        memcpy(&frame[n], inf, inf_len);
        n += inf_len;
    }

    BitBuffer* tx = bit_buffer_alloc(n * 8);
    bit_buffer_copy_bytes(tx, frame, n);
    nfc_listener_tx(inst->nfc, tx);
    bit_buffer_free(tx);

    /* Toggle PCB block number for next exchange */
    inst->current_pcb ^= 0x01;
}

/* ---- MFP command handlers ---- */

static void handle_get_version(MfpListener* inst, const uint8_t* data, size_t len) {
    UNUSED(data);
    UNUSED(len);
    /* Pre-canned NXP MFP SL3 GetVersion response stage 1: 0xAF + hw_vendor/type/etc.
     * hw_storage byte per NXP MF1PLUSx0y1 datasheet:
     *   2K (MF1PLUS80): 0x10
     *   4K (MF1PLUS60): 0x18
     */
    uint8_t resp[8];
    resp[0] = 0xAF; /* Additional frame */
    resp[1] = 0x04; /* hw_vendor = NXP */
    resp[2] = 0x01; /* hw_type = MFP */
    resp[3] = 0x01; /* hw_subtype */
    resp[4] = 0x01; /* hw_major */
    resp[5] = 0x00; /* hw_minor */
    resp[6] = (inst->size == MfpSize4K) ? 0x18 : 0x10; /* storage */
    resp[7] = 0x05; /* hw_proto */
    mfp_listener_send_inf(inst, resp, sizeof(resp));
}

static void handle_get_version_continue(MfpListener* inst, int stage) {
    if(stage == 2) {
        /* Software version */
        uint8_t resp[8];
        resp[0] = 0xAF;
        resp[1] = 0x04;
        resp[2] = 0x01;
        resp[3] = 0x01;
        resp[4] = 0x01;
        resp[5] = 0x00;
        resp[6] = (inst->size == MfpSize4K) ? 0x18 : 0x10;
        resp[7] = 0x05;
        mfp_listener_send_inf(inst, resp, sizeof(resp));
    } else if(stage == 3) {
        /* UID + batch: status 0x00 + UID */
        uint8_t resp[1 + 7];
        resp[0] = 0x00;
        memcpy(&resp[1], inst->uid, inst->uid_len);
        mfp_listener_send_inf(inst, resp, 1 + inst->uid_len);
    }
}

static void handle_auth_part1(MfpListener* inst, const uint8_t* data, size_t len, bool is_nonfirst) {
    /* AuthNonFirst requires an existing active session */
    if(is_nonfirst && !inst->authed) {
        uint8_t nak = MFP_STATUS_AUTH_ERR;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    if(len < 3) {
        uint8_t nak = MFP_STATUS_AUTH_ERR;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    /* If AuthNonFirst, preserve TI from the existing session */
    if(is_nonfirst) {
        memcpy(inst->saved_ti, inst->session.ti, 4);
        inst->has_saved_ti = true;
    } else {
        inst->has_saved_ti = false;
    }

    uint16_t key_num = data[1] | ((uint16_t)data[2] << 8);
    inst->auth_key_type = (key_num & 0x01) ? MfpKeyB : MfpKeyA;
    inst->auth_sector = (uint8_t)((key_num - 0x4000) / 2);

    if(inst->auth_sector >= inst->total_sectors) {
        uint8_t nak = MFP_STATUS_AUTH_ERR;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    const uint8_t* key = (inst->auth_key_type == MfpKeyA) ?
                             inst->sector_keys[inst->auth_sector].key_a :
                             inst->sector_keys[inst->auth_sector].key_b;
    bool key_valid = (inst->auth_key_type == MfpKeyA) ?
                         inst->sector_keys[inst->auth_sector].key_a_valid :
                         inst->sector_keys[inst->auth_sector].key_b_valid;
    if(!key_valid) {
        uint8_t nak = MFP_STATUS_AUTH_ERR;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    /* Generate RndB */
    furi_hal_random_fill_buf(inst->rnd_b, MFP_AES_BLOCK_SIZE);

    /* Encrypt with sector key (ECB) */
    uint8_t ek_rnd_b[MFP_AES_BLOCK_SIZE];
    mfp_crypto_ecb_encrypt(key, inst->rnd_b, ek_rnd_b);

    /* Response: 0x90 + ek(RndB) + PICCCap2(2 bytes, empty) = 19 bytes */
    uint8_t resp[1 + MFP_AES_BLOCK_SIZE + 2];
    resp[0] = MFP_STATUS_OK;
    memcpy(&resp[1], ek_rnd_b, MFP_AES_BLOCK_SIZE);
    resp[17] = 0x00;
    resp[18] = 0x00;
    mfp_listener_send_inf(inst, resp, sizeof(resp));

    inst->part1_done = true;
    inst->part1_is_nonfirst = is_nonfirst;
    /* Don't clear inst->authed here for NonFirst — keep old session valid
     * in case Part 2 fails and we want to fall back */
    if(!is_nonfirst) inst->authed = false;
}

static void handle_auth_first_part2(MfpListener* inst, const uint8_t* data, size_t len) {
    if(!inst->part1_done || len < 33) {
        uint8_t nak = MFP_STATUS_AUTH_ERR;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    const uint8_t* key = (inst->auth_key_type == MfpKeyA) ?
                             inst->sector_keys[inst->auth_sector].key_a :
                             inst->sector_keys[inst->auth_sector].key_b;

    /* Decrypt the 32-byte token with IV=0 */
    uint8_t zero_iv[MFP_AES_BLOCK_SIZE];
    memset(zero_iv, 0, MFP_AES_BLOCK_SIZE);
    uint8_t decrypted[32];
    mfp_crypto_cbc_decrypt(key, zero_iv, &data[1], decrypted, 32);

    /* Expected plaintext: [rotate_right(RndA,1) || rotate_left(RndB,1)] */
    uint8_t rnd_a_rrot[MFP_AES_BLOCK_SIZE];
    memcpy(rnd_a_rrot, &decrypted[0], MFP_AES_BLOCK_SIZE);

    uint8_t received_rnd_b_rot[MFP_AES_BLOCK_SIZE];
    memcpy(received_rnd_b_rot, &decrypted[16], MFP_AES_BLOCK_SIZE);

    /* Verify: received_rnd_b_rot == rotate_left(our_rnd_b, 1) */
    uint8_t expected_rnd_b_rot[MFP_AES_BLOCK_SIZE];
    mfp_crypto_rotate_left(inst->rnd_b, expected_rnd_b_rot);
    if(memcmp(received_rnd_b_rot, expected_rnd_b_rot, MFP_AES_BLOCK_SIZE) != 0) {
        uint8_t nak = MFP_STATUS_AUTH_ERR;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    /* Store RndA (rrotated) for session key derivation */
    memcpy(inst->rnd_a, rnd_a_rrot, MFP_AES_BLOCK_SIZE);

    /* Derive session keys using rnd_a_rrot (what reader sent) and our rnd_b */
    mfp_crypto_derive_session_keys(
        key, rnd_a_rrot, inst->rnd_b, inst->session.k_enc, inst->session.k_mac);

    /* TI: preserve for AuthNonFirst, generate fresh for AuthFirst */
    if(inst->part1_is_nonfirst && inst->has_saved_ti) {
        memcpy(inst->session.ti, inst->saved_ti, 4);
    } else {
        /* TI = first 4 bytes of (RndA XOR RndB) */
        for(int i = 0; i < 4; i++)
            inst->session.ti[i] = rnd_a_rrot[i] ^ inst->rnd_b[i];
    }
    inst->session.r_ctr = 0;
    inst->session.w_ctr = 0;

    /* Response: 0x90 + E_CBC(key, IV=0, [TI || RndA(unrotated) || caps_zero_12])
     * Card extracts rnd_a_rrot from token and un-rotates (rotate_left by 1) to
     * recover the reader's original RndA, which is sent back for verification. */
    uint8_t rnd_a_original[MFP_AES_BLOCK_SIZE];
    mfp_crypto_rotate_left(rnd_a_rrot, rnd_a_original);

    uint8_t resp_plain[32];
    memcpy(&resp_plain[0], inst->session.ti, 4);
    memcpy(&resp_plain[4], rnd_a_original, MFP_AES_BLOCK_SIZE);
    memset(&resp_plain[20], 0, 12); /* PICCCap2(6) + PCDCap2(6) */

    uint8_t resp_enc[32];
    mfp_crypto_cbc_encrypt(key, zero_iv, resp_plain, resp_enc, 32);

    uint8_t resp[1 + 32];
    resp[0] = MFP_STATUS_OK;
    memcpy(&resp[1], resp_enc, 32);
    mfp_listener_send_inf(inst, resp, sizeof(resp));

    inst->authed = true;
    inst->auths_count++;
    inst->last_op_type = 'A';
    inst->last_op_sector = inst->auth_sector;
    inst->last_op_block = inst->auth_sector;
    if(inst->on_activity) inst->on_activity(inst->activity_ctx);
}

static uint8_t block_to_sector(MfpListener* inst, uint8_t block) {
    if(inst->size == MfpSize4K && block >= 128) {
        return 32 + (block - 128) / 16;
    }
    return block / 4;
}

static void handle_read_encrypted(MfpListener* inst, const uint8_t* data, size_t len) {
    if(!inst->authed || len < 12) {
        uint8_t nak = 0x07;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    uint8_t block_num = data[1];
    uint8_t sector = block_to_sector(inst, block_num);

    if(sector != inst->auth_sector) {
        uint8_t nak = 0x0C; /* permission denied */
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    /* Verify received MAC */
    uint8_t cmd_payload[3] = {block_num, data[2], data[3]};
    uint8_t expected_mac[MFP_MAC_SIZE];
    mfp_crypto_calculate_mac(
        inst->session.k_mac, MFP_CMD_READ_ENC, inst->session.r_ctr, inst->session.ti,
        cmd_payload, sizeof(cmd_payload), expected_mac);

    if(memcmp(&data[4], expected_mac, MFP_MAC_SIZE) != 0) {
        uint8_t nak = 0x08; /* integrity error */
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    inst->session.r_ctr++;

    /* Encrypt block data with Kenc */
    uint8_t iv[MFP_AES_BLOCK_SIZE];
    mfp_crypto_build_read_iv(inst->session.ti, inst->session.r_ctr, inst->session.w_ctr, iv);

    uint8_t enc_block[MFP_BLOCK_SIZE];
    mfp_crypto_cbc_encrypt(
        inst->session.k_enc, iv, inst->blocks[block_num], enc_block, MFP_BLOCK_SIZE);

    /* Response MAC = CMAC8(Kmac, 0x90 || r_ctr || TI || block_num || hdr || cnt || ENC_block) */
    uint8_t mac_input[3 + MFP_BLOCK_SIZE];
    memcpy(&mac_input[0], cmd_payload, 3);
    memcpy(&mac_input[3], enc_block, MFP_BLOCK_SIZE);

    uint8_t resp_mac[MFP_MAC_SIZE];
    mfp_crypto_calculate_mac(
        inst->session.k_mac, MFP_STATUS_OK, inst->session.r_ctr, inst->session.ti,
        mac_input, sizeof(mac_input), resp_mac);

    /* Response: 0x90 + enc_block(16) + mac(8) + 2 bytes pad = 27 bytes */
    uint8_t resp[1 + MFP_BLOCK_SIZE + MFP_MAC_SIZE + 2];
    resp[0] = MFP_STATUS_OK;
    memcpy(&resp[1], enc_block, MFP_BLOCK_SIZE);
    memcpy(&resp[1 + MFP_BLOCK_SIZE], resp_mac, MFP_MAC_SIZE);
    resp[25] = 0x00;
    resp[26] = 0x00;
    mfp_listener_send_inf(inst, resp, sizeof(resp));

    inst->reads_count++;
    inst->last_op_type = 'R';
    inst->last_op_block = block_num;
    inst->last_op_sector = sector;
    if(inst->on_activity) inst->on_activity(inst->activity_ctx);
}

static void handle_write_encrypted(MfpListener* inst, const uint8_t* data, size_t len) {
    /* WriteEncrypted format: 0xA1 + block(1) + 0x00(1 hdr) + enc_data(16) + mac(8) = 27 bytes */
    if(!inst->authed || len < 27) {
        uint8_t nak = 0x07;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    uint8_t block_num = data[1];
    uint8_t sector = block_to_sector(inst, block_num);
    if(sector != inst->auth_sector) {
        uint8_t nak = 0x0C;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    const uint8_t* enc_data = &data[3];
    const uint8_t* received_mac = &data[3 + MFP_BLOCK_SIZE];

    /* Verify MAC: CMAC8(Kmac, 0xA1 || w_ctr || TI || block_num || 0x00 || enc_data) */
    uint8_t mac_input[2 + MFP_BLOCK_SIZE];
    mac_input[0] = block_num;
    mac_input[1] = 0x00;
    memcpy(&mac_input[2], enc_data, MFP_BLOCK_SIZE);

    uint8_t expected_mac[MFP_MAC_SIZE];
    mfp_crypto_calculate_mac(
        inst->session.k_mac, MFP_CMD_WRITE_ENC, inst->session.w_ctr, inst->session.ti,
        mac_input, sizeof(mac_input), expected_mac);

    if(memcmp(received_mac, expected_mac, MFP_MAC_SIZE) != 0) {
        uint8_t nak = 0x08;
        mfp_listener_send_inf(inst, &nak, 1);
        return;
    }

    /* Decrypt data */
    uint8_t iv[MFP_AES_BLOCK_SIZE];
    mfp_crypto_build_write_iv(inst->session.ti, inst->session.r_ctr, inst->session.w_ctr, iv);
    mfp_crypto_cbc_decrypt(inst->session.k_enc, iv, enc_data, inst->blocks[block_num], MFP_BLOCK_SIZE);

    inst->session.w_ctr++;

    /* Response: 0x90 + MAC(CMAC8(Kmac, 0x90 || w_ctr || TI)) */
    uint8_t resp_mac[MFP_MAC_SIZE];
    mfp_crypto_calculate_mac(
        inst->session.k_mac, MFP_STATUS_OK, inst->session.w_ctr, inst->session.ti,
        NULL, 0, resp_mac);

    uint8_t resp[1 + MFP_MAC_SIZE + 2];
    resp[0] = MFP_STATUS_OK;
    memcpy(&resp[1], resp_mac, MFP_MAC_SIZE);
    resp[9] = 0x00;
    resp[10] = 0x00;
    mfp_listener_send_inf(inst, resp, sizeof(resp));

    inst->writes_count++;
    inst->last_op_type = 'W';
    inst->last_op_block = block_num;
    inst->last_op_sector = sector;
    if(inst->on_activity) inst->on_activity(inst->activity_ctx);
}

/* ---- Main listener callback ---- */

static NfcCommand mfp_listener_cb(NfcGenericEvent event, void* ctx) {
    MfpListener* inst = ctx;
    furi_assert(event.protocol == NfcProtocolIso14443_4a);

    const Iso14443_4aListenerEvent* ev = event.event_data;

    if(ev->type == Iso14443_4aListenerEventTypeHalted ||
       ev->type == Iso14443_4aListenerEventTypeFieldOff) {
        /* Reset auth state */
        inst->authed = false;
        inst->part1_done = false;
        inst->current_pcb = 0x0A;
        return NfcCommandContinue;
    }

    if(ev->type != Iso14443_4aListenerEventTypeReceivedData) {
        return NfcCommandContinue;
    }

    const BitBuffer* rx = ev->data->buffer;
    size_t rx_len = bit_buffer_get_size_bytes(rx);
    if(rx_len < 1) return NfcCommandContinue;

    const uint8_t* data = bit_buffer_get_data(rx);
    uint8_t cmd = data[0];

    static int get_version_stage = 0;

    /* Handle 0xAF continuation for GetVersion */
    if(cmd == 0xAF && get_version_stage > 0) {
        get_version_stage++;
        handle_get_version_continue(inst, get_version_stage);
        if(get_version_stage >= 3) get_version_stage = 0;
        return NfcCommandContinue;
    }
    get_version_stage = 0;

    switch(cmd) {
    case MFP_CMD_GET_VERSION:
        handle_get_version(inst, data, rx_len);
        get_version_stage = 1;
        break;
    case MFP_CMD_AUTH1_PART1:
        handle_auth_part1(inst, data, rx_len, /* is_nonfirst = */ false);
        break;
    case MFP_CMD_AUTH_NONFIRST:
        handle_auth_part1(inst, data, rx_len, /* is_nonfirst = */ true);
        break;
    case MFP_CMD_AUTH1_PART2:
        handle_auth_first_part2(inst, data, rx_len);
        break;
    case MFP_CMD_READ_ENC:
        handle_read_encrypted(inst, data, rx_len);
        break;
    case MFP_CMD_WRITE_ENC:
        handle_write_encrypted(inst, data, rx_len);
        break;
    default: {
        uint8_t nak = 0x0B; /* command not supported */
        mfp_listener_send_inf(inst, &nak, 1);
        break;
    }
    }

    return NfcCommandContinue;
}

/* ---- Public API ---- */

MfpListener* mfp_listener_alloc(Nfc* nfc) {
    MfpListener* inst = malloc(sizeof(MfpListener));
    memset(inst, 0, sizeof(*inst));
    inst->nfc = nfc;
    inst->iso_data = iso14443_4a_alloc();
    inst->current_pcb = 0x0A;
    return inst;
}

void mfp_listener_free(MfpListener* instance) {
    if(!instance) return;
    if(instance->listener) {
        nfc_listener_stop(instance->listener);
        nfc_listener_free(instance->listener);
    }
    if(instance->iso_data) iso14443_4a_free(instance->iso_data);
    free(instance);
}

void mfp_listener_set_from_app(MfpListener* instance, const MfpApp* app) {
    /* Copy blocks */
    memcpy(instance->blocks, app->blocks, sizeof(instance->blocks));

    /* Copy keys from scan results — each key A/B is independent, so
     * the emulator exposes exactly what the card has. No fallback. */
    memset(instance->sector_keys, 0, sizeof(instance->sector_keys));
    for(uint8_t s = 0; s < MFP_SECTORS_4K; s++) {
        const MfpSectorResult* r = &app->sector_results[s];
        if(r->status != MfpSectorOk) continue;
        if(r->key_a_found) {
            memcpy(instance->sector_keys[s].key_a, r->key_a, MFP_AES_KEY_SIZE);
            instance->sector_keys[s].key_a_valid = true;
        }
        if(r->key_b_found) {
            memcpy(instance->sector_keys[s].key_b, r->key_b, MFP_AES_KEY_SIZE);
            instance->sector_keys[s].key_b_valid = true;
        }
    }

    /* Card identification */
    instance->size = app->version.size;
    instance->total_sectors = mfp_sector_count(app->version.size);
    memcpy(instance->uid, app->version.uid, app->version.uid_len);
    instance->uid_len = app->version.uid_len;
    instance->sak = app->sak ? app->sak : 0x20;
    instance->atqa[0] = app->atqa[0] ? app->atqa[0] : 0x44;
    instance->atqa[1] = app->atqa[1];

    /* Configure ISO14443-4A data */
    iso14443_4a_reset(instance->iso_data);
    iso14443_4a_set_uid(instance->iso_data, instance->uid, instance->uid_len);

    Iso14443_3aData* iso3a = iso14443_4a_get_base_data(instance->iso_data);
    iso14443_3a_set_sak(iso3a, instance->sak);
    iso14443_3a_set_atqa(iso3a, instance->atqa);

    /* Set ATS historical bytes to match a real MFP card */
    instance->iso_data->ats_data.tl = 0x0C;
    instance->iso_data->ats_data.t0 = 0x75; /* TA TB TC present, FSCI=5 */
    instance->iso_data->ats_data.ta_1 = 0x77;
    instance->iso_data->ats_data.tb_1 = 0x80;
    instance->iso_data->ats_data.tc_1 = 0x02; /* CID supported */

    /* Historical bytes: C1 05 21 30 00 77 C1 (from our test card) */
    static const uint8_t hist[] = {0xC1, 0x05, 0x21, 0x30, 0x00, 0x77, 0xC1};
    simple_array_init(instance->iso_data->ats_data.t1_tk, sizeof(hist));
    memcpy(simple_array_get_data(instance->iso_data->ats_data.t1_tk), hist, sizeof(hist));
}

void mfp_listener_start(MfpListener* instance) {
    if(instance->listener) return;
    instance->listener = nfc_listener_alloc(
        instance->nfc, NfcProtocolIso14443_4a, instance->iso_data);
    instance->current_pcb = 0x0A;
    instance->authed = false;
    instance->part1_done = false;
    nfc_listener_start(instance->listener, mfp_listener_cb, instance);
}

void mfp_listener_stop(MfpListener* instance) {
    if(!instance->listener) return;
    nfc_listener_stop(instance->listener);
    nfc_listener_free(instance->listener);
    instance->listener = NULL;
}

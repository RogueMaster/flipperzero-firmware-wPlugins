#include "mfp_poller.h"
#include "mfp_crypto.h"

#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <bit_lib/bit_lib.h>
#include <furi.h>
#include <furi_hal_random.h>
#include <string.h>
#include <stdlib.h>

/* ---- helpers ---- */

#define MFP_CMD_GET_VERSION      0x60
#define MFP_CMD_AUTH1_PART1      0x70
#define MFP_CMD_AUTH_NONFIRST    0x76
#define MFP_CMD_AUTH1_PART2      0x72
#define MFP_CMD_READ_ENC         0x31
#define MFP_CMD_WRITE_ENC        0xA1

#define MFP_STATUS_OK            0x90
#define MFP_STATUS_ADDITIONAL    0xAF

static MfpError iso4a_error_to_mfp(Iso14443_4aError e) {
    return (e == Iso14443_4aErrorNone) ? MfpOk : MfpErrorComm;
}

static MfpError mfp_send_ex(
    Iso14443_4aPoller* poller,
    const uint8_t* cmd,
    size_t cmd_len,
    uint8_t* resp,
    size_t* resp_len,
    size_t resp_capacity,
    Iso14443_4aError* out_iso_err) {
    BitBuffer* tx = bit_buffer_alloc(cmd_len * 8);
    BitBuffer* rx = bit_buffer_alloc(resp_capacity * 8);

    bit_buffer_copy_bytes(tx, cmd, cmd_len);
    Iso14443_4aError iso_err = iso14443_4a_poller_send_block(poller, tx, rx);

    if(out_iso_err) *out_iso_err = iso_err;

    if(iso_err == Iso14443_4aErrorNone && resp && resp_len) {
        *resp_len = bit_buffer_get_size_bytes(rx);
        if(*resp_len > resp_capacity) *resp_len = resp_capacity;
        memcpy(resp, bit_buffer_get_data(rx), *resp_len);
    }

    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return iso4a_error_to_mfp(iso_err);
}

static MfpError mfp_send(
    Iso14443_4aPoller* poller,
    const uint8_t* cmd,
    size_t cmd_len,
    uint8_t* resp,
    size_t* resp_len,
    size_t resp_capacity) {
    return mfp_send_ex(poller, cmd, cmd_len, resp, resp_len, resp_capacity, NULL);
}

/* ---- GetVersion ---- */

MfpError mfp_poller_read_version(
    void* iso4a_poller,
    MfpVersion* out,
    int* out_iso_err,
    uint8_t* out_raw_resp,
    uint8_t* out_raw_len) {
    Iso14443_4aPoller* poller = (Iso14443_4aPoller*)iso4a_poller;

    uint8_t cmd[1] = {MFP_CMD_GET_VERSION};
    uint8_t resp[32];
    size_t resp_len = 0;

    Iso14443_4aError iso_e = Iso14443_4aErrorNone;
    MfpError err = mfp_send_ex(poller, cmd, 1, resp, &resp_len, sizeof(resp), &iso_e);
    if(out_iso_err) *out_iso_err = (int)iso_e;

    if(out_raw_resp && out_raw_len) {
        uint8_t cap_len = (resp_len > 16) ? 16 : (uint8_t)resp_len;
        memcpy(out_raw_resp, resp, cap_len);
        *out_raw_len = cap_len;
    }

    if(err != MfpOk) return err;
    if(resp_len < 8 || resp[0] != MFP_STATUS_ADDITIONAL) return MfpErrorNotMfp;

    out->hw_vendor  = resp[1];
    out->hw_type    = resp[2];
    out->hw_subtype = resp[3];
    out->hw_major   = resp[4];
    out->hw_minor   = resp[5];
    out->hw_storage = resp[6];
    out->hw_proto   = resp[7];

    uint8_t cont[1] = {MFP_STATUS_ADDITIONAL};
    err = mfp_send(poller, cont, 1, resp, &resp_len, sizeof(resp));
    if(err != MfpOk || resp_len < 8 || resp[0] != MFP_STATUS_ADDITIONAL) return MfpErrorProtocol;

    out->sw_vendor  = resp[1];
    out->sw_type    = resp[2];
    out->sw_subtype = resp[3];
    out->sw_major   = resp[4];
    out->sw_minor   = resp[5];
    out->sw_storage = resp[6];
    out->sw_proto   = resp[7];

    err = mfp_send(poller, cont, 1, resp, &resp_len, sizeof(resp));
    if(err != MfpOk || resp_len < 2 || resp[0] != 0x00) return MfpErrorProtocol;

    uint8_t uid_len = resp_len - 1;
    if(uid_len > 7) uid_len = 7;
    memcpy(out->uid, &resp[1], uid_len);
    out->uid_len = uid_len;

    out->sl = (MfpSecurityLevel)((out->hw_subtype >> 4) & 0x0F);
    if(out->sl < MfpSL1 || out->sl > MfpSL3) out->sl = MfpSL3;
    /* hw_storage per NXP MF1PLUSx0y1: 2K = 0x10, 4K = 0x18.
     * Threshold 0x13 tolerates odd clone encodings (0x11/0x12 as 2K). */
    out->size = (out->hw_storage & 0x1F) >= 0x13 ? MfpSize4K : MfpSize2K;

    if(out->hw_vendor != 0x04) return MfpErrorNotMfp;
    return MfpOk;
}

/* ---- SL3 Two-phase authentication ---- */

/* Shared implementation of AuthFirst (0x70) and AuthNonFirst (0x76).
 * The only difference is the Part 1 command byte and whether TI is preserved. */
static MfpError mfp_poller_auth_common(
    void* iso4a_poller,
    uint8_t sector,
    MfpKeyType key_type,
    const MfpKey key,
    MfpSession* session,
    uint8_t cmd_byte,
    bool preserve_ti) {
    Iso14443_4aPoller* poller = (Iso14443_4aPoller*)iso4a_poller;

    /* Save existing TI if this is a NonFirst auth */
    uint8_t saved_ti[4] = {0};
    if(preserve_ti) memcpy(saved_ti, session->ti, 4);

    /* Part 1 */
    uint16_t key_id = 0x4000U + ((uint16_t)sector << 1) + (uint8_t)key_type;
    uint8_t cmd1[4];
    cmd1[0] = cmd_byte;
    cmd1[1] = (uint8_t)(key_id & 0xFF);
    cmd1[2] = (uint8_t)((key_id >> 8) & 0xFF);
    cmd1[3] = 0x00;

    uint8_t resp1[32];
    size_t  resp1_len = 0;
    MfpError err = mfp_send(poller, cmd1, sizeof(cmd1), resp1, &resp1_len, sizeof(resp1));
    if(err != MfpOk) return err;
    if(resp1_len < 17 || resp1[0] != MFP_STATUS_OK) return MfpErrorProtocol;

    uint8_t ek_rnd_b[MFP_AES_BLOCK_SIZE];
    memcpy(ek_rnd_b, &resp1[1], MFP_AES_BLOCK_SIZE);

    /* Decrypt RndB */
    uint8_t rnd_b[MFP_AES_BLOCK_SIZE];
    mfp_crypto_ecb_decrypt(key, ek_rnd_b, rnd_b);

    /* Generate RndA */
    uint8_t rnd_a[MFP_AES_BLOCK_SIZE];
    furi_hal_random_fill_buf(rnd_a, MFP_AES_BLOCK_SIZE);

    /* Rotate RndB left by 1 byte */
    uint8_t rnd_b_rot[MFP_AES_BLOCK_SIZE];
    mfp_crypto_rotate_left(rnd_b, rnd_b_rot);

    /* Token = AES-CBC(key, iv=0, RndA_rrot || RndB_rot)
     * NXP MFP convention: first block is rotate_right(RndA, 1 byte) */
    uint8_t rnd_a_rrot[MFP_AES_BLOCK_SIZE];
    rnd_a_rrot[0] = rnd_a[MFP_AES_BLOCK_SIZE - 1];
    memcpy(&rnd_a_rrot[1], rnd_a, MFP_AES_BLOCK_SIZE - 1);

    uint8_t plain[32];
    memcpy(&plain[0],  rnd_a_rrot, MFP_AES_BLOCK_SIZE);
    memcpy(&plain[16], rnd_b_rot,  MFP_AES_BLOCK_SIZE);

    uint8_t zero_iv[MFP_AES_BLOCK_SIZE];
    memset(zero_iv, 0, MFP_AES_BLOCK_SIZE);

    uint8_t token[32];
    mfp_crypto_cbc_encrypt(key, zero_iv, plain, token, 32);

    /* Part 2 */
    uint8_t cmd2[33];
    cmd2[0] = MFP_CMD_AUTH1_PART2;
    memcpy(&cmd2[1], token, 32);

    uint8_t resp2[64];
    size_t  resp2_len = 0;
    err = mfp_send(poller, cmd2, sizeof(cmd2), resp2, &resp2_len, sizeof(resp2));
    if(err != MfpOk) return err;
    if(resp2_len < 33 || resp2[0] != MFP_STATUS_OK) return MfpErrorAuth;

    /* Decrypt response with IV=zero */
    uint8_t dec[32];
    mfp_crypto_cbc_decrypt(key, zero_iv, &resp2[1], dec, 32);

    /* Verify key correctness: caps area (bytes 20-31) must be zeros */
    for(int i = 20; i < 32; i++) {
        if(dec[i] != 0) return MfpErrorAuth;
    }

    /* Verify: card returns rotate_right(RndA, 1) at offset 4
     * (same value we sent in the token) */
    if(memcmp(&dec[4], rnd_a_rrot, MFP_AES_BLOCK_SIZE) != 0) {
        /* Also try other conventions */
        uint8_t rnd_a_rot[MFP_AES_BLOCK_SIZE];
        mfp_crypto_rotate_left(rnd_a, rnd_a_rot);
        if(memcmp(&dec[4], rnd_a_rot, MFP_AES_BLOCK_SIZE) != 0 &&
           memcmp(&dec[4], rnd_a, MFP_AES_BLOCK_SIZE) != 0) {
            return MfpErrorAuth;
        }
    }

    /* Extract TI (or preserve for NonFirst) */
    if(preserve_ti) {
        memcpy(session->ti, saved_ti, 4);
    } else {
        memcpy(session->ti, &dec[0], 4);
    }
    session->r_ctr = 0;
    session->w_ctr = 0;

    /* Derive session keys using rnd_a_rrot (what the card sees as RndA) */
    mfp_crypto_derive_session_keys(key, rnd_a_rrot, rnd_b, session->k_enc, session->k_mac);

    return MfpOk;
}

MfpError mfp_poller_auth(
    void* iso4a_poller,
    uint8_t sector,
    MfpKeyType key_type,
    const MfpKey key,
    MfpSession* session) {
    return mfp_poller_auth_common(
        iso4a_poller, sector, key_type, key, session,
        MFP_CMD_AUTH1_PART1, /* preserve_ti = */ false);
}

MfpError mfp_poller_auth_nonfirst(
    void* iso4a_poller,
    uint8_t sector,
    MfpKeyType key_type,
    const MfpKey key,
    MfpSession* session) {
    return mfp_poller_auth_common(
        iso4a_poller, sector, key_type, key, session,
        MFP_CMD_AUTH_NONFIRST, /* preserve_ti = */ true);
}

/* ---- Encrypted block read ---- */

MfpError mfp_poller_read_block(
    void* iso4a_poller,
    uint8_t block_num,
    MfpSession* session,
    MfpBlock out_block) {
    Iso14443_4aPoller* poller = (Iso14443_4aPoller*)iso4a_poller;

    uint8_t cmd_payload[3] = {block_num, 0x00, 0x01};
    uint8_t mac_t[MFP_MAC_SIZE];
    mfp_crypto_calculate_mac(
        session->k_mac, MFP_CMD_READ_ENC, session->r_ctr, session->ti,
        cmd_payload, sizeof(cmd_payload), mac_t);

    uint8_t cmd[12];
    cmd[0] = MFP_CMD_READ_ENC;
    cmd[1] = block_num;
    cmd[2] = 0x00;
    cmd[3] = 1;
    memcpy(&cmd[4], mac_t, MFP_MAC_SIZE);

    uint8_t resp[32];
    size_t  resp_len = 0;
    MfpError err = mfp_send(poller, cmd, sizeof(cmd), resp, &resp_len, sizeof(resp));
    if(err != MfpOk) return err;
    /* Response: status(1) + enc_block(16) + mac(8) [+ optional 2 pad bytes] */
    if(resp_len < 25 || resp[0] != MFP_STATUS_OK) return MfpErrorProtocol;

    session->r_ctr++;

    uint8_t mac_resp_input[3 + MFP_BLOCK_SIZE];
    memcpy(&mac_resp_input[0], cmd_payload, sizeof(cmd_payload));
    memcpy(&mac_resp_input[3], &resp[1], MFP_BLOCK_SIZE);

    uint8_t expected_mac[MFP_MAC_SIZE];
    mfp_crypto_calculate_mac(
        session->k_mac, MFP_STATUS_OK, session->r_ctr, session->ti,
        mac_resp_input, sizeof(mac_resp_input), expected_mac);

    if(memcmp(&resp[17], expected_mac, MFP_MAC_SIZE) != 0) return MfpErrorMac;

    uint8_t iv[MFP_AES_BLOCK_SIZE];
    mfp_crypto_build_read_iv(session->ti, session->r_ctr, session->w_ctr, iv);
    mfp_crypto_cbc_decrypt(session->k_enc, iv, &resp[1], out_block, MFP_BLOCK_SIZE);

    return MfpOk;
}

/* ---- Encrypted block write ---- */

MfpError mfp_poller_write_block(
    void* iso4a_poller,
    uint8_t block_num,
    MfpSession* session,
    const uint8_t data[MFP_BLOCK_SIZE]) {
    Iso14443_4aPoller* poller = (Iso14443_4aPoller*)iso4a_poller;

    /* Encrypt block data with Kenc and write IV */
    uint8_t iv[MFP_AES_BLOCK_SIZE];
    mfp_crypto_build_write_iv(session->ti, session->r_ctr, session->w_ctr, iv);

    uint8_t enc_data[MFP_BLOCK_SIZE];
    mfp_crypto_cbc_encrypt(session->k_enc, iv, data, enc_data, MFP_BLOCK_SIZE);

    /* MAC_t = CMAC8(Kmac, cmd || w_ctr || TI || block_num || 0x00 || enc_data)
     * Per PM3 trace: MAC input includes a 0x00 extension byte after block_num */
    uint8_t mac_input[2 + MFP_BLOCK_SIZE];
    mac_input[0] = block_num;
    mac_input[1] = 0x00; /* ext/header byte */
    memcpy(&mac_input[2], enc_data, MFP_BLOCK_SIZE);

    uint8_t mac_t[MFP_MAC_SIZE];
    mfp_crypto_calculate_mac(
        session->k_mac, MFP_CMD_WRITE_ENC, session->w_ctr, session->ti,
        mac_input, sizeof(mac_input), mac_t);

    /* Command: 0xA1 + block_num(1) + 0x00(1 header) + enc_data(16) + mac(8) = 27 bytes */
    uint8_t cmd[1 + 2 + MFP_BLOCK_SIZE + MFP_MAC_SIZE];
    cmd[0] = MFP_CMD_WRITE_ENC;
    cmd[1] = block_num;
    cmd[2] = 0x00;
    memcpy(&cmd[3], enc_data, MFP_BLOCK_SIZE);
    memcpy(&cmd[3 + MFP_BLOCK_SIZE], mac_t, MFP_MAC_SIZE);

    uint8_t resp[16];
    size_t  resp_len = 0;
    MfpError err = mfp_send(poller, cmd, sizeof(cmd), resp, &resp_len, sizeof(resp));
    if(err != MfpOk) return err;

    /* Response: 0x90 + MAC_r(8) */
    if(resp_len < 9 || resp[0] != MFP_STATUS_OK) return MfpErrorProtocol;

    session->w_ctr++;

    /* Verify MAC_r = CMAC8(Kmac, 0x90 || w_ctr || TI) — no data */
    uint8_t expected_mac[MFP_MAC_SIZE];
    mfp_crypto_calculate_mac(
        session->k_mac, MFP_STATUS_OK, session->w_ctr, session->ti,
        NULL, 0, expected_mac);

    if(memcmp(&resp[1], expected_mac, MFP_MAC_SIZE) != 0) return MfpErrorMac;

    return MfpOk;
}

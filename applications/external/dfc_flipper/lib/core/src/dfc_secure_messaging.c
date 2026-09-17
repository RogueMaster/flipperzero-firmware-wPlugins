#include "dfc_secure_messaging.h"
#include "dfc_port.h"
#include <string.h>

#define TAG "DfcSecureMessaging"

DfcSecureMessaging* dfc_secure_messaging_alloc(
    uint8_t cipher,
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* initial_iv) {
    if(!session_key || session_key_len > DFC_MAX_KEY_LEN) return NULL;

    DfcSecureMessaging* sm =
        dfc_platform_alloc(sizeof(DfcSecureMessaging), DfcAllocSecureMessaging);
    if(!sm) return NULL;
    memset(sm, 0, sizeof(DfcSecureMessaging));

    sm->cipher = cipher;
    sm->session_key_len = session_key_len;
    memcpy(sm->session_key, session_key, session_key_len);
    if(initial_iv) {
        memcpy(sm->iv, initial_iv, sizeof(sm->iv));
    }

    return sm;
}

void dfc_secure_messaging_free(DfcSecureMessaging* sm) {
    DFC_ASSERT(sm);
    dfc_platform_free(sm);
}

void dfc_secure_messaging_reset_iv(DfcSecureMessaging* sm) {
    DFC_ASSERT(sm);
    memset(sm->iv, 0, sizeof(sm->iv));
}

bool dfc_secure_messaging_applies_ev1(DfcSecureMessaging* sm, uint8_t cmd) {
    DFC_ASSERT(sm);
    return sm->cipher != DFC_CMD_AUTHENTICATE_LEGACY && cmd != DFC_CMD_SELECT_APPLICATION &&
           cmd != DFC_CMD_AUTHENTICATE_LEGACY && cmd != DFC_CMD_AUTHENTICATE_ISO &&
           cmd != DFC_CMD_AUTHENTICATE_AES && cmd != DFC_CMD_ADDITIONAL_FRAME;
}

static uint16_t crc16_iso14443(const uint8_t* data, size_t len) {
    uint16_t crc = 0x6363;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i] ^ (crc & 0xFF);
        byte ^= byte << 4;
        crc = (crc >> 8) ^ ((uint16_t)byte << 8) ^ ((uint16_t)byte << 3) ^ ((uint16_t)byte >> 4);
    }
    return crc;
}

static uint32_t crc32_dfc_extend(uint32_t crc, const uint8_t* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(size_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0 - (crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return crc;
}

// D40 sessions use a 4-byte MAC field. ISO(0x1A)/AES(0xAA) sessions use an
// 8-byte truncated CMAC.
static size_t mac_len_for_cipher(uint8_t cipher) {
    return cipher == DFC_CMD_AUTHENTICATE_LEGACY ? 4 : 8;
}

static void d40_mac(DfcSecureMessaging* sm, const uint8_t* data, size_t data_len, uint8_t* mac_out) {
    size_t padded_len = ((data_len + 7) / 8) * 8;
    uint8_t* padded = sm->mac_input_scratch;
    uint8_t* encrypted = sm->crypto_scratch;
    memcpy(padded, data, data_len);
    if(padded_len > data_len) {
        memset(padded + data_len, 0, padded_len - data_len);
    }

    uint8_t iv[8] = {0};
    dfc_worker_des_cbc_encrypt(
        sm->session_key, sm->session_key_len, iv, padded_len, padded, encrypted);
    memcpy(mac_out, encrypted + padded_len - 8, 4);
}

static void
    compute_mac(DfcSecureMessaging* sm, const uint8_t* data, size_t data_len, uint8_t* mac_out) {
    if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        d40_mac(sm, data, data_len, mac_out);
        return;
    }

    if(sm->cipher == DFC_CMD_AUTHENTICATE_AES) {
        uint8_t full_mac[16];
        aes_cmac(sm->session_key, sm->session_key_len, (uint8_t*)data, data_len, full_mac);
        // DESFire truncates the AES CMAC to every other byte, taking bytes 1,3,5,...,15.
        for(size_t i = 0; i < 8; i++) {
            mac_out[i] = full_mac[(i * 2) + 1];
        }
        return;
    }

    // des_cmac() selects the correct 1/2/3-key DES engine from session_key_len
    // (8/16/24), matching whichever key length the ISO(0x1A) session actually used.
    // A DES CMAC is one 8-byte block, which is the MAC as sent; there is nothing
    // to truncate.
    des_cmac(sm->session_key, sm->session_key_len, (uint8_t*)data, data_len, mac_out);
}

static size_t compute_full_cmac(
    DfcSecureMessaging* sm,
    const uint8_t* data,
    size_t data_len,
    uint8_t* mac_out) {
    if(sm->cipher == DFC_CMD_AUTHENTICATE_AES) {
        aes_cmac_with_iv(
            sm->session_key, sm->session_key_len, (uint8_t*)data, data_len, sm->iv, mac_out);
        return 16;
    }

    des_cmac_with_iv(
        sm->session_key, sm->session_key_len, (uint8_t*)data, data_len, sm->iv, mac_out);
    return 8;
}

static void
    update_iv_from_full_cmac(DfcSecureMessaging* sm, const uint8_t* full_mac, size_t full_mac_len) {
    memset(sm->iv, 0, sizeof(sm->iv));
    memcpy(sm->iv, full_mac, DFC_MIN(full_mac_len, sizeof(sm->iv)));
}

void dfc_secure_messaging_update_ev1_command(
    DfcSecureMessaging* sm,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len) {
    if(!dfc_secure_messaging_applies_ev1(sm, cmd)) return;

    uint8_t* mac_input = sm->mac_input_scratch;
    mac_input[0] = cmd;
    memcpy(mac_input + 1, data, data_len);

    uint8_t full_mac[16];
    size_t full_mac_len = compute_full_cmac(sm, mac_input, data_len + 1, full_mac);
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
}

bool dfc_secure_messaging_ev1_transmits_command_mac(uint8_t cmd) {
    switch(cmd) {
    case DFC_CMD_WRITE_DATA:
    case DFC_CMD_CREDIT:
    case DFC_CMD_DEBIT:
    case DFC_CMD_LIMITED_CREDIT:
        return true;
    default:
        return false;
    }
}

size_t dfc_secure_messaging_verify_ev1_transmitted_command_mac(
    DfcSecureMessaging* sm,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len) {
    DFC_ASSERT(sm);
    size_t truncated_len = 8;
    if(data_len < truncated_len) return SIZE_MAX;

    size_t plain_len = data_len - truncated_len;
    uint8_t* mac_input = sm->mac_input_scratch;
    mac_input[0] = cmd;
    memcpy(mac_input + 1, data, plain_len);

    uint8_t full_mac[16];
    size_t full_mac_len = compute_full_cmac(sm, mac_input, plain_len + 1, full_mac);
    size_t trunc = DFC_MIN((size_t)8, full_mac_len);
    if(memcmp(full_mac, data + plain_len, trunc) != 0) {
        DFC_LOG_W(TAG, "EV1 command CMAC mismatch");
        return SIZE_MAX;
    }
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    return plain_len;
}

size_t dfc_secure_messaging_generate_ev1_response(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out) {
    uint8_t* mac_input = sm->mac_input_scratch;
    memcpy(mac_input, plain, plain_len);
    mac_input[plain_len] = status;

    uint8_t full_mac[16];
    size_t full_mac_len = compute_full_cmac(sm, mac_input, plain_len + 1, full_mac);
    size_t truncated_len = DFC_MIN((size_t)8, full_mac_len);

    memcpy(out, plain, plain_len);
    memcpy(out + plain_len, full_mac, truncated_len);
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    return plain_len + truncated_len;
}

size_t dfc_secure_messaging_unwrap_ev1_response(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out) {
    size_t full_mac_len = sm->cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
    size_t truncated_len = DFC_MIN((size_t)8, full_mac_len);
    if(wrapped_len < truncated_len) return SIZE_MAX;

    size_t plain_len = wrapped_len - truncated_len;
    uint8_t* mac_input = sm->mac_input_scratch;
    memcpy(mac_input, wrapped, plain_len);
    mac_input[plain_len] = status;

    uint8_t full_mac[16];
    full_mac_len = compute_full_cmac(sm, mac_input, plain_len + 1, full_mac);
    truncated_len = DFC_MIN((size_t)8, full_mac_len);
    if(memcmp(full_mac, wrapped + plain_len, truncated_len) != 0) {
        DFC_LOG_W(TAG, "EV1 response CMAC mismatch");
        return SIZE_MAX;
    }

    memcpy(out, wrapped, plain_len);
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    return plain_len;
}

// A legacy (D40) session picks its DES primitive by party, not by direction: the
// PICC encrypts what it sends and encrypts again to recover what it receives,
// while the PCD decrypts in both directions. So a cryptogram travelling to the
// PICC is C[i] = dec(P[i] ^ C[i-1]) and one travelling to the PCD is
// C[i] = enc(P[i] ^ C[i-1]); each party recovers with its own primitive. With
// encrypt = true these two are the ordinary CBC encrypt and decrypt.
static void legacy_ecb(const DfcSecureMessaging* sm, bool encrypt, const uint8_t* in, uint8_t* out) {
    uint8_t iv[8] = {0};
    if(encrypt) {
        dfc_worker_des_cbc_encrypt(sm->session_key, sm->session_key_len, iv, 8, in, out);
    } else {
        dfc_worker_des_cbc_decrypt(sm->session_key, sm->session_key_len, iv, 8, in, out);
    }
}

// C[i] = prim(P[i] ^ C[i-1])
static void legacy_cbc_forward(
    const DfcSecureMessaging* sm,
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    uint8_t prev[8] = {0};
    for(size_t off = 0; off + 8 <= len; off += 8) {
        uint8_t blk[8];
        for(size_t i = 0; i < 8; i++) blk[i] = in[off + i] ^ prev[i];
        legacy_ecb(sm, encrypt, blk, out + off);
        memcpy(prev, out + off, 8);
    }
}

// P[i] = prim(C[i]) ^ C[i-1]
static void legacy_cbc_reverse(
    const DfcSecureMessaging* sm,
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    uint8_t prev[8] = {0};
    for(size_t off = 0; off + 8 <= len; off += 8) {
        uint8_t blk[8];
        legacy_ecb(sm, encrypt, in + off, blk);
        for(size_t i = 0; i < 8; i++) out[off + i] = blk[i] ^ prev[i];
        memcpy(prev, in + off, 8);
    }
}

// Shared by both directions: enciphered mode is a plain CBC-encrypt-with-CRC scheme with no
// directional asymmetry (unlike MAC mode, which MACs a different one-byte prefix/suffix
// depending on direction).
static size_t enciphered_encode(
    DfcSecureMessaging* sm,
    const uint8_t* crc_prefix,
    size_t crc_prefix_len,
    const uint8_t* plain,
    size_t plain_len,
    const uint8_t* crc_suffix,
    size_t crc_suffix_len,
    uint8_t* out) {
    size_t block_size = sm->cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
    uint8_t* clear = sm->crypto_scratch;
    memcpy(clear, plain, plain_len);

    size_t with_crc_len;
    if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        uint16_t crc = crc16_iso14443(plain, plain_len);
        clear[plain_len] = crc & 0xFF;
        clear[plain_len + 1] = (crc >> 8) & 0xFF;
        with_crc_len = plain_len + 2;
    } else {
        uint32_t crc = 0xFFFFFFFF;
        crc = crc32_dfc_extend(crc, crc_prefix, crc_prefix_len);
        crc = crc32_dfc_extend(crc, plain, plain_len);
        crc = crc32_dfc_extend(crc, crc_suffix, crc_suffix_len);
        clear[plain_len] = crc & 0xFF;
        clear[plain_len + 1] = (crc >> 8) & 0xFF;
        clear[plain_len + 2] = (crc >> 16) & 0xFF;
        clear[plain_len + 3] = (crc >> 24) & 0xFF;
        with_crc_len = plain_len + 4;
    }

    size_t padded_len = ((with_crc_len + block_size - 1) / block_size) * block_size;
    if(padded_len == 0) padded_len = block_size;

    // Pad with zeroes up to padded_len
    if(padded_len > with_crc_len) {
        memset(clear + with_crc_len, 0, padded_len - with_crc_len);
    }

    // D40 (0x0A) sessions reset the IV to zero for every operation; ISO(0x1A)/AES(0xAA)
    // sessions chain a single shared IV across every operation (hardware-confirmed - see
    // DfcSecureMessaging's iv field comment). The CBC helpers update the passed IV
    // buffer in place to the chaining state after the call, so using sm->iv directly here
    // is what makes the ISO/AES chaining work automatically across successive calls.
    uint8_t zero_iv[16] = {0};
    uint8_t* iv = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? zero_iv : sm->iv;

    if(sm->cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_encrypt(
            sm->session_key, sm->session_key_len, iv, padded_len, clear, out);
    } else if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        legacy_cbc_forward(sm, !sm->pcd, clear, padded_len, out);
    } else {
        dfc_worker_des_cbc_encrypt(
            sm->session_key, sm->session_key_len, iv, padded_len, clear, out);
    }
    return padded_len;
}

static size_t enciphered_decode(
    DfcSecureMessaging* sm,
    const uint8_t* crc_prefix,
    size_t crc_prefix_len,
    const uint8_t* crc_suffix,
    size_t crc_suffix_len,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out) {
    uint8_t* clear = sm->crypto_scratch;
    uint8_t zero_iv[16] = {0};
    uint8_t* iv = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? zero_iv : sm->iv;

    if(sm->cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_decrypt(
            sm->session_key, sm->session_key_len, iv, wrapped_len, wrapped, clear);
    } else if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        legacy_cbc_reverse(sm, !sm->pcd, wrapped, wrapped_len, clear);
    } else {
        dfc_worker_des_cbc_decrypt(
            sm->session_key, sm->session_key_len, iv, wrapped_len, wrapped, clear);
    }

    size_t crc_len = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? 2 : 4;
    if(wrapped_len < crc_len) return 0;

    size_t data_len = SIZE_MAX;
    if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        // Where the payload ends is not on the wire, so try each boundary whose
        // tail is padding and keep the first whose CRC16 agrees. Accepting a
        // boundary without checking the CRC would admit any ciphertext at all.
        for(size_t candidate = 0; candidate + crc_len <= wrapped_len; candidate++) {
            size_t crc_offset = candidate;
            bool padding_is_zero = true;
            for(size_t i = crc_offset + crc_len; i < wrapped_len; i++) {
                if(clear[i] != 0x00) {
                    padding_is_zero = false;
                    break;
                }
            }
            if(!padding_is_zero) continue;

            uint16_t expected_crc = crc16_iso14443(clear, candidate);
            uint16_t actual_crc =
                (uint16_t)clear[crc_offset] | (uint16_t)((uint16_t)clear[crc_offset + 1] << 8);
            if(actual_crc == expected_crc) {
                data_len = candidate;
                break;
            }
        }
        if(data_len == SIZE_MAX) return 0;
    } else {
        for(size_t clear_len = 0; clear_len <= wrapped_len - crc_len; clear_len++) {
            size_t crc_offset = clear_len;
            bool padding_is_zero = true;
            for(size_t i = crc_offset + crc_len; i < wrapped_len; i++) {
                if(clear[i] != 0x00) {
                    padding_is_zero = false;
                    break;
                }
            }
            if(!padding_is_zero) continue;

            uint32_t expected_crc = 0xFFFFFFFF;
            expected_crc = crc32_dfc_extend(expected_crc, crc_prefix, crc_prefix_len);
            expected_crc = crc32_dfc_extend(expected_crc, clear, clear_len);
            expected_crc = crc32_dfc_extend(expected_crc, crc_suffix, crc_suffix_len);

            uint32_t actual_crc =
                (uint32_t)clear[crc_offset] | ((uint32_t)clear[crc_offset + 1] << 8) |
                ((uint32_t)clear[crc_offset + 2] << 16) | ((uint32_t)clear[crc_offset + 3] << 24);
            if(actual_crc == expected_crc) {
                data_len = clear_len;
                break;
            }
        }
        if(data_len == SIZE_MAX) return 0;
    }

    memcpy(out, clear, data_len);
    return data_len;
}

// PCD -> PICC direction (reader sending a command). `header` is the command's
// cleartext head, Cmd followed by whatever parameters precede the payload, e.g.
// Cmd||FileNo||Offset||Length for a write. An EV1 session covers it; a legacy
// session covers the payload alone, both for the MAC and for the CRC.
size_t dfc_secure_messaging_wrap(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out) {
    if(comm_mode == DFC_COMM_PLAIN) {
        memcpy(out, plain, plain_len);
        return plain_len;
    }

    bool legacy = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY;

    if(comm_mode == DFC_COMM_MAC) {
        size_t mac_len = mac_len_for_cipher(sm->cipher);
        size_t prefix_len = legacy ? 0 : header_len;
        uint8_t* mac_input = sm->mac_input_scratch;
        if(prefix_len) memcpy(mac_input, header, prefix_len);
        memcpy(mac_input + prefix_len, plain, plain_len);

        uint8_t mac[8];
        compute_mac(sm, mac_input, prefix_len + plain_len, mac);

        memcpy(out, plain, plain_len);
        memcpy(out + plain_len, mac, mac_len);
        return plain_len + mac_len;
    }

    return enciphered_encode(sm, header, header_len, plain, plain_len, NULL, 0, out);
}

// PICC -> PCD direction (reader receiving a response): MAC covers [...data, status].
size_t dfc_secure_messaging_unwrap(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out) {
    if(comm_mode == DFC_COMM_PLAIN) {
        memcpy(out, wrapped, wrapped_len);
        return wrapped_len;
    }

    if(comm_mode == DFC_COMM_MAC) {
        size_t mac_len = mac_len_for_cipher(sm->cipher);
        if(wrapped_len < mac_len) return 0;
        size_t data_len = wrapped_len - mac_len;

        // An EV1 session covers the status byte; a legacy session covers the
        // payload alone.
        size_t suffix_len = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? 0 : 1;
        uint8_t* mac_input = sm->mac_input_scratch;
        memcpy(mac_input, wrapped, data_len);
        if(suffix_len) mac_input[data_len] = status;

        uint8_t mac[8];
        compute_mac(sm, mac_input, data_len + suffix_len, mac);

        if(memcmp(mac, wrapped + data_len, mac_len) != 0) {
            DFC_LOG_W(TAG, "MAC mismatch");
            return 0;
        }

        memcpy(out, wrapped, data_len);
        return data_len;
    }

    return enciphered_decode(sm, NULL, 0, &status, 1, wrapped, wrapped_len, out);
}

// PICC -> PCD direction (emulator generating a response): mirrors dfc_secure_messaging_unwrap's
// MAC construction ([...data, status]) so a real reader's unwrap() call will verify it.
size_t dfc_secure_messaging_generate_response(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out) {
    if(comm_mode == DFC_COMM_PLAIN) {
        memcpy(out, plain, plain_len);
        return plain_len;
    }

    if(comm_mode == DFC_COMM_MAC) {
        size_t mac_len = mac_len_for_cipher(sm->cipher);
        // Mirrors dfc_secure_messaging_unwrap: EV1 covers the status byte, legacy
        // covers the payload alone.
        size_t suffix_len = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? 0 : 1;
        uint8_t* mac_input = sm->mac_input_scratch;
        memcpy(mac_input, plain, plain_len);
        if(suffix_len) mac_input[plain_len] = status;

        uint8_t mac[8];
        compute_mac(sm, mac_input, plain_len + suffix_len, mac);

        memcpy(out, plain, plain_len);
        memcpy(out + plain_len, mac, mac_len);
        return plain_len + mac_len;
    }

    return enciphered_encode(sm, NULL, 0, plain, plain_len, &status, 1, out);
}

// PCD -> PICC direction (emulator verifying an incoming command): mirrors
// dfc_secure_messaging_wrap, so `header` has the same meaning there.
size_t dfc_secure_messaging_verify_command(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out) {
    if(comm_mode == DFC_COMM_PLAIN) {
        memcpy(out, wrapped, wrapped_len);
        return wrapped_len;
    }

    bool legacy = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY;

    if(comm_mode == DFC_COMM_MAC) {
        size_t mac_len = mac_len_for_cipher(sm->cipher);
        if(wrapped_len < mac_len) return 0;
        size_t data_len = wrapped_len - mac_len;

        size_t prefix_len = legacy ? 0 : header_len;
        uint8_t* mac_input = sm->mac_input_scratch;
        if(prefix_len) memcpy(mac_input, header, prefix_len);
        memcpy(mac_input + prefix_len, wrapped, data_len);

        uint8_t mac[8];
        compute_mac(sm, mac_input, prefix_len + data_len, mac);

        if(memcmp(mac, wrapped + data_len, mac_len) != 0) {
            DFC_LOG_W(TAG, "MAC mismatch");
            return 0;
        }

        memcpy(out, wrapped, data_len);
        return data_len;
    }

    return enciphered_decode(sm, header, header_len, NULL, 0, wrapped, wrapped_len, out);
}

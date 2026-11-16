#include "dfc_secure_messaging.h"
#include "dfc_crypto_internal.h"
#include "dfc_port.h"
#include <string.h>

#define TAG "DfcSecureMessaging"

static bool cipher_accepts_key_length(uint8_t cipher, size_t key_len) {
    switch(cipher) {
    case DFC_CMD_AUTHENTICATE_LEGACY:
        return key_len == 8 || key_len == 16;
    case DFC_CMD_AUTHENTICATE_ISO:
        return key_len == 8 || key_len == 16 || key_len == 24;
    case DFC_CMD_AUTHENTICATE_AES:
        return key_len == DFC_AES_KEY_LENGTH;
    default:
        return false;
    }
}

static bool session_is_valid(const DfcSecureMessaging* sm) {
    return sm && cipher_accepts_key_length(sm->cipher, sm->session_key_len);
}

static bool output_fits(uint8_t* out, size_t capacity, size_t required) {
    return required == 0 || (out && capacity >= required);
}

bool dfc_secure_messaging_init(
    DfcSecureMessaging* sm,
    uint8_t cipher,
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* initial_iv) {
    if(!sm) return false;
    if(!session_key || !cipher_accepts_key_length(cipher, session_key_len)) {
        dfc_secure_wipe(sm, sizeof(*sm));
        return false;
    }

    memset(sm, 0, sizeof(*sm));
    sm->cipher = cipher;
    sm->session_key_len = (uint8_t)session_key_len;
    memcpy(sm->session_key, session_key, session_key_len);
    if(initial_iv) memcpy(sm->iv, initial_iv, sizeof(sm->iv));
    return true;
}

DfcSecureMessaging* dfc_secure_messaging_alloc(
    uint8_t cipher,
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* initial_iv) {
    if(!session_key || !cipher_accepts_key_length(cipher, session_key_len)) return NULL;

    DfcSecureMessaging* sm =
        dfc_platform_alloc(sizeof(DfcSecureMessaging), DfcAllocSecureMessaging);
    if(!sm) return NULL;
    if(!dfc_secure_messaging_init(sm, cipher, session_key, session_key_len, initial_iv)) {
        dfc_platform_free(sm);
        return NULL;
    }
    return sm;
}

void dfc_secure_messaging_free(DfcSecureMessaging* sm) {
    DFC_ASSERT(sm);
    dfc_secure_wipe(sm, sizeof(*sm));
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
           cmd != DFC_CMD_AUTHENTICATE_AES && cmd != DFC_CMD_AUTHENTICATE_EV2_FIRST &&
           cmd != DFC_CMD_AUTHENTICATE_EV2_NON_FIRST && cmd != DFC_CMD_ADDITIONAL_FRAME;
}

// Every candidate at or after this boundary has a valid zero or 0x80/zero
// padding tail. Earlier candidates cannot verify, so do not recompute their CRCs.
static size_t first_padded_clear_length(const uint8_t* data, size_t len, size_t crc_len) {
    size_t nonzero_end = len;
    while(nonzero_end > 0 && data[nonzero_end - 1] == 0)
        nonzero_end--;
    if(nonzero_end == 0) return 0;

    size_t last_nonzero = nonzero_end - 1;
    size_t required_crc_end =
        last_nonzero + (data[last_nonzero] == DFC_SM_LENGTH_MARKER ? 0u : 1u);
    return required_crc_end > crc_len ? required_crc_end - crc_len : 0;
}

typedef DfcCryptoSegment DfcMacSegment;

static bool d40_mac_segments(
    DfcSecureMessaging* sm,
    const DfcMacSegment* segments,
    size_t segment_count,
    uint8_t* mac_out) {
    size_t total_len = 0;
    for(size_t i = 0; i < segment_count; i++) {
        if((segments[i].length && !segments[i].data) ||
           segments[i].length > SIZE_MAX - total_len) {
            memset(mac_out, 0, DFC_SM_LEGACY_MAC_LENGTH);
            return false;
        }
        total_len += segments[i].length;
    }
    if(total_len == 0) {
        memset(mac_out, 0, DFC_SM_LEGACY_MAC_LENGTH);
        return true;
    }
    const size_t block_size = DFC_SM_LEGACY_BLOCK_SIZE;
    uint8_t iv[DFC_SM_LEGACY_BLOCK_SIZE] = {0};
    uint8_t block[DFC_SM_LEGACY_BLOCK_SIZE] = {0};
    uint8_t encrypted[DFC_SM_LEGACY_BLOCK_SIZE] = {0};
    DfcCryptoDesBlock cipher;
    bool ok = dfc_crypto_des_block_init(&cipher, true, sm->session_key, sm->session_key_len);
    if(!ok) {
        memset(mac_out, 0, DFC_SM_LEGACY_MAC_LENGTH);
        return false;
    }
    size_t pending_len = 0;
    for(size_t segment_index = 0; segment_index < segment_count && ok; segment_index++) {
        const uint8_t* data = segments[segment_index].data;
        size_t remaining = segments[segment_index].length;
        while(remaining) {
            size_t take = block_size - pending_len;
            if(take > remaining) take = remaining;
            memcpy(block + pending_len, data, take);
            pending_len += take;
            data += take;
            remaining -= take;
            if(pending_len == block_size) {
                for(size_t i = 0; i < block_size; i++)
                    block[i] ^= iv[i];
                ok = dfc_crypto_des_block_crypt(&cipher, block, encrypted);
                if(!ok) break;
                memcpy(iv, encrypted, sizeof(iv));
                memset(block, 0, sizeof(block));
                pending_len = 0;
            }
        }
    }
    if(ok && pending_len) {
        for(size_t i = 0; i < block_size; i++)
            block[i] ^= iv[i];
        ok = dfc_crypto_des_block_crypt(&cipher, block, encrypted);
    }
    if(ok) memcpy(mac_out, encrypted, DFC_SM_LEGACY_MAC_LENGTH);
    dfc_crypto_des_block_clear(&cipher);
    dfc_secure_wipe(iv, sizeof(iv));
    dfc_secure_wipe(block, sizeof(block));
    dfc_secure_wipe(encrypted, sizeof(encrypted));
    if(!ok) memset(mac_out, 0, DFC_SM_LEGACY_MAC_LENGTH);
    return ok;
}

static bool compute_full_cmac_segments(
    DfcSecureMessaging* sm,
    const DfcMacSegment* segments,
    size_t segment_count,
    uint8_t* mac_out,
    size_t* mac_len) {
    *mac_len = 0;
    if(sm->cipher == DFC_CMD_AUTHENTICATE_AES) {
        if(!aes_cmac_segments_with_iv(
               sm->session_key, sm->session_key_len, segments, segment_count, sm->iv, mac_out)) {
            return false;
        }
        *mac_len = 16;
        return true;
    }

    bool ok = des_cmac_segments_with_iv(
        sm->session_key, sm->session_key_len, segments, segment_count, sm->iv, mac_out);
    if(!ok) {
        return false;
    }
    *mac_len = 8;
    return true;
}

static void
    update_iv_from_full_cmac(DfcSecureMessaging* sm, const uint8_t* full_mac, size_t full_mac_len) {
    memset(sm->iv, 0, sizeof(sm->iv));
    memcpy(sm->iv, full_mac, DFC_MIN(full_mac_len, sizeof(sm->iv)));
}

bool dfc_secure_messaging_update_ev1_command(
    DfcSecureMessaging* sm,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len) {
    if(!sm || (data_len && !data)) return false;
    // AdditionalFrame is normally excluded because it also carries auth steps.
    // A caller may explicitly advance the IV for a GetVersion continuation.
    if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ||
       (cmd != DFC_CMD_ADDITIONAL_FRAME && !dfc_secure_messaging_applies_ev1(sm, cmd)))
        return true;

    DfcMacSegment segments[] = {
        {.data = &cmd, .length = 1},
        {.data = data, .length = data_len},
    };
    uint8_t full_mac[16] = {0};
    size_t full_mac_len = 0;
    bool ok = compute_full_cmac_segments(sm, segments, 2, full_mac, &full_mac_len);
    if(!ok) {
        dfc_secure_wipe(full_mac, sizeof(full_mac));
        return false;
    }
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    return true;
}

bool dfc_secure_messaging_update_ev1_command_full(
    DfcSecureMessaging* sm,
    const uint8_t* command,
    size_t command_len) {
    if(!sm || !command || command_len == 0) return false;
    if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ||
       !dfc_secure_messaging_applies_ev1(sm, command[0])) {
        return true;
    }
    DfcMacSegment segment = {.data = command, .length = command_len};
    uint8_t full_mac[16] = {0};
    size_t full_mac_len = 0;
    if(!compute_full_cmac_segments(sm, &segment, 1, full_mac, &full_mac_len)) {
        dfc_secure_wipe(full_mac, sizeof(full_mac));
        return false;
    }
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    return true;
}

bool dfc_secure_messaging_ev1_transmits_command_mac(uint8_t cmd) {
    switch(dfc_record_command_base(cmd)) {
    case DFC_CMD_WRITE_DATA:
    case DFC_CMD_WRITE_RECORD:
    case DFC_CMD_UPDATE_RECORD:
    case DFC_CMD_UPDATE_RECORD_ISO:
    case DFC_CMD_CREDIT:
    case DFC_CMD_DEBIT:
    case DFC_CMD_LIMITED_CREDIT:
    case DFC_CMD_GET_FILE_COUNTERS:
    case DFC_CMD_COMMIT_READER_ID:
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
    DfcMacSegment segments[] = {
        {.data = &cmd, .length = 1},
        {.data = data, .length = plain_len},
    };
    uint8_t full_mac[16] = {0};
    size_t full_mac_len = 0;
    bool ok = compute_full_cmac_segments(sm, segments, 2, full_mac, &full_mac_len);
    if(!ok) {
        dfc_secure_wipe(full_mac, sizeof(full_mac));
        return SIZE_MAX;
    }
    size_t trunc = DFC_MIN((size_t)8, full_mac_len);
    if(!dfc_ct_equal(full_mac, data + plain_len, trunc)) {
        DFC_LOG_W(TAG, "EV1 command CMAC mismatch");
        dfc_secure_wipe(full_mac, sizeof(full_mac));
        return SIZE_MAX;
    }
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    return plain_len;
}

size_t dfc_secure_messaging_verify_ev1_transmitted_command_mac_full(
    DfcSecureMessaging* sm,
    const uint8_t* command,
    size_t command_len) {
    if(!sm || !command || command_len < 1 + DFC_WIRE_MAC_LENGTH) return SIZE_MAX;
    size_t clear_len = command_len - DFC_WIRE_MAC_LENGTH;
    DfcMacSegment segment = {.data = command, .length = clear_len};
    uint8_t full_mac[16] = {0};
    size_t full_mac_len = 0;
    if(!compute_full_cmac_segments(sm, &segment, 1, full_mac, &full_mac_len) ||
       full_mac_len < DFC_WIRE_MAC_LENGTH ||
       !dfc_ct_equal(full_mac, command + clear_len, DFC_WIRE_MAC_LENGTH)) {
        dfc_secure_wipe(full_mac, sizeof(full_mac));
        return SIZE_MAX;
    }
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    return clear_len - 1;
}

bool dfc_secure_messaging_generate_ev1_response_mac(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t mac[DFC_WIRE_MAC_LENGTH]) {
    if(!session_is_valid(sm) || sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY || !mac ||
       (plain_len && !plain))
        return false;
    DfcMacSegment segments[] = {
        {.data = plain, .length = plain_len},
        {.data = &status, .length = 1},
    };
    uint8_t full_mac[16] = {0};
    size_t full_mac_len = 0;
    if(!compute_full_cmac_segments(sm, segments, 2, full_mac, &full_mac_len) ||
       full_mac_len < DFC_WIRE_MAC_LENGTH) {
        dfc_secure_wipe(full_mac, sizeof(full_mac));
        return false;
    }
    memcpy(mac, full_mac, DFC_WIRE_MAC_LENGTH);
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    return true;
}

size_t dfc_secure_messaging_generate_ev1_response(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity) {
    if(plain_len > SIZE_MAX - DFC_WIRE_MAC_LENGTH ||
       !output_fits(out, out_capacity, plain_len + DFC_WIRE_MAC_LENGTH) || (plain_len && !plain))
        return 0;
    uint8_t mac[DFC_WIRE_MAC_LENGTH];
    if(!dfc_secure_messaging_generate_ev1_response_mac(sm, status, plain, plain_len, mac)) {
        return 0;
    }
    if(plain_len) memmove(out, plain, plain_len);
    memcpy(out + plain_len, mac, sizeof(mac));
    dfc_secure_wipe(mac, sizeof(mac));
    return plain_len + sizeof(mac);
}

size_t dfc_secure_messaging_generate_ev1_response_in_place(
    DfcSecureMessaging* sm,
    uint8_t status,
    uint8_t* buffer,
    size_t plain_len,
    size_t capacity) {
    if(!session_is_valid(sm) || sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY || !buffer ||
       plain_len > capacity || DFC_WIRE_MAC_LENGTH > capacity - plain_len)
        return 0;
    if(!dfc_secure_messaging_generate_ev1_response_mac(
           sm, status, buffer, plain_len, buffer + plain_len)) {
        return 0;
    }
    return plain_len + DFC_WIRE_MAC_LENGTH;
}

size_t dfc_secure_messaging_unwrap_ev1_response(
    DfcSecureMessaging* sm,
    uint8_t status,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity) {
    if(!session_is_valid(sm) || sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ||
       (wrapped_len && !wrapped))
        return SIZE_MAX;
    size_t full_mac_len = sm->cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
    size_t truncated_len = DFC_MIN((size_t)8, full_mac_len);
    if(wrapped_len < truncated_len) return SIZE_MAX;

    size_t plain_len = wrapped_len - truncated_len;
    if(!output_fits(out, out_capacity, plain_len)) return SIZE_MAX;
    DfcMacSegment segments[] = {
        {.data = wrapped, .length = plain_len},
        {.data = &status, .length = 1},
    };
    uint8_t full_mac[16] = {0};
    bool ok = compute_full_cmac_segments(sm, segments, 2, full_mac, &full_mac_len);
    if(!ok) {
        dfc_secure_wipe(full_mac, sizeof(full_mac));
        return SIZE_MAX;
    }
    truncated_len = DFC_MIN((size_t)8, full_mac_len);
    if(!dfc_ct_equal(full_mac, wrapped + plain_len, truncated_len)) {
        DFC_LOG_W(TAG, "EV1 response CMAC mismatch");
        dfc_secure_wipe(full_mac, sizeof(full_mac));
        return SIZE_MAX;
    }

    if(plain_len) memmove(out, wrapped, plain_len);
    update_iv_from_full_cmac(sm, full_mac, full_mac_len);
    dfc_secure_wipe(full_mac, sizeof(full_mac));
    return plain_len;
}

// A legacy (D40) session picks its DES primitive by party, not by direction: the
// PICC encrypts what it sends and encrypts again to recover what it receives,
// while the PCD decrypts in both directions. So a cryptogram travelling to the
// PICC is C[i] = dec(P[i] ^ C[i-1]) and one travelling to the PCD is
// C[i] = enc(P[i] ^ C[i-1]); each party recovers with its own primitive. With
// encrypt = true these two are the ordinary CBC encrypt and decrypt.
// C[i] = prim(P[i] ^ C[i-1])
static bool legacy_cbc_forward(
    const DfcSecureMessaging* sm,
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    static const uint8_t zero_iv[8] = {0};
    if(!sm || len > DFC_SM_MAX_CRYPTO_SIZE) {
        if(out && len <= DFC_SM_MAX_CRYPTO_SIZE) dfc_secure_wipe(out, len);
        return false;
    }
    return dfc_crypto_des_chain(
        encrypt,
        DfcCryptoDesChainForward,
        sm->session_key,
        sm->session_key_len,
        zero_iv,
        in,
        out,
        len);
}

// P[i] = prim(C[i]) ^ C[i-1]
static bool legacy_cbc_reverse(
    const DfcSecureMessaging* sm,
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    static const uint8_t zero_iv[8] = {0};
    if(!sm || len > DFC_SM_MAX_CRYPTO_SIZE) {
        if(out && len <= DFC_SM_MAX_CRYPTO_SIZE) dfc_secure_wipe(out, len);
        return false;
    }
    return dfc_crypto_des_chain(
        encrypt,
        DfcCryptoDesChainReverse,
        sm->session_key,
        sm->session_key_len,
        zero_iv,
        in,
        out,
        len);
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
    bool response_padding,
    uint8_t* out,
    size_t out_capacity) {
    if(!session_is_valid(sm) || (plain_len && !plain) || (crc_prefix_len && !crc_prefix) ||
       (crc_suffix_len && !crc_suffix) || plain_len > DFC_MAX_FILE_DATA) {
        return 0;
    }
    size_t block_size = sm->cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
    size_t crc_len = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? 2 : 4;
    if(plain_len > SIZE_MAX - crc_len) return 0;
    size_t with_crc_len = plain_len + crc_len;
    if(with_crc_len > SIZE_MAX - (block_size - 1)) return 0;
    size_t padded_len = ((with_crc_len + block_size - 1) / block_size) * block_size;
    if(padded_len == 0) padded_len = block_size;
    if(response_padding && padded_len == with_crc_len) {
        if(padded_len > SIZE_MAX - block_size) return 0;
        padded_len += block_size;
    }
    if(padded_len > DFC_SM_MAX_CRYPTO_SIZE) return 0;
    // CBC writes the complete padded message before the clear length is known.
    if(!output_fits(out, out_capacity, padded_len)) return 0;

    uint32_t crc;
    if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        crc = dfc_crc16_iso14443(plain, plain_len);
    } else {
        crc = 0xFFFFFFFF;
        crc = dfc_crc32_extend(crc, crc_prefix, crc_prefix_len);
        crc = dfc_crc32_extend(crc, plain, plain_len);
        crc = dfc_crc32_extend(crc, crc_suffix, crc_suffix_len);
    }

    // The public API permits in-place operation. Compute the checksum before
    // moving a partially overlapping input so it always covers the original bytes.
    memmove(out, plain, plain_len);
    if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        out[plain_len] = crc & 0xFF;
        out[plain_len + 1] = (crc >> 8) & 0xFF;
    } else {
        out[plain_len] = crc & 0xFF;
        out[plain_len + 1] = (crc >> 8) & 0xFF;
        out[plain_len + 2] = (crc >> 16) & 0xFF;
        out[plain_len + 3] = (crc >> 24) & 0xFF;
    }
    if(response_padding) {
        out[with_crc_len] = DFC_SM_LENGTH_MARKER;
        memset(out + with_crc_len + 1, 0, padded_len - with_crc_len - 1);
    } else if(padded_len > with_crc_len) {
        memset(out + with_crc_len, 0, padded_len - with_crc_len);
    }
    // D40 (0x0A) sessions reset the IV to zero for every operation; ISO(0x1A)/AES(0xAA)
    // sessions chain a single shared IV across every operation (hardware-confirmed - see
    // DfcSecureMessaging's iv field comment). The CBC helpers update the passed IV
    // buffer in place. Work on a local IV so failure cannot advance session state.
    uint8_t next_iv[16] = {0};
    if(sm->cipher != DFC_CMD_AUTHENTICATE_LEGACY) {
        memcpy(next_iv, sm->iv, sizeof(next_iv));
    }

    bool ok;
    if(sm->cipher == DFC_CMD_AUTHENTICATE_AES) {
        ok = dfc_worker_aes_cbc_encrypt(
            sm->session_key, sm->session_key_len, next_iv, padded_len, out, out);
    } else if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        ok = legacy_cbc_forward(sm, !sm->pcd, out, padded_len, out);
    } else {
        ok = dfc_worker_des_cbc_encrypt(
            sm->session_key, sm->session_key_len, next_iv, padded_len, out, out);
    }
    if(!ok) {
        dfc_secure_wipe(out, padded_len);
        dfc_secure_wipe(next_iv, sizeof(next_iv));
        return 0;
    }
    if(sm->cipher != DFC_CMD_AUTHENTICATE_LEGACY) memcpy(sm->iv, next_iv, sizeof(sm->iv));
    dfc_secure_wipe(next_iv, sizeof(next_iv));
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
    uint8_t* out,
    size_t out_capacity) {
    if(!session_is_valid(sm) || !wrapped || wrapped_len == 0 ||
       wrapped_len > DFC_SM_MAX_CRYPTO_SIZE || (crc_prefix_len && !crc_prefix) ||
       (crc_suffix_len && !crc_suffix) || !output_fits(out, out_capacity, wrapped_len)) {
        return SIZE_MAX;
    }
    size_t block_size = sm->cipher == DFC_CMD_AUTHENTICATE_AES ? DFC_AES_KEY_LENGTH :
                                                                 DFC_SM_LEGACY_BLOCK_SIZE;
    if(wrapped_len % block_size != 0) {
        dfc_secure_wipe(out, wrapped_len);
        return SIZE_MAX;
    }
    uint8_t next_iv[16] = {0};
    if(sm->cipher != DFC_CMD_AUTHENTICATE_LEGACY) {
        memcpy(next_iv, sm->iv, sizeof(next_iv));
    }

    bool ok;
    if(sm->cipher == DFC_CMD_AUTHENTICATE_AES) {
        ok = dfc_worker_aes_cbc_decrypt(
            sm->session_key, sm->session_key_len, next_iv, wrapped_len, wrapped, out);
    } else if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        ok = legacy_cbc_reverse(sm, !sm->pcd, wrapped, wrapped_len, out);
    } else {
        ok = dfc_worker_des_cbc_decrypt(
            sm->session_key, sm->session_key_len, next_iv, wrapped_len, wrapped, out);
    }
    if(!ok) {
        dfc_secure_wipe(out, wrapped_len);
        dfc_secure_wipe(next_iv, sizeof(next_iv));
        return SIZE_MAX;
    }

    size_t crc_len = sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY ? 2 : 4;
    if(wrapped_len < crc_len) goto failure;

    size_t data_len = SIZE_MAX;
    size_t first_candidate = first_padded_clear_length(out, wrapped_len, crc_len);
    if(sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
        // Where the payload ends is not on the wire, so try each boundary whose
        // tail is padding and keep the first whose CRC16 agrees. Accepting a
        // boundary without checking the CRC would admit any ciphertext at all.
        uint16_t expected_crc = dfc_crc16_iso14443_extend(0x6363, out, first_candidate);
        for(size_t candidate = first_candidate; candidate + crc_len <= wrapped_len; candidate++) {
            size_t crc_offset = candidate;
            uint16_t actual_crc = (uint16_t)out[crc_offset] |
                                  (uint16_t)((uint16_t)out[crc_offset + 1] << 8);
            if(actual_crc == expected_crc) {
                data_len = candidate;
                break;
            }
            expected_crc = dfc_crc16_iso14443_extend(expected_crc, &out[candidate], 1);
        }
        if(data_len == SIZE_MAX) goto failure;
    } else {
        uint32_t running_crc = 0xFFFFFFFF;
        running_crc = dfc_crc32_extend(running_crc, crc_prefix, crc_prefix_len);
        running_crc = dfc_crc32_extend(running_crc, out, first_candidate);
        for(size_t clear_len = first_candidate; clear_len <= wrapped_len - crc_len; clear_len++) {
            size_t crc_offset = clear_len;
            uint32_t expected_crc = running_crc;
            expected_crc = dfc_crc32_extend(expected_crc, crc_suffix, crc_suffix_len);

            uint32_t actual_crc =
                (uint32_t)out[crc_offset] | ((uint32_t)out[crc_offset + 1] << 8) |
                ((uint32_t)out[crc_offset + 2] << 16) | ((uint32_t)out[crc_offset + 3] << 24);
            if(actual_crc == expected_crc) {
                data_len = clear_len;
                break;
            }
            running_crc = dfc_crc32_extend(running_crc, &out[clear_len], 1);
        }
        if(data_len == SIZE_MAX) goto failure;
    }

    if(sm->cipher != DFC_CMD_AUTHENTICATE_LEGACY) memcpy(sm->iv, next_iv, sizeof(sm->iv));
    if(data_len < wrapped_len) dfc_secure_wipe(out + data_len, wrapped_len - data_len);
    dfc_secure_wipe(next_iv, sizeof(next_iv));
    return data_len;
failure:
    dfc_secure_wipe(out, wrapped_len);
    dfc_secure_wipe(next_iv, sizeof(next_iv));
    return SIZE_MAX;
}

static bool d40_session_is_valid(DfcSecureMessaging* sm) {
    return session_is_valid(sm) && sm->cipher == DFC_CMD_AUTHENTICATE_LEGACY;
}

size_t dfc_secure_messaging_wrap_d40_command_mac(
    DfcSecureMessaging* sm,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity) {
    if(!d40_session_is_valid(sm) || (plain_len && !plain) ||
       plain_len > SIZE_MAX - DFC_SM_LEGACY_MAC_LENGTH ||
       !output_fits(out, out_capacity, plain_len + DFC_SM_LEGACY_MAC_LENGTH)) {
        return 0;
    }
    DfcMacSegment segment = {.data = plain, .length = plain_len};
    uint8_t mac[DFC_SM_LEGACY_MAC_LENGTH] = {0};
    if(!d40_mac_segments(sm, &segment, 1, mac)) {
        dfc_secure_wipe(mac, sizeof(mac));
        return 0;
    }
    if(plain_len) memmove(out, plain, plain_len);
    memcpy(out + plain_len, mac, sizeof(mac));
    dfc_secure_wipe(mac, sizeof(mac));
    return plain_len + DFC_SM_LEGACY_MAC_LENGTH;
}

static bool d40_mac_verify(
    DfcSecureMessaging* sm,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len) {
    if(out_len) *out_len = 0;
    if(!d40_session_is_valid(sm) || (wrapped_len && !wrapped) ||
       wrapped_len < DFC_SM_LEGACY_MAC_LENGTH || !out_len) {
        return false;
    }
    size_t data_len = wrapped_len - DFC_SM_LEGACY_MAC_LENGTH;
    if(!output_fits(out, out_capacity, data_len)) return false;
    DfcMacSegment segment = {.data = wrapped, .length = data_len};
    uint8_t mac[DFC_SM_LEGACY_MAC_LENGTH] = {0};
    bool ok = d40_mac_segments(sm, &segment, 1, mac) &&
              dfc_ct_equal(mac, wrapped + data_len, sizeof(mac));
    dfc_secure_wipe(mac, sizeof(mac));
    if(!ok) return false;
    if(data_len) memmove(out, wrapped, data_len);
    *out_len = data_len;
    return true;
}

bool dfc_secure_messaging_verify_d40_command_mac(
    DfcSecureMessaging* sm,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len) {
    return d40_mac_verify(sm, wrapped, wrapped_len, out, out_capacity, out_len);
}

size_t dfc_secure_messaging_generate_d40_response_mac(
    DfcSecureMessaging* sm,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity) {
    return dfc_secure_messaging_wrap_d40_command_mac(sm, plain, plain_len, out, out_capacity);
}

bool dfc_secure_messaging_unwrap_d40_response_mac(
    DfcSecureMessaging* sm,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len) {
    return d40_mac_verify(sm, wrapped, wrapped_len, out, out_capacity, out_len);
}

// PCD -> PICC direction for plain and enciphered command payloads.
size_t dfc_secure_messaging_wrap(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity) {
    if(!session_is_valid(sm) || (header_len && !header) || (plain_len && !plain)) return 0;
    if(comm_mode == DFC_COMM_PLAIN) {
        if(!output_fits(out, out_capacity, plain_len)) return 0;
        if(plain_len) memmove(out, plain, plain_len);
        return plain_len;
    }

    if(comm_mode == DFC_COMM_MAC) return 0;

    if(comm_mode != DFC_COMM_ENCIPHERED) return 0;
    return enciphered_encode(
        sm, header, header_len, plain, plain_len, NULL, 0, false, out, out_capacity);
}

// PICC -> PCD direction for plain and enciphered response payloads.
bool dfc_secure_messaging_unwrap(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* wrapped,
    size_t wrapped_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len) {
    if(out_len) *out_len = 0;
    if(!session_is_valid(sm) || !out_len || (wrapped_len && !wrapped)) return false;
    if(comm_mode == DFC_COMM_PLAIN) {
        if(!output_fits(out, out_capacity, wrapped_len)) return false;
        if(wrapped_len) memmove(out, wrapped, wrapped_len);
        *out_len = wrapped_len;
        return true;
    }

    if(comm_mode == DFC_COMM_MAC) return false;

    if(comm_mode != DFC_COMM_ENCIPHERED) return false;
    size_t clear_len =
        enciphered_decode(sm, NULL, 0, &status, 1, wrapped, wrapped_len, out, out_capacity);
    if(clear_len == SIZE_MAX) return false;
    *out_len = clear_len;
    return true;
}

// PICC -> PCD direction for plain and enciphered response payloads.
size_t dfc_secure_messaging_generate_response_with_length_marker(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity,
    bool length_unknown) {
    if(!session_is_valid(sm) || (plain_len && !plain)) return 0;
    if(comm_mode == DFC_COMM_PLAIN) {
        if(!output_fits(out, out_capacity, plain_len)) return 0;
        if(plain_len) memmove(out, plain, plain_len);
        return plain_len;
    }

    if(comm_mode == DFC_COMM_MAC) return 0;

    if(comm_mode != DFC_COMM_ENCIPHERED) return 0;
    return enciphered_encode(
        sm, NULL, 0, plain, plain_len, &status, 1, length_unknown, out, out_capacity);
}

size_t dfc_secure_messaging_generate_response(
    DfcSecureMessaging* sm,
    uint8_t comm_mode,
    uint8_t status,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* out,
    size_t out_capacity) {
    return dfc_secure_messaging_generate_response_with_length_marker(
        sm, comm_mode, status, plain, plain_len, out, out_capacity, false);
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
    uint8_t* out,
    size_t out_capacity) {
    if(!session_is_valid(sm) || (header_len && !header) || (wrapped_len && !wrapped)) return 0;
    if(comm_mode == DFC_COMM_PLAIN) {
        if(!output_fits(out, out_capacity, wrapped_len)) return 0;
        if(wrapped_len) memmove(out, wrapped, wrapped_len);
        return wrapped_len;
    }

    if(comm_mode == DFC_COMM_MAC) return 0;

    if(comm_mode != DFC_COMM_ENCIPHERED) return 0;
    size_t clear_len = enciphered_decode(
        sm, header, header_len, NULL, 0, wrapped, wrapped_len, out, out_capacity);
    return clear_len == SIZE_MAX ? 0 : clear_len;
}

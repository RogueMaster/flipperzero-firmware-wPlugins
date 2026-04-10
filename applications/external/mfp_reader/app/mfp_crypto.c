#include "mfp_crypto.h"
#include "lib/aes/aes.h"
#include <stdbool.h>
#include <string.h>

void mfp_crypto_ecb_encrypt(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t input[MFP_AES_BLOCK_SIZE],
    uint8_t output[MFP_AES_BLOCK_SIZE]) {
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key);
    memcpy(output, input, MFP_AES_BLOCK_SIZE);
    AES_ECB_encrypt(&ctx, output);
}

void mfp_crypto_ecb_decrypt(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t input[MFP_AES_BLOCK_SIZE],
    uint8_t output[MFP_AES_BLOCK_SIZE]) {
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key);
    memcpy(output, input, MFP_AES_BLOCK_SIZE);
    AES_ECB_decrypt(&ctx, output);
}

void mfp_crypto_cbc_encrypt(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t iv[MFP_AES_BLOCK_SIZE],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    memcpy(output, input, length);
    AES_CBC_encrypt_buffer(&ctx, output, length);
}

void mfp_crypto_cbc_decrypt(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t iv[MFP_AES_BLOCK_SIZE],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    memcpy(output, input, length);
    AES_CBC_decrypt_buffer(&ctx, output, length);
}

/* ---- CMAC (RFC 4493) ---- */

static void cmac_shift_left(
    const uint8_t in[MFP_AES_BLOCK_SIZE],
    uint8_t out[MFP_AES_BLOCK_SIZE]) {
    uint8_t carry = 0;
    for(int i = MFP_AES_BLOCK_SIZE - 1; i >= 0; i--) {
        out[i] = (uint8_t)((in[i] << 1) | carry);
        carry = (in[i] >> 7) & 1;
    }
}

static void cmac_generate_subkeys(
    const uint8_t key[MFP_AES_KEY_SIZE],
    uint8_t k1[MFP_AES_BLOCK_SIZE],
    uint8_t k2[MFP_AES_BLOCK_SIZE]) {
    const uint8_t rb = 0x87;
    uint8_t l[MFP_AES_BLOCK_SIZE];
    memset(l, 0, MFP_AES_BLOCK_SIZE);
    mfp_crypto_ecb_encrypt(key, l, l);

    cmac_shift_left(l, k1);
    if(l[0] & 0x80) k1[MFP_AES_BLOCK_SIZE - 1] ^= rb;

    cmac_shift_left(k1, k2);
    if(k1[0] & 0x80) k2[MFP_AES_BLOCK_SIZE - 1] ^= rb;
}

void mfp_crypto_cmac(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t* data,
    size_t length,
    uint8_t mac[MFP_AES_BLOCK_SIZE]) {
    uint8_t k1[MFP_AES_BLOCK_SIZE], k2[MFP_AES_BLOCK_SIZE];
    cmac_generate_subkeys(key, k1, k2);

    size_t n = (length + MFP_AES_BLOCK_SIZE - 1) / MFP_AES_BLOCK_SIZE;
    if(n == 0) n = 1;

    bool last_complete = (length > 0) && (length % MFP_AES_BLOCK_SIZE == 0);

    uint8_t x[MFP_AES_BLOCK_SIZE];
    memset(x, 0, MFP_AES_BLOCK_SIZE);

    for(size_t i = 0; i < n - 1; i++) {
        uint8_t y[MFP_AES_BLOCK_SIZE];
        for(size_t j = 0; j < MFP_AES_BLOCK_SIZE; j++)
            y[j] = x[j] ^ data[i * MFP_AES_BLOCK_SIZE + j];
        mfp_crypto_ecb_encrypt(key, y, x);
    }

    uint8_t last[MFP_AES_BLOCK_SIZE];
    size_t offset = (n - 1) * MFP_AES_BLOCK_SIZE;
    if(last_complete) {
        for(size_t j = 0; j < MFP_AES_BLOCK_SIZE; j++)
            last[j] = data[offset + j] ^ k1[j];
    } else {
        memset(last, 0, MFP_AES_BLOCK_SIZE);
        size_t rem = length - offset;
        memcpy(last, &data[offset], rem);
        last[rem] = 0x80;
        for(size_t j = 0; j < MFP_AES_BLOCK_SIZE; j++)
            last[j] ^= k2[j];
    }

    uint8_t y[MFP_AES_BLOCK_SIZE];
    for(size_t j = 0; j < MFP_AES_BLOCK_SIZE; j++)
        y[j] = x[j] ^ last[j];
    mfp_crypto_ecb_encrypt(key, y, mac);
}

void mfp_crypto_cmac8(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t* data,
    size_t length,
    uint8_t mac[MFP_MAC_SIZE]) {
    uint8_t full[MFP_AES_BLOCK_SIZE];
    mfp_crypto_cmac(key, data, length, full);
    for(size_t i = 0; i < MFP_MAC_SIZE; i++)
        mac[i] = full[i * 2 + 1];
}

/* ---- MFP session key derivation (NXP AN10922) ---- */

void mfp_crypto_derive_session_keys(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t rnd_a[MFP_AES_BLOCK_SIZE],
    const uint8_t rnd_b[MFP_AES_BLOCK_SIZE],
    uint8_t k_enc[MFP_AES_KEY_SIZE],
    uint8_t k_mac[MFP_AES_KEY_SIZE]) {
    uint8_t sv[MFP_AES_BLOCK_SIZE];

    /* Kenc = AES(key, RndA[11..15] || RndB[11..15] || (RndA[4..8] XOR RndB[4..8]) || 0x11) */
    memcpy(&sv[0], &rnd_a[11], 5);
    memcpy(&sv[5], &rnd_b[11], 5);
    for(int i = 0; i < 5; i++) sv[10 + i] = rnd_a[4 + i] ^ rnd_b[4 + i];
    sv[15] = 0x11;
    mfp_crypto_ecb_encrypt(key, sv, k_enc);

    /* Kmac = AES(key, RndA[7..11] || RndB[7..11] || (RndA[0..4] XOR RndB[0..4]) || 0x22) */
    memcpy(&sv[0], &rnd_a[7], 5);
    memcpy(&sv[5], &rnd_b[7], 5);
    for(int i = 0; i < 5; i++) sv[10 + i] = rnd_a[i] ^ rnd_b[i];
    sv[15] = 0x22;
    mfp_crypto_ecb_encrypt(key, sv, k_mac);
}

void mfp_crypto_calculate_mac(
    const uint8_t k_mac[MFP_AES_KEY_SIZE],
    uint8_t cmd,
    uint16_t counter,
    const uint8_t ti[4],
    const uint8_t* data,
    size_t data_length,
    uint8_t mac[MFP_MAC_SIZE]) {
    uint8_t buf[7 + 256];
    size_t n = 0;
    buf[n++] = cmd;
    buf[n++] = (uint8_t)(counter & 0xFF);
    buf[n++] = (uint8_t)((counter >> 8) & 0xFF);
    memcpy(&buf[n], ti, 4); n += 4;
    if(data && data_length > 0) {
        size_t copy = data_length > 256 ? 256 : data_length;
        memcpy(&buf[n], data, copy);
        n += copy;
    }
    mfp_crypto_cmac8(k_mac, buf, n, mac);
}

void mfp_crypto_build_read_iv(
    const uint8_t ti[4],
    uint16_t r_ctr,
    uint16_t w_ctr,
    uint8_t iv[MFP_AES_BLOCK_SIZE]) {
    uint8_t ctr[4] = {
        (uint8_t)(r_ctr & 0xFF),
        (uint8_t)((r_ctr >> 8) & 0xFF),
        (uint8_t)(w_ctr & 0xFF),
        (uint8_t)((w_ctr >> 8) & 0xFF),
    };
    for(int i = 0; i < 3; i++) {
        memcpy(&iv[i * 4], ctr, sizeof(ctr));
    }
    memcpy(&iv[12], ti, 4);
}

void mfp_crypto_build_write_iv(
    const uint8_t ti[4],
    uint16_t r_ctr,
    uint16_t w_ctr,
    uint8_t iv[MFP_AES_BLOCK_SIZE]) {
    memcpy(&iv[0], ti, 4);
    for(int i = 0; i < 3; i++) {
        size_t off = 4 + i * 4;
        iv[off + 0] = (uint8_t)(r_ctr & 0xFF);
        iv[off + 1] = (uint8_t)((r_ctr >> 8) & 0xFF);
        iv[off + 2] = (uint8_t)(w_ctr & 0xFF);
        iv[off + 3] = (uint8_t)((w_ctr >> 8) & 0xFF);
    }
}

void mfp_crypto_rotate_left(
    const uint8_t input[MFP_AES_BLOCK_SIZE],
    uint8_t output[MFP_AES_BLOCK_SIZE]) {
    memcpy(output, &input[1], MFP_AES_BLOCK_SIZE - 1);
    output[MFP_AES_BLOCK_SIZE - 1] = input[0];
}

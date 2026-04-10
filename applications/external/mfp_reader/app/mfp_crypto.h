#pragma once

#include <stdint.h>
#include <stddef.h>

#define MFP_AES_BLOCK_SIZE 16
#define MFP_AES_KEY_SIZE   16
#define MFP_MAC_SIZE       8

/** AES-128 ECB encrypt/decrypt (single block, in-place output). */
void mfp_crypto_ecb_encrypt(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t input[MFP_AES_BLOCK_SIZE],
    uint8_t output[MFP_AES_BLOCK_SIZE]);

void mfp_crypto_ecb_decrypt(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t input[MFP_AES_BLOCK_SIZE],
    uint8_t output[MFP_AES_BLOCK_SIZE]);

/** AES-128 CBC encrypt/decrypt. length must be multiple of 16. */
void mfp_crypto_cbc_encrypt(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t iv[MFP_AES_BLOCK_SIZE],
    const uint8_t* input,
    uint8_t* output,
    size_t length);

void mfp_crypto_cbc_decrypt(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t iv[MFP_AES_BLOCK_SIZE],
    const uint8_t* input,
    uint8_t* output,
    size_t length);

/** AES-CMAC per RFC 4493. mac = 16 bytes. */
void mfp_crypto_cmac(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t* data,
    size_t length,
    uint8_t mac[MFP_AES_BLOCK_SIZE]);

/** Truncated CMAC: 8 bytes (odd-indexed bytes of full MAC). */
void mfp_crypto_cmac8(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t* data,
    size_t length,
    uint8_t mac[MFP_MAC_SIZE]);

/**
 * Derive Kenc and Kmac session keys from master key + RndA + RndB.
 * Per NXP AN10922 for MIFARE Plus SL3.
 */
void mfp_crypto_derive_session_keys(
    const uint8_t key[MFP_AES_KEY_SIZE],
    const uint8_t rnd_a[MFP_AES_BLOCK_SIZE],
    const uint8_t rnd_b[MFP_AES_BLOCK_SIZE],
    uint8_t k_enc[MFP_AES_KEY_SIZE],
    uint8_t k_mac[MFP_AES_KEY_SIZE]);

/**
 * Calculate 8-byte MAC over: cmd(1) || ctr_lo(1) || ctr_hi(1) || TI(4) || data(N).
 */
void mfp_crypto_calculate_mac(
    const uint8_t k_mac[MFP_AES_KEY_SIZE],
    uint8_t cmd,
    uint16_t counter,
    const uint8_t ti[4],
    const uint8_t* data,
    size_t data_length,
    uint8_t mac[MFP_MAC_SIZE]);

/** Build IV for encrypted read response: [R_CTR || W_CTR] x3 || TI(4). */
void mfp_crypto_build_read_iv(
    const uint8_t ti[4],
    uint16_t r_ctr,
    uint16_t w_ctr,
    uint8_t iv[MFP_AES_BLOCK_SIZE]);

/** Build IV for encrypted write command: TI(4) || [R_CTR || W_CTR] x3. */
void mfp_crypto_build_write_iv(
    const uint8_t ti[4],
    uint16_t r_ctr,
    uint16_t w_ctr,
    uint8_t iv[MFP_AES_BLOCK_SIZE]);

/** Rotate 16-byte block left by 1 byte. */
void mfp_crypto_rotate_left(
    const uint8_t input[MFP_AES_BLOCK_SIZE],
    uint8_t output[MFP_AES_BLOCK_SIZE]);

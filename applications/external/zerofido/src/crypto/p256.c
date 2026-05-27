/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "p256.h"

#include <furi_hal_random.h>
#include <string.h>

#include "micro_ecc/uECC.h"

static const uint8_t zf_p256_order[ZF_PRIVATE_KEY_LEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84, 0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51,
};

static void zf_p256_secure_zero(void *data, size_t size) {
    volatile uint8_t *ptr = data;

    if (!ptr) {
        return;
    }

    while (size-- > 0U) {
        *ptr++ = 0;
    }
}

static int zf_p256_random_cb(uint8_t *output, unsigned output_len) {
    furi_hal_random_fill_buf(output, output_len);
    return 1;
}

static uECC_Curve zf_p256_curve(void) {
    return uECC_secp256r1();
}

static void zf_p256_prepare_rng(void) {
    uECC_set_rng(zf_p256_random_cb);
}

static void zf_p256_public_from_xy(const uint8_t x[ZF_PUBLIC_KEY_LEN],
                                   const uint8_t y[ZF_PUBLIC_KEY_LEN],
                                   uint8_t public_key[ZF_PUBLIC_KEY_LEN * 2U]) {
    memcpy(public_key, x, ZF_PUBLIC_KEY_LEN);
    memcpy(public_key + ZF_PUBLIC_KEY_LEN, y, ZF_PUBLIC_KEY_LEN);
}

static void zf_p256_public_to_xy(const uint8_t public_key[ZF_PUBLIC_KEY_LEN * 2U],
                                 uint8_t x[ZF_PUBLIC_KEY_LEN], uint8_t y[ZF_PUBLIC_KEY_LEN]) {
    memcpy(x, public_key, ZF_PUBLIC_KEY_LEN);
    memcpy(y, public_key + ZF_PUBLIC_KEY_LEN, ZF_PUBLIC_KEY_LEN);
}

bool zf_p256_generate_keypair(uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                              uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                              uint8_t public_y[ZF_PUBLIC_KEY_LEN]) {
    uint8_t public_key[ZF_PUBLIC_KEY_LEN * 2U];
    bool ok = false;

    if (!private_key || !public_x || !public_y) {
        return false;
    }

    zf_p256_prepare_rng();
    ok = uECC_make_key(public_key, private_key, zf_p256_curve()) == 1;
    if (ok) {
        zf_p256_public_to_xy(public_key, public_x, public_y);
    }
    zf_p256_secure_zero(public_key, sizeof(public_key));
    return ok;
}

bool zf_p256_private_key_valid(const uint8_t private_key[ZF_PRIVATE_KEY_LEN]) {
    bool all_zero = true;

    if (!private_key) {
        return false;
    }
    for (size_t i = 0; i < ZF_PRIVATE_KEY_LEN; ++i) {
        all_zero = all_zero && private_key[i] == 0;
    }
    if (all_zero) {
        return false;
    }
    return memcmp(private_key, zf_p256_order, ZF_PRIVATE_KEY_LEN) < 0;
}

bool zf_p256_public_key_valid(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                              const uint8_t public_y[ZF_PUBLIC_KEY_LEN]) {
    uint8_t public_key[ZF_PUBLIC_KEY_LEN * 2U];
    bool ok = false;

    if (!public_x || !public_y) {
        return false;
    }
    zf_p256_public_from_xy(public_x, public_y, public_key);
    ok = uECC_valid_public_key(public_key, zf_p256_curve()) == 1;
    zf_p256_secure_zero(public_key, sizeof(public_key));
    return ok;
}

bool zf_p256_compute_public_key(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                uint8_t public_y[ZF_PUBLIC_KEY_LEN]) {
    uint8_t public_key[ZF_PUBLIC_KEY_LEN * 2U];
    bool ok = false;

    if (!private_key || !public_x || !public_y || !zf_p256_private_key_valid(private_key)) {
        return false;
    }
    ok = uECC_compute_public_key(private_key, public_key, zf_p256_curve()) == 1;
    if (ok) {
        zf_p256_public_to_xy(public_key, public_x, public_y);
    }
    zf_p256_secure_zero(public_key, sizeof(public_key));
    return ok;
}

bool zf_p256_ecdh_raw_secret(const ZfP256KeyAgreementKey *key,
                             const uint8_t peer_x[ZF_PUBLIC_KEY_LEN],
                             const uint8_t peer_y[ZF_PUBLIC_KEY_LEN], uint8_t out[32]) {
    uint8_t peer_public_key[ZF_PUBLIC_KEY_LEN * 2U];
    bool ok = false;

    if (!key || !peer_x || !peer_y || !out || !zf_p256_private_key_valid(key->private_key)) {
        return false;
    }
    zf_p256_public_from_xy(peer_x, peer_y, peer_public_key);
    zf_p256_prepare_rng();
    ok = uECC_valid_public_key(peer_public_key, zf_p256_curve()) == 1 &&
         uECC_shared_secret(peer_public_key, key->private_key, out, zf_p256_curve()) == 1;
    zf_p256_secure_zero(peer_public_key, sizeof(peer_public_key));
    return ok;
}

bool zf_p256_sign_hash_raw(const uint8_t private_key[ZF_PRIVATE_KEY_LEN], const uint8_t hash[32],
                           uint8_t out[ZF_PUBLIC_KEY_LEN * 2U]) {
    if (!private_key || !hash || !out || !zf_p256_private_key_valid(private_key)) {
        return false;
    }
    zf_p256_prepare_rng();
    if (uECC_sign(private_key, hash, 32, out, zf_p256_curve()) != 1) {
        zf_p256_secure_zero(out, ZF_PUBLIC_KEY_LEN * 2U);
        return false;
    }
    return true;
}

bool zf_p256_verify_hash_raw(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                             const uint8_t public_y[ZF_PUBLIC_KEY_LEN], const uint8_t hash[32],
                             const uint8_t raw_signature[ZF_PUBLIC_KEY_LEN * 2U]) {
    uint8_t public_key[ZF_PUBLIC_KEY_LEN * 2U];
    bool ok = false;

    if (!public_x || !public_y || !hash || !raw_signature) {
        return false;
    }
    zf_p256_public_from_xy(public_x, public_y, public_key);
    ok = uECC_valid_public_key(public_key, zf_p256_curve()) == 1 &&
         uECC_verify(public_key, hash, 32, raw_signature, zf_p256_curve()) == 1;
    zf_p256_secure_zero(public_key, sizeof(public_key));
    return ok;
}

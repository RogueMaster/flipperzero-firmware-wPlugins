/*
 * BioVault PIN KDF — turns a PIN/passphrase into 32 bytes of key material.
 * PBKDF2-HMAC-SHA256 (software) then an enclave-iterated AES chain, so every
 * brute-force guess costs `hw_iters` sequential ops on THIS device's enclave:
 * the stretch can never be offloaded to faster hardware.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BV_PIN_SALT_SIZE 16
#define BV_PIN_KEY_SIZE 32
#define BV_PIN_LEN 6 // exactly six digits
// Defaults for new keystores; stored per-keystore so they stay tunable.
#define BV_PIN_SW_ITERS 4096
#define BV_PIN_HW_ITERS 16384

// Derive the unlock key. ~0.5s by design (the enclave chain dominates).
bool bv_pin_derive(
    const char* pin,
    const uint8_t salt[BV_PIN_SALT_SIZE],
    uint32_t sw_iters,
    uint32_t hw_iters,
    uint8_t out[BV_PIN_KEY_SIZE]);

// PBKDF2 known-answer test + determinism check of the full derivation.
bool bv_pin_selftest(void);

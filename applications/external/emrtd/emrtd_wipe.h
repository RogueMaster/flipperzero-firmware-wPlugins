/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Erasing a secret so that it stays erased.
 *
 * A plain memset() over a buffer that is never read again is a dead store, and
 * the optimiser is entitled to delete it. It does: built with the -Os that
 * ufbt passes to every FAP, arm-none-eabi-gcc drops the memset() that used to
 * clear the key derivation digest, and the same happens to the MRZ password,
 * Kseed, the session keys and the per-APDU plaintext. The wipes that survive
 * do so only because the allocation is in another translation unit, which is
 * an accident that -flto would take away.
 *
 * mbedtls_platform_zeroize() exists for exactly this reason and the firmware
 * exports it to applications - it is in the API symbol table alongside the
 * rest of mbed TLS - so the fix costs nothing but the call.
 *
 * Use it for key material, for the credentials, and for anything derived from
 * either. An ordinary buffer being reset is still memset()'s job.
 */
#pragma once

#include <stddef.h>

#include <mbedtls/platform_util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Overwrite a buffer with zeroes in a way the compiler may not remove.
 *
 * @param data  the buffer, which may be NULL only when len is zero
 * @param len   its size in bytes
 */
static inline void emrtd_secure_wipe(void* data, size_t len) {
    if(len == 0) {
        return;
    }
    mbedtls_platform_zeroize(data, len);
}

/** The same, for a whole object: emrtd_secure_wipe_object(&session). */
#define emrtd_secure_wipe_object(ptr) emrtd_secure_wipe((ptr), sizeof(*(ptr)))

#ifdef __cplusplus
}
#endif

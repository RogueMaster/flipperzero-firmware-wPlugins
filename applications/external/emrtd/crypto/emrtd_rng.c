/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_rng.h"

#ifdef EMRTD_HOST_BUILD

#include <stdlib.h>

void emrtd_random_fill(uint8_t* out, size_t len) {
    arc4random_buf(out, len);
}

#else

#include <furi_hal_random.h>

void emrtd_random_fill(uint8_t* out, size_t len) {
    furi_hal_random_fill_buf(out, (uint32_t)len);
}

#endif

int emrtd_random_mbedtls(void* ctx, unsigned char* out, size_t len) {
    (void)ctx;
    emrtd_random_fill((uint8_t*)out, len);
    return 0;
}

/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Random bytes for the ephemeral values of BAC and PACE.
 *
 * On the Flipper this is the hardware RNG. The host test build uses the
 * operating system generator, and the vector tests inject fixed values
 * instead so that they stay reproducible.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Fill @p len bytes of @p out. */
void emrtd_random_fill(uint8_t* out, size_t len);

/**
 * mbed TLS shaped wrapper.
 *
 * mbedtls_ecp_mul() refuses a NULL generator because it randomizes its
 * intermediate results, so this is passed to every scalar multiplication.
 *
 * @return always 0
 */
int emrtd_random_mbedtls(void* ctx, unsigned char* out, size_t len);

#ifdef __cplusplus
}
#endif

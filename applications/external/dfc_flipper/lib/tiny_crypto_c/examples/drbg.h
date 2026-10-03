/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Example: derive session keys from an SP 800-90A HMAC_DRBG seeded by a
 * platform entropy source. */
#ifndef EXAMPLE_DRBG_H_
#define EXAMPLE_DRBG_H_
#include <tiny_crypto/drbg.h>

/* Application-owned generator. Place it in static or long-lived storage. */
typedef struct {
  TC_DRBG drbg;
} ExampleRandom;

/* Instantiate HMAC_DRBG with SHA-256 at 256-bit strength. entropy must be
 * the platform's conditioned entropy source, and device_id personalizes the
 * instance. The nonce is drawn from the entropy source. */
TC_DRBG_result example_random_start(ExampleRandom* random, TC_random_source entropy,
                                    TC_bytes device_id);

/* Write a 32-byte session key. The session label is additional input, which
 * separates keys for different purposes. */
TC_DRBG_result example_random_session_key(ExampleRandom* random, TC_bytes label, uint8_t key[32]);

/* Reseed after a platform event, such as wake from sleep. */
TC_DRBG_result example_random_refresh(ExampleRandom* random);

/* Wipe the generator. */
void example_random_stop(ExampleRandom* random);
#endif

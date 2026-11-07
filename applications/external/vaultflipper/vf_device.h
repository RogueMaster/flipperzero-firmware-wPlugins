// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Derive the device secret from the crypto enclave's unique key slot.
 * The key never leaves the enclave; only the AES encryption of a fixed
 * block is used. */
bool vf_device_secret(uint8_t out[32]);

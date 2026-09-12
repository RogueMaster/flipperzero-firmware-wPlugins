// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// PIN hashing helpers.
//
// The PIN is never stored in clear text. We keep only a salted hash and the
// salt; verification recomputes the hash and compares.
//
// NOTE: FNV-1a is a fast non-cryptographic hash. It is enough to avoid storing
// the PIN in clear text on the SD card and to gate the on-device UI, but it is
// NOT a strong defense against an attacker with physical access to the card
// and time to brute-force 4 digits offline. This is a deliberate, documented
// trade-off for a self-contained device app.
// =============================================================================

#include <furi.h>

// Compute the salted hash of a PIN string.
uint32_t tc_pin_hash(const char* pin, uint32_t salt);

// Generate a random salt.
uint32_t tc_pin_make_salt(void);

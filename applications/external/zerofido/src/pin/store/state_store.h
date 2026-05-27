/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>

#include <storage/storage.h>

#include "../../zerofido_pin.h"

typedef enum {
    ZfPinLoadMissing = 0,
    ZfPinLoadOk,
    ZfPinLoadInvalid,
} ZfPinLoadStatus;

/*
 * Persistent PIN state store. It stores the PIN hash, retry counters, and
 * fail-closed poison state separately from transient PIN token metadata.
 */
void zf_pin_state_store_cleanup_temp(Storage *storage);
ZfPinLoadStatus zf_pin_state_store_load(Storage *storage, uint8_t pin_hash[ZF_PIN_HASH_LEN],
                                        uint8_t *pin_retries, uint8_t *pin_consecutive_mismatches,
                                        bool *pin_auth_blocked);
bool zf_pin_state_store_persist(Storage *storage, const ZfClientPinState *state);
bool zf_pin_state_store_fail_closed(Storage *storage, const ZfClientPinState *state);
bool zf_pin_state_store_clear(Storage *storage);

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

#include <stdint.h>

#include "../../zerofido_types.h"

/*
 * v2 path intentionally ignores pre-overhaul development PIN files. Several NFC
 * bring-up builds could persist a ClientPIN hash before the transport/session
 * ownership was stable; reusing that state makes readers report "incorrect PIN"
 * without ever issuing setPIN again.
 */
#define ZF_PIN_FILE_PATH ZF_APP_DATA_DIR "/client_pin_v2.bin"
#define ZF_PIN_FILE_TEMP_PATH ZF_APP_DATA_DIR "/client_pin_v2.tmp"
#define ZF_PIN_FILE_MAGIC 0x50494E31UL
#define ZF_PIN_FILE_VERSION 1U
#define ZF_PIN_FILE_FLAG_AUTH_BLOCKED 0x01U
#define ZF_PIN_RETRY_SEAL_MAGIC 0x504E5231UL
#define ZF_PIN_RETRY_SEAL_SIZE 32U

/*
 * Persisted PIN file format. The PIN hash is encrypted with the device unique
 * enclave key; retry/auth-block state is additionally sealed to the PIN hash so
 * file rollback or counter splicing is detected on load.
 */
typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t pin_retries;
    uint8_t pin_consecutive_mismatches;
    uint8_t flags;
    uint8_t iv[ZF_WRAP_IV_LEN];
    uint8_t encrypted_pin_hash[ZF_PIN_HASH_LEN];
} ZfPinFileRecordBase;

typedef struct {
    uint32_t magic;
    uint8_t pin_retries;
    uint8_t pin_consecutive_mismatches;
    uint8_t flags;
    uint8_t reserved;
    uint8_t digest[24];
} ZfPinRetrySeal;

typedef struct {
    ZfPinFileRecordBase base;
    uint8_t retry_seal_iv[ZF_WRAP_IV_LEN];
    uint8_t encrypted_retry_seal[ZF_PIN_RETRY_SEAL_SIZE];
} ZfPinFileRecord;

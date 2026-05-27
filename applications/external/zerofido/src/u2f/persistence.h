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

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include <storage/storage.h>

/*
 * U2F persistence manages three independent assets under the U2F subtree:
 * attestation cert/key, device key, and monotonic counter. Load routines return
 * false for missing or invalid files so u2f_init can distinguish bootstrap from
 * corruption.
 *
 * Reset wipes relying-party state only. The local attestation cert/key are the
 * authenticator identity used by certification metadata and must remain stable
 * across CTAP reset.
 */
bool u2f_data_check(bool cert_only);

bool u2f_data_cert_check(void);

uint32_t u2f_data_cert_load(uint8_t *cert, size_t capacity);

bool u2f_data_cert_key_load(uint8_t *cert_key);
bool u2f_data_cert_key_matches(const uint8_t *cert_key);
bool u2f_data_bootstrap_attestation_assets(const uint8_t *cert, size_t cert_len,
                                           const uint8_t *cert_key, size_t cert_key_len);
bool u2f_data_generate_attestation_assets(void);

bool u2f_data_key_exists(void);
bool u2f_data_key_load(uint8_t *device_key);

bool u2f_data_key_generate(uint8_t *device_key);

bool u2f_data_cnt_exists(void);
bool u2f_data_cnt_read(uint32_t *cnt);

/*
 * Counter reserve writes a durable high-water value. Callers publish the actual
 * response counter from RAM, then later reserve another window when exhausted.
 */
bool u2f_data_cnt_reserve(uint32_t cnt, uint32_t *reserved_cnt);
bool u2f_data_cnt_write(uint32_t cnt);
bool u2f_data_wipe(Storage *storage);

#ifdef __cplusplus
}
#endif

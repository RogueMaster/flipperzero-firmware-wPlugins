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

#include "../zerofido_types.h"

/*
 * Record-format helpers own the serialized credential file contract. They
 * expose index-only loads for boot, display-only loads that avoid private key
 * material, and full loads for signing paths.
 */
void zf_store_record_format_hex_encode(const uint8_t *data, size_t size, char *out);
bool zf_store_record_format_is_record_name(const char *name);
bool zf_store_record_format_load_index_with_buffer(Storage *storage, const char *file_name,
                                                   ZfCredentialIndexEntry *entry, uint8_t *buffer,
                                                   size_t buffer_size);
bool zf_store_record_format_load_record_with_buffer(Storage *storage, const char *file_name,
                                                    ZfCredentialRecord *record, uint8_t *buffer,
                                                    size_t buffer_size);
bool zf_store_record_format_load_record_for_display_with_buffer(Storage *storage,
                                                                const char *file_name,
                                                                ZfCredentialRecord *record,
                                                                uint8_t *buffer,
                                                                size_t buffer_size);
/*
 * Counter reservation updates only the small counter floor/high-water file.
 * It is intentionally narrower than a full record rewrite so response
 * publication can be fail-closed around monotonic counters.
 */
bool zf_store_record_format_reserve_counter_with_buffer(Storage *storage,
                                                        const ZfCredentialRecord *record,
                                                        uint8_t *buffer, size_t buffer_size,
                                                        uint32_t *out_high_water);
bool zf_store_record_format_reserve_counter(Storage *storage, const ZfCredentialRecord *record,
                                            uint32_t *out_high_water);
bool zf_store_record_format_write_record_with_buffer(Storage *storage,
                                                     const ZfCredentialRecord *record,
                                                     uint8_t *buffer, size_t buffer_size);
